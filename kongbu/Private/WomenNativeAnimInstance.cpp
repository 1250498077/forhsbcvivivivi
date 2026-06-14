#include "WomenNativeAnimInstance.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetMathLibrary.h"
#include "MyPlayerController.h"
#include "PickupActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "WomenCharacter.h"


void UWomenNativeAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    CurrentHoldType = EHoldItemType::None;
    PreviousHoldType = EHoldItemType::None;
    CurrentHeldActor = nullptr;
    PreviousHeldActor = nullptr;
    CurrentIdlePose = nullptr;
    CurrentLocomotionState = EWomenNativeLocomotionState::Idle;
    PreviousLocomotionState = EWomenNativeLocomotionState::Idle;
    bWasStandThrowing = false;
    bWasSquatThrowing = false;
    bWasInAir = false;
    bWasSquat = false;
    bPendingCrouchEnter = false;
    bPendingCrouchExit = false;
    JumpStartTimeRemaining = 0.f;
    SmoothedLookRotation = FRotator::ZeroRotator;
    bAnimationTransitionLocked = false;
    AnimationLockTimeRemaining = 0.f;
    bOneShotActionLocked = false;
    OneShotActionLockTimeRemaining = 0.f;
    ActiveThrowDynamicMontage = nullptr;
    bThrowUsesLoopingUpperBodySequence = false;
    bThrowUsesFullBodySlot = false;
    Spine2LookRotation = FRotator::ZeroRotator;
    NeckLookRotation = FRotator::ZeroRotator;
    HeadLookRotation = FRotator::ZeroRotator;

    // 默认策略示例：
    // Idle / Walk / Run / FallLoop / Crouch 动作都是循环状态，必须可以随时被打断。
    // JumpStart 是跳跃起手，通常不希望刚播放一帧就被 FallLoop 或 Walk 打断，所以默认 MustFinish。
    IdleAnimation.InterruptPolicy = EWomenNativeAnimationInterruptPolicy::Interruptible;
    WalkAnimation.InterruptPolicy = EWomenNativeAnimationInterruptPolicy::Interruptible;
    RunAnimation.InterruptPolicy = EWomenNativeAnimationInterruptPolicy::Interruptible;
    FallLoopAnimation.InterruptPolicy = EWomenNativeAnimationInterruptPolicy::Interruptible;
    CrouchIdleAnimation.InterruptPolicy = EWomenNativeAnimationInterruptPolicy::Interruptible;
    CrouchWalkAnimation.InterruptPolicy = EWomenNativeAnimationInterruptPolicy::Interruptible;
    JumpStartAnimation.InterruptPolicy = EWomenNativeAnimationInterruptPolicy::MustFinish;
    CrouchEnterAnimation.InterruptPolicy = EWomenNativeAnimationInterruptPolicy::MustFinish;
    CrouchExitAnimation.InterruptPolicy = EWomenNativeAnimationInterruptPolicy::MustFinish;

    if (JumpStartAnimation.LockDuration <= 0.f)
    {
        JumpStartAnimation.LockDuration = JumpStartDuration;
    }

    PreviousLocomotionState = EWomenNativeLocomotionState::Run; // 随便给一个非Idle的值

    InitializeDefaultLookBoneRotations();
}

void UWomenNativeAnimInstance::InitializeDefaultLookBoneRotations()
{
    if (LookBoneRotations.Num() > 0)
    {
        return;
    }

    // 固定骨骼链（你当前项目用的）
    const TArray<FName> LookBoneNames = {
        TEXT("Spine2"),
        TEXT("Neck"),
        TEXT("Head")
    };

    const float PitchWeights[] = { 1.0f, 1.0f, 1.0f };

    for (int32 Index = 0; Index < LookBoneNames.Num(); ++Index)
    {
        // 可选：防御（避免骨骼缺失崩溃）
        if (!HasSkeletonBone(LookBoneNames[Index]))
        {
            continue;
        }

        FNativeLookBoneRotation LookBoneRotation;
        LookBoneRotation.BoneName = LookBoneNames[Index];
        LookBoneRotation.PitchWeight = PitchWeights[Index];

        LookBoneRotations.Add(LookBoneRotation);
    }
}


bool UWomenNativeAnimInstance::HasSkeletonBone(FName BoneName) const
{
    const USkeletalMeshComponent* MeshComponent = GetSkelMeshComponent();
    return MeshComponent && MeshComponent->GetBoneIndex(BoneName) != INDEX_NONE;
}

void UWomenNativeAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
    Super::NativeUpdateAnimation(DeltaTime);
    APawn* Pawn = TryGetPawnOwner();
    if (!Pawn) return;
    Speed = Pawn->GetVelocity().Size2D();

    UpdateOneShotActionLock(DeltaTime);
    UpdateNativeLookBoneRotations(DeltaTime);
    UpdateLocomotionState(DeltaTime);
    UpdateHoldTypeFromOwner();
    HandleHoldTypeChanged();
    HandleThrowMontageState();
}

void UWomenNativeAnimInstance::UpdateOneShotActionLock(float DeltaTime)
{
    if (OneShotActionLockTimeRemaining > 0.f)
    {
        OneShotActionLockTimeRemaining = FMath::Max(0.f, OneShotActionLockTimeRemaining - DeltaTime);
    }

    if (OneShotActionLockTimeRemaining <= 0.f)
    {
        bOneShotActionLocked = false;
    }
}

void UWomenNativeAnimInstance::UpdateNativeLookBoneRotations(float DeltaTime)
{
    const USkeletalMeshComponent* MeshComponent = GetSkelMeshComponent();
    if (!MeshComponent)
    {
        return;
    }
    
    if (!bEnableNativeLookBoneCalculation)
    {
        Spine2LookRotation = FRotator::ZeroRotator;
        NeckLookRotation = FRotator::ZeroRotator;
        HeadLookRotation = FRotator::ZeroRotator;
        return;
    }

    if (LookBoneRotations.Num() == 0)
    {
        InitializeDefaultLookBoneRotations();
    }

    const FRotator TargetLookRotation = CalculateControlRotationDelta();
    if (LookRotationInterpSpeed <= 0.f)
    {
        SmoothedLookRotation = TargetLookRotation;
    }
    else
    {
        SmoothedLookRotation = FMath::RInterpTo(
            SmoothedLookRotation,
            TargetLookRotation,
            DeltaTime,
            LookRotationInterpSpeed);
    }

    Pitch = SmoothedLookRotation.Pitch;
    Yaw = SmoothedLookRotation.Yaw;
    Roll = SmoothedLookRotation.Roll;

    Spine2LookRotation = FRotator(0.f, 0.f, SmoothedLookRotation.Pitch * -1.0f);
    NeckLookRotation = FRotator(0.f, 0.f, SmoothedLookRotation.Pitch * -0.15f);
    HeadLookRotation = FRotator(0.f, 0.f, SmoothedLookRotation.Pitch * -0.1f);

    for (FNativeLookBoneRotation& LookBoneRotation : LookBoneRotations)
    {
        LookBoneRotation.ComputedRotation = FRotator{
            0.f,
            SmoothedLookRotation.Yaw * LookBoneRotation.YawWeight,
            SmoothedLookRotation.Pitch * -LookBoneRotation.PitchWeight
            + SmoothedLookRotation.Roll * LookBoneRotation.RollWeight
        };
    }
}


FRotator UWomenNativeAnimInstance::CalculateControlRotationDelta() const
{
    const APawn* PawnOwner = TryGetPawnOwner();
    if (!PawnOwner)
    {
        return FRotator::ZeroRotator;
    }

    const FRotator ControlRotation = PawnOwner->GetControlRotation();
    const FRotator ActorRotation = PawnOwner->GetActorRotation();
    FRotator DeltaRotation = UKismetMathLibrary::NormalizedDeltaRotator(ControlRotation, ActorRotation);

    DeltaRotation.Pitch = FMath::Clamp(DeltaRotation.Pitch, MaxLookDownPitch, MaxLookUpPitch);
    DeltaRotation.Yaw = FMath::Clamp(DeltaRotation.Yaw, -MaxLookYaw, MaxLookYaw);
    DeltaRotation.Roll = 0.f;

    return DeltaRotation;
}

void UWomenNativeAnimInstance::UpdateLocomotionState(float DeltaTime)
{
    // 动作锁倒计时。
    // 例如 JumpStart 设置 MustFinish 后，会在这里等待它播放完。
    // 期间即使 ResolveLocomotionState() 检测到已经应该进入 FallLoop，也不会立刻切换。
    if (AnimationLockTimeRemaining > 0.f)
    {
        AnimationLockTimeRemaining = FMath::Max(0.f, AnimationLockTimeRemaining - DeltaTime);
    }

    if (AnimationLockTimeRemaining <= 0.f)
    {
        bAnimationTransitionLocked = false;
    }

    if (bIsInAir && !bWasInAir)
    {
        JumpStartTimeRemaining = JumpStartDuration;
    }

    // 检测蹲下状态变化：
    // false -> true: 先进入 CrouchEnter 过渡动作。
    // true -> false: 先进入 CrouchExit 过渡动作。
    // 这两个过渡动作默认进入 CrouchIdle/Idle 等结果状态。
    if (bIsSquat && !bWasSquat)
    {
        bPendingCrouchEnter = HasPlayableLocomotionAnimation(EWomenNativeLocomotionState::CrouchEnter);
        bPendingCrouchExit = false;
    }
    else if (!bIsSquat && bWasSquat)
    {
        bPendingCrouchExit = HasPlayableLocomotionAnimation(EWomenNativeLocomotionState::CrouchExit);
        bPendingCrouchEnter = false;
    }

    if (JumpStartTimeRemaining > 0.f)
    {
        JumpStartTimeRemaining = FMath::Max(0.f, JumpStartTimeRemaining - DeltaTime);
    }

    CurrentLocomotionState = ResolveLocomotionState();
    HandleLocomotionStateChanged();

    bWasInAir = bIsInAir;
    bWasSquat = bIsSquat;
}

EWomenNativeLocomotionState UWomenNativeAnimInstance::ResolveLocomotionState() const
{
    if (bIsInAir)
    {
        return JumpStartTimeRemaining > 0.f
            ? EWomenNativeLocomotionState::JumpStart
            : EWomenNativeLocomotionState::FallLoop;
    }

    if (bPendingCrouchEnter)
    {
        return EWomenNativeLocomotionState::CrouchEnter;
    }

    if (bPendingCrouchExit)
    {
        return EWomenNativeLocomotionState::CrouchExit;
    }

    const bool bMoving = Speed > MoveSpeedThreshold;

    if (bIsSquat)
    {
        if (bMoving && HasPlayableLocomotionAnimation(EWomenNativeLocomotionState::CrouchWalk))
        {
            return EWomenNativeLocomotionState::CrouchWalk;
        }

        if (!bMoving && HasPlayableLocomotionAnimation(EWomenNativeLocomotionState::CrouchIdle))
        {
            return EWomenNativeLocomotionState::CrouchIdle;
        }
    }

    if (!bMoving)
    {
        return EWomenNativeLocomotionState::Idle;
    }

    return bIsSprint
        ? EWomenNativeLocomotionState::Run
        : EWomenNativeLocomotionState::Walk;
}

void UWomenNativeAnimInstance::HandleLocomotionStateChanged()
{
    if (CurrentLocomotionState == PreviousLocomotionState)
    {
        return;
    }

    if (IsFirstPersonAnimationInstance() && (bIsStandThrowing || bIsSquatThrowing))
    {
        return;
    }

    // 这里就是“动作能不能被打断”的核心。
    // 例如：
    // - Walk / Run 是 Interruptible，任何时候都能被 Idle, Jump, Crouch 打断。
    // - JumpStart 是 MustFinish，在锁时间结束前，不允许 FallLoop/Walk/Run 抢走播放权。
    if (!CanInterruptCurrentLocomotion(CurrentLocomotionState))
    {
        return;
    }

    PlayLocomotionAnimation(CurrentLocomotionState);
    PreviousLocomotionState = CurrentLocomotionState;
}

const FNativeLocomotionAnimationSet* UWomenNativeAnimInstance::GetAnimationSetForLocomotionState(EWomenNativeLocomotionState LocomotionState) const
{
    switch (LocomotionState)
    {
        case EWomenNativeLocomotionState::Idle: return &IdleAnimation;
        case EWomenNativeLocomotionState::Walk: return &WalkAnimation;
        case EWomenNativeLocomotionState::Run: return &RunAnimation;
        case EWomenNativeLocomotionState::JumpStart: return &JumpStartAnimation;
        case EWomenNativeLocomotionState::FallLoop: return &FallLoopAnimation;
        case EWomenNativeLocomotionState::CrouchEnter: return &CrouchEnterAnimation;
        case EWomenNativeLocomotionState::CrouchExit: return &CrouchExitAnimation;
        case EWomenNativeLocomotionState::CrouchIdle: return &CrouchIdleAnimation;
        case EWomenNativeLocomotionState::CrouchWalk: return &CrouchWalkAnimation;
        default: return nullptr;
    }
}

bool UWomenNativeAnimInstance::HasPlayableLocomotionAnimation(EWomenNativeLocomotionState LocomotionState) const
{
    if (GetHeldLocomotionAnimation(LocomotionState))
    {
        return true;
    }

    const FNativeLocomotionAnimationSet* AnimationSet = GetAnimationSetForLocomotionState(LocomotionState);
    return AnimationSet && AnimationSet->Animation;
}

bool UWomenNativeAnimInstance::PlayLocomotionAnimation(EWomenNativeLocomotionState LocomotionState)
{
    const FNativeLocomotionAnimationSet* AnimationSet = GetAnimationSetForLocomotionState(LocomotionState);

    UAnimSequenceBase* Animation = GetHeldLocomotionAnimation(LocomotionState);
    if (!Animation)
    {
        Animation = AnimationSet ? AnimationSet->Animation : nullptr;
    }
    if (!Animation || LocomotionSlotName.IsNone())
    {
        return false;
    }

    const bool bLoop = LocomotionState != EWomenNativeLocomotionState::JumpStart
        && LocomotionState != EWomenNativeLocomotionState::CrouchEnter
        && LocomotionState != EWomenNativeLocomotionState::CrouchExit;
    PlaySlotAnimationAsDynamicMontage(
        Animation,
        LocomotionSlotName,
        LocomotionBlendInTime,
        LocomotionBlendOutTime,
        1.f,
        bLoop ? MAX_int32 : 1);

    StartAnimationLockIfNeeded(*AnimationSet);

    if (LocomotionState == EWomenNativeLocomotionState::CrouchEnter)
    {
        bPendingCrouchEnter = false;
    }
    else if (LocomotionState == EWomenNativeLocomotionState::CrouchExit)
    {
        bPendingCrouchExit = false;
    }

    return true;
}

bool UWomenNativeAnimInstance::CanInterruptCurrentLocomotion(EWomenNativeLocomotionState RequestedState) const
{
    if (!bAnimationTransitionLocked)
    {
        return true;
    }

    // 这里留一个“强制优先级”入口。
    // 如果后面有受击、死亡、被抓等更高优先级动作，可以在这里允许它们无视锁直接打断。
    // 现在的基础移动状态里，没有比 JumpStart 更高优先级的状态，所以锁住时直接拒绝绝切换。
    return false;
}

void UWomenNativeAnimInstance::StartAnimationLockIfNeeded(const FNativeLocomotionAnimationSet& AnimationSet)
{
    if (AnimationSet.InterruptPolicy != EWomenNativeAnimationInterruptPolicy::MustFinish)
    {
        bAnimationTransitionLocked = false;
        AnimationLockTimeRemaining = 0.f;
        return;
    }

    float LockDuration = AnimationSet.LockDuration;
    if (LockDuration <= 0.f && AnimationSet.Animation)
    {
        LockDuration = AnimationSet.Animation->GetPlayLength();
    }

    bAnimationTransitionLocked = LockDuration > 0.f;
    AnimationLockTimeRemaining = FMath::Max(0.f, LockDuration);
}

void UWomenNativeAnimInstance::UpdateHoldTypeFromOwner()
{
    EHoldItemType NewHoldType = EHoldItemType::None;
    APickupActor* NewHeldActor = nullptr;

    APawn* PawnOwner = TryGetPawnOwner();
    if (!PawnOwner)
    {
        CurrentHoldType = NewHoldType;
        CurrentHeldActor = nullptr;
        return;
    }

    if (AMyPlayerController* PlayerController = Cast<AMyPlayerController>(PawnOwner->GetController()))
    {
        if (APickupActor* HeldActor = PlayerController->GetHeldActor())
        {
            NewHoldType = HeldActor->GetHoldType();
            NewHeldActor = HeldActor;
        }
    }

    if (NewHoldType == EHoldItemType::None)
    {
        TArray<AActor*> AttachedActors;
        PawnOwner->GetAttachedActors(AttachedActors);

        for (AActor* AttachedActor : AttachedActors)
        {
            APickupActor* PickupActor = Cast<APickupActor>(AttachedActor);
            if (PickupActor && PickupActor->IsHeldByPlayer())
            {
                NewHoldType = PickupActor->GetHoldType();
                NewHeldActor = PickupActor;
                break;
            }
        }
    }

    CurrentHoldType = NewHoldType;
    CurrentHeldActor = NewHeldActor;
}

const FPickupHeldAnimationSet* UWomenNativeAnimInstance::GetCurrentHeldAnimationSet() const
{
    return CurrentHeldActor 
        ? &CurrentHeldActor->GetHeldAnimationSetForView(IsFirstPersonAnimationInstance()) 
        : nullptr;
}

bool UWomenNativeAnimInstance::IsFirstPersonAnimationInstance() const
{
    const AWomenCharacter* WomenCharacter = Cast<AWomenCharacter>(TryGetPawnOwner());
    const USkeletalMeshComponent* MeshComponent = GetSkelMeshComponent();
    
    return WomenCharacter && MeshComponent && MeshComponent == WomenCharacter->FirstPersonMesh;
}


UAnimSequenceBase* UWomenNativeAnimInstance::GetHeldLocomotionAnimation(EWomenNativeLocomotionState LocomotionState) const
{
    const FPickupHeldAnimationSet* HeldAnimationSet = GetCurrentHeldAnimationSet();
    if (!HeldAnimationSet)
    {
        return nullptr;
    }

    switch (LocomotionState)
    {
        case EWomenNativeLocomotionState::Idle:
            return HeldAnimationSet->IdlePose.Get();
        case EWomenNativeLocomotionState::Walk:
            return HeldAnimationSet->WalkAnimation.Get();
        case EWomenNativeLocomotionState::Run:
            return HeldAnimationSet->RunAnimation.Get();
        case EWomenNativeLocomotionState::CrouchIdle:
            return HeldAnimationSet->CrouchIdleAnimation.Get();
        case EWomenNativeLocomotionState::CrouchWalk:
            return HeldAnimationSet->CrouchWalkAnimation.Get();
        default:
            return nullptr;
    }
}

UAnimSequenceBase* UWomenNativeAnimInstance::GetHeldThrowUpperBodyAnimation(bool bSquatThrow, bool bRunThrow) const
{
    const FPickupHeldAnimationSet* HeldAnimationSet = GetCurrentHeldAnimationSet();
    if (!HeldAnimationSet)
    {
        return nullptr;
    }

    if (bSquatThrow)
    {
        return HeldAnimationSet->SquatThrowUpperBodyAnimation.Get();
    }

    return bRunThrow
        ? HeldAnimationSet->RunThrowUpperBodyAnimation.Get()
        : HeldAnimationSet->StandThrowUpperBodyAnimation.Get();
}

UAnimMontage* UWomenNativeAnimInstance::GetHeldThrowMontage(bool bSquatThrow, bool bRunThrow) const
{
    const FPickupHeldAnimationSet* HeldAnimationSet = GetCurrentHeldAnimationSet();
    if (!HeldAnimationSet)
    {
        return nullptr;
    }

    if (bSquatThrow)
    {
        return HeldAnimationSet->SquatThrowMontage.Get();
    }

    return bRunThrow
        ? HeldAnimationSet->RunThrowMontage.Get()
        : HeldAnimationSet->StandThrowMontage.Get();
}

void UWomenNativeAnimInstance::HandleHoldTypeChanged()
{
    if (CurrentHoldType == PreviousHoldType && CurrentHeldActor == PreviousHeldActor)
    {
        return;
    }

    const FPickupHeldAnimationSet* HeldAnimationSet = GetCurrentHeldAnimationSet();
    const FNativeHoldAnimationSet* AnimationSet = HoldAnimations.Find(CurrentHoldType);
    CurrentIdlePose = HeldAnimationSet && HeldAnimationSet->IdlePose
                      ? HeldAnimationSet->IdlePose.Get()
                      : AnimationSet && AnimationSet->IdlePose ? AnimationSet->IdlePose.Get() : nullptr;

    if (CurrentHeldActor || CurrentHoldType != EHoldItemType::None)
    {
        PlayEquipMontageForHoldType(CurrentHoldType);
    }

    if (CanInterruptCurrentLocomotion(CurrentLocomotionState))
    {
        PlayLocomotionAnimation(CurrentLocomotionState);
        PreviousLocomotionState = CurrentLocomotionState;
    }

    PreviousHoldType = CurrentHoldType;
    PreviousHeldActor = CurrentHeldActor;
}

void UWomenNativeAnimInstance::HandleThrowMontageState()
{
    // 投掷动作是“动作层”，不是“移动层”。
    // 这里不会停止 Walk/Run。Walk/Run 继续由 LocomotionSlotName 播放。
    // 投掷只播放到 UpperBodySlotName，然后由动画蓝图的 Layered Blend Per Bone 合并到上半身/右手。
    if (bIsStandThrowing && !bWasStandThrowing)
    {
        PlayThrowAction(false);
    }

    if (bIsSquatThrowing && !bWasSquatThrowing)
    {
        PlayThrowAction(true);
    }

    if ((!bIsStandThrowing && bWasStandThrowing) || (!bIsSquatThrowing && bWasSquatThrowing))
    {
        StopThrowAction();
    }
    
    bWasStandThrowing = bIsStandThrowing;
    bWasSquatThrowing = bIsSquatThrowing;
}

bool UWomenNativeAnimInstance::PlayThrowAction(bool bSquatThrow)
{
    if (!CanPlayOneShotAction())
    {
        return false;
    }

    StopThrowAction();

    const bool bRunThrow = !bSquatThrow && IsRunningThrow();
    const FPickupHeldAnimationSet* HeldAnimationSet = GetCurrentHeldAnimationSet();
    const bool bFallbackToDefaultThrowAnimation = !HeldAnimationSet || HeldAnimationSet->bFallbackToDefaultThrowAnimationWhenUnset;

    UAnimSequenceBase* UpperBodyAnimation = GetHeldThrowUpperBodyAnimation(bSquatThrow, bRunThrow);
    if (bFallbackToDefaultThrowAnimation && bSquatThrow)
    {
        UpperBodyAnimation = UpperBodyAnimation ? UpperBodyAnimation : SquatThrowUpperBodyAnimation.Get();
    }
    else if (bFallbackToDefaultThrowAnimation && bRunThrow)
    {
        UpperBodyAnimation = UpperBodyAnimation ? UpperBodyAnimation : RunThrowUpperBodyAnimation.Get();
    }
    else if (bFallbackToDefaultThrowAnimation)
    {
        UpperBodyAnimation = UpperBodyAnimation ? UpperBodyAnimation : StandThrowUpperBodyAnimation.Get();
    }

    if (UpperBodyAnimation)
    {
        const bool bCanUseFullBodyThrowSlot = bUseFullBodySlotForThrow || IsFirstPersonAnimationInstance();
        if (bCanUseFullBodyThrowSlot && !LocomotionSlotName.IsNone())
        {
            ActiveThrowDynamicMontage = PlaySlotAnimationAsDynamicMontage(
                UpperBodyAnimation,
                LocomotionSlotName,
                LocomotionBlendInTime,
                LocomotionBlendOutTime,
                1.f,
                1);

            bThrowUsesLoopingUpperBodySequence = ActiveThrowDynamicMontage != nullptr;
            bThrowUsesFullBodySlot = ActiveThrowDynamicMontage != nullptr;
            if (bThrowUsesLoopingUpperBodySequence)
            {
                ActiveThrowDynamicMontage->bEnableAutoBlendOut = false;
                bAnimationTransitionLocked = true;
                AnimationLockTimeRemaining = 9999.f;
                bOneShotActionLocked = true;
                OneShotActionLockTimeRemaining = 9999.f;
                return true;
            }
        }


        if (bLoopThrowUpperBodySequenceUntilThrowEnds && !UpperBodySlotName.IsNone())
        {
            ActiveThrowDynamicMontage = PlaySlotAnimationAsDynamicMontage(
                UpperBodyAnimation,
                UpperBodySlotName,
                UpperBodyBlendInTime,
                UpperBodyBlendOutTime,
                1.f,
                1);

            bThrowUsesLoopingUpperBodySequence = ActiveThrowDynamicMontage != nullptr;
            bThrowUsesFullBodySlot = false;
            if (bThrowUsesLoopingUpperBodySequence)
            {
                ActiveThrowDynamicMontage->bEnableAutoBlendOut = false; // 关键：投掷结束时手动停止，禁止自动淡出。
                bOneShotActionLocked = true;
                OneShotActionLockTimeRemaining = 9999.f;
                return true;
            }
        }

        return PlayOneShotAction(UpperBodyAnimation, nullptr);
    }

    UAnimMontage* ThrowMontage = GetHeldThrowMontage(bSquatThrow, bRunThrow);
    if (bFallbackToDefaultThrowAnimation && bSquatThrow)
    {
        ThrowMontage = ThrowMontage ? ThrowMontage : SquatThrowMontage.Get();
    }
    else if (bFallbackToDefaultThrowAnimation && bRunThrow)
    {
        ThrowMontage = ThrowMontage ? ThrowMontage : RunThrowMontage.Get();
    }
    else if (bFallbackToDefaultThrowAnimation)
    {
        ThrowMontage = ThrowMontage ? ThrowMontage : StandThrowMontage.Get();
    }

    if (!ThrowMontage)
    {
        return false;
    }

    return PlayOneShotAction(nullptr, ThrowMontage);
}

void UWomenNativeAnimInstance::StopThrowAction()
{
    if (!bThrowUsesLoopingUpperBodySequence)
    {
        return;
    }

    if (ActiveThrowDynamicMontage)
    {
        Montage_Stop(bThrowUsesFullBodySlot ? LocomotionBlendOutTime : UpperBodyBlendOutTime, ActiveThrowDynamicMontage);
        ActiveThrowDynamicMontage = nullptr;
    }

    bThrowUsesLoopingUpperBodySequence = false;
    const bool bWasFullBodyThrow = bThrowUsesFullBodySlot;
    bThrowUsesFullBodySlot = false;
    bOneShotActionLocked = false;
    OneShotActionLockTimeRemaining = 0.f;

    if (bWasFullBodyThrow)
    {
        bAnimationTransitionLocked = false;
        AnimationLockTimeRemaining = 0.f;
        PlayLocomotionAnimation(CurrentLocomotionState);
    }
}

bool UWomenNativeAnimInstance::IsRunningThrow() const
{
    return CurrentLocomotionState == EWomenNativeLocomotionState::Run
        || (bIsSprint && Speed > MoveSpeedThreshold);
}

bool UWomenNativeAnimInstance::PlayUpperBodyAnimation(UAnimSequenceBase* Animation, bool bLoop)
{
    if (!Animation || UpperBodySlotName.IsNone())
    {
        return false;
    }

    PlaySlotAnimationAsDynamicMontage(
        Animation,
        UpperBodySlotName,
        UpperBodyBlendInTime,
        UpperBodyBlendOutTime,
        1.f,
        bLoop ? MAX_int32 : 1);

    return true;
}

bool UWomenNativeAnimInstance::PlayOpenDoorAction()
{
    const bool bCrouchAction = IsCrouchAction();
    UAnimSequenceBase* UpperBodyAnimation = bCrouchAction
        ? SquatOpenDoorUpperBodyAnimation.Get()
        : StandOpenDoorUpperBodyAnimation.Get();
    UAnimMontage* Montage = bCrouchAction
        ? SquatOpenDoorMontage.Get()
        : StandOpenDoorMontage.Get();

    return PlayOneShotAction(UpperBodyAnimation, Montage);
}

bool UWomenNativeAnimInstance::PlayHitAction()
{
    const bool bCrouchAction = IsCrouchAction();
    UAnimSequenceBase* UpperBodyAnimation = bCrouchAction 
        ? SquatHitUpperBodyAnimation.Get() 
        : StandHitUpperBodyAnimation.Get();
    UAnimMontage* Montage = bCrouchAction 
        ? SquatHitMontage.Get() 
        : StandHitMontage.Get();

    return PlayOneShotAction(UpperBodyAnimation, Montage);
}

bool UWomenNativeAnimInstance::IsCrouchAction() const
{
    return bIsSquat
        || CurrentLocomotionState == EWomenNativeLocomotionState::CrouchEnter
        || CurrentLocomotionState == EWomenNativeLocomotionState::CrouchIdle
        || CurrentLocomotionState == EWomenNativeLocomotionState::CrouchWalk;
}

bool UWomenNativeAnimInstance::CanPlayOneShotAction() const
{
    return !bOneShotActionLocked;
}

bool UWomenNativeAnimInstance::PlayOneShotAction(UAnimSequenceBase* UpperBodyAnimation, UAnimMontage* Montage)
{
    if (!CanPlayOneShotAction())
    {
        return false;
    }

    float LockDuration = 0.f;
    bool bPlayed = false;

    if (UpperBodyAnimation && !UpperBodySlotName.IsNone())
    {
        PlaySlotAnimationAsDynamicMontage(
            UpperBodyAnimation,
            UpperBodySlotName,
            UpperBodyBlendInTime,
            UpperBodyBlendOutTime,
            1.f,
            1
        );

        LockDuration = UpperBodyAnimation->GetPlayLength();
        bPlayed = true;
    }
    else if (Montage)
    {
        // 注意：Montage 自己的 Slot 必须设置成 UpperBody，否则它仍会按 Montage 里的 Slot 播放。
        Montage_Play(Montage);
        LockDuration = Montage->GetPlayLength();
        bPlayed = true;
    }

    if (bPlayed)
    {
        StartOneShotActionLock(LockDuration);
    }

    return bPlayed;
}

void UWomenNativeAnimInstance::StartOneShotActionLock(float LockDuration)
{
    bOneShotActionLocked = LockDuration > 0.f;
    OneShotActionLockTimeRemaining = FMath::Max(0.f, LockDuration);
}

bool UWomenNativeAnimInstance::PlayEquipMontageForHoldType(EHoldItemType HoldItemType)
{
    if (HoldItemType == CurrentHoldType)
    {
        const FPickupHeldAnimationSet* HeldAnimationSet = GetCurrentHeldAnimationSet();
        if (HeldAnimationSet && HeldAnimationSet->EquipMontage)
        {
            Montage_Play(HeldAnimationSet->EquipMontage.Get());
            return true;
        }
    }

    const FNativeHoldAnimationSet* AnimationSet = HoldAnimations.Find(HoldItemType);
    if (!AnimationSet || !AnimationSet->EquipMontage)
    {
        return false;
    }

    Montage_Play(AnimationSet->EquipMontage);
    return true;
}

bool UWomenNativeAnimInstance::PlayUseMontageForHoldType(EHoldItemType HoldItemType)
{
    if (HoldItemType == CurrentHoldType)
    {
        const FPickupHeldAnimationSet* HeldAnimationSet = GetCurrentHeldAnimationSet();
        if (HeldAnimationSet && HeldAnimationSet->UseMontage)
        {
            Montage_Play(HeldAnimationSet->UseMontage.Get());
            return true;
        }
    }

    const FNativeHoldAnimationSet* AnimationSet = HoldAnimations.Find(HoldItemType);
    if (!AnimationSet || !AnimationSet->UseMontage)
    {
        return false;
    }

    Montage_Play(AnimationSet->UseMontage);
    return true;
}