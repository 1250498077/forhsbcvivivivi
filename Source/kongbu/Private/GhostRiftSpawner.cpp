#include "GhostRiftSpawner.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "GhostSerpentVFXActor.h"
#include "Materials/MaterialInstanceDynamic.h"

AGhostRiftSpawner::AGhostRiftSpawner()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRootComponent"));
    RootComponent = SceneRootComponent;

    MainRiftMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MainRiftMeshComponent"));
    MainRiftMeshComponent->SetupAttachment(SceneRootComponent);
    MainRiftMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MainRiftMeshComponent->SetGenerateOverlapEvents(false);
    MainRiftMeshComponent->SetCanEverAffectNavigation(false);

    ChildRiftRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ChildRiftRootComponent"));
    ChildRiftRootComponent->SetupAttachment(SceneRootComponent);
}

void AGhostRiftSpawner::BeginPlay()
{
    Super::BeginPlay();
    RestartRiftGrowth();
    SpawnTimeRemaining = bAutoSpawn && !bSpawnSerpentsOnlyAfterMainGrowth ? 0.f : GetNextSpawnDelay();
}

void AGhostRiftSpawner::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    InitializeMainRiftVisual();

    if (MainRiftMeshComponent)
    {
        MainRiftMeshComponent->SetWorldScale3D(FVector(MainRiftFinalScale));
    }
}

void AGhostRiftSpawner::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    CleanupActiveSerpents();
    UpdateRiftGrowth(DeltaTime);

    if (bShowDebugSpawnRadius && GetWorld())
    {
        DrawDebugSphere(GetWorld(), GetActorLocation(), SpawnRadius, 16, FColor::Purple, false, 0.f, 0, 1.5f);
    }

    if (bSpawnSerpentsOnlyAfterMainGrowth && !bMainRiftFinishedGrowing)
    {
        return;
    }

    if (!bAutoSpawn)
    {
        return;
    }

    if (TotalSpawnCount > 0 && SpawnedCount >= TotalSpawnCount)
    {
        return;
    }

    if (MaxActiveSerpents > 0 && ActiveSerpents.Num() >= MaxActiveSerpents)
    {
        return;
    }

    SpawnTimeRemaining -= DeltaTime;
    if (SpawnTimeRemaining > 0.f)
    {
        return;
    }

    SpawnSerpent();
    SpawnTimeRemaining = GetNextSpawnDelay();
}

AGhostSerpentVFXActor* AGhostRiftSpawner::SpawnSerpent()
{
    if (!GetWorld() || !SerpentClass || !IsValid(TargetActor))
    {
        return nullptr;
    }

    CleanupActiveSerpents();
    if (MaxActiveSerpents > 0 && ActiveSerpents.Num() >= MaxActiveSerpents)
    {
        return nullptr;
    }

    const FVector SpawnLocation = GetSpawnLocation();
    const FRotator SpawnRotation = (TargetActor->GetActorLocation() - SpawnLocation).Rotation();

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = this;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AGhostSerpentVFXActor* Serpent = GetWorld()->SpawnActor<AGhostSerpentVFXActor>(
        SerpentClass,
        SpawnLocation,
        SpawnRotation,
        SpawnParameters);

    if (!IsValid(Serpent))
    {
        return nullptr;
    }

    Serpent->OnSerpentAbsorbed.AddDynamic(this, &AGhostRiftSpawner::HandleSerpentAbsorbed);
    if (Serpent->bUseAutonomousBehavior)
    {
        Serpent->StartAutonomousFromLocation(SpawnLocation);
    }
    else
    {
        Serpent->StartFlightFromLocation(SpawnLocation, TargetActor, TargetSocketName);
    }

    ActiveSerpents.Add(Serpent);
    ++SpawnedCount;
    return Serpent;
}

void AGhostRiftSpawner::StartSpawning()
{
    bAutoSpawn = true;
    SpawnTimeRemaining = 0.f;
}

void AGhostRiftSpawner::StopSpawning()
{
    bAutoSpawn = false;
}

void AGhostRiftSpawner::RestartRiftGrowth()
{
    for (FRuntimeRiftCrack& Crack : RiftCracks)
    {
        if (Crack.MeshComponent && Crack.MeshComponent != MainRiftMeshComponent)
        {
            Crack.MeshComponent->DestroyComponent();
        }
    }

    RiftCracks.Reset();
    SpawnedChildRiftCount = 0;
    ChildRiftBurstTimeRemaining = ChildRiftBurstInterval;
    bMainRiftFinishedGrowing = !bGrowRiftOnBeginPlay;

    InitializeMainRiftVisual();

    if (MainRiftMeshComponent)
    {
        const FVector MainTargetScale(MainRiftFinalScale);
        FRuntimeRiftCrack* MainCrack = AddRuntimeRiftCrack(MainRiftMeshComponent, MainTargetScale, MainRiftGrowthDuration);
        if (MainCrack)
        {
            MainCrack->bFinishedGrowing = !bGrowRiftOnBeginPlay;
            MainCrack->Age = bGrowRiftOnBeginPlay ? 0.f : MainRiftGrowthDuration;
            const float StartScale = bGrowRiftOnBeginPlay ? MainRiftStartScale : MainRiftFinalScale;
            MainRiftMeshComponent->SetWorldScale3D(FVector(StartScale));
            if (MainCrack->DynamicMaterial && !GrowthMaterialScalarName.IsNone())
            {
                MainCrack->DynamicMaterial->SetScalarParameterValue(GrowthMaterialScalarName, bGrowRiftOnBeginPlay ? 0.f : 1.f);
            }
        }
    }
}

void AGhostRiftSpawner::CleanupActiveSerpents()
{
    ActiveSerpents.RemoveAll(
        [](const TWeakObjectPtr<AGhostSerpentVFXActor>& Entry)
        {
            return !Entry.IsValid();
        });
}

void AGhostRiftSpawner::InitializeMainRiftVisual()
{
    if (!MainRiftMeshComponent)
    {
        return;
    }

    if (RiftStaticMesh)
    {
        MainRiftMeshComponent->SetStaticMesh(RiftStaticMesh);
    }

    ApplyRiftMaterial(MainRiftMeshComponent);
    MainRiftMeshComponent->SetVisibility(true, true);

    if (bRandomizeMainRiftRotation)
    {
        MainRiftMeshComponent->SetRelativeRotation(GetRandomRotationInRange(MainRiftRandomRotationRange));
    }
}

void AGhostRiftSpawner::UpdateRiftGrowth(float DeltaTime)
{
    bool bMainFinishedThisFrame = false;

    for (int32 CrackIndex = 0; CrackIndex < RiftCracks.Num(); ++CrackIndex)
    {
        FRuntimeRiftCrack& Crack = RiftCracks[CrackIndex];
        if (!Crack.MeshComponent || Crack.bFinishedGrowing)
        {
            continue;
        }

        Crack.Age += DeltaTime;
        const float Alpha = Crack.GrowthDuration <= KINDA_SMALL_NUMBER
            ? 1.f
            : FMath::Clamp(Crack.Age / Crack.GrowthDuration, 0.f, 1.f);
        const float SmoothAlpha = FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.2f);

        Crack.MeshComponent->SetWorldScale3D(Crack.TargetScale * SmoothAlpha);
        if (Crack.DynamicMaterial && !GrowthMaterialScalarName.IsNone())
        {
            Crack.DynamicMaterial->SetScalarParameterValue(GrowthMaterialScalarName, SmoothAlpha);
        }

        if (Alpha >= 1.f)
        {
            Crack.bFinishedGrowing = true;
            if (Crack.MeshComponent == MainRiftMeshComponent && !bMainRiftFinishedGrowing)
            {
                bMainRiftFinishedGrowing = true;
                bMainFinishedThisFrame = true;
            }
        }
    }

    if (bMainFinishedThisFrame)
    {
        ChildRiftBurstTimeRemaining = 0.f;
    }

    if (!bEnableChildRiftBranching || !bMainRiftFinishedGrowing || SpawnedChildRiftCount >= MaxChildRifts)
    {
        return;
    }

    ChildRiftBurstTimeRemaining -= DeltaTime;
    if (ChildRiftBurstTimeRemaining > 0.f)
    {
        return;
    }

    TrySpawnChildRiftBurst();
    ChildRiftBurstTimeRemaining = ChildRiftBurstInterval;
}

void AGhostRiftSpawner::TrySpawnChildRiftBurst()
{
    if (!GetWorld() || !RiftStaticMesh || MaxChildRifts <= 0)
    {
        return;
    }

    const int32 RemainingCount = MaxChildRifts - SpawnedChildRiftCount;
    const int32 SpawnCount = FMath::Min(ChildRiftsPerBurst, RemainingCount);

    for (int32 Index = 0; Index < SpawnCount; ++Index)
    {
        FTransform ChildTransform;
        if (!TryFindChildRiftTransform(ChildTransform))
        {
            continue;
        }

        UStaticMeshComponent* ChildMeshComponent = NewObject<UStaticMeshComponent>(this);
        if (!ChildMeshComponent)
        {
            continue;
        }

        ChildMeshComponent->SetStaticMesh(RiftStaticMesh);
        ChildMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ChildMeshComponent->SetGenerateOverlapEvents(false);
        ChildMeshComponent->SetCanEverAffectNavigation(false);
        ChildMeshComponent->AttachToComponent(ChildRiftRootComponent ? ChildRiftRootComponent : SceneRootComponent, FAttachmentTransformRules::KeepWorldTransform);
        ChildMeshComponent->RegisterComponent();
        ChildMeshComponent->SetWorldTransform(ChildTransform);
        ChildMeshComponent->SetWorldScale3D(FVector(0.001f));
        AddInstanceComponent(ChildMeshComponent);

        const float ChildScale = FMath::FRandRange(ChildRiftMinScale, ChildRiftMaxScale);
        AddRuntimeRiftCrack(ChildMeshComponent, FVector(ChildScale), ChildRiftGrowthDuration);
        ++SpawnedChildRiftCount;
    }
}

bool AGhostRiftSpawner::TryFindChildRiftTransform(FTransform& OutTransform) const
{
    for (int32 Attempt = 0; Attempt < ChildRiftPlacementAttempts; ++Attempt)
    {
        const FVector ParentLocation = GetRandomBranchParentLocation();
        FVector Direction = FMath::VRand();
        if (Direction.IsNearlyZero())
        {
            Direction = GetActorForwardVector();
        }

        const float Distance = FMath::FRandRange(ChildRiftMinDistanceFromParent, ChildRiftMaxDistanceFromParent);
        const FVector CandidateLocation = ParentLocation + Direction * Distance;

        if (FVector::DistSquared(CandidateLocation, GetActorLocation()) > FMath::Square(MaxChildRiftSpreadDistance))
        {
            continue;
        }

        if (!IsChildRiftLocationClear(CandidateLocation))
        {
            continue;
        }

        const FRotator CandidateRotation = GetRandomRotationInRange(ChildRiftRandomRotationRange);
        OutTransform = FTransform(CandidateRotation, CandidateLocation, FVector(0.001f));
        return true;
    }

    return false;
}

bool AGhostRiftSpawner::IsChildRiftLocationClear(const FVector& Location) const
{
    for (const FRuntimeRiftCrack& Crack : RiftCracks)
    {
        if (Crack.MeshComponent && FVector::DistSquared(Crack.MeshComponent->GetComponentLocation(), Location) < FMath::Square(MinDistanceBetweenRifts))
        {
            return false;
        }
    }

    if (!bAvoidSceneOverlapForChildRifts || !GetWorld())
    {
        return true;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GhostRiftChildPlacement), false, this);
    const FCollisionShape CheckShape = FCollisionShape::MakeSphere(ChildRiftOverlapCheckRadius);

    const bool bHitsWorldStatic = GetWorld()->OverlapBlockingTestByChannel(
        Location,
        FQuat::Identity,
        ECC_WorldStatic,
        CheckShape,
        QueryParams);

    const bool bHitsWorldDynamic = GetWorld()->OverlapBlockingTestByChannel(
        Location,
        FQuat::Identity,
        ECC_WorldDynamic,
        CheckShape,
        QueryParams);

    if (bShowDebugChildPlacement && GetWorld())
    {
        DrawDebugSphere(
            GetWorld(),
            Location,
            ChildRiftOverlapCheckRadius,
            10,
            (bHitsWorldStatic || bHitsWorldDynamic) ? FColor::Red : FColor::Green,
            false,
            1.f,
            0,
            1.f);
    }

    return !bHitsWorldStatic && !bHitsWorldDynamic;
}

FVector AGhostRiftSpawner::GetRandomBranchParentLocation() const
{
    TArray<const FRuntimeRiftCrack*> FinishedCracks;
    FinishedCracks.Reserve(RiftCracks.Num());

    for (const FRuntimeRiftCrack& Crack : RiftCracks)
    {
        if (Crack.MeshComponent && Crack.bFinishedGrowing)
        {
            FinishedCracks.Add(&Crack);
        }
    }

    if (FinishedCracks.IsEmpty())
    {
        return GetActorLocation();
    }

    const FRuntimeRiftCrack* SelectedCrack = FinishedCracks[FMath::RandRange(0, FinishedCracks.Num() - 1)];
    return SelectedCrack && SelectedCrack->MeshComponent
        ? SelectedCrack->MeshComponent->GetComponentLocation()
        : GetActorLocation();
}

FRuntimeRiftCrack* AGhostRiftSpawner::AddRuntimeRiftCrack(UStaticMeshComponent* MeshComponent, const FVector& TargetScale, float GrowthDuration)
{
    if (!MeshComponent)
    {
        return nullptr;
    }

    FRuntimeRiftCrack& Crack = RiftCracks.AddDefaulted_GetRef();
    Crack.MeshComponent = MeshComponent;
    Crack.DynamicMaterial = ApplyRiftMaterial(MeshComponent);
    Crack.TargetScale = TargetScale;
    Crack.Age = 0.f;
    Crack.GrowthDuration = FMath::Max(0.01f, GrowthDuration);
    Crack.bFinishedGrowing = false;
    return &Crack;
}

UMaterialInstanceDynamic* AGhostRiftSpawner::ApplyRiftMaterial(UStaticMeshComponent* MeshComponent) const
{
    if (!MeshComponent)
    {
        return nullptr;
    }

    if (!RiftMaterial)
    {
        return Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(0));
    }

    UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(RiftMaterial, MeshComponent);
    if (DynamicMaterial)
    {
        MeshComponent->SetMaterial(0, DynamicMaterial);
        if (!GrowthMaterialScalarName.IsNone())
        {
            DynamicMaterial->SetScalarParameterValue(GrowthMaterialScalarName, 0.f);
        }
    }

    return DynamicMaterial;
}

FRotator AGhostRiftSpawner::GetRandomRotationInRange(const FRotator& Range) const
{
    return FRotator(
        FMath::FRandRange(-Range.Pitch, Range.Pitch),
        FMath::FRandRange(-Range.Yaw, Range.Yaw),
        FMath::FRandRange(-Range.Roll, Range.Roll));
}

float AGhostRiftSpawner::GetNextSpawnDelay() const
{
    const float RandomOffset = SpawnIntervalRandomness > 0.f
        ? FMath::FRandRange(-SpawnIntervalRandomness, SpawnIntervalRandomness)
        : 0.f;
    return FMath::Max(0.05f, SpawnInterval + RandomOffset);
}

FVector AGhostRiftSpawner::GetSpawnLocation() const
{
    if (SpawnRadius <= 0.f)
    {
        return GetActorLocation();
    }

    const FVector RandomOffset = FMath::VRand() * FMath::FRandRange(0.f, SpawnRadius);
    return GetActorLocation() + RandomOffset;
}

void AGhostRiftSpawner::HandleSerpentAbsorbed(AGhostSerpentVFXActor* SerpentActor, AActor* AbsorbedTarget)
{
    (void)AbsorbedTarget;
    ActiveSerpents.RemoveAll(
        [SerpentActor](const TWeakObjectPtr<AGhostSerpentVFXActor>& Entry)
        {
            return !Entry.IsValid() || Entry.Get() == SerpentActor;
        });
}