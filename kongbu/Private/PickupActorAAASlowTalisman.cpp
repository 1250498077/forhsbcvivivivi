#include "PickupActorAAASlowTalisman.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GhostCharacter.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MyAIController.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

namespace
{
constexpr int32 SlowTalismanShardCount = 10;
const FVector SlowTalismanShardScaleMin(0.12f, 0.06f, 0.12f);
const FVector SlowTalismanShardScaleMax(0.28f, 0.12f, 0.32f);
constexpr float SlowTalismanShardBurstImpulse = 150.f;
constexpr float SlowTalismanImpactVelocityInheritance = 0.35f;
constexpr float SlowTalismanShardLifetime = 4.5f;

UStaticMesh* ResolveSlowTalismanShardMesh(const UStaticMeshComponent* SourceMeshComponent)
{
    if (IsValid(SourceMeshComponent))
    {
        if (UStaticMesh* SourceMesh = SourceMeshComponent->GetStaticMesh())
        {
            return SourceMesh;
        }
    }

    static UStaticMesh* FallbackShardMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    return FallbackShardMesh;
}

void SpawnSlowTalismanShards(
    UWorld* World,
    UStaticMesh* ShardMesh,
    UMaterialInterface* ShardMaterial,
    const FVector& ImpactPoint,
    const FVector& ImpactNormal,
    const FVector& InheritedVelocity)
{
    if (!World || !ShardMesh)
    {
        return;
    }

    const FVector SafeImpactNormal = ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
    FVector TangentX = FVector::ForwardVector;
    FVector TangentY = FVector::RightVector;
    SafeImpactNormal.FindBestAxisVectors(TangentX, TangentY);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    for (int32 ShardIndex = 0; ShardIndex < SlowTalismanShardCount; ++ShardIndex)
    {
        const FVector RandomOffset = FMath::VRand() * FMath::FRandRange(1.5f, 10.f);
        const FRotator RandomRotation = FRotator(
            FMath::FRandRange(-180.f, 180.f),
            FMath::FRandRange(-180.f, 180.f),
            FMath::FRandRange(-180.f, 180.f));

        AStaticMeshActor* ShardActor = World->SpawnActor<AStaticMeshActor>(
            ImpactPoint + RandomOffset,
            RandomRotation,
            SpawnParams);
        if (!ShardActor)
        {
            continue;
        }

        UStaticMeshComponent* ShardMeshComponent = ShardActor->GetStaticMeshComponent();
        if (!ShardMeshComponent)
        {
            ShardActor->Destroy();
            continue;
        }

        ShardMeshComponent->SetMobility(EComponentMobility::Movable);
        ShardMeshComponent->SetStaticMesh(ShardMesh);
        ShardMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        ShardMeshComponent->SetCollisionObjectType(ECC_PhysicsBody);
        ShardMeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
        ShardMeshComponent->SetNotifyRigidBodyCollision(false);
        ShardMeshComponent->SetEnableGravity(true);
        ShardMeshComponent->SetSimulatePhysics(true);
        ShardMeshComponent->BodyInstance.bUseCCD = true;

        const FVector RandomScale(
            FMath::FRandRange(SlowTalismanShardScaleMin.X, SlowTalismanShardScaleMax.X),
            FMath::FRandRange(SlowTalismanShardScaleMin.Y, SlowTalismanShardScaleMax.Y),
            FMath::FRandRange(SlowTalismanShardScaleMin.Z, SlowTalismanShardScaleMax.Z));
        ShardMeshComponent->SetWorldScale3D(RandomScale);

        if (ShardMaterial)
        {
            ShardMeshComponent->SetMaterial(0, ShardMaterial);
        }

        const FVector ScatterDirection = (
            SafeImpactNormal * 0.55f +
            TangentX * FMath::FRandRange(-1.f, 1.f) +
            TangentY * FMath::FRandRange(-1.f, 1.f) +
            FMath::VRand() * 0.35f).GetSafeNormal();

        const FVector Impulse =
            ScatterDirection * (SlowTalismanShardBurstImpulse * FMath::FRandRange(0.75f, 1.35f)) +
            InheritedVelocity * SlowTalismanImpactVelocityInheritance;

        ShardMeshComponent->WakeAllRigidBodies();
        ShardMeshComponent->AddImpulse(Impulse, NAME_None, false);
        ShardMeshComponent->AddAngularImpulseInDegrees(
            FMath::VRand() * FMath::FRandRange(280.f, 1100.f),
            NAME_None,
            false);

        ShardActor->SetLifeSpan(SlowTalismanShardLifetime);
    }
}
}

APickupActorAAASlowTalisman::APickupActorAAASlowTalisman()
{
    PrimaryActorTick.bCanEverTick = true;

    HoldType = EHoldItemType::Talisman;
    FP_SocketName = TEXT("RightSocket");
    FP_LocationOffset = FVector::ZeroVector;
    FP_RotationOffset = FRotator::ZeroRotator;
    TP_SocketName = TEXT("RightSocket");
    TP_LocationOffset = FVector::ZeroVector;
    TP_RotationOffset = FRotator::ZeroRotator;

    ItemMassKg = 0.12f;
    ItemThrowForceMultiplier = 0.18f;
    ItemLinearDamping = 0.18f;
    ItemAngularDamping = 0.7f;
    ItemThrowSpinRateDegrees = 700.f;

    Tags.Add(FName("SlowTalisman"));
    Tags.Add(FName("Pickup"));
    Tags.Add(FName("Slow"));

    if (MeshComponent)
    {
        MeshComponent->SetNotifyRigidBodyCollision(true);
        MeshComponent->BodyInstance.bUseCCD = true;
        MeshComponent->OnComponentHit.AddDynamic(this, &APickupActorAAASlowTalisman::HandleTalismanHit);
    }

    SlowTriggerComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SlowTriggerComponent"));
    SlowTriggerComponent->SetupAttachment(MeshComponent);
    SlowTriggerComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SlowTriggerComponent->SetCollisionObjectType(ECC_WorldDynamic);
    SlowTriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    SlowTriggerComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    SlowTriggerComponent->SetGenerateOverlapEvents(false);
    SlowTriggerComponent->SetCanEverAffectNavigation(false);
    SlowTriggerComponent->OnComponentBeginOverlap.AddDynamic(this, &APickupActorAAASlowTalisman::HandleSlowTriggerBeginOverlap);

    GlowLightComponent = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLightComponent"));
    GlowLightComponent->SetupAttachment(MeshComponent);
    GlowLightComponent->SetCastShadows(false);
    GlowLightComponent->SetUseInverseSquaredFalloff(false);
    GlowLightComponent->SetLightColor(ActivationGlowColor);
    GlowLightComponent->SetIntensity(0.f);
    GlowLightComponent->SetAttenuationRadius(ActivationLightRadius);
    GlowLightComponent->SetVisibility(false);
    GlowLightComponent->SetHiddenInGame(true);
    GlowLightComponent->SetCanEverAffectNavigation(false);

    UpdateSlowTriggerRadius();
    UpdateSlowTriggerState();
}

void APickupActorAAASlowTalisman::BeginPlay()
{
    Super::BeginPlay();
    CacheOriginalMaterials();
    ApplyThrowablePhysicsTuning();
    UpdateSlowTriggerRadius();
    UpdateSlowTriggerState();
    UpdateActivationVisualState();
}

void APickupActorAAASlowTalisman::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    CacheOriginalMaterials();
    ApplyThrowablePhysicsTuning();
    UpdateSlowTriggerRadius();
    UpdateSlowTriggerState();

    if (GlowLightComponent)
    {
        GlowLightComponent->SetHiddenInGame(true);
        GlowLightComponent->SetVisibility(false);
        GlowLightComponent->SetIntensity(0.f);
    }
}

void APickupActorAAASlowTalisman::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(SlowDurationHandle);
    ClearAttachedSlowEffect();
    OriginalMaterials.Reset();
    Super::EndPlay(EndPlayReason);
}

void APickupActorAAASlowTalisman::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!MeshComponent || AttachedController.IsValid())
    {
        return;
    }

    if (IsHeldByPlayer())
    {
        HeldFlutterTime += DeltaTime;

        const float PitchOffset = FMath::Sin(HeldFlutterTime * HeldFlutterSpeed) * HeldFlutterPitchAngle;
        const float RollOffset = FMath::Cos(HeldFlutterTime * (HeldFlutterSpeed * 1.31f)) * HeldFlutterRollAngle;
        SetActorRelativeRotation(FRotator(PitchOffset, 0.f, RollOffset));

        FlightFlutterTime = 0.f;
        return;
    }

    HeldFlutterTime = 0.f;

    if (!bAwaitingThrowImpact || !MeshComponent->IsSimulatingPhysics())
    {
        FlightFlutterTime = 0.f;
        return;
    }

    const FVector Velocity = MeshComponent->GetPhysicsLinearVelocity();
    const float Speed = Velocity.Size();
    if (Speed < FlightFlutterMinSpeed)
    {
        return;
    }

    FlightFlutterTime += DeltaTime;

    const float FlutterStrength = FMath::Clamp((Speed - FlightFlutterMinSpeed) / FMath::Max(FlightFlutterMinSpeed, 1.f), 0.f, 1.f);
    const float PrimaryWave = FMath::Sin(FlightFlutterTime * FlightFlutterFrequency);
    const float SecondaryWave = FMath::Cos(FlightFlutterTime * (FlightFlutterFrequency * 0.63f));
    const float LiftScale = (0.35f + FMath::Abs(PrimaryWave) * 0.65f) * FlutterStrength;

    MeshComponent->AddForce(FVector::UpVector * FlightFlutterLiftForce * LiftScale * MeshComponent->GetMass());

    const FVector FlutterTorque =
        MeshComponent->GetRightVector() * (PrimaryWave * FlightFlutterTorque * FlutterStrength) +
        MeshComponent->GetForwardVector() * (SecondaryWave * FlightFlutterTorque * 0.45f * FlutterStrength);
    MeshComponent->AddTorqueInDegrees(FlutterTorque, NAME_None, false);
}

void APickupActorAAASlowTalisman::OnPickedUp()
{
    ClearAttachedSlowEffect();
    ClearPreferredStickTarget();
    GetWorldTimerManager().ClearTimer(SlowDurationHandle);
    RestoreDefaultThrowableCollision();

    bAwaitingThrowImpact = false;
    bHasConsumedTarget = false;
    bPendingGroundActivation = false;
    bClosedByPlayer = true;
    HeldFlutterTime = 0.f;
    FlightFlutterTime = 0.f;

    Super::OnPickedUp();
    DeactivateTalisman();
    UpdateActivationVisualState();
}

void APickupActorAAASlowTalisman::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    const bool bShouldActivateOnRelease = bPendingGroundActivation && !bClosedByPlayer;

    ClearAttachedSlowEffect();
    ClearPreferredStickTarget();
    GetWorldTimerManager().ClearTimer(SlowDurationHandle);
    RestoreDefaultThrowableCollision();

    Super::OnPutDown(PlaceLocation, PlaceRotation);

    bAwaitingThrowImpact = false;
    bHasConsumedTarget = false;
    bPendingGroundActivation = false;
    FlightFlutterTime = 0.f;

    if (bShouldActivateOnRelease)
    {
        ActivateTalisman();
    }
    else
    {
        DeactivateTalisman();
    }

    UpdateActivationVisualState();
}

void APickupActorAAASlowTalisman::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    const bool bShouldActivateOnImpact = bPendingGroundActivation && !bClosedByPlayer;

    ClearAttachedSlowEffect();
    GetWorldTimerManager().ClearTimer(SlowDurationHandle);
    RestoreDefaultThrowableCollision();

    bAwaitingThrowImpact = true;
    bHasConsumedTarget = false;
    bPendingGroundActivation = bShouldActivateOnImpact;
    bClosedByPlayer = !bShouldActivateOnImpact;
    FlightFlutterTime = 0.f;

    Super::OnThrown(ThrowDirection, ThrowForce);
    DeactivateTalisman();
    UpdateActivationVisualState();
}

bool APickupActorAAASlowTalisman::CanBeClosedByPlayer_Implementation() const
{
    return true;
}

void APickupActorAAASlowTalisman::CloseByPlayer_Implementation(AActor* ClosingActor)
{
    bClosedByPlayer = true;
    bPendingGroundActivation = false;
    DeactivateTalisman();
    UpdateActivationVisualState();

    UE_LOG(LogTemp, Log, TEXT("%s talisman closed by %s"),
        *GetName(),
        *GetNameSafe(ClosingActor));
}

bool APickupActorAAASlowTalisman::IsClosedByPlayer_Implementation() const
{
    return bClosedByPlayer && !bTalismanActive && !bPendingGroundActivation;
}

bool APickupActorAAASlowTalisman::TryAttachToGhostZone(
    APawn* TargetPawn,
    AMyAIController* TargetController,
    USceneComponent* AttachComponent)
{
    if (!bAwaitingThrowImpact || bHasConsumedTarget || !IsValid(TargetPawn) || !TargetController)
    {
        return false;
    }

    if (!TargetController->IsGhostRevealedByEffect())
    {
        return false;
    }

    AttachToAITarget(TargetPawn, TargetController, AttachComponent);
    return bHasConsumedTarget;
}

void APickupActorAAASlowTalisman::SetPreferredStickTarget(const FHitResult& HitResult)
{
    if (!HitResult.bBlockingHit || !IsValid(HitResult.GetActor()))
    {
        ClearPreferredStickTarget();
        return;
    }

    bHasPreferredStickTarget = true;
    PreferredStickActor = HitResult.GetActor();
    PreferredStickComponent = HitResult.GetComponent();
    PreferredStickImpactPoint = HitResult.ImpactPoint;
    PreferredStickImpactNormal = HitResult.ImpactNormal;
}

void APickupActorAAASlowTalisman::ClearPreferredStickTarget()
{
    bHasPreferredStickTarget = false;
    PreferredStickActor.Reset();
    PreferredStickComponent.Reset();
    PreferredStickImpactPoint = FVector::ZeroVector;
    PreferredStickImpactNormal = FVector::ZeroVector;
}

void APickupActorAAASlowTalisman::OpenByPlayer_Implementation(AActor* OpeningActor)
{
    bClosedByPlayer = false;

    if (IsHeldByPlayer())
    {
        bPendingGroundActivation = true;
        UpdateSlowTriggerState();
        UpdateActivationVisualState();

        UE_LOG(LogTemp, Log, TEXT("%s talisman armed by %s while held"),
            *GetName(),
            *GetNameSafe(OpeningActor));
        return;
    }

    ActivateTalisman();
    if (!bTalismanActive)
    {
        return;
    }

    bPendingGroundActivation = false;
    UpdateActivationVisualState();

    UE_LOG(LogTemp, Log, TEXT("%s talisman opened by %s"),
        *GetName(),
        *GetNameSafe(OpeningActor));
}

void APickupActorAAASlowTalisman::HandleTalismanHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    if (!bAwaitingThrowImpact || HitComponent != MeshComponent || !Hit.bBlockingHit)
    {
        return;
    }

    if (!IsValid(OtherActor) || OtherActor == this)
    {
        return;
    }

    // ====================
    APawn* HitPawn = Cast<APawn>(OtherActor);
    AMyAIController* HitAIController = nullptr;

    // 只有显形的鬼才能被慢符击中；隐形时符纸穿过不生效。
    if (HitPawn)
    {    
        HitAIController = Cast<AMyAIController>(HitPawn->GetController());
        if (HitAIController)
        {
            if (!HitAIController->IsGhostRevealedByEffect())
            {
                return;
            }
        }
    }

    if (HitPawn && HitAIController && !bHasConsumedTarget)
    {
        AGhostCharacter* GhostCharacter = Cast<AGhostCharacter>(HitPawn);
        if (GhostCharacter && GhostCharacter->IsWorldLocationInsideGhostAttachZone(Hit.ImpactPoint))
        {
            AttachToAITarget(HitPawn, HitAIController, Cast<USceneComponent>(GhostCharacter->GetGhostAttachZoneComponent()));
            return;
        }

        UE_LOG(LogTemp, Verbose, TEXT("%s hit revealed ghost body outside GhostAttachZoneComponent; waiting for zone overlap"), *GetName());
        return;
    }
    // ====================

    StickToImpact(Hit, OtherComp);

    if (bPendingGroundActivation && !bHasConsumedTarget)
    {
        ActivateTalisman();
        if (bTalismanActive)
        {
            bPendingGroundActivation = false;
        }
    }
}

void APickupActorAAASlowTalisman::HandleSlowTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!bTalismanActive || bHasConsumedTarget)
    {
        return;
    }

    TryAttachToOverlappingAI(OtherActor);
}

void APickupActorAAASlowTalisman::ActivateTalisman()
{
    if (bHasConsumedTarget || IsHeldByPlayer())
    {
        return;
    }

    bClosedByPlayer = false;
    bTalismanActive = true;
    UpdateSlowTriggerState();
    UpdateActivationVisualState();

    TArray<AActor*> OverlappingActors;
    SlowTriggerComponent->GetOverlappingActors(OverlappingActors, APawn::StaticClass());
    for (AActor* OverlappingActor : OverlappingActors)
    {
        if (bHasConsumedTarget)
        {
            break;
        }

        TryAttachToOverlappingAI(OverlappingActor);
    }
}

void APickupActorAAASlowTalisman::DeactivateTalisman()
{
    bTalismanActive = false;
    UpdateSlowTriggerState();
    UpdateActivationVisualState();
}

void APickupActorAAASlowTalisman::UpdateSlowTriggerState()
{
    if (!SlowTriggerComponent)
    {
        return;
    }

    const bool bEnableTrigger = bTalismanActive && !bHasConsumedTarget && !IsHeldByPlayer();
    SlowTriggerComponent->SetGenerateOverlapEvents(bEnableTrigger);
    SlowTriggerComponent->SetCollisionEnabled(bEnableTrigger ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void APickupActorAAASlowTalisman::UpdateSlowTriggerRadius()
{
    if (SlowTriggerComponent)
    {
        SlowTriggerComponent->SetSphereRadius(FMath::Max(10.f, EffectRadius));
    }
}

void APickupActorAAASlowTalisman::ApplyThrowablePhysicsTuning()
{
    ApplyPickupPhysicsTuning();

    if (GlowLightComponent)
    {
        GlowLightComponent->SetLightColor(ActivationGlowColor);
        GlowLightComponent->SetAttenuationRadius(ActivationLightRadius);
    }
}

void APickupActorAAASlowTalisman::CacheOriginalMaterials()
{
    if (!MeshComponent || HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
    {
        return;
    }

    const int32 MaterialSlotCount = MeshComponent->GetNumMaterials();
    OriginalMaterials.SetNumZeroed(MaterialSlotCount);

    for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
    {
        if (!OriginalMaterials[MaterialIndex])
        {   
            OriginalMaterials[MaterialIndex] = MeshComponent->GetMaterial(MaterialIndex);
        }
    }
}

void APickupActorAAASlowTalisman::UpdateActivationVisualState()
{
    const bool bShouldGlow = bTalismanActive || bPendingGroundActivation || AttachedController.IsValid();
    const UWorld* World = GetWorld();

    if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || !World || !World->IsGameWorld())
    {
        if (GlowLightComponent)
        {
            GlowLightComponent->SetHiddenInGame(true);
            GlowLightComponent->SetVisibility(false);
            GlowLightComponent->SetIntensity(0.f);
        }

        return;
    }

    if (GlowLightComponent)
    {
        GlowLightComponent->SetHiddenInGame(!bShouldGlow);
        GlowLightComponent->SetVisibility(bShouldGlow);
        GlowLightComponent->SetIntensity(bShouldGlow ? ActivationLightIntensity : 0.f);
        GlowLightComponent->SetLightColor(ActivationGlowColor);
        GlowLightComponent->SetAttenuationRadius(ActivationLightRadius);
    }

    const float GlowScalar = bShouldGlow ? ActivationGlowIntensity : 0.f;
    const FLinearColor GoldTint(
        ActivationGlowColor.R,
        ActivationGlowColor.G,
        ActivationGlowColor.B,
        1.0f);

    if (!MeshComponent)
    {
        return;
    }

    CacheOriginalMaterials();

    const int32 MaterialSlotCount = MeshComponent->GetNumMaterials();
    for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
    {
        UMaterialInterface* OriginalMaterial = OriginalMaterials.IsValidIndex(MaterialIndex)
            ? OriginalMaterials[MaterialIndex].Get()
            : nullptr;

        if (!bShouldGlow)
        {
            if (OriginalMaterial)
            {
                MeshComponent->SetMaterial(MaterialIndex, OriginalMaterial);
            }

            continue;
        }

        UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(MaterialIndex));
        if (!IsValid(DynamicMaterial))
        {
            UMaterialInterface* BaseMaterial = OriginalMaterial ? OriginalMaterial : MeshComponent->GetMaterial(MaterialIndex);
            if (!BaseMaterial)
            {
                continue;
            }

            DynamicMaterial = MeshComponent->CreateDynamicMaterialInstance(MaterialIndex, BaseMaterial);
            if (!IsValid(DynamicMaterial))
            {
                continue;
            }
        }

        DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), ActivationGlowColor);
        DynamicMaterial->SetVectorParameterValue(TEXT("GlowColor"), ActivationGlowColor);
        DynamicMaterial->SetVectorParameterValue(TEXT("HighlightColor"), ActivationGlowColor);
        DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), GoldTint);
        DynamicMaterial->SetVectorParameterValue(TEXT("TintColor"), GoldTint);
        DynamicMaterial->SetVectorParameterValue(TEXT("ColorAndOpacity"), GoldTint);
        DynamicMaterial->SetVectorParameterValue(TEXT("TintColorAndOpacity"), GoldTint);
        DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveIntensity"), GlowScalar);
        DynamicMaterial->SetScalarParameterValue(TEXT("GlowIntensity"), GlowScalar);
        DynamicMaterial->SetScalarParameterValue(TEXT("HighlightIntensity"), GlowScalar);
        DynamicMaterial->SetScalarParameterValue(TEXT("GlowEnabled"), bShouldGlow ? 1.f : 0.f);
    }
}

void APickupActorAAASlowTalisman::RestoreDefaultThrowableCollision()
{
    if (!MeshComponent)
    {
        return;
    }

    ApplyReleasedCollisionProfile();
    MeshComponent->SetNotifyRigidBodyCollision(true);
}

void APickupActorAAASlowTalisman::StickToImpact(const FHitResult& Hit, UPrimitiveComponent* HitComponent)
{
    bAwaitingThrowImpact = false;
    FlightFlutterTime = 0.f;

    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    FVector ResolvedImpactPoint = Hit.ImpactPoint;
    FVector ResolvedImpactNormal = Hit.ImpactNormal;
    if (bHasPreferredStickTarget)
    {
        const bool bSameActor = PreferredStickActor.IsValid() && PreferredStickActor.Get() == Hit.GetActor();
        const bool bSameComponent = PreferredStickComponent.IsValid() && PreferredStickComponent.Get() == HitComponent;
        if (bSameActor || bSameComponent)
        {
            ResolvedImpactPoint = PreferredStickImpactPoint;
            if (!PreferredStickImpactNormal.IsNearlyZero())
            {
                ResolvedImpactNormal = PreferredStickImpactNormal;
            }
        }
    }

    ClearPreferredStickTarget();

    const FRotator StickRotation = ResolvedImpactNormal.IsNearlyZero()
        ? GetActorRotation()
        : (-ResolvedImpactNormal).Rotation();

    FVector LocalBoundsMin = FVector::ZeroVector;
    FVector LocalBoundsMax = FVector::ZeroVector;
    FVector MeshCenterOffset = FVector::ZeroVector;
    if (MeshComponent)
    {
        MeshComponent->GetLocalBounds(LocalBoundsMin, LocalBoundsMax);
        const FVector LocalBoundsCenter = (LocalBoundsMin + LocalBoundsMax) * 0.5f;
        const FVector MeshScale = MeshComponent->GetComponentScale();
        MeshCenterOffset = StickRotation.RotateVector(LocalBoundsCenter * MeshScale);
    }

    const FVector StickLocation = ResolvedImpactPoint - MeshCenterOffset;

    SetActorLocationAndRotation(StickLocation, StickRotation, false, nullptr, ETeleportType::TeleportPhysics);

    MeshComponent->SetSimulatePhysics(false);
    MeshComponent->SetEnableGravity(false);
    MeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
    MeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    if (IsValid(HitComponent))
    {
        AttachToComponent(HitComponent, FAttachmentTransformRules::KeepWorldTransform);
    }
}
 
void APickupActorAAASlowTalisman::TryAttachToOverlappingAI(AActor* OtherActor)
{
    if (!IsValid(OtherActor) || OtherActor == this)
    {
        return;
    }

    APawn* TargetPawn = Cast<APawn>(OtherActor);
    if (!IsValid(TargetPawn))
    {
        return;
    }

    AMyAIController* TargetController = Cast<AMyAIController>(TargetPawn->GetController());
    if (!TargetController)
    {
        return;
    }

    // 只有显形的鬼才能被慢符附着。
    if (!TargetController->IsGhostRevealedByEffect())
    {
        return;
    }

    AttachToAITarget(TargetPawn, TargetController);
}

void APickupActorAAASlowTalisman::ApplyRandomGhostAttachPose(APawn* TargetPawn, USceneComponent* AttachComponent)
{
    if (!MeshComponent || !IsValid(TargetPawn) || !IsValid(AttachComponent))
    {
        return;
    }

    FVector BoundsOrigin = FVector::ZeroVector;
    FVector BoundsExtent = FVector::ZeroVector;
    // TargetPawn->GetActorBounds(true, BoundsOrigin, BoundsExtent);
    if (const UPrimitiveComponent* AttachPrimitive = Cast<UPrimitiveComponent>(AttachComponent))
    {
        BoundsOrigin = AttachPrimitive->Bounds.Origin;
        BoundsExtent = AttachPrimitive->Bounds.BoxExtent;
    }
    else
    {
        TargetPawn->GetActorBounds(true, BoundsOrigin, BoundsExtent);
    }

    const float HorizontalExtent = FMath::Max(BoundsExtent.X, BoundsExtent.Y);
    const float HorizontalRadius = FMath::Max(18.f, HorizontalExtent * GhostAttachHorizontalRadiusRatio);
    const float VerticalMin = -BoundsExtent.Z + (BoundsExtent.Z * 2.f * GhostAttachVerticalMinRatio);
    const float VerticalMax = -BoundsExtent.Z + (BoundsExtent.Z * 2.f * GhostAttachVerticalMaxRatio);

    const float RandomYawDegrees = FMath::FRandRange(0.f, 360.f);
    const FVector HorizontalDirection = FRotator(0.f, RandomYawDegrees, 0.f).Vector().GetSafeNormal2D();
    const float HeightOffset = FMath::FRandRange(FMath::Min(VerticalMin, VerticalMax), FMath::Max(VerticalMin, VerticalMax));

    FVector SurfacePoint = BoundsOrigin + HorizontalDirection * HorizontalRadius;
    SurfacePoint.Z += HeightOffset;

    FVector SurfaceNormal = SurfacePoint - BoundsOrigin;
    if (SurfaceNormal.IsNearlyZero())
    {
        SurfaceNormal = -TargetPawn->GetActorForwardVector();
    }
    SurfaceNormal.Normalize();

    const FRotator AttachRotation = (-SurfaceNormal).Rotation();

    FVector LocalBoundsMin = FVector::ZeroVector;
    FVector LocalBoundsMax = FVector::ZeroVector;
    MeshComponent->GetLocalBounds(LocalBoundsMin, LocalBoundsMax);
    const FVector LocalBoundsCenter = (LocalBoundsMin + LocalBoundsMax) * 0.5f;
    const FVector MeshScale = MeshComponent->GetComponentScale();
    const FVector MeshCenterOffset = AttachRotation.RotateVector(LocalBoundsCenter * MeshScale);
    const FVector AttachLocation = SurfacePoint - MeshCenterOffset + SurfaceNormal * GhostAttachSurfacePadding;

    SetActorLocationAndRotation(AttachLocation, AttachRotation, false, nullptr, ETeleportType::TeleportPhysics);
    AttachToComponent(AttachComponent, FAttachmentTransformRules::KeepWorldTransform);
}

void APickupActorAAASlowTalisman::AttachToAITarget(
    APawn* TargetPawn,
    AMyAIController* TargetController,
    USceneComponent* OverrideAttachComponent)
{
    if (!IsValid(TargetPawn) || !TargetController || bHasConsumedTarget)
    {
        return;
    }

    bAwaitingThrowImpact = false;
    bHasConsumedTarget = true;
    bPendingGroundActivation = false;
    bClosedByPlayer = false;
    bTalismanActive = false;
    FlightFlutterTime = 0.f;
    ClearPreferredStickTarget();
    UpdateSlowTriggerState();

    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    MeshComponent->SetSimulatePhysics(false);
    MeshComponent->SetEnableGravity(false);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // USceneComponent* AttachComponent = TargetPawn->GetRootComponent();
    // if (const ACharacter* CharacterTarget = Cast<ACharacter>(TargetPawn))
    // {
    //     if (IsValid(CharacterTarget->GetMesh()))
    //     {
    //         AttachComponent = CharacterTarget->GetMesh();
    USceneComponent* AttachComponent = OverrideAttachComponent;
    if (!IsValid(AttachComponent))
    {
        AttachComponent = TargetPawn->GetRootComponent();
        if (const ACharacter* CharacterTarget = Cast<ACharacter>(TargetPawn))
        {
            if (IsValid(CharacterTarget->GetMesh()))
            {
                AttachComponent = CharacterTarget->GetMesh();
            }

        }
    }

    if (IsValid(AttachComponent))
    {
        ApplyRandomGhostAttachPose(TargetPawn, AttachComponent);
    }

    AttachedController = TargetController;
    AttachedTargetActor = TargetPawn;
    TargetController->ApplyAttachedDisableSource(this);
    TargetController->ApplySpeedReductionSource(this, SpeedReductionAmount);
    UpdateActivationVisualState();

    GetWorldTimerManager().ClearTimer(SlowDurationHandle);
    GetWorldTimerManager().SetTimer(
        SlowDurationHandle,
        this,
        &APickupActorAAASlowTalisman::ExpireAttachedSlow,
        FMath::Max(0.1f, SlowDuration),
        false);
}

void APickupActorAAASlowTalisman::ExpireAttachedSlow()
{
    FVector ImpactPoint = GetActorLocation();
    FVector ImpactNormal = FVector::UpVector;
    FVector InheritedVelocity = FVector::ZeroVector;
    UStaticMesh* ShardMesh = ResolveSlowTalismanShardMesh(MeshComponent);
    UMaterialInterface* ShardMaterial = nullptr;

    if (MeshComponent)
    {
        ImpactPoint = MeshComponent->Bounds.Origin;
        InheritedVelocity = MeshComponent->GetPhysicsLinearVelocity();
        ShardMaterial = OriginalMaterials.IsValidIndex(0) && OriginalMaterials[0].Get()
            ? OriginalMaterials[0].Get()
            : MeshComponent->GetMaterial(0);
    }

    if (AActor* AttachedTarget = AttachedTargetActor.Get())
    {
        InheritedVelocity = AttachedTarget->GetVelocity();
        ImpactNormal = (ImpactPoint - AttachedTarget->GetActorLocation()).GetSafeNormal();
    }

    if (ImpactNormal.IsNearlyZero())
    {
        ImpactNormal = GetActorUpVector();
    }

    if (ImpactNormal.IsNearlyZero())
    {
        ImpactNormal = FVector::UpVector;
    }

    ClearAttachedSlowEffect();

    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    if (MeshComponent)
    {
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        MeshComponent->SetSimulatePhysics(false);
        MeshComponent->SetEnableGravity(false);
        MeshComponent->SetHiddenInGame(true, true);
        MeshComponent->SetVisibility(false, true);
    }

    SpawnSlowTalismanShards(GetWorld(), ShardMesh, ShardMaterial, ImpactPoint, ImpactNormal, InheritedVelocity);
    Destroy();
}

void APickupActorAAASlowTalisman::ClearAttachedSlowEffect()
{
    if (AMyAIController* TargetController = AttachedController.Get())
    {
        TargetController->RemoveAttachedDisableSource(this);
        TargetController->RemoveSpeedReductionSource(this);
    }

    AttachedController.Reset();
    AttachedTargetActor.Reset();
    UpdateActivationVisualState();
}