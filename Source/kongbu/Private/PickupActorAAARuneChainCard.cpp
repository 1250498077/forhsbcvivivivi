#include "PickupActorAAARuneChainCard.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
    TMap<uint32, TSet<FString>> BrokenCanvasLinkEdgeKeysByWorld;

    const TArray<TArray<int32>> &GetChainCardExpectedSequences()
    {
        static const TArray<TArray<int32>> Sequences = {
            {380, 419, 418, 417, 416, 415, 414, 413, 412, 411, 410} // 此处因图片截断，后续元素未显示完整
        };
        return Sequences;
    }


    TSet<FString> &GetBrokenCanvasLinkEdgeKeysForWorld(const UWorld *World)
    {
        static TSet<FString> EmptyBrokenEdgeKeys;
        if (!World)
        {
            EmptyBrokenEdgeKeys.Reset();
            return EmptyBrokenEdgeKeys;
        }

        return BrokenCanvasLinkEdgeKeysByWorld.FindOrAdd(World->GetUniqueID());
    }

    bool AreCanvasLocationsEquivalent(const FVector &A, const FVector &B)
    {
        return A.Equals(B, 0.1f);
    }

    FVector GetCanvasLinkAnchorLocation(const APickupActorAAARuneChainCard *Canvas)
    {
        return IsValid(Canvas) ? Canvas->GetCanvasLinkAnchorWorldLocation() : FVector::ZeroVector;
    }

    float GetEffectiveCanvasLinkDistance(
        const APickupActorAAARuneChainCard *A,
        const APickupActorAAARuneChainCard *B)
    {
        if (!IsValid(A) || !IsValid(B))
        {
            return 0.f;
        }

        return FMath::Min(A->GetConfiguredCanvasLinkDistance(), B->GetConfiguredCanvasLinkDistance());
    }

    bool AreCanvasesLinked(const APickupActorAAARuneChainCard *A, const APickupActorAAARuneChainCard *B)
    {
        if (!IsValid(A) || !IsValid(B) || A == B)
        {
            return false;
        }

        if (!A->CanParticipateInCanvasLinks() || !B->CanParticipateInCanvasLinks())
        {
            return false;
        }

        return FVector::DistSquared(GetCanvasLinkAnchorLocation(A), GetCanvasLinkAnchorLocation(B)) <= FMath::Square(GetEffectiveCanvasLinkDistance(A, B));
    }

    FString BuildCanvasLinkEdgeKey(const APickupActorAAARuneChainCard *A, const APickupActorAAARuneChainCard *B)
    {
        if (!IsValid(A) || !IsValid(B))
        {
            return FString();
        }

        const FString FirstPath = A->GetPathName();
        const FString SecondPath = B->GetPathName();
        return FirstPath < SecondPath
                   ? FirstPath + TEXT("|") + SecondPath
                   : SecondPath + TEXT("|") + FirstPath;
    }

    bool IsCanvasLinkEdgeBroken(const APickupActorAAARuneChainCard *A, const APickupActorAAARuneChainCard *B)
    {
        return IsValid(A) && GetBrokenCanvasLinkEdgeKeysForWorld(A->GetWorld()).Contains(BuildCanvasLinkEdgeKey(A, B));
    }

    bool IsCanvasPathPartOfEdgeKey(const FString &EdgeKey, const FString &CanvasPath)
    {
        FString FirstPath;
        FString SecondPath;
        if (!EdgeKey.Split(TEXT("|"), &FirstPath, &SecondPath))
        {
            return false;
        }

        return FirstPath == CanvasPath || SecondPath == CanvasPath;
    }

    void GatherAttachedRuneCanvases(UWorld *World, TArray<APickupActorAAARuneChainCard *> &OutCanvases)
    {
        OutCanvases.Reset();

        if (!World)
        {
            return;
        }

        for (TActorIterator<APickupActorAAARuneChainCard> It(World); It; ++It)
        {
            APickupActorAAARuneChainCard *Canvas = *It;
            if (!IsValid(Canvas) || !Canvas->CanParticipateInCanvasLinks())
            {
                continue;
            }

            OutCanvases.Add(Canvas);
        }
    }

    bool HasAnyUnbrokenCanvasLink(const APickupActorAAARuneChainCard *Canvas, const TArray<APickupActorAAARuneChainCard *> &AllCanvases)
    {
        if (!IsValid(Canvas) || !Canvas->CanParticipateInCanvasLinks())
        {
            return false;
        }

        for (const APickupActorAAARuneChainCard *Candidate : AllCanvases)
        {
            if (!IsValid(Candidate) || Candidate == Canvas || !AreCanvasesLinked(Canvas, Candidate) || IsCanvasLinkEdgeBroken(Canvas, Candidate))
            {
                continue;
            }

            return true;
        }

        return false;
    }
}

APickupActorAAARuneChainCard::APickupActorAAARuneChainCard()
{
    CanvasLinkRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("CanvasLinkRootComponent"));
    CanvasLinkRootComponent->SetupAttachment(VisualMeshRootComponent);
}

void APickupActorAAARuneChainCard::BeginPlay()
{
    Super::BeginPlay();
}

void APickupActorAAARuneChainCard::OnConstruction(const FTransform &Transform)
{
    Super::OnConstruction(Transform);
}

void APickupActorAAARuneChainCard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearChainLinks();
    DestroyBrokenChainLinks();
    Super::EndPlay(EndPlayReason);
}

void APickupActorAAARuneChainCard::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateChainLinkPulseVisuals(DeltaTime);
    UpdateBrokenChainLinkVisuals(DeltaTime);

    if (!IsAttachedToSurface())
    {
        return;
    }

    LinkRefreshAccumulator += DeltaTime;
    if (LinkRefreshAccumulator < FMath::Max(LinkRefreshInterval, 0.02f))
    {
        return;
    }

    LinkRefreshAccumulator = 0.f;
    RefreshCanvasLinks();
}

void APickupActorAAARuneChainCard::OnPickedUp()
{
    Super::OnPickedUp();
}

void APickupActorAAARuneChainCard::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    Super::OnPutDown(PlaceLocation, PlaceRotation);
}

void APickupActorAAARuneChainCard::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    Super::OnThrown(ThrowDirection, ThrowForce);
}

void APickupActorAAARuneChainCard::OnRuneCanvasAttachedToSurface()
{
    LinkRefreshAccumulator = LinkRefreshInterval;
    RefreshCanvasLinks();
}

void APickupActorAAARuneChainCard::OnRuneCanvasDetachedFromSurface()
{
    LinkRefreshAccumulator = 0.f;
    ClearChainLinks();
}

const TArray<int32> &APickupActorAAARuneChainCard::GetExpectedNodeSequenceForCurrentCard() const
{
    static const TArray<int32> EmptySequence;
    const TArray<TArray<int32>> &ExpectedSequences = GetChainCardExpectedSequences();
    const int32 CardResourceIndex = GetCurrentCardResourceIndex();
    return ExpectedSequences.IsValidIndex(CardResourceIndex)
               ? ExpectedSequences[CardResourceIndex]
               : EmptySequence;
}

bool APickupActorAAARuneChainCard::CanParticipateInCanvasLinks() const
{
    return IsAttachedToSurface() && !IsHeldByPlayer() && !IsDisabledByRage();
}

float APickupActorAAARuneChainCard::GetConfiguredCanvasLinkDistance() const
{
    return LinkDistance;
}

FVector APickupActorAAARuneChainCard::GetCanvasLinkAnchorWorldLocation() const
{
    if (DrawSurfaceComponent)
    {
        return DrawSurfaceComponent->GetComponentTransform().TransformPosition(ChainLinkAnchorLocalOffset);
    }

    return GetActorLocation();
}

void APickupActorAAARuneChainCard::RefreshCanvasLinks()
{
    UWorld *World = GetWorld();
    if (!World || !CanParticipateInCanvasLinks())
    {
        ClearChainLinks();
        return;
    }

    TArray<APickupActorAAARuneChainCard *> AllCanvases;
    GatherAttachedRuneCanvases(World, AllCanvases);

    TArray<FRuneCanvasLinkEdge> LinkEdges;
    const FString ThisPathName = GetPathName();
    const FVector ThisAnchor = GetCanvasLinkAnchorLocation(this);

    for (APickupActorAAARuneChainCard *Candidate : AllCanvases)
    {
        if (!IsValid(Candidate) || Candidate == this || !AreCanvasesLinked(this, Candidate) || IsCanvasLinkEdgeBroken(this, Candidate))
        {
            continue;
        }

        if (ThisPathName > Candidate->GetPathName())
        {
            continue;
        }

        FRuneCanvasLinkEdge LinkEdge;
        LinkEdge.EdgeKey = BuildCanvasLinkEdgeKey(this, Candidate);
        LinkEdge.Start = ThisAnchor;
        LinkEdge.End = GetCanvasLinkAnchorLocation(Candidate);
        LinkEdges.Add(LinkEdge);
    }

    const FString NewBuildSignature = BuildChainLinkBuildSignature(LinkEdges);
    if (!NewBuildSignature.IsEmpty() && NewBuildSignature == ActiveChainLinkBuildSignature)
    {
        return;
    }

    RebuildChainLinks(LinkEdges);
}

FString APickupActorAAARuneChainCard::BuildChainLinkBuildSignature(const TArray<FRuneCanvasLinkEdge> &LinkEdges) const
{
    TArray<FString> EdgeSignatures;
    EdgeSignatures.Reserve(LinkEdges.Num());
    EdgeSignatures.Add(FString::Printf(
        TEXT("settings:spacing-%.2f:max=%d:scale=%.2f,%.2f,%.2f:mesh=%s"),
        ChainLinkSpacing,
        MaxChainLinksPerEdge,
        ChainLinkScale.X,
        ChainLinkScale.Y,
        ChainLinkScale.Z,
        *GetNameSafe(ChainLinkMesh)));

    for (const FRuneCanvasLinkEdge &LinkEdge : LinkEdges)
    {
        if (LinkEdge.EdgeKey.IsEmpty() || BreakingChainLinkEdges.Contains(LinkEdge.EdgeKey))
        {
            continue;
        }

        const FVector Start = LinkEdge.Start.GridSnap(1.f);
        const FVector End = LinkEdge.End.GridSnap(1.f);
        EdgeSignatures.Add(FString::Printf(
            TEXT("%s:%d,%d,%d>%d,%d,%d"),
            *LinkEdge.EdgeKey,
            FMath::RoundToInt(Start.X),
            FMath::RoundToInt(Start.Y),
            FMath::RoundToInt(Start.Z),
            FMath::RoundToInt(End.X),
            FMath::RoundToInt(End.Y),
            FMath::RoundToInt(End.Z)));
    }

    EdgeSignatures.Sort();
    return FString::Join(EdgeSignatures, TEXT(";"));
}

void APickupActorAAARuneChainCard::RebuildChainLinks(const TArray<FRuneCanvasLinkEdge> &LinkEdges)
{
    ClearChainLinks(true);
    ActiveChainLinkBuildSignature = BuildChainLinkBuildSignature(LinkEdges);

    if (!bRenderLinkChains || !CanvasLinkRootComponent || !ChainLinkMesh || LinkEdges.IsEmpty())
    {
        return;
    }

    const float SafeSpacing = FMath::Max(1.f, ChainLinkSpacing);

    for (const FRuneCanvasLinkEdge &LinkEdge : LinkEdges)
    {
        if (LinkEdge.EdgeKey.IsEmpty() || BreakingChainLinkEdges.Contains(LinkEdge.EdgeKey) || GetBrokenCanvasLinkEdgeKeysForWorld(GetWorld()).Contains(LinkEdge.EdgeKey))
        {
            continue;
        }

        const FVector StartWorld = LinkEdge.Start;
        const FVector EndWorld = LinkEdge.End;
        const FVector Delta = EndWorld - StartWorld;
        const float Distance = Delta.Size();
        if (Distance <= UE_KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const FVector Direction = Delta / Distance;
        const int32 ChainLinkCount = FMath::Clamp(FMath::CeilToInt(Distance / SafeSpacing), 1, FMath::Max(1, MaxChainLinksPerEdge));
        const float StepDistance = Distance / static_cast<float>(ChainLinkCount);
        const FRotator BaseRotation = Direction.Rotation() + ChainLinkRotationOffset;

        for (int32 ChainLinkIndex = 0; ChainLinkIndex < ChainLinkCount; ++ChainLinkIndex)
        {
            UStaticMeshComponent *ChainLinkComponent = NewObject<UStaticMeshComponent>(this);
            if (!ChainLinkComponent)
            {
                continue;
            }

            FRotator ChainRotation = BaseRotation;
            if (bAlternateChainLinkRoll && (ChainLinkIndex % 2) == 1)
            {
                ChainRotation.Roll += AlternateChainLinkRollDegrees;
            }

            const FVector ChainLocation = StartWorld + Direction * ((static_cast<float>(ChainLinkIndex) + 0.5f) * StepDistance);

            ChainLinkComponent->SetupAttachment(CanvasLinkRootComponent);
            ChainLinkComponent->SetMobility(EComponentMobility::Movable);
            if (ChainLinkTouchedMaterialOverride)
            {
                ChainLinkComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                ChainLinkComponent->SetCollisionObjectType(ECC_WorldDynamic);
                ChainLinkComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
                ChainLinkComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
                ChainLinkComponent->SetGenerateOverlapEvents(true);
                ChainLinkComponent->OnComponentBeginOverlap.AddDynamic(this, &APickupActorAAARuneChainCard::HandleChainLinkBeginOverlap);
                ChainLinkComponent->OnComponentEndOverlap.AddDynamic(this, &APickupActorAAARuneChainCard::HandleChainLinkEndOverlap);
            }
            else
            {
                ChainLinkComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                ChainLinkComponent->SetGenerateOverlapEvents(false);
            }
            ChainLinkComponent->SetCanEverAffectNavigation(false);
            ChainLinkComponent->SetCastShadow(false);
            ChainLinkComponent->SetVisibleInRayTracing(false);
            ChainLinkComponent->SetStaticMesh(ChainLinkMesh);
            ChainLinkComponent->RegisterComponent();
            ChainLinkComponent->SetWorldLocationAndRotation(ChainLocation, ChainRotation);
            ChainLinkComponent->SetWorldScale3D(ChainLinkScale);

            ApplyChainLinkDefaultMaterial(ChainLinkComponent);

            ActiveChainLinkComponents.Add(ChainLinkComponent);
            ChainLinkEdgeKeys.Add(ChainLinkComponent, LinkEdge.EdgeKey);
            ActiveChainLinkComponentsByEdge.FindOrAdd(LinkEdge.EdgeKey).Add(ChainLinkComponent);
        }
    }
}

void APickupActorAAARuneChainCard::ClearChainLinks(bool bPreserveBreakingEdges)
{
    TArray<TObjectPtr<UStaticMeshComponent>> PreservedChainLinkComponents;
    TArray<TObjectPtr<UMaterialInstanceDynamic>> PreservedMaterialInstances;
    TMap<TObjectPtr<UStaticMeshComponent>, TObjectPtr<UMaterialInstanceDynamic>> PreservedDefaultMaterialInstances;
    TMap<TObjectPtr<UStaticMeshComponent>, TObjectPtr<UMaterialInstanceDynamic>> PreservedTouchedMaterialInstances;
    TMap<TObjectPtr<UStaticMeshComponent>, FString> PreservedEdgeKeys;
    TMap<FString, TArray<TObjectPtr<UStaticMeshComponent>>> PreservedComponentsByEdge;

    for (UStaticMeshComponent *ChainLinkComponent : ActiveChainLinkComponents)
    {
        const FString *EdgeKey = ChainLinkEdgeKeys.Find(ChainLinkComponent);
        if (bPreserveBreakingEdges && EdgeKey && BreakingChainLinkEdges.Contains(*EdgeKey))
        {
            PreservedChainLinkComponents.Add(ChainLinkComponent);
            PreservedEdgeKeys.Add(ChainLinkComponent, *EdgeKey);
            PreservedComponentsByEdge.FindOrAdd(*EdgeKey).Add(ChainLinkComponent);

            if (TObjectPtr<UMaterialInstanceDynamic> *DefaultMaterial = ChainLinkDefaultMaterialInstances.Find(ChainLinkComponent))
            {
                PreservedDefaultMaterialInstances.Add(ChainLinkComponent, *DefaultMaterial);
                PreservedMaterialInstances.AddUnique(*DefaultMaterial);
            }

            if (TObjectPtr<UMaterialInstanceDynamic> *TouchedMaterial = ChainLinkTouchedMaterialInstances.Find(ChainLinkComponent))
            {
                PreservedTouchedMaterialInstances.Add(ChainLinkComponent, *TouchedMaterial);
                PreservedMaterialInstances.AddUnique(*TouchedMaterial);
            }

            continue;
        }

        if (IsValid(ChainLinkComponent))
        {
            ChainLinkComponent->DestroyComponent();
        }
    }

    ActiveChainLinkComponents = MoveTemp(PreservedChainLinkComponents);
    ActiveChainLinkMaterialInstances = MoveTemp(PreservedMaterialInstances);
    ChainLinkDefaultMaterialInstances = MoveTemp(PreservedDefaultMaterialInstances);
    ChainLinkTouchedMaterialInstances = MoveTemp(PreservedTouchedMaterialInstances);
    ChainLinkEdgeKeys = MoveTemp(PreservedEdgeKeys);
    ActiveChainLinkComponentsByEdge = MoveTemp(PreservedComponentsByEdge);
    if (!bPreserveBreakingEdges)
    {
        ActiveChainLinkBuildSignature.Empty();
    }
}

void APickupActorAAARuneChainCard::DestroyBrokenChainLinks()
{
    for (UStaticMeshComponent *ChainLinkComponent : BrokenChainLinkComponents)
    {
        if (IsValid(ChainLinkComponent))
        {
            ChainLinkComponent->DestroyComponent();
        }
    }

    BrokenChainLinkComponents.Reset();
    BrokenChainLinkVisualStates.Reset();
}

void APickupActorAAARuneChainCard::ApplyChainLinkDefaultMaterial(UStaticMeshComponent *ChainLinkComponent)
{
    if (!IsValid(ChainLinkComponent))
    {
        return;
    }

    UMaterialInstanceDynamic *DynamicMaterial = nullptr;
    if (TObjectPtr<UMaterialInstanceDynamic> *ExistingMaterial = ChainLinkDefaultMaterialInstances.Find(ChainLinkComponent))
    {
        DynamicMaterial = ExistingMaterial->Get();
    }

    if (!IsValid(DynamicMaterial))
    {
        UMaterialInterface *BaseMaterial = ChainLinkMaterialOverride
                                               ? ChainLinkMaterialOverride.Get()
                                               : ChainLinkComponent->GetMaterial(0);
        if (!BaseMaterial)
        {
            return;
        }

        DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        if (!DynamicMaterial)
        {
            return;
        }

        ChainLinkDefaultMaterialInstances.Add(ChainLinkComponent, DynamicMaterial);
        ActiveChainLinkMaterialInstances.Add(DynamicMaterial);
    }

    DynamicMaterial->SetVectorParameterValue(LinkColorParameterName, LinkGlowColor);
    DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), LinkGlowColor);
    DynamicMaterial->SetScalarParameterValue(LinkGlowScalarParameterName, LinkGlowIntensity);
    DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveIntensity"), LinkGlowIntensity);
    DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), LinkGlowIntensity);
    DynamicMaterial->SetScalarParameterValue(LinkPulseScalarParameterName, bAnimateLinkPulse ? LinkPulseMax : 1.f);
    DynamicMaterial->SetScalarParameterValue(TEXT("EmissivePulse"), bAnimateLinkPulse ? LinkPulseMax : 1.f);

    const int32 MaterialSlotCount = FMath::Max(ChainLinkComponent->GetNumMaterials(), 1);
    for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
    {
        ChainLinkComponent->SetMaterial(MaterialIndex, DynamicMaterial);
    }
}

void APickupActorAAARuneChainCard::ApplyChainLinkTouchedMaterial(UStaticMeshComponent *ChainLinkComponent)
{
    if (!IsValid(ChainLinkComponent) || !ChainLinkTouchedMaterialOverride)
    {
        return;
    }

    UMaterialInstanceDynamic *DynamicMaterial = nullptr;
    if (TObjectPtr<UMaterialInstanceDynamic> *ExistingMaterial = ChainLinkTouchedMaterialInstances.Find(ChainLinkComponent))
    {
        DynamicMaterial = ExistingMaterial->Get();
    }

    if (!IsValid(DynamicMaterial))
    {
        DynamicMaterial = UMaterialInstanceDynamic::Create(ChainLinkTouchedMaterialOverride.Get(), this);
        if (!DynamicMaterial)
        {
            return;
        }

        ChainLinkTouchedMaterialInstances.Add(ChainLinkComponent, DynamicMaterial);
        ActiveChainLinkMaterialInstances.Add(DynamicMaterial);
    }

    DynamicMaterial->SetVectorParameterValue(LinkColorParameterName, LinkGlowColor);
    DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), LinkGlowColor);
    DynamicMaterial->SetScalarParameterValue(LinkGlowScalarParameterName, LinkGlowIntensity);
    DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveIntensity"), LinkGlowIntensity);
    DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), LinkGlowIntensity);
    DynamicMaterial->SetScalarParameterValue(LinkPulseScalarParameterName, bAnimateLinkPulse ? LinkPulseMax : 1.f);
    DynamicMaterial->SetScalarParameterValue(TEXT("EmissivePulse"), bAnimateLinkPulse ? LinkPulseMax : 1.f);

    const int32 MaterialSlotCount = FMath::Max(ChainLinkComponent->GetNumMaterials(), 1);
    for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
    {
        ChainLinkComponent->SetMaterial(MaterialIndex, DynamicMaterial);
    }
}

void APickupActorAAARuneChainCard::HandleChainLinkBeginOverlap(
    UPrimitiveComponent *OverlappedComponent,
    AActor *OtherActor,
    UPrimitiveComponent *OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult &SweepResult)
{
    (void)OtherComp;
    (void)OtherBodyIndex;
    (void)bFromSweep;
    (void)SweepResult;

    if (!Cast<APawn>(OtherActor))
    {
        return;
    }

    UStaticMeshComponent *ChainLinkComponent = Cast<UStaticMeshComponent>(OverlappedComponent);
    if (!IsValid(ChainLinkComponent))
    {
        return;
    }

    if (FString *EdgeKey = ChainLinkEdgeKeys.Find(ChainLinkComponent))
    {
        MarkChainLinkEdgeTouched(*EdgeKey);
        return;
    }

    ApplyChainLinkTouchedMaterial(ChainLinkComponent);
}

void APickupActorAAARuneChainCard::HandleChainLinkEndOverlap(
    UPrimitiveComponent *OverlappedComponent,
    AActor *OtherActor,
    UPrimitiveComponent *OtherComp,
    int32 OtherBodyIndex)
{
    (void)OtherComp;
    (void)OtherBodyIndex;

    if (!Cast<APawn>(OtherActor))
    {
        return;
    }

    UStaticMeshComponent *ChainLinkComponent = Cast<UStaticMeshComponent>(OverlappedComponent);
    if (!IsValid(ChainLinkComponent))
    {
        return;
    }

    TArray<AActor *> OverlappingPawns;
    ChainLinkComponent->GetOverlappingActors(OverlappingPawns, APawn::StaticClass());
    const FString *EdgeKey = ChainLinkEdgeKeys.Find(ChainLinkComponent);
    if (OverlappingPawns.IsEmpty() && (!EdgeKey || !BreakingChainLinkEdges.Contains(*EdgeKey)))
    {
        ApplyChainLinkDefaultMaterial(ChainLinkComponent);
    }
}

void APickupActorAAARuneChainCard::MarkChainLinkEdgeTouched(const FString &EdgeKey)
{
    if (EdgeKey.IsEmpty() || GetBrokenCanvasLinkEdgeKeysForWorld(GetWorld()).Contains(EdgeKey))
    {
        return;
    }

    if (TArray<TObjectPtr<UStaticMeshComponent>> *ChainLinkComponents = ActiveChainLinkComponentsByEdge.Find(EdgeKey))
    {
        for (UStaticMeshComponent *ChainLinkComponent : *ChainLinkComponents)
        {
            ApplyChainLinkTouchedMaterial(ChainLinkComponent);
        }
    }

    if (BreakingChainLinkEdges.Contains(EdgeKey))
    {
        return;
    }

    BreakingChainLinkEdges.Add(EdgeKey);

    FTimerDelegate BreakDelegate;
    BreakDelegate.BindUObject(this, &APickupActorAAARuneChainCard::BreakChainLinkEdge, EdgeKey);
    GetWorldTimerManager().SetTimer(
        ChainLinkBreakTimerHandles.FindOrAdd(EdgeKey),
        BreakDelegate,
        FMath::Max(0.f, ChainLinkBreakDelay),
        false);
}

void APickupActorAAARuneChainCard::BreakChainLinkEdge(FString EdgeKey)
{
    if (EdgeKey.IsEmpty())
    {
        return;
    }

    GetBrokenCanvasLinkEdgeKeysForWorld(GetWorld()).Add(EdgeKey);
    BreakingChainLinkEdges.Remove(EdgeKey);
    ChainLinkBreakTimerHandles.Remove(EdgeKey);

    TArray<TObjectPtr<UStaticMeshComponent>> ChainLinkComponents;
    if (TArray<TObjectPtr<UStaticMeshComponent>> *ExistingComponents = ActiveChainLinkComponentsByEdge.Find(EdgeKey))
    {
        ChainLinkComponents = *ExistingComponents;
    }

    ActiveChainLinkComponentsByEdge.Remove(EdgeKey);

    UWorld *World = GetWorld();
    FVector EdgeCenter = FVector::ZeroVector;
    int32 ValidChainLinkCount = 0;
    for (UStaticMeshComponent *ChainLinkComponent : ChainLinkComponents)
    {
        if (IsValid(ChainLinkComponent))
        {
            EdgeCenter += ChainLinkComponent->GetComponentLocation();
            ++ValidChainLinkCount;
        }
    }
    if (ValidChainLinkCount > 0)
    {
        EdgeCenter /= static_cast<float>(ValidChainLinkCount);
    }

    for (UStaticMeshComponent *ChainLinkComponent : ChainLinkComponents)
    {
        if (!IsValid(ChainLinkComponent))
        {
            continue;
        }

        ActiveChainLinkComponents.Remove(ChainLinkComponent);
        ChainLinkEdgeKeys.Remove(ChainLinkComponent);
        ChainLinkDefaultMaterialInstances.Remove(ChainLinkComponent);
        ChainLinkTouchedMaterialInstances.Remove(ChainLinkComponent);
        ChainLinkComponent->OnComponentBeginOverlap.RemoveAll(this);
        ChainLinkComponent->OnComponentEndOverlap.RemoveAll(this);
        ChainLinkComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

        ChainLinkComponent->SetCollisionEnabled(bBrokenChainLinksCollideWithWorld ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        ChainLinkComponent->SetCollisionObjectType(ECC_WorldDynamic);
        ChainLinkComponent->SetCollisionResponseToAllChannels(bBrokenChainLinksCollideWithWorld ? ECR_Block : ECR_Ignore);
        ChainLinkComponent->SetGenerateOverlapEvents(false);
        ChainLinkComponent->SetSimulatePhysics(bBrokenChainLinksSimulatePhysics);
        ChainLinkComponent->SetEnableGravity(bBrokenChainLinksSimulatePhysics);

        FVector RadialScatterDirection = (ChainLinkComponent->GetComponentLocation() - EdgeCenter).GetSafeNormal();
        if (RadialScatterDirection.IsNearlyZero())
        {
            RadialScatterDirection = FMath::VRand();
        }

        FVector RandomScatterDirection = FMath::VRand();
        RandomScatterDirection.Z = FMath::Abs(RandomScatterDirection.Z);
        const FVector BreakScatterDirection = (RadialScatterDirection * 0.55f + RandomScatterDirection * 0.85f + FVector::UpVector * 0.25f).GetSafeNormal();
        const FVector BreakImpulse = BreakScatterDirection * BrokenChainLinkImpulse;
        if (bBrokenChainLinksSimulatePhysics)
        {
            ChainLinkComponent->WakeAllRigidBodies();
            ChainLinkComponent->AddImpulse(BreakImpulse, NAME_None, true);
        }
        else
        {
            FBrokenChainLinkVisualState VisualState;
            VisualState.Velocity = BreakScatterDirection * FMath::Max(120.f, BrokenChainLinkImpulse * 2.6f);
            VisualState.AngularVelocity = FRotator(
                FMath::FRandRange(-240.f, 240.f),
                FMath::FRandRange(-360.f, 360.f),
                FMath::FRandRange(-360.f, 360.f));
            BrokenChainLinkVisualStates.Add(ChainLinkComponent, VisualState);
        }
        BrokenChainLinkComponents.Add(ChainLinkComponent);

        if (World && BrokenChainLinkLifetime > 0.f)
        {
            FTimerDelegate DestroyDelegate;
            DestroyDelegate.BindUObject(this, &APickupActorAAARuneChainCard::FinishBrokenChainLinkComponent, ChainLinkComponent);
            FTimerHandle DestroyTimerHandle;
            World->GetTimerManager().SetTimer(DestroyTimerHandle, DestroyDelegate, BrokenChainLinkLifetime, false);
        }
    }

    DestroyIfNoActiveCanvasLinks(EdgeKey);
}

void APickupActorAAARuneChainCard::FinishBrokenChainLinkComponent(UStaticMeshComponent *ChainLinkComponent)
{
    BrokenChainLinkComponents.Remove(ChainLinkComponent);
    BrokenChainLinkVisualStates.Remove(ChainLinkComponent);
    if (IsValid(ChainLinkComponent))
    {
        ChainLinkComponent->DestroyComponent();
    }
}

void APickupActorAAARuneChainCard::UpdateBrokenChainLinkVisuals(float DeltaTime)
{
    if (BrokenChainLinkVisualStates.IsEmpty() || DeltaTime <= 0.f)
    {
        return;
    }

    TArray<TObjectPtr<UStaticMeshComponent>> InvalidComponents;
    const float DampingAlpha = FMath::Clamp(BrokenChainLinkVisualDamping, 0.f, 1.f);
    const float DampingFactor = FMath::Pow(1.f - DampingAlpha, DeltaTime);

    for (TPair<TObjectPtr<UStaticMeshComponent>, FBrokenChainLinkVisualState> &Entry : BrokenChainLinkVisualStates)
    {
        UStaticMeshComponent *ChainLinkComponent = Entry.Key.Get();
        if (!IsValid(ChainLinkComponent))
        {
            InvalidComponents.Add(Entry.Key);
            continue;
        }

        FBrokenChainLinkVisualState &VisualState = Entry.Value;
        VisualState.Velocity.Z -= FMath::Max(0.f, BrokenChainLinkVisualGravity) * DeltaTime;
        VisualState.Velocity *= DampingFactor;
        VisualState.AngularVelocity.Pitch *= DampingFactor;
        VisualState.AngularVelocity.Yaw *= DampingFactor;
        VisualState.AngularVelocity.Roll *= DampingFactor;

        ChainLinkComponent->AddWorldOffset(VisualState.Velocity * DeltaTime, false, nullptr, ETeleportType::TeleportPhysics);
        ChainLinkComponent->AddWorldRotation(VisualState.AngularVelocity * DeltaTime, false, nullptr, ETeleportType::TeleportPhysics);
    }

    for (UStaticMeshComponent *ChainLinkComponent : InvalidComponents)
    {
        BrokenChainLinkVisualStates.Remove(ChainLinkComponent);
    }
}

void APickupActorAAARuneChainCard::DestroyIfNoActiveCanvasLinks(const FString &BrokenEdgeKey)
{
    UWorld *World = GetWorld();
    if (!World)
    {
        return;
    }

    TArray<APickupActorAAARuneChainCard *> AllCanvases;
    GatherAttachedRuneCanvases(World, AllCanvases);

    TArray<APickupActorAAARuneChainCard *> CanvasesToDestroy;
    for (APickupActorAAARuneChainCard *Canvas : AllCanvases)
    {
        if (!IsValid(Canvas) || !IsCanvasPathPartOfEdgeKey(BrokenEdgeKey, Canvas->GetPathName()))
        {
            continue;
        }

        if (!HasAnyUnbrokenCanvasLink(Canvas, AllCanvases))
        {
            CanvasesToDestroy.Add(Canvas);
        }
    }

    for (APickupActorAAARuneChainCard *Canvas : CanvasesToDestroy)
    {
        if (!IsValid(Canvas))
        {
            continue;
        }

        Canvas->OnRuneCanvasDetachedFromSurface();
        Canvas->DisableAndHideRuneCanvasForLifetime(BrokenChainLinkLifetime + 0.05f);
    }
}

void APickupActorAAARuneChainCard::UpdateChainLinkPulseVisuals(float DeltaTime)
{
    if (ActiveChainLinkMaterialInstances.IsEmpty())
    {
        LinkPulseTimeAccumulator = 0.f;
        return;
    }

    float PulseValue = 1.f;
    float EffectiveGlowIntensity = LinkGlowIntensity;
    if (bAnimateLinkPulse)
    {
        LinkPulseTimeAccumulator += DeltaTime * FMath::Max(LinkPulseSpeed, 0.f);
        const float PulseAlpha = (FMath::Sin(LinkPulseTimeAccumulator) + 1.f) * 0.5f;
        PulseValue = FMath::Lerp(LinkPulseMin, LinkPulseMax, PulseAlpha);
        EffectiveGlowIntensity = LinkGlowIntensity * FMath::Max(PulseValue, 0.f);
    }

    for (UMaterialInstanceDynamic *DynamicMaterial : ActiveChainLinkMaterialInstances)
    {
        if (!IsValid(DynamicMaterial))
        {
            continue;
        }

        DynamicMaterial->SetVectorParameterValue(LinkColorParameterName, LinkGlowColor);
        DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), LinkGlowColor);
        DynamicMaterial->SetScalarParameterValue(LinkGlowScalarParameterName, EffectiveGlowIntensity);
        DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveIntensity"), EffectiveGlowIntensity);
        DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), EffectiveGlowIntensity);
        DynamicMaterial->SetScalarParameterValue(LinkPulseScalarParameterName, PulseValue);
        DynamicMaterial->SetScalarParameterValue(TEXT("EmissivePulse"), PulseValue);
    }
}
