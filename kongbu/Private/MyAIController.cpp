#include "MyAIController.h"

#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PickupActor.h"
#include "ExorcismSubsystem.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GhostCharacter.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"

namespace
{
    // 匿名命名空间里的常量只在当前 cpp 文件内可见。
    // 这样既能避免污染全局命名空间，也方便集中调 AI 的基础参数。
    constexpr float PatrolSearchRadius = 500.f;
    constexpr float ChaseAcceptanceRadius = 50.f;
    constexpr float FleeAcceptanceRadius = 25.f;
    constexpr float PatrolRetryDelayOnMoveComplete = 0.1f;
    constexpr float LookAroundYawToleranceDegrees = 2.5f;
    constexpr float DebugFallbackSightRadius = 1000.f;
    constexpr float DebugFallbackLoseSightRadius = 1200.f;
    constexpr float DebugFallbackPeripheralVisionAngleDegrees = 60.f;
    constexpr float MaxRageDisableRadiusExpansion = 120.f;
    const FName LostSightStimulusTag(TEXT("LostSight"));
    const FName RageDisabledItemStimulusTag(TEXT("RageDisabledItem"));

    bool IsPathFollowingMoveActive(const AAIController* Controller)
    {
        if (!IsValid(Controller))
        {
            return false;
        }

        const UPathFollowingComponent* PathFollowingComponent = Controller->GetPathFollowingComponent();
        return IsValid(PathFollowingComponent)
            && PathFollowingComponent->GetStatus() != EPathFollowingStatus::Idle;
    }

    float GetDistanceToBounds2D(const FVector& Point, const FVector& BoundsOrigin, const FVector& BoundsExtent)
    {
        const FVector ClosestPointOnBounds = Point.BoundToBox(BoundsOrigin - BoundsExtent, BoundsOrigin + BoundsExtent);
        return FVector::Dist2D(Point, ClosestPointOnBounds);
    }

    float GetPawnCollisionRadius(const APawn* Pawn)
    {
        constexpr float DefaultPawnRadius = 34.f;

        if (!IsValid(Pawn))
        {
            return DefaultPawnRadius;
        }

        if (const UCapsuleComponent* CapsuleComponent = Pawn->FindComponentByClass<UCapsuleComponent>())
        {
            float CapsuleRadius = DefaultPawnRadius;
            float CapsuleHalfHeight = 88.f;
            CapsuleComponent->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
            return CapsuleRadius;
        }

        return DefaultPawnRadius;
    }

    bool TryFindRagePickupApproachLocation(
        const APawn* ControlledPawn,
        const APickupActor* RagePickup,
        const float BaseAcceptanceRadius,
        FVector& OutMoveLocation,
        float& OutDisableAcceptanceRadius)
    {
        if (!IsValid(ControlledPawn) || !IsValid(RagePickup))
        {
            return false;
        }

        UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(ControlledPawn->GetWorld());
        if (!IsValid(NavSystem))
        {
            return false;
        }

        FVector PickupOrigin = RagePickup->GetActorLocation();
        FVector PickupExtent = FVector::ZeroVector;
        RagePickup->GetActorBounds(true, PickupOrigin, PickupExtent);

        const FVector PawnLocation = ControlledPawn->GetActorLocation();
        FVector PreferredDirection = PawnLocation - PickupOrigin;
        PreferredDirection.Z = 0.f;
        if (!PreferredDirection.Normalize())
        {
            PreferredDirection = FVector::ForwardVector;
        }

        const float PawnRadius = GetPawnCollisionRadius(ControlledPawn);
        const float ApproachPadding = PawnRadius + 16.f;
        const FVector ProjectionExtent(450.f, 450.f, 250.f);

        const FVector DirectionSamples[] =
        {
            PreferredDirection,
            FVector(1.f, 0.f, 0.f),
            FVector(-1.f, 0.f, 0.f),
            FVector(0.f, 1.f, 0.f),
            FVector(0.f, -1.f, 0.f),
            FVector(0.70710677f, 0.70710677f, 0.f),
            FVector(0.70710677f, -0.70710677f, 0.f),
            FVector(-0.70710677f, 0.70710677f, 0.f),
            FVector(-0.70710677f, -0.70710677f, 0.f)
        };

        bool bFoundProjectedPoint = false;
        float BestDistanceToBounds = TNumericLimits<float>::Max();
        float BestDistanceToPawn = TNumericLimits<float>::Max();

        for (const FVector& DirectionSample : DirectionSamples)
        {
            FVector Direction2D = DirectionSample;
            Direction2D.Z = 0.f;
            if (!Direction2D.Normalize())
            {
                continue;
            }

            const FVector CandidateLocation(
                PickupOrigin.X + Direction2D.X * (PickupExtent.X + ApproachPadding),
                PickupOrigin.Y + Direction2D.Y * (PickupExtent.Y + ApproachPadding),
                PickupOrigin.Z);

            FNavLocation ProjectedLocation;
            if (!NavSystem->ProjectPointToNavigation(CandidateLocation, ProjectedLocation, ProjectionExtent))
            {
                continue;
            }

            if (UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
                ControlledPawn->GetWorld(), PawnLocation, ProjectedLocation.Location, const_cast<APawn*>(ControlledPawn)))
            {
                if (!Path->IsValid() || Path->IsPartial())
                {
                    continue;
                }
            }
            else
            {
                continue;
            }

            const float CandidateDistanceToBounds = GetDistanceToBounds2D(ProjectedLocation.Location, PickupOrigin, PickupExtent);
            const float CandidateDistanceToPawn = FVector::DistSquared2D(PawnLocation, ProjectedLocation.Location);

            if (!bFoundProjectedPoint
                || CandidateDistanceToBounds < BestDistanceToBounds - 1.f
                || (FMath::IsNearlyEqual(CandidateDistanceToBounds, BestDistanceToBounds, 1.f)
                    && CandidateDistanceToPawn < BestDistanceToPawn))
            {
                bFoundProjectedPoint = true;
                BestDistanceToBounds = CandidateDistanceToBounds;
                BestDistanceToPawn = CandidateDistanceToPawn;
                OutMoveLocation = ProjectedLocation.Location;
            }
        }

        if (!bFoundProjectedPoint)
        {
            return false;
        }

        OutDisableAcceptanceRadius = FMath::Clamp(
            BestDistanceToBounds + 20.f,
            BaseAcceptanceRadius,
            BaseAcceptanceRadius + MaxRageDisableRadiusExpansion);
        return true;
    }
}

// 构造函数：当 Controller 对象被创建时调用。
// 这里适合做“组件创建”和“默认参数初始化”，
// 但不适合访问 Pawn 位置，因为这时通常还没有 Possess 任何角色。
AMyAIController::AMyAIController()
{
    // AI 的状态推进和调试绘制都依赖 Tick。
    PrimaryActorTick.bCanEverTick = true;

    // CreateDefaultSubobject 是 UE 的标准 API，
    // 用来在构造阶段创建会跟随当前 Actor 生命周期存在的默认组件。
    AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));

    // 视觉用于“看到玩家就追”，听觉用于“听到噪声就调查”。
    SightConfig->SightRadius = 1000.f;
    SightConfig->LoseSightRadius = 1200.f;
    SightConfig->PeripheralVisionAngleDegrees = 60.f;
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

    HearingConfig->HearingRange = 1600.f;
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

    // ConfigureSense 会把具体的感知配置注册给感知组件。
    AIPerception->ConfigureSense(*SightConfig);
    AIPerception->ConfigureSense(*HearingConfig);

    // SetDominantSense 指定“主感知”，通常用来表示最重要的感知来源。
    AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());

    // AddDynamic 是 UE 动态委托绑定写法。
    // 含义是：当感知到目标状态变化时，自动调用 OnPerceptionUpdated。
    AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AMyAIController::OnPerceptionUpdated);
}

void AMyAIController::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
    Super::GetActorEyesViewPoint(OutLocation, OutRotation);

    const APawn* ControlledPawn = GetPawn();
    if (!IsValid(ControlledPawn))
    {
        return;
    }

    ControlledPawn->GetActorEyesViewPoint(OutLocation, OutRotation);

    if (UMeshComponent* VisualMesh = ResolveControlledVisualMesh())
    {
        if (!SightOriginSocketName.IsNone() && VisualMesh->DoesSocketExist(SightOriginSocketName))
        {
            OutLocation = VisualMesh->GetSocketLocation(SightOriginSocketName);
            OutRotation = GetControlRotation();
            return;
        }
    }

    OutLocation += ControlledPawn->GetActorTransform().TransformVectorNoScale(SightOriginOffset);
    OutRotation = GetControlRotation();
}
// OnPossess 是 UE 的 AAIController 生命周期回调。
// 当这个控制器正式接管某个 Pawn 时会自动调用，
// 所以这里才是初始化运行时状态、启动巡逻的正确位置。
void AMyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    ClearPatrolRetry();
    ClearInvestigationState();
    ActiveRageThreatSources.Reset();
    ActiveSlowSources.Reset();
    bPendingFearRepath = false;
    bCachedWalkSpeedValid = false;
    CachedWalkSpeedBeforeFear = 0.f;
    TrackedUnmodifiedWalkSpeed = 0.f;
    bHasTrackedUnmodifiedWalkSpeed = false;
    bFearActive = false;
    bFearShouldRepath = false;
    FearTimeRemaining = 0.f;
    FearRepathTimeRemaining = 0.f;
    ActiveFearStimulus = FFearStimulus();
    bHasLastSeenPlayerData = false;
    LastSeenPlayerLocation = FVector::ZeroVector;
    LostSightTrackedPlayer = nullptr;
    PendingLostSightActor = nullptr;
    CurrentFearHidePoint = nullptr;
    FearResponseMode = EFearResponseMode::None;
    bFearHoldingAtHidePoint = false;
    bFearDetourActive = false;
    bForcePatrolAfterFearEnds = false;
    FearDetourTarget = FVector::ZeroVector;
    FearDetourTimeRemaining = 0.f;
    FearHideEscapeTimeRemaining = 0.f;
    FearRetreatTimeRemaining = 0.f;
    bFearRetreatInputActive = false;
    FearRetreatLogTimeRemaining = 0.f;
    ResetFearAnger();
    RageTimeRemaining = 0.f;
    LostSightSearchTimeRemaining = 0.f;
    LostSightTrackTimeRemaining = 0.f;
    LostSightConfirmationTimeRemaining = 0.f;
    ResetChaseProgressTracking();
    ActiveMoveRequestId = FAIRequestID::InvalidRequest;
    bPendingRageEnter = false;
    ClearRageSourceState();
    bCanSeePlayer = false;
    TargetPlayer = nullptr;
    StunTimeRemaining = 0.f;
    StunImmunityTimeRemaining = 0.f;
    SuppressedFearAngerTimeRemaining = 0.f;
    SuppressedStunTimeRemaining = 0.f;
    StunTurnDirection = 1.f;
    ActiveAttachedDisableSources.Reset();
    ClearLureState();
    bFlashlightRevealActive = false;
    FlashlightRevealTimeRemaining = 0.f;
    bWeaknessActive = false;
    bWeaknessRevealPhaseVisible = false;
    WeaknessTimeRemaining = 0.f;
    WeaknessRevealToggleTimeAccumulator = 0.f;
    bGhostVisualStateApplied = false;
    bHasCachedGhostCapsuleCollisionResponses = false;
    bHasCachedGhostMeshCollisionResponses = false;

    const float InitialWalkSpeed = GetCurrentUnmodifiedWalkSpeed();
    if (InitialWalkSpeed > 0.f)
    {
        TrackedUnmodifiedWalkSpeed = InitialWalkSpeed;
        bHasTrackedUnmodifiedWalkSpeed = true;
    }

    ApplyGhostVisualState();

    AssignExorcismGhostType();

    Patrol();
}




void AMyAIController::AssignExorcismGhostType()
{
    if (ExorcismGhostTypeId != INDEX_NONE)
    {
        return;
    }

    if (const AGhostCharacter* GhostPawn = Cast<AGhostCharacter>(GetPawn()))
    {
        if (GhostPawn->ExorcismGhostTypeId != INDEX_NONE)
        {
            ExorcismGhostTypeId = GhostPawn->ExorcismGhostTypeId;
            UE_LOG(LogTemp, Log, TEXT("%s: using configured ghost type %d from %s"),
                *GetName(),
                ExorcismGhostTypeId,
                *GetNameSafe(GhostPawn));
            return;
        }
    }

    const UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        return;
    }

    UExorcismSubsystem* Subsystem = GI->GetSubsystem<UExorcismSubsystem>();
    if (!Subsystem || !Subsystem->HasMappings())
    {
        return;
    }

    ExorcismGhostTypeId = Subsystem->ClaimNextGhostType();
    if (ExorcismGhostTypeId == INDEX_NONE)
    {
        // UE_LOG(LogTemp, Warning, TEXT("%s: no unclaimed ghost types left"), *GetName());
        return;
    }

    const FSoftClassPath GhostClassPath = Subsystem->GetGhostClassPath(ExorcismGhostTypeId);
    if (!GhostClassPath.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: ghost type %d has no ghost class path"), *GetName(), ExorcismGhostTypeId);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("%s: claimed ghost type %d (%s)"),
        *GetName(),
        ExorcismGhostTypeId,
        *GhostClassPath.ToString());
}

// Tick 是 UE 每帧调用一次的更新函数。
// 当前 AI 的主状态机优先级就在这里统一决定：RageBreakout > Stunned > Rage > Fear > Chase > Investigate > Patrol。
void AMyAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 鬼的显隐完全由控制器维护：默认半透明，被手电命中后短暂恢复正常不透明。
    UpdateGhostVisualState(DeltaTime);

    
    if (const AGhostCharacter* GhostPawn = Cast<AGhostCharacter>(GetPawn()))
    {
        if (GhostPawn->bStopAIMovementDuringSoulSuck && GhostPawn->bIsSoulSucking)
        {
            StopMovement();
            ActiveMoveRequestId = FAIRequestID::InvalidRequest;
            bIsMoving = false;
            return;
        }

        if (GhostPawn->IsSoulSuckBreakRecoveryActive())
        {
            StopMovement();
            ActiveMoveRequestId = FAIRequestID::InvalidRequest;
            bIsMoving = false;
            return;
        }

        if (GhostPawn->bStopAIMovementDuringTelekineticThrow && GhostPawn->bIsTelekineticThrowActive)
        {
            StopMovement();
            ActiveMoveRequestId = FAIRequestID::InvalidRequest;
            bIsMoving = false;
            return;
        }
    }

    // DeltaTime = “这一帧经过了多少秒”。
    // UE 每帧都会把这个值传进来，所有倒计时、速度平滑和状态推进都依赖它，
    // 这样 AI 在高帧率和低帧率下都能得到大致一致的行为结果。

    // 调试绘制只影响可视化，不参与 AI 决策。
    // 放在 Tick 开头可以保证你每帧都看到最新的感知范围、状态文本和调试线。
    // bShowDebugSight 是一个调试开关，定义在头文件的 Debug 分类里；
    // 你可以在蓝图 Details 面板里直接开关它。
    if (bShowDebugSight)
    {
        DrawDebugSight();
    }

    // 关卡里放置的 AI 有可能先 OnPossess，后由 GameMode 在 BeginPlay 里生成驱魔映射。
    // 这种顺序下第一次领取会失败，所以这里做一次轻量补领，避免 ExorcismGhostTypeId 永远停在 -1。
    if (ExorcismGhostTypeId == INDEX_NONE)
    {
        AssignExorcismGhostType();
    }

    // 驱魔类型调试：独立于 bShowDebugSight，始终在鬼头顶显示符咒编号。
    if (bShowExorcismDebug)
    {
        const APawn* DebugPawn = GetPawn();
        if (IsValid(DebugPawn))
        {
            const FVector PawnLocation = DebugPawn->GetActorLocation();
            FString ExorcismDebugText = FString::Printf(TEXT("GHOST TYPE %d"), ExorcismGhostTypeId);

            if (const UGameInstance* GI = GetGameInstance())
            {
                if (const UExorcismSubsystem* Subsystem = GI->GetSubsystem<UExorcismSubsystem>())
                {
                    const FSoftObjectPath RunePath = Subsystem->GetRuneTextureForGhostType(ExorcismGhostTypeId);
                    if (RunePath.IsValid())
                    {
                        const FString RuneName = FPaths::GetBaseFilename(RunePath.GetAssetPathString());
                        ExorcismDebugText += FString::Printf(TEXT("\nRUNE: %s"), *RuneName);
                    }
                }
            }

            DrawDebugString(
                GetWorld(),
                PawnLocation + FVector(0.f, 0.f, 160.f),
                ExorcismDebugText,
                nullptr,
                FColor::Magenta,
                0.f,
                true,
                1.2f);
        }
    }

    SuppressedFearAngerTimeRemaining = FMath::Max(0.f, SuppressedFearAngerTimeRemaining - DeltaTime);
    SuppressedStunTimeRemaining = FMath::Max(0.f, SuppressedStunTimeRemaining - DeltaTime);

    StunImmunityTimeRemaining = FMath::Max(0.f, StunImmunityTimeRemaining - DeltaTime);

    // Rage 改为“累计受惊量”触发后，这里只负责推进命中冷却，
    // 防止同一个高频脉冲源在极短时间里重复加太多怒气。
    UpdateFearAnger(DeltaTime);

    // bPendingRageEnter 表示“怒气阈值已经满足，但还没正式切进 Rage”。
    // 这里额外判断 CurrentState != Rage，是为了避免已经在 Rage 里时又重复进入一次，
    // 否则会把 Rage 的计时、速度和移动状态反复重置。
    if (bPendingRageEnter && CurrentState != EEnemyAIState::Rage)
    {
        bPendingRageEnter = false;
        EnterRageState();
        return;
    }

    if (CurrentState == EEnemyAIState::Stunned)
    {
        UpdateStunnedState(DeltaTime);
        return;
    }

    // 怒气衰减只在“非 Rage 且当前没有持续 Fear”时进行。
    // 这样可以避免角色在仍处于恐惧反应中时，怒气值被错误地提前回落。
    // CurrentState 是这个 AI 当前的大状态枚举，来源于 MyAIController 自己维护的状态机；
    // 它的取值通常是 Patrol / Investigate / Chase / Flee / Rage 之一。
    // IsFearActive() 读取的是 Controller 内部维护的 Fear 标记 bFearActive，
    // 用来表示“现在是否还处于受惊主流程”。
    if (CurrentState != EEnemyAIState::Rage
        && CurrentState != EEnemyAIState::Stunned
        && !IsFearActive())
    {
        // 只要当前不是被恐惧驱动的逃跑动作，就清掉“后退输入已激活”的标记，
        // 防止上一帧残留的 Fear 后退状态影响后续逻辑。
        // bFearRetreatInputActive 是一个调试/状态标记，表示这一帧有没有真的给 Pawn
        // 下发“后退移动输入”。它主要用于 Debug 文本显示，也方便区分
        // “只是处于 Flee 状态”还是“当前正处于后退输入阶段”。
        bFearRetreatInputActive = false;

        // UpdateFearAngerDecay 会推进怒气自然衰减；它内部会改 FearAngerLevel，
        // 也就是“鬼被持续惊吓后积累的怒气值”。
        UpdateFearAngerDecay(DeltaTime);
    }

    // Rage 是最高优先级的战斗/反扑状态。
    // 一旦进入 Rage，就直接交给 Rage 分支处理，并立刻 return，
    // 这样 Fear、追逐、调查、巡逻都不会在同一帧里抢占控制权。
    if (CurrentState == EEnemyAIState::Rage)
    {
        UpdateRageState(DeltaTime);
        return;
    }

    // Fear 仍然是仅次于 Rage 的最高优先级状态。
    // AdvanceFear 会推进 Controller 自己维护的恐惧计时和阶段；只要它还返回 true，
    // 就说明当前这一帧仍然处在“受惊”流程中，应该优先执行 Fear 行为。
    // 这里推进的核心变量有：
    // bFearActive：是否仍在 Fear 中。
    // FearTimeRemaining：Fear 还剩多久。
    // FearRepathTimeRemaining：多久后要再次请求重算逃跑路径。
    // bFearShouldRepath：这一帧是否要把“请重新寻路”交给 Fear 状态机处理。
    if (AdvanceFear(DeltaTime))
    {
        // Fear 期间如果路径被打断、目标发生变化或者需要重算逃跑路线，
        // 这里会把“是否需要重新规划路径”的信息合并后传给 Fear 状态更新函数。
        // bPendingFearRepath 是 Controller 自己额外记住的“待重规划”标记；
        // 它通常在 OnMoveCompleted 里被置为 true，表示上一条路走完/失败/被打断后，
        // 下一帧 Fear 应该重新想一条路线。
        // ConsumeFearShouldRepath() 会读取并清掉 bFearShouldRepath，
        // 这个标记来自 Fear 的定时器，表示“即使没撞墙，也该周期性重算一下逃跑路径”。
        const bool bShouldRepathFromFear = bPendingFearRepath || ConsumeFearShouldRepath();

        // 一次性消费掉这帧的重规划请求，避免同一个请求在后续帧里重复触发。
        bPendingFearRepath = false;

        // Fear 期间不处理追逐/调查/巡逻，统一由 Fear 状态机决定是后退、逃向躲藏点，还是维持当前逃跑动作。
        // bShouldRepathFromFear 是这一帧的局部临时变量，不会长期保存；
        // 它只是把“现在要不要重新算逃跑路径”传给 UpdateFearState 使用。
        UpdateFearState(DeltaTime, bShouldRepathFromFear);
        return;
    }

    if (bLureActive)
    {
        UpdateLureState(DeltaTime);
        return;
    }

    // 即使内部 Fear 主计时已经结束，只要当前仍处在“逃向躲藏点”的子流程里，
    // 也要把这段动作完整跑完，避免 Fear 提前结束导致动作生硬中断。
    // CurrentState == Flee 表示大状态还在“逃跑”。
    // FearResponseMode 是 Flee 下面的子状态，表示当前逃跑阶段属于：
    // None / Retreat / EscapeToHide / Rage 其中之一。
    // FearHideEscapeTimeRemaining 表示“朝躲藏点跑”这个阶段剩余多久。
    // HasPendingFearHideTarget() 则表示：虽然计时可能快结束了，但当前仍有没跑到的躲藏点目标。
    if (CurrentState == EEnemyAIState::Flee
        && FearResponseMode == EFearResponseMode::EscapeToHide
        && (FearHideEscapeTimeRemaining > 0.f || HasPendingFearHideTarget()))
    {
        // 这里依然沿用 Fear 状态更新，只是此时可能不再依赖主 Fear 计时。
        UpdateFearState(DeltaTime, bPendingFearRepath);

        // 重规划请求在这里同样只消费一次，防止反复重进路径重算分支。
        bPendingFearRepath = false;
        return;
    }

    // 如果已经不再处于 Fear 主流程，但状态还停留在 Flee，
    // 说明恐惧刚刚结束，需要先做恢复收尾，再把控制权交回普通状态机。
    // 这里判断的核心变量还是 CurrentState：
    // 只要它还等于 Flee，就说明这个 AI 还没从“逃跑状态”彻底收尾完成。
    if (CurrentState == EEnemyAIState::Flee)
    {
        HandleFearEnded();
        return;
    }

    // 恢复到普通 AI 后，优先检查是否已经看见玩家。
    // 视觉追击的优先级高于调查和巡逻，所以命中后立刻切入 Chase。
    // bCanSeePlayer 是“当前是否有可见玩家目标”的缓存布尔值；
    // 它主要由视觉感知回调 HandleSightStimulus 改写。
    // TargetPlayer 是当前锁定的玩家 Actor 指针；
    // IsValid(TargetPlayer) 用来防止这个 Actor 已经被销毁、失效或为空。

    if (LostSightConfirmationTimeRemaining > 0.f)
    {
        LostSightConfirmationTimeRemaining = FMath::Max(0.f, LostSightConfirmationTimeRemaining - DeltaTime);
        if (LostSightConfirmationTimeRemaining <= 0.f && PendingLostSightActor == TargetPlayer)
        {
            AActor* ConfirmedLostActor = PendingLostSightActor;
            PendingLostSightActor = nullptr;
            bCanSeePlayer = false;
            TargetPlayer = nullptr;

            if (!IsFearActive())
            {
                StartLostSightInvestigation(ConfirmedLostActor);
            }
        }
    }

    if (bCanSeePlayer && IsValid(TargetPlayer))
    {
        ChasePlayer(ShouldForceChaseRefresh(DeltaTime));
        return;
    }

    // 没有直接看到玩家时，如果当前仍处于调查状态，就继续执行调查/搜查流程。
    // 这里依赖的仍然是 CurrentState，表示 AI 前面可能因为噪声、丢失视野等原因
    // 已经进入了 Investigate，所以这时应该继续推进调查，而不是直接回巡逻。
    if (CurrentState == EEnemyAIState::Investigate)
    {
        UpdateInvestigation(DeltaTime);
        return;
    }

    // 如果前面所有更高优先级状态都没有接管，并且当前也没有在移动，
    // 就回到默认巡逻，开始新一轮路径选择。
    // bIsMoving 是“当前有没有正在进行中的移动请求”的缓存值；
    // 它会在 MoveTo 成功提交时设为 true，在 OnMoveCompleted 时设回 false。
    // 这里判断它，是为了避免每帧都重复下发新的 Patrol 路径请求。
    if (!bIsMoving)
    {
        Patrol();
    }
}

// OnPerceptionUpdated 是感知组件的回调入口，不是纯虚 override，
// 而是通过 AddDynamic 绑定后，在视觉/听觉状态变化时自动被调用。
void AMyAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    // 感知系统有时会把无效对象、自己本体，甚至已经被销毁边缘状态的 Actor 送进回调。
    // 这里统一过滤，避免 AI 因为“感知到自己”而错误进入追击/调查。
    if (!IsValid(Actor) || Actor == GetPawn() || CurrentState == EEnemyAIState::Stunned)
    {
        return;
    }

    const FAISenseID SightSenseID = UAISense::GetSenseID<UAISense_Sight>();
    const FAISenseID HearingSenseID = UAISense::GetSenseID<UAISense_Hearing>();

    // 同一个感知回调里根据 SenseID 分流，避免把视觉/听觉逻辑混在一起。
    if (Stimulus.Type == SightSenseID)
    {
        HandleSightStimulus(Actor, Stimulus);
        return;
    }

    if (Stimulus.Type == HearingSenseID)
    {
        HandleHearingStimulus(Actor, Stimulus);
    }
}

// 给外部系统提供一个直接施加恐惧刺激的入口。
// 例如机关、技能、道具效果都可以调用这个函数让 AI 进入 Fear。
void AMyAIController::ApplyFearStimulus(const FFearStimulus& Stimulus)
{
    if (!bReactToFearStimuli)
    {
        return;
    }

    FFearStimulus NormalizedStimulus = Stimulus;
    if (NormalizedStimulus.SourceActor == GetPawn() || NormalizedStimulus.SourceActor == this)
    {
        NormalizedStimulus.SourceActor = nullptr;
    }

    RegisterRageThreatSource(NormalizedStimulus.SourceActor);

    // Rage 已经是“恐惧积累后的反扑态”，
    // 不应该再被新的 Fear 事件重新覆盖回 Flee。
    // bPendingRageEnter 也一并拦掉，避免这一帧先吃 Fear、下一帧又立刻切 Rage，
    // 导致行为抖动。
    if (CurrentState == EEnemyAIState::Stunned
        || CurrentState == EEnemyAIState::Rage
        || bPendingRageEnter)
    {
        return;
    }

    const bool bWasFearActive = bFearActive;
    const bool bSameSourceActor = ActiveFearStimulus.SourceActor == NormalizedStimulus.SourceActor;
    const bool bSameTag = ActiveFearStimulus.StimulusTag == NormalizedStimulus.StimulusTag;
    const bool bSameLocation = FVector::DistSquared(ActiveFearStimulus.SourceLocation, NormalizedStimulus.SourceLocation)
        <= FMath::Square(30.f);
    // “同一个持续刺激源”必须同时满足：
    // 1. 上一帧本来就在 Fear。
    // 2. 来源 Actor 没变。
    // 3. Tag 没变，说明还是同一种刺激语义。
    // 4. 位置几乎没变，避免把“同一个道具移动到别处”误当成旧刺激刷新。
    // 只有都成立时，下面才走“刷新现有 Fear”而不是“重新受惊”分支。
    const bool bIsSameContinuousSource = bWasFearActive && bSameSourceActor && bSameTag && bSameLocation;

    ActiveFearStimulus = NormalizedStimulus;
    FearTimeRemaining = GetResolvedFearDuration(NormalizedStimulus);
    bFearActive = FearTimeRemaining > 0.f;

    if (!bFearActive)
    {
        // 持续时间被解析成 0 时，表示这条刺激不足以进入 Fear。
        // 这里顺手把 repath 相关标记清掉，防止上一轮 Fear 的残留请求串到后面。
        FearRepathTimeRemaining = 0.f;
        bFearShouldRepath = false;
        return;
    }

    RegisterFearAngerStimulus(ActiveFearStimulus);
    if (CurrentState == EEnemyAIState::Rage)
    {
        return;
    }

    if (!bWasFearActive)
    {
        // 第一次进入 Fear 时，立刻要求一次重新规划，并立刻切进 Fear 行为。
        FearRepathTimeRemaining = 0.f;
        bFearShouldRepath = true;
        HandleFearApplied(ActiveFearStimulus);
        return;
    }

    if (bIsSameContinuousSource)
    {
        // 同一个 ItemX 持续照射时，普通情况下只刷新 Fear 剩余时间；
        // 但如果当前正处于 Flee，就不能让鬼在 Hide 阶段继续前冲穿过射线。
        // 这里要么重进 Retreat，要么继续拉长 Retreat 时间，让它持续后退直到真正脱离恐惧源。
        FearRepathTimeRemaining = 0.f;
        bFearShouldRepath = true;

        if (CurrentState == EEnemyAIState::Flee)
        {
            if (FearResponseMode == EFearResponseMode::EscapeToHide)
            {
                HandleFearApplied(ActiveFearStimulus);
                return;
            }

            if (FearResponseMode == EFearResponseMode::Retreat)
            {
                FearRetreatTimeRemaining = FMath::Max(FearRetreatTimeRemaining, FearRetreatDuration);
                bIsMoving = true;
            }
        }

        return;
    }

    // 如果是新的恐惧源，或者同源但位置明显变了，就把它当成一次新的惊吓处理。
    FearRepathTimeRemaining = 0.f;
    bFearShouldRepath = true;
    HandleFearApplied(ActiveFearStimulus);
}

// 这是 ApplyFearStimulus 的便捷版本。
// 只需要给一个世界坐标和持续时间，就能快速构造一条恐惧事件。
// 对新手来说可以理解为“蓝图更容易调用的包装函数”。
// 它本身不是 UE 自动调用，而是给你手动调用的接口。
void AMyAIController::ApplyFearFromLocation(FVector SourceLocation, float Duration)
{
    FFearStimulus Stimulus;
    Stimulus.SourceLocation = SourceLocation;
    Stimulus.Duration = Duration;
    Stimulus.StimulusTag = TEXT("ItemXSonar");
    ApplyFearStimulus(Stimulus);
}

void AMyAIController::ApplyStun(float Duration)
{
    ApplyStunFromSource(Duration, nullptr);
}

void AMyAIController::ApplyStunFromSource(float Duration, AActor* SourceActor)
{
    if (!GetPawn()
        || CurrentState == EEnemyAIState::Rage
        || bPendingRageEnter
        || StunImmunityTimeRemaining > 0.f)
    {
        return;
    }

    if (CurrentState == EEnemyAIState::Investigate
        && CurrentInvestigateStimulusTag == RageDisabledItemStimulusTag)
    {
        return;
    }

    const bool bMatchesSuppressedStunSourceActor = IsValid(SuppressedStunSourceActor)
        && SuppressedStunSourceActor == SourceActor;
    const bool bMatchesSuppressedStunSourceLocation = IsValid(SourceActor)
        && FVector::DistSquared(SuppressedStunSourceLocation, SourceActor->GetActorLocation()) <= FMath::Square(60.f);
    if (SuppressedStunTimeRemaining > 0.f
        && (bMatchesSuppressedStunSourceActor || bMatchesSuppressedStunSourceLocation))
    {
        return;
    }

    const float ResolvedStunDuration = Duration > 0.f ? Duration : DefaultStunDuration;
    if (ResolvedStunDuration <= 0.f)
    {
        return;
    }

    StunTimeRemaining = ResolvedStunDuration;
    EnterStunnedState();
}

void AMyAIController::ApplyAttachedDisableSource(AActor* SourceActor)
{
    if (!GetPawn() || !IsValid(SourceActor))
    {
        return;
    }

    CleanupInvalidAttachedDisableSources();

    const bool bAlreadyRegistered = ActiveAttachedDisableSources.ContainsByPredicate(
        [SourceActor](const TWeakObjectPtr<AActor>& Entry)
        {
            return Entry.Get() == SourceActor;
        });

    if (!bAlreadyRegistered)
    {
        ActiveAttachedDisableSources.Add(SourceActor);
    }

    // 持续贴身禁用优先级高于 Rage，避免鬼被符贴住后仍继续追人或拆道具。
    bPendingRageEnter = false;
    RageTimeRemaining = 0.f;
    ClearRageSourceState();

    EnterStunnedState();
}

void AMyAIController::RemoveAttachedDisableSource(AActor* SourceActor)
{
    if (!SourceActor)
    {
        return;
    }

    CleanupInvalidAttachedDisableSources();
    ActiveAttachedDisableSources.RemoveAll(
        [SourceActor](const TWeakObjectPtr<AActor>& Entry)
        {
            return Entry.Get() == SourceActor;
        });

    if (!HasAttachedDisableSource()
        && CurrentState == EEnemyAIState::Stunned
        && StunTimeRemaining <= 0.f)
    {
        HandleStunEnded();
    }
}

void AMyAIController::BeginSoulSuckBreakRecoveryPause()
{
    ClearPatrolRetry();
    StopMovement();
    ActiveMoveRequestId = FAIRequestID::InvalidRequest;
    bIsMoving = false;

    bCanSeePlayer = false;
    TargetPlayer = nullptr;
    bHasLastSeenPlayerData = false;
    LastSeenPlayerLocation = FVector::ZeroVector;
    LostSightTrackedPlayer = nullptr;
    CurrentFleeTarget = FVector::ZeroVector;
    PatrolTarget = FVector::ZeroVector;

    ClearInvestigationState();
    ClearLureState();
    ResetChaseProgressTracking();
    SetAIState(EEnemyAIState::Patrol);
}

void AMyAIController::EnterStunnedState()
{
    const bool bWasAlreadyStunned = CurrentState == EEnemyAIState::Stunned;

    ClearPatrolRetry();
    SetAIState(EEnemyAIState::Stunned);

    if (!bWasAlreadyStunned)
    {
        StunTurnDirection = FMath::RandBool() ? 1.f : -1.f;
    }
    else if (FMath::IsNearlyZero(StunTurnDirection))
    {
        StunTurnDirection = 1.f;
    }

    StopMovement();
    ActiveMoveRequestId = FAIRequestID::InvalidRequest;
    bIsMoving = false;

    bCanSeePlayer = false;
    TargetPlayer = nullptr;
    bHasLastSeenPlayerData = false;
    LastSeenPlayerLocation = FVector::ZeroVector;
    PatrolTarget = FVector::ZeroVector;
    CurrentFleeTarget = FVector::ZeroVector;
    LostSightTrackedPlayer = nullptr;

    ClearInvestigationState();

    bPendingRageEnter = false;
    RageTimeRemaining = 0.f;
    ClearRageSourceState();

    bFearActive = false;
    FearTimeRemaining = 0.f;
    FearRepathTimeRemaining = 0.f;
    bFearShouldRepath = false;
    bPendingFearRepath = false;
    CurrentFearHidePoint = nullptr;
    FearResponseMode = EFearResponseMode::None;
    bFearHoldingAtHidePoint = false;
    bForcePatrolAfterFearEnds = false;
    FearHideEscapeTimeRemaining = 0.f;
    FearRetreatTimeRemaining = 0.f;
    bFearRetreatInputActive = false;
    FearRetreatLogTimeRemaining = 0.f;
    ClearFearDetour();

    RestoreWalkSpeedAfterFear();
}

void AMyAIController::UpdateStunnedState(float DeltaTime)
{
    CleanupInvalidAttachedDisableSources();

    APawn* ControlledPawn = GetPawn();
    if (!IsValid(ControlledPawn))
    {
        HandleStunEnded();
        return;
    }

    if (IsPathFollowingMoveActive(this))
    {
        StopMovement();
    }

    ActiveMoveRequestId = FAIRequestID::InvalidRequest;
    bIsMoving = false;
    bCanSeePlayer = false;
    TargetPlayer = nullptr;

    const float TurnStepDegrees = FMath::Max(0.f, StunTurnRateDegrees) * DeltaTime * StunTurnDirection;
    FRotator NewRotation = GetControlRotation();
    NewRotation.Yaw = FRotator::NormalizeAxis(NewRotation.Yaw + TurnStepDegrees);
    SetControlRotation(NewRotation);
    ControlledPawn->FaceRotation(NewRotation, DeltaTime);

    if (!HasAttachedDisableSource())
    {
        StunTimeRemaining = FMath::Max(0.f, StunTimeRemaining - DeltaTime);
        if (StunTimeRemaining <= 0.f)
        {
            HandleStunEnded();
        }
    }
}

void AMyAIController::HandleStunEnded()
{
    if (CurrentState != EEnemyAIState::Stunned)
    {
        return;
    }

    CleanupInvalidAttachedDisableSources();
    if (HasAttachedDisableSource())
    {
        return;
    }

    StunTimeRemaining = 0.f;
    ActiveMoveRequestId = FAIRequestID::InvalidRequest;
    bIsMoving = false;
    bCanSeePlayer = false;
    TargetPlayer = nullptr;
    bHasLastSeenPlayerData = false;
    LastSeenPlayerLocation = FVector::ZeroVector;
    ClearInvestigationState();
    ClearPatrolRetry();

    SetAIState(EEnemyAIState::Patrol);
    Patrol();
}

void AMyAIController::CleanupInvalidAttachedDisableSources()
{
    ActiveAttachedDisableSources.RemoveAll(
        [](const TWeakObjectPtr<AActor>& Entry)
        {
            return !Entry.IsValid();
        });
}

bool AMyAIController::HasAttachedDisableSource() const
{
    for (const TWeakObjectPtr<AActor>& Entry : ActiveAttachedDisableSources)
    {
        if (Entry.IsValid())
        {
            return true;
        }
    }

    return false;
}

void AMyAIController::ClearFear()
{
    const bool bWasFearActive = bFearActive;

    bFearActive = false;
    FearTimeRemaining = 0.f;
    FearRepathTimeRemaining = 0.f;
    bFearShouldRepath = false;
    ActiveFearStimulus = FFearStimulus();

    if (bWasFearActive)
    {
        HandleFearEnded();
    }
}

bool AMyAIController::AdvanceFear(float DeltaTime)
{
    if (!bFearActive)
    {
        return false;
    }

    // 一个计时控制总持续时间，另一个计时控制多久要求 AI 重算一次逃跑路线。
    FearTimeRemaining -= DeltaTime;
    FearRepathTimeRemaining -= DeltaTime;

    if (FearTimeRemaining <= 0.f)
    {
        ClearFear();
        return false;
    }

    if (FearRepathTimeRemaining <= 0.f)
    {
        // 定期触发 repath，避免受惊中的 AI 因地形阻挡卡在一条旧路径上。
        FearRepathTimeRemaining = FearRepathInterval;
        bFearShouldRepath = true;
    }

    return true;
}

bool AMyAIController::ConsumeFearShouldRepath()
{
    const bool bConsume = bFearShouldRepath;
    bFearShouldRepath = false;
    return bConsume;
}

bool AMyAIController::IsFearActive() const
{
    return bFearActive;
}

bool AMyAIController::TryGetTrackedPlayerLocation(FVector& OutLocation) const
{
    if (bCanSeePlayer && IsValid(TargetPlayer))
    {
        OutLocation = TargetPlayer->GetActorLocation();
        return true;
    }

    if (bHasLastSeenPlayerData)
    {
        OutLocation = LastSeenPlayerLocation;
        return true;
    }

    OutLocation = FVector::ZeroVector;
    return false;
}

FVector AMyAIController::GetFearSourceLocation() const
{
    return ActiveFearStimulus.SourceLocation;
}

float AMyAIController::GetFearTimeRemaining() const
{
    return FearTimeRemaining;
}

float AMyAIController::GetFearEscapeDistance() const
{
    return FearEscapeDistance;
}

float AMyAIController::GetResolvedFearDuration(const FFearStimulus& Stimulus) const
{
    return Stimulus.Duration > 0.f ? Stimulus.Duration : DefaultFearDuration;
}

// ChasePlayer 负责真正向当前目标玩家发起追逐请求。
// 它通常不会被外部直接调用，而是在 Tick 发现“当前仍能看到目标”时持续触发。
void AMyAIController::ChasePlayer(bool bForceRefreshMove)
{
    if (CurrentState == EEnemyAIState::Stunned || IsFearActive() || !IsValid(TargetPlayer))
    {
        return;
    }

    ClearPatrolRetry();
    const bool bWasAlreadyChasing = CurrentState == EEnemyAIState::Chase;
    SetAIState(EEnemyAIState::Chase);
    if (bWasAlreadyChasing && IsPathFollowingMoveActive(this) && !bForceRefreshMove)
    {
        bIsMoving = true;
        return;
    }
    bIsMoving = RequestMoveToActor(TargetPlayer, ChaseAcceptanceRadius);
}

bool AMyAIController::ShouldForceChaseRefresh(float DeltaTime)
{
    APawn* ControlledPawn = GetPawn();
    if (!IsValid(ControlledPawn)
        || CurrentState != EEnemyAIState::Chase
        || !IsValid(TargetPlayer)
        || !IsPathFollowingMoveActive(this))
    {
        ResetChaseProgressTracking();
        return false;
    }

    ChaseRepathCooldownRemaining = FMath::Max(0.f, ChaseRepathCooldownRemaining - DeltaTime);

    const FVector CurrentLocation = ControlledPawn->GetActorLocation();
    if (!bHasLastChaseProgressLocation)
    {
        LastChaseProgressLocation = CurrentLocation;
        bHasLastChaseProgressLocation = true;
        ChaseStuckTime = 0.f;
        return false;
    }

    const float ProgressDistance = FVector::Dist2D(CurrentLocation, LastChaseProgressLocation);
    if (ProgressDistance >= ChaseStuckMovementThreshold)
    {
        LastChaseProgressLocation = CurrentLocation;
        ChaseStuckTime = 0.f;
        return false;
    }

    ChaseStuckTime += DeltaTime;
    if (ChaseStuckTime < ChaseStuckRepathDelay || ChaseRepathCooldownRemaining > 0.f)
    {
        return false;
    }

    LastChaseProgressLocation = CurrentLocation;
    ChaseStuckTime = 0.f;
    ChaseRepathCooldownRemaining = ChaseStuckRepathCooldown;
    return true;
}

void AMyAIController::ResetChaseProgressTracking()
{
    bHasLastChaseProgressLocation = false;
    LastChaseProgressLocation = FVector::ZeroVector;
    ChaseStuckTime = 0.f;
    ChaseRepathCooldownRemaining = 0.f;
}

// Patrol 是默认行为。
// 当没有 Fear、没有玩家可追、也没有调查目标时，AI 就会进入巡逻逻辑。
void AMyAIController::Patrol()
{
    if (CurrentState == EEnemyAIState::Stunned || IsFearActive() || !GetPawn())
    {
        return;
    }

    if (bCanSeePlayer && IsValid(TargetPlayer))
    {
        ChasePlayer();
        return;
    }

    ClearPatrolRetry();
    // 巡逻点是随机可达点；如果本次拿到的点几乎等于当前位置，就延后再抽一次。
    PatrolTarget = GetRandomPatrolPoint();
    SetAIState(EEnemyAIState::Patrol);

    if (PatrolTarget.Equals(GetPawn()->GetActorLocation(), 1.f))
    {
        bIsMoving = false;
        SchedulePatrolRetry();
        return;
    }

    bIsMoving = RequestMoveToLocation(PatrolTarget, 5.f);
    if (!bIsMoving)
    {
        SchedulePatrolRetry();
    }
}

// OnMoveCompleted 是 AAIController 的内置回调。
// 凡是 MoveToActor / MoveToLocation 发起的移动，在结束、失败或被打断时都会进这里。
void AMyAIController::OnMoveCompleted(FAIRequestID RequestID,
    const FPathFollowingResult& Result)
{
    Super::OnMoveCompleted(RequestID, Result);

    // StopMovement 或旧请求被打断时，UE 仍然会回调 OnMoveCompleted。
    // 这里先过滤掉“已经不是当前激活请求”的旧回调，避免它把新移动状态冲掉。
    if (ActiveMoveRequestId.IsValid() && RequestID != ActiveMoveRequestId)
    {
        return;
    }

    ActiveMoveRequestId = FAIRequestID::InvalidRequest;

    bIsMoving = false;

    if (CurrentState == EEnemyAIState::Stunned)
    {
        return;
    }

    if (CurrentState == EEnemyAIState::Flee)
    {
        // Retreat 阶段本来就不是靠 MoveTo 驱动，而是 Tick 里直接 AddMovementInput 后退。
        // 所以这里即便收到了某个旧导航请求的完成回调，也不能把它当成真正的 Fear 阶段结束。
        if (FearResponseMode == EFearResponseMode::Retreat)
        {
            return;
        }
        else if (FearResponseMode == EFearResponseMode::EscapeToHide)
        {
            if (bFearDetourActive)
            {
                // Detour 是“临时绕一下”的中间段。
                // 成功则回到正常躲藏路径；失败则说明绕路也不稳，直接退回 Retreat 重想路线。
                if (Result.Code == EPathFollowingResult::Success)
                {
                    ClearFearDetour();
                }
                else
                {
                    ClearFearDetour();
                    EnterFearRetreatState(true);
                }

                return;
            }

            if (Result.Code == EPathFollowingResult::Success)
            {
                // 成功跑到躲藏点后，不立刻结束 Fear，
                // 而是先保持“躲着”的逃跑状态，直到 FearHideEscapeDuration 走完。
                bFearHoldingAtHidePoint = true;
                bIsMoving = true;
            }
            else
            {
                bFearHoldingAtHidePoint = false;
                EnterFearRetreatState(true);
            }
        }

        return;
    }

    if (CurrentState == EEnemyAIState::Rage)
    {
        if (bLogRageThreats && Result.Code != EPathFollowingResult::Success)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("RAGE THREATS MOVE_COMPLETED Current=%s Result=%d TimeRemaining=%.2f"),
                *GetNameSafe(RageSourceActor.Get()),
                static_cast<int32>(Result.Code),
                RageTimeRemaining);
        }

        if (Result.Code != EPathFollowingResult::Success)
        {
            ActiveRageMoveGoalLocation = FVector::ZeroVector;
            bHasActiveRageMoveGoalLocation = false;

            APickupActor* RagePickup = Cast<APickupActor>(RageSourceActor.Get());
            APawn* ControlledPawn = GetPawn();
            if (IsValid(RagePickup) && IsValid(ControlledPawn))
            {
                FVector PickupOrigin = RagePickup->GetActorLocation();
                FVector PickupExtent = FVector::ZeroVector;
                RagePickup->GetActorBounds(true, PickupOrigin, PickupExtent);

                const FVector PawnLocation = ControlledPawn->GetActorLocation();
                const float DistanceToPickupBounds = GetDistanceToBounds2D(PawnLocation, PickupOrigin, PickupExtent);
                const float ExpandedDisableAcceptanceRadius = FMath::Clamp(
                    DistanceToPickupBounds + 20.f,
                    RageDisableItemAcceptanceRadius,
                    RageDisableItemAcceptanceRadius + MaxRageDisableRadiusExpansion);

                if (Result.Code == EPathFollowingResult::Blocked || Result.Code == EPathFollowingResult::OffPath)
                {
                    ActiveRageMoveSourceActor = RageSourceActor;
                    ActiveRageDisableAcceptanceRadius = FMath::Max(
                        ActiveRageDisableAcceptanceRadius,
                        ExpandedDisableAcceptanceRadius);
                }

                if (bLogRageThreats && (Result.Code == EPathFollowingResult::Blocked || Result.Code == EPathFollowingResult::OffPath))
                {
                    UE_LOG(
                        LogTemp,
                        Warning,
                        TEXT("RAGE THREATS MOVE_RECOVERY Current=%s Result=%d DistanceToBounds=%.1f EffectiveDisableAcceptance=%.1f"),
                        *GetNameSafe(RageSourceActor.Get()),
                        static_cast<int32>(Result.Code),
                        DistanceToPickupBounds,
                        ActiveRageDisableAcceptanceRadius);
                }
            }
        }

        return;
    }

    if (IsFearActive())
    {
        bPendingFearRepath = true;
        return;
    }

    if (CurrentState == EEnemyAIState::Investigate)
    {
        if (bLureActive)
        {
            if (Result.Code == EPathFollowingResult::Success
                || (GetPawn() && FVector::Dist2D(GetPawn()->GetActorLocation(), LureTargetLocation) <= LureAcceptanceRadius))
            {
                bLureReachedTarget = true;
                bIsMoving = false;
            }
            else
            {
                ClearLureState();
                ClearInvestigationState();
                Patrol();
            }

            return;
        }
        // 丢视野搜查的前半段允许“短时间继续跟着真实目标刷新最后位置”。
        // 这时即使一次 Move 完成了，也不要立刻开始左右环顾，
        // 因为玩家可能还在短暂跟踪窗口内继续移动。
        if (CurrentInvestigateStimulusTag == LostSightStimulusTag
            && LostSightTrackTimeRemaining > 0.f
            && IsValid(LostSightTrackedPlayer))
        {
            return;
        }

        if (bUseLookAroundAfterInvestigateMove)
        {
            BeginLookAround();
        }

        return;
    }

    if (!bCanSeePlayer)
    {
        SchedulePatrolRetry(PatrolRetryDelayOnMoveComplete);
    }
}

// 从导航系统里拿一个随机可达巡逻点。
// 注意它返回的不是随便一个世界坐标，而是尽量落在 NavMesh 上的可行走位置。
FVector AMyAIController::GetRandomPatrolPoint()
{
    if (!GetPawn())
    {
        return FVector::ZeroVector;
    }

    const FVector Origin = GetPawn()->GetActorLocation();
    FNavLocation NavLocation;

    if (UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        // GetRandomReachablePointInRadius 是 UE 导航 API，
        // 会在半径范围内找一个“可到达”的点，适合做巡逻点。
        if (NavSystem->GetRandomReachablePointInRadius(Origin, PatrolSearchRadius, NavLocation))
        {
            return NavLocation.Location;
        }
    }

    return Origin;
}

// 让外部系统用一个坐标直接把 AI 切到调查状态。
// 常见来源是听觉刺激、事件广播或者机关发出的噪声。
void AMyAIController::ApplyInvestigationFromLocation(FVector SourceLocation, float WaitDuration,
    FName StimulusTag)
{
    if (CurrentState == EEnemyAIState::Stunned || IsFearActive() || !GetPawn())
    {
        return;
    }

    if (bCanSeePlayer && IsValid(TargetPlayer))
    {
        ChasePlayer();
        return;
    }

    // 调查状态会记住目标点和停留时间。到点后不会立刻切回巡逻，而是先原地观察一小段时间。
    InvestigateTarget = SourceLocation;
    InvestigateWaitTimeRemaining = WaitDuration > 0.f ? WaitDuration : DefaultInvestigateWaitDuration;
    CurrentInvestigateStimulusTag = StimulusTag;
    bHasInvestigateTarget = true;
    bUseLookAroundAfterInvestigateMove = false;
    bIsLookAroundActive = false;
    LookAroundYawAccumulated = 0.f;
    bIsMoving = true;

    ClearPatrolRetry();
    SetAIState(EEnemyAIState::Investigate);

    if (FVector::Dist2D(GetPawn()->GetActorLocation(), InvestigateTarget) <= InvestigateAcceptanceRadius)
    {
        // Dist2D 是 UE 提供的二维距离计算，忽略高度差，
        // 对地面角色来说通常比三维距离更适合判断“是否到点”。
        bIsMoving = false;
    }
    else
    {
        // StopMovement 是 AAIController 自带 API，表示取消当前路径跟随。
        StopMovement();
        MoveToInvestigateTarget();
    }
}

void AMyAIController::ApplyLureFromLocation(FVector SourceLocation, float DazeDuration,
    float AcceptanceRadius, AActor* SourceActor)
{
    APawn* ControlledPawn = GetPawn();
    if (!IsValid(ControlledPawn)
        || CurrentState == EEnemyAIState::Stunned
        || CurrentState == EEnemyAIState::Rage
        || IsFearActive())
    {
        return;
    }

    LureTargetLocation = SourceLocation;
    LureSourceActor = SourceActor;
    LureDazeTimeRemaining = FMath::Max(0.1f, DazeDuration);
    LureAcceptanceRadius = FMath::Max(30.f, AcceptanceRadius);
    bLureActive = true;
    bLureReachedTarget = FVector::Dist2D(ControlledPawn->GetActorLocation(), LureTargetLocation) <= LureAcceptanceRadius;

    bCanSeePlayer = false;
    TargetPlayer = nullptr;
    ClearPatrolRetry();
    ClearInvestigationState();
    SetAIState(EEnemyAIState::Investigate);

    InvestigateTarget = LureTargetLocation;
    bHasInvestigateTarget = true;
    CurrentInvestigateStimulusTag = TEXT("GhostLure");
    InvestigateWaitTimeRemaining = LureDazeTimeRemaining;

    StopMovement();
    ActiveMoveRequestId = FAIRequestID::InvalidRequest;

    if (bLureReachedTarget)
    {
        bIsMoving = false;
        return;
    }

    bIsMoving = RequestMoveToLocation(LureTargetLocation, LureAcceptanceRadius);
}

void AMyAIController::ApplySlowSource(AActor* SourceActor, float SlowMultiplier)
{
    if (!IsValid(SourceActor))
    {
        return;
    }

    const float CurrentUnmodifiedSpeed = GetCurrentUnmodifiedWalkSpeed();

    CleanupInvalidSlowSources();
    ActiveSlowSources.FindOrAdd(SourceActor) = FMath::Clamp(SlowMultiplier, 0.05f, 1.f);

    if (CurrentUnmodifiedSpeed > 0.f)
    {
        TrackedUnmodifiedWalkSpeed = CurrentUnmodifiedSpeed;
        bHasTrackedUnmodifiedWalkSpeed = true;
    }

    RefreshControlledWalkSpeedForCurrentState();
}

void AMyAIController::RemoveSlowSource(AActor* SourceActor)
{
    if (!SourceActor)
    {
        return;
    }

    const float CurrentUnmodifiedSpeed = GetCurrentUnmodifiedWalkSpeed();

    CleanupInvalidSlowSources();
    ActiveSlowSources.Remove(SourceActor);

    if (CurrentUnmodifiedSpeed > 0.f)
    {
        TrackedUnmodifiedWalkSpeed = CurrentUnmodifiedSpeed;
        bHasTrackedUnmodifiedWalkSpeed = true;
    }

    RefreshControlledWalkSpeedForCurrentState();
}

void AMyAIController::ApplySpeedReductionSource(AActor* SourceActor, float SpeedReduction)
{
    if (!IsValid(SourceActor))
    {
        return;
    }

    const float CurrentUnmodifiedSpeed = GetCurrentUnmodifiedWalkSpeed();

    CleanupInvalidSpeedReductionSources();
    ActiveSpeedReductionSources.FindOrAdd(SourceActor) = FMath::Max(0.f, SpeedReduction);

    if (CurrentUnmodifiedSpeed > 0.f)
    {
        TrackedUnmodifiedWalkSpeed = CurrentUnmodifiedSpeed;
        bHasTrackedUnmodifiedWalkSpeed = true;
    }

    RefreshControlledWalkSpeedForCurrentState();
}

void AMyAIController::RemoveSpeedReductionSource(AActor* SourceActor)
{
    if (!SourceActor)
    {
        return;
    }

    const float CurrentUnmodifiedSpeed = GetCurrentUnmodifiedWalkSpeed();

    CleanupInvalidSpeedReductionSources();
    ActiveSpeedReductionSources.Remove(SourceActor);

    if (CurrentUnmodifiedSpeed > 0.f)
    {
        TrackedUnmodifiedWalkSpeed = CurrentUnmodifiedSpeed;
        bHasTrackedUnmodifiedWalkSpeed = true;
    }

    RefreshControlledWalkSpeedForCurrentState();
}

void AMyAIController::ApplyFlashlightReveal(float RevealDuration)
{
    const float ResolvedRevealDuration = RevealDuration > 0.f
        ? RevealDuration
        : DefaultFlashlightRevealDuration;

    if (ResolvedRevealDuration <= 0.f)
    {
        return;
    }

    bFlashlightRevealActive = true;
    FlashlightRevealTimeRemaining = FMath::Max(FlashlightRevealTimeRemaining, ResolvedRevealDuration);
    ApplyGhostVisualState();
}

void AMyAIController::ApplyWeakness(float Duration)
{
    if (Duration <= 0.f)
    {
        return;
    }

    const bool bWasRevealedByEffect = IsGhostRevealedByEffect();
    const bool bRestartWeaknessVisualCycle = !bWeaknessActive;

    bWeaknessActive = true;
    WeaknessTimeRemaining = FMath::Max(WeaknessTimeRemaining, Duration);

    // 像十字架区域这种持续刷新型来源，只应该“续时”，不应该每次都把闪烁节奏打回起点。
    if (bRestartWeaknessVisualCycle)
    {
        bWeaknessRevealPhaseVisible = true;
        WeaknessRevealToggleTimeAccumulator = 0.f;
    }

    if (bWasRevealedByEffect != IsGhostRevealedByEffect())
    {
        ApplyGhostVisualState();
    }
}

void AMyAIController::ClearWeakness()
{
    const bool bWasWeaknessVisible = bWeaknessActive && bWeaknessRevealPhaseVisible;

    bWeaknessActive = false;
    bWeaknessRevealPhaseVisible = false;
    WeaknessTimeRemaining = 0.f;
    WeaknessRevealToggleTimeAccumulator = 0.f;

    if (bWasWeaknessVisible)
    {
        ApplyGhostVisualState();
    }
}

bool AMyAIController::IsWeaknessActive() const
{
    return bWeaknessActive;
}

void AMyAIController::ApplyFearAngerStimulus(const FFearStimulus& Stimulus)
{
    if (!bReactToFearStimuli)
    {
        return;
    }

    FFearStimulus NormalizedStimulus = Stimulus;
    if (NormalizedStimulus.SourceActor == GetPawn() || NormalizedStimulus.SourceActor == this)
    {
        NormalizedStimulus.SourceActor = nullptr;
    }

    // 手电这类刺激不应该切入 Fear/Flee，
    // 但仍要更新 Rage 来源缓存，让鬼进 Rage 后知道该去拆谁。
    ActiveFearStimulus = NormalizedStimulus;
    RegisterRageThreatSource(ActiveFearStimulus.SourceActor);
    RegisterFearAngerStimulus(ActiveFearStimulus);
}

float AMyAIController::GetActiveSlowMultiplier() const
{
    float ResolvedMultiplier = 1.f;

    for (const TPair<TWeakObjectPtr<AActor>, float>& Entry : ActiveSlowSources)
    {
        if (!Entry.Key.IsValid())
        {
            continue;
        }

        ResolvedMultiplier = FMath::Min(ResolvedMultiplier, FMath::Clamp(Entry.Value, 0.05f, 1.f));
    }

    return ResolvedMultiplier;
}

float AMyAIController::GetActiveSpeedReductionAmount() const
{
    float TotalReduction = 0.f;

    for (const TPair<TWeakObjectPtr<AActor>, float>& Entry : ActiveSpeedReductionSources)
    {
        if (!Entry.Key.IsValid())
        {
            continue;
        }

        TotalReduction += FMath::Max(0.f, Entry.Value);
    }

    return TotalReduction;
}

// 恐惧正式生效时的内部入口。
// ApplyFearStimulus 在判定为一次新的惊吓后，会直接调用这里。
// 它会缓存速度、切到 Flee，并打断当前移动，让下一帧重新规划逃跑路线。
void AMyAIController::HandleFearApplied(FFearStimulus Stimulus)
{
    if (CurrentState == EEnemyAIState::Rage)
    {
        ClearFear();
        return;
    }

    // 先缓存原速度，再根据 Controller 上的 Fear 配置切入逃跑速度。
    CacheWalkSpeedIfNeeded();

    // 受惊会立刻打断当前移动，并在下一帧重新规划远离路径。
    SetAIState(EEnemyAIState::Flee);
    ClearPatrolRetry();
    ClearInvestigationState();
    bPendingFearRepath = true;
    CurrentFearHidePoint = nullptr;
    bFearHoldingAtHidePoint = false;
    ClearFearDetour();
    bForcePatrolAfterFearEnds = false;
    FearHideEscapeTimeRemaining = 0.f;
    StopMovement();
    EnterFearRetreatState(true);
}

// 处理视觉刺激。
// 什么时候调用：OnPerceptionUpdated 判断当前刺激属于 Sight 感知时。
// 这里要处理两件事：
// 1. 看到玩家时立刻锁定并准备追逐。
// 2. 当前追逐目标刚丢失时，转入“丢视野后搜查”。
void AMyAIController::HandleSightStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
    if (!Actor->IsA<ACharacter>())
    {
        return;
    }

    const bool bWasSuccessfullySensed = Stimulus.WasSuccessfullySensed();

    if (bWasSuccessfullySensed)
    {
        // 看见玩家时，追击会压过巡逻和调查；但如果正在 Flee，仍然先保留 Fear 优先级。
        RecordLastSeenPlayerData(Actor, &Stimulus);
        bCanSeePlayer = true;
        TargetPlayer = Actor;
        PendingLostSightActor = nullptr;
        LostSightConfirmationTimeRemaining = 0.f;

        if (!IsFearActive())
        {
            const bool bWasAlreadyChasing = CurrentState == EEnemyAIState::Chase;
            // 这里先停掉旧移动并切状态。
            // 真正的持续追逐请求由 Tick 每帧调用 ChasePlayer 来统一下发。
            ClearPatrolRetry();
            if (!bWasAlreadyChasing)
            {
                StopMovement();
            }
            ClearInvestigationState();
            SetAIState(EEnemyAIState::Chase);
            ChasePlayer();
        }
        return;
    }



    if (TargetPlayer == Actor)
    {
        if (AcquireVisiblePlayerTarget())
        {
            if (!IsFearActive())
            {
                const bool bWasAlreadyChasing = CurrentState == EEnemyAIState::Chase;
                ClearPatrolRetry();
                if (!bWasAlreadyChasing)
                {
                    StopMovement();
                }
                ClearInvestigationState();
                SetAIState(EEnemyAIState::Chase);
                ChasePlayer();
            }

            return;
        }

        PendingLostSightActor = Actor;
        LostSightConfirmationTimeRemaining = LostSightConfirmationDelay;

        if (LostSightConfirmationTimeRemaining > 0.f)
        {
            return;
        }

        PendingLostSightActor = nullptr;

        bCanSeePlayer = false;
        TargetPlayer = nullptr;

        if (!IsFearActive())
        {
            StartLostSightInvestigation(Actor);
        }
    }
}

// 处理听觉刺激。
// 什么时候调用：OnPerceptionUpdated 判断刺激来自 Hearing 时。
// 如果当前已经稳定看到玩家，就保持追逐，不让普通噪声打断更高优先级行为。
void AMyAIController::HandleHearingStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
    if (!Stimulus.WasSuccessfullySensed())
    {
        return;
    }

    // 如果已经稳定看到玩家，可选择忽略噪声，防止追击状态被调查状态打断。
    if (bIgnoreNoiseWhileSeeingPlayer && bCanSeePlayer && IsValid(TargetPlayer))
    {
        return;
    }

    const FVector HeardLocation = !Stimulus.StimulusLocation.IsNearlyZero()
        ? Stimulus.StimulusLocation
        : Actor->GetActorLocation();
    const FName NoiseTag = Stimulus.Tag != NAME_None ? Stimulus.Tag : TEXT("HeardNoise");

    ApplyInvestigationFromLocation(HeardLocation, DefaultInvestigateWaitDuration, NoiseTag);
}

// 缓存最近一次确认到的玩家位置。
// 这个位置会作为“丢失视野后搜查”的基础数据使用。
// 如果感知刺激本身带了更准确的 StimulusLocation，就优先用它。
void AMyAIController::RecordLastSeenPlayerData(AActor* Actor, const FAIStimulus* Stimulus)
{
    if (!IsValid(Actor))
    {
        return;
    }

    FVector SeenLocation = Actor->GetActorLocation();
    // StimulusLocation 往往代表“真正被感知系统确认到的位置”，
    // 可能比 Actor 当前根节点位置更接近 AI 当时实际看到/听到的位置。
    // 只有在指针有效且位置非零时才覆盖，避免把默认零向量误写成最后已知点。
    if (Stimulus && !Stimulus->StimulusLocation.IsNearlyZero())
    {
        SeenLocation = Stimulus->StimulusLocation;
    }

    LastSeenPlayerLocation = SeenLocation;
    bHasLastSeenPlayerData = true;
}

// 在多人场景下重新找一个当前仍在视野内的玩家。
// 什么时候调用：当前目标丢失时、Fear 结束恢复时。
// 当前策略很直接：谁离 AI 最近，就先追谁。
bool AMyAIController::AcquireVisiblePlayerTarget()
{
    if (!AIPerception)
    {
        return false;
    }

    TArray<AActor*> VisibleActors;

    // GetCurrentlyPerceivedActors 是 UAIPerceptionComponent 的查询 API，
    // 用来获取“当前这个感知类型仍然感知中的 Actor 列表”。
    AIPerception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), VisibleActors);

    AActor* BestTarget = nullptr;
    float BestDistanceSquared = TNumericLimits<float>::Max();
    const FVector PawnLocation = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;

    for (AActor* Candidate : VisibleActors)
    {
        // 三类候选要直接排除：
        // 1. 已失效对象。
        // 2. 自己本体。
        // 3. 不是 Character 的对象，因为当前追击系统默认只针对玩家/角色类目标。
        if (!IsValid(Candidate) || Candidate == GetPawn() || !Candidate->IsA<ACharacter>())
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(PawnLocation, Candidate->GetActorLocation());
        // BestTarget 为空表示“这是第一个合法目标”；
        // 否则就保留离当前 AI 最近的那个，作为简单稳定的多目标选择策略。
        if (!BestTarget || DistanceSquared < BestDistanceSquared)
        {
            BestTarget = Candidate;
            BestDistanceSquared = DistanceSquared;
        }
    }

    if (!BestTarget)
    {
        bCanSeePlayer = false;
        TargetPlayer = nullptr;
        return false;
    }

    TargetPlayer = BestTarget;
    bCanSeePlayer = true;
    PendingLostSightActor = nullptr;
    LostSightConfirmationTimeRemaining = 0.f;
    RecordLastSeenPlayerData(BestTarget);
    return true;
}

// 当前追逐目标从视野里消失时进入这里。
// 它会启动完整的丢视野搜查流程：
// 1. 先保留短时间真实位置跟踪。
// 2. 跑到最后已知点。
// 3. 到点后做“鬼式左右环顾”。
void AMyAIController::StartLostSightInvestigation(AActor* LostActor)
{
    if (IsFearActive() || !GetPawn())
    {
        return;
    }

    // 刚丢掉当前目标时，先试一次“当前视野里还有没有别的玩家”。
    // 这样多人场景下不会因为一个玩家出视野就盲目切调查，
    // 而是优先保持追击节奏。
    if (AcquireVisiblePlayerTarget())
    {
        ChasePlayer();
        return;
    }

    // 如果连最后确认位置都没有，就无法启动搜查流程；
    // 这时回巡逻比冲向零向量或旧数据更安全。
    if (!bHasLastSeenPlayerData)
    {
        Patrol();
        return;
    }

    LostSightTrackedPlayer = LostActor;
    LostSightSearchTimeRemaining = FMath::Max(LostSightMaxSearchDuration, LostSightTrackDuration);
    LostSightTrackTimeRemaining = LostSightTrackDuration;
    if (IsValid(LostSightTrackedPlayer))
    {
        RecordLastSeenPlayerData(LostSightTrackedPlayer);
    }

    InvestigateTarget = GetLostSightInvestigateLocation();
    InvestigateWaitTimeRemaining = 0.f;
    CurrentInvestigateStimulusTag = LostSightStimulusTag;
    bHasInvestigateTarget = true;
    bUseLookAroundAfterInvestigateMove = true;
    bIsLookAroundActive = false;
    LookAroundYawAccumulated = 0.f;
    bIsMoving = true;

    ClearPatrolRetry();
    SetAIState(EEnemyAIState::Investigate);

    // StopMovement 用来取消之前可能还在执行的追逐路径，
    // 否则 AI 可能会沿着旧路径继续跑，影响搜查动作。
    StopMovement();

    if (FVector::Dist2D(GetPawn()->GetActorLocation(), InvestigateTarget) <= InvestigateAcceptanceRadius)
    {
        bIsMoving = false;
        // 已经站在最后已知点附近时，只有在“真实跟踪窗口”也结束了，
        // 才允许直接进入环顾；否则仍要给短暂继续修正位置的机会。
        if (LostSightTrackTimeRemaining <= 0.f || !IsValid(LostSightTrackedPlayer))
        {
            BeginLookAround();
        }
        return;
    }

    MoveToInvestigateTarget();
    // 这里覆盖一种极端情况：MoveTo 提交失败，或者目标点就在接受半径边缘，
    // 导致根本没进入移动状态。
    // 如果此时也已经没有可继续跟踪的玩家，就直接开始环顾，避免卡住。
    if (!bIsMoving && (LostSightTrackTimeRemaining <= 0.f || !IsValid(LostSightTrackedPlayer)))
    {
        BeginLookAround();
    }
}

// 调查/搜查状态的逐帧更新逻辑。
// 什么时候调用：Tick 里当前状态为 Investigate 时，每帧调用一次。
// 它负责：
// 1. 维护丢视野后的总搜查时长上限。
// 2. 在短时间内持续读取玩家真实位置并重规划。
// 3. 到点后进入环顾阶段。
void AMyAIController::UpdateInvestigation(float DeltaTime)
{
    if (!bHasInvestigateTarget)
    {
        Patrol();
        return;
    }

    // 只有“丢视野搜查”这类调查，才需要维护两套额外计时：
    // 1. 总搜查时长 LostSightSearchTimeRemaining。
    // 2. 还能继续读取真实玩家位置的 LostSightTrackTimeRemaining。
    // 普通听觉调查不会进这套分支。
    if (CurrentInvestigateStimulusTag == LostSightStimulusTag)
    {
        LostSightSearchTimeRemaining = FMath::Max(0.f, LostSightSearchTimeRemaining - DeltaTime);
        if (LostSightSearchTimeRemaining <= 0.f)
        {
            ClearInvestigationState();
            Patrol();
            return;
        }

        if (LostSightTrackTimeRemaining > 0.f && IsValid(LostSightTrackedPlayer))
        {
            LostSightTrackTimeRemaining = FMath::Max(0.f, LostSightTrackTimeRemaining - DeltaTime);
            RecordLastSeenPlayerData(LostSightTrackedPlayer);

            const FVector UpdatedTarget = GetLostSightInvestigateLocation();
            const float RepathDistanceThreshold = FMath::Max(InvestigateAcceptanceRadius * 0.5f, 30.f);

            if (FVector::Dist2D(InvestigateTarget, UpdatedTarget) > RepathDistanceThreshold)
            {
                InvestigateTarget = UpdatedTarget;

                if (FVector::Dist2D(GetPawn()->GetActorLocation(), InvestigateTarget) > InvestigateAcceptanceRadius)
                {
                    // 当目标点明显变化时，重新下发一次导航请求，
                    // 这样 AI 才会跟着最新位置继续搜，而不是跑向旧点。
                    StopMovement();
                    MoveToInvestigateTarget();
                }
            }
        }
        else
        {
            LostSightTrackTimeRemaining = 0.f;
            LostSightTrackedPlayer = nullptr;
        }
    }

    // Investigate 分成两个阶段：路上先移动，到点后再倒计时等待。
    if (bIsMoving)
    {
        return;
    }

    if (bUseLookAroundAfterInvestigateMove)
    {
        if (!bIsLookAroundActive)
        {
            // 丢视野搜查里，只有“真实位置跟踪窗口”结束后，
            // 才开始原地左右看。
            // 否则 AI 会刚跑到点就转圈，错过继续修正目标点的机会。
            if (CurrentInvestigateStimulusTag == LostSightStimulusTag && LostSightTrackTimeRemaining > 0.f)
            {
                return;
            }

            BeginLookAround();
            return;
        }

        UpdateLookAround(DeltaTime);
        return;
    }

    InvestigateWaitTimeRemaining -= DeltaTime;
    if (InvestigateWaitTimeRemaining <= 0.f)
    {
        ClearInvestigationState();
        Patrol();
    }
}

void AMyAIController::UpdateLureState(float DeltaTime)
{
    APawn* ControlledPawn = GetPawn();
    if (!IsValid(ControlledPawn))
    {
        ClearLureState();
        return;
    }

    if (IsValid(LureSourceActor))
    {
        LureTargetLocation = LureSourceActor->GetActorLocation();
    }

    if (!bLureReachedTarget)
    {
        if (FVector::Dist2D(ControlledPawn->GetActorLocation(), LureTargetLocation) <= LureAcceptanceRadius)
        {
            StopMovement();
            ActiveMoveRequestId = FAIRequestID::InvalidRequest;
            bIsMoving = false;
            bLureReachedTarget = true;
            return;
        }

        if (!IsPathFollowingMoveActive(this))
        {
            SetAIState(EEnemyAIState::Investigate);
            bIsMoving = RequestMoveToLocation(LureTargetLocation, LureAcceptanceRadius);
        }

        return;
    }

    StopMovement();
    ActiveMoveRequestId = FAIRequestID::InvalidRequest;
    bIsMoving = false;

    FVector LookDirection = LureTargetLocation - ControlledPawn->GetActorLocation();
    LookDirection.Z = 0.f;
    if (!LookDirection.IsNearlyZero())
    {
        SetControlRotation(LookDirection.Rotation());
    }

    LureDazeTimeRemaining -= DeltaTime;
    if (LureDazeTimeRemaining <= 0.f)
    {
        ClearLureState();
        ClearInvestigationState();
        Patrol();
    }
}

void AMyAIController::ClearLureState()
{
    bLureActive = false;
    bLureReachedTarget = false;
    LureDazeTimeRemaining = 0.f;
    LureAcceptanceRadius = 120.f;
    LureTargetLocation = FVector::ZeroVector;
    LureSourceActor = nullptr;
}



// 把当前调查目标提交给导航系统。
// 本质上是对 MoveToLocation 的语义化包装，让上层不需要重复关心细节。
void AMyAIController::MoveToInvestigateTarget()
{
    if (!bHasInvestigateTarget)
    {
        return;
    }

    SetAIState(EEnemyAIState::Investigate);
    bIsMoving = RequestMoveToLocation(InvestigateTarget, InvestigateAcceptanceRadius);
}

// 开始执行“鬼式搜查动作”。
// 调用时机：已经走到最后已知位置，并且短暂的真实位置跟踪时间已经结束。
// 这里先把朝向对准最后位置，再进入左扫、右扫、回正几个阶段。
void AMyAIController::BeginLookAround()
{
    if (!GetPawn())
    {
        ClearInvestigationState();
        Patrol();
        return;
    }

    // sweep 角度或转向速度被配成非正数时，环顾动作无法成立：
    // 要么完全不转，要么没有左右扫的幅度。
    // 这里直接结束调查，防止 AI 卡在一个永远完成不了的“看一圈”状态。
    if (LostSightLookAroundSweepDegrees <= 0.f || LostSightLookAroundTurnRateDegrees <= 0.f)
    {
        ClearInvestigationState();
        Patrol();
        return;
    }

    bUseLookAroundAfterInvestigateMove = true;
    bIsLookAroundActive = true;
    LookAroundYawAccumulated = 0.f;
    LookAroundPhase = ELostSightLookAroundPhase::FaceLastKnown;

    FVector SearchForward = LastSeenPlayerLocation - GetPawn()->GetActorLocation();
    SearchForward.Z = 0.f;

    const float FallbackYaw = GetControlRotation().Yaw;
    LookAroundCenterYaw = !SearchForward.IsNearlyZero() ? SearchForward.Rotation().Yaw : FallbackYaw;
    LookAroundTargetYaw = LookAroundCenterYaw;
}

// 逐帧推进搜查环顾。
// 什么时候调用：UpdateInvestigation 判断已经进入环顾阶段时，每帧调用一次。
// 它不是整圈匀速旋转，而是平滑朝当前目标角度转过去，到了再切下一段。
void AMyAIController::UpdateLookAround(float DeltaTime)
{
    if (!bIsLookAroundActive || !GetPawn())
    {
        ClearInvestigationState();
        Patrol();
        return;
    }

    if (LookAroundPhase == ELostSightLookAroundPhase::Completed)
    {
        ClearInvestigationState();
        Patrol();
        return;
    }

    const float CurrentYaw = GetControlRotation().Yaw;
    const float MaxTurnStep = LostSightLookAroundTurnRateDegrees * DeltaTime;

    // FMath::FixedTurn 是 UE 的角度插值辅助函数，
    // 可以理解为“本帧最多转这么多度，逐渐靠近目标角度”。
    const float NewYawValue = FMath::FixedTurn(CurrentYaw, LookAroundTargetYaw, MaxTurnStep);

    FRotator NewRotation = GetControlRotation();
    NewRotation.Yaw = NewYawValue;

    // SetControlRotation 改的是 Controller 朝向；
    // FaceRotation 让 Pawn 本体也跟着旋转，否则视觉上可能不会真的转过去。
    SetControlRotation(NewRotation);
    GetPawn()->FaceRotation(NewRotation, DeltaTime);

    LookAroundYawAccumulated += FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentYaw, NewYawValue));

    if (FMath::Abs(FMath::FindDeltaAngleDegrees(NewYawValue, LookAroundTargetYaw)) <= LookAroundYawToleranceDegrees)
    {
        AdvanceLookAroundPhase();
    }
}

// 切换到环顾的下一阶段。
// 当前顺序固定为：看向最后位置 -> 左扫 -> 右扫 -> 回正 -> 结束。
void AMyAIController::AdvanceLookAroundPhase()
{
    switch (LookAroundPhase)
    {
    case ELostSightLookAroundPhase::FaceLastKnown:
        LookAroundPhase = ELostSightLookAroundPhase::SweepLeft;
        LookAroundTargetYaw = LookAroundCenterYaw - LostSightLookAroundSweepDegrees;
        break;
    case ELostSightLookAroundPhase::SweepLeft:
        LookAroundPhase = ELostSightLookAroundPhase::SweepRight;
        LookAroundTargetYaw = LookAroundCenterYaw + LostSightLookAroundSweepDegrees;
        break;
    case ELostSightLookAroundPhase::SweepRight:
        LookAroundPhase = ELostSightLookAroundPhase::ReturnCenter;
        LookAroundTargetYaw = LookAroundCenterYaw;
        break;
    case ELostSightLookAroundPhase::ReturnCenter:
        LookAroundPhase = ELostSightLookAroundPhase::Completed;
        bIsLookAroundActive = false;
        ClearInvestigationState();
        Patrol();
        break;
    case ELostSightLookAroundPhase::Completed:
    default:
        break;
    }
}

// 清空调查和搜查相关的临时数据。
// 什么时候调用：调查结束、搜查超时、被追逐/Fear 等更高优先级行为中断时。
// 它只清理 Investigate 这一整套状态，不会去改 Fear 或追逐目标缓存。
void AMyAIController::ClearInvestigationState()
{
    bHasInvestigateTarget = false;
    InvestigateWaitTimeRemaining = 0.f;
    InvestigateTarget = FVector::ZeroVector;
    CurrentInvestigateStimulusTag = NAME_None;
    LostSightTrackedPlayer = nullptr;
    LostSightSearchTimeRemaining = 0.f;
    LostSightTrackTimeRemaining = 0.f;
    PendingLostSightActor = nullptr;
    LostSightConfirmationTimeRemaining = 0.f;
    bUseLookAroundAfterInvestigateMove = false;
    bIsLookAroundActive = false;
    LookAroundCenterYaw = 0.f;
    LookAroundTargetYaw = 0.f;
    LookAroundYawAccumulated = 0.f;
    LookAroundPhase = ELostSightLookAroundPhase::Completed;
}

// 对 UE 自带 MoveToActor 的简单包装。
// MoveToActor 的特点是：目标 Actor 位置变化时，AI 会继续朝这个 Actor 追过去。
// 这很适合“追逐玩家”这种动态目标。
bool AMyAIController::RequestMoveToActor(AActor* GoalActor, float AcceptanceRadius)
{
    if (!IsValid(GoalActor))
    {
        ActiveMoveRequestId = FAIRequestID::InvalidRequest;
        return false;
    }

    FAIMoveRequest MoveRequest;
    MoveRequest.SetGoalActor(GoalActor);
    MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
    MoveRequest.SetUsePathfinding(true);

    const FPathFollowingRequestResult MoveResult = MoveTo(MoveRequest);
    if (MoveResult.Code == EPathFollowingRequestResult::RequestSuccessful)
    {
        ActiveMoveRequestId = MoveResult.MoveId;
        return true;
    }

    ActiveMoveRequestId = FAIRequestID::InvalidRequest;
    return false;
}

// 对 UE 自带 MoveToLocation 的简单包装。
// 和 MoveToActor 不同，它追的是固定坐标，不会自动跟随目标移动。
// 返回值表示“这次导航请求有没有成功提交给路径系统”。
bool AMyAIController::RequestMoveToLocation(const FVector& GoalLocation, float AcceptanceRadius)
{
    FAIMoveRequest MoveRequest;
    MoveRequest.SetGoalLocation(GoalLocation);
    MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
    MoveRequest.SetUsePathfinding(true);

    const FPathFollowingRequestResult MoveResult = MoveTo(MoveRequest);
    if (MoveResult.Code == EPathFollowingRequestResult::RequestSuccessful)
    {
        ActiveMoveRequestId = MoveResult.MoveId;
        return true;
    }

    ActiveMoveRequestId = FAIRequestID::InvalidRequest;
    return false;
}

// 安排一次“延迟后再巡逻”。
// 什么时候调用：巡逻目标无效、巡逻移动失败、巡逻走完后准备下一轮时。
// 之所以不用立刻 Patrol，是为了避免回调里形成高频循环。
void AMyAIController::SchedulePatrolRetry(float Delay)
{
    if (!GetWorld())
    {
        return;
    }

    // 只在“真的该回巡逻”时才安排重试。
    // 如果已经看到玩家，或者已经在 Investigate，延迟触发的 Patrol 会把更高优先级状态抢掉。
    if (bCanSeePlayer
        || CurrentState == EEnemyAIState::Investigate
        || CurrentState == EEnemyAIState::Stunned)
    {
        return;
    }

    // Fear 期间同样不能安排巡逻重试，
    // 否则定时器可能在受惊流程中途触发，造成状态争用。
    if (IsFearActive())
    {
        return;
    }

    const float RetryDelay = Delay >= 0.f ? Delay : FailedMoveRetryDelay;

    // GetWorldTimerManager 是 UE 的定时器入口；
    // SetTimer 会在延迟后自动调用指定成员函数。
    GetWorldTimerManager().ClearTimer(PatrolRetryTimerHandle);
    GetWorldTimerManager().SetTimer(PatrolRetryTimerHandle, this, &AMyAIController::Patrol, RetryDelay, false);
}

// 清除尚未触发的巡逻重试定时器。
// 一旦进入追逐、调查、Fear 等更高优先级状态，就应该先取消旧的巡逻计划。
void AMyAIController::ClearPatrolRetry()
{
    if (GetWorld())
    {
        GetWorldTimerManager().ClearTimer(PatrolRetryTimerHandle);
    }
}

// 按当前 Controller 记录的恐惧源重新规划一次逃跑路径。
// 什么时候调用：Fear 持续期间需要重新算 flee 目标时。
void AMyAIController::MoveAwayFromFearSource()
{
    if (!GetPawn() || !IsFearActive())
    {
        return;
    }

    // 旧逻辑是直接朝反方向跑；现在这里保留成一个兼容入口，
    // 但实际会切回新的 Fear 第一阶段：先后退，再尝试逃向躲藏点。
    EnterFearRetreatState(true);
}

// 把“最后看到玩家的位置”转换成真正可走的调查点。
// 因为记录下来的坐标不一定正好落在 NavMesh 上，所以先尝试投射到导航网格。
FVector AMyAIController::GetLostSightInvestigateLocation() const
{
    const FVector DesiredLocation = LastSeenPlayerLocation;

    if (UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        FNavLocation NavLocation;

        // ProjectPointToNavigation 是 UE 导航 API，
        // 作用是把普通世界坐标投影到最近的可导航区域。
        if (NavSystem->ProjectPointToNavigation(DesiredLocation, NavLocation, FVector(250.f, 250.f, 200.f)))
        {
            return NavLocation.Location;
        }

        if (NavSystem->ProjectPointToNavigation(LastSeenPlayerLocation, NavLocation, FVector(200.f, 200.f, 200.f)))
        {
            return NavLocation.Location;
        }
    }

    return DesiredLocation;
}

// 根据恐惧源位置计算一个“远离它”的 flee 点。
// 什么时候调用：Fear 状态需要发起逃跑，或者逃跑途中需要重新规划路径时。
FVector AMyAIController::GetFleeLocationFromSource(FVector SourceLocation) const
{
    if (!GetPawn())
    {
        return FVector::ZeroVector;
    }

    // 逃跑方向 = Pawn 位置减去恐惧源位置，也就是“背离刺激源”的水平向量。
    const FVector PawnLocation = GetPawn()->GetActorLocation();
    FVector FleeDirection = PawnLocation - SourceLocation;
    FleeDirection.Z = 0.f;

    if (FleeDirection.IsNearlyZero())
    {
        // 如果恐惧源和 Pawn 位置几乎重合，就退化为朝自身前方反向逃开。
        FleeDirection = -GetPawn()->GetActorForwardVector();
        FleeDirection.Z = 0.f;
    }

    FleeDirection.Normalize();
    const float EscapeDistance = GetFearEscapeDistance();
    const FVector DesiredLocation = PawnLocation + FleeDirection * EscapeDistance;

    // 先尝试把理想逃跑点投射到导航网格上；失败时再在附近随机找一个可达点兜底。
    if (UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        FNavLocation NavLocation;
        if (NavSystem->ProjectPointToNavigation(DesiredLocation, NavLocation, FVector(300.f, 300.f, 200.f)))
        {
            return NavLocation.Location;
        }

        if (NavSystem->GetRandomReachablePointInRadius(DesiredLocation, EscapeDistance * 0.35f, NavLocation))
        {
            return NavLocation.Location;
        }
    }

    return DesiredLocation;
}

// 取得“正常状态下”的基准移动速度。
// 优先使用 Fear 发生前缓存下来的速度，这样 Fear/Rage 都能基于同一份基础速度计算。
float AMyAIController::GetBaseWalkSpeedForState() const
{
    if (bCachedWalkSpeedValid)
    {
        return CachedWalkSpeedBeforeFear;
    }

    if (bHasTrackedUnmodifiedWalkSpeed)
    {
        return TrackedUnmodifiedWalkSpeed;
    }

    const float CurrentUnmodifiedSpeed = GetCurrentUnmodifiedWalkSpeed();
    if (CurrentUnmodifiedSpeed > 0.f)
    {
        return CurrentUnmodifiedSpeed;
    }

    return 300.f;
}

// Fear 后退阶段使用的速度，明显慢于正常速度。
float AMyAIController::ResolveFearRetreatWalkSpeed() const
{
    return FMath::Max(GetBaseWalkSpeedForState() * FearRetreatSpeedMultiplier, FearRetreatMinWalkSpeed);
}

// Fear 躲藏点逃跑阶段使用的速度，比后退快，让鬼看起来是在急着找地方藏起来。
float AMyAIController::ResolveFearHideRunWalkSpeed() const
{
    return FMath::Max(GetBaseWalkSpeedForState() * FearHideRunSpeedMultiplier, FearHideRunMinWalkSpeed);
}

// Rage 期间使用的速度，显著高于普通移动速度。
float AMyAIController::ResolveRageWalkSpeed() const
{
    return FMath::Max(GetBaseWalkSpeedForState() * RageSpeedMultiplier, RageMinWalkSpeed);
}

// 直接修改当前被控制角色的移动速度。
// 这里依赖的是 CharacterMovementComponent，它是 UE 角色移动系统的核心组件之一。
void AMyAIController::SetControlledWalkSpeed(float NewWalkSpeed)
{
    TrackedUnmodifiedWalkSpeed = NewWalkSpeed;
    bHasTrackedUnmodifiedWalkSpeed = true;

    if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn()))
    {
        if (UCharacterMovementComponent* MovementComponent = ControlledCharacter->GetCharacterMovement())
        {
            const float ReducedWalkSpeed = FMath::Max(0.f, NewWalkSpeed - GetActiveSpeedReductionAmount());
            MovementComponent->MaxWalkSpeed = ReducedWalkSpeed * GetActiveSlowMultiplier();
        }
    }
}

// Fear 结束后，把移动速度恢复到进入 Fear 之前缓存下来的值。
void AMyAIController::RestoreWalkSpeedAfterFear()
{
    if (bCachedWalkSpeedValid)
    {
        SetControlledWalkSpeed(CachedWalkSpeedBeforeFear);
    }

    bCachedWalkSpeedValid = false;
}

// 只在第一次需要时缓存基础速度。
// 这样多次触发 Fear 也不会把“已经被改过的速度”错误地再次当作原始速度保存。
void AMyAIController::CacheWalkSpeedIfNeeded()
{
    if (bCachedWalkSpeedValid)
    {
        return;
    }

    const float CurrentUnmodifiedSpeed = GetCurrentUnmodifiedWalkSpeed();
    if (CurrentUnmodifiedSpeed > 0.f)
    {
        CachedWalkSpeedBeforeFear = CurrentUnmodifiedSpeed;
        bCachedWalkSpeedValid = true;
        TrackedUnmodifiedWalkSpeed = CurrentUnmodifiedSpeed;
        bHasTrackedUnmodifiedWalkSpeed = true;
    }
}

void AMyAIController::RefreshControlledWalkSpeedForCurrentState()
{
    CleanupInvalidSlowSources();
    CleanupInvalidSpeedReductionSources();

    float TargetWalkSpeed = GetBaseWalkSpeedForState();

    if (CurrentState == EEnemyAIState::Rage)
    {
        TargetWalkSpeed = ResolveRageWalkSpeed();
    }
    else if (CurrentState == EEnemyAIState::Flee)
    {
        switch (FearResponseMode)
        {
        case EFearResponseMode::Retreat:
            TargetWalkSpeed = ResolveFearRetreatWalkSpeed();
            break;
        case EFearResponseMode::EscapeToHide:
            TargetWalkSpeed = ResolveFearHideRunWalkSpeed();
            break;
        case EFearResponseMode::Rage:
            TargetWalkSpeed = ResolveRageWalkSpeed();
            break;
        case EFearResponseMode::None:
        default:
            break;
        }
    }

    SetControlledWalkSpeed(TargetWalkSpeed);
}

float AMyAIController::GetCurrentUnmodifiedWalkSpeed() const
{
    if (bHasTrackedUnmodifiedWalkSpeed)
    {
        return TrackedUnmodifiedWalkSpeed;
    }

    if (const ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn()))
    {
        if (const UCharacterMovementComponent* MovementComponent = ControlledCharacter->GetCharacterMovement())
        {
            const float SlowMultiplier = GetActiveSlowMultiplier();
            if (SlowMultiplier > KINDA_SMALL_NUMBER)
            {
                return (MovementComponent->MaxWalkSpeed / SlowMultiplier) + GetActiveSpeedReductionAmount();
            }

            return MovementComponent->MaxWalkSpeed + GetActiveSpeedReductionAmount();
        }
    }

    return 0.f;
}

void AMyAIController::CleanupInvalidSlowSources()
{
    for (auto It = ActiveSlowSources.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
        }
    }
}

void AMyAIController::CleanupInvalidSpeedReductionSources()
{
    for (auto It = ActiveSpeedReductionSources.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
        }
    }
}

UMeshComponent* AMyAIController::ResolveControlledVisualMesh() const
{
    APawn* ControlledPawn = GetPawn();
    if (!IsValid(ControlledPawn))
    {
        return nullptr;
    }

    if (ACharacter* ControlledCharacter = Cast<ACharacter>(ControlledPawn))
    {
        if (IsValid(ControlledCharacter->GetMesh()))
        {
            return ControlledCharacter->GetMesh();
        }
    }

    return ControlledPawn->FindComponentByClass<UMeshComponent>();
}

void AMyAIController::CacheOriginalGhostMaterials(UMeshComponent* VisualMesh)
{
    if (!IsValid(VisualMesh))
    {
        OriginalGhostMaterials.Reset();
        return;
    }

    const int32 MaterialSlotCount = VisualMesh->GetNumMaterials();
    if (MaterialSlotCount <= 0)
    {
        OriginalGhostMaterials.Reset();
        return;
    }

    if (OriginalGhostMaterials.Num() == MaterialSlotCount)
    {
        bool bHasAnyMissingMaterial = false;
        for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
        {
            if (!OriginalGhostMaterials[MaterialIndex])
            {
                bHasAnyMissingMaterial = true;
                break;
            }
        }

        if (!bHasAnyMissingMaterial)
        {
            return;
        }
    }

    OriginalGhostMaterials.SetNum(MaterialSlotCount);
    for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
    {
        OriginalGhostMaterials[MaterialIndex] = VisualMesh->GetMaterial(MaterialIndex);
    }
}

void AMyAIController::ApplyGhostCollisionState()
{
    APawn* ControlledPawn = GetPawn();
    if (!IsValid(ControlledPawn))
    {
        bHasCachedGhostCapsuleCollisionResponses = false;
        bHasCachedGhostMeshCollisionResponses = false;
        return;
    }

    UPrimitiveComponent* PrimaryCollisionComponent = nullptr;
    if (ACharacter* ControlledCharacter = Cast<ACharacter>(ControlledPawn))
    {
        PrimaryCollisionComponent = ControlledCharacter->GetCapsuleComponent();
    }

    if (!IsValid(PrimaryCollisionComponent))
    {
        PrimaryCollisionComponent = Cast<UPrimitiveComponent>(ControlledPawn->GetRootComponent());
    }

    UPrimitiveComponent* VisualCollisionComponent = Cast<UPrimitiveComponent>(ResolveControlledVisualMesh());
    const bool bShouldUseSolidCollision = IsGhostRevealedByEffect();

    auto ApplyCollisionMode = [bShouldUseSolidCollision](
        UPrimitiveComponent* CollisionComponent,
        bool& bHasCachedResponses,
        ECollisionResponse& CachedWorldDynamicResponse,
        ECollisionResponse& CachedPhysicsBodyResponse)
    {
        if (!IsValid(CollisionComponent))
        {
            bHasCachedResponses = false;
            return;
        }

        if (!bHasCachedResponses)
        {
            CachedWorldDynamicResponse = CollisionComponent->GetCollisionResponseToChannel(ECC_WorldDynamic);
            CachedPhysicsBodyResponse = CollisionComponent->GetCollisionResponseToChannel(ECC_PhysicsBody);
            bHasCachedResponses = true;
        }

        if (bShouldUseSolidCollision)
        {
            CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, CachedWorldDynamicResponse);
            // CollisionComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, CachedPhysicsBodyResponse);
            // 显形时需要真正和可拾取物/物理体发生阻挡，
            // 否则如果角色原始碰撞配置对 PhysicsBody 只是 Overlap，
            // 就算十字架一侧允许 Pawn/Physics 碰撞，鬼也仍然会直接穿过去，
            // 进而导致十字架的 Hit 回调和补冲量逻辑根本不触发。
            CollisionComponent->SetCollisionResponseToChannel(
                ECC_PhysicsBody,
                CachedPhysicsBodyResponse == ECR_Ignore ? ECR_Block : ECR_Block);
            return;
        }

        // 隐身时保留地面/墙体阻挡，只把动态物体和可拾取物降成 Overlap，
        // 这样鬼还能正常寻路和站在地面上，但不会被这些小物件卡住。
        CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
        CollisionComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
    };

    ApplyCollisionMode(
        PrimaryCollisionComponent,
        bHasCachedGhostCapsuleCollisionResponses,
        CachedGhostCapsuleWorldDynamicResponse,
        CachedGhostCapsulePhysicsBodyResponse);

    if (VisualCollisionComponent != PrimaryCollisionComponent)
    {
        ApplyCollisionMode(
            VisualCollisionComponent,
            bHasCachedGhostMeshCollisionResponses,
            CachedGhostMeshWorldDynamicResponse,
            CachedGhostMeshPhysicsBodyResponse);
    }
}

void AMyAIController::ApplyGhostVisualState()
{
    UMeshComponent* VisualMesh = ResolveControlledVisualMesh();
    if (!IsValid(VisualMesh))
    {
        GhostVisualMaterials.Reset();
        bGhostVisualStateApplied = false;
        ApplyGhostCollisionState();
        return;
    }

    CacheOriginalGhostMaterials(VisualMesh);

    const bool bShouldRevealGhost = IsGhostRevealedByEffect();
    const float TargetOpacity = FMath::Clamp(
        bShouldRevealGhost ? RevealedGhostOpacity : HiddenGhostOpacity,
        0.f,
        1.f);
    const FLinearColor BaseTint(GhostTintColor.R, GhostTintColor.G, GhostTintColor.B, 1.f);
    const FLinearColor TintWithOpacity(GhostTintColor.R, GhostTintColor.G, GhostTintColor.B, TargetOpacity);

    const int32 MaterialSlotCount = VisualMesh->GetNumMaterials();
    GhostVisualMaterials.SetNum(MaterialSlotCount);

    if (bShouldRevealGhost)
    {
        for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
        {
            if (OriginalGhostMaterials.IsValidIndex(MaterialIndex) && OriginalGhostMaterials[MaterialIndex])
            {
                VisualMesh->SetMaterial(MaterialIndex, OriginalGhostMaterials[MaterialIndex]);
            }
        }

        bGhostVisualStateApplied = true;
        ApplyGhostCollisionState();
        return;
    }

    for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
    {
        UMaterialInstanceDynamic* DynamicMaterial = GhostVisualMaterials[MaterialIndex].Get();
        UMaterialInterface* HiddenOverrideMaterial = HiddenGhostMaterialOverrides.IsValidIndex(MaterialIndex)
            ? HiddenGhostMaterialOverrides[MaterialIndex].Get()
            : nullptr;

        if (IsValid(HiddenOverrideMaterial))
        {
            if (!IsValid(DynamicMaterial))
            {
                DynamicMaterial = UMaterialInstanceDynamic::Create(HiddenOverrideMaterial, this);
            }

            if (IsValid(DynamicMaterial))
            {
                VisualMesh->SetMaterial(MaterialIndex, DynamicMaterial);
            }
        }

        if (!IsValid(DynamicMaterial))
        {
            UMaterialInterface* BaseMaterial = OriginalGhostMaterials.IsValidIndex(MaterialIndex)
                ? OriginalGhostMaterials[MaterialIndex].Get()
                : VisualMesh->GetMaterial(MaterialIndex);

            if (!BaseMaterial)
            {
                continue;
            }

            DynamicMaterial = VisualMesh->CreateDynamicMaterialInstance(MaterialIndex, BaseMaterial);
        }

        if (!IsValid(DynamicMaterial))
        {
            continue;
        }

        GhostVisualMaterials[MaterialIndex] = DynamicMaterial;
        DynamicMaterial->SetVectorParameterValue(TEXT("TintColorAndOpacity"), TintWithOpacity);
        DynamicMaterial->SetVectorParameterValue(TEXT("TintColor"), TintWithOpacity);
        DynamicMaterial->SetVectorParameterValue(TEXT("ColorAndOpacity"), TintWithOpacity);
        DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), BaseTint);
        DynamicMaterial->SetVectorParameterValue(TEXT("GhostTint"), TintWithOpacity);
        DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), TargetOpacity);
        DynamicMaterial->SetScalarParameterValue(TEXT("Alpha"), TargetOpacity);
        DynamicMaterial->SetScalarParameterValue(TEXT("Transparency"), 1.f - TargetOpacity);
        DynamicMaterial->SetScalarParameterValue(TEXT("OpacityFromTexture"), TargetOpacity);
        DynamicMaterial->SetScalarParameterValue(TEXT("GhostOpacity"), TargetOpacity);
        DynamicMaterial->SetScalarParameterValue(TEXT("RevealOpacity"), TargetOpacity);
        DynamicMaterial->SetScalarParameterValue(TEXT("GhostVisible"), bShouldRevealGhost ? 1.f : 0.f);
    }

    bGhostVisualStateApplied = true;
    ApplyGhostCollisionState();
}

void AMyAIController::UpdateGhostVisualState(float DeltaTime)
{
    if (!bGhostVisualStateApplied)
    {
        ApplyGhostVisualState();
    }

    const bool bWasRevealedByEffect = IsGhostRevealedByEffect();

    if (bFlashlightRevealActive)
    {
        FlashlightRevealTimeRemaining = FMath::Max(0.f, FlashlightRevealTimeRemaining - DeltaTime);
        if (FlashlightRevealTimeRemaining <= 0.f)
        {
            bFlashlightRevealActive = false;
        }
    }

    if (bWeaknessActive)
    {
        WeaknessTimeRemaining = FMath::Max(0.f, WeaknessTimeRemaining - DeltaTime);
        if (WeaknessTimeRemaining <= 0.f)
        {
            bWeaknessActive = false;
            bWeaknessRevealPhaseVisible = false;
            WeaknessRevealToggleTimeAccumulator = 0.f;
        }
        else
        {
            const float ToggleInterval = 1.f / FMath::Max(WeaknessRevealToggleFrequency, 0.1f);
            WeaknessRevealToggleTimeAccumulator += DeltaTime;

            while (WeaknessRevealToggleTimeAccumulator >= ToggleInterval)
            {
                WeaknessRevealToggleTimeAccumulator -= ToggleInterval;
                bWeaknessRevealPhaseVisible = !bWeaknessRevealPhaseVisible;
            }
        }
    }

    if (bWasRevealedByEffect != IsGhostRevealedByEffect())
    {
        ApplyGhostVisualState();
    }
}

bool AMyAIController::IsGhostRevealedByEffect() const
{
    return bFlashlightRevealActive || (bWeaknessActive && bWeaknessRevealPhaseVisible);
}

// 恐惧结束时的恢复入口。
// 当 ClearFear 把内部 Fear 状态清掉时，会进入这里做收尾。
// 恢复顺序是：先看有没有玩家可追，再看有没有调查目标，最后再回巡逻。
void AMyAIController::HandleFearEnded()
{
    if (CurrentState != EEnemyAIState::Flee)
    {
        return;
    }

    // 虽然内部主 Fear 计时已经结束，但如果当前还确实有一个没跑完的 hide 点目标，
    // 就继续把它当成 Flee 的收尾动作完成，避免角色在躲藏途中突然切回普通状态。
    if (FearResponseMode == EFearResponseMode::EscapeToHide && HasPendingFearHideTarget())
    {
        SetAIState(EEnemyAIState::Flee);

        // 只有在没移动、也不在 detour 时才补发 hide move，
        // 避免把正在执行中的收尾路径重置掉。
        if (!bIsMoving && !bFearDetourActive)
        {
            MoveToFearHidePoint(false);
        }

        return;
    }

    RestoreWalkSpeedAfterFear();
    ClearPatrolRetry();
    CurrentFearHidePoint = nullptr;
    FearResponseMode = EFearResponseMode::None;
    bFearHoldingAtHidePoint = false;
    ClearFearDetour();
    FearHideEscapeTimeRemaining = 0.f;
    FearRetreatTimeRemaining = 0.f;
    bFearRetreatInputActive = false;
    bPendingFearRepath = false;
    FearAngerDecayTimeRemaining = FMath::Max(0.1f, FearAngerDecayInterval);

    // 这个标记代表“躲到点后强制回巡逻”，
    // 它会压过后面的 chase / investigate 恢复顺序。
    // 用途是某些设计希望鬼躲好之后直接冷静下来，不要立刻重新追人。
    if (bForcePatrolAfterFearEnds)
    {
        bForcePatrolAfterFearEnds = false;
        bCanSeePlayer = false;
        TargetPlayer = nullptr;
        ClearInvestigationState();
        Patrol();
        return;
    }

    // Flee 结束后并不是一律回巡逻，而是按“仍看到玩家 > 仍有调查目标 > 巡逻”恢复。
    if (bCanSeePlayer && IsValid(TargetPlayer))
    {
        ChasePlayer();
        return;
    }

    if (AcquireVisiblePlayerTarget())
    {
        ChasePlayer();
        return;
    }

    if (bHasInvestigateTarget)
    {
        MoveToInvestigateTarget();
        return;
    }

    Patrol();
}

// Fear 的总调度入口。
// 它不直接决定“往哪里跑”，而是根据当前 FearResponseMode 把逻辑分发到后退、逃向躲藏点或 Rage。
void AMyAIController::UpdateFearState(float DeltaTime, bool bShouldRepath)
{
    SetAIState(EEnemyAIState::Flee);

    switch (FearResponseMode)
    {
    case EFearResponseMode::None:
        EnterFearRetreatState(true);
        return;
    case EFearResponseMode::Retreat:
        UpdateFearRetreat(DeltaTime, bShouldRepath);
        return;
    case EFearResponseMode::EscapeToHide:
        UpdateFearHideEscape(DeltaTime, bShouldRepath);
        return;
    case EFearResponseMode::Rage:
        return;
    default:
        break;
    }
}

// Fear 第一阶段：被吓到后先缩一下，慢速后退，给鬼一个“本能反应”。
void AMyAIController::UpdateFearRetreat(float DeltaTime, bool bShouldRepath)
{
    FearRetreatTimeRemaining = FMath::Max(0.f, FearRetreatTimeRemaining - DeltaTime);
    FearRetreatLogTimeRemaining = FMath::Max(0.f, FearRetreatLogTimeRemaining - DeltaTime);

    FVector RetreatDirection = FVector::ZeroVector;
    if (GetPawn())
    {
        const FVector SourceLocation = GetFearSourceLocation();
        RetreatDirection = GetPawn()->GetActorLocation() - SourceLocation;
        RetreatDirection.Z = 0.f;

        if (RetreatDirection.IsNearlyZero())
        {
            RetreatDirection = -GetPawn()->GetActorForwardVector();
            RetreatDirection.Z = 0.f;
        }

        RetreatDirection.Normalize();
        CurrentFleeTarget = GetPawn()->GetActorLocation() + RetreatDirection * FearRetreatDistance;
    }

    SetControlledWalkSpeed(ResolveFearRetreatWalkSpeed());
    bFearRetreatInputActive = false;

    if (GetPawn() && !RetreatDirection.IsNearlyZero())
    {
        const FRotator RetreatRotation = RetreatDirection.Rotation();
        SetControlRotation(RetreatRotation);
        GetPawn()->FaceRotation(RetreatRotation, DeltaTime);

        // 后退阶段直接给 Pawn 输入，而不是走导航，
        // 这样即使 ItemX 连续照射，也会先有明显的“缩退”动作。
        GetPawn()->AddMovementInput(RetreatDirection, 1.f);
        bFearRetreatInputActive = true;

        if (bLogFearRetreat && FearRetreatLogTimeRemaining <= 0.f)
        {
            UE_LOG(LogTemp, Log, TEXT("%s FEAR RETREAT INPUT Dir=%s Remaining=%.2f Anger=%d/%d Source=%s"),
                *GetNameSafe(GetPawn()),
                *RetreatDirection.ToCompactString(),
                FearRetreatTimeRemaining,
                FearAngerLevel,
                FearAngerThreshold,
                *GetNameSafe(ActiveFearStimulus.SourceActor.Get()));
            FearRetreatLogTimeRemaining = 0.35f;
        }
    }

    bIsMoving = true;

    if (FearRetreatTimeRemaining <= 0.f)
    {
        // 每次“缓慢后退”结束后，都重新随机选一个躲藏点，
        // 避免鬼一直执着于上一次的 GhostHidePoint。
        EnterFearHideEscapeState(true);
    }
}

// Fear 第二阶段：朝用户预设的躲藏点跑。
// 内部 Fear 的定时 repath 在这里主要用于刷新“逃向躲藏点”的路径；
// 真正遇到新的恐惧刺激时，会由 HandleFearApplied 把它重新打回后退阶段。
void AMyAIController::UpdateFearHideEscape(float DeltaTime, bool bShouldRepath)
{
    FVector ActiveHideDestination = CurrentFleeTarget;
    const bool bHasResolvedHideDestination = IsValid(CurrentFearHidePoint)
        && ResolveFearHideDestination(CurrentFearHidePoint, ActiveHideDestination);
    const FVector CurrentEscapeMoveTarget = bFearDetourActive && !FearDetourTarget.IsNearlyZero()
        ? FearDetourTarget
        : (bHasResolvedHideDestination ? ActiveHideDestination : CurrentFleeTarget);

    FearHideEscapeTimeRemaining = FMath::Max(0.f, FearHideEscapeTimeRemaining - DeltaTime);
    if (FearHideEscapeTimeRemaining <= 0.f)
    {
        // 只要当前已经选中了有效的 GhostHidePoint，就继续逃到点位，
        // 不能因为时间到了就提前回巡逻。
        if (!bHasResolvedHideDestination)
        {
            if (IsFearActive())
            {
                ClearFear();
            }
            else
            {
                HandleFearEnded();
            }

            return;
        }

        FearHideEscapeTimeRemaining = 0.f;
    }

    if (bHasResolvedHideDestination && GetPawn())
    {
        const float DistanceToHidePoint = FVector::Dist2D(GetPawn()->GetActorLocation(), ActiveHideDestination);
        if (DistanceToHidePoint <= FearHideAcceptanceRadius)
        {
            bFearHoldingAtHidePoint = true;
            bIsMoving = false;
        }
    }

    // PathFollowing 有时已经悄悄 Idle 了，但 Controller 这边的 bIsMoving 还残留着 true。
    // 这个判断专门清理这种“移动状态假阳性”，否则下面不会重新发起 hide move，
    // 鬼就会卡在原地一直以为自己还在跑。
    if (bIsMoving && !bFearHoldingAtHidePoint && !bFearDetourActive && !IsPathFollowingMoveActive(this))
    {
        bIsMoving = false;
        ActiveMoveRequestId = FAIRequestID::InvalidRequest;
    }

    // 如果当前目标方向会明显把鬼重新送回恐惧源附近，就不能继续前冲，
    // 否则会出现“刚后退完又朝光束里冲”的违和行为。
    if (ShouldInterruptHideEscapeForFearSource(CurrentEscapeMoveTarget))
    {
        if (bLogFearRetreat)
        {
            UE_LOG(LogTemp, Warning, TEXT("%s FEAR ESCAPE INTERRUPTED Target=%s Source=%s Detour=%s"),
                *GetNameSafe(GetPawn()),
                *CurrentEscapeMoveTarget.ToCompactString(),
                *GetNameSafe(ActiveFearStimulus.SourceActor.Get()),
                bFearDetourActive ? TEXT("true") : TEXT("false"));
        }

        EnterFearRetreatState(true);
        return;
    }

    if (bFearDetourActive)
    {
        FearDetourTimeRemaining = FMath::Max(0.f, FearDetourTimeRemaining - DeltaTime);
        if (FearDetourTimeRemaining <= 0.f || !bIsMoving)
        {
            ClearFearDetour();

            if (!MoveToFearHidePoint(false))
            {
                EnterFearRetreatState(true);
            }
        }

        return;
    }
    if (bShouldRepath)
    {
        bFearHoldingAtHidePoint = false;
        if (!MoveToFearHidePoint(false))
        {
            EnterFearRetreatState(true);
        }
        return;
    }

    if (bFearHoldingAtHidePoint && IsValid(CurrentFearHidePoint))
    {
        const FVector FinalHideDestination = bHasResolvedHideDestination
            ? ActiveHideDestination
            : CurrentFearHidePoint->GetActorLocation();
        const float DistanceToHidePoint = FVector::Dist2D(GetPawn()->GetActorLocation(),
            FinalHideDestination);
        if (DistanceToHidePoint <= FearHideAcceptanceRadius)
        {
            if (bReturnToPatrolAfterHideReached)
            {
                bForcePatrolAfterFearEnds = true;

                if (IsFearActive())
                {
                    ClearFear();
                }
                else
                {
                    HandleFearEnded();
                }

                return;
            }

            // 已经躲到点里了，就保持 Fear 高优先级继续躲着，不追人。
            return;
        }

        bFearHoldingAtHidePoint = false;
    }

    if (!bIsMoving)
    {
        if (!MoveToFearHidePoint(false))
        {
            EnterFearRetreatState(true);
        }
    }
}

// 进入 Fear 后退阶段。
// bChooseNewHidePoint 为 true 时，会丢弃当前已选躲藏点，意味着这次要重新想办法绕开恐惧源。
void AMyAIController::EnterFearRetreatState(bool bChooseNewHidePoint)
{
    CacheWalkSpeedIfNeeded();

    ClearFearDetour();

    if (bChooseNewHidePoint)
    {
        CurrentFearHidePoint = nullptr;
    }

    FearResponseMode = EFearResponseMode::Retreat;
    bFearHoldingAtHidePoint = false;
    FearRetreatTimeRemaining = FearRetreatDuration;
    FearRetreatLogTimeRemaining = 0.f;
    bFearRetreatInputActive = false;
    SetAIState(EEnemyAIState::Flee);
    SetControlledWalkSpeed(ResolveFearRetreatWalkSpeed());

    StopMovement();
    ActiveMoveRequestId = FAIRequestID::InvalidRequest;
    CurrentFleeTarget = GetFearRetreatLocation();
    bIsMoving = true;

    if (bLogFearRetreat)
    {
        UE_LOG(LogTemp, Log, TEXT("%s ENTER FEAR RETREAT Target=%s HidePointReset=%s"),
            *GetNameSafe(GetPawn()),
            *CurrentFleeTarget.ToCompactString(),
            bChooseNewHidePoint ? TEXT("true") : TEXT("false"));
    }
}

// 进入 Fear 躲藏点逃跑阶段。
void AMyAIController::EnterFearHideEscapeState(bool bChooseNewHidePoint)
{
    FearResponseMode = EFearResponseMode::EscapeToHide;
    FearHideEscapeTimeRemaining = FearHideEscapeDuration;
    bFearHoldingAtHidePoint = false;
    bFearRetreatInputActive = false;
    ClearFearDetour();
    SetAIState(EEnemyAIState::Flee);
    SetControlledWalkSpeed(ResolveFearHideRunWalkSpeed());

    if (bLogFearRetreat)
    {
        UE_LOG(LogTemp, Log, TEXT("%s ENTER FEAR HIDE ESCAPE Duration=%.2f ChooseNew=%s"),
            *GetNameSafe(GetPawn()),
            FearHideEscapeTimeRemaining,
            bChooseNewHidePoint ? TEXT("true") : TEXT("false"));
    }

    if (!MoveToFearHidePoint(bChooseNewHidePoint))
    {
        EnterFearRetreatState(true);
    }
}

// 从场景里带有指定 Tag 的空 Actor / TargetPoint 中随机选一个。
// 对新手来说，最简单的配置方式就是在关卡里放几个空 Actor，然后加上 GhostHidePoint 这个 Tag。
AActor* AMyAIController::ChooseFearHidePoint() const
{
    if (!GetWorld() || FearHidePointTag.IsNone() || !GetPawn())
    {
        return nullptr;
    }

    TArray<AActor*> TaggedActors;
    GatherFearHidePointCandidates(TaggedActors);

    if (TaggedActors.IsEmpty())
    {
        return nullptr;
    }

    TArray<AActor*> ReachableActors;
    const FVector PawnLocation = GetPawn()->GetActorLocation();

    for (AActor* Candidate : TaggedActors)
    {
        if (!IsValid(Candidate))
        {
            continue;
        }

        // 如果场上有多个点，优先不要连续两次选中同一个 hide point，
        // 这样 Fear 多次触发时路线会更自然，也能减少“原地往返同一点”的机械感。
        if (CurrentFearHidePoint && TaggedActors.Num() > 1 && Candidate == CurrentFearHidePoint)
        {
            continue;
        }

        FVector ProjectedDestination = FVector::ZeroVector;
        if (!ResolveFearHideDestination(Candidate, ProjectedDestination))
        {
            continue;
        }

        if (UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
            GetWorld(), PawnLocation, ProjectedDestination, GetPawn()))
        {
            // 只收“完整可达”的点。
            // Partial path 说明导航最多只能到一半，真跑起来大概率会把 Fear 卡成半路停住。
            if (Path->IsValid() && !Path->IsPartial())
            {
                ReachableActors.Add(Candidate);
            }
        }
    }

    if (ReachableActors.IsEmpty())
    {
        return nullptr;
    }

    return ReachableActors[FMath::RandRange(0, ReachableActors.Num() - 1)];
}

// 统一收集“当前关卡里可作为躲藏点候选”的 Actor。
// 这里既支持 Actor 自己带 Tag，也支持蓝图里组件带同名 Tag。
void AMyAIController::GatherFearHidePointCandidates(TArray<AActor*>& OutCandidates) const
{
    OutCandidates.Reset();

    if (!GetWorld() || FearHidePointTag.IsNone())
    {
        return;
    }

    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }

        if (HasFearHidePointTag(Candidate))
        {
            OutCandidates.Add(Candidate);
        }
    }
}

// 蓝图里常见的两种配法都认：
// 1. 在 Actor 的 Tags 里直接加 GhostHidePoint
// 2. 在 BP 某个组件的 Component Tags 里加 GhostHidePoint
bool AMyAIController::HasFearHidePointTag(const AActor* Candidate) const
{
    if (!IsValid(Candidate) || FearHidePointTag.IsNone())
    {
        return false;
    }

    if (Candidate->ActorHasTag(FearHidePointTag))
    {
        return true;
    }

    TArray<UActorComponent*> Components;
    Candidate->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        if (IsValid(Component) && Component->ComponentHasTag(FearHidePointTag))
        {
            return true;
        }
    }

    return false;
}

// 把关卡里放置的躲藏点 Actor 位置投到 NavMesh 上，
// 避免空 Actor / TargetPoint 稍微离地时，AI 把它误判为不可达。
bool AMyAIController::ResolveFearHideDestination(AActor* HidePoint, FVector& OutDestination) const
{
    OutDestination = FVector::ZeroVector;

    if (!IsValid(HidePoint) || !GetWorld())
    {
        return false;
    }

    const FVector DesiredLocation = HidePoint->GetActorLocation();

    if (UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        FNavLocation NavLocation;
        if (NavSystem->ProjectPointToNavigation(DesiredLocation, NavLocation, FVector(300.f, 300.f, 300.f)))
        {
            OutDestination = NavLocation.Location;
            return true;
        }
    }

    return false;
}

// 向当前躲藏点发起移动。
// 如果没有配置躲藏点，或者所有点当前都走不过去，会回退到旧的“朝反方向跑”兜底逻辑。
bool AMyAIController::MoveToFearHidePoint(bool bChooseNewHidePoint)
{
    if (!GetPawn())
    {
        return false;
    }

    if (bChooseNewHidePoint || !IsValid(CurrentFearHidePoint))
    {
        CurrentFearHidePoint = ChooseFearHidePoint();

        if (bLogFearRetreat)
        {
            UE_LOG(LogTemp, Log, TEXT("%s FEAR HIDE PICK Selected=%s"),
                *GetNameSafe(GetPawn()),
                *GetNameSafe(CurrentFearHidePoint));
        }
    }

    SetControlledWalkSpeed(ResolveFearHideRunWalkSpeed());

    FVector HideMoveDestination = FVector::ZeroVector;
    if (IsValid(CurrentFearHidePoint) && ResolveFearHideDestination(CurrentFearHidePoint, HideMoveDestination))
    {
        if (FVector::Dist2D(GetPawn()->GetActorLocation(), HideMoveDestination) <= FearHideAcceptanceRadius)
        {
            CurrentFleeTarget = HideMoveDestination;
            bFearHoldingAtHidePoint = true;
            bIsMoving = false;

            if (bLogFearRetreat)
            {
                UE_LOG(LogTemp, Log, TEXT("%s FEAR HIDE ALREADY_AT_POINT Target=%s Point=%s Remaining=%.2f"),
                    *GetNameSafe(GetPawn()),
                    *CurrentFleeTarget.ToCompactString(),
                    *GetNameSafe(CurrentFearHidePoint),
                    FearHideEscapeTimeRemaining);
            }

            return true;
        }

        // 绕路状态下，如果已经有一条同目标的有效导航请求在跑，
        // 就不要每帧 StopMovement + RequestMove 一次。
        // 这个 if 的作用是避免 Fear hide 在 Tick 中不断重提同一条路径，
        // 造成回调噪声和移动抖动。
        const bool bAlreadyMovingToSameHidePoint = bIsMoving
            && ActiveMoveRequestId.IsValid()
            && IsPathFollowingMoveActive(this)
            && FVector::Dist2D(CurrentFleeTarget, HideMoveDestination)
                <= FMath::Max(10.f, FearHideAcceptanceRadius * 0.5f);

        if (bAlreadyMovingToSameHidePoint)
        {
            if (bLogFearRetreat)
            {
                UE_LOG(LogTemp, Verbose, TEXT("%s FEAR HIDE KEEP_MOVING Target=%s Point=%s Remaining=%.2f"),
                    *GetNameSafe(GetPawn()),
                    *HideMoveDestination.ToCompactString(),
                    *GetNameSafe(CurrentFearHidePoint),
                    FearHideEscapeTimeRemaining);
            }

            return true;
        }

        StopMovement();
        CurrentFleeTarget = HideMoveDestination;
        bIsMoving = RequestMoveToLocation(CurrentFleeTarget, FearHideAcceptanceRadius);

        if (bLogFearRetreat)
        {
            UE_LOG(LogTemp, Log, TEXT("%s FEAR HIDE MOVE Target=%s Point=%s Submitted=%s Remaining=%.2f"),
                *GetNameSafe(GetPawn()),
                *CurrentFleeTarget.ToCompactString(),
                *GetNameSafe(CurrentFearHidePoint),
                bIsMoving ? TEXT("true") : TEXT("false"),
                FearHideEscapeTimeRemaining);
        }

        return bIsMoving;
    }

    if (IsValid(CurrentFearHidePoint) && bLogFearRetreat)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s FEAR HIDE INVALID_NAV Point=%s Location=%s"),
            *GetNameSafe(GetPawn()),
            *GetNameSafe(CurrentFearHidePoint),
            *CurrentFearHidePoint->GetActorLocation().ToCompactString());
    }

    StopMovement();
    CurrentFleeTarget = GetFleeLocationFromSource(GetFearSourceLocation());
    bIsMoving = RequestMoveToLocation(CurrentFleeTarget, FleeAcceptanceRadius);

    if (bLogFearRetreat)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s FEAR HIDE FALLBACK Target=%s Submitted=%s"),
            *GetNameSafe(GetPawn()),
            *CurrentFleeTarget.ToCompactString(),
            bIsMoving ? TEXT("true") : TEXT("false"));
    }

    return bIsMoving;
}

// 逃向躲藏点时，如果当前目标方向会把鬼重新送向恐惧源，
// 就应该立刻打断前冲，重新执行“缓慢后退”。
bool AMyAIController::ShouldInterruptHideEscapeForFearSource(const FVector& HideDestination) const
{
    if (!GetPawn() || !IsFearActive())
    {
        return false;
    }

    const FVector PawnLocation = GetPawn()->GetActorLocation();
    FVector ToHide = HideDestination - PawnLocation;
    FVector ToSource = GetFearSourceLocation() - PawnLocation;
    ToHide.Z = 0.f;
    ToSource.Z = 0.f;

    const float DistanceToSource = ToSource.Size();
    if (DistanceToSource <= KINDA_SMALL_NUMBER)
    {
        // 恐惧源几乎贴在脸上时，没有任何“继续冲向躲藏点”的安全余地，
        // 直接强制打断，让上层回到 Retreat。
        return true;
    }

    if (DistanceToSource > FearEscapeBlockSourceDistance)
    {
        // 恐惧源已经离得足够远，就没必要为了它打断当前逃跑。
        // 这个阈值是为了避免 AI 在安全距离外还对旧刺激过度敏感。
        return false;
    }

    if (ToHide.IsNearlyZero())
    {
        // hide 目标无效或离自己太近时，继续“前冲”没有意义；
        // 上层应重新选择路线，而不是沿着零向量硬算。
        return true;
    }

    const FVector ClosestPointOnPath = FMath::ClosestPointOnSegment(ToSource, FVector::ZeroVector, ToHide);
    const float PathBlockDistance = FVector::Dist2D(ClosestPointOnPath, ToSource);

    ToHide.Normalize();
    ToSource.Normalize();

    // 两个条件任意一个成立都认为“前方被恐惧源挡住了”：
    // 1. Dot 很高：说明 hide 方向和恐惧源方向太接近，本质上是在往源头冲。
    // 2. PathBlockDistance 很小：说明从当前位置到 hide 点的线段会擦过恐惧源附近。
    return FVector::DotProduct(ToHide, ToSource) >= FearEscapeBlockDirectionDot
        || PathBlockDistance <= FearEscapePathBlockRadius;
}

// 只要当前仍有有效的躲藏点，并且鬼还没真正进入到达半径内，
// 就继续把 EscapeToHide 当成进行中的高优先级状态。
bool AMyAIController::HasPendingFearHideTarget() const
{
    if (!GetPawn() || !IsValid(CurrentFearHidePoint))
    {
        return false;
    }

    FVector ResolvedHideDestination = FVector::ZeroVector;
    if (!ResolveFearHideDestination(CurrentFearHidePoint, ResolvedHideDestination))
    {
        return false;
    }

    return FVector::Dist2D(GetPawn()->GetActorLocation(), ResolvedHideDestination) > FearHideAcceptanceRadius;
}

// 被恐惧源挡路时，优先尝试左右绕开，避免鬼在“前冲/后退”之间来回抖动。
bool AMyAIController::TryStartFearDetour(const FVector& HideDestination)
{
    if (!GetPawn() || !IsFearActive() || !GetWorld())
    {
        return false;
    }

    const FVector PawnLocation = GetPawn()->GetActorLocation();
    FVector AwayFromSource = PawnLocation - GetFearSourceLocation();
    AwayFromSource.Z = 0.f;

    if (AwayFromSource.IsNearlyZero())
    {
        AwayFromSource = -GetPawn()->GetActorForwardVector();
        AwayFromSource.Z = 0.f;
    }

    AwayFromSource.Normalize();

    FVector SideDirection = FVector::CrossProduct(FVector::UpVector, AwayFromSource);
    SideDirection.Z = 0.f;
    SideDirection.Normalize();

    FVector ToHide = HideDestination - PawnLocation;
    ToHide.Z = 0.f;
    if (!ToHide.IsNearlyZero())
    {
        ToHide.Normalize();
    }

    TArray<FVector> CandidateDestinations;
    CandidateDestinations.Add(PawnLocation + SideDirection * FearEscapeDetourDistance + AwayFromSource * FearEscapeDetourBackBias);
    CandidateDestinations.Add(PawnLocation - SideDirection * FearEscapeDetourDistance + AwayFromSource * FearEscapeDetourBackBias);
    CandidateDestinations.Add(PawnLocation + SideDirection * (FearEscapeDetourDistance * 0.6f) + AwayFromSource * (FearEscapeDetourDistance * 0.9f));
    CandidateDestinations.Add(PawnLocation - SideDirection * (FearEscapeDetourDistance * 0.6f) + AwayFromSource * (FearEscapeDetourDistance * 0.9f));

    FVector BestDestination = FVector::ZeroVector;
    float BestScore = -TNumericLimits<float>::Max();

    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSystem)
    {
        return false;
    }

    for (const FVector& Candidate : CandidateDestinations)
    {
        FNavLocation NavLocation;
        if (!NavSystem->ProjectPointToNavigation(Candidate, NavLocation, FVector(250.f, 250.f, 200.f)))
        {
            continue;
        }

        if (UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
            GetWorld(), PawnLocation, NavLocation.Location, GetPawn()))
        {
            if (!Path->IsValid() || Path->IsPartial())
            {
                continue;
            }
        }
        else
        {
            continue;
        }

        FVector CandidateDirection = NavLocation.Location - PawnLocation;
        CandidateDirection.Z = 0.f;
        if (CandidateDirection.IsNearlyZero())
        {
            continue;
        }

        CandidateDirection.Normalize();

        const float SourceDistanceScore = FVector::Dist2D(NavLocation.Location, GetFearSourceLocation());
        const float HideDistancePenalty = FVector::Dist2D(NavLocation.Location, HideDestination);
        const float HideAlignmentBonus = ToHide.IsNearlyZero() ? 0.f : FVector::DotProduct(CandidateDirection, ToHide) * 220.f;
        const float SafetyBonus = FVector::DotProduct(CandidateDirection, AwayFromSource) * 180.f;
        const float CandidateScore = SourceDistanceScore - HideDistancePenalty * 0.35f + HideAlignmentBonus + SafetyBonus;

        if (CandidateScore > BestScore)
        {
            BestScore = CandidateScore;
            BestDestination = NavLocation.Location;
        }
    }

    // BestScore 以“极小值哨兵”初始化。
    // 如果循环结束后它仍然没被刷新，说明所有候选绕路点都不可用；
    // BestDestination 仍是零向量时也视为没有合法 detour。
    if (BestScore <= -TNumericLimits<float>::Max() || BestDestination.IsNearlyZero())
    {
        return false;
    }

    StopMovement();
    FearDetourTarget = BestDestination;
    FearDetourTimeRemaining = FearEscapeDetourDuration;
    bFearDetourActive = true;
    bFearHoldingAtHidePoint = false;
    CurrentFleeTarget = FearDetourTarget;
    bIsMoving = RequestMoveToLocation(FearDetourTarget, FearHideAcceptanceRadius);

    if (bLogFearRetreat)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s FEAR HIDE DETOUR Target=%s Submitted=%s"),
            *GetNameSafe(GetPawn()),
            *FearDetourTarget.ToCompactString(),
            bIsMoving ? TEXT("true") : TEXT("false"));
    }

    if (!bIsMoving)
    {
        ClearFearDetour();
    }

    return bIsMoving;
}

// detour 只是逃点过程中的短暂绕行，不应该长期占用 Fear 状态。
void AMyAIController::ClearFearDetour()
{
    bFearDetourActive = false;
    FearDetourTimeRemaining = 0.f;
    FearDetourTarget = FVector::ZeroVector;
}

// 计算短距离后退点。
// 这个点和原来的“长距离逃跑点”不同，它故意更近，让鬼先有一个被吓退的瞬间动作。
FVector AMyAIController::GetFearRetreatLocation() const
{
    if (!GetPawn())
    {
        return FVector::ZeroVector;
    }

    const FVector SourceLocation = GetFearSourceLocation();
    const FVector PawnLocation = GetPawn()->GetActorLocation();
    FVector RetreatDirection = PawnLocation - SourceLocation;
    RetreatDirection.Z = 0.f;

    if (RetreatDirection.IsNearlyZero())
    {
        RetreatDirection = -GetPawn()->GetActorForwardVector();
        RetreatDirection.Z = 0.f;
    }

    RetreatDirection.Normalize();
    const FVector DesiredLocation = PawnLocation + RetreatDirection * FearRetreatDistance;

    if (UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        FNavLocation NavLocation;
        if (NavSystem->ProjectPointToNavigation(DesiredLocation, NavLocation, FVector(200.f, 200.f, 160.f)))
        {
            return NavLocation.Location;
        }
    }

    return DesiredLocation;
}

// Rage 改成“累计受惊量”后，这里只推进命中冷却计时。
void AMyAIController::UpdateFearAnger(float DeltaTime)
{
    FearAngerHitCooldownRemaining = FMath::Max(0.f, FearAngerHitCooldownRemaining - DeltaTime);
}

void AMyAIController::RegisterFearAngerStimulus(const FFearStimulus& Stimulus)
{
    // 三种情况下不累计怒气：
    // 1. 阈值被配置成 0 或更小，等于关闭 Rage 系统。
    // 2. 已经在 Rage 中。
    // 3. 已经满足阈值，正等待下一帧正式进入 Rage。
    if (FearAngerThreshold <= 0 || CurrentState == EEnemyAIState::Rage || bPendingRageEnter)
    {
        return;
    }

    if (CurrentState == EEnemyAIState::Investigate
        && CurrentInvestigateStimulusTag == RageDisabledItemStimulusTag)
    {
        return;
    }

    const bool bMatchesSuppressedSourceActor = IsValid(SuppressedFearAngerSourceActor)
        && SuppressedFearAngerSourceActor == Stimulus.SourceActor;
    const bool bMatchesSuppressedSourceLocation = FVector::DistSquared(
        SuppressedFearAngerSourceLocation,
        Stimulus.SourceLocation) <= FMath::Square(60.f);
    if (SuppressedFearAngerTimeRemaining > 0.f
        && (bMatchesSuppressedSourceActor || bMatchesSuppressedSourceLocation))
    {
        return;
    }

    const bool bSameSourceActor = LastFearAngerSourceActor == Stimulus.SourceActor;
    const bool bSameTag = LastFearAngerStimulusTag == Stimulus.StimulusTag;
    const bool bSameLocation = FVector::DistSquared(LastFearAngerSourceLocation, Stimulus.SourceLocation)
        <= FMath::Square(30.f);
    const bool bIsSameRecentStimulus = bSameSourceActor && bSameTag && bSameLocation;

    // 同一个来源、同一种 Tag、几乎同一个位置的连续脉冲，
    // 在冷却时间内只算一次，防止高频 Tick/Overlap 把怒气瞬间灌满。
    if (bIsSameRecentStimulus && FearAngerHitCooldownRemaining > 0.f)
    {
        return;
    }

    LastFearAngerSourceActor = Stimulus.SourceActor;
    LastFearAngerStimulusTag = Stimulus.StimulusTag;
    LastFearAngerSourceLocation = Stimulus.SourceLocation;
    FearAngerHitCooldownRemaining = FMath::Max(0.1f, FearAngerHitCooldown);
    const int32 AngerAmountToAdd = Stimulus.RageAngerAmount > 0
        ? Stimulus.RageAngerAmount
        : FMath::Max(1, FearAngerPerStimulus);
    FearAngerLevel += AngerAmountToAdd;

    if (FearAngerLevel >= FearAngerThreshold)
    {
        FearAngerLevel = FearAngerThreshold;
        bPendingRageEnter = true;
    }
}

// Fear/Rage 结束后让怒气值缓慢掉下来，保留“情绪余温”。
void AMyAIController::UpdateFearAngerDecay(float DeltaTime)
{
    if (FearAngerLevel <= 0)
    {
        FearAngerDecayTimeRemaining = FMath::Max(0.1f, FearAngerDecayInterval);
        return;
    }

    FearAngerDecayTimeRemaining -= DeltaTime;
    const float DecayInterval = FMath::Max(0.1f, FearAngerDecayInterval);

    while (FearAngerDecayTimeRemaining <= 0.f && FearAngerLevel > 0)
    {
        FearAngerDecayTimeRemaining += DecayInterval;
        FearAngerLevel = FMath::Max(0, FearAngerLevel - 1);
    }
}

// 清空 Fear 怒气累计。
void AMyAIController::ResetFearAnger()
{
    FearAngerLevel = 0;
    FearAngerHitCooldownRemaining = 0.f;
    FearAngerDecayTimeRemaining = FMath::Max(0.1f, FearAngerDecayInterval);
    bPendingRageEnter = false;
    LastFearAngerSourceActor = nullptr;
    LastFearAngerStimulusTag = NAME_None;
    LastFearAngerSourceLocation = FVector::ZeroVector;
}

void AMyAIController::CacheRageSourceFromFear()
{
    RageSourceActor = ActiveFearStimulus.SourceActor;
    RageSourceLocation = ActiveFearStimulus.SourceLocation;

    // Rage 源如果还是自己，就不能拿来做后面的“冲过去禁用物体”逻辑。
    // 这里清掉 Actor，只保留位置，表示 Rage 知道怒气从哪来，但不会把自己当目标。
    if (RageSourceActor == GetPawn() || RageSourceActor == this)
    {
        RageSourceActor = nullptr;
    }

    if (IsValid(RageSourceActor))
    {
        RageSourceLocation = RageSourceActor->GetActorLocation();
    }

    bHasRageSourceLocation = IsValid(RageSourceActor) || !RageSourceLocation.IsNearlyZero();
    bRageSourceDisabled = false;
}

bool AMyAIController::HasActiveRageDisableTarget() const
{
    for (const TWeakObjectPtr<AActor>& Entry : ActiveRageThreatSources)
    {
        const APickupActor* RagePickup = Cast<APickupActor>(Entry.Get());
        if (IsValid(RagePickup) && RagePickup->CanBeDisabledByRageNative())
        {
            return true;
        }
    }

    const APickupActor* RagePickup = Cast<APickupActor>(RageSourceActor);
    return !bRageSourceDisabled && IsValid(RagePickup) && RagePickup->CanBeDisabledByRageNative();
}

bool AMyAIController::TryDisableRageSource()
{
    APickupActor* RagePickup = Cast<APickupActor>(RageSourceActor);
    // 这里是一组前置条件总闸：
    // 没 Pawn、已经禁用过、来源不是有效 Pickup、或者该 Pickup 本身不支持 Rage 禁用，
    // 都不应该继续往下做贴脸破坏判定。
    if (!GetPawn() || bRageSourceDisabled || !IsValid(RagePickup) || !RagePickup->CanBeDisabledByRageNative())
    {
        return false;
    }

    RageSourceLocation = RagePickup->GetActorLocation();
    bHasRageSourceLocation = true;

    FVector PickupOrigin = RageSourceLocation;
    FVector PickupExtent = FVector::ZeroVector;
    RagePickup->GetActorBounds(true, PickupOrigin, PickupExtent);

    const FVector PawnLocation = GetPawn()->GetActorLocation();
    const FVector BoxMin = PickupOrigin - PickupExtent;
    const FVector BoxMax = PickupOrigin + PickupExtent;
    const FVector ClosestPointOnPickupBounds = PawnLocation.BoundToBox(BoxMin, BoxMax);
    const float DistanceToPickupBounds = FVector::Dist2D(PawnLocation, ClosestPointOnPickupBounds);
    const float EffectiveDisableAcceptanceRadius = ActiveRageMoveSourceActor.Get() == RageSourceActor.Get()
        ? FMath::Max(RageDisableItemAcceptanceRadius, ActiveRageDisableAcceptanceRadius)
        : RageDisableItemAcceptanceRadius;

    // 这里不是判到 Actor 原点的距离，而是判到包围盒最近点的距离。
    // 这样大物件也能在“贴边靠近”时被正确判定为够近可以破坏。
    if (DistanceToPickupBounds > EffectiveDisableAcceptanceRadius)
    {
        return false;
    }

    RagePickup->DisableByRageNative(GetPawn());
    bRageSourceDisabled = true;
    LoggedRageMoveSubmitFailureSource = nullptr;
    ActiveRageMoveSourceActor = nullptr;
    ActiveRageMoveGoalLocation = FVector::ZeroVector;
    bHasActiveRageMoveGoalLocation = false;
    ActiveRageDisableAcceptanceRadius = 0.f;
    ActiveRageThreatSources.RemoveAll([RagePickup](const TWeakObjectPtr<AActor>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == RagePickup;
    });
    LogRageThreatState(TEXT("DISABLED_CURRENT"));
    StopMovement();
    ActiveMoveRequestId = FAIRequestID::InvalidRequest;
    bIsMoving = false;
    return true;
}

bool AMyAIController::TryRetargetRageSourceFromLatestStimulus(AActor* IgnoredSourceActor)
{
    CleanupInvalidRageThreatSources();

    APickupActor* BestPickup = nullptr;
    float BestDistanceSquared = TNumericLimits<float>::Max();
    const FVector PawnLocation = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;

    for (const TWeakObjectPtr<AActor>& Entry : ActiveRageThreatSources)
    {
        APickupActor* CandidatePickup = Cast<APickupActor>(Entry.Get());
        if (!IsValid(CandidatePickup)
            || CandidatePickup == IgnoredSourceActor
            || CandidatePickup == RageSourceActor
            || !CandidatePickup->CanBeDisabledByRageNative())
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(PawnLocation, CandidatePickup->GetActorLocation());
        if (!BestPickup || DistanceSquared < BestDistanceSquared)
        {
            BestPickup = CandidatePickup;
            BestDistanceSquared = DistanceSquared;
        }
    }

    if (!IsValid(BestPickup))
    {
        return false;
    }

    RageSourceActor = BestPickup;
    RageSourceLocation = BestPickup->GetActorLocation();
    bHasRageSourceLocation = true;
    bRageSourceDisabled = false;
    LoggedRageMoveSubmitFailureSource = nullptr;
    ActiveRageMoveSourceActor = nullptr;
    ActiveRageMoveGoalLocation = FVector::ZeroVector;
    bHasActiveRageMoveGoalLocation = false;
    ActiveRageDisableAcceptanceRadius = 0.f;
    LogRageThreatState(TEXT("RETARGETED"));
    return true;
}

void AMyAIController::RegisterRageThreatSource(AActor* SourceActor)
{
    APickupActor* PickupSource = Cast<APickupActor>(SourceActor);
    if (!IsValid(PickupSource) || !PickupSource->CanBeDisabledByRageNative())
    {
        return;
    }

    CleanupInvalidRageThreatSources();

    for (const TWeakObjectPtr<AActor>& Entry : ActiveRageThreatSources)
    {
        if (Entry.Get() == PickupSource)
        {
            return;
        }
    }

    ActiveRageThreatSources.Add(PickupSource);
    LogRageThreatState(TEXT("REGISTERED"));
}

void AMyAIController::CleanupInvalidRageThreatSources()
{
    ActiveRageThreatSources.RemoveAll([](const TWeakObjectPtr<AActor>& Entry)
    {
        const APickupActor* Pickup = Cast<APickupActor>(Entry.Get());
        return !IsValid(Pickup) || !Pickup->CanBeDisabledByRageNative();
    });
}

void AMyAIController::LogRageThreatState(const TCHAR* Context) const
{
    if (!bLogRageThreats)
    {
        return;
    }

    TArray<FString> ThreatNames;
    int32 ValidThreatCount = 0;

    for (const TWeakObjectPtr<AActor>& Entry : ActiveRageThreatSources)
    {
        const APickupActor* Pickup = Cast<APickupActor>(Entry.Get());
        if (!IsValid(Pickup))
        {
            continue;
        }

        ++ValidThreatCount;
        ThreatNames.Add(FString::Printf(
            TEXT("%s[%s]"),
            *GetNameSafe(Pickup),
            Pickup->CanBeDisabledByRageNative() ? TEXT("active") : TEXT("disabled")));
    }

    const FString ThreatSummary = ThreatNames.Num() > 0
        ? FString::Join(ThreatNames, TEXT(", "))
        : TEXT("<none>");

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("RAGE THREATS %s Count=%d Current=%s LatestStimulus=%s Entries=%s"),
        Context,
        ValidThreatCount,
        *GetNameSafe(RageSourceActor.Get()),
        *GetNameSafe(ActiveFearStimulus.SourceActor.Get()),
        *ThreatSummary);
}

void AMyAIController::ClearRageSourceState()
{
    RageSourceActor = nullptr;
    RageSourceLocation = FVector::ZeroVector;
    bHasRageSourceLocation = false;
    bRageSourceDisabled = false;
    LoggedRageMoveSubmitFailureSource = nullptr;
    ActiveRageMoveSourceActor = nullptr;
    ActiveRageMoveGoalLocation = FVector::ZeroVector;
    bHasActiveRageMoveGoalLocation = false;
    ActiveRageDisableAcceptanceRadius = 0.f;
}

// 进入 Rage：鬼会临时无视 Fear 逃跑逻辑，转而高速反扑。
void AMyAIController::EnterRageState()
{
    CleanupInvalidAttachedDisableSources();
    if (HasAttachedDisableSource())
    {
        bPendingRageEnter = false;
        return;
    }

    if (CurrentState == EEnemyAIState::Rage)
    {
        return;
    }

    bPendingRageEnter = false;
    ClearPatrolRetry();
    ClearInvestigationState();
    bPendingFearRepath = false;
    CurrentFearHidePoint = nullptr;
    FearRetreatTimeRemaining = 0.f;
    FearResponseMode = EFearResponseMode::Rage;
    RageTimeRemaining = RageDuration;
    StunTimeRemaining = 0.f;
    StunImmunityTimeRemaining = FMath::Max(StunImmunityTimeRemaining, RageStunImmunityDuration);
    bFearRetreatInputActive = false;
    CacheRageSourceFromFear();
    LogRageThreatState(TEXT("ENTER_RAGE"));
    // Rage 已经正式触发后，当前这一轮累计怒气就应该清零重新开始。
    // 否则像音响这种持续脉冲源会让鬼在 Rage 结束后立刻再次满怒重进，
    // 表现成 "RAGE BREAK ITEM" 倒计时不停回到 10 秒。
    ResetFearAnger();
    SetAIState(EEnemyAIState::Rage);
    SetControlledWalkSpeed(ResolveRageWalkSpeed());
    StopMovement();

    ClearFear();
}

// Rage 持续期间的行为：优先冲可见玩家；如果暂时看不到，就冲向最后已知玩家位置。
void AMyAIController::UpdateRageState(float DeltaTime)
{
    RageTimeRemaining = FMath::Max(0.f, RageTimeRemaining - DeltaTime);
    SetControlledWalkSpeed(ResolveRageWalkSpeed());

    if (!HasActiveRageDisableTarget())
    {
        TryRetargetRageSourceFromLatestStimulus();
    }

    // Rage 的优先级链是：
    // 贴脸就地破坏恐惧源 > 冲向可破坏恐惧源 > 冲向恐惧源位置 > 可见玩家 > 重新获取可见玩家 > 冲向最后见到的玩家位置。
    // 这样 Rage 一旦已经确认“是谁把我激怒了”，会优先把那个道具拆掉，
    // 避免被玩家可见性或最后已知位置分走优先级，导致音响一直不被处理。
    AActor* DisabledSourceActor = RageSourceActor.Get();
    if (TryDisableRageSource())
    {
        if (TryRetargetRageSourceFromLatestStimulus(DisabledSourceActor))
        {
            return;
        }

        ExitRageState(true);
        return;
    }
    else if (HasActiveRageDisableTarget())
    {
        AActor* CurrentRageSource = RageSourceActor.Get();
        APickupActor* RagePickup = Cast<APickupActor>(CurrentRageSource);
        FVector RageMoveLocation = RageSourceLocation;
        bool bHasValidRageMoveLocation = bHasRageSourceLocation;
        float EffectiveDisableAcceptanceRadius = RageDisableItemAcceptanceRadius;

        if (IsValid(RagePickup) && GetPawn())
        {
            FVector PickupOrigin = RagePickup->GetActorLocation();
            FVector PickupExtent = FVector::ZeroVector;
            RagePickup->GetActorBounds(true, PickupOrigin, PickupExtent);

            RageSourceLocation = PickupOrigin;
            bHasRageSourceLocation = true;
            bHasValidRageMoveLocation = TryFindRagePickupApproachLocation(
                GetPawn(),
                RagePickup,
                RageDisableItemAcceptanceRadius,
                RageMoveLocation,
                EffectiveDisableAcceptanceRadius);

            if (!bHasValidRageMoveLocation)
            {
                const FVector PawnLocation = GetPawn()->GetActorLocation();
                const FVector ClosestPointOnPickupBounds = PawnLocation.BoundToBox(
                    PickupOrigin - PickupExtent,
                    PickupOrigin + PickupExtent);

                RageMoveLocation = ClosestPointOnPickupBounds;
                bHasValidRageMoveLocation = true;

                if (UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld()))
                {
                    FNavLocation NavLocation;
                    if (NavSystem->ProjectPointToNavigation(RageMoveLocation, NavLocation, FVector(350.f, 350.f, 250.f)))
                    {
                        RageMoveLocation = NavLocation.Location;
                        EffectiveDisableAcceptanceRadius = FMath::Clamp(
                            GetDistanceToBounds2D(RageMoveLocation, PickupOrigin, PickupExtent) + 20.f,
                            RageDisableItemAcceptanceRadius,
                            RageDisableItemAcceptanceRadius + MaxRageDisableRadiusExpansion);
                    }
                    else if (bHasRageSourceLocation
                        && NavSystem->ProjectPointToNavigation(RageSourceLocation, NavLocation, FVector(350.f, 350.f, 250.f)))
                    {
                        RageMoveLocation = NavLocation.Location;
                        EffectiveDisableAcceptanceRadius = FMath::Clamp(
                            GetDistanceToBounds2D(RageMoveLocation, PickupOrigin, PickupExtent) + 20.f,
                            RageDisableItemAcceptanceRadius,
                            RageDisableItemAcceptanceRadius + MaxRageDisableRadiusExpansion);
                    }
                }
            }
        }

        const bool bShouldSubmitNewRageMove = !bIsMoving
            || !ActiveMoveRequestId.IsValid()
            || ActiveRageMoveSourceActor.Get() != CurrentRageSource
            || !bHasActiveRageMoveGoalLocation
            || FVector::DistSquared2D(ActiveRageMoveGoalLocation, RageMoveLocation) > FMath::Square(20.f);

        if (bShouldSubmitNewRageMove)
        {
            ActiveRageMoveSourceActor = CurrentRageSource;
            ActiveRageMoveGoalLocation = RageMoveLocation;
            bHasActiveRageMoveGoalLocation = bHasValidRageMoveLocation;
            ActiveRageDisableAcceptanceRadius = EffectiveDisableAcceptanceRadius;

            bIsMoving = bHasValidRageMoveLocation
                && RequestMoveToLocation(RageMoveLocation, EffectiveDisableAcceptanceRadius);

            if (bIsMoving)
            {
                LoggedRageMoveSubmitFailureSource = nullptr;
            }
            else
            {
                float DistanceToPickupBounds = -1.f;

                if (APickupActor* CurrentRagePickup = Cast<APickupActor>(CurrentRageSource))
                {
                    FVector PickupBoundsOrigin = CurrentRagePickup->GetActorLocation();
                    FVector PickupBoundsExtent = FVector::ZeroVector;
                    CurrentRagePickup->GetActorBounds(true, PickupBoundsOrigin, PickupBoundsExtent);

                    if (APawn* ControlledPawn = GetPawn())
                    {
                        const FVector PawnLocation = ControlledPawn->GetActorLocation();
                        const FVector ClosestPointOnPickupBounds = PawnLocation.BoundToBox(
                            PickupBoundsOrigin - PickupBoundsExtent,
                            PickupBoundsOrigin + PickupBoundsExtent);
                        DistanceToPickupBounds = FVector::Dist2D(PawnLocation, ClosestPointOnPickupBounds);

                        const float ExpandedDisableAcceptanceRadius = FMath::Clamp(
                            DistanceToPickupBounds + 20.f,
                            RageDisableItemAcceptanceRadius,
                            RageDisableItemAcceptanceRadius + MaxRageDisableRadiusExpansion);
                        ActiveRageMoveSourceActor = CurrentRageSource;
                        ActiveRageDisableAcceptanceRadius = FMath::Max(
                            ActiveRageDisableAcceptanceRadius,
                            ExpandedDisableAcceptanceRadius);
                    }
                }

                if (bLogRageThreats && LoggedRageMoveSubmitFailureSource.Get() != CurrentRageSource)
                {
                    LoggedRageMoveSubmitFailureSource = CurrentRageSource;
                    UE_LOG(
                        LogTemp,
                        Warning,
                        TEXT("RAGE THREATS MOVE_SUBMIT_FAILED Current=%s DistanceToBounds=%.1f Acceptance=%.1f EffectiveDisableAcceptance=%.1f SourceLocation=(%.1f, %.1f, %.1f)"),
                        *GetNameSafe(CurrentRageSource),
                        DistanceToPickupBounds,
                        RageDisableItemAcceptanceRadius,
                        ActiveRageDisableAcceptanceRadius,
                        RageMoveLocation.X,
                        RageMoveLocation.Y,
                        RageMoveLocation.Z);
                }
            }
        }
    }
    // 来源物还没被禁用，但当前已经拿不到一个可直接操作的 RagePickup 接口时，
    // 仍然可以先冲向记录下来的来源位置。
    else if (!bRageSourceDisabled && bHasRageSourceLocation)
    {
        FVector RageMoveLocation = RageSourceLocation;
        if (UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld()))
        {
            FNavLocation NavLocation;
            if (NavSystem->ProjectPointToNavigation(RageSourceLocation, NavLocation, FVector(250.f, 250.f, 200.f)))
            {
                RageMoveLocation = NavLocation.Location;
                RageSourceLocation = RageMoveLocation;
            }
        }

        bIsMoving = RequestMoveToLocation(RageMoveLocation, RageDisableItemAcceptanceRadius);
    }
    else if (bCanSeePlayer && IsValid(TargetPlayer))
    {
        bIsMoving = RequestMoveToActor(TargetPlayer, ChaseAcceptanceRadius);
    }
    else if (AcquireVisiblePlayerTarget())
    {
        bIsMoving = RequestMoveToActor(TargetPlayer, ChaseAcceptanceRadius);
    }
    // 如果既没有玩家可追，也没有来源物可拆，而当前又没在移动，
    // 就退一步去冲最后见到的玩家位置，维持 Rage 的攻击感。
    else if (!bIsMoving && bHasLastSeenPlayerData)
    {
        bIsMoving = RequestMoveToLocation(LastSeenPlayerLocation, ChaseAcceptanceRadius);
    }
    // 上面的 fallback 也走不通，但又仍然记着 Rage 来源位置时，
    // 说明这次 Rage 已经没有合适动作可做了，直接退出并转入后续调查。
    else if (!bIsMoving && bHasRageSourceLocation)
    {
        ExitRageState(true);
        return;
    }

    // Rage 到时后是否要调查恐惧源，取决于“源位置是否还有效”且“当前看不到玩家”。
    // 如果还看着玩家，就直接把控制权交回追击，不必多绕一层调查。
    if (RageTimeRemaining <= 0.f)
    {
        ExitRageState(bHasRageSourceLocation && !bCanSeePlayer);
    }
}

// Rage 结束后回到正常速度，并重新交给普通状态机决定下一步做什么。
void AMyAIController::ExitRageState(bool bInvestigateFearSource)
{
    if (CurrentState != EEnemyAIState::Rage)
    {
        return;
    }

    const FVector RageSourceInvestigateLocation = RageSourceLocation;
    const TObjectPtr<AActor> RageSourceActorBeforeClear = RageSourceActor;
    // 这里只在退出前先把“是否值得调查 Rage 来源”算出来，
    // 因为下面 ClearRageSourceState 会把位置缓存清掉。
    const bool bShouldInvestigateFearSource = bInvestigateFearSource && bHasRageSourceLocation;

    RageTimeRemaining = 0.f;
    FearResponseMode = EFearResponseMode::None;
    bFearRetreatInputActive = false;
    StunImmunityTimeRemaining = FMath::Max(StunImmunityTimeRemaining, RageStunImmunityDuration);
    // Rage 结束后，如果还要去调查同一个来源，短时间内忽略它再次提供的怒气刺激。
    // 这样鬼不会一边走向音响，一边又被同一个持续脉冲源立刻重新打回 Rage。
    SuppressedFearAngerSourceActor = RageSourceActorBeforeClear;
    SuppressedFearAngerSourceLocation = RageSourceInvestigateLocation;
    SuppressedStunSourceActor = RageSourceActorBeforeClear;
    SuppressedStunSourceLocation = RageSourceInvestigateLocation;
    SuppressedFearAngerTimeRemaining = bShouldInvestigateFearSource
        ? FMath::Max(RageSourceInvestigateDuration, RageStunImmunityDuration)
        : 0.f;
    SuppressedStunTimeRemaining = bShouldInvestigateFearSource
        ? FMath::Max(RageSourceInvestigateDuration, RageStunImmunityDuration)
        : 0.f;
    RestoreWalkSpeedAfterFear();
    ClearRageSourceState();

    if (bCanSeePlayer && IsValid(TargetPlayer))
    {
        ChasePlayer();
        return;
    }

    if (AcquireVisiblePlayerTarget())
    {
        ChasePlayer();
        return;
    }

    // 只有退出 Rage 时明确要求调查来源，且来源位置确实有效，才补一轮 Investigate。
    // 这让 Rage 结束后既不会无脑回巡逻，也不会在没有有效来源时去查一个空点。
    if (bShouldInvestigateFearSource)
    {
        ApplyInvestigationFromLocation(RageSourceInvestigateLocation, RageSourceInvestigateDuration,
            RageDisabledItemStimulusTag);
        return;
    }

    if (bHasInvestigateTarget)
    {
        MoveToInvestigateTarget();
        return;
    }

    Patrol();
}

// 统一设置 AI 当前状态。
// 现在只是简单赋值，但封装后以后加日志、调试或状态切换检查会更方便。
void AMyAIController::SetAIState(EEnemyAIState NewState)
{
    CurrentState = NewState;
}

// 生成当前状态对应的调试文本。
// 什么时候调用：DrawDebugSight 在显示头顶状态文字时会调用这里。
FString AMyAIController::GetStateDebugText() const
{
    switch (CurrentState)
    {
    case EEnemyAIState::Patrol:
        return TEXT("PATROL");
    case EEnemyAIState::Investigate:
        if (CurrentInvestigateStimulusTag == LostSightStimulusTag
            && LostSightTrackTimeRemaining > 0.f
            && IsValid(LostSightTrackedPlayer))
        {
            return FString::Printf(TEXT("SEARCH TRACK  %.1fs / %.1fs"),
                LostSightTrackTimeRemaining, LostSightSearchTimeRemaining);
        }
        if (CurrentInvestigateStimulusTag == LostSightStimulusTag && bIsLookAroundActive)
        {
            const TCHAR* PhaseText = TEXT("LOOK");
            switch (LookAroundPhase)
            {
            case ELostSightLookAroundPhase::FaceLastKnown:
                PhaseText = TEXT("LOOK FRONT");
                break;
            case ELostSightLookAroundPhase::SweepLeft:
                PhaseText = TEXT("LOOK LEFT");
                break;
            case ELostSightLookAroundPhase::SweepRight:
                PhaseText = TEXT("LOOK RIGHT");
                break;
            case ELostSightLookAroundPhase::ReturnCenter:
                PhaseText = TEXT("LOOK RESET");
                break;
            case ELostSightLookAroundPhase::Completed:
            default:
                break;
            }

            return FString::Printf(TEXT("SEARCH %s  %.1fs"), PhaseText, LostSightSearchTimeRemaining);
        }
        if (CurrentInvestigateStimulusTag == LostSightStimulusTag)
        {
            return FString::Printf(TEXT("SEARCH MOVE  %.1fs"), LostSightSearchTimeRemaining);
        }
        if (bHasInvestigateTarget)
        {
            return FString::Printf(TEXT("INVESTIGATE  %.1fs"), InvestigateWaitTimeRemaining);
        }
        return TEXT("INVESTIGATE");
    case EEnemyAIState::Chase:
        return TEXT("CHASE");
    case EEnemyAIState::Flee:
        if (FearResponseMode == EFearResponseMode::Retreat)
        {
            return FString::Printf(TEXT("RETREAT INPUT=%s  %.2fs  Anger %d/%d"),
                bFearRetreatInputActive ? TEXT("YES") : TEXT("NO"),
                FearRetreatTimeRemaining,
                FearAngerLevel,
                FearAngerThreshold);
        }
        if (FearResponseMode == EFearResponseMode::EscapeToHide)
        {
            return FString::Printf(TEXT("FEAR HIDE  %.1fs  %s  %d/%d"),
                FearHideEscapeTimeRemaining,
                bFearHoldingAtHidePoint ? TEXT("HIDING") : (bFearDetourActive ? TEXT("DETOUR") : TEXT("RUN")),
                FearAngerLevel,
                FearAngerThreshold);
        }
        if (IsFearActive())
        {
            return FString::Printf(TEXT("FLEE  %.1fs"), GetFearTimeRemaining());
        }
        return TEXT("FLEE");
    case EEnemyAIState::Stunned:
        return FString::Printf(TEXT("STUNNED  %.1fs"), StunTimeRemaining);
    case EEnemyAIState::Rage:
        if (HasActiveRageDisableTarget())
        {
            return FString::Printf(TEXT("RAGE BREAK ITEM  %.1fs"), RageTimeRemaining);
        }
        if (bHasRageSourceLocation && !bCanSeePlayer)
        {
            return FString::Printf(TEXT("RAGE TO SOURCE  %.1fs"), RageTimeRemaining);
        }
        return FString::Printf(TEXT("RAGE  %.1fs"), RageTimeRemaining);
    default:
        return TEXT("UNKNOWN");
    }
}

// 给当前状态选择对应颜色。
// 同样主要服务于调试绘制，让不同状态在画面里更容易区分。
FColor AMyAIController::GetStateDebugColor() const
{
    switch (CurrentState)
    {
    case EEnemyAIState::Patrol:
        return PatrolStateColor;
    case EEnemyAIState::Investigate:
        return InvestigateStateColor;
    case EEnemyAIState::Chase:
        return ChaseStateColor;
    case EEnemyAIState::Flee:
        return FleeStateColor;
    case EEnemyAIState::Stunned:
        return StunnedStateColor;
    case EEnemyAIState::Rage:
        return RageStateColor;
    default:
        return FColor::White;
    }
}

// 统一绘制这个 AI 的所有调试信息。
// 什么时候调用：Tick 中如果 bShowDebugSight 为 true，就会每帧执行一次。
// 这里绘制的内容只用于开发调试，不会真正改变 AI 行为。
void AMyAIController::DrawDebugSight()
{
    APawn* ControlledPawn = GetPawn();
    UWorld* World = GetWorld();
    if (!IsValid(ControlledPawn) || !IsValid(World))
    {
        return;
    }

    FVector AILocation = ControlledPawn->GetActorLocation();
    FRotator EyeRotation = ControlledPawn->GetActorRotation();
    GetActorEyesViewPoint(AILocation, EyeRotation);
    const FVector ForwardVector = EyeRotation.Vector();
    const float PeripheralVisionAngleDegrees = IsValid(SightConfig)
        ? SightConfig->PeripheralVisionAngleDegrees
        : DebugFallbackPeripheralVisionAngleDegrees;
    const float SightRadius = IsValid(SightConfig)
        ? SightConfig->SightRadius
        : DebugFallbackSightRadius;
    const float LoseSightRadius = IsValid(SightConfig)
        ? SightConfig->LoseSightRadius
        : DebugFallbackLoseSightRadius;
    const float HalfAngle = FMath::DegreesToRadians(PeripheralVisionAngleDegrees);
    const FColor CurrentColor = bCanSeePlayer ? DetectedColor : SightColor;

    // 第一层锥体显示实时可见范围，第二层显示丢失目标前还能保留感知的范围。
    // DrawDebugCone / DrawDebugLine / DrawDebugSphere / DrawDebugString 都是 UE 的调试绘制 API，
    // 它们只负责把辅助图形画到场景里，方便你观察 AI 行为。
    DrawDebugCone(
        World,
        AILocation,
        ForwardVector,
        SightRadius,
        HalfAngle,
        HalfAngle,
        32,
        CurrentColor,
        false,
        -1.f,
        0,
        2.f);

    DrawDebugCone(
        World,
        AILocation,
        ForwardVector,
        LoseSightRadius,
        HalfAngle,
        HalfAngle,
        32,
        LoseSightColor,
        false,
        -1.f,
        0,
        1.f);

    const FVector LeftBoundary = ForwardVector.RotateAngleAxis(
        -PeripheralVisionAngleDegrees, FVector::UpVector);
    DrawDebugLine(
        World,
        AILocation,
        AILocation + LeftBoundary * SightRadius,
        CurrentColor,
        false,
        -1.f,
        0,
        3.f);

    const FVector RightBoundary = ForwardVector.RotateAngleAxis(
        PeripheralVisionAngleDegrees, FVector::UpVector);
    DrawDebugLine(
        World,
        AILocation,
        AILocation + RightBoundary * SightRadius,
        CurrentColor,
        false,
        -1.f,
        0,
        3.f);

    if (bShowDetectionLines && bCanSeePlayer && IsValid(TargetPlayer))
    {
        const FVector TargetLocation = TargetPlayer->GetActorLocation();

        DrawDebugLine(
            World,
            AILocation,
            TargetLocation,
            FColor::Red,
            false,
            -1.f,
            0,
            5.f);

        DrawDebugSphere(
            World,
            TargetLocation,
            50.f,
            12,
            FColor::Red,
            false,
            -1.f);
    }

    if (CurrentState == EEnemyAIState::Patrol && !bCanSeePlayer && bIsMoving)
    {
        DrawDebugSphere(
            World,
            PatrolTarget,
            30.f,
            12,
            FColor::Cyan,
            false,
            -1.f);

        DrawDebugLine(
            World,
            AILocation,
            PatrolTarget,
            FColor::Cyan,
            false,
            -1.f,
            0,
            2.f);
    }

    if (bShowInvestigateDebug && bHasInvestigateTarget)
    {
        if (CurrentInvestigateStimulusTag == LostSightStimulusTag && bHasLastSeenPlayerData)
        {
            DrawDebugSphere(
                World,
                LastSeenPlayerLocation,
                28.f,
                12,
                FColor(192, 192, 192),
                false,
                -1.f);

            if (IsValid(LostSightTrackedPlayer) && LostSightTrackTimeRemaining > 0.f)
            {
                DrawDebugLine(
                    World,
                    LastSeenPlayerLocation,
                    LostSightTrackedPlayer->GetActorLocation(),
                    FColor(192, 192, 192),
                    false,
                    -1.f,
                    0,
                    1.5f);
            }
        }

        DrawDebugSphere(
            GetWorld(),
            InvestigateTarget,
            40.f,
            12,
            InvestigateStateColor,
            false,
            -1.f);

        DrawDebugLine(
            GetWorld(),
            AILocation,
            InvestigateTarget,
            InvestigateStateColor,
            false,
            -1.f,
            0,
            2.f);
    }

    if (bShowFearDebug && IsFearActive())
    {
        DrawDebugLine(
            GetWorld(),
            GetFearSourceLocation(),
            AILocation,
            FearDebugColor,
            false,
            -1.f,
            0,
            2.f);

        DrawDebugDirectionalArrow(
            GetWorld(),
            AILocation,
            CurrentFleeTarget,
            100.f,
            FearDebugColor,
            false,
            -1.f,
            0,
            4.f);

        DrawDebugSphere(
            GetWorld(),
            CurrentFleeTarget,
            35.f,
            12,
            FearDebugColor,
            false,
            -1.f);

        if (IsValid(CurrentFearHidePoint))
        {
            DrawDebugSphere(
                GetWorld(),
                CurrentFearHidePoint->GetActorLocation(),
                45.f,
                12,
                FColor::Blue,
                false,
                -1.f);

            DrawDebugLine(
                GetWorld(),
                AILocation,
                CurrentFearHidePoint->GetActorLocation(),
                FColor::Blue,
                false,
                -1.f,
                0,
                2.f);
        }
    }

    if (bShowFearDebug && !FearHidePointTag.IsNone())
    {
        TArray<AActor*> TaggedHidePoints;
        GatherFearHidePointCandidates(TaggedHidePoints);

        for (AActor* HidePoint : TaggedHidePoints)
        {
            if (!IsValid(HidePoint))
            {
                continue;
            }

            const FVector HidePointLocation = HidePoint->GetActorLocation();
            FVector ProjectedNavLocation = FVector::ZeroVector;
            const bool bHasProjectedNavLocation = ResolveFearHideDestination(HidePoint, ProjectedNavLocation);
            const bool bIsCurrentHidePoint = HidePoint == CurrentFearHidePoint;

            const FColor HidePointColor = bIsCurrentHidePoint
                ? FColor::Blue
                : (bHasProjectedNavLocation ? FColor::Emerald : FColor::Red);

            DrawDebugSphere(
                GetWorld(),
                HidePointLocation,
                28.f,
                12,
                HidePointColor,
                false,
                -1.f,
                0,
                2.f);

            if (bHasProjectedNavLocation)
            {
                DrawDebugSphere(
                    GetWorld(),
                    ProjectedNavLocation,
                    20.f,
                    10,
                    FColor::Green,
                    false,
                    -1.f,
                    0,
                    2.f);

                DrawDebugLine(
                    GetWorld(),
                    HidePointLocation,
                    ProjectedNavLocation,
                    FColor::Green,
                    false,
                    -1.f,
                    0,
                    1.5f);
            }

            const FString HidePointDebugText = FString::Printf(
                TEXT("%s\nRAW %s\nNAV %s"),
                *GetNameSafe(HidePoint),
                *HidePointLocation.ToCompactString(),
                bHasProjectedNavLocation ? *ProjectedNavLocation.ToCompactString() : TEXT("NO NAV"));

            DrawDebugString(
                GetWorld(),
                HidePointLocation + FVector(0.f, 0.f, 90.f),
                HidePointDebugText,
                nullptr,
                HidePointColor,
                0.f,
                true,
                1.2f);
        }
    }

    if (CurrentState == EEnemyAIState::Rage && bHasLastSeenPlayerData)
    {
        DrawDebugSphere(
            GetWorld(),
            LastSeenPlayerLocation,
            45.f,
            12,
            RageStateColor,
            false,
            -1.f);
    }

    // State 文本和调试线帮助快速判断 AI 当前是在巡逻、调查、追击还是逃跑。
    if (bShowStateText)
    {
        DrawDebugString(
            GetWorld(),
            AILocation + FVector(0.f, 0.f, 120.f),
            GetStateDebugText(),
            nullptr,
            GetStateDebugColor(),
            0.f,
            true,
            1.5f);
    }
}