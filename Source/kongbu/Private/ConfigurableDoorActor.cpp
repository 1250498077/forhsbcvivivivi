#include "ConfigurableDoorActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"

AConfigurableDoorActor::AConfigurableDoorActor()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRootComponent"));
    RootComponent = SceneRootComponent;

    StaticMeshesRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("StaticMeshesRootComponent"));
    StaticMeshesRootComponent->SetupAttachment(SceneRootComponent);

    DoorMotionRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("DoorMotionRootComponent"));
    DoorMotionRootComponent->SetupAttachment(SceneRootComponent);
    DoorMotionRootComponent->SetMobility(EComponentMobility::Movable);

    DoorMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMeshComponent"));
    DoorMeshComponent->SetupAttachment(DoorMotionRootComponent);
    DoorMeshComponent->SetMobility(EComponentMobility::Movable);
    DoorMeshComponent->SetCanEverAffectNavigation(false);
}

void AConfigurableDoorActor::BeginPlay()
{
    Super::BeginPlay();

    bIsAnimating = false;
    bIsOpen = bStartOpened;
    ApplyDoorStateInstant(bStartOpened);
}

void AConfigurableDoorActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (GetWorld() && GetWorld()->IsGameWorld())
    {
        ApplyDoorStateInstant(bStartOpened);
        return;
    }

    ApplyDoorStateInstant(bStartOpened);
}

void AConfigurableDoorActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bIsAnimating || !DoorMotionRootComponent)
    {
        return;
    }

    CurrentAnimationTime += DeltaSeconds;

    const float NormalizedAlpha = TransitionDuration <= KINDA_SMALL_NUMBER
                                      ? 1.f
                                      : FMath::Clamp(CurrentAnimationTime / TransitionDuration, 0.f, 1.f);

    const float EvaluatedAlpha = EvaluateAnimationAlpha(NormalizedAlpha);
    const FVector NewLocation = FMath::Lerp(AnimationStartLocation, AnimationTargetLocation, EvaluatedAlpha);
    const FQuat NewRotation = FQuat::Slerp(AnimationStartRotation, AnimationTargetRotation, EvaluatedAlpha);

    DoorMotionRootComponent->SetRelativeLocationAndRotation(NewLocation, NewRotation.Rotator());

    if (NormalizedAlpha >= 1.f)
    {
        bIsAnimating = false;
        bIsOpen = bAnimationTargetOpen;
        ApplyDoorStateInstant(bIsOpen);
    }
}

void AConfigurableDoorActor::ApplyDoorStateInstant(bool bOpenState)
{
    if (!DoorMotionRootComponent)
    {
        return;
    }

    const FTransform TargetTransform = bOpenState ? GetOpenedDoorTransform() : GetClosedDoorTransform();
    DoorMotionRootComponent->SetRelativeLocationAndRotation(
        TargetTransform.GetLocation(),
        TargetTransform.GetRotation().Rotator());
}

void AConfigurableDoorActor::StartDoorAnimation(bool bOpenState)
{
    if (!DoorMotionRootComponent)
    {
        return;
    }

    if (TransitionDuration <= KINDA_SMALL_NUMBER)
    {
        bIsAnimating = false;
        bIsOpen = bOpenState;
        ApplyDoorStateInstant(bOpenState);
        return;
    }

    const FTransform TargetTransform = bOpenState ? GetOpenedDoorTransform() : GetClosedDoorTransform();

    bAnimationTargetOpen = bOpenState;
    bIsAnimating = true;
    CurrentAnimationTime = 0.f;
    AnimationStartLocation = DoorMotionRootComponent->GetRelativeLocation();
    AnimationStartRotation = DoorMotionRootComponent->GetRelativeRotation().Quaternion();
    AnimationTargetLocation = TargetTransform.GetLocation();
    AnimationTargetRotation = TargetTransform.GetRotation();
}

float AConfigurableDoorActor::EvaluateAnimationAlpha(float NormalizedAlpha) const
{
    const float ClampedAlpha = FMath::Clamp(NormalizedAlpha, 0.f, 1.f);

    if (!TransitionCurve)
    {
        return ClampedAlpha;
    }

    return FMath::Clamp(TransitionCurve->GetFloatValue(ClampedAlpha), 0.f, 1.f);
}

FTransform AConfigurableDoorActor::GetClosedDoorTransform() const
{
    return FTransform(ClosedRelativeRotation, ClosedRelativeLocation);
}

FTransform AConfigurableDoorActor::GetOpenedDoorTransform() const
{
    return FTransform(OpenRelativeRotation, OpenRelativeLocation);
}

void AConfigurableDoorActor::OpenDoor()
{
    SetDoorOpenState(true, false);
}

void AConfigurableDoorActor::CloseDoor()
{
    SetDoorOpenState(false, false);
}

void AConfigurableDoorActor::ToggleDoor()
{
    const bool bNextOpenState = bIsAnimating ? !bAnimationTargetOpen : !bIsOpen;
    SetDoorOpenState(bNextOpenState, false);
}

void AConfigurableDoorActor::SetDoorOpenState(bool bNewOpenState, bool bInstant)
{
    bAnimationTargetOpen = bNewOpenState;

    if (bInstant)
    {
        bIsAnimating = false;
        bIsOpen = bNewOpenState;
        ApplyDoorStateInstant(bNewOpenState);
        return;
    }

    StartDoorAnimation(bNewOpenState);
}

void AConfigurableDoorActor::CaptureCurrentAsClosedState()
{
    if (!DoorMotionRootComponent)
    {
        return;
    }

    ClosedRelativeLocation = DoorMotionRootComponent->GetRelativeLocation();
    ClosedRelativeRotation = DoorMotionRootComponent->GetRelativeRotation();
}

void AConfigurableDoorActor::CaptureCurrentAsOpenState()
{
    if (!DoorMotionRootComponent)
    {
        return;
    }

    OpenRelativeLocation = DoorMotionRootComponent->GetRelativeLocation();
    OpenRelativeRotation = DoorMotionRootComponent->GetRelativeRotation();
}

void AConfigurableDoorActor::PreviewClosedState()
{
    ApplyDoorStateInstant(false);
}

void AConfigurableDoorActor::PreviewOpenState()
{
    ApplyDoorStateInstant(true);
}