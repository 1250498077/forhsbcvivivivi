#pragma once

#include "CoreMinimal.h"
#include "PickupActor.h"
#include "PickupActorAAARuneCanvasInstrument.generated.h"

class APlayerController;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture2D;
class UTextureRenderTarget2D;
struct FHitResult;

USTRUCT(BlueprintType)
struct FRuneCanvasPattern
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas")
    FName PatternId = TEXT("Default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas")
    TArray<int32> NodeSequence;
};

UCLASS()
class KONGBU_API APickupActorAAARuneCanvasInstrument : public APickupActor
{
    GENERATED_BODY()

public:
    APickupActorAAARuneCanvasInstrument();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform &Transform) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnPickedUp() override;
    virtual void OnPutDown(FVector PlaceLocation, FRotator PlaceRotation) override;
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce) override;

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas")
    bool BeginRuneDraw(APlayerController *UsingController);

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas")
    bool BeginRuneStroke(APlayerController *UsingController, const FVector2D &ScreenPosition);

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas")
    void EndRuneStroke();

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas")
    void UpdateRuneDrawFromScreenPosition(APlayerController *UsingController, const FVector2D &ScreenPosition);

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas")
    TArray<int32> EndRuneDraw(APlayerController *UsingController);

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas")
    void ResetRuneState();

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas")
    void CommitRuneSequenceAuthority(const TArray<int32> &NodeSequence, AActor *SolvingActor);

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas|RenderTarget")
    void ClearDrawTexture();

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas|RenderTarget")
    void ReinitializeDrawResources();

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas|Card")
    void CycleCardResource(int32 Direction);

    UFUNCTION(BlueprintPure, Category = "RuneCanvas|Card")
    int32 GetCurrentCardResourceIndex() const
    {
        return CurrentCardResourceIndex;
    }

    UFUNCTION(BlueprintPure, Category = "RuneCanvas")
    bool IsRuneDrawActive() const
    {
        return bRuneDrawActive;
    }

    UFUNCTION(BlueprintPure, Category = "RuneCanvas")
    bool IsRuneStrokeActive() const
    {
        return bRuneStrokeActive;
    }

    UFUNCTION(BlueprintPure, Category = "RuneCanvas")
    bool IsRuneSolved() const
    {
        return bPatternSolved;
    }

    UFUNCTION(BlueprintPure, Category = "RuneCanvas")
    FName GetSolvedPatternId() const
    {
        return SolvedPatternId;
    }

    UFUNCTION(BlueprintPure, Category = "RuneCanvas")
    TArray<int32> GetRecognizedNodeSequence() const
    {
        return RecognizedNodeSequence;
    }

    UFUNCTION(BlueprintPure, Category = "RuneCanvas")
    TArray<FVector2D> GetDrawnUVPoints() const
    {
        return DrawnUVPoints;
    }

    UFUNCTION(BlueprintPure, Category = "RuneCanvas|Recognition")
    int32 GetHiddenNodeId(int32 Row, int32 Column) const;

    UFUNCTION(BlueprintPure, Category = "RuneCanvas|RenderTarget")
    UTextureRenderTarget2D *GetDrawRenderTarget() const
    {
        return DrawRenderTarget;
    }

    UFUNCTION(BlueprintPure, Category = "RuneCanvas|Attach")
    bool IsAttachedToSurface() const
    {
        return bAttachedToSurface;
    }

    UFUNCTION(BlueprintPure, Category = "RuneCanvas|Pickup")
    bool CanBePickedUpByPlayer() const
    {
        return !bAttachedToSurface;
    }

    bool CanParticipateInCanvasLinks() const;
    float GetConfiguredCanvasLinkDistance() const;

    bool GetPreferredDrawStartScreenPosition(APlayerController *PC, FVector2D &OutScreenPosition) const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> DrawSurfaceComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UPointLightComponent> GlowLightComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> CanvasLinkRootComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Surface")
    FVector2D DrawSurfaceSize = FVector2D(60.f, 20.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Surface", meta = (ClampMin = "0.05", ClampMax = "4.0", DisplayName = "Draw UV Sensitivity"))
    FVector2D DrawUVSensitivity = FVector2D(1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Surface|MouseTrace")
    bool bUseMouseTraceCollisionUV = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Surface|MouseTrace", meta = (ClampMin = "100.0"))
    float MouseTraceDistance = 5000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Surface|MouseTrace", meta = (ClampMin = "0", UIMin = "0"))
    int32 MouseTraceUVChannel = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Surface", meta = (DisplayName = "Swap Draw Surface Axes"))
    bool bSwapDrawSurfaceAxes = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Surface", meta = (DisplayName = "Invert Draw U"))
    bool bInvertDrawU = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Surface", meta = (DisplayName = "Invert Draw V"))
    bool bInvertDrawV = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|RenderTarget", meta = (ClampMin = "64", ClampMax = "4096"))
    int32 DrawTextureResolution = 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|RenderTarget")
    bool bMatchDrawTextureAspectToSurface = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|RenderTarget")
    FLinearColor DrawTextureClearColor = FLinearColor::Transparent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|RenderTarget")
    FName DrawTextureParameterName = TEXT("DrawTexture");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|RenderTarget")
    FName CardResourceTextureParameterName = TEXT("BaseTexture");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|RenderTarget")
    int32 CardMaterialSlotIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|RenderTarget")
    TObjectPtr<UMaterialInterface> CardBaseMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Draw")
    bool bClearTextureOnBeginDraw = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Draw")
    FLinearColor StrokeColor = FLinearColor(0.12f, 0.85f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Draw", meta = (ClampMin = "1.0", ClampMax = "128.0"))
    float StrokeThicknessPixels = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Draw")
    bool bCompensateStrokeAspectRatio = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Draw", meta = (ClampMin = "0.0001", ClampMax = "0.25"))
    float MinDrawPointDistanceUV = 0.004f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition")
    bool bEnableHiddenNodeRecognition = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Area", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    FVector2D RecognitionAreaCenterUV = FVector2D(0.5f, 0.5f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Area", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    FVector2D RecognitionAreaSizeUV = FVector2D(1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Area")
    bool bRestrictDrawingToRecognitionArea = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Debug")
    bool bDrawRecognitionGridGuide = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Debug")
    bool bShowRecognitionGridPreview = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Debug")
    bool bShowDrawSurfacePreview = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Debug", meta = (ClampMin = "0.01"))
    float DrawSurfacePreviewBorderThickness = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Debug", meta = (ClampMin = "0.001"))
    FVector RecognitionGridPreviewNodeScale = FVector(0.012f, 0.012f, 0.012f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Debug")
    TObjectPtr<UStaticMesh> RecognitionGridPreviewNodeMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Debug")
    TObjectPtr<UMaterialInterface> RecognitionGridPreviewMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Debug")
    TObjectPtr<UMaterialInterface> DrawSurfacePreviewMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Debug")
    FLinearColor RecognitionGridGuideColor = FLinearColor(0.15f, 0.85f, 1.f, 0.45f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Debug", meta = (ClampMin = "1.0", ClampMax = "16.0"))
    float RecognitionGridGuideThicknessPixels = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition", meta = (ClampMin = "1", UIMin = "1"))
    int32 HiddenNodeRows = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition", meta = (ClampMin = "1", UIMin = "1"))
    int32 HiddenNodeColumns = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition", meta = (ClampMin = "0.0", ClampMax = "0.45"))
    float HiddenNodeEdgePaddingUV = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition", meta = (ClampMin = "0.001", ClampMax = "0.5"))
    float HiddenNodeHitRadiusUV = 0.06f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition")
    bool bAllowNodeRepeat = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition")
    bool bInterpolateSkippedRecognitionNodes = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition")
    bool bRequireExactPatternMatch = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Debug")
    bool bLogRecognizedNodeSequence = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Similarity", meta = (ClampMin = "0.1", ClampMax = "20.0"))
    float SimilarityToleranceCells = 4.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Similarity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ShapeSimilarityWeight = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Similarity", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float ExtraUserNodeCost = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition|Similarity", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float MissingExpectedNodeCost = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Patterns", meta = (TitleProperty = "PatternId"))
    TArray<FRuneCanvasPattern> AcceptedPatterns;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Attach", meta = (ClampMin = "0.0"))
    float AttachSurfacePadding = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Attach", meta = (ClampMin = "1.0"))
    float AttachTraceRadius = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Attach", meta = (ClampMin = "1.0"))
    float AttachTraceForwardPadding = 35.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Visual")
    FLinearColor ActivationGlowColor = FLinearColor(0.12f, 0.85f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Visual", meta = (ClampMin = "0.0"))
    float ActivationGlowIntensity = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Visual", meta = (ClampMin = "0.0"))
    float ActivationLightIntensity = 2800.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Visual", meta = (ClampMin = "50.0"))
    float ActivationLightRadius = 220.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "50.0"))
    float LinkDistance = 550.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    bool bRenderLinkChains = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    TObjectPtr<UStaticMesh> ChainLinkMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    TObjectPtr<UMaterialInterface> ChainLinkMaterialOverride = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "1.0"))
    float ChainLinkSpacing = 28.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.001"))
    FVector ChainLinkScale = FVector(1.f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    FRotator ChainLinkRotationOffset = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    bool bAlternateChainLinkRoll = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    float AlternateChainLinkRollDegrees = 90.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.02"))
    float LinkRefreshInterval = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    bool bAnimateLinkPulse = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.0"))
    float LinkPulseSpeed = 1.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.0"))
    float LinkPulseMin = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.0"))
    float LinkPulseMax = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.0"))
    float LinkGlowIntensity = 6.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    FLinearColor LinkGlowColor = FLinearColor(0.65f, 0.95f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    FName LinkPulseScalarParameterName = TEXT("Pulse");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    FName LinkGlowScalarParameterName = TEXT("GlowIntensity");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    FName LinkColorParameterName = TEXT("GlowColor");

    UFUNCTION(BlueprintImplementableEvent, Category = "RuneCanvas|Visual")
    void ReceiveRuneCanvasDrawStateChanged(const TArray<FVector2D> &ActiveUVPoints, bool bDrawingActive);

    UFUNCTION(BlueprintImplementableEvent, Category = "RuneCanvas|Recognition")
    void ReceiveRuneCanvasNodeSequenceChanged(const TArray<int32> &ActiveNodeSequence, int32 NewNodeId);

    UFUNCTION(BlueprintImplementableEvent, Category = "RuneCanvas|Gameplay")
    void ReceiveRuneCanvasPatternSolved(FName PatternId, AActor *SolvingActor);

private:
    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> DrawRenderTarget = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> CardDynamicMaterial = nullptr;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UObject>> LoadedCardResources;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> DefaultRecognitionGridPreviewNodeMesh = nullptr;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> RecognitionGridPreviewComponents;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> ActiveChainLinkComponents;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> ActiveChainLinkMaterialInstances;

    TArray<FVector2D> DrawnUVPoints;
    TArray<int32> RecognizedNodeSequence;

    int32 CurrentCardResourceIndex = 0;
    bool bRuneDrawActive = false;
    bool bRuneStrokeActive = false;
    bool bPatternSolved = false;
    bool bCanConnectNextDrawPoint = false;
    bool bCanInterpolateNextRecognizedNode = false;
    bool bAwaitingThrowImpact = false;
    bool bAttachedToSurface = false;
    FName SolvedPatternId = NAME_None;
    FVector LastAttachTraceLocation = FVector::ZeroVector;
    float LinkRefreshAccumulator = 0.f;
    float LinkPulseTimeAccumulator = 0.f;

    UFUNCTION()
    void HandleRuneCanvasHit(
        UPrimitiveComponent *HitComponent,
        AActor *OtherActor,
        UPrimitiveComponent *OtherComp,
        FVector NormalImpulse,
        const FHitResult &Hit);

    bool EnsureDrawResources();
    void ApplyThrowablePhysicsTuning();
    void RestoreDefaultThrowableCollision();
    void UpdateThrownAttachTrace();
    void StickToImpact(const FHitResult &Hit, UPrimitiveComponent *HitComponent);
    void UpdateActivationVisualState();
    void RefreshCanvasLinks();
    void RebuildChainLinks(const TArray<TPair<FVector, FVector>> &LinkEdges);
    void ClearChainLinks();
    void UpdateChainLinkPulseVisuals(float DeltaTime);
    void LoadCardResources();
    void ApplyCurrentCardResourceTexture();
    const TArray<int32> &GetExpectedNodeSequenceForCurrentCard() const;
    float CalculateNodeSequenceSimilarityPercent(const TArray<int32> &ExpectedNodeSequence, const TArray<int32> &UserNodeSequence) const;
    float CalculateBidirectionalCoverageScore(const TArray<int32> &SourceNodeSequence, const TArray<int32> &TargetNodeSequence) const;
    float CalculateWeightedEditSimilarityScore(const TArray<int32> &ExpectedNodeSequence, const TArray<int32> &UserNodeSequence) const;
    float CalculateNodeDistanceInCells(int32 NodeA, int32 NodeB) const;
    void LogCurrentCardAndUserSequences(const TCHAR *Context, const TArray<int32> &UserNodeSequence) const;
    void EnableMouseTraceCollisionIfNeeded();
    void RestoreHeldCollisionAfterMouseTrace();
    bool IsUVInsideRecognitionArea(const FVector2D &UV) const;
    FVector2D NormalizeUVToRecognitionArea(const FVector2D &UV) const;
    FVector2D DenormalizeRecognitionAreaUV(const FVector2D &AreaUV) const;
    FVector GetDrawSurfaceLocalLocationFromUV(const FVector2D &UV) const;
    void DrawRecognitionGridGuide();
    void RebuildRecognitionGridPreview();
    void ClearRecognitionGridPreview();
    bool ResolveDrawUVFromMouseTrace(APlayerController *UsingController, const FVector2D &ScreenPosition, FVector2D &OutUV) const;
    bool ResolveDrawUVFromScreenPosition(APlayerController *UsingController, const FVector2D &ScreenPosition, FVector2D &OutUV) const;
    bool ResolveDrawUVFromWorldLocation(const FVector &WorldLocation, FVector2D &OutUV) const;
    void AddDrawPoint(const FVector2D &UV);
    void DrawStrokeSegment(const FVector2D &StartUV, const FVector2D &EndUV);
    int32 ResolveHiddenNodeFromUV(const FVector2D &UV) const;
    bool GetHiddenNodeCoordinatesForId(int32 NodeId, int32 &OutRow, int32 &OutColumn) const;
    void TryAppendRecognizedNode(int32 NodeId);
    bool TryAppendSingleRecognizedNode(int32 NodeId);
    void AppendInterpolatedNodesBetween(int32 FromNodeId, int32 ToNodeId);
    FString BuildRecognizedNodeSequenceString() const;
    void LogRecognizedNodeSequence(const TCHAR *Context) const;
    bool TryResolveAcceptedPattern(const TArray<int32> &NodeSequence, FName &OutPatternId) const;
};