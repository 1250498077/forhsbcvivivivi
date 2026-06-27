#include "PickupActorAAARuneInstrument.h"

#include "Components/LightComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "ExorcismSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GhostCharacter.h"
#include "Materials/MaterialInterface.h"
#include "MyAIController.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

APickupActorAAARuneInstrument::APickupActorAAARuneInstrument()
{
    PrimaryActorTick.bCanEverTick = false;

    HoldType = EHoldItemType::RuneInstrument;
    FP_SocketName = TEXT("RightSocket");
    FP_LocationOffset = FVector::ZeroVector;
    FP_RotationOffset = FRotator::ZeroRotator;
    TP_SocketName = TEXT("RightSocket");
    TP_LocationOffset = FVector::ZeroVector;
    TP_RotationOffset = FRotator::ZeroRotator;

    ItemMassKg = 1.4f;
    ItemThrowForceMultiplier = 0.9f;
    ItemLinearDamping = 0.08f;
    ItemAngularDamping = 0.25f;
    ItemThrowSpinRateDegrees = 1800.f;
    bApplyThrowSpin = false;

    Tags.Add(FName("RuneInstrument"));
    Tags.Add(FName("Pickup"));
    Tags.Add(FName("Rune"));

    RuneLineRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RuneLineRootComponent"));
    RuneLineRootComponent->SetupAttachment(VisualMeshRootComponent);

    if (MeshComponent)
    {
        MeshComponent->SetNotifyRigidBodyCollision(true);
        MeshComponent->BodyInstance.bUseCCD = true;
        MeshComponent->OnComponentHit.AddDynamic(this, &APickupActorAAARuneInstrument::HandleRuneHit);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultCylinderMesh(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (DefaultCylinderMesh.Succeeded())
    {
        LineSegmentMesh = DefaultCylinderMesh.Object;
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultCubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (DefaultCubeMesh.Succeeded())
    {
        DefaultShardMesh = DefaultCubeMesh.Object;
    }
}

void APickupActorAAARuneInstrument::BeginPlay()
{
    Super::BeginPlay();
    ApplyRunePhysicsTuning();
    LoadExorcismPatterns();
    ResolveNodeBindings();
    ResolveLightBindings();
    ApplySequenceToVisuals(CommittedSequence, bPatternSolved, bRuneDrawActive);
}

void APickupActorAAARuneInstrument::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyRunePhysicsTuning();
    ResolveNodeBindings();
    ResolveLightBindings();
    ApplySequenceToVisuals(CommittedSequence, bPatternSolved, bRuneDrawActive);
}

void APickupActorAAARuneInstrument::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DisarmRuneShatter();
    
    // =======================
    GetWorldTimerManager().ClearTimer(ExorcismDetonationHandle);
    bRuneArmedForExorcism = false;
    bAwaitingArmedThrowFirstImpact = false;
    bAttachedToExorcismGhost = false;
    ArmedGhostTypeId = INDEX_NONE;
    ArmedSolvingActor.Reset();
    AttachedGhostPawn.Reset();
    AttachedGhostController.Reset();
    // =======================

    ClearSolvedLights();
    ResolvedLightComponents.Reset();
    ResolvedNodeComponents.Reset();
    OriginalNodeMaterials.Reset();
    PersistentVisualSequence.Reset();
    ClearLineMeshes();
    Super::EndPlay(EndPlayReason);
}

void APickupActorAAARuneInstrument::OnPickedUp()
{
    Super::OnPickedUp();
    DisarmRuneShatter();
    GetWorldTimerManager().ClearTimer(ExorcismDetonationHandle);
    bHasShattered = false;
    ResetRuneState();
}

void APickupActorAAARuneInstrument::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    Super::OnPutDown(PlaceLocation, PlaceRotation);
    DisarmRuneShatter();
    // ================================================================================================
    GetWorldTimerManager().ClearTimer(ExorcismDetonationHandle);
    bRuneArmedForExorcism = false;
    bAwaitingArmedThrowFirstImpact = false;
    bAttachedToExorcismGhost = false;
    ArmedGhostTypeId = INDEX_NONE;
    ArmedSolvingActor.Reset();
    AttachedGhostPawn.Reset();
    AttachedGhostController.Reset();
    // ================================================================================================
    if (MeshComponent)
    {
        MeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }
    SetRuneGroundedState();
}

void APickupActorAAARuneInstrument::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    Super::OnThrown(ThrowDirection, ThrowForce);

    bAttachedToExorcismGhost = false;
    AttachedGhostPawn.Reset();
    AttachedGhostController.Reset();
    GetWorldTimerManager().ClearTimer(ExorcismDetonationHandle);
    bAwaitingArmedThrowFirstImpact = bRequireThrownImpactForExorcism && bRuneArmedForExorcism;

    if (!MeshComponent)
    {
        return;
    }
    
    MeshComponent->SetNotifyRigidBodyCollision(true);

    if (bApplyThrowInitialRotationOffset && !ThrowInitialRotationOffset.IsNearlyZero())
    {
        const FQuat CurrentRotation = GetActorQuat();
        const FQuat OffsetRotation = ThrowInitialRotationOffset.Quaternion();
        SetActorRotation((CurrentRotation * OffsetRotation).Rotator());
    }

    bHasShattered = false;
    DisarmRuneShatter();

    FVector SpinAxisSource = ItemThrowSpinAxisBias;
    // 去除随机旋转，改为固定旋转轴，方便玩家对准和记忆碎片飞行方向
    // SpinAxisSource.X += FMath::FRandRange(-0.25f, 0.25f);
    // SpinAxisSource.Y += FMath::FRandRange(-0.25f, 0.25f);
    // SpinAxisSource.Z += FMath::FRandRange(-0.25f, 0.25f);

    if (SpinAxisSource.IsNearlyZero())
    {
        SpinAxisSource = FVector::RightVector;
    }

    FVector SpinAxis = bUseLocalSpaceSpinAxis
        ? GetActorTransform().TransformVectorNoScale(SpinAxisSource)
        : SpinAxisSource;
    SpinAxis = SpinAxis.GetSafeNormal();
    if (SpinAxis.IsNearlyZero())
    {
        SpinAxis = bUseLocalSpaceSpinAxis
            ? GetActorTransform().TransformVectorNoScale(FVector::RightVector).GetSafeNormal()
            : FVector::RightVector;
    }

    MeshComponent->SetPhysicsAngularVelocityInDegrees(SpinAxis * ItemThrowSpinRateDegrees, false);
    MeshComponent->WakeAllRigidBodies();

    if (ShatterArmDelay <= 0.f)
    {
        ArmRuneForImpactShatter();
        return;
    }

    GetWorldTimerManager().SetTimer(
        ShatterArmHandle,
        this,
        &APickupActorAAARuneInstrument::ArmRuneForImpactShatter,
        ShatterArmDelay,
        false);
}

bool APickupActorAAARuneInstrument::BeginRuneDraw(APlayerController* UsingController)
{
    if (!UsingController)
    {
        return false;
    }

    ResolveNodeBindings();
    if (ResolvedNodeComponents.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s rune draw start failed: no node components resolved"), *GetName());
        return false;
    }

    bRuneDrawActive = true;
    bPatternSolved = false;
    SolvedPatternId = NAME_None;
    CurrentDrawSequence.Reset();
    CommittedSequence.Reset();

    SetHoveredNodeId(INDEX_NONE, true);

    ApplySequenceToVisuals(CurrentDrawSequence, false, true);
    return true;
}

void APickupActorAAARuneInstrument::UpdateRuneDrawFromScreenPosition(
    APlayerController* UsingController,
    const FVector2D& ScreenPosition)
{
    if (!bRuneDrawActive || !UsingController)
    {
        return;
    }

    const int32 NewHoveredNodeId = ResolveHoveredNodeFromScreenPosition(UsingController, ScreenPosition);
    SetHoveredNodeId(NewHoveredNodeId);
    TryAppendResolvedNode(NewHoveredNodeId);
}

TArray<int32> APickupActorAAARuneInstrument::EndRuneDraw(APlayerController* UsingController)
{
    if (!bRuneDrawActive)
    {
        return CommittedSequence;
    }

    (void)UsingController;

    bRuneDrawActive = false;
    SetHoveredNodeId(INDEX_NONE, true);
    CommittedSequence = CurrentDrawSequence;
    CurrentDrawSequence.Reset();

    SolvedPatternId = NAME_None;
    bPatternSolved = TryResolveAcceptedPattern(CommittedSequence, SolvedPatternId);
    ApplySequenceToVisuals(CommittedSequence, bPatternSolved, false);
    return CommittedSequence;
}

void APickupActorAAARuneInstrument::ResetRuneState()
{
    GetWorldTimerManager().ClearTimer(ExorcismDetonationHandle);
    bRuneDrawActive = false;
    bPatternSolved = false;
    SolvedPatternId = NAME_None;
    // =======================================
    bRuneArmedForExorcism = false;
    bAwaitingArmedThrowFirstImpact = false;
    bAttachedToExorcismGhost = false;
    ArmedGhostTypeId = INDEX_NONE;
    ArmedSolvingActor.Reset();
    AttachedGhostPawn.Reset();
    AttachedGhostController.Reset();
    // =======================================
    PersistentVisualSequence.Reset();
    CurrentDrawSequence.Reset();
    CommittedSequence.Reset();
    SetHoveredNodeId(INDEX_NONE, true);
    ApplySequenceToVisuals(CommittedSequence, false, false);
}

void APickupActorAAARuneInstrument::CommitRuneSequenceAuthority(
    const TArray<int32>& NodeSequence,
    AActor* SolvingActor)
{
    if (!HasAuthority())
    {
        return;
    }

    CommittedSequence = NodeSequence;
    SolvedPatternId = NAME_None;
    bPatternSolved = TryResolveAcceptedPattern(CommittedSequence, SolvedPatternId);
    // ==========================================
    bRuneArmedForExorcism = false;
    bAwaitingArmedThrowFirstImpact = false;
    bAttachedToExorcismGhost = false;
    ArmedGhostTypeId = INDEX_NONE;
    ArmedSolvingActor = SolvingActor;
    AttachedGhostPawn.Reset();
    AttachedGhostController.Reset();
    GetWorldTimerManager().ClearTimer(ExorcismDetonationHandle);
    // ==========================================
    ApplySequenceToVisuals(CommittedSequence, bPatternSolved, false);

    if (bPatternSolved)
    {
        ReceiveRunePatternSolved(SolvedPatternId, SolvingActor);

        // 符咒匹配成功时，查找并销毁对应类型的鬼
        const UWorld* World = GetWorld();
        const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
        const UExorcismSubsystem* Subsystem = GI ? GI->GetSubsystem<UExorcismSubsystem>() : nullptr;
        if (Subsystem)
        {
            ArmedGhostTypeId = Subsystem->GetGhostTypeForPatternId(SolvedPatternId);
            bRuneArmedForExorcism = ArmedGhostTypeId != INDEX_NONE;

            if (bRuneArmedForExorcism)
            {
                UE_LOG(LogTemp, Log, TEXT("Exorcism: rune '%s' armed for ghost type %d; waiting for thrown impact"),
                *SolvedPatternId.ToString(), ArmedGhostTypeId);
            }
        }
    }
}

void APickupActorAAARuneInstrument::LoadExorcismPatterns()
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const UGameInstance* GI = World->GetGameInstance();
    if (!GI)
    {
        return;
    }

    const UExorcismSubsystem* Subsystem = GI->GetSubsystem<UExorcismSubsystem>();
    if (!Subsystem || !Subsystem->HasMappings())
    {
        return;
    }

    TArray<FRuneInstrumentPattern> ExorcismPatterns = Subsystem->GetAllAcceptedRunePatterns();
    if (ExorcismPatterns.Num() == 0)
    {
        return;
    }

    AcceptedPatterns = MoveTemp(ExorcismPatterns);

    UE_LOG(LogTemp, Log, TEXT("%s: loaded %d exorcism patterns from subsystem"),
        *GetName(), AcceptedPatterns.Num());
}

void APickupActorAAARuneInstrument::ResolveNodeBindings()
{
    ResolvedNodeComponents.Reset();

    TArray<USceneComponent*> SceneComponents;
    GetComponents(SceneComponents);

    for (USceneComponent* SceneComponent : SceneComponents)
    {
        if (!IsValid(SceneComponent) || SceneComponent == MeshComponent || SceneComponent == VisualMeshRootComponent ||
            SceneComponent == RuneLineRootComponent)
        {
            continue;
        }

        const FString ComponentName = SceneComponent->GetName();
        if (!ComponentName.StartsWith(NodeNamePrefix, ESearchCase::IgnoreCase))
        {
            continue;
        }

        const FString IdString = ComponentName.RightChop(NodeNamePrefix.Len());
        if (IdString.IsEmpty())
        {
            continue;
        }

        bool bIsNumericId = true;
        for (const TCHAR Char : IdString)
        {
            if (!FChar::IsDigit(Char))
            {
                bIsNumericId = false;
                break;
            }
        }

        if (!bIsNumericId)
        {
            continue;
        }

        const int32 NodeId = FCString::Atoi(*IdString);
        if (NodeId <= 0)
        {
            continue;
        }

        if (ResolvedNodeComponents.Contains(NodeId))
        {
            UE_LOG(LogTemp, Warning, TEXT("%s has duplicate rune node id %d from component %s"), *GetName(), NodeId, *ComponentName);
            continue;
        }

        ResolvedNodeComponents.Add(NodeId, SceneComponent);
    }
}

void APickupActorAAARuneInstrument::ResolveLightBindings()
{
    ResolvedLightComponents.Reset();

    TArray<ULightComponent*> LightComponents;
    GetComponents(LightComponents);

    for (ULightComponent* LightComponent : LightComponents)
    {
        if (!IsValid(LightComponent))
        {
            continue;
        }

        const FString ComponentName = LightComponent->GetName();
        if (!ComponentName.StartsWith(LightNamePrefix, ESearchCase::IgnoreCase))
        {
            continue;
        }

        const FString IdString = ComponentName.RightChop(LightNamePrefix.Len());
        if (IdString.IsEmpty())
        {
            continue;
        }

        bool bIsNumericId = true;
        for (const TCHAR Char : IdString)
        {
            if (!FChar::IsDigit(Char))
            {
                bIsNumericId = false;
                break;
            }
        }

        if (!bIsNumericId)
        {
            continue;
        }

        const int32 NodeId = FCString::Atoi(*IdString);
        if (NodeId <= 0)
        {
            continue;
        }

        if (ResolvedLightComponents.Contains(NodeId))
        {
            UE_LOG(LogTemp, Warning, TEXT("%s has duplicate rune light id %d from component %s"), *GetName(), NodeId, *ComponentName);
            continue;
        }

        ResolvedLightComponents.Add(NodeId, LightComponent);
    }
}

int32 APickupActorAAARuneInstrument::ResolveHoveredNodeFromScreenPosition(
    APlayerController* UsingController,
    const FVector2D& ScreenPosition) const
{
    if (!UsingController)
    {
        return INDEX_NONE;
    }

    const float MaxDistanceSquared = FMath::Square(FMath::Max(1.f, NodeSelectionScreenRadius));
    float BestDistanceSquared = MaxDistanceSquared;
    int32 BestNodeId = INDEX_NONE;

    for (const TPair<int32, TWeakObjectPtr<USceneComponent>>& NodePair : ResolvedNodeComponents)
    {
        const USceneComponent* NodeComponent = NodePair.Value.Get();
        if (!IsValid(NodeComponent))
        {
            continue;
        }

        FVector2D ProjectedScreenLocation = FVector2D::ZeroVector;
        if (!UsingController->ProjectWorldLocationToScreen(NodeComponent->GetComponentLocation(), ProjectedScreenLocation, true))
        {
            continue;
        }

        const float DistanceSquared = FVector2D::DistSquared(ProjectedScreenLocation, ScreenPosition);
        if (DistanceSquared > BestDistanceSquared)
        {
            continue;
        }

        BestDistanceSquared = DistanceSquared;
        BestNodeId = NodePair.Key;
    }

    return BestNodeId;
}

bool APickupActorAAARuneInstrument::TryAppendResolvedNode(int32 NodeId)
{
    if (NodeId == INDEX_NONE)
    {
        return false;
    }

    if (CurrentDrawSequence.Num() > 0 && CurrentDrawSequence.Last() == NodeId)
    {
        return false;
    }

    if (!bAllowNodeRepeat && CurrentDrawSequence.Contains(NodeId))
    {
        return false;
    }

    CurrentDrawSequence.Add(NodeId);
    ApplySequenceToVisuals(CurrentDrawSequence, false, true);
    return true;
}

bool APickupActorAAARuneInstrument::TryResolveAcceptedPattern(
    const TArray<int32>& NodeSequence,
    FName& OutPatternId) const
{
    OutPatternId = NAME_None;

    for (const FRuneInstrumentPattern& Pattern : AcceptedPatterns)
    {
        if (Pattern.NodeSequence.Num() == 0)
        {
            continue;
        }

        if (Pattern.NodeSequence == NodeSequence)
        {
            OutPatternId = Pattern.PatternId;
            return true;
        }
    }

    return false;
}

FVector APickupActorAAARuneInstrument::GetNodeWorldLocationById(int32 NodeId) const
{
    if (const TWeakObjectPtr<USceneComponent>* NodeComponentPtr = ResolvedNodeComponents.Find(NodeId))
    {
        if (const USceneComponent* NodeComponent = NodeComponentPtr->Get())
        {
            return NodeComponent->GetComponentLocation();
        }
    }

    return GetActorLocation();
}

void APickupActorAAARuneInstrument::ApplySequenceToVisuals(
    const TArray<int32>& NodeSequence,
    bool bSolved,
    bool bDrawingActive)
{
    (void)bSolved;

    const bool bReachedVisualFinalNode = bDrawingActive && HasReachedVisualFinalNode(NodeSequence);
    if (bReachedVisualFinalNode)
    {
        PersistentVisualSequence = NodeSequence;
    }

    const TArray<int32>& ActiveVisualSequence =
        PersistentVisualSequence.Num() > 0 ? PersistentVisualSequence : NodeSequence;
    const bool bHasPersistentVisuals = PersistentVisualSequence.Num() > 0;

    RebuildLineMeshes(NodeSequence);
    if (bHasPersistentVisuals)
    {
        SpawnSolvedLights(ActiveVisualSequence);
    }
    else
    {
        ClearSolvedLights();
    }

    if (bHasPersistentVisuals)
    {
        ApplySolvedNodeMaterials(ActiveVisualSequence);
    }
    else
    {
        ClearSolvedNodeMaterials();
    }

    ReceiveRuneVisualStateChanged(NodeSequence, bSolved, bDrawingActive);
}

bool APickupActorAAARuneInstrument::HasReachedVisualFinalNode(const TArray<int32>& NodeSequence) const
{
    if (NodeSequence.Num() == 0 || ResolvedNodeComponents.Num() == 0)
    {
        return false;
    }

    int32 HighestNodeId = INDEX_NONE;
    for (const TPair<int32, TWeakObjectPtr<USceneComponent>>& NodePair : ResolvedNodeComponents)
    {
        if (NodePair.Key > HighestNodeId && IsValid(NodePair.Value.Get()))
        {
            HighestNodeId = NodePair.Key;
        }
    }

    if (HighestNodeId == INDEX_NONE)
    {
        return false;
    }

    return NodeSequence.Last() == HighestNodeId;
}

void APickupActorAAARuneInstrument::SetHoveredNodeId(int32 NewHoveredNodeId, bool bForceBroadcast)
{
    if (!bForceBroadcast && HoveredNodeId == NewHoveredNodeId)
    {
        return;
    }

    HoveredNodeId = NewHoveredNodeId;
    ReceiveRuneHoverStateChanged(HoveredNodeId, bRuneDrawActive);
}

void APickupActorAAARuneInstrument::RebuildLineMeshes(const TArray<int32>& NodeSequence)
{
    ClearLineMeshes();

    if (!RuneLineRootComponent || !LineSegmentMesh || NodeSequence.Num() < 2)
    {
        return;
    }

    const FTransform RootTransform = RuneLineRootComponent->GetComponentTransform();

    for (int32 NodeIndex = 1; NodeIndex < NodeSequence.Num(); ++NodeIndex)
    {
        const FVector StartWorld = GetNodeWorldLocationById(NodeSequence[NodeIndex - 1]);
        const FVector EndWorld = GetNodeWorldLocationById(NodeSequence[NodeIndex]);
        const FVector StartLocal = RootTransform.InverseTransformPosition(StartWorld);
        const FVector EndLocal = RootTransform.InverseTransformPosition(EndWorld);
        const FVector Tangent = (EndLocal - StartLocal) * 0.5f;

        USplineMeshComponent* LineComponent = NewObject<USplineMeshComponent>(this);
        if (!LineComponent)
        {
            continue;
        }

        LineComponent->SetupAttachment(RuneLineRootComponent);
        LineComponent->SetMobility(EComponentMobility::Movable);
        LineComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        LineComponent->SetGenerateOverlapEvents(false);
        LineComponent->SetCanEverAffectNavigation(false);
        LineComponent->SetCastShadow(false);
        LineComponent->SetVisibleInRayTracing(false);
        LineComponent->SetStaticMesh(LineSegmentMesh);
        LineComponent->SetForwardAxis(LineForwardAxis, false);
        LineComponent->RegisterComponent();
        LineComponent->SetStartScale(FVector2D(LineThickness, LineThickness), false);
        LineComponent->SetEndScale(FVector2D(LineThickness, LineThickness), false);
        LineComponent->SetStartAndEnd(StartLocal, Tangent, EndLocal, Tangent, true);

        if (LineMaterialOverride)
        {
            int32 MaterialSlotCount = LineSegmentMesh->GetStaticMaterials().Num();
            if (MaterialSlotCount <= 0)
            {
                MaterialSlotCount = 1;
            }

            for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
            {
                LineComponent->SetMaterial(MaterialIndex, LineMaterialOverride);
            }
            LineComponent->MarkRenderStateDirty();
        }

        ActiveLineComponents.Add(LineComponent);
    }
}

void APickupActorAAARuneInstrument::ClearLineMeshes()
{
    for (USplineMeshComponent* LineComponent : ActiveLineComponents)
    {
        if (!IsValid(LineComponent))
        {
            continue;
        }

        LineComponent->DestroyComponent();
    }

    ActiveLineComponents.Reset();
}

void APickupActorAAARuneInstrument::SpawnSolvedLights(const TArray<int32>& NodeSequence)
{
    ClearSolvedLights();

    if (NodeSequence.Num() == 0)
    {
        return;
    }

    TSet<int32> SpawnedNodeIds;
    for (const int32 NodeId : NodeSequence)
    {
        if (NodeId <= 0 || SpawnedNodeIds.Contains(NodeId))
        {
            continue;
        }

        const TWeakObjectPtr<ULightComponent>* LightComponentPtr = ResolvedLightComponents.Find(NodeId);
        ULightComponent* LightComponent = LightComponentPtr ? LightComponentPtr->Get() : nullptr;
        if (!IsValid(LightComponent))
        {
            continue;
        }

        LightComponent->SetVisibility(true, false);
        SpawnedNodeIds.Add(NodeId);
    }
}

void APickupActorAAARuneInstrument::ClearSolvedLights()
{
    for (const TPair<int32, TWeakObjectPtr<ULightComponent>>& LightPair : ResolvedLightComponents)
    {
        ULightComponent* LightComponent = LightPair.Value.Get();
        if (!IsValid(LightComponent))
        {
            continue;
        }

        LightComponent->SetVisibility(false, false);
    }
}

void APickupActorAAARuneInstrument::ApplySolvedNodeMaterials(const TArray<int32>& NodeSequence)
{
    ClearSolvedNodeMaterials();

    if (!SolvedNodeMaterial || NodeSequence.Num() == 0)
    {
        return;
    }

    TSet<int32> ProcessedIds;
    for (const int32 NodeId : NodeSequence)
    {
        if (NodeId <= 0 || ProcessedIds.Contains(NodeId))
        {
            continue;
        }

        const TWeakObjectPtr<USceneComponent>* NodeComponentPtr = ResolvedNodeComponents.Find(NodeId);
        UMeshComponent* MeshComp = NodeComponentPtr ? Cast<UMeshComponent>(NodeComponentPtr->Get()) : nullptr;
        if (!IsValid(MeshComp))
        {
            continue;
        }

        const int32 SlotCount = MeshComp->GetNumMaterials();
        TArray<UMaterialInterface*>& Originals = OriginalNodeMaterials.Add(NodeId);
        for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
        {
            Originals.Add(MeshComp->GetMaterial(SlotIndex));
            MeshComp->SetMaterial(SlotIndex, SolvedNodeMaterial);
        }

        ProcessedIds.Add(NodeId);
    }
}

void APickupActorAAARuneInstrument::ClearSolvedNodeMaterials()
{
    for (const TPair<int32, TArray<UMaterialInterface*>>& OriginalPair : OriginalNodeMaterials)
    {
        const TWeakObjectPtr<USceneComponent>* NodeComponentPtr = ResolvedNodeComponents.Find(OriginalPair.Key);
        UMeshComponent* MeshComp = NodeComponentPtr ? Cast<UMeshComponent>(NodeComponentPtr->Get()) : nullptr;
        if (!IsValid(MeshComp))
        {
            continue;
        }

        for (int32 SlotIndex = 0; SlotIndex < OriginalPair.Value.Num(); ++SlotIndex)
        {
            MeshComp->SetMaterial(SlotIndex, OriginalPair.Value[SlotIndex]);
        }
    }

    OriginalNodeMaterials.Reset();
}

void APickupActorAAARuneInstrument::GetNodeScreenPositions(
    APlayerController* PC,
    TArray<TPair<int32, FVector2D>>& OutPositions) const
{
    OutPositions.Reset();
    if (!PC)
    {
        return;
    }

    for (const TPair<int32, TWeakObjectPtr<USceneComponent>>& NodePair : ResolvedNodeComponents)
    {
        const USceneComponent* NodeComponent = NodePair.Value.Get();
        if (!IsValid(NodeComponent))
        {
            continue;
        }

        FVector2D ScreenPos = FVector2D::ZeroVector;
        if (PC->ProjectWorldLocationToScreen(NodeComponent->GetComponentLocation(), ScreenPos, true))
        {
            OutPositions.Emplace(NodePair.Key, ScreenPos);
        }
    }
}

bool APickupActorAAARuneInstrument::GetPreferredDrawStartScreenPosition(
    APlayerController* PC,
    FVector2D& OutScreenPosition) const
{
    OutScreenPosition = FVector2D::ZeroVector;
    if (!PC)
    {
        return false;
    }

    auto TryProjectNode = [this, PC, &OutScreenPosition](int32 NodeId)
    {
        const TWeakObjectPtr<USceneComponent>* NodeComponentPtr = ResolvedNodeComponents.Find(NodeId);
        const USceneComponent* NodeComponent = NodeComponentPtr ? NodeComponentPtr->Get() : nullptr;
        if (!IsValid(NodeComponent))
        {
            return false;
        }

        return PC->ProjectWorldLocationToScreen(NodeComponent->GetComponentLocation(), OutScreenPosition, true);
    };

    if (TryProjectNode(1))
    {
        return true;
    }

    int32 FallbackNodeId = MAX_int32;
    for (const TPair<int32, TWeakObjectPtr<USceneComponent>>& NodePair : ResolvedNodeComponents)
    {
        if (NodePair.Key > 0 && IsValid(NodePair.Value.Get()))
        {
            FallbackNodeId = FMath::Min(FallbackNodeId, NodePair.Key);
        }
    }

    return FallbackNodeId != MAX_int32 && TryProjectNode(FallbackNodeId);
}

bool APickupActorAAARuneInstrument::TryResolveMatchedRevealedGhost(
    AActor* OtherActor,
    APawn*& OutPawn,
    AMyAIController*& OutAIController) const
{
    OutPawn = nullptr;
    OutAIController = nullptr;

    if (!bRuneArmedForExorcism || ArmedGhostTypeId == INDEX_NONE || !IsValid(OtherActor) || OtherActor == this)
    {
        return false;
    }

    APawn* HitPawn = Cast<APawn>(OtherActor);
    if (!IsValid(HitPawn))
    {
        return false;
    }

    AMyAIController* AIController = Cast<AMyAIController>(HitPawn->GetController());
    if (!IsValid(AIController) || !AIController->IsGhostRevealedByEffect())
    {
        return false;
    }

    if (AIController->ExorcismGhostTypeId != ArmedGhostTypeId)
    {
        return false;
    }

    OutPawn = HitPawn;
    OutAIController = AIController;
    return true;
}


bool APickupActorAAARuneInstrument::TryAttachToMatchedGhostZone(
    APawn* TargetPawn,
    AMyAIController* TargetController,
    UPrimitiveComponent* AttachComponent)
{
    if (!bRuneArmedForExorcism || !bAwaitingArmedThrowFirstImpact || !IsValid(TargetPawn) || !IsValid(TargetController))
    {
        return false;
    }

    if (!TargetController->IsGhostRevealedByEffect() || TargetController->ExorcismGhostTypeId != ArmedGhostTypeId)
    {
        return false;
    }

    AttachRuneToGhostZone(TargetPawn, TargetController, AttachComponent);
    return bAttachedToExorcismGhost;
}

void APickupActorAAARuneInstrument::AttachRuneToGhost(
    APawn* HitPawn,
    UPrimitiveComponent* HitComponent,
    const FHitResult& Hit)
{
    if (!MeshComponent || !IsValid(HitPawn))
    {
        return;
    }

    AMyAIController* AIController = Cast<AMyAIController>(HitPawn->GetController());
    if (!IsValid(AIController))
    {
        return;
    }

    DisarmRuneShatter();
    GetWorldTimerManager().ClearTimer(ExorcismDetonationHandle);

    bAttachedToExorcismGhost = true;
    bAwaitingArmedThrowFirstImpact = false;
    AttachedGhostPawn = HitPawn;
    AttachedGhostController = AIController;

    MeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
    MeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    MeshComponent->SetSimulatePhysics(false);
    MeshComponent->SetEnableGravity(false);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    FVector StickLocation = Hit.bBlockingHit ? FVector(Hit.ImpactPoint) : GetActorLocation();
    FVector StickNormal = Hit.bBlockingHit ? Hit.ImpactNormal.GetSafeNormal() : FVector::ZeroVector;
    if (!StickNormal.IsNearlyZero())
    {
        StickLocation += StickNormal * 2.f;
    }

    SetActorLocation(StickLocation);

    if (USceneComponent* AttachParent = HitComponent ? Cast<USceneComponent>(HitComponent) : HitPawn->GetRootComponent())
    {
        AttachToComponent(AttachParent, FAttachmentTransformRules::KeepWorldTransform);
    }
    else
    {
        AttachToActor(HitPawn, FAttachmentTransformRules::KeepWorldTransform);
    }

    AIController->ApplyFlashlightReveal(FMath::Max(0.1f, ExorcismDetonationDelay + ExorcismRevealExtraTime));

    UE_LOG(LogTemp, Log, TEXT("Exorcism: %s latched onto revealed ghost %s; detonation in %.2fs"),
        *GetName(),
        *HitPawn->GetName(),
        ExorcismDetonationDelay);

    GetWorldTimerManager().SetTimer(
        ExorcismDetonationHandle,
        this,
        &APickupActorAAARuneInstrument::DetonateAttachedExorcism,
        FMath::Max(0.f, ExorcismDetonationDelay),
        false);
}

void APickupActorAAARuneInstrument::AttachRuneToGhostZone(
    APawn* HitPawn,
    AMyAIController* AIController,
    UPrimitiveComponent* AttachComponent)
{
    if (!MeshComponent || !IsValid(HitPawn) || !IsValid(AIController))
    {
        return;
    }

    DisarmRuneShatter();
    GetWorldTimerManager().ClearTimer(ExorcismDetonationHandle);

    bAttachedToExorcismGhost = true;
    bAwaitingArmedThrowFirstImpact = false;
    AttachedGhostPawn = HitPawn;
    AttachedGhostController = AIController;

    MeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
    MeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    MeshComponent->SetSimulatePhysics(false);
    MeshComponent->SetEnableGravity(false);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    FVector StickLocation = GetActorLocation();
    FVector StickNormal = FVector::ForwardVector;

    if (IsValid(AttachComponent))
    {
        const FVector ZoneOrigin = AttachComponent->Bounds.Origin;
        FVector HorizontalDirection = StickLocation - ZoneOrigin;
        HorizontalDirection.Z = 0.f;
        if (!HorizontalDirection.Normalize())
        {
            HorizontalDirection = HitPawn->GetActorForwardVector().GetSafeNormal2D();
            if (HorizontalDirection.IsNearlyZero())
            {
                HorizontalDirection = FVector::ForwardVector;
            }
        }

        const float SurfaceRadius = FMath::Max(AttachComponent->Bounds.BoxExtent.X, AttachComponent->Bounds.BoxExtent.Y);
        StickLocation = ZoneOrigin + HorizontalDirection * SurfaceRadius;
        StickLocation.Z = FMath::Clamp(GetActorLocation().Z,
            ZoneOrigin.Z - AttachComponent->Bounds.BoxExtent.Z,
            ZoneOrigin.Z + AttachComponent->Bounds.BoxExtent.Z);
        StickNormal = HorizontalDirection.GetSafeNormal();
    }

    if (!StickNormal.IsNearlyZero())
    {
        StickLocation += StickNormal * 2.f;
        SetActorRotation((-StickNormal).Rotation());
    }

    SetActorLocation(StickLocation);

    if (USceneComponent* AttachParent = Cast<USceneComponent>(AttachComponent))
    {
        AttachToComponent(AttachParent, FAttachmentTransformRules::KeepWorldTransform);
    }
    else
    {
        AttachToActor(HitPawn, FAttachmentTransformRules::KeepWorldTransform);
    }

    AIController->ApplyFlashlightReveal(FMath::Max(0.1f, ExorcismDetonationDelay + ExorcismRevealExtraTime));

    UE_LOG(LogTemp, Log, TEXT("Exorcism: %s latched onto ghost attach zone on %s; detonation in %.2fs"),
        *GetName(),
        *HitPawn->GetName(),
        ExorcismDetonationDelay);

    GetWorldTimerManager().SetTimer(
        ExorcismDetonationHandle,
        this,
        &APickupActorAAARuneInstrument::DetonateAttachedExorcism,
        FMath::Max(0.f, ExorcismDetonationDelay),
        false);
}


void APickupActorAAARuneInstrument::DetonateAttachedExorcism()
{
    if (bHasShattered)
    {
        return;
    }

    APawn* TargetPawn = AttachedGhostPawn.Get();
    AMyAIController* AIController = AttachedGhostController.Get();

    bAttachedToExorcismGhost = false;
    bRuneArmedForExorcism = false;
    bAwaitingArmedThrowFirstImpact = false;

    if (IsValid(AIController))
    {
        AIController->ApplyFlashlightReveal(FMath::Max(0.1f, ExorcismRevealExtraTime));
    }

    if (IsValid(TargetPawn) && IsValid(AIController) && AIController->ExorcismGhostTypeId == ArmedGhostTypeId)
    {
        UE_LOG(LogTemp, Log, TEXT("Exorcism: rune '%s' detonated on matched ghost %s"),
            *SolvedPatternId.ToString(),
            *TargetPawn->GetName());

        AIController->UnPossess();
        TargetPawn->Destroy();
        AIController->Destroy();
    }

    FHitResult SyntheticHit;
    SyntheticHit.bBlockingHit = true;
    SyntheticHit.ImpactPoint = GetActorLocation();
    SyntheticHit.ImpactNormal = FVector::UpVector;
    ShatterRune(SyntheticHit, FVector::ZeroVector);
}

void APickupActorAAARuneInstrument::HandleRuneHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit
)
{
    if (bHasShattered || HitComponent != MeshComponent)
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

    if (bAwaitingArmedThrowFirstImpact)
    {
        APawn* MatchedPawn = nullptr;
        AMyAIController* MatchedAIController = nullptr;
        const bool bMatchedRevealedGhost = TryResolveMatchedRevealedGhost(OtherActor, MatchedPawn, MatchedAIController);

        if (bMatchedRevealedGhost)
        {
            AGhostCharacter* GhostCharacter = Cast<AGhostCharacter>(MatchedPawn);
            if (GhostCharacter && GhostCharacter->IsWorldLocationInsideGhostAttachZone(Hit.ImpactPoint))
            {
                // AttachRuneToGhostZone(MatchedPawn, MatchedAIController, GhostCharacter->GetGhostAttachZoneComponent());
                AttachRuneToGhostZone(MatchedPawn, MatchedAIController, Cast<UPrimitiveComponent>(GhostCharacter->GetGhostAttachZoneComponent()));
                return;
            }

            UE_LOG(LogTemp, Verbose, TEXT("Exorcism: %s hit matched ghost body outside GhostAttachZoneComponent; waiting for zone overlap"), *GetName());
            return;
        }

        UE_LOG(LogTemp, Log, TEXT("Exorcism: %s first throw impact was not a matched revealed ghost; rune deactivated"), *GetName());
        ResetRuneState();
        SetRuneGroundedState();
        return;
    }

    bAwaitingArmedThrowFirstImpact = false;

    if (!bCanShatterOnImpact)
    {
        SetRuneGroundedState();
        return;
    }

    // 只有命中显形的鬼 AI 时才碎裂，撞墙/地板不碎。
    APawn* HitPawn = Cast<APawn>(OtherActor);
    if (!HitPawn)
    {
        SetRuneGroundedState();
        return;
    }

    AMyAIController* AIController = Cast<AMyAIController>(HitPawn->GetController());
    if (!AIController)
    {
        SetRuneGroundedState();
        return;
    }

    if (!AIController->IsGhostRevealedByEffect())
    {
        SetRuneGroundedState();
        return;
    }

    ShatterRune(Hit, NormalImpulse);
}


void APickupActorAAARuneInstrument::SetRuneGroundedState()
{
    if (!bSettleIntoGroundStateAfterImpact || !MeshComponent || IsHeldByPlayer() || bAttachedToExorcismGhost)
    {
        return;
    }

    ApplyReleasedCollisionProfile();
    MeshComponent->SetSimulatePhysics(true);
    MeshComponent->SetEnableGravity(true);
    MeshComponent->SetNotifyRigidBodyCollision(true);
    MeshComponent->WakeAllRigidBodies();
}

void APickupActorAAARuneInstrument::ArmRuneForImpactShatter()
{
    bCanShatterOnImpact = true;
}

void APickupActorAAARuneInstrument::DisarmRuneShatter()
{
    bCanShatterOnImpact = false;
    GetWorldTimerManager().ClearTimer(ShatterArmHandle);
}

void APickupActorAAARuneInstrument::ShatterRune(const FHitResult& Hit, const FVector& NormalImpulse)
{
    if (!MeshComponent || bHasShattered)
    {
        return;
    }

    bHasShattered = true;
    DisarmRuneShatter();

    const FVector RuneVelocity = MeshComponent->GetPhysicsLinearVelocity();
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

    SpawnRuneShards(ImpactPoint, ImpactNormal, RuneVelocity);

    UE_LOG(LogTemp, Warning, TEXT("%s shattered on ghost impact with %s"),
        *GetName(),
        *GetNameSafe(Hit.GetActor()));

    Destroy();
}

void APickupActorAAARuneInstrument::SpawnRuneShards(
    const FVector& ImpactPoint,
    const FVector& ImpactNormal,
    const FVector& RuneVelocity)
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

    UMaterialInterface* ShardMaterial = nullptr;
    if (MeshComponent)
    {
        ShardMaterial = MeshComponent->GetMaterial(0);
    }

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
            RuneVelocity * ImpactVelocityInheritance;

        ShardMeshComponent->WakeAllRigidBodies();
        ShardMeshComponent->AddImpulse(Impulse, NAME_None, false);
        ShardMeshComponent->AddAngularImpulseInDegrees(
            FMath::VRand() * FMath::FRandRange(400.f, 1400.f),
            NAME_None,
            false);

        ShardActor->SetLifeSpan(ShardLifetime);
    }
}

void APickupActorAAARuneInstrument::ApplyRunePhysicsTuning()
{
    ApplyPickupPhysicsTuning();
}

UStaticMesh* APickupActorAAARuneInstrument::ResolveShardMesh() const
{
    return ShardMeshOverride ? ShardMeshOverride.Get() : DefaultShardMesh.Get();
}

