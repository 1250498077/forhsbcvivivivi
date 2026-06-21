#include "PickupActorAAARuneCanvasInstrument.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodyInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CollisionShape.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    const FString CardResourceScanPath = TEXT("/Game/item/canvas/cardtype");
    const FString CardResourcePrefix = TEXT("card_resource_");
    TMap<uint32, TSet<FString>> BrokenCanvasLinkEdgeKeysByWorld;

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

    const TArray<TArray<int32>> &GetHardcodedCardExpectedSequences()
    {
        // card_resource_0 -> {1, 2, 3, 4}. 后续你从 log 复制后，在这里继续按下标追加。
        static const TArray<TArray<int32>> Sequences = {
            {380, 419, 418, 417, 416, 415, 414, 413, 412, 411, 410, 409, 408, 407, 406, 446, 447, 448, 449, 450, 451, 452, 453, 454, 455, 456, 457, 458, 459, 500, 501, 502, 503, 504, 505, 506, 507, 508, 465, 464, 463, 462, 421, 420, 378, 377, 376, 335, 334, 293, 292, 291, 251, 250, 210, 331, 372, 493, 533, 573, 613, 574, 575, 536, 537, 498, 499, 460, 461, 424, 425, 426, 427, 388, 389, 390, 391, 392, 353, 354, 355, 356, 357, 358, 319, 318, 317, 316, 315, 314, 313, 352, 351, 350, 349, 348, 347, 346, 345, 384, 383, 382, 381, 379, 375, 374}};
        return Sequences;
    }

    bool TryParseCardResourceIndex(const FString &AssetName, int32 &OutIndex)
    {
        OutIndex = INDEX_NONE;
        if (!AssetName.StartsWith(CardResourcePrefix, ESearchCase::IgnoreCase))
        {
            return false;
        }

        const FString IndexString = AssetName.RightChop(CardResourcePrefix.Len());
        if (IndexString.IsEmpty())
        {
            return false;
        }

        for (const TCHAR Ch : IndexString)
        {
            if (!FChar::IsDigit(Ch))
            {
                return false;
            }
        }

        OutIndex = FCString::Atoi(*IndexString);
        return OutIndex >= 0;
    }

    FString BuildNodeSequenceLogString(const TArray<int32> &NodeSequence)
    {
        FString Result = TEXT("[");
        for (int32 Index = 0; Index < NodeSequence.Num(); ++Index)
        {
            if (Index > 0)
            {
                Result += TEXT(",");
            }
            Result += FString::FromInt(NodeSequence[Index]);
        }
        Result += TEXT("]");
        return Result;
    }

    bool AreCanvasLocationsEquivalent(const FVector &A, const FVector &B)
    {
        return A.Equals(B, 0.1f);
    }

    bool HasMatchingCanvasEdge(const TArray<TPair<FVector, FVector>> &Edges, const FVector &First, const FVector &Second)
    {
        return Edges.ContainsByPredicate(
            [&](const TPair<FVector, FVector> &ExistingEdge)
            {
                return (AreCanvasLocationsEquivalent(ExistingEdge.Key, First) && AreCanvasLocationsEquivalent(ExistingEdge.Value, Second)) || (AreCanvasLocationsEquivalent(ExistingEdge.Key, Second) && AreCanvasLocationsEquivalent(ExistingEdge.Value, First));
            });
    }

    void AddUniqueCanvasEdge(TArray<TPair<FVector, FVector>> &Edges, const FVector &First, const FVector &Second)
    {
        if (AreCanvasLocationsEquivalent(First, Second) || HasMatchingCanvasEdge(Edges, First, Second))
        {
            return;
        }

        Edges.Emplace(First, Second);
    }

    FVector GetCanvasLinkAnchorLocation(const APickupActorAAARuneCanvasInstrument *Canvas)
    {
        return IsValid(Canvas) ? Canvas->GetCanvasLinkAnchorWorldLocation() : FVector::ZeroVector;
    }

    float GetEffectiveCanvasLinkDistance(
        const APickupActorAAARuneCanvasInstrument *A,
        const APickupActorAAARuneCanvasInstrument *B)
    {
        if (!IsValid(A) || !IsValid(B))
        {
            return 0.f;
        }

        return FMath::Min(A->GetConfiguredCanvasLinkDistance(), B->GetConfiguredCanvasLinkDistance());
    }

    bool AreCanvasesLinked(const APickupActorAAARuneCanvasInstrument *A, const APickupActorAAARuneCanvasInstrument *B)
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

    FString BuildCanvasLinkEdgeKey(const APickupActorAAARuneCanvasInstrument *A, const APickupActorAAARuneCanvasInstrument *B)
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

    bool IsCanvasLinkEdgeBroken(const APickupActorAAARuneCanvasInstrument *A, const APickupActorAAARuneCanvasInstrument *B)
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

    bool HasAnyUnbrokenCanvasLink(const APickupActorAAARuneCanvasInstrument *Canvas, const TArray<APickupActorAAARuneCanvasInstrument *> &AllCanvases)
    {
        if (!IsValid(Canvas) || !Canvas->CanParticipateInCanvasLinks())
        {
            return false;
        }

        for (const APickupActorAAARuneCanvasInstrument *Candidate : AllCanvases)
        {
            if (!IsValid(Candidate) || Candidate == Canvas || !AreCanvasesLinked(Canvas, Candidate) || IsCanvasLinkEdgeBroken(Canvas, Candidate))
            {
                continue;
            }

            return true;
        }

        return false;
    }


    void GatherAttachedRuneCanvases(UWorld *World, TArray<APickupActorAAARuneCanvasInstrument *> &OutCanvases)
    {
        OutCanvases.Reset();

        if (!World)
        {
            return;
        }

        for (TActorIterator<APickupActorAAARuneCanvasInstrument> It(World); It; ++It)
        {
            APickupActorAAARuneCanvasInstrument *Canvas = *It;
            if (!IsValid(Canvas) || !Canvas->CanParticipateInCanvasLinks())
            {
                continue;
            }

            OutCanvases.Add(Canvas);
        }
    }
}

APickupActorAAARuneCanvasInstrument::APickupActorAAARuneCanvasInstrument()
{
    PrimaryActorTick.bCanEverTick = true;

    HoldType = EHoldItemType::RuneGridInstrument;
    FP_SocketName = TEXT("RightSocket");
    FP_LocationOffset = FVector::ZeroVector;
    FP_RotationOffset = FRotator::ZeroRotator;
    TP_SocketName = TEXT("RightSocket");
    TP_LocationOffset = FVector::ZeroVector;
    TP_RotationOffset = FRotator::ZeroRotator;

    ItemMassKg = 1.2f;
    ItemThrowForceMultiplier = 0.85f;
    ItemLinearDamping = 0.12f;
    ItemAngularDamping = 0.45f;
    ItemThrowSpinRateDegrees = 720.f;

    Tags.Add(FName("RuneCanvasInstrument"));
    Tags.Add(FName("Pickup"));
    Tags.Add(FName("Rune"));

    if (MeshComponent)
    {
        MeshComponent->SetNotifyRigidBodyCollision(true);
        MeshComponent->BodyInstance.bUseCCD = true;
        MeshComponent->OnComponentHit.AddDynamic(this, &APickupActorAAARuneCanvasInstrument::HandleRuneCanvasHit);
    }

    DrawSurfaceComponent = CreateDefaultSubobject<USceneComponent>(TEXT("DrawSurfaceComponent"));
    DrawSurfaceComponent->SetupAttachment(VisualMeshRootComponent);

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
    CanvasLinkRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("CanvasLinkRootComponent"));
    CanvasLinkRootComponent->SetupAttachment(VisualMeshRootComponent);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultPlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (DefaultPlaneMesh.Succeeded())
    {
        DefaultRecognitionGridPreviewNodeMesh = DefaultPlaneMesh.Object;
        RecognitionGridPreviewNodeMesh = DefaultPlaneMesh.Object;
    }
}

void APickupActorAAARuneCanvasInstrument::BeginPlay()
{
    Super::BeginPlay();
    RebuildRecognitionGridPreview();
    LoadCardResources();
    EnsureDrawResources();
    ApplyThrowablePhysicsTuning();
    ApplyCurrentCardResourceTexture();
    UpdateActivationVisualState();
}

void APickupActorAAARuneCanvasInstrument::OnConstruction(const FTransform &Transform)
{
    Super::OnConstruction(Transform);
    RebuildRecognitionGridPreview();
    ApplyThrowablePhysicsTuning();

    if (GlowLightComponent)
    {
        GlowLightComponent->SetHiddenInGame(true);
        GlowLightComponent->SetVisibility(false);
        GlowLightComponent->SetIntensity(0.f);
    }
}

void APickupActorAAARuneCanvasInstrument::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DrawnUVPoints.Reset();
    RecognizedNodeSequence.Reset();
    LoadedCardResources.Reset();
    ClearRecognitionGridPreview();
    ClearChainLinks();
    DestroyBrokenChainLinks();
    DrawRenderTarget = nullptr;
    CardDynamicMaterial = nullptr;
    bAwaitingThrowImpact = false;
    bAttachedToSurface = false;
    LastAttachTraceLocation = FVector::ZeroVector;

    Super::EndPlay(EndPlayReason);
}

void APickupActorAAARuneCanvasInstrument::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateThrownAttachTrace();
    UpdateChainLinkPulseVisuals(DeltaTime);

    if (!bAttachedToSurface)
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

void APickupActorAAARuneCanvasInstrument::OnPickedUp()
{
    bAwaitingThrowImpact = false;
    bAttachedToSurface = false;
    LastAttachTraceLocation = FVector::ZeroVector;
    LinkRefreshAccumulator = 0.f;
    ClearChainLinks();
    RestoreDefaultThrowableCollision();
    Super::OnPickedUp();
    ResetRuneState();
    UpdateActivationVisualState();
}

void APickupActorAAARuneCanvasInstrument::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    bAwaitingThrowImpact = false;
    bAttachedToSurface = false;
    LastAttachTraceLocation = FVector::ZeroVector;
    LinkRefreshAccumulator = 0.f;
    ClearChainLinks();
    RestoreDefaultThrowableCollision();
    Super::OnPutDown(PlaceLocation, PlaceRotation);
    ResetRuneState();
    UpdateActivationVisualState();
}

void APickupActorAAARuneCanvasInstrument::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    bAwaitingThrowImpact = true;
    bAttachedToSurface = false;
    LastAttachTraceLocation = GetActorLocation();
    LinkRefreshAccumulator = 0.f;
    ClearChainLinks();
    RestoreDefaultThrowableCollision();
    Super::OnThrown(ThrowDirection, ThrowForce);
    ResetRuneState();
    UpdateActivationVisualState();
}

void APickupActorAAARuneCanvasInstrument::HandleRuneCanvasHit(
    UPrimitiveComponent *HitComponent,
    AActor *OtherActor,
    UPrimitiveComponent *OtherComp,
    FVector NormalImpulse,
    const FHitResult &Hit)
{
    (void)NormalImpulse;

    if (!bAwaitingThrowImpact || bAttachedToSurface || HitComponent != MeshComponent || !Hit.bBlockingHit)
    {
        return;
    }

    if (OtherActor == this || (!IsValid(OtherActor) && !IsValid(OtherComp)))
    {
        return;
    }

    StickToImpact(Hit, OtherComp);
    bAttachedToSurface = true;
    UpdateActivationVisualState();
    LinkRefreshAccumulator = LinkRefreshInterval;
    RefreshCanvasLinks();

    UE_LOG(LogTemp, Log, TEXT("%s rune canvas attached to %s"), *GetName(), *GetNameSafe(OtherActor));
}

bool APickupActorAAARuneCanvasInstrument::BeginRuneDraw(APlayerController *UsingController)
{
    if (!UsingController || !EnsureDrawResources())
    {
        return false;
    }

    bRuneDrawActive = true;
    bRuneStrokeActive = false;
    bPatternSolved = false;
    bCanConnectNextDrawPoint = false;
    bCanInterpolateNextRecognizedNode = false;
    SolvedPatternId = NAME_None;
    DrawnUVPoints.Reset();
    RecognizedNodeSequence.Reset();
    EnableMouseTraceCollisionIfNeeded();

    if (bClearTextureOnBeginDraw)
    {
        ClearDrawTexture();
    }

    ReceiveRuneCanvasDrawStateChanged(DrawnUVPoints, true);
    ReceiveRuneCanvasNodeSequenceChanged(RecognizedNodeSequence, INDEX_NONE);
    return true;
}

bool APickupActorAAARuneCanvasInstrument::BeginRuneStroke(
    APlayerController *UsingController,
    const FVector2D &ScreenPosition)
{
    if (!bRuneDrawActive || !UsingController)
    {
        return false;
    }

    bRuneStrokeActive = true;
    bCanConnectNextDrawPoint = false;
    bCanInterpolateNextRecognizedNode = false;
    UpdateRuneDrawFromScreenPosition(UsingController, ScreenPosition);
    return true;
}

// 2. 结束绘制笔触
void APickupActorAAARuneCanvasInstrument::EndRuneStroke()
{
    bRuneStrokeActive = false;
    bCanConnectNextDrawPoint = false;
    bCanInterpolateNextRecognizedNode = false;
}

// 3. 根据屏幕位置更新绘制
void APickupActorAAARuneCanvasInstrument::UpdateRuneDrawFromScreenPosition(
    APlayerController *UsingController,
    const FVector2D &ScreenPosition)
{
    if (!bRuneDrawActive || !bRuneStrokeActive || !UsingController)
    {
        return;
    }

    FVector2D UV = FVector2D::ZeroVector;
    if (!ResolveDrawUVFromScreenPosition(UsingController, ScreenPosition, UV))
    {
        bCanConnectNextDrawPoint = false;
        bCanInterpolateNextRecognizedNode = true;
        return;
    }

    AddDrawPoint(UV);

    if (bEnableHiddenNodeRecognition)
    {
        TryAppendRecognizedNode(ResolveHiddenNodeFromUV(UV));
    }
}

TArray<int32> APickupActorAAARuneCanvasInstrument::EndRuneDraw(APlayerController *UsingController)
{
    (void)UsingController;

    if (!bRuneDrawActive)
    {
        return RecognizedNodeSequence;
    }

    bRuneDrawActive = false;
    EndRuneStroke();
    bCanConnectNextDrawPoint = false;
    RestoreHeldCollisionAfterMouseTrace();
    SolvedPatternId = NAME_None;
    bPatternSolved = TryResolveAcceptedPattern(RecognizedNodeSequence, SolvedPatternId);
    LogRecognizedNodeSequence(TEXT("EndRuneDraw"));
    LogCurrentCardAndUserSequences(TEXT("EndRuneDraw"), RecognizedNodeSequence);
    ReceiveRuneCanvasDrawStateChanged(DrawnUVPoints, false);

    return RecognizedNodeSequence;
}

void APickupActorAAARuneCanvasInstrument::ResetRuneState()
{
    bRuneDrawActive = false;
    bRuneStrokeActive = false;
    bCanConnectNextDrawPoint = false;
    bCanInterpolateNextRecognizedNode = false;
    RestoreHeldCollisionAfterMouseTrace();
    bPatternSolved = false;
    SolvedPatternId = NAME_None;
    DrawnUVPoints.Reset();
    RecognizedNodeSequence.Reset();
    ClearDrawTexture();
    ReceiveRuneCanvasDrawStateChanged(DrawnUVPoints, false);
    ReceiveRuneCanvasNodeSequenceChanged(RecognizedNodeSequence, INDEX_NONE);
}

void APickupActorAAARuneCanvasInstrument::CommitRuneSequenceAuthority(
    const TArray<int32> &NodeSequence,
    AActor *SolvingActor)
{
    if (!HasAuthority())
    {
        return;
    }

    bRuneDrawActive = false;
    bRuneStrokeActive = false;
    bCanConnectNextDrawPoint = false;
    bCanInterpolateNextRecognizedNode = false;
    RecognizedNodeSequence = NodeSequence;
    SolvedPatternId = NAME_None;
    bPatternSolved = TryResolveAcceptedPattern(RecognizedNodeSequence, SolvedPatternId);
    LogRecognizedNodeSequence(TEXT("CommitRuneSequenceAuthority"));
    LogCurrentCardAndUserSequences(TEXT("CommitRuneSequenceAuthority"), RecognizedNodeSequence);
    ReceiveRuneCanvasDrawStateChanged(DrawnUVPoints, false);
    ReceiveRuneCanvasNodeSequenceChanged(
        RecognizedNodeSequence,
        RecognizedNodeSequence.Num() > 0 ? RecognizedNodeSequence.Last() : INDEX_NONE);

    if (bPatternSolved)
    {
        ReceiveRuneCanvasPatternSolved(SolvedPatternId, SolvingActor);
    }
}

void APickupActorAAARuneCanvasInstrument::ClearDrawTexture()
{
    if (EnsureDrawResources() && DrawRenderTarget)
    {
        UKismetRenderingLibrary::ClearRenderTarget2D(this, DrawRenderTarget, DrawTextureClearColor);
        DrawRecognitionGridGuide();
    }
}

void APickupActorAAARuneCanvasInstrument::ReinitializeDrawResources()
{
    DrawRenderTarget = nullptr;
    CardDynamicMaterial = nullptr;
    EnsureDrawResources();
    ApplyCurrentCardResourceTexture();
    ClearDrawTexture();
}

void APickupActorAAARuneCanvasInstrument::CycleCardResource(int32 Direction)
{
    if (Direction == 0)
    {
        return;
    }

    if (bRuneDrawActive)
    {
        UE_LOG(LogTemp, Warning, TEXT("RuneCanvas: cannot switch card while drawing"));
        return;
    }

    if (LoadedCardResources.Num() == 0)
    {
        LoadCardResources();
    }

    if (LoadedCardResources.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("RuneCanvas: no card_resource_x resources found under %s"), *CardResourceScanPath);
        return;
    }

    const int32 ResourceCount = LoadedCardResources.Num();
    CurrentCardResourceIndex = (CurrentCardResourceIndex + Direction) % ResourceCount;
    if (CurrentCardResourceIndex < 0)
    {
        CurrentCardResourceIndex += ResourceCount;
    }

    ApplyCurrentCardResourceTexture();
    ClearDrawTexture();

    const TArray<int32> &ExpectedSequence = GetExpectedNodeSequenceForCurrentCard();
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("RuneCanvas: switched to card_resource_%d, expected=%s"),
        CurrentCardResourceIndex,
        *BuildNodeSequenceLogString(ExpectedSequence));
}

void APickupActorAAARuneCanvasInstrument::LoadCardResources()
{
    LoadedCardResources.Reset();

    IAssetRegistry &AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*CardResourceScanPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Assets;
    AssetRegistry.GetAssets(Filter, Assets);

    TArray<TPair<int32, TWeakObjectPtr<UObject>>> IndexedResources;
    for (const FAssetData &Asset : Assets)
    {
        int32 ResourceIndex = INDEX_NONE;
        if (!TryParseCardResourceIndex(Asset.AssetName.ToString(), ResourceIndex))
        {
            continue;
        }

        UObject *Resource = Asset.GetAsset();
        if (!Resource || (!Resource->IsA<UTexture2D>() && !Resource->IsA<UMaterialInterface>()))
        {
            continue;
        }

        IndexedResources.Emplace(ResourceIndex, Resource);
    }

    IndexedResources.Sort([](const TPair<int32, TWeakObjectPtr<UObject>> &A, const TPair<int32, TWeakObjectPtr<UObject>> &B)
                          { return A.Key < B.Key; });

    for (const TPair<int32, TWeakObjectPtr<UObject>> &Entry : IndexedResources)
    {
        if (UObject *Resource = Entry.Value.Get())
        {
            LoadedCardResources.Add(Resource);
        }
    }

    CurrentCardResourceIndex = FMath::Clamp(CurrentCardResourceIndex, 0, FMath::Max(0, LoadedCardResources.Num() - 1));

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("RuneCanvas: loaded %d card_resource_x resources from %s"),
        LoadedCardResources.Num(),
        *CardResourceScanPath);
}

void APickupActorAAARuneCanvasInstrument::ApplyCurrentCardResourceTexture()
{
    if (!EnsureDrawResources() || !CardDynamicMaterial || CardResourceTextureParameterName.IsNone())
    {
        return;
    }

    if (LoadedCardResources.Num() == 0)
    {
        return;
    }

    UObject *CardResource = LoadedCardResources.IsValidIndex(CurrentCardResourceIndex)
                                ? LoadedCardResources[CurrentCardResourceIndex].Get()
                                : nullptr;
    if (!CardResource)
    {
        return;
    }

    if (UMaterialInterface *CardMaterial = Cast<UMaterialInterface>(CardResource))
    {
        CardDynamicMaterial = MeshComponent->CreateDynamicMaterialInstance(
            FMath::Max(0, CardMaterialSlotIndex),
            CardMaterial);
        if (CardDynamicMaterial && DrawRenderTarget && !DrawTextureParameterName.IsNone())
        {
            CardDynamicMaterial->SetTextureParameterValue(DrawTextureParameterName, DrawRenderTarget);
        }
        UpdateActivationVisualState();
        return;
    }

    if (UTexture2D *CardTexture = Cast<UTexture2D>(CardResource))
    {
        CardDynamicMaterial->SetTextureParameterValue(CardResourceTextureParameterName, CardTexture);
    }

    UpdateActivationVisualState();
}

const TArray<int32> &APickupActorAAARuneCanvasInstrument::GetExpectedNodeSequenceForCurrentCard() const
{
    static const TArray<int32> EmptySequence;
    const TArray<TArray<int32>> &ExpectedSequences = GetHardcodedCardExpectedSequences();
    return ExpectedSequences.IsValidIndex(CurrentCardResourceIndex)
               ? ExpectedSequences[CurrentCardResourceIndex]
               : EmptySequence;
}

bool APickupActorAAARuneCanvasInstrument::CanParticipateInCanvasLinks() const
{
    return bAttachedToSurface && !IsHeldByPlayer() && !IsDisabledByRage();
}

float APickupActorAAARuneCanvasInstrument::GetConfiguredCanvasLinkDistance() const
{
    return LinkDistance;
}

FVector APickupActorAAARuneCanvasInstrument::GetCanvasLinkAnchorWorldLocation() const
{
    if (DrawSurfaceComponent)
    {
        return DrawSurfaceComponent->GetComponentTransform().TransformPosition(ChainLinkAnchorLocalOffset);
    }

    return GetActorLocation();
}

// --- CalculateNodeSequenceSimilarityPercent ---
float APickupActorAAARuneCanvasInstrument::CalculateNodeSequenceSimilarityPercent(
    const TArray<int32> &ExpectedNodeSequence,
    const TArray<int32> &UserNodeSequence) const
{
    if (ExpectedNodeSequence.Num() == 0 || UserNodeSequence.Num() == 0)
    {
        return 0.f;
    }

    const float UserToExpectedScore = CalculateBidirectionalCoverageScore(UserNodeSequence, ExpectedNodeSequence);
    const float ExpectedToUserScore = CalculateBidirectionalCoverageScore(ExpectedNodeSequence, UserNodeSequence);
    const float ShapeScore = (UserToExpectedScore + ExpectedToUserScore) * 0.5f;
    const float EditScore = CalculateWeightedEditSimilarityScore(ExpectedNodeSequence, UserNodeSequence);
    const float SafeShapeWeight = FMath::Clamp(ShapeSimilarityWeight, 0.f, 1.f);
    const float CombinedScore = ShapeScore * SafeShapeWeight + EditScore * (1.f - SafeShapeWeight);

    return FMath::Clamp(CombinedScore, 0.f, 1.f) * 100.f;
}

// --- CalculateBidirectionalCoverageScore ---
float APickupActorAAARuneCanvasInstrument::CalculateBidirectionalCoverageScore(
    const TArray<int32> &SourceNodeSequence,
    const TArray<int32> &TargetNodeSequence) const
{
    if (SourceNodeSequence.Num() == 0 || TargetNodeSequence.Num() == 0)
    {
        return 0.f;
    }

    const float SafeTolerance = FMath::Max(0.1f, SimilarityToleranceCells);
    float TotalScore = 0.f;

    for (const int32 SourceNodeId : SourceNodeSequence)
    {
        float BestDistance = TNumericLimits<float>::Max();
        for (const int32 TargetNodeId : TargetNodeSequence)
        {
            BestDistance = FMath::Min(BestDistance, CalculateNodeDistanceInCells(SourceNodeId, TargetNodeId));
        }

        TotalScore += 1.f - FMath::Clamp(BestDistance / SafeTolerance, 0.f, 1.f);
    }

    return TotalScore / static_cast<float>(SourceNodeSequence.Num());
}

// --- CalculateWeightedEditSimilarityScore ---
float APickupActorAAARuneCanvasInstrument::CalculateWeightedEditSimilarityScore(
    const TArray<int32> &ExpectedNodeSequence,
    const TArray<int32> &UserNodeSequence) const
{
    const int32 ExpectedCount = ExpectedNodeSequence.Num();
    const int32 UserCount = UserNodeSequence.Num();
    if (ExpectedCount == 0 || UserCount == 0)
    {
        return 0.f;
    }

    const float SafeTolerance = FMath::Max(0.1f, SimilarityToleranceCells);
    const float InsertCost = FMath::Max(0.f, ExtraUserNodeCost);
    const float DeleteCost = FMath::Max(0.f, MissingExpectedNodeCost);

    TArray<float> PreviousRow;
    TArray<float> CurrentRow;
    PreviousRow.SetNumZeroed(UserCount + 1);
    CurrentRow.SetNumZeroed(UserCount + 1);

    for (int32 UserIndex = 1; UserIndex <= UserCount; ++UserIndex)
    {
        PreviousRow[UserIndex] = PreviousRow[UserIndex - 1] + InsertCost;
    }

    for (int32 ExpectedIndex = 1; ExpectedIndex <= ExpectedCount; ++ExpectedIndex)
    {
        CurrentRow[0] = PreviousRow[0] + DeleteCost;

        for (int32 UserIndex = 1; UserIndex <= UserCount; ++UserIndex)
        {
            const float NodeDistance = CalculateNodeDistanceInCells(
                ExpectedNodeSequence[ExpectedIndex - 1],
                UserNodeSequence[UserIndex - 1]);
            const float ReplaceCost = FMath::Clamp(NodeDistance / SafeTolerance, 0.f, 1.f);
            const float DeletePathCost = PreviousRow[UserIndex] + DeleteCost;
            const float InsertPathCost = CurrentRow[UserIndex - 1] + InsertCost;
            const float ReplacePathCost = PreviousRow[UserIndex - 1] + ReplaceCost;

            CurrentRow[UserIndex] = FMath::Min3(DeletePathCost, InsertPathCost, ReplacePathCost);
        }

        Swap(PreviousRow, CurrentRow);
    }

    const float MaxReasonableCost = FMath::Max(
        static_cast<float>(ExpectedCount) * FMath::Max(DeleteCost, 1.f),
        static_cast<float>(UserCount) * FMath::Max(InsertCost, 1.f));
    if (MaxReasonableCost <= UE_KINDA_SMALL_NUMBER)
    {
        return 0.f;
    }

    return 1.f - FMath::Clamp(PreviousRow[UserCount] / MaxReasonableCost, 0.f, 1.f);
}

// --- CalculateNodeDistanceInCells ---
float APickupActorAAARuneCanvasInstrument::CalculateNodeDistanceInCells(int32 NodeA, int32 NodeB) const
{
    int32 RowA = INDEX_NONE;
    int32 ColumnA = INDEX_NONE;
    int32 RowB = INDEX_NONE;
    int32 ColumnB = INDEX_NONE;

    if (!GetHiddenNodeCoordinatesForId(NodeA, RowA, ColumnA) || !GetHiddenNodeCoordinatesForId(NodeB, RowB, ColumnB))
    {
        return SimilarityToleranceCells;
    }

    const float DeltaRow = static_cast<float>(RowA - RowB);
    const float DeltaColumn = static_cast<float>(ColumnA - ColumnB);
    return FMath::Sqrt(DeltaRow * DeltaRow + DeltaColumn * DeltaColumn);
}

void APickupActorAAARuneCanvasInstrument::LogCurrentCardAndUserSequences(
    const TCHAR *Context,
    const TArray<int32> &UserNodeSequence) const
{
    const TArray<int32> &ExpectedSequence = GetExpectedNodeSequenceForCurrentCard();
    const float SimilarityPercent = CalculateNodeSequenceSimilarityPercent(ExpectedSequence, UserNodeSequence);
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("RuneCanvas %s card_resource_%d expected=%s"),
        Context,
        CurrentCardResourceIndex,
        *BuildNodeSequenceLogString(ExpectedSequence));
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("RuneCanvas %s user=%s"),
        Context,
        *BuildNodeSequenceLogString(UserNodeSequence));
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("RuneCanvas %s similarity=%.1f%%"),
        Context,
        SimilarityPercent);
}

int32 APickupActorAAARuneCanvasInstrument::GetHiddenNodeId(int32 Row, int32 Column) const
{
    const int32 SafeRows = FMath::Max(1, HiddenNodeRows);
    const int32 SafeColumns = FMath::Max(1, HiddenNodeColumns);
    if (Row < 0 || Row >= SafeRows || Column < 0 || Column >= SafeColumns)
    {
        return INDEX_NONE;
    }

    return Row * SafeColumns + Column + 1;
}

void APickupActorAAARuneCanvasInstrument::EnableMouseTraceCollisionIfNeeded()
{
    if (!bUseMouseTraceCollisionUV || !MeshComponent)
    {
        return;
    }

    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    MeshComponent->SetGenerateOverlapEvents(false);
}

void APickupActorAAARuneCanvasInstrument::RestoreHeldCollisionAfterMouseTrace()
{
    if (!bUseMouseTraceCollisionUV || !bIsHeldByPlayer || !MeshComponent)
    {
        return;
    }

    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetGenerateOverlapEvents(false);
}

void APickupActorAAARuneCanvasInstrument::ApplyThrowablePhysicsTuning()
{
    ApplyPickupPhysicsTuning();

    if (GlowLightComponent)
    {
        GlowLightComponent->SetLightColor(ActivationGlowColor);
        GlowLightComponent->SetAttenuationRadius(ActivationLightRadius);
    }
}

void APickupActorAAARuneCanvasInstrument::RestoreDefaultThrowableCollision()
{
    if (!MeshComponent)
    {
        return;
    }

    ApplyReleasedCollisionProfile();
    MeshComponent->SetNotifyRigidBodyCollision(true);
    MeshComponent->BodyInstance.bUseCCD = true;
}

void APickupActorAAARuneCanvasInstrument::UpdateThrownAttachTrace()
{
    if (!bAwaitingThrowImpact || bAttachedToSurface || !MeshComponent || !MeshComponent->IsSimulatingPhysics())
    {
        LastAttachTraceLocation = GetActorLocation();
        return;
    }

    UWorld *World = GetWorld();
    if (!World)
    {
        return;
    }

    const FVector CurrentLocation = GetActorLocation();
    FVector TraceStart = LastAttachTraceLocation.IsNearlyZero() ? CurrentLocation : LastAttachTraceLocation;
    FVector TraceEnd = CurrentLocation;

    const FVector Velocity = MeshComponent->GetPhysicsLinearVelocity();
    if (!Velocity.IsNearlyZero())
    {
        TraceEnd += Velocity.GetSafeNormal() * AttachTraceForwardPadding;
    }

    if (FVector::DistSquared(TraceStart, TraceEnd) <= FMath::Square(1.f))
    {
        LastAttachTraceLocation = CurrentLocation;
        return;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RuneCanvasAttachTrace), false, this);
    QueryParams.AddIgnoredActor(this);
    if (AActor *OwnerActor = GetOwner())
    {
        QueryParams.AddIgnoredActor(OwnerActor);
    }
    if (AActor *InstigatorActor = GetInstigator())
    {
        QueryParams.AddIgnoredActor(InstigatorActor);
    }

    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

    FHitResult TraceHit;
    const bool bHit = World->SweepSingleByObjectType(
        TraceHit,
        TraceStart,
        TraceEnd,
        FQuat::Identity,
        ObjectQueryParams,
        FCollisionShape::MakeSphere(FMath::Max(1.f, AttachTraceRadius)),
        QueryParams);

    LastAttachTraceLocation = CurrentLocation;

    if (!bHit || !TraceHit.bBlockingHit)
    {
        return;
    }

    UPrimitiveComponent *HitComponent = TraceHit.GetComponent();
    if (!IsValid(HitComponent))
    {
        return;
    }

    StickToImpact(TraceHit, HitComponent);
    bAttachedToSurface = true;
    UpdateActivationVisualState();
    LinkRefreshAccumulator = LinkRefreshInterval;
    RefreshCanvasLinks();

    UE_LOG(LogTemp, Log, TEXT("%s rune canvas attached by trace to %s"), *GetName(), *GetNameSafe(TraceHit.GetActor()));
}

void APickupActorAAARuneCanvasInstrument::StickToImpact(const FHitResult &Hit, UPrimitiveComponent *HitComponent)
{
    bAwaitingThrowImpact = false;

    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    FVector ResolvedImpactNormal = Hit.ImpactNormal;
    if (ResolvedImpactNormal.IsNearlyZero())
    {
        ResolvedImpactNormal = FVector::UpVector;
    }

    const FRotator StickRotation = (-ResolvedImpactNormal).Rotation();

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

    const FVector StickLocation = Hit.ImpactPoint - MeshCenterOffset + ResolvedImpactNormal.GetSafeNormal() * AttachSurfacePadding;

    SetActorLocationAndRotation(StickLocation, StickRotation, false, nullptr, ETeleportType::TeleportPhysics);

    if (MeshComponent)
    {
        MeshComponent->SetSimulatePhysics(false);
        MeshComponent->SetEnableGravity(false);
        MeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
        MeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
        MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        MeshComponent->SetGenerateOverlapEvents(false);
    }

    if (IsValid(HitComponent))
    {
        AttachToComponent(HitComponent, FAttachmentTransformRules::KeepWorldTransform);
    }
}

void APickupActorAAARuneCanvasInstrument::UpdateActivationVisualState()
{
    const bool bShouldGlow = bAttachedToSurface;
    const UWorld *World = GetWorld();

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
        GlowLightComponent->SetHiddenInGame(!(bShouldGlow && bEnableActivationLight));
        GlowLightComponent->SetVisibility(bShouldGlow && bEnableActivationLight);
        GlowLightComponent->SetIntensity((bShouldGlow && bEnableActivationLight) ? ActivationLightIntensity : 0.f);
        GlowLightComponent->SetLightColor(ActivationGlowColor);
        GlowLightComponent->SetAttenuationRadius(ActivationLightRadius);
    }

    if (!MeshComponent)
    {
        return;
    }

    const float GlowScalar = bShouldGlow ? ActivationGlowIntensity : 0.f;
    const FLinearColor TintColor(
        ActivationGlowColor.R,
        ActivationGlowColor.G,
        ActivationGlowColor.B,
        1.f);

    const int32 MaterialSlotCount = MeshComponent->GetNumMaterials();
    for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
    {
        UMaterialInstanceDynamic *DynamicMaterial = nullptr;
        if (MaterialIndex == FMath::Max(0, CardMaterialSlotIndex) && IsValid(CardDynamicMaterial))
        {
            DynamicMaterial = CardDynamicMaterial;
        }
        else
        {
            DynamicMaterial = Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(MaterialIndex));
            if (!IsValid(DynamicMaterial))
            {
                UMaterialInterface *BaseMaterial = MeshComponent->GetMaterial(MaterialIndex);
                if (!BaseMaterial)
                {
                    continue;
                }

                DynamicMaterial = MeshComponent->CreateDynamicMaterialInstance(MaterialIndex, BaseMaterial);
            }
        }

        if (!IsValid(DynamicMaterial))
        {
            continue;
        }

        if (bShouldGlow)
        {
            DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), ActivationGlowColor);
            DynamicMaterial->SetVectorParameterValue(TEXT("GlowColor"), ActivationGlowColor);
            DynamicMaterial->SetVectorParameterValue(TEXT("HighlightColor"), ActivationGlowColor);
            DynamicMaterial->SetVectorParameterValue(TEXT("TintColor"), TintColor);
            DynamicMaterial->SetVectorParameterValue(TEXT("ColorAndOpacity"), TintColor);
            DynamicMaterial->SetVectorParameterValue(TEXT("TintColorAndOpacity"), TintColor);
        }

        DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveIntensity"), GlowScalar);
        DynamicMaterial->SetScalarParameterValue(TEXT("GlowIntensity"), GlowScalar);
        DynamicMaterial->SetScalarParameterValue(TEXT("HighlightIntensity"), GlowScalar);
        DynamicMaterial->SetScalarParameterValue(TEXT("GlowEnabled"), bShouldGlow ? 1.f : 0.f);
    }
}

void APickupActorAAARuneCanvasInstrument::RefreshCanvasLinks()
{
    UWorld* World = GetWorld();
    if (!World || !CanParticipateInCanvasLinks())
    {
        ClearChainLinks();
        return;
    }

    TArray<APickupActorAAARuneCanvasInstrument*> AllCanvases;
    GatherAttachedRuneCanvases(World, AllCanvases);

    TArray<FRuneCanvasLinkEdge> LinkEdges;
    const FString ThisPathName = GetPathName();
    const FVector ThisAnchor = GetCanvasLinkAnchorLocation(this);

    for (APickupActorAAARuneCanvasInstrument* Candidate : AllCanvases)
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

FString APickupActorAAARuneCanvasInstrument::BuildChainLinkBuildSignature(const TArray<FRuneCanvasLinkEdge>& LinkEdges) const
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

    for (const FRuneCanvasLinkEdge& LinkEdge : LinkEdges)
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

void APickupActorAAARuneCanvasInstrument::RebuildChainLinks(const TArray<FRuneCanvasLinkEdge>& LinkEdges)
{
    ClearChainLinks(true);
    ActiveChainLinkBuildSignature = BuildChainLinkBuildSignature(LinkEdges);

    if (!bRenderLinkChains || !CanvasLinkRootComponent || !ChainLinkMesh || LinkEdges.IsEmpty())
    {
        return;
    }

    const float SafeSpacing = FMath::Max(1.f, ChainLinkSpacing);

    for (const FRuneCanvasLinkEdge& LinkEdge : LinkEdges)
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
            UStaticMeshComponent* ChainLinkComponent = NewObject<UStaticMeshComponent>(this);
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
            // ChainLinkComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            // ChainLinkComponent->SetGenerateOverlapEvents(false);
            if (ChainLinkTouchedMaterialOverride)
            {
                ChainLinkComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                ChainLinkComponent->SetCollisionObjectType(ECC_WorldDynamic);
                ChainLinkComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
                ChainLinkComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
                ChainLinkComponent->SetGenerateOverlapEvents(true);
                ChainLinkComponent->OnComponentBeginOverlap.AddDynamic(this, &APickupActorAAARuneCanvasInstrument::HandleChainLinkBeginOverlap);
                ChainLinkComponent->OnComponentEndOverlap.AddDynamic(this, &APickupActorAAARuneCanvasInstrument::HandleChainLinkEndOverlap);
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

            // const int32 MaterialSlotCount = FMath::Max(ChainLinkMesh->GetStaticMaterials().Num(), 1);
            // for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
            // {
            //     UMaterialInterface* BaseMaterial = ChainLinkMaterialOverride
            //         ? ChainLinkMaterialOverride.Get()
            //         : ChainLinkComponent->GetMaterial(MaterialIndex);
            ApplyChainLinkDefaultMaterial(ChainLinkComponent);

            ActiveChainLinkComponents.Add(ChainLinkComponent);
            ChainLinkEdgeKeys.Add(ChainLinkComponent, LinkEdge.EdgeKey);
            ActiveChainLinkComponentsByEdge.FindOrAdd(LinkEdge.EdgeKey).Add(ChainLinkComponent);
        }
    }
}

void APickupActorAAARuneCanvasInstrument::ClearChainLinks(bool bPreserveBreakingEdges)
{
    TArray<TObjectPtr<UStaticMeshComponent>> PreservedChainLinkComponents;
    TArray<TObjectPtr<UMaterialInstanceDynamic>> PreservedMaterialInstances;
    TMap<TObjectPtr<UStaticMeshComponent>, TObjectPtr<UMaterialInstanceDynamic>> PreservedDefaultMaterialInstances;
    TMap<TObjectPtr<UStaticMeshComponent>, TObjectPtr<UMaterialInstanceDynamic>> PreservedTouchedMaterialInstances;
    TMap<TObjectPtr<UStaticMeshComponent>, FString> PreservedEdgeKeys;
    TMap<FString, TArray<TObjectPtr<UStaticMeshComponent>>> PreservedComponentsByEdge;

    for (UStaticMeshComponent* ChainLinkComponent : ActiveChainLinkComponents)
    {
        const FString* EdgeKey = ChainLinkEdgeKeys.Find(ChainLinkComponent);
        if (bPreserveBreakingEdges && EdgeKey && BreakingChainLinkEdges.Contains(*EdgeKey))
        {
            PreservedChainLinkComponents.Add(ChainLinkComponent);
            PreservedEdgeKeys.Add(ChainLinkComponent, *EdgeKey);
            PreservedComponentsByEdge.FindOrAdd(*EdgeKey).Add(ChainLinkComponent);

            if (TObjectPtr<UMaterialInstanceDynamic>* DefaultMaterial = ChainLinkDefaultMaterialInstances.Find(ChainLinkComponent))
            {
                PreservedDefaultMaterialInstances.Add(ChainLinkComponent, *DefaultMaterial);
                PreservedMaterialInstances.AddUnique(*DefaultMaterial);
            }

            if (TObjectPtr<UMaterialInstanceDynamic>* TouchedMaterial = ChainLinkTouchedMaterialInstances.Find(ChainLinkComponent))
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

void APickupActorAAARuneCanvasInstrument::DestroyBrokenChainLinks()
{
    for (UStaticMeshComponent* ChainLinkComponent : BrokenChainLinkComponents)
    {
        if (IsValid(ChainLinkComponent))
        {
            ChainLinkComponent->DestroyComponent();
        }
    }

    BrokenChainLinkComponents.Reset();
}

void APickupActorAAARuneCanvasInstrument::ApplyChainLinkDefaultMaterial(UStaticMeshComponent* ChainLinkComponent)
{
    if (!IsValid(ChainLinkComponent))
    {
        return;
    }

    UMaterialInstanceDynamic* DynamicMaterial = nullptr;
    if (TObjectPtr<UMaterialInstanceDynamic>* ExistingMaterial = ChainLinkDefaultMaterialInstances.Find(ChainLinkComponent))
    {
        DynamicMaterial = ExistingMaterial->Get();
    }

    if (!IsValid(DynamicMaterial))
    {
        UMaterialInterface* BaseMaterial = ChainLinkMaterialOverride
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

void APickupActorAAARuneCanvasInstrument::ApplyChainLinkTouchedMaterial(UStaticMeshComponent* ChainLinkComponent)
{
    if (!IsValid(ChainLinkComponent) || !ChainLinkTouchedMaterialOverride)
    {
        return;
    }

    UMaterialInstanceDynamic* DynamicMaterial = nullptr;
    if (TObjectPtr<UMaterialInstanceDynamic>* ExistingMaterial = ChainLinkTouchedMaterialInstances.Find(ChainLinkComponent))
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

void APickupActorAAARuneCanvasInstrument::HandleChainLinkBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    (void)OtherComp;
    (void)OtherBodyIndex;
    (void)bFromSweep;
    (void)SweepResult;

    if (!Cast<APawn>(OtherActor))
    {
        return;
    }

    UStaticMeshComponent* ChainLinkComponent = Cast<UStaticMeshComponent>(OverlappedComponent);
    if (!IsValid(ChainLinkComponent))
    {
        return;
    }

    if (FString* EdgeKey = ChainLinkEdgeKeys.Find(ChainLinkComponent))
    {
        MarkChainLinkEdgeTouched(*EdgeKey);
        return;
    }

    ApplyChainLinkTouchedMaterial(ChainLinkComponent);
}

void APickupActorAAARuneCanvasInstrument::HandleChainLinkEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    (void)OtherComp;
    (void)OtherBodyIndex;

    if (!Cast<APawn>(OtherActor))
    {
        return;
    }

    UStaticMeshComponent* ChainLinkComponent = Cast<UStaticMeshComponent>(OverlappedComponent);
    if (!IsValid(ChainLinkComponent))
    {
        return;
    }

    TArray<AActor*> OverlappingPawns;
    ChainLinkComponent->GetOverlappingActors(OverlappingPawns, APawn::StaticClass());
    const FString* EdgeKey = ChainLinkEdgeKeys.Find(ChainLinkComponent);
    if (OverlappingPawns.IsEmpty() && (!EdgeKey || !BreakingChainLinkEdges.Contains(*EdgeKey)))
    {
        ApplyChainLinkDefaultMaterial(ChainLinkComponent);
    }
}

void APickupActorAAARuneCanvasInstrument::MarkChainLinkEdgeTouched(const FString& EdgeKey)
{
    if (EdgeKey.IsEmpty() || GetBrokenCanvasLinkEdgeKeysForWorld(GetWorld()).Contains(EdgeKey))
    {
        return;
    }

    if (TArray<TObjectPtr<UStaticMeshComponent>>* ChainLinkComponents = ActiveChainLinkComponentsByEdge.Find(EdgeKey))
    {
        for (UStaticMeshComponent* ChainLinkComponent : *ChainLinkComponents)
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
    BreakDelegate.BindUObject(this, &APickupActorAAARuneCanvasInstrument::BreakChainLinkEdge, EdgeKey);
    GetWorldTimerManager().SetTimer(
        ChainLinkBreakTimerHandles.FindOrAdd(EdgeKey),
        BreakDelegate,
        FMath::Max(0.f, ChainLinkBreakDelay),
        false);
}

void APickupActorAAARuneCanvasInstrument::BreakChainLinkEdge(FString EdgeKey)
{
    if (EdgeKey.IsEmpty())
    {
        return;
    }

    GetBrokenCanvasLinkEdgeKeysForWorld(GetWorld()).Add(EdgeKey);
    BreakingChainLinkEdges.Remove(EdgeKey);
    ChainLinkBreakTimerHandles.Remove(EdgeKey);

    TArray<TObjectPtr<UStaticMeshComponent>> ChainLinkComponents;
    if (TArray<TObjectPtr<UStaticMeshComponent>>* ExistingComponents = ActiveChainLinkComponentsByEdge.Find(EdgeKey))
    {
        ChainLinkComponents = *ExistingComponents;
    }

    ActiveChainLinkComponentsByEdge.Remove(EdgeKey);

    UWorld* World = GetWorld();
    for (UStaticMeshComponent* ChainLinkComponent : ChainLinkComponents)
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

        const FVector BreakImpulseDirection = (ChainLinkComponent->GetComponentLocation() - GetCanvasLinkAnchorWorldLocation()).GetSafeNormal();
        const FVector BreakImpulse = (BreakImpulseDirection.IsNearlyZero() ? FVector::UpVector : BreakImpulseDirection + FVector(0.f, 0.f, 0.35f)).GetSafeNormal() * BrokenChainLinkImpulse;

        if (bBrokenChainLinksSimulatePhysics)
        {
            ChainLinkComponent->WakeAllRigidBodies();
            ChainLinkComponent->AddImpulse(BreakImpulse, NAME_None, true);
        }
        else
        {
            ChainLinkComponent->AddWorldOffset(BreakImpulse * 0.03f, false, nullptr, ETeleportType::TeleportPhysics);
        }
        BrokenChainLinkComponents.Add(ChainLinkComponent);

        if (World && BrokenChainLinkLifetime > 0.f)
        {
            FTimerDelegate DestroyDelegate;
            DestroyDelegate.BindUObject(this, &APickupActorAAARuneCanvasInstrument::FinishBrokenChainLinkComponent, ChainLinkComponent);
            FTimerHandle DestroyTimerHandle;
            World->GetTimerManager().SetTimer(DestroyTimerHandle, DestroyDelegate, BrokenChainLinkLifetime, false);
        }
    }

    DestroyIfNoActiveCanvasLinks(EdgeKey);
}

void APickupActorAAARuneCanvasInstrument::FinishBrokenChainLinkComponent(UStaticMeshComponent* ChainLinkComponent)
{
    BrokenChainLinkComponents.Remove(ChainLinkComponent);
    if (IsValid(ChainLinkComponent))
    {
        ChainLinkComponent->DestroyComponent();
    }
}

void APickupActorAAARuneCanvasInstrument::DestroyIfNoActiveCanvasLinks(const FString& BrokenEdgeKey)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    TArray<APickupActorAAARuneCanvasInstrument*> AllCanvases;
    GatherAttachedRuneCanvases(World, AllCanvases);

    TArray<APickupActorAAARuneCanvasInstrument*> CanvasesToDestroy;
    for (APickupActorAAARuneCanvasInstrument* Canvas : AllCanvases)
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

    for (APickupActorAAARuneCanvasInstrument* Canvas : CanvasesToDestroy)
    {
        if (IsValid(Canvas))
        {
            Canvas->Destroy();
        }
    }
}

void APickupActorAAARuneCanvasInstrument::UpdateChainLinkPulseVisuals(float DeltaTime)
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

    for (UMaterialInstanceDynamic* DynamicMaterial : ActiveChainLinkMaterialInstances)
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

bool APickupActorAAARuneCanvasInstrument::GetPreferredDrawStartScreenPosition(
    APlayerController *PC,
    FVector2D &OutScreenPosition) const
{
    OutScreenPosition = FVector2D::ZeroVector;
    return PC && DrawSurfaceComponent && PC->ProjectWorldLocationToScreen(DrawSurfaceComponent->GetComponentLocation(), OutScreenPosition, true);
}

bool APickupActorAAARuneCanvasInstrument::EnsureDrawResources()
{
    if (!DrawRenderTarget)
    {
        const int32 SafeResolution = FMath::Clamp(DrawTextureResolution, 64, 4096);
        int32 TargetWidth = SafeResolution;
        int32 TargetHeight = SafeResolution;

        if (bMatchDrawTextureAspectToSurface)
        {
            const float SafeSurfaceWidth = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.X);
            const float SafeSurfaceHeight = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.Y);

            if (SafeSurfaceWidth >= SafeSurfaceHeight)
            {
                TargetHeight = FMath::Clamp(
                    FMath::RoundToInt(static_cast<float>(SafeResolution) * (SafeSurfaceHeight / SafeSurfaceWidth)),
                    64,
                    4096);
            }
            else
            {
                TargetWidth = FMath::Clamp(
                    FMath::RoundToInt(static_cast<float>(SafeResolution) * (SafeSurfaceWidth / SafeSurfaceHeight)),
                    64,
                    4096);
            }
        }

        DrawRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("RuneCanvasDrawRenderTarget"));
        if (!DrawRenderTarget)
        {
            return false;
        }

        DrawRenderTarget->ClearColor = DrawTextureClearColor;
        DrawRenderTarget->InitAutoFormat(TargetWidth, TargetHeight);
        DrawRenderTarget->UpdateResourceImmediate(true);
        UKismetRenderingLibrary::ClearRenderTarget2D(this, DrawRenderTarget, DrawTextureClearColor);
        DrawRecognitionGridGuide();
    }

    if (!CardDynamicMaterial && MeshComponent)
    {
        CardDynamicMaterial = MeshComponent->CreateDynamicMaterialInstance(
            FMath::Max(0, CardMaterialSlotIndex),
            CardBaseMaterial.Get());
    }

    if (CardDynamicMaterial && DrawRenderTarget && !DrawTextureParameterName.IsNone())
    {
        CardDynamicMaterial->SetTextureParameterValue(DrawTextureParameterName, DrawRenderTarget);
    }

    if (CardDynamicMaterial && LoadedCardResources.Num() > 0 && !CardResourceTextureParameterName.IsNone())
    {
        UTexture2D *CardTexture = LoadedCardResources.IsValidIndex(CurrentCardResourceIndex)
                                      ? Cast<UTexture2D>(LoadedCardResources[CurrentCardResourceIndex].Get())
                                      : nullptr;
        if (CardTexture)
        {
            CardDynamicMaterial->SetTextureParameterValue(CardResourceTextureParameterName, CardTexture);
        }
    }

    return DrawRenderTarget != nullptr;
}

bool APickupActorAAARuneCanvasInstrument::IsUVInsideRecognitionArea(const FVector2D &UV) const
{
    const FVector2D SafeAreaSize(
        FMath::Clamp(RecognitionAreaSizeUV.X, 0.01f, 1.f),
        FMath::Clamp(RecognitionAreaSizeUV.Y, 0.01f, 1.f));
    const FVector2D AreaMin = RecognitionAreaCenterUV - SafeAreaSize * 0.5f;
    const FVector2D AreaMax = RecognitionAreaCenterUV + SafeAreaSize * 0.5f;

    return UV.X >= AreaMin.X && UV.X <= AreaMax.X && UV.Y >= AreaMin.Y && UV.Y <= AreaMax.Y;
}

FVector2D APickupActorAAARuneCanvasInstrument::NormalizeUVToRecognitionArea(const FVector2D &UV) const
{
    const FVector2D SafeAreaSize(
        FMath::Clamp(RecognitionAreaSizeUV.X, 0.01f, 1.f),
        FMath::Clamp(RecognitionAreaSizeUV.Y, 0.01f, 1.f));
    const FVector2D AreaMin = RecognitionAreaCenterUV - SafeAreaSize * 0.5f;

    return FVector2D(
        (UV.X - AreaMin.X) / SafeAreaSize.X,
        (UV.Y - AreaMin.Y) / SafeAreaSize.Y);
}

FVector2D APickupActorAAARuneCanvasInstrument::DenormalizeRecognitionAreaUV(const FVector2D &AreaUV) const
{
    const FVector2D SafeAreaSize(
        FMath::Clamp(RecognitionAreaSizeUV.X, 0.01f, 1.f),
        FMath::Clamp(RecognitionAreaSizeUV.Y, 0.01f, 1.f));
    const FVector2D AreaMin = RecognitionAreaCenterUV - SafeAreaSize * 0.5f;

    return AreaMin + FVector2D(AreaUV.X * SafeAreaSize.X, AreaUV.Y * SafeAreaSize.Y);
}

FVector APickupActorAAARuneCanvasInstrument::GetDrawSurfaceLocalLocationFromUV(const FVector2D &UV) const
{
    const float SafeWidth = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.X);
    const float SafeHeight = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.Y);
    const FVector2D SafeSensitivity(
        FMath::Max(UE_KINDA_SMALL_NUMBER, DrawUVSensitivity.X),
        FMath::Max(UE_KINDA_SMALL_NUMBER, DrawUVSensitivity.Y));

    const float MappedU = ((UV.X - 0.5f) / SafeSensitivity.X) + 0.5f;
    const float MappedV = ((UV.Y - 0.5f) / SafeSensitivity.Y) + 0.5f;
    const float RawU = bInvertDrawU ? 1.f - MappedU : MappedU;
    const float RawV = bInvertDrawV ? 1.f - MappedV : MappedV;
    const float SurfaceHorizontal = (RawU - 0.5f) * SafeWidth;
    const float SurfaceVertical = (RawV - 0.5f) * SafeHeight;

    return bSwapDrawSurfaceAxes
               ? FVector(SurfaceHorizontal, SurfaceVertical, 0.1f)
               : FVector(SurfaceVertical, SurfaceHorizontal, 0.1f);
}

void APickupActorAAARuneCanvasInstrument::RebuildRecognitionGridPreview()
{
    ClearRecognitionGridPreview();

    const bool bShouldShowNodePreview = bShowRecognitionGridPreview || bDrawRecognitionGridGuide;
    const bool bShouldShowSurfacePreview = bShowDrawSurfacePreview || bShouldShowNodePreview;

    if ((!bShouldShowNodePreview && !bShouldShowSurfacePreview) || !DrawSurfaceComponent)
    {
        return;
    }

    UStaticMesh *PreviewMesh = RecognitionGridPreviewNodeMesh
                                   ? RecognitionGridPreviewNodeMesh.Get()
                                   : DefaultRecognitionGridPreviewNodeMesh.Get();
    if (!PreviewMesh)
    {
        return;
    }

    if (bShouldShowSurfacePreview)
    {
        const FVector SurfaceMin = GetDrawSurfaceLocalLocationFromUV(FVector2D::ZeroVector);
        const FVector SurfaceMax = GetDrawSurfaceLocalLocationFromUV(FVector2D(1.f, 1.f));
        const float MinX = FMath::Min(SurfaceMin.X, SurfaceMax.X);
        const float MaxX = FMath::Max(SurfaceMin.X, SurfaceMax.X);
        const float MinY = FMath::Min(SurfaceMin.Y, SurfaceMax.Y);
        const float MaxY = FMath::Max(SurfaceMin.Y, SurfaceMax.Y);
        const float CenterX = (MinX + MaxX) * 0.5f;
        const float CenterY = (MinY + MaxY) * 0.5f;
        const float SizeX = FMath::Max(UE_KINDA_SMALL_NUMBER, MaxX - MinX);
        const float SizeY = FMath::Max(UE_KINDA_SMALL_NUMBER, MaxY - MinY);
        const float BorderThickness = FMath::Max(0.01f, DrawSurfacePreviewBorderThickness);

        const TArray<TPair<FVector, FVector>> BorderTransforms = {
            TPair<FVector, FVector>(FVector(MinX, CenterY, 0.12f), FVector(BorderThickness / 100.f, SizeY / 100.f, 1.f)),
            TPair<FVector, FVector>(FVector(MaxX, CenterY, 0.12f), FVector(BorderThickness / 100.f, SizeY / 100.f, 1.f)),
            TPair<FVector, FVector>(FVector(CenterX, MinY, 0.12f), FVector(SizeX / 100.f, BorderThickness / 100.f, 1.f)),
            TPair<FVector, FVector>(FVector(CenterX, MaxY, 0.12f), FVector(SizeX / 100.f, BorderThickness / 100.f, 1.f))};

        for (int32 BorderIndex = 0; BorderIndex < BorderTransforms.Num(); ++BorderIndex)
        {
            const FName ComponentName = *FString::Printf(TEXT("rune_canvas_preview_surface_border_%d"), BorderIndex);
            UStaticMeshComponent *BorderComponent = NewObject<UStaticMeshComponent>(this, ComponentName);
            if (!BorderComponent)
            {
                continue;
            }

            BorderComponent->ComponentTags.Add(FName("GeneratedRuneCanvasRecognitionPreview"));
            BorderComponent->SetupAttachment(DrawSurfaceComponent);
            BorderComponent->SetMobility(EComponentMobility::Movable);
            BorderComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            BorderComponent->SetGenerateOverlapEvents(false);
            BorderComponent->SetCanEverAffectNavigation(false);
            BorderComponent->SetCastShadow(false);
            BorderComponent->SetHiddenInGame(false);
            BorderComponent->SetStaticMesh(PreviewMesh);
            BorderComponent->SetRelativeLocation(BorderTransforms[BorderIndex].Key);
            BorderComponent->SetRelativeRotation(FRotator::ZeroRotator);
            BorderComponent->SetRelativeScale3D(BorderTransforms[BorderIndex].Value);
            BorderComponent->RegisterComponent();

            UMaterialInterface *BorderMaterial = DrawSurfacePreviewMaterial
                                                     ? DrawSurfacePreviewMaterial.Get()
                                                     : RecognitionGridPreviewMaterial.Get();
            if (BorderMaterial)
            {
                BorderComponent->SetMaterial(0, BorderMaterial);
            }

            RecognitionGridPreviewComponents.Add(BorderComponent);
        }
    }

    if (!bShouldShowNodePreview)
    {
        return;
    }

    const int32 SafeRows = FMath::Max(1, HiddenNodeRows);
    const int32 SafeColumns = FMath::Max(1, HiddenNodeColumns);
    const float SafePadding = FMath::Clamp(HiddenNodeEdgePaddingUV, 0.f, 0.45f);
    const float Span = FMath::Max(UE_KINDA_SMALL_NUMBER, 1.f - SafePadding * 2.f);

    for (int32 RowIndex = 0; RowIndex < SafeRows; ++RowIndex)
    {
        for (int32 ColumnIndex = 0; ColumnIndex < SafeColumns; ++ColumnIndex)
        {
            const int32 NodeId = GetHiddenNodeId(RowIndex, ColumnIndex);
            const FName ComponentName = *FString::Printf(TEXT("rune_canvas_preview_node_%d"), NodeId);
            UStaticMeshComponent *PreviewComponent = NewObject<UStaticMeshComponent>(this, ComponentName);
            if (!PreviewComponent)
            {
                continue;
            }

            const FVector2D AreaNodeUV(
                SafeColumns == 1 ? 0.5f : SafePadding + (static_cast<float>(ColumnIndex) / static_cast<float>(SafeColumns - 1)) * Span,
                SafeRows == 1 ? 0.5f : SafePadding + (static_cast<float>(RowIndex) / static_cast<float>(SafeRows - 1)) * Span);
            const FVector2D NodeUV = DenormalizeRecognitionAreaUV(AreaNodeUV);

            PreviewComponent->ComponentTags.Add(FName("GeneratedRuneCanvasRecognitionPreview"));
            PreviewComponent->SetupAttachment(DrawSurfaceComponent);
            PreviewComponent->SetMobility(EComponentMobility::Movable);
            PreviewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            PreviewComponent->SetGenerateOverlapEvents(false);
            PreviewComponent->SetCanEverAffectNavigation(false);
            PreviewComponent->SetCastShadow(false);
            PreviewComponent->SetHiddenInGame(false);
            PreviewComponent->SetStaticMesh(PreviewMesh);
            PreviewComponent->SetRelativeLocation(GetDrawSurfaceLocalLocationFromUV(NodeUV));
            PreviewComponent->SetRelativeRotation(FRotator::ZeroRotator);
            PreviewComponent->SetRelativeScale3D(RecognitionGridPreviewNodeScale);
            PreviewComponent->RegisterComponent();

            if (RecognitionGridPreviewMaterial)
            {
                PreviewComponent->SetMaterial(0, RecognitionGridPreviewMaterial);
            }

            RecognitionGridPreviewComponents.Add(PreviewComponent);
        }
    }
}

void APickupActorAAARuneCanvasInstrument::ClearRecognitionGridPreview()
{
    TArray<UStaticMeshComponent *> ComponentsToDestroy;
    for (UStaticMeshComponent *PreviewComponent : RecognitionGridPreviewComponents)
    {
        if (IsValid(PreviewComponent))
        {
            ComponentsToDestroy.Add(PreviewComponent);
        }
    }

    if (ComponentsToDestroy.Num() == 0)
    {
        TArray<UStaticMeshComponent *> AttachedStaticMeshes;
        GetComponents<UStaticMeshComponent>(AttachedStaticMeshes);
        for (UStaticMeshComponent *StaticMeshComponent : AttachedStaticMeshes)
        {
            if (IsValid(StaticMeshComponent) && StaticMeshComponent->ComponentHasTag(FName("GeneratedRuneCanvasRecognitionPreview")))
            {
                ComponentsToDestroy.Add(StaticMeshComponent);
            }
        }
    }

    for (UStaticMeshComponent *PreviewComponent : ComponentsToDestroy)
    {
        if (IsValid(PreviewComponent) && PreviewComponent != MeshComponent)
        {
            PreviewComponent->DestroyComponent();
        }
    }

    RecognitionGridPreviewComponents.Reset();
}

void APickupActorAAARuneCanvasInstrument::DrawRecognitionGridGuide()
{
    if (!bDrawRecognitionGridGuide || !DrawRenderTarget)
    {
        return;
    }

    UCanvas *Canvas = nullptr;
    FVector2D CanvasSize = FVector2D::ZeroVector;
    FDrawToRenderTargetContext DrawContext;
    UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, DrawRenderTarget, Canvas, CanvasSize, DrawContext);

    if (Canvas)
    {
        const FVector2D AreaMin = DenormalizeRecognitionAreaUV(FVector2D::ZeroVector);
        const FVector2D AreaMax = DenormalizeRecognitionAreaUV(FVector2D(1.f, 1.f));
        const FVector2D TopLeft(AreaMin.X * CanvasSize.X, AreaMin.Y * CanvasSize.Y);
        const FVector2D TopRight(AreaMax.X * CanvasSize.X, AreaMin.Y * CanvasSize.Y);
        const FVector2D BottomLeft(AreaMin.X * CanvasSize.X, AreaMax.Y * CanvasSize.Y);
        const FVector2D BottomRight(AreaMax.X * CanvasSize.X, AreaMax.Y * CanvasSize.Y);

        Canvas->K2_DrawLine(TopLeft, TopRight, RecognitionGridGuideThicknessPixels, RecognitionGridGuideColor);
        Canvas->K2_DrawLine(TopRight, BottomRight, RecognitionGridGuideThicknessPixels, RecognitionGridGuideColor);
        Canvas->K2_DrawLine(BottomRight, BottomLeft, RecognitionGridGuideThicknessPixels, RecognitionGridGuideColor);
        Canvas->K2_DrawLine(BottomLeft, TopLeft, RecognitionGridGuideThicknessPixels, RecognitionGridGuideColor);

        const int32 SafeRows = FMath::Max(1, HiddenNodeRows);
        const int32 SafeColumns = FMath::Max(1, HiddenNodeColumns);
        const float SafePadding = FMath::Clamp(HiddenNodeEdgePaddingUV, 0.f, 0.45f);
        const float Span = FMath::Max(UE_KINDA_SMALL_NUMBER, 1.f - SafePadding * 2.f);
        const float CrossHalfSizePixels = FMath::Max(4.f, RecognitionGridGuideThicknessPixels * 3.f);

        for (int32 RowIndex = 0; RowIndex < SafeRows; ++RowIndex)
        {
            for (int32 ColumnIndex = 0; ColumnIndex < SafeColumns; ++ColumnIndex)
            {
                const FVector2D AreaNodeUV(
                    SafeColumns == 1 ? 0.5f : SafePadding + (static_cast<float>(ColumnIndex) / static_cast<float>(SafeColumns - 1)) * Span,
                    SafeRows == 1 ? 0.5f : SafePadding + (static_cast<float>(RowIndex) / static_cast<float>(SafeRows - 1)) * Span);
                const FVector2D NodeUV = DenormalizeRecognitionAreaUV(AreaNodeUV);
                const FVector2D NodePixel(NodeUV.X * CanvasSize.X, NodeUV.Y * CanvasSize.Y);

                Canvas->K2_DrawLine(
                    NodePixel - FVector2D(CrossHalfSizePixels, 0.f),
                    NodePixel + FVector2D(CrossHalfSizePixels, 0.f),
                    RecognitionGridGuideThicknessPixels,
                    RecognitionGridGuideColor);
                Canvas->K2_DrawLine(
                    NodePixel - FVector2D(0.f, CrossHalfSizePixels),
                    NodePixel + FVector2D(0.f, CrossHalfSizePixels),
                    RecognitionGridGuideThicknessPixels,
                    RecognitionGridGuideColor);
            }
        }
    }

    UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, DrawContext);
}

bool APickupActorAAARuneCanvasInstrument::ResolveDrawUVFromScreenPosition(
    APlayerController *UsingController,
    const FVector2D &ScreenPosition,
    FVector2D &OutUV) const
{
    OutUV = FVector2D::ZeroVector;
    if (!UsingController || !DrawSurfaceComponent)
    {
        return false;
    }

    if (bUseMouseTraceCollisionUV && ResolveDrawUVFromMouseTrace(UsingController, ScreenPosition, OutUV))
    {
        return true;
    }

    FVector RayOrigin = FVector::ZeroVector;
    FVector RayDirection = FVector::ZeroVector;
    if (!UsingController->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, RayOrigin, RayDirection))
    {
        return false;
    }

    const FTransform SurfaceTransform = DrawSurfaceComponent->GetComponentTransform();
    const FVector PlaneOrigin = SurfaceTransform.GetLocation();
    const FVector PlaneNormal = SurfaceTransform.GetUnitAxis(EAxis::Z);
    const float Denominator = FVector::DotProduct(RayDirection, PlaneNormal);

    if (FMath::IsNearlyZero(Denominator))
    {
        return false;
    }

    const float DistanceAlongRay = FVector::DotProduct(PlaneOrigin - RayOrigin, PlaneNormal) / Denominator;
    if (DistanceAlongRay < 0.f)
    {
        return false;
    }

    return ResolveDrawUVFromWorldLocation(RayOrigin + RayDirection * DistanceAlongRay, OutUV);
}
bool APickupActorAAARuneCanvasInstrument::ResolveDrawUVFromWorldLocation(
    const FVector &WorldLocation,
    FVector2D &OutUV) const
{
    OutUV = FVector2D::ZeroVector;
    if (!DrawSurfaceComponent)
    {
        return false;
    }

    const FTransform SurfaceTransform = DrawSurfaceComponent->GetComponentTransform();
    const FVector LocalHitLocation = SurfaceTransform.InverseTransformPosition(WorldLocation);

    const float SafeWidth = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.X);
    const float SafeHeight = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.Y);

    const float SurfaceHorizontal = bSwapDrawSurfaceAxes ? LocalHitLocation.X : LocalHitLocation.Y;
    const float SurfaceVertical = bSwapDrawSurfaceAxes ? LocalHitLocation.Y : LocalHitLocation.X;
    const float RawU = (SurfaceHorizontal / SafeWidth) + 0.5f;
    const float RawV = (SurfaceVertical / SafeHeight) + 0.5f;
    const float MappedU = bInvertDrawU ? 1.f - RawU : RawU;
    const float MappedV = bInvertDrawV ? 1.f - RawV : RawV;
    const float U = 0.5f + (MappedU - 0.5f) * DrawUVSensitivity.X;
    const float V = 0.5f + (MappedV - 0.5f) * DrawUVSensitivity.Y;

    if (U < 0.f || U > 1.f || V < 0.f || V > 1.f)
    {
        return false;
    }

    if (bRestrictDrawingToRecognitionArea && !IsUVInsideRecognitionArea(FVector2D(U, V)))
    {
        return false;
    }

    OutUV = FVector2D(U, V);
    return true;
}

bool APickupActorAAARuneCanvasInstrument::ResolveDrawUVFromMouseTrace(
    APlayerController *UsingController,
    const FVector2D &ScreenPosition,
    FVector2D &OutUV) const
{
    OutUV = FVector2D::ZeroVector;
    if (!UsingController || !MeshComponent)
    {
        return false;
    }

    FVector RayOrigin = FVector::ZeroVector;
    FVector RayDirection = FVector::ZeroVector;
    if (!UsingController->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, RayOrigin, RayDirection))
    {
        return false;
    }

    UWorld *World = GetWorld();
    if (!World)
    {
        return false;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RuneCanvasMouseTrace), true);
    QueryParams.bReturnFaceIndex = true;
    if (APawn *Pawn = UsingController->GetPawn())
    {
        QueryParams.AddIgnoredActor(Pawn);
    }

    TArray<FHitResult> HitResults;
    const FVector RayEnd = RayOrigin + RayDirection.GetSafeNormal() * FMath::Max(100.f, MouseTraceDistance);
    if (!World->LineTraceMultiByChannel(HitResults, RayOrigin, RayEnd, ECC_Visibility, QueryParams))
    {
        return false;
    }

    for (const FHitResult &HitResult : HitResults)
    {
        const UActorComponent *HitComponent = HitResult.GetComponent();
        // 确保击中点属于当前 Actor 或其组件
        if (HitResult.GetActor() != this && (!HitComponent || HitComponent->GetOwner() != this))
        {
            continue;
        }

        FVector2D SurfaceUV = FVector2D::ZeroVector;
        const FVector SurfaceNormal = DrawSurfaceComponent->GetComponentTransform().GetUnitAxis(EAxis::Z);
        const float SurfaceNormalDot = FMath::Abs(FVector::DotProduct(HitResult.ImpactNormal.GetSafeNormal(), SurfaceNormal));
        if (SurfaceNormalDot < 0.5f || !ResolveDrawUVFromWorldLocation(HitResult.ImpactPoint, SurfaceUV))
        {
            continue;
        }

        FVector2D CollisionUV = FVector2D::ZeroVector;
        if (UGameplayStatics::FindCollisionUV(HitResult, FMath::Max(0, MouseTraceUVChannel), CollisionUV))
        {
            if (CollisionUV.X < 0.f || CollisionUV.X > 1.f || CollisionUV.Y < 0.f || CollisionUV.Y > 1.f)
            {
                continue;
            }

            if (bRestrictDrawingToRecognitionArea && !IsUVInsideRecognitionArea(CollisionUV))
            {
                continue;
            }

            OutUV = CollisionUV;
            return true;
        }
    }

    return false;
}

void APickupActorAAARuneCanvasInstrument::AddDrawPoint(const FVector2D &UV)
{
    if (DrawnUVPoints.Num() > 0 && bCanConnectNextDrawPoint)
    {
        const FVector2D PreviousUV = DrawnUVPoints.Last();
        if (FVector2D::DistSquared(PreviousUV, UV) < FMath::Square(MinDrawPointDistanceUV))
        {
            return;
        }
        DrawStrokeSegment(PreviousUV, UV);
    }
    DrawnUVPoints.Add(UV);
    bCanConnectNextDrawPoint = true;
    ReceiveRuneCanvasDrawStateChanged(DrawnUVPoints, bRuneDrawActive);
}

void APickupActorAAARuneCanvasInstrument::DrawStrokeSegment(const FVector2D &StartUV, const FVector2D &EndUV)
{
    if (!EnsureDrawResources() || !DrawRenderTarget)
    {
        return;
    }

    UCanvas *Canvas = nullptr;
    FVector2D CanvasSize = FVector2D::ZeroVector;
    FDrawToRenderTargetContext DrawContext;
    UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, DrawRenderTarget, Canvas, CanvasSize, DrawContext);

    if (Canvas)
    {
        const FVector2D StartPixel(StartUV.X * CanvasSize.X, StartUV.Y * CanvasSize.Y);
        const FVector2D EndPixel(EndUV.X * CanvasSize.X, EndUV.Y * CanvasSize.Y);
        float AdjustedStrokeThicknessPixels = StrokeThicknessPixels;
        if (bCompensateStrokeAspectRatio)
        {
            const FVector2D SegmentDirection = (EndUV - StartUV).GetSafeNormal();
            const FVector2D SegmentNormal(-SegmentDirection.Y, SegmentDirection.X);
            const float SafeWidth = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.X);
            const float SafeHeight = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.Y);
            const float NormalPhysicalScale = FMath::Sqrt(
                FMath::Square(SegmentNormal.X * SafeWidth) + FMath::Square(SegmentNormal.Y * SafeHeight));
            const float TargetPhysicalScale = FMath::Min(SafeWidth, SafeHeight);
            AdjustedStrokeThicknessPixels *= TargetPhysicalScale / FMath::Max(UE_KINDA_SMALL_NUMBER, NormalPhysicalScale);
        }
        Canvas->K2_DrawLine(StartPixel, EndPixel, FMath::Max(1.f, AdjustedStrokeThicknessPixels), StrokeColor);
    }

    UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, DrawContext);
}

int32 APickupActorAAARuneCanvasInstrument::ResolveHiddenNodeFromUV(const FVector2D &UV) const
{
    if (!IsUVInsideRecognitionArea(UV))
    {
        return INDEX_NONE;
    }

    const FVector2D AreaUV = NormalizeUVToRecognitionArea(UV);
    const int32 SafeRows = FMath::Max(1, HiddenNodeRows);
    const int32 SafeColumns = FMath::Max(1, HiddenNodeColumns);
    const float SafePadding = FMath::Clamp(HiddenNodeEdgePaddingUV, 0.f, 0.45f);
    const float Span = FMath::Max(UE_KINDA_SMALL_NUMBER, 1.f - SafePadding * 2.f);

    const float NormalizedColumn = (AreaUV.X - SafePadding) / Span;
    const float NormalizedRow = (AreaUV.Y - SafePadding) / Span;
    if (NormalizedColumn < 0.f || NormalizedColumn > 1.f || NormalizedRow < 0.f || NormalizedRow > 1.f)
    {
        return INDEX_NONE;
    }

    const int32 ColumnIndex = SafeColumns == 1
                                  ? 0
                                  : FMath::RoundToInt(NormalizedColumn * static_cast<float>(SafeColumns - 1));
    const int32 RowIndex = SafeRows == 1
                               ? 0
                               : FMath::RoundToInt(NormalizedRow * static_cast<float>(SafeRows - 1));

    const FVector2D AreaNodeUV(
        SafeColumns == 1 ? 0.5f : SafePadding + (static_cast<float>(ColumnIndex) / static_cast<float>(SafeColumns - 1)) * Span,
        SafeRows == 1 ? 0.5f : SafePadding + (static_cast<float>(RowIndex) / static_cast<float>(SafeRows - 1)) * Span);

    if (FVector2D::DistSquared(AreaUV, AreaNodeUV) > FMath::Square(HiddenNodeHitRadiusUV))
    {
        return INDEX_NONE;
    }

    return GetHiddenNodeId(RowIndex, ColumnIndex);
}

bool APickupActorAAARuneCanvasInstrument::GetHiddenNodeCoordinatesForId(
    int32 NodeId,
    int32 &OutRow,
    int32 &OutColumn) const
{
    OutRow = INDEX_NONE;
    OutColumn = INDEX_NONE;

    const int32 SafeRows = FMath::Max(1, HiddenNodeRows);
    const int32 SafeColumns = FMath::Max(1, HiddenNodeColumns);
    const int32 ZeroBasedNodeId = NodeId - 1;
    if (ZeroBasedNodeId < 0)
    {
        return false;
    }

    OutRow = ZeroBasedNodeId / SafeColumns;
    OutColumn = ZeroBasedNodeId % SafeColumns;
    return OutRow >= 0 && OutRow < SafeRows && OutColumn >= 0 && OutColumn < SafeColumns;
}

void APickupActorAAARuneCanvasInstrument::TryAppendRecognizedNode(int32 NodeId)
{
    if (NodeId == INDEX_NONE)
    {
        return;
    }

    if (RecognizedNodeSequence.Num() == 0 || !bInterpolateSkippedRecognitionNodes || !bCanInterpolateNextRecognizedNode)
    {
        if (TryAppendSingleRecognizedNode(NodeId) || RecognizedNodeSequence.Last() == NodeId)
        {
            bCanInterpolateNextRecognizedNode = true;
        }
        return;
    }

    const int32 LastNodeId = RecognizedNodeSequence.Last();
    if (LastNodeId == NodeId)
    {
        return;
    }

    AppendInterpolatedNodesBetween(LastNodeId, NodeId);
}

bool APickupActorAAARuneCanvasInstrument::TryAppendSingleRecognizedNode(int32 NodeId)
{
    if (NodeId == INDEX_NONE)
    {
        return false;
    }

    if (RecognizedNodeSequence.Num() > 0 && RecognizedNodeSequence.Last() == NodeId)
    {
        return false;
    }

    if (!bAllowNodeRepeat && RecognizedNodeSequence.Contains(NodeId))
    {
        return false;
    }

    RecognizedNodeSequence.Add(NodeId);
    ReceiveRuneCanvasNodeSequenceChanged(RecognizedNodeSequence, NodeId);
    return true;
}

void APickupActorAAARuneCanvasInstrument::AppendInterpolatedNodesBetween(int32 FromNodeId, int32 ToNodeId)
{
    int32 FromRow = INDEX_NONE;
    int32 FromColumn = INDEX_NONE;
    int32 ToRow = INDEX_NONE;
    int32 ToColumn = INDEX_NONE;

    if (!GetHiddenNodeCoordinatesForId(FromNodeId, FromRow, FromColumn) || !GetHiddenNodeCoordinatesForId(ToNodeId, ToRow, ToColumn))
    {
        TryAppendSingleRecognizedNode(ToNodeId);
        return;
    }

    int32 CurrentColumn = FromColumn;
    int32 CurrentRow = FromRow;
    const int32 DeltaColumn = FMath::Abs(ToColumn - FromColumn);
    const int32 DeltaRow = FMath::Abs(ToRow - FromRow);
    const int32 StepColumn = FromColumn < ToColumn ? 1 : -1;
    const int32 StepRow = FromRow < ToRow ? 1 : -1;
    int32 Error = DeltaColumn - DeltaRow;

    while (!(CurrentColumn == ToColumn && CurrentRow == ToRow))
    {
        const int32 DoubleError = Error * 2;
        if (DoubleError > -DeltaRow)
        {
            Error -= DeltaRow;
            CurrentColumn += StepColumn;
        }
        if (DoubleError < DeltaColumn)
        {
            Error += DeltaColumn;
            CurrentRow += StepRow;
        }

        const int32 InterpolatedNodeId = GetHiddenNodeId(CurrentRow, CurrentColumn);
        if (InterpolatedNodeId != INDEX_NONE)
        {
            TryAppendSingleRecognizedNode(InterpolatedNodeId);
        }
    }
}

FString APickupActorAAARuneCanvasInstrument::BuildRecognizedNodeSequenceString() const
{
    FString Result;
    for (int32 Index = 0; Index < RecognizedNodeSequence.Num(); ++Index)
    {
        if (Index > 0)
        {
            Result += TEXT(", ");
        }
        Result += FString::FromInt(RecognizedNodeSequence[Index]);
    }
    return Result;
}

void APickupActorAAARuneCanvasInstrument::LogRecognizedNodeSequence(const TCHAR *Context) const
{
    if (!bLogRecognizedNodeSequence)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("%s RuneCanvas %s nodes: [%s], Solved: %s, Pattern: %s"),
        *GetName(),
        Context,
        *BuildRecognizedNodeSequenceString(),
        bPatternSolved ? TEXT("true") : TEXT("false"),
        *SolvedPatternId.ToString());
}

bool APickupActorAAARuneCanvasInstrument::TryResolveAcceptedPattern(
    const TArray<int32> &NodeSequence,
    FName &OutPatternId) const
{
    OutPatternId = NAME_None;

    for (const FRuneCanvasPattern &Pattern : AcceptedPatterns)
    {
        if (Pattern.NodeSequence.Num() == 0)
        {
            continue;
        }

        if (bRequireExactPatternMatch)
        {
            if (Pattern.NodeSequence == NodeSequence)
            {
                OutPatternId = Pattern.PatternId;
                return true;
            }
            continue;
        }

        if (NodeSequence.Num() < Pattern.NodeSequence.Num())
        {
            continue;
        }

        bool bMatches = true;
        for (int32 Index = 0; Index < Pattern.NodeSequence.Num(); ++Index)
        {
            if (NodeSequence[Index] != Pattern.NodeSequence[Index])
            {
                bMatches = false;
                break;
            }
        }

        if (bMatches)
        {
            OutPatternId = Pattern.PatternId;
            return true;
        }
    }

    return false;
}
