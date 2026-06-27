#include "ConfigurableDoorActor.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "DrawDebugHelpers.h"

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
    DoorBlockerComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorBlockerComponent"));
    DoorBlockerComponent->SetupAttachment(DoorMotionRootComponent);
    DoorBlockerComponent->SetBoxExtent(DoorBlockerExtent);
    ConfigureDoorMeshBlocking();



    InteractionRangeComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionRangeComponent"));
    InteractionRangeComponent->SetupAttachment(SceneRootComponent);
    InteractionRangeComponent->SetBoxExtent(InteractionBoxExtent);
    InteractionRangeComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionRangeComponent->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionRangeComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionRangeComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    InteractionRangeComponent->SetGenerateOverlapEvents(true);
    InteractionRangeComponent->SetHiddenInGame(false);
    InteractionRangeComponent->ShapeColor = FColor::Cyan;

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

    ConfigureDoorMeshBlocking();
    bIsAnimating = false;
    bIsOpen = bStartOpened;
    ApplyDoorStateInstant(bStartOpened);
}

void AConfigurableDoorActor::OnConstruction(const FTransform &Transform)
{
    Super::OnConstruction(Transform);

    ConfigureDoorMeshBlocking();

    if (DoorBlockerComponent)
    {
        DoorBlockerComponent->SetBoxExtent(DoorBlockerExtent.ComponentMax(FVector::ZeroVector));
    }

    if (InteractionRangeComponent)
    {
        InteractionRangeComponent->SetBoxExtent(InteractionBoxExtent.ComponentMax(FVector::ZeroVector));
        InteractionRangeComponent->SetHiddenInGame(!bShowInteractionRangeDebug);
    }

    if (GetWorld() && GetWorld()->IsGameWorld())
    {
        ApplyDoorStateInstant(bStartOpened);
        return;
    }
#if WITH_EDITOR
    if (bEnableEditorPreview)
    {
        ApplyDoorStateInstant(EditorPreviewState == EDoorEditorPreviewState::Open);
    }
#endif
}

void AConfigurableDoorActor::ConfigureDoorMeshBlocking()
{
    if (DoorMeshComponent)
    {
        DoorMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        DoorMeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
        DoorMeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
        DoorMeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        DoorMeshComponent->SetGenerateOverlapEvents(false);
        DoorMeshComponent->SetCanEverAffectNavigation(true);
    }

    if (DoorBlockerComponent)
    {
        DoorBlockerComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        DoorBlockerComponent->SetCollisionObjectType(ECC_WorldDynamic);
        DoorBlockerComponent->SetCollisionResponseToAllChannels(ECR_Block);
        DoorBlockerComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        DoorBlockerComponent->SetGenerateOverlapEvents(false);
        DoorBlockerComponent->SetCanEverAffectNavigation(true);
        DoorBlockerComponent->SetHiddenInGame(true);
    }
}

void AConfigurableDoorActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bShowInteractionRangeDebug && InteractionRangeComponent && GetWorld())
    {
        DrawDebugBox(
            GetWorld(),
            InteractionRangeComponent->GetComponentLocation(),
            InteractionRangeComponent->GetScaledBoxExtent(),
            InteractionRangeComponent->GetComponentQuat(),
            FColor::Cyan,
            false,
            0.f,
            0,
            1.f);
    }

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
    if (bIsLockedClosed)
    {
        return;
    }
    SetDoorOpenState(true, false);
}

void AConfigurableDoorActor::CloseDoor()
{
    if (bIsLockedClosed)
    {
        return;
    }
    SetDoorOpenState(false, false);
}

void AConfigurableDoorActor::ToggleDoor()
{
    const bool bNextOpenState = bIsAnimating ? !bAnimationTargetOpen : !bIsOpen;
    SetDoorOpenState(bNextOpenState, false);
}

void AConfigurableDoorActor::SetDoorOpenState(bool bNewOpenState, bool bInstant)
{
    if (bIsLockedClosed && bNewOpenState)
    {
        return;
    }
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

void AConfigurableDoorActor::LockDoorClosedForDuration(float Duration)
{
    if (Duration <= 0.f)
    {
        return;
    }
    bIsLockedClosed = true;
    SetDoorOpenState(false, false);
    GetWorldTimerManager().ClearTimer(DoorLockTimerHandle);
    GetWorldTimerManager().SetTimer(
        DoorLockTimerHandle,
        this,
        &AConfigurableDoorActor::UnlockDoor,
        Duration,
        false);
}
void AConfigurableDoorActor::UnlockDoor()
{
    bIsLockedClosed = false;
    GetWorldTimerManager().ClearTimer(DoorLockTimerHandle);
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
        DoorMotionRootComponent->GetRelativeRotation());

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
        DoorMotionRootComponent->GetRelativeRotation());

#if WITH_EDITOR
    MarkPackageDirty();
#endif
}

void AConfigurableDoorActor::PreviewClosedState()
{
#if WITH_EDITOR
    Modify();
    EditorPreviewState = EDoorEditorPreviewState::Closed;
    MarkPackageDirty();
#endif
    ApplyDoorStateInstant(false);
}

void AConfigurableDoorActor::PreviewOpenState()
{
#if WITH_EDITOR
    Modify();
    EditorPreviewState = EDoorEditorPreviewState::Open;
    MarkPackageDirty();
#endif
    ApplyDoorStateInstant(true);
}

bool AConfigurableDoorActor::IsActorInInteractionRange(const AActor *InteractingActor) const
{
    if (!IsValid(InteractingActor) || !InteractionRangeComponent)
    {
        return false;
    }

    const FVector LocalActorLocation = InteractionRangeComponent->GetComponentTransform().InverseTransformPosition(InteractingActor->GetActorLocation());
    const FVector BoxExtent = InteractionRangeComponent->GetUnscaledBoxExtent();

    return FMath::Abs(LocalActorLocation.X) <= BoxExtent.X && FMath::Abs(LocalActorLocation.Y) <= BoxExtent.Y && FMath::Abs(LocalActorLocation.Z) <= BoxExtent.Z;
}

bool AConfigurableDoorActor::CanActorInteractFromView(
    const AActor *InteractingActor,
    const FVector &ViewLocation,
    const FVector &ViewDirection) const
{
    if (!IsActorInInteractionRange(InteractingActor))
    {
        return false;
    }

    const FVector DoorTargetLocation = DoorMeshComponent
                                           ? DoorMeshComponent->Bounds.Origin
                                           : GetActorLocation();
    const FVector DirectionToDoor = (DoorTargetLocation - ViewLocation).GetSafeNormal();
    if (DirectionToDoor.IsNearlyZero())
    {
        return true;
    }

    const float FacingDot = FVector::DotProduct(ViewDirection.GetSafeNormal(), DirectionToDoor);
    return FacingDot >= InteractionFacingDotThreshold;
}