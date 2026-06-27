#include "PickupActorAAARuneGlowSensorCard.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MyAIController.h"
#include "TimerManager.h"

APickupActorAAARuneGlowSensorCard::APickupActorAAARuneGlowSensorCard()
{
    PrimaryActorTick.bCanEverTick = true;

    Tags.Add(FName("RuneGlowSensorCard"));
    Tags.Add(FName("Rune"));
    Tags.Add(FName("GlowSensor"));

    SensorLightOriginComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SensorLightOriginComponent"));
    SensorLightOriginComponent->SetupAttachment(VisualMeshRootComponent);

    SensorLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("SensorLightComponent"));
    SensorLightComponent->SetupAttachment(SensorLightOriginComponent);
    SensorLightComponent->SetCastShadows(false);
    SensorLightComponent->SetUseInverseSquaredFalloff(false);
    SensorLightComponent->SetVisibility(false);
    SensorLightComponent->SetHiddenInGame(true);
    SensorLightComponent->SetIntensity(0.f);
    SensorLightComponent->SetCanEverAffectNavigation(false);

    FRuneCanvasPattern DefaultGlowSensorPattern;
    DefaultGlowSensorPattern.PatternId = TEXT("GlowSensor");
    DefaultGlowSensorPattern.NodeSequence = {
        210, 211, 212, 213, 214, 215, 216, 217,
        258, 299, 340, 381, 422, 463,
        462, 461, 460, 459, 458, 457, 456,
        415, 374, 333, 292, 251};
    GlowSensorCardExpectedSequences.Add(DefaultGlowSensorPattern);

    SyncSensorLightFromSettings();
    UpdateSensorLightVisualState();
}

void APickupActorAAARuneGlowSensorCard::BeginPlay()
{
    Super::BeginPlay();
    SyncSensorLightFromSettings();
    UpdateSensorLightVisualState();
}

void APickupActorAAARuneGlowSensorCard::OnConstruction(const FTransform &Transform)
{
    Super::OnConstruction(Transform);
    CurrentBrightnessAlpha = 0.f;
    SyncSensorLightFromSettings();
    UpdateSensorLightVisualState();
}

void APickupActorAAARuneGlowSensorCard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(WallLifetimeHandle);
    Super::EndPlay(EndPlayReason);
}

void APickupActorAAARuneGlowSensorCard::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    const UWorld *World = GetWorld();
    if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || !World || !World->IsGameWorld())
    {
        CurrentBrightnessAlpha = 0.f;
        UpdateSensorLightVisualState();
        return;
    }

    const APawn *NearestGhost = nullptr;
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

    UpdateSensorLightVisualState();

    if (bEnableGlowSensorDebug)
    {
        DrawGlowSensorDebug(NearestGhost, NearestDistance);
    }
}

void APickupActorAAARuneGlowSensorCard::OnPickedUp()
{
    bGlowSensorActive = false;
    bHasCachedThrowSimilarityMultiplier = false;
    CachedThrowSimilarityMultiplier = 0.f;
    CurrentBrightnessAlpha = 0.f;
    GetWorldTimerManager().ClearTimer(WallLifetimeHandle);
    Super::OnPickedUp();
    UpdateSensorLightVisualState();
}

void APickupActorAAARuneGlowSensorCard::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    bGlowSensorActive = false;
    bHasCachedThrowSimilarityMultiplier = false;
    CachedThrowSimilarityMultiplier = 0.f;
    CurrentBrightnessAlpha = 0.f;
    GetWorldTimerManager().ClearTimer(WallLifetimeHandle);
    Super::OnPutDown(PlaceLocation, PlaceRotation);
    UpdateSensorLightVisualState();
}

void APickupActorAAARuneGlowSensorCard::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    GetWorldTimerManager().ClearTimer(WallLifetimeHandle);
    CachedThrowSimilarityMultiplier = FMath::Clamp(GetCurrentCardSequenceSimilarityPercent() / 100.f, 0.f, 1.f);
    bHasCachedThrowSimilarityMultiplier = true;
    bGlowSensorActive = GetSimilarityMultiplier() > UE_KINDA_SMALL_NUMBER;
    CurrentBrightnessAlpha = 0.f;
    Super::OnThrown(ThrowDirection, ThrowForce);
    UpdateSensorLightVisualState();
}

void APickupActorAAARuneGlowSensorCard::OnRuneCanvasAttachedToSurface()
{
    const float ScaledLifetime = GetScaledWallLifetime();
    bGlowSensorActive = ScaledLifetime > UE_KINDA_SMALL_NUMBER;

    if (!bGlowSensorActive)
    {
        Destroy();
        return;
    }

    GetWorldTimerManager().ClearTimer(WallLifetimeHandle);
    GetWorldTimerManager().SetTimer(
        WallLifetimeHandle,
        this,
        &APickupActorAAARuneGlowSensorCard::ExpireGlowSensorCard,
        ScaledLifetime,
        false);

    UE_LOG(LogTemp, Log, TEXT("%s glow sensor attached for %.2fs with radius %.1f (similarity %.1f%%)"),
           *GetName(),
           ScaledLifetime,
           GetScaledEffectRadius(),
           GetCurrentCardSequenceSimilarityPercent());
}

void APickupActorAAARuneGlowSensorCard::OnRuneCanvasDetachedFromSurface()
{
    bGlowSensorActive = false;
    CurrentBrightnessAlpha = 0.f;
    GetWorldTimerManager().ClearTimer(WallLifetimeHandle);
    UpdateSensorLightVisualState();
}

bool APickupActorAAARuneGlowSensorCard::TryHandleRuneCanvasThrownImpact(const FHitResult &Hit, UPrimitiveComponent *HitComponent)
{
    (void)HitComponent;

    if (IsGhostActor(Hit.GetActor()))
    {
        UE_LOG(LogTemp, Verbose, TEXT("%s glow sensor card hit ghost %s and fizzled; it cannot attach to ghosts"),
               *GetName(),
               *GetNameSafe(Hit.GetActor()));
        Destroy();
        return true;
    }

    return false;
}

const TArray<int32> &APickupActorAAARuneGlowSensorCard::GetExpectedNodeSequenceForCurrentCard() const
{
    static const TArray<int32> EmptySequence;
    const int32 CardResourceIndex = GetCurrentCardResourceIndex();
    return GlowSensorCardExpectedSequences.IsValidIndex(CardResourceIndex)
               ? GlowSensorCardExpectedSequences[CardResourceIndex].NodeSequence
               : EmptySequence;
}

float APickupActorAAARuneGlowSensorCard::GetSimilarityMultiplier() const
{
    if (bHasCachedThrowSimilarityMultiplier)
    {
        return FMath::Clamp(CachedThrowSimilarityMultiplier, 0.f, 1.f);
    }

    return FMath::Clamp(GetCurrentCardSequenceSimilarityPercent() / 100.f, 0.f, 1.f);
}

float APickupActorAAARuneGlowSensorCard::GetScaledEffectRadius() const
{
    return FMath::Max(0.f, MaxEffectRadius) * GetSimilarityMultiplier();
}

float APickupActorAAARuneGlowSensorCard::GetScaledFullBrightnessDistance() const
{
    return FMath::Min(GetScaledEffectRadius(), FMath::Max(0.f, MaxFullBrightnessDistance) * GetSimilarityMultiplier());
}

float APickupActorAAARuneGlowSensorCard::GetScaledWallLifetime() const
{
    return FMath::Max(0.f, MaxWallLifetime) * GetSimilarityMultiplier();
}

bool APickupActorAAARuneGlowSensorCard::IsGhostActor(const AActor *Actor) const
{
    const APawn *Pawn = Cast<APawn>(Actor);
    return IsValid(Pawn) && IsValid(Cast<AMyAIController>(Pawn->GetController()));
}

float APickupActorAAARuneGlowSensorCard::ComputeTargetBrightnessAlpha(const APawn *&OutNearestGhost, float &OutNearestDistance) const
{
    OutNearestGhost = nullptr;
    OutNearestDistance = -1.f;

    const UWorld *World = GetWorld();
    const float SafeRadius = GetScaledEffectRadius();
    if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || !bGlowSensorActive || IsHeldByPlayer() || IsDisabledByRage() || SafeRadius <= UE_KINDA_SMALL_NUMBER || !World || !World->IsGameWorld())
    {
        return 0.f;
    }

    const FVector SensorOrigin = GetSensorLightOriginLocation();
    const float SafeFullBrightnessDistance = FMath::Clamp(GetScaledFullBrightnessDistance(), 0.f, SafeRadius);
    const float MaxDistanceSquared = FMath::Square(SafeRadius);
    float BestDistanceSquared = TNumericLimits<float>::Max();

    for (TActorIterator<APawn> It(World); It; ++It)
    {
        APawn *CandidatePawn = *It;
        if (!IsValid(CandidatePawn) || !Cast<AMyAIController>(CandidatePawn->GetController()))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(SensorOrigin, CandidatePawn->GetActorLocation());
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

FVector APickupActorAAARuneGlowSensorCard::GetSensorLightOriginLocation() const
{
    return IsValid(SensorLightOriginComponent)
               ? SensorLightOriginComponent->GetComponentLocation()
               : GetActorLocation();
}

void APickupActorAAARuneGlowSensorCard::SyncSensorLightFromSettings()
{
    if (!SensorLightComponent)
    {
        return;
    }

    SensorLightComponent->SetLightColor(SensorLightColor);
    SensorLightComponent->SetAttenuationRadius(FMath::Max(LightAttenuationRadius, GetScaledEffectRadius()));
}

void APickupActorAAARuneGlowSensorCard::UpdateSensorLightVisualState()
{
    SyncSensorLightFromSettings();
    UpdateSensorMaterialVisualState();

    if (!SensorLightComponent)
    {
        return;
    }

    const UWorld *World = GetWorld();
    if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || !World || !World->IsGameWorld())
    {
        SensorLightComponent->SetHiddenInGame(true);
        SensorLightComponent->SetVisibility(false);
        SensorLightComponent->SetIntensity(0.f);
        return;
    }

    const bool bShouldShowLight = CurrentBrightnessAlpha > KINDA_SMALL_NUMBER;
    SensorLightComponent->SetHiddenInGame(!bShouldShowLight);
    SensorLightComponent->SetVisibility(bShouldShowLight);
    SensorLightComponent->SetIntensity(bShouldShowLight ? MaxLightIntensity * CurrentBrightnessAlpha : 0.f);
}

void APickupActorAAARuneGlowSensorCard::UpdateSensorMaterialVisualState()
{
    if (!MeshComponent)
    {
        return;
    }

    const float GlowIntensity = MaxLightIntensity * 0.01f * CurrentBrightnessAlpha;
    const FLinearColor TintColor(
        SensorLightColor.R,
        SensorLightColor.G,
        SensorLightColor.B,
        FMath::Clamp(CurrentBrightnessAlpha, 0.f, 1.f));

    const int32 MaterialsSlotCount = MeshComponent->GetMaterials().Num();
    for (int32 MaterialIndex = 0; MaterialIndex < MaterialsSlotCount; ++MaterialIndex)
    {
        UMaterialInstanceDynamic *DynamicMaterial = Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(MaterialIndex));
        if (!IsValid(DynamicMaterial))
        {
            UMaterialInterface *BaseMaterial = MeshComponent->GetMaterial(MaterialIndex);
            if (!BaseMaterial)
            {
                continue;
            }

            DynamicMaterial = MeshComponent->CreateDynamicMaterialInstance(MaterialIndex, BaseMaterial);
        }

        if (!IsValid(DynamicMaterial))
        {
            continue;
        }

        DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), SensorLightColor);
        DynamicMaterial->SetVectorParameterValue(TEXT("GlowColor"), SensorLightColor);
        DynamicMaterial->SetVectorParameterValue(TEXT("TintColor"), TintColor);
        DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveIntensity"), GlowIntensity);
        DynamicMaterial->SetScalarParameterValue(TEXT("GlowIntensity"), GlowIntensity);
        DynamicMaterial->SetScalarParameterValue(TEXT("GlowEnabled"), CurrentBrightnessAlpha > KINDA_SMALL_NUMBER ? 1.f : 0.f);
    }
}

void APickupActorAAARuneGlowSensorCard::DrawGlowSensorDebug(const APawn *NearestGhost, float NearestDistance) const
{
    UWorld *World = GetWorld();
    if (!World)
    {
        return;
    }

    const FVector SensorOrigin = GetSensorLightOriginLocation();
    const FColor DebugColor = SensorLightColor.ToFColor(true);
    DrawDebugSphere(World, SensorOrigin, GetScaledEffectRadius(), 24, DebugColor, false, 0.f, 0, 1.5f);
    DrawDebugSphere(World, SensorOrigin, FMath::Max(8.f, GetScaledFullBrightnessDistance()), 16, DebugColor, false, 0.f, 0, 1.f);

    if (!IsValid(NearestGhost))
    {
        return;
    }

    DrawDebugLine(World, SensorOrigin, NearestGhost->GetActorLocation(), DebugColor, false, 0.f, 0, 2.f);
    DrawDebugString(
        World,
        SensorOrigin + FVector(0.f, 0.f, 12.f),
        FString::Printf(TEXT("GhostDist %.1f Radius %.1f Life %.1f"), NearestDistance, GetScaledEffectRadius(), GetScaledWallLifetime()),
        nullptr,
        DebugColor,
        0.f,
        false);
}

void APickupActorAAARuneGlowSensorCard::ExpireGlowSensorCard()
{
    Destroy();
}