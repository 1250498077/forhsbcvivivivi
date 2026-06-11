#include "GhostCharacter.h"

#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "MyAIController.h"
#include "PickupActorAAARuneInstrument.h"
#include "PickupActorAAASlowTalisman.h"

AGhostCharacter::AGhostCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    GhostAttachZoneComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("GhostAttachZoneComponent"));
    GhostAttachZoneComponent->SetupAttachment(GetMesh());
    GhostAttachZoneComponent->SetRelativeLocation(FVector(0.f, 0.f, 76.f));
    GhostAttachZoneComponent->SetCapsuleRadius(42.f);
    GhostAttachZoneComponent->SetCapsuleHalfHeight(72.f);
    ConfigureGhostAttachZoneCollision();
    GhostAttachZoneComponent->SetCanEverAffectNavigation(false);
    GhostAttachZoneComponent->SetHiddenInGame(true);
    GhostAttachZoneComponent->SetVisibility(true);
    GhostAttachZoneComponent->OnComponentBeginOverlap.AddDynamic(
        this,
        &AGhostCharacter::HandleGhostAttachZoneBeginOverlap);
}

void AGhostCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (GhostAttachZoneComponent)
    {
        ConfigureGhostAttachZoneCollision();
        UpdateGhostAttachZoneDebugVisibility();
    }
}

void AGhostCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bShowGhostAttachZoneDebug || !GhostAttachZoneComponent)
    {
        return;
    }

    DrawDebugCapsule(
        GetWorld(),
        GhostAttachZoneComponent->GetComponentLocation(),
        GhostAttachZoneComponent->GetScaledCapsuleHalfHeight(),
        GhostAttachZoneComponent->GetScaledCapsuleRadius(),
        GhostAttachZoneComponent->GetComponentQuat(),
        GhostAttachZoneDebugColor,
        false,
        0.f,
        0,
        2.f);
}

void AGhostCharacter::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    ConfigureGhostAttachZoneCollision();
    UpdateGhostAttachZoneDebugVisibility();
}

UCapsuleComponent* AGhostCharacter::GetGhostAttachZoneComponent() const
{
    return GhostAttachZoneComponent;
}

bool AGhostCharacter::IsWorldLocationInsideGhostAttachZone(const FVector& WorldLocation, float Tolerance) const
{
    if (!GhostAttachZoneComponent)
    {
        return false;
    }

    const FVector LocalLocation = GhostAttachZoneComponent->GetComponentTransform().InverseTransformPosition(WorldLocation);
    const float Radius = GhostAttachZoneComponent->GetScaledCapsuleRadius() + FMath::Max(0.f, Tolerance);
    const float HalfHeight = GhostAttachZoneComponent->GetScaledCapsuleHalfHeight() + FMath::Max(0.f, Tolerance);

    return FVector2D(LocalLocation.X, LocalLocation.Y).SizeSquared() <= FMath::Square(Radius)
        && FMath::Abs(LocalLocation.Z) <= HalfHeight;
}

void AGhostCharacter::ConfigureGhostAttachZoneCollision() const
{
    if (!GhostAttachZoneComponent)
    {
        return;
    }

    GhostAttachZoneComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GhostAttachZoneComponent->SetCollisionObjectType(ECC_WorldDynamic);
    GhostAttachZoneComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    GhostAttachZoneComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    GhostAttachZoneComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
    GhostAttachZoneComponent->SetGenerateOverlapEvents(true);
}

void AGhostCharacter::UpdateGhostAttachZoneDebugVisibility() const
{
    if (!GhostAttachZoneComponent)
    {
        return;
    }

    GhostAttachZoneComponent->SetHiddenInGame(!bShowGhostAttachZoneDebug);
    GhostAttachZoneComponent->SetVisibility(true);
    GhostAttachZoneComponent->ShapeColor = GhostAttachZoneDebugColor;
}

void AGhostCharacter::HandleGhostAttachZoneBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    (void)OverlappedComponent;
    (void)OtherComp;
    (void)OtherBodyIndex;
    (void)bFromSweep;
    (void)SweepResult;

    if (!IsValid(OtherActor) || OtherActor == this || !GhostAttachZoneComponent)
    {
        return;
    }

    AMyAIController* AIController = ResolveMyAIController();
    if (!IsValid(AIController) || !AIController->IsGhostRevealedByEffect())
    {
        return;
    }

    if (APickupActorAAASlowTalisman* SlowTalisman = Cast<APickupActorAAASlowTalisman>(OtherActor))
    {
        SlowTalisman->TryAttachToGhostZone(this, AIController, GhostAttachZoneComponent);
        return;
    }

    if (APickupActorAAARuneInstrument* RuneInstrument = Cast<APickupActorAAARuneInstrument>(OtherActor))
    {
        RuneInstrument->TryAttachToMatchedGhostZone(this, AIController, GhostAttachZoneComponent);
    }
}

AMyAIController* AGhostCharacter::ResolveMyAIController() const
{
    return Cast<AMyAIController>(GetController());
}