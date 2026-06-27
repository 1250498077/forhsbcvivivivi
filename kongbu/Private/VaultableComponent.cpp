#include "VaultableComponent.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UVaultableComponent::UVaultableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    SideAZoneComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("VaultSideAZoneComponent"));
    SideAZoneComponent->SetupAttachment(this);
    SideAZoneComponent->SetRelativeLocation(FVector(0.f, -90.f, 45.f));
    SideAZoneComponent->SetBoxExtent(FVector(90.f, 35.f, 80.f));

    SideBZoneComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("VaultSideBZoneComponent"));
    SideBZoneComponent->SetupAttachment(this);
    SideBZoneComponent->SetRelativeLocation(FVector(0.f, 90.f, 45.f));
    SideBZoneComponent->SetBoxExtent(FVector(90.f, 35.f, 80.f));

    ViewTargetBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("VaultViewTargetBoxComponent"));
    ViewTargetBoxComponent->SetupAttachment(this);
    ViewTargetBoxComponent->SetRelativeLocation(FVector(0.f, 0.f, 55.f));
    ViewTargetBoxComponent->SetBoxExtent(FVector(95.f, 55.f, 55.f));
}

void UVaultableComponent::OnRegister()
{
    Super::OnRegister();

    ConfigureBoxComponent(SideAZoneComponent, FColor::Green, false);
    ConfigureBoxComponent(SideBZoneComponent, FColor::Blue, false);
    ConfigureBoxComponent(ViewTargetBoxComponent, FColor::Yellow, true);
    UpdateDebugVisibility();
}

bool UVaultableComponent::CanVaultFromView(
    const ACharacter* Character,
    const FVector& ViewLocation,
    const FVector& ViewDirection,
    EVaultSide& OutStartSide,
    FVector& OutLandingLocation) const
{
    OutStartSide = EVaultSide::None;
    OutLandingLocation = FVector::ZeroVector;

    if (!IsValid(Character) || !ViewTargetBoxComponent)
    {
        return false;
    }

    const bool bInsideSideA = IsActorInsideBox(Character, SideAZoneComponent);
    const bool bInsideSideB = IsActorInsideBox(Character, SideBZoneComponent);
    if (bInsideSideA == bInsideSideB)
    {
        return false;
    }

    if (!IsViewFacingTarget(ViewLocation, ViewDirection))
    {
        return false;
    }

    if (bRequireViewTraceHit && !DoesViewTraceHitVaultTarget(Character, ViewLocation, ViewDirection))
    {
        return false;
    }

    OutStartSide = bInsideSideA ? EVaultSide::SideA : EVaultSide::SideB;
    OutLandingLocation = GetLandingLocationForSide(OutStartSide, Character);

    return IsLandingLocationClear(Character, OutLandingLocation);
}

FVector UVaultableComponent::GetLandingLocationForSide(EVaultSide StartSide, const ACharacter* Character) const
{
    const UBoxComponent* LandingZone = GetOppositeSideZone(StartSide);
    FVector LandingLocation = LandingZone ? LandingZone->GetComponentLocation() : GetComponentLocation();

    if (!bProjectLandingToGround || !GetWorld())
    {
        return LandingLocation;
    }

    float CapsuleHalfHeight = 88.f;
    if (IsValid(Character) && Character->GetCapsuleComponent())
    {
        float CapsuleRadius = 34.f;
        Character->GetCapsuleComponent()->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
    }

    const FVector TraceStart = LandingLocation + FVector(0.f, 0.f, GroundTraceUpDistance);
    const FVector TraceEnd = LandingLocation - FVector(0.f, 0.f, GroundTraceDownDistance);
    FHitResult GroundHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaultLandingGroundTrace), false);
    QueryParams.AddIgnoredActor(Character);
    QueryParams.AddIgnoredActor(GetOwner());

    if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams) && GroundHit.bBlockingHit)
    {
        LandingLocation.Z = GroundHit.ImpactPoint.Z + CapsuleHalfHeight;
    }

    return LandingLocation;
}

void UVaultableComponent::ConfigureBoxComponent(UBoxComponent* BoxComponent, const FColor& ShapeColor, bool bBlockVisibility) const
{
    if (!BoxComponent)
    {
        return;
    }

    BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BoxComponent->SetCollisionObjectType(ECC_WorldDynamic);
    BoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    BoxComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    if (bBlockVisibility)
    {
        BoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    }
    BoxComponent->SetGenerateOverlapEvents(true);
    BoxComponent->SetHiddenInGame(!bShowDebugZones);
    BoxComponent->SetVisibility(bShowDebugZones);
    BoxComponent->ShapeColor = ShapeColor;
}

void UVaultableComponent::UpdateDebugVisibility() const
{
    const UBoxComponent* Boxes[] = { SideAZoneComponent, SideBZoneComponent, ViewTargetBoxComponent };
    for (const UBoxComponent* BoxComponent : Boxes)
    {
        if (UBoxComponent* MutableBoxComponent = const_cast<UBoxComponent*>(BoxComponent))
        {
            MutableBoxComponent->SetHiddenInGame(!bShowDebugZones);
            MutableBoxComponent->SetVisibility(bShowDebugZones);
        }
    }
}

bool UVaultableComponent::IsActorInsideBox(const AActor* Actor, const UBoxComponent* BoxComponent) const
{
    if (!IsValid(Actor) || !BoxComponent)
    {
        return false;
    }

    const FVector LocalLocation = BoxComponent->GetComponentTransform().InverseTransformPosition(Actor->GetActorLocation());
    const FVector BoxExtent = BoxComponent->GetUnscaledBoxExtent();

    return FMath::Abs(LocalLocation.X) <= BoxExtent.X
        && FMath::Abs(LocalLocation.Y) <= BoxExtent.Y
        && FMath::Abs(LocalLocation.Z) <= BoxExtent.Z;
}

bool UVaultableComponent::IsViewFacingTarget(const FVector& ViewLocation, const FVector& ViewDirection) const
{
    if (!ViewTargetBoxComponent)
    {
        return false;
    }

    const FVector DirectionToTarget = (ViewTargetBoxComponent->GetComponentLocation() - ViewLocation).GetSafeNormal();
    if (DirectionToTarget.IsNearlyZero())
    {
        return true;
    }

    return FVector::DotProduct(ViewDirection.GetSafeNormal(), DirectionToTarget) >= FacingDotThreshold;
}

bool UVaultableComponent::DoesViewTraceHitVaultTarget(const AActor* Viewer, const FVector& ViewLocation, const FVector& ViewDirection) const
{
    if (!GetWorld())
    {
        return false;
    }

    FHitResult HitResult;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaultViewTrace), false);
    QueryParams.AddIgnoredActor(Viewer);

    const FVector TraceEnd = ViewLocation + ViewDirection.GetSafeNormal() * ViewTraceDistance;
    if (!GetWorld()->LineTraceSingleByChannel(HitResult, ViewLocation, TraceEnd, ECC_Visibility, QueryParams) || !HitResult.bBlockingHit)
    {
        return false;
    }

    return HitResult.GetActor() == GetOwner() || HitResult.GetComponent() == ViewTargetBoxComponent;
}

bool UVaultableComponent::IsLandingLocationClear(const ACharacter* Character, const FVector& LandingLocation) const
{
    if (!bCheckLandingCapsule || !IsValid(Character) || !Character->GetCapsuleComponent() || !GetWorld())
    {
        return true;
    }

    float CapsuleRadius = 34.f;
    float CapsuleHalfHeight = 88.f;
    Character->GetCapsuleComponent()->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VaultLandingCapsuleCheck), false);
    QueryParams.AddIgnoredActor(Character);
    QueryParams.AddIgnoredActor(GetOwner());

    return !GetWorld()->OverlapBlockingTestByChannel(
        LandingLocation,
        Character->GetActorQuat(),
        ECC_Pawn,
        FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
        QueryParams);
}

const UBoxComponent* UVaultableComponent::GetOppositeSideZone(EVaultSide StartSide) const
{
    if (StartSide == EVaultSide::SideA)
    {
        return SideBZoneComponent;
    }

    if (StartSide == EVaultSide::SideB)
    {
        return SideAZoneComponent;
    }

    return nullptr;
}