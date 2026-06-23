#include "PickupActorAAARuneCanvascommonInstrument.h"

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

}

APickupActorAAARuneCanvascommonInstrument::APickupActorAAARuneCanvascommonInstrument()
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
    ItemLinearDamping = 0.03f;
    ItemAngularDamping = 0.05f;
    ItemThrowSpinRateDegrees = 720.f;

    Tags.Add(FName("RuneCanvasInstrument"));
    Tags.Add(FName("Pickup"));
    Tags.Add(FName("Rune"));

    if (MeshComponent)
    {
        MeshComponent->SetNotifyRigidBodyCollision(true);
        MeshComponent->BodyInstance.bUseCCD = true;
        MeshComponent->OnComponentHit.AddDynamic(this, &APickupActorAAARuneCanvascommonInstrument::HandleRuneCanvasHit);
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
    static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultPlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (DefaultPlaneMesh.Succeeded())
    {
        DefaultRecognitionGridPreviewNodeMesh = DefaultPlaneMesh.Object;
        RecognitionGridPreviewNodeMesh = DefaultPlaneMesh.Object;
    }
}

void APickupActorAAARuneCanvascommonInstrument::BeginPlay()
{
    Super::BeginPlay();
    RebuildRecognitionGridPreview();
    LoadCardResources();
    EnsureDrawResources();
    ApplyThrowablePhysicsTuning();
    ApplyCurrentCardResourceTexture();
    UpdateActivationVisualState();
}

void APickupActorAAARuneCanvascommonInstrument::OnConstruction(const FTransform &Transform)
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

void APickupActorAAARuneCanvascommonInstrument::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopFlatCardFlight(false);
    DrawnUVPoints.Reset();
    RecognizedNodeSequence.Reset();
    LoadedCardResources.Reset();
    ClearRecognitionGridPreview();
    DrawRenderTarget = nullptr;
    CardDynamicMaterial = nullptr;
    bAwaitingThrowImpact = false;
    bAttachedToSurface = false;
    LastAttachTraceLocation = FVector::ZeroVector;

    Super::EndPlay(EndPlayReason);
}

void APickupActorAAARuneCanvascommonInstrument::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateFlatCardFlight(DeltaTime);
    UpdateThrownAttachTrace();
    if (!bAttachedToSurface)
    {
        return;
    }
}

void APickupActorAAARuneCanvascommonInstrument::OnPickedUp()
{
    StopFlatCardFlight(false);
    bAwaitingThrowImpact = false;
    bAttachedToSurface = false;
    LastAttachTraceLocation = FVector::ZeroVector;
    RestoreDefaultThrowableCollision();
    Super::OnPickedUp();
    ResetRuneState();
    OnRuneCanvasDetachedFromSurface();
    UpdateActivationVisualState();
}

void APickupActorAAARuneCanvascommonInstrument::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    StopFlatCardFlight(false);
    bAwaitingThrowImpact = false;
    bAttachedToSurface = false;
    LastAttachTraceLocation = FVector::ZeroVector;
    RestoreDefaultThrowableCollision();
    Super::OnPutDown(PlaceLocation, PlaceRotation);
    ResetRuneState();
    OnRuneCanvasDetachedFromSurface();
    UpdateActivationVisualState();
}

void APickupActorAAARuneCanvascommonInstrument::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    StopFlatCardFlight(false);
    bAwaitingThrowImpact = true;
    bAttachedToSurface = false;
    LastAttachTraceLocation = GetActorLocation();
    RestoreDefaultThrowableCollision();
     if (bUseFlatCardFlight && MeshComponent)
    {
        StartFlatCardFlight(ThrowDirection, ThrowForce);
    }
    else
    {
        Super::OnThrown(ThrowDirection, ThrowForce);
    }
    ResetRuneState();
    UpdateActivationVisualState();
}

void APickupActorAAARuneCanvascommonInstrument::HandleRuneCanvasHit(
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
    OnRuneCanvasAttachedToSurface();
    UpdateActivationVisualState();

    UE_LOG(LogTemp, Log, TEXT("%s rune canvas attached to %s"), *GetName(), *GetNameSafe(OtherActor));
}

bool APickupActorAAARuneCanvascommonInstrument::BeginRuneDraw(APlayerController *UsingController)
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

bool APickupActorAAARuneCanvascommonInstrument::BeginRuneStroke(
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
void APickupActorAAARuneCanvascommonInstrument::EndRuneStroke()
{
    bRuneStrokeActive = false;
    bCanConnectNextDrawPoint = false;
    bCanInterpolateNextRecognizedNode = false;
}

// 3. 根据屏幕位置更新绘制
void APickupActorAAARuneCanvascommonInstrument::UpdateRuneDrawFromScreenPosition(
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

TArray<int32> APickupActorAAARuneCanvascommonInstrument::EndRuneDraw(APlayerController *UsingController)
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

void APickupActorAAARuneCanvascommonInstrument::ResetRuneState()
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

void APickupActorAAARuneCanvascommonInstrument::CommitRuneSequenceAuthority(
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

void APickupActorAAARuneCanvascommonInstrument::ClearDrawTexture()
{
    if (EnsureDrawResources() && DrawRenderTarget)
    {
        UKismetRenderingLibrary::ClearRenderTarget2D(this, DrawRenderTarget, DrawTextureClearColor);
        DrawRecognitionGridGuide();
    }
}

void APickupActorAAARuneCanvascommonInstrument::ReinitializeDrawResources()
{
    DrawRenderTarget = nullptr;
    CardDynamicMaterial = nullptr;
    EnsureDrawResources();
    ApplyCurrentCardResourceTexture();
    ClearDrawTexture();
}

void APickupActorAAARuneCanvascommonInstrument::CycleCardResource(int32 Direction)
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

void APickupActorAAARuneCanvascommonInstrument::LoadCardResources()
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

void APickupActorAAARuneCanvascommonInstrument::ApplyCurrentCardResourceTexture()
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

const TArray<int32> &APickupActorAAARuneCanvascommonInstrument::GetExpectedNodeSequenceForCurrentCard() const
{
    static const TArray<int32> EmptySequence;
    const TArray<TArray<int32>> &ExpectedSequences = GetHardcodedCardExpectedSequences();
    return ExpectedSequences.IsValidIndex(CurrentCardResourceIndex)
               ? ExpectedSequences[CurrentCardResourceIndex]
               : EmptySequence;
}

// --- CalculateNodeSequenceSimilarityPercent ---
float APickupActorAAARuneCanvascommonInstrument::CalculateNodeSequenceSimilarityPercent(
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
float APickupActorAAARuneCanvascommonInstrument::CalculateBidirectionalCoverageScore(
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
float APickupActorAAARuneCanvascommonInstrument::CalculateWeightedEditSimilarityScore(
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
float APickupActorAAARuneCanvascommonInstrument::CalculateNodeDistanceInCells(int32 NodeA, int32 NodeB) const
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

void APickupActorAAARuneCanvascommonInstrument::LogCurrentCardAndUserSequences(
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

int32 APickupActorAAARuneCanvascommonInstrument::GetHiddenNodeId(int32 Row, int32 Column) const
{
    const int32 SafeRows = FMath::Max(1, HiddenNodeRows);
    const int32 SafeColumns = FMath::Max(1, HiddenNodeColumns);
    if (Row < 0 || Row >= SafeRows || Column < 0 || Column >= SafeColumns)
    {
        return INDEX_NONE;
    }

    return Row * SafeColumns + Column + 1;
}

void APickupActorAAARuneCanvascommonInstrument::EnableMouseTraceCollisionIfNeeded()
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

void APickupActorAAARuneCanvascommonInstrument::RestoreHeldCollisionAfterMouseTrace()
{
    if (!bUseMouseTraceCollisionUV || !bIsHeldByPlayer || !MeshComponent)
    {
        return;
    }

    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetGenerateOverlapEvents(false);
}

void APickupActorAAARuneCanvascommonInstrument::ApplyThrowablePhysicsTuning()
{
    ApplyPickupPhysicsTuning();

    if (GlowLightComponent)
    {
        GlowLightComponent->SetLightColor(ActivationGlowColor);
        GlowLightComponent->SetAttenuationRadius(ActivationLightRadius);
    }
}

void APickupActorAAARuneCanvascommonInstrument::RestoreDefaultThrowableCollision()
{
    if (!MeshComponent)
    {
        return;
    }

    ApplyReleasedCollisionProfile();
    MeshComponent->SetNotifyRigidBodyCollision(true);
    MeshComponent->BodyInstance.bUseCCD = true;
}

void APickupActorAAARuneCanvascommonInstrument::StartFlatCardFlight(const FVector &ThrowDirection, float ThrowForce)
{
    if (!MeshComponent)
    {
        return;
    }

    bIsHeldByPlayer = false;
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    RestoreInitialActorScale();
    RestoreDefaultThrowableCollision();
    ApplyThrowablePhysicsTuning();
    ConfigureAttachedDisplayMeshes();

    FVector FlightDirection = ThrowDirection.GetSafeNormal();
    if (FlightDirection.IsNearlyZero())
    {
        FlightDirection = GetActorForwardVector().GetSafeNormal();
    }

    FlightDirection.Z *= FMath::Clamp(FlatCardFlightVerticalAimInfluence, 0.f, 1.f);
    if (FlightDirection.IsNearlyZero())
    {
        FlightDirection = FVector::ForwardVector;
    }

    FlatCardFlightDirection = FlightDirection.GetSafeNormal();
    FlatCardFlightAxisDirection = FlatCardFlightDirection;
    const FVector AxisReferenceUp = FMath::Abs(FVector::DotProduct(FlatCardFlightAxisDirection, FVector::UpVector)) > 0.95f
                                        ? FVector::RightVector
                                        : FVector::UpVector;

    FlatCardFlightAxisRight = FVector::CrossProduct(AxisReferenceUp, FlatCardFlightAxisDirection).GetSafeNormal();
    if (FlatCardFlightAxisRight.IsNearlyZero())
    {
        FlatCardFlightAxisRight = FVector::RightVector;
    }

    FlatCardFlightAxisUp = FVector::CrossProduct(FlatCardFlightAxisDirection, FlatCardFlightAxisRight).GetSafeNormal();
    if (FlatCardFlightAxisUp.IsNearlyZero())
    {
        FlatCardFlightAxisUp = FVector::UpVector;
    }

    FlatCardFlightStartVisualCenter = GetFlatCardVisualCenterWorldLocation();
    FlatCardFlightElapsedTime = 0.f;
    FlatCardFlightSpinAngle = 0.f;
    FlatCardFlightAxisDistance = 0.f;
    FlatCardFlightHelixAngle = 0.f;
    FlatCardFlightHelixAlpha = 0.f;
    bFlatCardFlightActive = true;

    MeshComponent->SetSimulatePhysics(true);
    MeshComponent->SetEnableGravity(false);
    MeshComponent->WakeAllRigidBodies();

    FlatCardFlightSpeed = FMath::Max(0.f, ThrowForce) * ItemThrowForceMultiplier * FMath::Max(0.f, FlatCardFlightSpeedMultiplier);
    MeshComponent->SetPhysicsLinearVelocity(FlatCardFlightAxisDirection * FlatCardFlightSpeed, false);
    MeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector, false);
    ApplyFlatCardFlightRotationPreservingVisualCenter(BuildFlatCardFlightRotation());
}

void APickupActorAAARuneCanvascommonInstrument::UpdateFlatCardFlight(float DeltaTime)
{
    if (!bFlatCardFlightActive || !MeshComponent || !MeshComponent->IsSimulatingPhysics())
    {
        return;
    }

    FlatCardFlightElapsedTime += FMath::Max(0.f, DeltaTime);
    if (FlatCardFlightElapsedTime >= FMath::Max(0.f, FlatCardFlightDuration))
    {
        StopFlatCardFlight(true);
        return;
    }

    if (bEnableFlatCardFlightHelix)
    {
        UpdateFlatCardFlightHelix(DeltaTime);
    }
    else
    {
        UpdateFlatCardFlightCurve(DeltaTime);

        const float CurrentSpeed = MeshComponent->GetPhysicsLinearVelocity().Size();
        const float TargetSpeed = FMath::Max(CurrentSpeed, 1.f);
        MeshComponent->SetPhysicsLinearVelocity(FlatCardFlightDirection * TargetSpeed, false);
    }

    FlatCardFlightSpinAngle = FMath::Fmod(
        FlatCardFlightSpinAngle + FMath::Max(0.f, FlatCardFlightSpinRateDegrees) * DeltaTime,
        360.f);
    ApplyFlatCardFlightRotationPreservingVisualCenter(BuildFlatCardFlightRotation());
}

void APickupActorAAARuneCanvascommonInstrument::UpdateFlatCardFlightCurve(float DeltaTime)
{
    if (!bEnableFlatCardFlightCurve || DeltaTime <= 0.f)
    {
        return;
    }

    FVector CurvedDirection = FlatCardFlightDirection.GetSafeNormal();
    if (CurvedDirection.IsNearlyZero())
    {
        return;
    }

    const float SideCurveAngle = FlatCardFlightSideCurveDegreesPerSecond * DeltaTime;
    if (!FMath::IsNearlyZero(SideCurveAngle))
    {
        CurvedDirection = CurvedDirection.RotateAngleAxis(SideCurveAngle, FVector::UpVector).GetSafeNormal();
    }

    const float LiftCurveAngleRadians = FMath::DegreesToRadians(FlatCardFlightLiftCurveDegreesPerSecond * DeltaTime);
    if (!FMath::IsNearlyZero(LiftCurveAngleRadians))
    {
        CurvedDirection = (CurvedDirection + FVector::UpVector * FMath::Tan(LiftCurveAngleRadians)).GetSafeNormal();
    }

    if (!CurvedDirection.IsNearlyZero())
    {
        FlatCardFlightDirection = CurvedDirection;
    }
}

void APickupActorAAARuneCanvascommonInstrument::UpdateFlatCardFlightHelix(float DeltaTime)
{
    if (DeltaTime <= 0.f || FlatCardFlightSpeed <= UE_KINDA_SMALL_NUMBER)
    {
        return;
    }

    FlatCardFlightAxisDistance += FlatCardFlightSpeed * DeltaTime;
    FlatCardFlightHelixAngle = FMath::Fmod(
        FlatCardFlightHelixAngle + FlatCardFlightHelixAngularSpeedDegreesPerSecond * DeltaTime,
        360.f);

    const float StartDistance = FMath::Max(0.f, FlatCardFlightHelixStartDistance);
    const float BlendDistance = FMath::Max(1.f, FlatCardFlightHelixBlendDistance);
    const float RawHelixAlpha = FMath::Clamp((FlatCardFlightAxisDistance - StartDistance) / BlendDistance, 0.f, 1.f);
    const float HelixAlpha = RawHelixAlpha * RawHelixAlpha * (3.f - 2.f * RawHelixAlpha);
    FlatCardFlightHelixAlpha = HelixAlpha;
    const float HelixRadius = FMath::Max(0.f, FlatCardFlightHelixRadius) * HelixAlpha;

    const float HelixRadians = FMath::DegreesToRadians(FlatCardFlightHelixAngle);
    const FVector HelixOffset = (FlatCardFlightAxisRight * FMath::Cos(HelixRadians) + FlatCardFlightAxisUp * FMath::Sin(HelixRadians)) * HelixRadius;
    const FVector TargetVisualCenter = FlatCardFlightStartVisualCenter + FlatCardFlightAxisDirection * FlatCardFlightAxisDistance + HelixOffset;
    const FVector CurrentVisualCenter = GetFlatCardVisualCenterWorldLocation();
    const FVector DesiredVelocity = (TargetVisualCenter - CurrentVisualCenter) / DeltaTime;

    if (!DesiredVelocity.IsNearlyZero())
    {
        MeshComponent->SetPhysicsLinearVelocity(DesiredVelocity, false);
    }

    FlatCardFlightDirection = FlatCardFlightAxisDirection;
}

void APickupActorAAARuneCanvascommonInstrument::StopFlatCardFlight(bool bRestoreGravity)
{
    if (!bFlatCardFlightActive)
    {
        return;
    }

    bFlatCardFlightActive = false;
    FlatCardFlightElapsedTime = 0.f;
    FlatCardFlightSpinAngle = 0.f;
    FlatCardFlightSpeed = 0.f;
    FlatCardFlightAxisDistance = 0.f;
    FlatCardFlightHelixAngle = 0.f;
    FlatCardFlightHelixAlpha = 0.f;

    if (bRestoreGravity && MeshComponent && MeshComponent->IsSimulatingPhysics())
    {
        MeshComponent->SetEnableGravity(true);
    }
}

FRotator APickupActorAAARuneCanvascommonInstrument::BuildFlatCardFlightRotation() const
{
    
    FVector FlightForward = FlatCardFlightDirection.GetSafeNormal();
    if (FlightForward.IsNearlyZero())
    {
        FlightForward = GetActorForwardVector().GetSafeNormal();
    }
    if (FlightForward.IsNearlyZero())
    {
        FlightForward = FVector::ForwardVector;
    }

    const FVector ReferenceUp = FMath::Abs(FVector::DotProduct(FlightForward, FVector::UpVector)) > 0.95f
        ? FVector::RightVector
        : FVector::UpVector;
    const FQuat BaseRotation = FRotationMatrix::MakeFromXZ(FlightForward, ReferenceUp).ToQuat();
    const FQuat OffsetRotation = FlatCardFlightRotationOffset.Quaternion();

    FVector LocalSpinAxis = FVector::UpVector;

     switch (FlatCardFlightSpinAxis)
    {
    case EFlatCardFlightSpinAxis::Pitch:
        LocalSpinAxis = FVector::RightVector;
        break;

    case EFlatCardFlightSpinAxis::Yaw:
        LocalSpinAxis = FVector::UpVector;
        break;

    case EFlatCardFlightSpinAxis::Roll:
    default:
        LocalSpinAxis = FVector::ForwardVector;
        break;
    }

    FVector FaceWobbleAxis = FVector::RightVector;
    switch (FlatCardFlightHelixFaceWobbleAxis)
    {
    case EFlatCardFlightSpinAxis::Pitch:
        FaceWobbleAxis = FVector::RightVector;
        break;
    case EFlatCardFlightSpinAxis::Yaw:
        FaceWobbleAxis = FVector::UpVector;
        break;
    case EFlatCardFlightSpinAxis::Roll:
    default:
        FaceWobbleAxis = FVector::ForwardVector;
        break;
    }

    float FaceWobbleAngleDegrees = 0.f;
    if (bEnableFlatCardFlightHelix && bEnableFlatCardFlightHelixFaceWobble && FlatCardFlightHelixAlpha > UE_KINDA_SMALL_NUMBER)
    {
        const float FaceWobblePhase = FlatCardFlightHelixAngle * FMath::Max(0.f, FlatCardFlightHelixFaceWobbleCyclesPerHelixTurn) + FlatCardFlightHelixFaceWobblePhaseOffsetDegrees;
        const float FaceWobbleWave = 0.5f - 0.5f * FMath::Cos(FMath::DegreesToRadians(FaceWobblePhase));
        FaceWobbleAngleDegrees = FlatCardFlightHelixFaceWobbleAngleDegrees * FlatCardFlightHelixAlpha * FaceWobbleWave;
    }

    const FQuat FaceWobbleRotation(FaceWobbleAxis, FMath::DegreesToRadians(FaceWobbleAngleDegrees));

    const FQuat SpinRotation(LocalSpinAxis, FMath::DegreesToRadians(FlatCardFlightSpinAngle));
    return (BaseRotation * FaceWobbleRotation * SpinRotation * OffsetRotation).Rotator();
}


FVector APickupActorAAARuneCanvascommonInstrument::GetFlatCardVisualCenterWorldLocation() const
{
    if (!MeshComponent)
    {
        return GetActorLocation();
    }
    FVector LocalBoundsMin = FVector::ZeroVector;
    FVector LocalBoundsMax = FVector::ZeroVector;
    MeshComponent->GetLocalBounds(LocalBoundsMin, LocalBoundsMax);
    const FVector LocalBoundsCenter = (LocalBoundsMin + LocalBoundsMax) * 0.5f;
    return MeshComponent->GetComponentTransform().TransformPosition(LocalBoundsCenter);
}

void APickupActorAAARuneCanvascommonInstrument::ApplyFlatCardFlightRotationPreservingVisualCenter(const FRotator &NewRotation)
{
    if (!MeshComponent)
    {
        return;
    }
    const FVector VisualCenterBeforeRotation = GetFlatCardVisualCenterWorldLocation();
    MeshComponent->SetWorldRotation(NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
    const FVector VisualCenterAfterRotation = GetFlatCardVisualCenterWorldLocation();
    const FVector CenterCorrection = VisualCenterBeforeRotation - VisualCenterAfterRotation;
    if (!CenterCorrection.IsNearlyZero())
    {
        MeshComponent->AddWorldOffset(CenterCorrection, false, nullptr, ETeleportType::TeleportPhysics);
    }
}

void APickupActorAAARuneCanvascommonInstrument::UpdateThrownAttachTrace()
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
    OnRuneCanvasAttachedToSurface();
    UpdateActivationVisualState();

    UE_LOG(LogTemp, Log, TEXT("%s rune canvas attached by trace to %s"), *GetName(), *GetNameSafe(TraceHit.GetActor()));
}

void APickupActorAAARuneCanvascommonInstrument::StickToImpact(const FHitResult &Hit, UPrimitiveComponent *HitComponent)
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

void APickupActorAAARuneCanvascommonInstrument::UpdateActivationVisualState()
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

void APickupActorAAARuneCanvascommonInstrument::OnRuneCanvasAttachedToSurface()
{
}

void APickupActorAAARuneCanvascommonInstrument::OnRuneCanvasDetachedFromSurface()
{
}

void APickupActorAAARuneCanvascommonInstrument::DisableAndHideRuneCanvasForLifetime(float LifeSpanSeconds)
{
    bAttachedToSurface = false;
    bAwaitingThrowImpact = false;

    if (MeshComponent)
    {
        MeshComponent->SetHiddenInGame(true);
        MeshComponent->SetVisibility(false, false);
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        MeshComponent->SetGenerateOverlapEvents(false);
    }

    if (VisualMeshRootComponent)
    {
        VisualMeshRootComponent->SetHiddenInGame(true);
        VisualMeshRootComponent->SetVisibility(false, true);
    }

    if (GlowLightComponent)
    {
        GlowLightComponent->SetHiddenInGame(true);
        GlowLightComponent->SetVisibility(false);
        GlowLightComponent->SetIntensity(0.f);
    }

    SetLifeSpan(FMath::Max(0.05f, LifeSpanSeconds));
}

bool APickupActorAAARuneCanvascommonInstrument::GetPreferredDrawStartScreenPosition(
    APlayerController *PC,
    FVector2D &OutScreenPosition) const
{
    OutScreenPosition = FVector2D::ZeroVector;
    return PC && DrawSurfaceComponent && PC->ProjectWorldLocationToScreen(DrawSurfaceComponent->GetComponentLocation(), OutScreenPosition, true);
}

bool APickupActorAAARuneCanvascommonInstrument::EnsureDrawResources()
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

bool APickupActorAAARuneCanvascommonInstrument::IsUVInsideRecognitionArea(const FVector2D &UV) const
{
    const FVector2D SafeAreaSize(
        FMath::Clamp(RecognitionAreaSizeUV.X, 0.01f, 1.f),
        FMath::Clamp(RecognitionAreaSizeUV.Y, 0.01f, 1.f));
    const FVector2D AreaMin = RecognitionAreaCenterUV - SafeAreaSize * 0.5f;
    const FVector2D AreaMax = RecognitionAreaCenterUV + SafeAreaSize * 0.5f;

    return UV.X >= AreaMin.X && UV.X <= AreaMax.X && UV.Y >= AreaMin.Y && UV.Y <= AreaMax.Y;
}

FVector2D APickupActorAAARuneCanvascommonInstrument::NormalizeUVToRecognitionArea(const FVector2D &UV) const
{
    const FVector2D SafeAreaSize(
        FMath::Clamp(RecognitionAreaSizeUV.X, 0.01f, 1.f),
        FMath::Clamp(RecognitionAreaSizeUV.Y, 0.01f, 1.f));
    const FVector2D AreaMin = RecognitionAreaCenterUV - SafeAreaSize * 0.5f;

    return FVector2D(
        (UV.X - AreaMin.X) / SafeAreaSize.X,
        (UV.Y - AreaMin.Y) / SafeAreaSize.Y);
}

FVector2D APickupActorAAARuneCanvascommonInstrument::DenormalizeRecognitionAreaUV(const FVector2D &AreaUV) const
{
    const FVector2D SafeAreaSize(
        FMath::Clamp(RecognitionAreaSizeUV.X, 0.01f, 1.f),
        FMath::Clamp(RecognitionAreaSizeUV.Y, 0.01f, 1.f));
    const FVector2D AreaMin = RecognitionAreaCenterUV - SafeAreaSize * 0.5f;

    return AreaMin + FVector2D(AreaUV.X * SafeAreaSize.X, AreaUV.Y * SafeAreaSize.Y);
}

FVector APickupActorAAARuneCanvascommonInstrument::GetDrawSurfaceLocalLocationFromUV(const FVector2D &UV) const
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

void APickupActorAAARuneCanvascommonInstrument::RebuildRecognitionGridPreview()
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

void APickupActorAAARuneCanvascommonInstrument::ClearRecognitionGridPreview()
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

void APickupActorAAARuneCanvascommonInstrument::DrawRecognitionGridGuide()
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

bool APickupActorAAARuneCanvascommonInstrument::ResolveDrawUVFromScreenPosition(
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
bool APickupActorAAARuneCanvascommonInstrument::ResolveDrawUVFromWorldLocation(
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

bool APickupActorAAARuneCanvascommonInstrument::ResolveDrawUVFromMouseTrace(
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

void APickupActorAAARuneCanvascommonInstrument::AddDrawPoint(const FVector2D &UV)
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

void APickupActorAAARuneCanvascommonInstrument::DrawStrokeSegment(const FVector2D &StartUV, const FVector2D &EndUV)
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

int32 APickupActorAAARuneCanvascommonInstrument::ResolveHiddenNodeFromUV(const FVector2D &UV) const
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

bool APickupActorAAARuneCanvascommonInstrument::GetHiddenNodeCoordinatesForId(
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

void APickupActorAAARuneCanvascommonInstrument::TryAppendRecognizedNode(int32 NodeId)
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

bool APickupActorAAARuneCanvascommonInstrument::TryAppendSingleRecognizedNode(int32 NodeId)
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

void APickupActorAAARuneCanvascommonInstrument::AppendInterpolatedNodesBetween(int32 FromNodeId, int32 ToNodeId)
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

FString APickupActorAAARuneCanvascommonInstrument::BuildRecognizedNodeSequenceString() const
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

void APickupActorAAARuneCanvascommonInstrument::LogRecognizedNodeSequence(const TCHAR *Context) const
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

bool APickupActorAAARuneCanvascommonInstrument::TryResolveAcceptedPattern(
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