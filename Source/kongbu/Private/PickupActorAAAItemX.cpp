// PickupActorAAAItemX.cpp
#include "PickupActorAAAItemX.h"

#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "MyAIController.h"

APickupActorAAAItemX::APickupActorAAAItemX()
{
    // Tick 用来累计脉冲时间，并按间隔发射声波。
    PrimaryActorTick.bCanEverTick = true;

    HoldType = EHoldItemType::ItemX;
    FP_SocketName = TEXT("RightSocket");
    FP_LocationOffset = FVector::ZeroVector;
    FP_RotationOffset = FRotator::ZeroRotator;
    TP_SocketName = TEXT("RightSocket");
    TP_LocationOffset = FVector::ZeroVector;
    TP_RotationOffset = FRotator::ZeroRotator;

    ItemMassKg = 8.0f;
    ItemThrowForceMultiplier = 1.6f;
    ItemLinearDamping = 0.25f;
    ItemAngularDamping = 0.9f;
    ItemThrowSpinRateDegrees = 220.f;

    bAllowPawnCollision = true;
    bAllowPhysicsBodyCollision = true;
    ApplyReleasedCollisionProfile();

    // 给 ItemX 打上常用标签，便于蓝图、调试和关卡脚本快速识别。
    Tags.Add(FName("ItemX"));
    Tags.Add(FName("Pickup"));
    Tags.Add(FName("Collectible"));
}

void APickupActorAAAItemX::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bSonarActive)
    {
        return;
    }

    // 一个时间用于动画表现，另一个时间用于决定何时真正发出下一次脉冲。
    SonarPulseTime += DeltaTime;
    SonarPulseAccumulator += DeltaTime;

    if (bEnableSonarDebug)
    {
        DrawSonarDebug();
    }

    if (SonarPulseAccumulator >= SonarPulseInterval)
    {
        SonarPulseAccumulator = 0.f;
        EmitSonarPulse();
    }
}

void APickupActorAAAItemX::ActivateSonar()
{
    // Rage 失控状态下，这个物品已经被判定为“不可再启用”，
    // 所以这里即使外部强行调用开启，也会立刻回落到关闭状态。
    if (IsDisabledByRage())
    {
        DeactivateSonar();
        return;
    }

    // 玩家主动打开后，需要把“手动关闭”状态清掉，
    // 否则放下或投掷时生命周期逻辑会继续把它视为应保持关闭。
    bClosedByPlayer = false;
    bSonarActive = true;

    // 每次重新开启都从一轮新脉冲开始，避免沿用旧时间导致调试动画或脉冲节奏跳变。
    SonarPulseTime = 0.f;
    SonarPulseAccumulator = 0.f;

    // 如果之前因为投掷挂了一个延迟激活定时器，这里需要先清掉，防止重复触发。
    GetWorldTimerManager().ClearTimer(DelayedSonarActivationHandle);

    // 开启瞬间先打一发，让 AI 反馈更直接，不必等到下一个 Tick 周期。
    EmitSonarPulse();
    UE_LOG(LogTemp, Log, TEXT("Sonar Activated"));
}

void APickupActorAAAItemX::DeactivateSonar()
{
    bSonarActive = false;

    // 关闭后无需继续累计下一发脉冲，但调试动画时间是否保留并不重要；
    // 下次 ActivateSonar 会统一重置。
    SonarPulseAccumulator = 0.f;

    // 关闭时顺带取消延迟激活，确保“关掉”是最终结果，不会过一会又被重新打开。
    GetWorldTimerManager().ClearTimer(DelayedSonarActivationHandle);
    UE_LOG(LogTemp, Log, TEXT("Sonar Deactivated"));
}

void APickupActorAAAItemX::EmitSonarPulse()
{
    // 没开声波或世界无效时，不应该产生任何 AI 侧效果。
    if (!bSonarActive || !GetWorld())
    {
        return;
    }

    // 这个开关主要用于调试和玩法拆分：保留视觉脉冲，但暂时不驱动 AI 逻辑。
    if (!bAffectAIControllers)
    {
        return;
    }

    // 先构建一份“本次脉冲”的恐惧刺激模板，
    // 后面命中的每个 AI 都会收到同一批参数。
    FFearStimulus Stimulus;
    Stimulus.SourceLocation = GetActorLocation();
    Stimulus.Duration = AIReactionDuration;
    Stimulus.StimulusTag = SonarStimulusTag;
    Stimulus.SourceActor = this;

    // 手持时更容易激怒鬼，因此 Rage 累积值使用单独配置；
    // Fear 的来源位置、时长和标签仍然共用这一份刺激。
    Stimulus.RageAngerAmount = IsHeldByPlayer()
        ? FMath::Max(1, HeldFearAngerAmount)
        : FMath::Max(1, PlacedFearAngerAmount);

    // 遍历世界里的 Pawn，但只处理确实由 MyAIController 驱动的角色。
    // 这样可以避免误伤玩家、普通 NPC 或还没挂上目标控制器的测试 Pawn。
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        APawn* TargetPawn = *It;
        if (!IsValid(TargetPawn))
        {
            continue;
        }

        AMyAIController* FearController = Cast<AMyAIController>(TargetPawn->GetController());
        if (!FearController)
        {
            continue;
        }

        // 第一层筛选是扇形几何范围：距离够近，而且方向必须落在 ItemX 前方夹角内。
        if (!IsActorInsideSonarCone(TargetPawn))
        {
            continue;
        }

        // 第二层筛选是视线阻挡：如果配置要求 LOS，则墙、门或遮挡体会截断这次脉冲。
        if (bRequireLineOfSightToAffectTargets && !HasLineOfSightToTarget(TargetPawn))
        {
            continue;
        }

        // 只有通过全部筛选的 AI，才会真正收到这一发恐惧刺激。
        FearController->ApplyFearStimulus(Stimulus);
    }
}

void APickupActorAAAItemX::DrawSonarDebug()
{
    // 调试图形由一个主锥体、两条边界线和多层波纹组成，方便观察声波方向和节奏。
    if (!GetWorld()) return;

    FVector SonarLocation = GetActorLocation();
    FVector ForwardVector = GetActorForwardVector();
    float HalfAngleRadians = FMath::DegreesToRadians(SonarAngle);

    // PulseAlpha 让主锥体透明度随时间呼吸变化，更容易在编辑器里观察“正在发声”。
    float PulseAlpha = FMath::Abs(FMath::Sin(SonarPulseTime * 2.f));
    FColor PulseColor = FColor(
        SonarColor.R,
        SonarColor.G,
        SonarColor.B,
        static_cast<uint8>(100 + PulseAlpha * 155)
    );

    DrawDebugCone(
        GetWorld(),
        SonarLocation,
        ForwardVector,
        SonarRadius,
        HalfAngleRadians,
        HalfAngleRadians,
        32,
        PulseColor,
        false,
        -1.f,
        0,
        2.f
    );

    // 额外画出左右边界线，便于确认锥体边缘与目标夹角是否匹配。
    FVector LeftBoundary = ForwardVector.RotateAngleAxis(-SonarAngle, FVector::UpVector);
    DrawDebugLine(GetWorld(), SonarLocation, SonarLocation + LeftBoundary * SonarRadius,
        SonarColor, false, -1.f, 0, 3.f);

    FVector RightBoundary = ForwardVector.RotateAngleAxis(SonarAngle, FVector::UpVector);
    DrawDebugLine(GetWorld(), SonarLocation, SonarLocation + RightBoundary * SonarRadius,
        SonarColor, false, -1.f, 0, 3.f);

    // 多层波纹只用于调试显示，不参与实际判定。
    int32 WaveCount = 3;
    for (int32 i = 1; i <= WaveCount; ++i)
    {
        float WaveRadius = SonarRadius * (i / static_cast<float>(WaveCount));
        float WavePhase = SonarPulseTime * 3.f - i * 0.5f;
        float WaveAlpha = FMath::Clamp(FMath::Abs(FMath::Sin(WavePhase)), 0.f, 1.f);

        FColor WaveColor = FColor(
            SonarColor.R,
            SonarColor.G,
            SonarColor.B,
            static_cast<uint8>(WaveAlpha * 100)
        );

        DrawDebugCone(
            GetWorld(),
            SonarLocation,
            ForwardVector,
            WaveRadius,
            HalfAngleRadians,
            HalfAngleRadians,
            24,
            WaveColor,
            false,
            -1.f,
            0,
            1.f
        );
    }

    // 中心球帮助确认声波发射原点。
    DrawDebugSphere(GetWorld(), SonarLocation, 20.f, 12, SonarColor, false, -1.f);
}

void APickupActorAAAItemX::OnPickedUp()
{
    // 拿起后关闭声波，避免玩家手持时持续影响 AI。
    Super::OnPickedUp();

    DeactivateSonar();

    UE_LOG(LogTemp, Log, TEXT("APickupActorAAAItemX Picked Up"));
}

void APickupActorAAAItemX::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    // 放下后是否自动激活，取决于它是被设计成陷阱还是普通可摆放道具。
    Super::OnPutDown(PlaceLocation, PlaceRotation);

    // 如果玩家之前明确把它关了，那么即使现在放回地面，也要尊重这个状态。
    if (bClosedByPlayer)
    {
        DeactivateSonar();
        UE_LOG(LogTemp, Log, TEXT("APickupActorAAAItemX remains closed after put down"));
        return;
    }

    if (bAutoActivateSonarWhenPlaced)
    {
        ActivateSonar();
    }
    else
    {
        DeactivateSonar();
    }

    UE_LOG(LogTemp, Log, TEXT("APickupActorAAAItemX Put Down"));
}

void APickupActorAAAItemX::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    // 投掷时先按基类逻辑飞出去，再根据配置决定是否延迟重新发声。
    Super::OnThrown(ThrowDirection, ThrowForce);

    // 玩家手动关闭过，或者设计上不允许投掷后自动激活时，直接保持静默。
    if (bClosedByPlayer || !bAutoActivateSonarWhenThrown)
    {
        DeactivateSonar();
        return;
    }

    // 小于等于 0 表示不需要缓冲，投出后立刻重新开始发声。
    if (ThrowSonarActivationDelay <= 0.f)
    {
        ActivateSonar();
        return;
    }

    // 使用一次性定时器，把“重新发声”的时机推迟到投掷动作之后。
    GetWorldTimerManager().SetTimer(
        DelayedSonarActivationHandle,
        this,
        &APickupActorAAAItemX::ActivateSonar,
        ThrowSonarActivationDelay,
        false);
}

bool APickupActorAAAItemX::CanBeClosedByPlayer_Implementation() const
{
    // ItemX 设计上始终允许玩家手动关闭；如果以后有剧情锁定需求，可以从这里收口。
    return true;
}

void APickupActorAAAItemX::CloseByPlayer_Implementation(AActor* ClosingActor)
{
    // 先记录“这是玩家主动关闭”的事实，再真正停掉声波。
    // 后续放下、投掷等生命周期都会读取这个状态，决定是否自动重开。
    bClosedByPlayer = true;
    DeactivateSonar();

    UE_LOG(LogTemp, Log, TEXT("%s sonar closed by %s"),
        *GetName(),
        *GetNameSafe(ClosingActor));
}

bool APickupActorAAAItemX::IsClosedByPlayer_Implementation() const
{
    // 对外表现上，只要被玩家手动关掉，或者当前根本没在发声，都视为“关闭”。
    return bClosedByPlayer || !bSonarActive;
}

void APickupActorAAAItemX::OpenByPlayer_Implementation(AActor* OpeningActor)
{
    // 打开操作本质上就是重新执行完整的声波激活流程。
    ActivateSonar();

    // 如果 ActivateSonar 因 Rage 禁用等原因失败，就不要再打印“已打开”的日志。
    if (!bSonarActive)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("%s sonar opened by %s"),
        *GetName(),
        *GetNameSafe(OpeningActor));
}

void APickupActorAAAItemX::DisableByRage_Implementation(AActor* DisablingActor)
{
    // Rage 关闭属于强制禁用，不只是暂时停声；
    // 因此这里也把 bClosedByPlayer 置为 true，让后续自动激活流程全部失效。
    Super::DisableByRage_Implementation(DisablingActor);
    bClosedByPlayer = true;
    DeactivateSonar();

    UE_LOG(LogTemp, Warning, TEXT("%s sonar disabled by Rage actor %s"),
        *GetName(),
        *GetNameSafe(DisablingActor));
}

bool APickupActorAAAItemX::IsActorInsideSonarCone(const AActor* TargetActor) const
{
    if (!IsValid(TargetActor))
    {
        return false;
    }

    // 这里只做水平面判定，忽略 Z，高低差不会影响 ItemX 的扇形覆盖。
    FVector ToTarget = TargetActor->GetActorLocation() - GetActorLocation();
    ToTarget.Z = 0.f;

    if (ToTarget.IsNearlyZero())
    {
        // 目标几乎和 ItemX 重合时，默认认为命中，避免归一化零向量带来不稳定结果。
        return true;
    }

    const float DistanceSquared = ToTarget.SizeSquared();
    if (DistanceSquared > FMath::Square(SonarRadius))
    {
        // 先做半径早退，避免对明显超范围的目标继续做归一化和点积计算。
        return false;
    }

    FVector ForwardVector = GetActorForwardVector();
    ForwardVector.Z = 0.f;
    ForwardVector.Normalize();
    ToTarget.Normalize();

    // 用点积和余弦阈值判断目标是否落在扇形夹角内。
    // 目标方向与前向越接近，点积越接近 1；低于阈值就说明超出夹角。
    const float MinDot = FMath::Cos(FMath::DegreesToRadians(SonarAngle));
    return FVector::DotProduct(ForwardVector, ToTarget) >= MinDot;
}

bool APickupActorAAAItemX::HasLineOfSightToTarget(const AActor* TargetActor) const
{
    if (!GetWorld() || !IsValid(TargetActor))
    {
        return false;
    }

    FVector TargetOrigin;
    FVector TargetExtent;
    TargetActor->GetActorBounds(true, TargetOrigin, TargetExtent);

    // 默认走 Visibility，是因为大多数关卡阻挡体本来就会阻挡该通道，配置成本最低。
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ItemXSonarLineOfSight), false, this);
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(TargetActor);

    FHitResult HitResult;

    // 起点稍微抬高 20cm，避免射线从地面或物体底部起步时，
    // 立刻撞到自己脚下的地板、台阶或贴地碰撞，造成“明明面前没墙却判定被挡住”。
    const FVector TraceStart = GetActorLocation() + FVector(0.f, 0.f, 20.f);

    // 终点使用目标包围盒中心，而不是 Actor 原点，
    // 这样对不同体型 Pawn 更稳定，也更接近“是否看得到这个目标主体”。
    const FVector TraceEnd = TargetOrigin;

    // 沿配置的阻挡通道打一条单射线；只要中途命中任何遮挡物，就认为没有直达视线。
    return !GetWorld()->LineTraceSingleByChannel(
        HitResult,
        TraceStart,
        TraceEnd,
        SonarBlockTraceChannel,
        QueryParams);
}