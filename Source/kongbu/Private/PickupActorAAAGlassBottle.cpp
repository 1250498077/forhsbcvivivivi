#include "PickupActorAAAGlassBottle.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MyAIController.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

// 该文件实现玻璃瓶的运行时逻辑：
// - 管理动态材质与视觉参数
// - 应用可调物理质量
// - 响应玩家的拾取/放下/投掷交互
// - 在合适条件下把瓶子碎裂成碎片并施加物理冲量

APickupActorAAAGlassBottle::APickupActorAAAGlassBottle()
{
    PrimaryActorTick.bCanEverTick = false;

    HoldType = EHoldItemType::Bottle;
    FP_SocketName = TEXT("RightSocket");
    FP_LocationOffset = FVector::ZeroVector;
    FP_RotationOffset = FRotator::ZeroRotator;
    TP_SocketName = TEXT("RightSocket");
    TP_LocationOffset = FVector::ZeroVector;
    TP_RotationOffset = FRotator::ZeroRotator;

    ItemMassKg = 0.55f;
    ItemThrowForceMultiplier = 0.8f;
    ItemLinearDamping = 0.04f;
    ItemAngularDamping = 0.15f;
    ItemThrowSpinRateDegrees = 2200.f;

    Tags.Add(FName("Bottle"));
    Tags.Add(FName("Pickup"));
    Tags.Add(FName("Breakable"));
    Tags.Add(FName("Glass"));

    if (MeshComponent)
    {
        // 开启刚体碰撞通知，注册命中回调以便在撞击时判断是否碎裂。
        MeshComponent->SetNotifyRigidBodyCollision(true);
        // 高速物体开启连续碰撞检测，减少穿透漏检。
        MeshComponent->BodyInstance.bUseCCD = true;
        MeshComponent->OnComponentHit.AddDynamic(this, &APickupActorAAAGlassBottle::HandleBottleHit);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> PrimaryGlassMaterial(
        TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent.Widget3DPassThrough_Translucent"));
    if (PrimaryGlassMaterial.Succeeded())
    {
        DefaultGlassMaterial = PrimaryGlassMaterial.Object;
    }
    else
    {
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> SecondaryGlassMaterial(
            TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent_OneSided.Widget3DPassThrough_Translucent_OneSided"));
        if (SecondaryGlassMaterial.Succeeded())
        {
            DefaultGlassMaterial = SecondaryGlassMaterial.Object;
        }
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultCubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (DefaultCubeMesh.Succeeded())
    {
        DefaultShardMesh = DefaultCubeMesh.Object;
    }
}

void APickupActorAAAGlassBottle::BeginPlay()
{
    Super::BeginPlay();
    ApplyBottlePhysicsTuning();
    ApplyGlassMaterial();
}

void APickupActorAAAGlassBottle::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyBottlePhysicsTuning();
    ApplyGlassMaterial();
}

void APickupActorAAAGlassBottle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DisarmBottleShatter();
    Super::EndPlay(EndPlayReason);
}

void APickupActorAAAGlassBottle::OnPickedUp()
{
    Super::OnPickedUp();

    DisarmBottleShatter();

    if (MeshComponent)
    {
        MeshComponent->SetHiddenInGame(false, true);
        MeshComponent->SetVisibility(true, true);
    }
}

void APickupActorAAAGlassBottle::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    Super::OnPutDown(PlaceLocation, PlaceRotation);

    DisarmBottleShatter();

    if (MeshComponent)
    {
        MeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }
}

void APickupActorAAAGlassBottle::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    Super::OnThrown(ThrowDirection, ThrowForce);

    if (!MeshComponent)
    {
        return;
    }

    bHasShattered = false;
    DisarmBottleShatter();

    // FVector SpinAxis = ThrowSpinAxisBias;
    // SpinAxis.X += FMath::FRandRange(-0.25f, 0.25f);
    // SpinAxis.Y += FMath::FRandRange(-0.25f, 0.25f);
    // SpinAxis.Z += FMath::FRandRange(-0.25f, 0.25f);
    // SpinAxis = SpinAxis.GetSafeNormal();
    // if (SpinAxis.IsNearlyZero())
    // {
    //     SpinAxis = FVector::RightVector;
    // }

    // MeshComponent->SetPhysicsAngularVelocityInDegrees(SpinAxis * ThrowSpinRateDegrees, false);
    MeshComponent->WakeAllRigidBodies();

    if (ShatterArmDelay <= 0.f)
    {
        ArmBottleForImpactShatter();
        return;
    }

    GetWorldTimerManager().SetTimer(
        ShatterArmHandle,
        this,
        &APickupActorAAAGlassBottle::ArmBottleForImpactShatter,
        ShatterArmDelay,
        false);
}

void APickupActorAAAGlassBottle::HandleBottleHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    if (!bCanShatterOnImpact || bHasShattered || HitComponent != MeshComponent)
    {
        return;
    }

    if (!Hit.bBlockingHit)
    {
        return;
    }

    if (!IsValid(OtherActor) || OtherActor == this)
    {
        return;
    }

    // 只有显形的鬼才能被瓶子击中；隐形时瓶子穿过不碎。
    if (APawn* HitPawn = Cast<APawn>(OtherActor))
    {
        if (AMyAIController* AIController = Cast<AMyAIController>(HitPawn->GetController()))
        {
            if (!AIController->IsGhostRevealedByEffect())
            {
                return;
            }
        }
    }

    ShatterBottle(Hit, NormalImpulse);
}

void APickupActorAAAGlassBottle::ApplyBottlePhysicsTuning()
{
    // if (!MeshComponent)
    // {
    //     return;
    // }

    // MeshComponent->SetMassOverrideInKg(NAME_None, BottleMassKg, true);
    ApplyPickupPhysicsTuning();
}

void APickupActorAAAGlassBottle::ApplyGlassMaterial()
{
    if (!MeshComponent)
    {
        return;
    }

    UMaterialInterface* GlassMaterial = ResolveGlassMaterial();
    if (!GlassMaterial)
    {
        return;
    }

    GlassMaterialInstance = UMaterialInstanceDynamic::Create(GlassMaterial, this);
    if (!GlassMaterialInstance)
    {
        return;
    }

    const FLinearColor TintWithOpacity(GlassTint.R, GlassTint.G, GlassTint.B, GlassOpacity);
    GlassMaterialInstance->SetVectorParameterValue(TEXT("TintColorAndOpacity"), TintWithOpacity);
    GlassMaterialInstance->SetVectorParameterValue(TEXT("TintColor"), TintWithOpacity);
    GlassMaterialInstance->SetVectorParameterValue(TEXT("ColorAndOpacity"), TintWithOpacity);
    GlassMaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(GlassTint.R, GlassTint.G, GlassTint.B));
    GlassMaterialInstance->SetVectorParameterValue(TEXT("GlassColor"), FLinearColor(GlassTint.R, GlassTint.G, GlassTint.B));
    GlassMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), GlassOpacity);
    GlassMaterialInstance->SetScalarParameterValue(TEXT("Alpha"), GlassOpacity);
    GlassMaterialInstance->SetScalarParameterValue(TEXT("Transparency"), 1.f - GlassOpacity);
    GlassMaterialInstance->SetScalarParameterValue(TEXT("OpacityFromTexture"), GlassOpacity);
    GlassMaterialInstance->SetScalarParameterValue(TEXT("Refraction"), 1.02f);
    GlassMaterialInstance->SetScalarParameterValue(TEXT("Roughness"), 0.08f);
    GlassMaterialInstance->SetScalarParameterValue(TEXT("Specular"), 0.9f);

    const int32 MaterialSlotCount = FMath::Max(1, MeshComponent->GetNumMaterials());
    for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
    {
        MeshComponent->SetMaterial(MaterialIndex, GlassMaterialInstance);
    }
}

void APickupActorAAAGlassBottle::ArmBottleForImpactShatter()
{
    bCanShatterOnImpact = true;
}

void APickupActorAAAGlassBottle::DisarmBottleShatter()
{
    bCanShatterOnImpact = false;
    GetWorldTimerManager().ClearTimer(ShatterArmHandle);
}

void APickupActorAAAGlassBottle::ShatterBottle(const FHitResult& Hit, const FVector& NormalImpulse)
{
    if (!MeshComponent || bHasShattered)
    {
        return;
    }

    bHasShattered = true;
    DisarmBottleShatter();

    const FVector BottleVelocity = MeshComponent->GetPhysicsLinearVelocity();
    const FVector ImpactPoint = Hit.bBlockingHit
        ? FVector(Hit.ImpactPoint)
        : GetActorLocation();

    FVector ImpactNormal = Hit.bBlockingHit ? Hit.ImpactNormal : FVector::UpVector;
    if (ImpactNormal.IsNearlyZero() && !NormalImpulse.IsNearlyZero())
    {
        ImpactNormal = -NormalImpulse.GetSafeNormal();
    }
    if (ImpactNormal.IsNearlyZero())
    {
        ImpactNormal = FVector::UpVector;
    }

    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetSimulatePhysics(false);
    MeshComponent->SetEnableGravity(false);
    MeshComponent->SetHiddenInGame(true, true);
    MeshComponent->SetVisibility(false, true);

    SpawnGlassShards(ImpactPoint, ImpactNormal, BottleVelocity);

    UE_LOG(LogTemp, Warning, TEXT("%s shattered on impact with %s"),
        *GetName(),
        *GetNameSafe(Hit.GetActor()));

    Destroy();
}

void APickupActorAAAGlassBottle::SpawnGlassShards(
    const FVector& ImpactPoint,
    const FVector& ImpactNormal,
    const FVector& BottleVelocity)
{
    UWorld* World = GetWorld();
    UStaticMesh* ShardMesh = ResolveShardMesh();
    if (!World || !ShardMesh)
    {
        return;
    }

    const FVector SafeImpactNormal = ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
    FVector TangentX;
    FVector TangentY;
    SafeImpactNormal.FindBestAxisVectors(TangentX, TangentY);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    UMaterialInterface* ShardMaterial = GlassMaterialInstance ? Cast<UMaterialInterface>(GlassMaterialInstance) : ResolveGlassMaterial();

    for (int32 ShardIndex = 0; ShardIndex < ShardCount; ++ShardIndex)
    {
        const FVector RandomOffset = FMath::VRand() * FMath::FRandRange(2.f, 12.f);
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

        const FVector RandomExtent(
            FMath::FRandRange(ShardBoxExtentMin.X, ShardBoxExtentMax.X),
            FMath::FRandRange(ShardBoxExtentMin.Y, ShardBoxExtentMax.Y),
            FMath::FRandRange(ShardBoxExtentMin.Z, ShardBoxExtentMax.Z));
        ShardMeshComponent->SetWorldScale3D(RandomExtent / 50.f);

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
            ScatterDirection * (ShardBurstImpulse * FMath::FRandRange(0.75f, 1.35f)) +
            BottleVelocity * ImpactVelocityInheritance;

        ShardMeshComponent->WakeAllRigidBodies();
        ShardMeshComponent->AddImpulse(Impulse, NAME_None, false);
        ShardMeshComponent->AddAngularImpulseInDegrees(
            FMath::VRand() * FMath::FRandRange(400.f, 1400.f),
            NAME_None,
            false);

        ShardActor->SetLifeSpan(ShardLifetime);
    }
}

UMaterialInterface* APickupActorAAAGlassBottle::ResolveGlassMaterial() const
{
    return GlassMaterialOverride ? GlassMaterialOverride : DefaultGlassMaterial;
}

UStaticMesh* APickupActorAAAGlassBottle::ResolveShardMesh() const
{
    return ShardMeshOverride ? ShardMeshOverride : DefaultShardMesh;
}