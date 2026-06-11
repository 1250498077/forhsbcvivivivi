#include "PickupActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

APickupActor::APickupActor()
{
    bReplicates = true;
    SetReplicateMovement(true);

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    RootComponent = MeshComponent;
    MeshComponent->SetIsReplicated(true);
    MeshComponent->SetVisibleInRayTracing(true);

    VisualMeshRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("VisualMeshRootComponent"));
    VisualMeshRootComponent->SetupAttachment(MeshComponent);

    // 默认按可拾取物来配置：可移动、带物理、受重力影响，并阻挡常规碰撞。
    MeshComponent->SetMobility(EComponentMobility::Movable);
    // 道具不参与 NavMesh 阻挡，避免鬼在巡逻/追逐时把它们当成寻路障碍。
    MeshComponent->SetCanEverAffectNavigation(false);
    MeshComponent->SetSimulatePhysics(true);
    MeshComponent->SetEnableGravity(true);
    ApplyReleasedCollisionProfile();
}

void APickupActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(APickupActor, bIsHeldByPlayer);
    DOREPLIFETIME(APickupActor, bDisabledByRage);
}

void APickupActor::BeginPlay()
{
    Super::BeginPlay();
    CacheInitialActorScale();
    ApplyPickupPhysicsTuning();
    ConfigureAttachedDisplayMeshes();
}

void APickupActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    CacheInitialActorScale();
    ApplyPickupPhysicsTuning();
    ConfigureAttachedDisplayMeshes();
}

void APickupActor::ApplyReleasedCollisionProfile()
{
    if (!MeshComponent)
    {
        return;
    }

    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetCollisionObjectType(ECC_PhysicsBody);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
    MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, bAllowPawnCollision ? ECR_Block : ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, bAllowPhysicsBodyCollision ? ECR_Block : ECR_Ignore);
    MeshComponent->SetGenerateOverlapEvents(true);
}

void APickupActor::ApplyPickupPhysicsTuning()
{
    if (!MeshComponent)
    {
        return;
    }

    MeshComponent->SetMassOverrideInKg(NAME_None, FMath::Max(0.01f, ItemMassKg), true);
    MeshComponent->SetLinearDamping(FMath::Max(0.f, ItemLinearDamping));
    MeshComponent->SetAngularDamping(FMath::Max(0.f, ItemAngularDamping));
    MeshComponent->SetPhysMaterialOverride(ItemPhysicalMaterial.Get());
}

FVector APickupActor::BuildThrowAngularVelocityInDegrees() const
{
    if (!bApplyThrowSpin || ItemThrowSpinRateDegrees <= 0.f)
    {
        return FVector::ZeroVector;
    }

    FVector SpinAxis = ItemThrowSpinAxisBias;
    if (bRandomizeThrowSpinAxis)
    {
        SpinAxis.X += FMath::FRandRange(-0.25f, 0.25f);
        SpinAxis.Y += FMath::FRandRange(-0.25f, 0.25f);
        SpinAxis.Z += FMath::FRandRange(-0.25f, 0.25f);
    }

    if (SpinAxis.IsNearlyZero())
    {
        SpinAxis = FVector::RightVector;
    }

    return SpinAxis.GetSafeNormal() * ItemThrowSpinRateDegrees;
}

void APickupActor::CacheInitialActorScale()
{
    CachedInitialActorScale3D = GetActorScale3D();
    bHasCachedInitialActorScale = true;
}

void APickupActor::RestoreInitialActorScale()
{
    if (!bHasCachedInitialActorScale)
    {
        return;
    }

    SetActorScale3D(CachedInitialActorScale3D);
}

void APickupActor::ConfigureAttachedDisplayMeshes()
{
    TArray<UStaticMeshComponent*> DisplayMeshComponents;
    GatherAttachedDisplayMeshComponents(DisplayMeshComponents);

    for (UStaticMeshComponent* DisplayMeshComponent : DisplayMeshComponents)
    {
        ConfigureDisplayMeshComponent(DisplayMeshComponent);
    }
}

void APickupActor::ConfigureDisplayMeshComponent(UStaticMeshComponent* DisplayMeshComponent) const
{
    if (!DisplayMeshComponent || DisplayMeshComponent == MeshComponent)
    {
        return;
    }

    DisplayMeshComponent->SetMobility(EComponentMobility::Movable);
    DisplayMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DisplayMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    DisplayMeshComponent->SetGenerateOverlapEvents(false);
    DisplayMeshComponent->SetCanEverAffectNavigation(false);
    DisplayMeshComponent->SetNotifyRigidBodyCollision(false);
    DisplayMeshComponent->SetSimulatePhysics(false);
    DisplayMeshComponent->SetEnableGravity(false);
    DisplayMeshComponent->SetVisibleInRayTracing(true);
}

void APickupActor::GatherAttachedDisplayMeshComponents(TArray<UStaticMeshComponent*>& OutDisplayMeshComponents) const
{
    OutDisplayMeshComponents.Reset();

    if (!VisualMeshRootComponent)
    {
        return;
    }

    TArray<USceneComponent*> ChildComponents;
    VisualMeshRootComponent->GetChildrenComponents(true, ChildComponents);

    for (USceneComponent* ChildComponent : ChildComponents)
    {
        UStaticMeshComponent* DisplayMeshComponent = Cast<UStaticMeshComponent>(ChildComponent);
        if (!DisplayMeshComponent || DisplayMeshComponent == MeshComponent)
        {
            continue;
        }

        OutDisplayMeshComponents.Add(DisplayMeshComponent);
    }
}

void APickupActor::OnRep_IsHeldByPlayer()
{
    if (!MeshComponent)
    {
        return;
    }
    RestoreInitialActorScale();

    if (bIsHeldByPlayer)
    {
        MeshComponent->SetSimulatePhysics(false);
        MeshComponent->SetEnableGravity(false);
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    else
    {
        ApplyReleasedCollisionProfile();
        MeshComponent->SetSimulatePhysics(true);
        MeshComponent->SetEnableGravity(true);
    }

    ConfigureAttachedDisplayMeshes();
}

void APickupActor::OnPickedUp()
{
    // 先脱离当前附着关系，再彻底关闭物理和碰撞，避免拿在手里还和世界发生碰撞。
    bIsHeldByPlayer = true;
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    RestoreInitialActorScale();
    MeshComponent->SetSimulatePhysics(false);
    MeshComponent->SetEnableGravity(false);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetGenerateOverlapEvents(false);
    ConfigureAttachedDisplayMeshes();
}

void APickupActor::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    // 放下时回到世界空间，并恢复基础物理状态。
    bIsHeldByPlayer = false;
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    RestoreInitialActorScale();
    SetActorLocation(PlaceLocation);
    SetActorRotation(PlaceRotation);
    ApplyReleasedCollisionProfile();
    ApplyPickupPhysicsTuning();
    MeshComponent->SetSimulatePhysics(true);
    MeshComponent->SetEnableGravity(true);
    ConfigureAttachedDisplayMeshes();
}

// 真正的投掷时机由 PlayerController 控制，这里只负责恢复物理并施加冲量。
void APickupActor::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    bIsHeldByPlayer = false;
    // GetRootComponent()->DetachFromComponent(
    //     FDetachmentTransformRules::KeepWorldTransform
    // );
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    RestoreInitialActorScale();
    ApplyReleasedCollisionProfile();
    ApplyPickupPhysicsTuning();
    MeshComponent->SetSimulatePhysics(true);
    MeshComponent->SetEnableGravity(true);
    ConfigureAttachedDisplayMeshes();
    MeshComponent->WakeAllRigidBodies();
    FVector Impulse = ThrowDirection * ThrowForce * ItemThrowForceMultiplier;
    const FVector AngularVelocity = BuildThrowAngularVelocityInDegrees();
    // 延到下一帧再加冲量，避免和刚恢复物理/分离附着的时机互相干扰。
    GetWorld()->GetTimerManager().SetTimerForNextTick([this, Impulse, AngularVelocity]()
    {
        UE_LOG(LogTemp, Warning, TEXT("Throw Impulse: %s"), *Impulse.ToString());
        MeshComponent->AddImpulse(Impulse, NAME_None, false);
        if (!AngularVelocity.IsNearlyZero())
        {
            MeshComponent->SetPhysicsAngularVelocityInDegrees(AngularVelocity, false);
        }
    });
}

bool APickupActor::CanBeClosedByPlayer_Implementation() const
{
    return false;
}

void APickupActor::CloseByPlayer_Implementation(AActor* ClosingActor)
{
}

bool APickupActor::IsClosedByPlayer_Implementation() const
{
    return false;
}

void APickupActor::OpenByPlayer_Implementation(AActor* OpeningActor)
{
}

bool APickupActor::CanBeDisabledByRage_Implementation() const
{
    return bCanBeDisabledByRage && !bDisabledByRage;
}

void APickupActor::DisableByRage_Implementation(AActor* DisablingActor)
{
    if (!CanBeDisabledByRage_Implementation())
    {
        return;
    }

    bDisabledByRage = true;

    UE_LOG(LogTemp, Log, TEXT("%s disabled by Rage source %s"),
        *GetName(),
        *GetNameSafe(DisablingActor));
}

void APickupActor::RestoreAfterRageDisable_Implementation()
{
    bDisabledByRage = false;
}

bool APickupActor::CanBeClosedByPlayerNative() const
{
    return CanBeClosedByPlayer_Implementation();
}

void APickupActor::CloseByPlayerNative(AActor* ClosingActor)
{
    CloseByPlayer_Implementation(ClosingActor);
}

bool APickupActor::IsClosedByPlayerNative() const
{
    return IsClosedByPlayer_Implementation();
}

void APickupActor::OpenByPlayerNative(AActor* OpeningActor)
{
    OpenByPlayer_Implementation(OpeningActor);
}

bool APickupActor::CanBeDisabledByRageNative() const
{
    return CanBeDisabledByRage_Implementation();
}

void APickupActor::DisableByRageNative(AActor* DisablingActor)
{
    DisableByRage_Implementation(DisablingActor);
}

void APickupActor::RestoreAfterRageDisableNative()
{
    RestoreAfterRageDisable_Implementation();
}