#include "PickupActorAAAFlashlight.h"

#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "MyAIController.h"

APickupActorAAAFlashlight::APickupActorAAAFlashlight()
{
    PrimaryActorTick.bCanEverTick = true;


    HoldType = EHoldItemType::Flashlight;
    FP_SocketName = TEXT("RightSocket");
    FP_LocationOffset = FVector::ZeroVector;
    FP_RotationOffset = FRotator::ZeroRotator;
    TP_SocketName = TEXT("RightSocket");
    TP_LocationOffset = FVector::ZeroVector;
    TP_RotationOffset = FRotator::ZeroRotator;

    ItemMassKg = 0.7f;
    ItemThrowForceMultiplier = 0.7f;
    ItemLinearDamping = 0.08f;
    ItemAngularDamping = 0.3f;
    ItemThrowSpinRateDegrees = 900.f;

    bAllowPawnCollision = true;
    bAllowPhysicsBodyCollision = true;
    ApplyReleasedCollisionProfile();

    Tags.Add(FName("Flashlight"));
    Tags.Add(FName("Pickup"));
    Tags.Add(FName("Flash"));

    FlashOriginComponent = CreateDefaultSubobject<USceneComponent>(TEXT("FlashOriginComponent"));
    FlashOriginComponent->SetupAttachment(MeshComponent);
    FlashOriginComponent->SetRelativeLocation(FVector(25.f, 0.f, 8.f));

    FlashLightComponent = CreateDefaultSubobject<USpotLightComponent>(TEXT("FlashLightComponent"));
    FlashLightComponent->SetupAttachment(FlashOriginComponent);
    FlashLightComponent->SetCastShadows(false);
    FlashLightComponent->SetUseInverseSquaredFalloff(false);
    FlashLightComponent->SetVisibility(false);
    FlashLightComponent->SetHiddenInGame(true);
    FlashLightComponent->SetIntensity(0.f);
    FlashLightComponent->SetCanEverAffectNavigation(false);

    SyncFlashLightFromSettings();
    UpdateFlashVisualState();
}

void APickupActorAAAFlashlight::BeginPlay()
{
    Super::BeginPlay();
    SyncFlashLightFromSettings();
    UpdateFlashVisualState();
}

void APickupActorAAAFlashlight::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    SyncFlashLightFromSettings();
    FlashVisibleTimeRemaining = 0.f;
    UpdateFlashVisualState();
}

void APickupActorAAAFlashlight::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (IsHeldByPlayer())
    {
        if (bFlashlightActive || FlashVisibleTimeRemaining > 0.f)
        {
            bPendingActivationWhileHeld = bPendingActivationWhileHeld || !bClosedByPlayer;
            DeactivateFlashlight();
        }

        return;
    }

    if (!bFlashlightActive)
    {
        return;
    }

    FlashPulseAccumulator += DeltaTime;

    if (FlashVisibleTimeRemaining > 0.f)
    {
        FlashVisibleTimeRemaining = FMath::Max(0.f, FlashVisibleTimeRemaining - DeltaTime);
        if (FlashVisibleTimeRemaining <= 0.f)
        {
            UpdateFlashVisualState();
        }
    }

    if (bEnableFlashDebug)
    {
        DrawFlashDebug();
    }

    if (FlashPulseAccumulator >= FlashPulseInterval)
    {
        FlashPulseAccumulator = 0.f;
        EmitFlashPulse();
    }
}

void APickupActorAAAFlashlight::ActivateFlashlight()
{
    if (IsDisabledByRage())
    {
        bPendingActivationWhileHeld = false;
        DeactivateFlashlight();
        return;
    }

    bClosedByPlayer = false;

    if (IsHeldByPlayer())
    {
        bPendingActivationWhileHeld = true;
        DeactivateFlashlight();
        return;
    }

    bPendingActivationWhileHeld = false;
    bFlashlightActive = true;
    FlashPulseAccumulator = 0.f;
    FlashVisibleTimeRemaining = 0.f;

    GetWorldTimerManager().ClearTimer(DelayedFlashActivationHandle);

    EmitFlashPulse();
    UE_LOG(LogTemp, Log, TEXT("Flashlight activated"));
}

void APickupActorAAAFlashlight::DeactivateFlashlight()
{
    bFlashlightActive = false;
    FlashPulseAccumulator = 0.f;
    FlashVisibleTimeRemaining = 0.f;

    GetWorldTimerManager().ClearTimer(DelayedFlashActivationHandle);
    UpdateFlashVisualState();

    UE_LOG(LogTemp, Log, TEXT("Flashlight deactivated"));
}

void APickupActorAAAFlashlight::EmitFlashPulse()
{
    if (!bFlashlightActive || !GetWorld())
    {
        return;
    }

    FlashVisibleTimeRemaining = FMath::Max(0.01f, FlashVisibleDuration);
    UpdateFlashVisualState();

    if (!bAffectAIControllers)
    {
        return;
    }

    FFearStimulus Stimulus;
    Stimulus.SourceLocation = GetFlashOriginLocation();
    Stimulus.Duration = AIReactionDuration;
    Stimulus.StimulusTag = FlashStimulusTag;
    Stimulus.SourceActor = this;
    Stimulus.RageAngerAmount = IsHeldByPlayer()
        ? FMath::Max(1, HeldFearAngerAmount)
        : FMath::Max(1, PlacedFearAngerAmount);

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

        if (!IsActorInsideFlashCone(TargetPawn))
        {
            continue;
        }

        if (bRequireLineOfSightToAffectTargets && !HasLineOfSightToTarget(TargetPawn))
        {
            continue;
        }

        FearController->ApplyFlashlightReveal(FlashVisibleDuration);
        FearController->ApplyFearAngerStimulus(Stimulus);
    }
}

void APickupActorAAAFlashlight::OnPickedUp()
{
    Super::OnPickedUp();

    GetWorldTimerManager().ClearTimer(DelayedFlashActivationHandle);
    bPendingActivationWhileHeld = bPendingActivationWhileHeld || (!bClosedByPlayer && bFlashlightActive);
    FlashPulseAccumulator = 0.f;
    DeactivateFlashlight();
}

void APickupActorAAAFlashlight::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    Super::OnPutDown(PlaceLocation, PlaceRotation);

    if (bClosedByPlayer)
    {
        bPendingActivationWhileHeld = false;
        DeactivateFlashlight();
        UE_LOG(LogTemp, Log, TEXT("%s remains closed after put down"), *GetName());
        return;
    }

    if (bPendingActivationWhileHeld || bAutoActivateFlashlightWhenPlaced)
    {
        ActivateFlashlight();
    }
    else
    {
        DeactivateFlashlight();
    }
}

void APickupActorAAAFlashlight::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    Super::OnThrown(ThrowDirection, ThrowForce);

    if (bClosedByPlayer || !bAutoActivateFlashlightWhenThrown)
    {
        bPendingActivationWhileHeld = false;
        DeactivateFlashlight();
        return;
    }

    bPendingActivationWhileHeld = false;

    if (ThrowFlashlightActivationDelay <= 0.f)
    {
        ActivateFlashlight();
        return;
    }

    DeactivateFlashlight();

    GetWorldTimerManager().SetTimer(
        DelayedFlashActivationHandle,
        this,
        &APickupActorAAAFlashlight::ActivateFlashlight,
        ThrowFlashlightActivationDelay,
        false);
}

bool APickupActorAAAFlashlight::CanBeClosedByPlayer_Implementation() const
{
    return true;
}

void APickupActorAAAFlashlight::CloseByPlayer_Implementation(AActor* ClosingActor)
{
    bClosedByPlayer = true;
    bPendingActivationWhileHeld = false;
    DeactivateFlashlight();

    UE_LOG(LogTemp, Log, TEXT("%s flashlight closed by %s"),
        *GetName(),
        *GetNameSafe(ClosingActor));
}

bool APickupActorAAAFlashlight::IsClosedByPlayer_Implementation() const
{
    return bClosedByPlayer || !bFlashlightActive;
}

void APickupActorAAAFlashlight::OpenByPlayer_Implementation(AActor* OpeningActor)
{
    if (IsHeldByPlayer())
    {
        bClosedByPlayer = false;
        bPendingActivationWhileHeld = true;
        DeactivateFlashlight();

        UE_LOG(LogTemp, Log, TEXT("%s flashlight armed by %s while held"),
            *GetName(),
            *GetNameSafe(OpeningActor));
        return;
    }

    ActivateFlashlight();
    if (!bFlashlightActive)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("%s flashlight opened by %s"),
        *GetName(),
        *GetNameSafe(OpeningActor));
}

void APickupActorAAAFlashlight::DisableByRage_Implementation(AActor* DisablingActor)
{
    Super::DisableByRage_Implementation(DisablingActor);
    bClosedByPlayer = true;
    bPendingActivationWhileHeld = false;
    DeactivateFlashlight();

    UE_LOG(LogTemp, Warning, TEXT("%s flashlight disabled by Rage actor %s"),
        *GetName(),
        *GetNameSafe(DisablingActor));
}

void APickupActorAAAFlashlight::SyncFlashLightFromSettings()
{
    if (!FlashLightComponent)
    {
        return;
    }

    const float SafeOuterCone = FMath::Clamp(FlashHalfAngleDegrees, 1.f, 89.f);
    const float SafeInnerCone = FMath::Clamp(FlashInnerConeAngleDegrees, 0.f, SafeOuterCone);

    FlashLightComponent->SetLightColor(FlashLightColor);
    FlashLightComponent->SetIntensity(0.f);
    FlashLightComponent->SetAttenuationRadius(FMath::Max(FlashRange, FlashLightAttenuationRadius));
    FlashLightComponent->SetInnerConeAngle(SafeInnerCone);
    FlashLightComponent->SetOuterConeAngle(SafeOuterCone);
}

void APickupActorAAAFlashlight::UpdateFlashVisualState()
{
    SyncFlashLightFromSettings();

    if (!FlashLightComponent)
    {
        return;
    }

    const bool bShouldShowLight = bFlashlightActive && FlashVisibleTimeRemaining > 0.f;
    FlashLightComponent->SetHiddenInGame(!bShouldShowLight);
    FlashLightComponent->SetVisibility(bShouldShowLight);
    FlashLightComponent->SetIntensity(bShouldShowLight ? FlashLightIntensity : 0.f);
}

void APickupActorAAAFlashlight::DrawFlashDebug()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FVector FlashOrigin = GetFlashOriginLocation();
    const FVector FlashForward = GetFlashForwardVector();
    const float HalfAngleRadians = FMath::DegreesToRadians(FlashHalfAngleDegrees);
    const FColor DebugColor = FlashLightColor.ToFColor(true);
    const float PersistTime = -1.f;

    DrawDebugCone(
        World,
        FlashOrigin,
        FlashForward,
        FlashRange,
        HalfAngleRadians,
        HalfAngleRadians,
        24,
        DebugColor,
        false,
        PersistTime,
        0,
        2.f);

    DrawDebugLine(
        World,
        FlashOrigin,
        FlashOrigin + FlashForward * FlashRange,
        DebugColor,
        false,
        PersistTime,
        0,
        2.5f);

    DrawDebugSphere(World, FlashOrigin, 8.f, 12, DebugColor, false, PersistTime, 0, 1.5f);
}

bool APickupActorAAAFlashlight::IsActorInsideFlashCone(const AActor* TargetActor) const
{
    if (!IsValid(TargetActor))
    {
        return false;
    }

    FVector ToTarget = TargetActor->GetActorLocation() - GetFlashOriginLocation();
    if (ToTarget.IsNearlyZero())
    {
        return true;
    }

    const float DistanceSquared = ToTarget.SizeSquared();
    if (DistanceSquared > FMath::Square(FlashRange))
    {
        return false;
    }

    FVector FlashForward = GetFlashForwardVector();
    if (!FlashForward.Normalize())
    {
        FlashForward = GetActorForwardVector();
        FlashForward.Normalize();
    }

    ToTarget.Normalize();

    const float MinDot = FMath::Cos(FMath::DegreesToRadians(FlashHalfAngleDegrees));
    return FVector::DotProduct(FlashForward, ToTarget) >= MinDot;
}

bool APickupActorAAAFlashlight::HasLineOfSightToTarget(const AActor* TargetActor) const
{
    UWorld* World = GetWorld();
    if (!World || !IsValid(TargetActor))
    {
        return false;
    }

    FVector TargetOrigin = FVector::ZeroVector;
    FVector TargetExtent = FVector::ZeroVector;
    TargetActor->GetActorBounds(true, TargetOrigin, TargetExtent);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FlashlightLineOfSight), false, this);
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(TargetActor);

    FHitResult HitResult;
    const FVector TraceStart = GetFlashOriginLocation();
    const FVector TraceEnd = TargetOrigin;

    return !World->LineTraceSingleByChannel(
        HitResult,
        TraceStart,
        TraceEnd,
        FlashBlockTraceChannel,
        QueryParams);
}

FVector APickupActorAAAFlashlight::GetFlashOriginLocation() const
{
    return IsValid(FlashOriginComponent)
        ? FlashOriginComponent->GetComponentLocation()
        : GetActorLocation();
}

FVector APickupActorAAAFlashlight::GetFlashForwardVector() const
{
    return IsValid(FlashOriginComponent)
        ? FlashOriginComponent->GetForwardVector()
        : GetActorForwardVector();
}