#include "ConfigurableDoorActor.h"

#include "Components/ArrowComponent.h"
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

    // 创建并设置“关闭状态”的变换组件 (红色箭头)
    ClosedStateTransformComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("ClosedStateTransformComponent"));
    ClosedStateTransformComponent->SetupAttachment(SceneRootComponent);
    ClosedStateTransformComponent->ArrowColor = FColor::Red;
    ClosedStateTransformComponent->ArrowSize = 1.5f;

    // 创建并设置“开启状态”的变换组件 (绿色箭头)
    OpenStateTransformComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("OpenStateTransformComponent"));
    OpenStateTransformComponent->SetupAttachment(SceneRootComponent);
    OpenStateTransformComponent->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
    OpenStateTransformComponent->ArrowColor = FColor::Green;
    OpenStateTransformComponent->ArrowSize = 1.5f;
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
    }

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
    return ClosedStateTransformComponent 
        ? ClosedStateTransformComponent->GetRelativeTransform() 
        : FTransform::Identity;
}

FTransform AConfigurableDoorActor::GetOpenedDoorTransform() const
{
    return OpenStateTransformComponent 
    ? OpenStateTransformComponent->GetRelativeTransform() 
    : FTransform(FRotator(0.f, 90.f, 0.f));
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
    if (!DoorMotionRootComponent || !ClosedStateTransformComponent)
    {
        return;
    }
#if WITH_EDITOR
    ClosedStateTransformComponent->Modify();
#endif

    ClosedStateTransformComponent->SetRelativeLocationAndRotation(
        DoorMotionRootComponent->GetRelativeLocation(),
        DoorMotionRootComponent->GetRelativeRotation()
    );

#if WITH_EDITOR
    MarkPackageDirty();
#endif
}

void AConfigurableDoorActor::CaptureCurrentAsOpenState()
{
    if (!DoorMotionRootComponent || !OpenStateTransformComponent)
    {
        return;
    }

    #if WITH_EDITOR
        OpenStateTransformComponent->Modify();
    #endif

    OpenStateTransformComponent->SetRelativeLocationAndRotation(
        DoorMotionRootComponent->GetRelativeLocation(),
        DoorMotionRootComponent->GetRelativeRotation()
    );

    #if WITH_EDITOR
        MarkPackageDirty();
    #endif
}

void AConfigurableDoorActor::PreviewClosedState()
{
    ApplyDoorStateInstant(false);
}

void AConfigurableDoorActor::PreviewOpenState()
{
    ApplyDoorStateInstant(true);
}