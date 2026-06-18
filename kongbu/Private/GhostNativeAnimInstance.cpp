#include "GhostNativeAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "GameFramework/Pawn.h"
#include "MyAIController.h"
#include "GhostCharacter.h"

void UGhostNativeAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    Speed = 0.f;
    CurrentAIState = EEnemyAIState::Patrol;
    CurrentLocomotionState = EGhostNativeLocomotionState::Idle;
    PreviousLocomotionState = EGhostNativeLocomotionState::Run;
    bActionLocked = false;
    bIsOpeningDoor = false;
    bIsSoulSucking = false;
    ActionLockTimeRemaining = 0.f;
    ActiveActionDynamicMontage = nullptr;
    bHasPlayedLocomotion = false;
}

void UGhostNativeAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
    Super::NativeUpdateAnimation(DeltaTime);

    AGhostCharacter* GhostOwner = GetGhostOwner();
    if (!GhostOwner)
    {
        return;
    }

    Speed = GhostOwner->GetVelocity().Size2D();
    bIsSoulSucking = GhostOwner->bIsSoulSucking;

    if (const AMyAIController* AIController = Cast<AMyAIController>(GhostOwner->GetController()))
    {
        CurrentAIState = AIController->GetCurrentAIState();
    }

    UpdateActionLock(DeltaTime);

    if (bActionLocked)
    {
        return;
    }

    CurrentLocomotionState = ResolveLocomotionState(GhostOwner);
    HandleLocomotionStateChanged(GhostOwner);
}

bool UGhostNativeAnimInstance::PlayOpenDoorAction()
{
    AGhostCharacter* GhostOwner = GetGhostOwner();
    if (!GhostOwner)
    {
        return false;
    }

    const bool bPlayed = PlayAction(
        GhostOwner->OpenDoorAnimation.Animation.Get(),
        GhostOwner->OpenDoorAnimation.Montage.Get(),
        GhostOwner->ActionSlotName,
        GhostOwner->AnimationBlendInTime,
        GhostOwner->AnimationBlendOutTime,
        GhostOwner->OpenDoorAnimation.LockDuration);

    bIsOpeningDoor = bPlayed;
    bIsSoulSucking = false;
    return bPlayed;
}

bool UGhostNativeAnimInstance::PlaySoulSuckAction()
{
    AGhostCharacter* GhostOwner = GetGhostOwner();
    if (!GhostOwner)
    {
        return false;
    }

    const float LockDuration = GhostOwner->SoulSuckDuration > 0.f
        ? GhostOwner->SoulSuckDuration
        : GhostOwner->SoulSuckAnimation.LockDuration;

    const bool bPlayed = PlayAction(
        GhostOwner->SoulSuckAnimation.Animation.Get(),
        GhostOwner->SoulSuckAnimation.Montage.Get(),
        GhostOwner->ActionSlotName,
        GhostOwner->AnimationBlendInTime,
        GhostOwner->AnimationBlendOutTime,
        LockDuration);

    bIsOpeningDoor = false;
    bIsSoulSucking = bPlayed;
    return bPlayed;
}

bool UGhostNativeAnimInstance::PlayKnockdownAction()
{
    AGhostCharacter* GhostOwner = GetGhostOwner();
    if (!GhostOwner)
    {
        return false;
    }

    Montage_Stop(0.05f);
    bActionLocked = false;
    ActionLockTimeRemaining = 0.f;
    bHasPlayedLocomotion = false;

    const bool bPlayed = PlayAction(
        GhostOwner->KnockdownAnimation.Animation.Get(),
        GhostOwner->KnockdownAnimation.Montage.Get(),
        GhostOwner->ActionSlotName,
        GhostOwner->AnimationBlendInTime,
        GhostOwner->AnimationBlendOutTime,
        GhostOwner->KnockdownAnimation.LockDuration > 0.f
            ? GhostOwner->KnockdownAnimation.LockDuration
            : GetKnockdownActionDuration());

    bIsOpeningDoor = false;
    bIsSoulSucking = false;
    return bPlayed;
}

float UGhostNativeAnimInstance::GetKnockdownActionDuration() const
{
    const AGhostCharacter* GhostOwner = GetGhostOwner();
    if (!GhostOwner)
    {
        return 0.f;
    }

    if (GhostOwner->KnockdownAnimation.Montage)
    {
        return GhostOwner->KnockdownAnimation.Montage->GetPlayLength();
    }

    return GhostOwner->KnockdownAnimation.Animation
        ? GhostOwner->KnockdownAnimation.Animation->GetPlayLength()
        : 0.f;
}

AGhostCharacter* UGhostNativeAnimInstance::GetGhostOwner() const
{
    return Cast<AGhostCharacter>(TryGetPawnOwner());
}

void UGhostNativeAnimInstance::UpdateActionLock(float DeltaTime)
{
    const bool bWasActionLocked = bActionLocked;

    if (ActionLockTimeRemaining > 0.f)
    {
        ActionLockTimeRemaining = FMath::Max(0.f, ActionLockTimeRemaining - DeltaTime);
    }

    if (ActionLockTimeRemaining <= 0.f)
    {
        bActionLocked = false;
        bIsOpeningDoor = false;
        bIsSoulSucking = false;

        if (bWasActionLocked)
        {
            bHasPlayedLocomotion = false;
        }
    }
}

EGhostNativeLocomotionState UGhostNativeAnimInstance::ResolveLocomotionState(const AGhostCharacter* GhostOwner) const
{
    if (!GhostOwner || Speed <= GhostOwner->MoveSpeedThreshold)
    {
        return EGhostNativeLocomotionState::Idle;
    }

    const bool bShouldUseRun = CurrentAIState == EEnemyAIState::Chase
        || CurrentAIState == EEnemyAIState::Flee
        || CurrentAIState == EEnemyAIState::Rage;

    return bShouldUseRun ? EGhostNativeLocomotionState::Run : EGhostNativeLocomotionState::Walk;
}

void UGhostNativeAnimInstance::HandleLocomotionStateChanged(const AGhostCharacter* GhostOwner)
{
    if (bHasPlayedLocomotion && CurrentLocomotionState == PreviousLocomotionState)
    {
        return;
    }

    if (PlayLocomotionAnimation(GhostOwner, CurrentLocomotionState))
    {
        PreviousLocomotionState = CurrentLocomotionState;
        bHasPlayedLocomotion = true;
    }
}

bool UGhostNativeAnimInstance::PlayLocomotionAnimation(const AGhostCharacter* GhostOwner, EGhostNativeLocomotionState LocomotionState)
{
    if (!GhostOwner || GhostOwner->LocomotionSlotName.IsNone())
    {
        return false;
    }

    UAnimSequenceBase* Animation = nullptr;
    switch (LocomotionState)
    {
    case EGhostNativeLocomotionState::Idle:
        Animation = GhostOwner->IdleAnimation.Animation.Get();
        break;
    case EGhostNativeLocomotionState::Walk:
        Animation = GhostOwner->WalkAnimation.Animation.Get();
        break;
    case EGhostNativeLocomotionState::Run:
        Animation = GhostOwner->RunAnimation.Animation.Get();
        break;
    default:
        break;
    }

    if (!Animation)
    {
        return false;
    }

    PlaySlotAnimationAsDynamicMontage(
        Animation,
        GhostOwner->LocomotionSlotName,
        GhostOwner->AnimationBlendInTime,
        GhostOwner->AnimationBlendOutTime,
        1.f,
        MAX_int32);

    return true;
}

bool UGhostNativeAnimInstance::PlayAction(UAnimSequenceBase* Animation, UAnimMontage* Montage, FName SlotName, float BlendInTime, float BlendOutTime, float LockDuration)
{
    if (Montage)
    {
        Montage_Play(Montage, 1.f);
        StartActionLock(LockDuration > 0.f ? LockDuration : Montage->GetPlayLength());
        return true;
    }

    if (!Animation || SlotName.IsNone())
    {
        return false;
    }

    ActiveActionDynamicMontage = PlaySlotAnimationAsDynamicMontage(
        Animation,
        SlotName,
        BlendInTime,
        BlendOutTime,
        1.f,
        1);

    if (!ActiveActionDynamicMontage)
    {
        return false;
    }

    StartActionLock(LockDuration > 0.f ? LockDuration : Animation->GetPlayLength());
    return true;
}

void UGhostNativeAnimInstance::StartActionLock(float LockDuration)
{
    bActionLocked = LockDuration > 0.f;
    ActionLockTimeRemaining = FMath::Max(0.f, LockDuration);
}