#include "PickupActorAAALightBulb.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "MyAIController.h"
#include "TimerManager.h"

APickupActorAAALightBulb::APickupActorAAALightBulb()
{
    PrimaryActorTick.bCanEverTick = true;

    HoldType = EHoldItemType::LightBulb;
    FP_SocketName = TEXT("RightSocket");
    FP_LocationOffset = FVector::ZeroVector;
    FP_RotationOffset = FRotator::ZeroRotator;
    TP_SocketName = TEXT("RightSocket");
    TP_LocationOffset = FVector::ZeroVector;
    TP_RotationOffset = FRotator::ZeroRotator;

    ItemMassKg = 0.15f;
    ItemThrowForceMultiplier = 0.2f;
    ItemLinearDamping = 0.03f;
    ItemAngularDamping = 0.12f;
    ItemThrowSpinRateDegrees = 1800.f;

    Tags.Add(FName("LightBulb"));
    Tags.Add(FName("Pickup"));
    Tags.Add(FName("Light"));

    LightOriginComponent = CreateDefaultSubobject<USceneComponent>(TEXT("LightOriginComponent"));
    LightOriginComponent->SetupAttachment(MeshComponent);
    LightOriginComponent->SetRelativeLocation(FVector(0.f, 0.f, 18.f));

    BulbLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("BulbLightComponent"));
    BulbLightComponent->SetupAttachment(LightOriginComponent);
    BulbLightComponent->SetCastShadows(false);
    BulbLightComponent->SetUseInverseSquaredFalloff(false);
    BulbLightComponent->SetVisibility(false);
    BulbLightComponent->SetHiddenInGame(true);
    BulbLightComponent->SetIntensity(0.f);
    BulbLightComponent->SetCanEverAffectNavigation(false);

    SyncLightFromSettings();
    UpdateLightVisualState();
}

void APickupActorAAALightBulb::BeginPlay()
{
    Super::BeginPlay();

    SyncLightFromSettings();

    bLightBulbActive = !bClosedByPlayer && !IsDisabledByRage();
    CurrentBrightnessAlpha = 0.f;
    UpdateLightVisualState();
}

void APickupActorAAALightBulb::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    CurrentBrightnessAlpha = 0.f;
    SyncLightFromSettings();
    UpdateLightVisualState();
}

void APickupActorAAALightBulb::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    const UWorld* World = GetWorld();
    if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || !World || !World->IsGameWorld())
    {
        CurrentBrightnessAlpha = 0.f;
        UpdateLightVisualState();
        return;
    }

    const APawn* NearestGhost = nullptr;
    float NearestDistance = -1.f;
    const float TargetBrightnessAlpha = ComputeTargetBrightnessAlpha(NearestGhost, NearestDistance);

    const float InterpSpeed = TargetBrightnessAlpha > CurrentBrightnessAlpha
        ? BrightenInterpSpeed
        : DimInterpSpeed;

    if (InterpSpeed <= 0.f)
    {
        CurrentBrightnessAlpha = TargetBrightnessAlpha;
    }
    else
    {
        CurrentBrightnessAlpha = FMath::FInterpTo(
            CurrentBrightnessAlpha,
            TargetBrightnessAlpha,
            DeltaTime,
            InterpSpeed);
    }

    UpdateLightVisualState();

    if (bEnableLightBulbDebug)
    {
        DrawLightBulbDebug(NearestGhost, NearestDistance);
    }
}

void APickupActorAAALightBulb::ActivateLightBulb()
{
    if (IsDisabledByRage())
    {
        DeactivateLightBulb();
        return;
    }

    bClosedByPlayer = false;
    bLightBulbActive = true;

    GetWorldTimerManager().ClearTimer(DelayedLightBulbActivationHandle);

    UE_LOG(LogTemp, Log, TEXT("%s light bulb activated"), *GetName());
}

void APickupActorAAALightBulb::DeactivateLightBulb()
{
    bLightBulbActive = false;
    CurrentBrightnessAlpha = 0.f;

    GetWorldTimerManager().ClearTimer(DelayedLightBulbActivationHandle);
    UpdateLightVisualState();

    UE_LOG(LogTemp, Log, TEXT("%s light bulb deactivated"), *GetName());
}

void APickupActorAAALightBulb::OnPickedUp()
{
    Super::OnPickedUp();

    GetWorldTimerManager().ClearTimer(DelayedLightBulbActivationHandle);

    if (bClosedByPlayer || IsDisabledByRage())
    {
        DeactivateLightBulb();
        return;
    }

    bLightBulbActive = true;
    UpdateLightVisualState();
}

void APickupActorAAALightBulb::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    Super::OnPutDown(PlaceLocation, PlaceRotation);

    if (bClosedByPlayer || IsDisabledByRage())
    {
        DeactivateLightBulb();
        return;
    }

    if (bAutoActivateLightBulbWhenPlaced)
    {
        ActivateLightBulb();
    }
    else
    {
        DeactivateLightBulb();
    }
}

void APickupActorAAALightBulb::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    Super::OnThrown(ThrowDirection, ThrowForce);

    if (bClosedByPlayer || IsDisabledByRage() || !bAutoActivateLightBulbWhenThrown)
    {
        DeactivateLightBulb();
        return;
    }

    if (ThrowLightBulbActivationDelay <= 0.f)
    {
        ActivateLightBulb();
        return;
    }

    DeactivateLightBulb();

    GetWorldTimerManager().SetTimer(
        DelayedLightBulbActivationHandle,
        this,
        &APickupActorAAALightBulb::ActivateLightBulb,
        ThrowLightBulbActivationDelay,
        false);
}

void APickupActorAAALightBulb::DisableByRage_Implementation(AActor* DisablingActor)
{
    Super::DisableByRage_Implementation(DisablingActor);
    if (!IsDisabledByRage())
    {
        return;
    }

    bClosedByPlayer = true;
    DeactivateLightBulb();

    UE_LOG(LogTemp, Warning, TEXT("%s light bulb disabled by Rage actor %s"),
        *GetName(),
        *GetNameSafe(DisablingActor));
}

void APickupActorAAALightBulb::SyncLightFromSettings()
{
    if (!BulbLightComponent)
    {
        return;
    }

    BulbLightComponent->SetLightColor(LightColor);
    BulbLightComponent->SetAttenuationRadius(FMath::Max(LightAttenuationRadius, EffectRadius));
}

void APickupActorAAALightBulb::UpdateLightVisualState()
{
    SyncLightFromSettings();

    if (!BulbLightComponent)
    {
        return;
    }

    const UWorld* World = GetWorld();
    if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || !World || !World->IsGameWorld())
    {
        BulbLightComponent->SetHiddenInGame(true);
        BulbLightComponent->SetVisibility(false);
        BulbLightComponent->SetIntensity(0.f);
        return;
    }

    const bool bShouldShowLight = CurrentBrightnessAlpha > KINDA_SMALL_NUMBER;
    BulbLightComponent->SetHiddenInGame(!bShouldShowLight);
    BulbLightComponent->SetVisibility(bShouldShowLight);
    BulbLightComponent->SetIntensity(bShouldShowLight ? MaxLightIntensity * CurrentBrightnessAlpha : 0.f);
}

float APickupActorAAALightBulb::ComputeTargetBrightnessAlpha(const APawn*& OutNearestGhost, float& OutNearestDistance) const
{
    OutNearestGhost = nullptr;
    OutNearestDistance = -1.f;

    const UWorld* World = GetWorld();
    if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || !bLightBulbActive || IsDisabledByRage() || !World || !World->IsGameWorld())
    {
        return 0.f;
    }

    const FVector LightOrigin = GetLightOriginLocation();
    const float SafeRadius = FMath::Max(1.f, EffectRadius);
    const float SafeFullBrightnessDistance = FMath::Clamp(FullBrightnessDistance, 0.f, SafeRadius);
    const float MaxDistanceSquared = FMath::Square(SafeRadius);

    float BestDistanceSquared = TNumericLimits<float>::Max();

    for (TActorIterator<APawn> It(World); It; ++It)
    {
        APawn* CandidatePawn = *It;
        if (!IsValid(CandidatePawn))
        {
            continue;
        }

        if (!Cast<AMyAIController>(CandidatePawn->GetController()))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(LightOrigin, CandidatePawn->GetActorLocation());
        if (DistanceSquared > MaxDistanceSquared || DistanceSquared >= BestDistanceSquared)
        {
            continue;
        }

        BestDistanceSquared = DistanceSquared;
        OutNearestGhost = CandidatePawn;
    }

    if (!OutNearestGhost)
    {
        return 0.f;
    }

    OutNearestDistance = FMath::Sqrt(BestDistanceSquared);
    if (OutNearestDistance <= SafeFullBrightnessDistance)
    {
        return 1.f;
    }

    const float FadeDistance = FMath::Max(1.f, SafeRadius - SafeFullBrightnessDistance);
    const float NormalizedAlpha = 1.f - ((OutNearestDistance - SafeFullBrightnessDistance) / FadeDistance);
    return FMath::Clamp(NormalizedAlpha, 0.f, 1.f);
}

void APickupActorAAALightBulb::DrawLightBulbDebug(const APawn* NearestGhost, float NearestDistance) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FVector LightOrigin = GetLightOriginLocation();
    const FColor DebugColor = LightColor.ToFColor(true);

    DrawDebugSphere(World, LightOrigin, EffectRadius, 24, DebugColor, false, 0.f, 0, 1.5f);
    DrawDebugSphere(World, LightOrigin, FMath::Max(8.f, FullBrightnessDistance), 16, DebugColor, false, 0.f, 0, 1.f);

    if (!IsValid(NearestGhost))
    {
        return;
    }

    DrawDebugLine(
        World,
        LightOrigin,
        NearestGhost->GetActorLocation(),
        DebugColor,
        false,
        0.f,
        0,
        2.f);

    DrawDebugString(
        World,
        LightOrigin + FVector(0.f, 0.f, 12.f),
        FString::Printf(TEXT("GhostDist %.1f"), NearestDistance),
        nullptr,
        DebugColor,
        0.f,
        false);
}

FVector APickupActorAAALightBulb::GetLightOriginLocation() const
{
    return IsValid(LightOriginComponent)
        ? LightOriginComponent->GetComponentLocation()
        : GetActorLocation();
}