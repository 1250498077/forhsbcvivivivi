#pragma once

#include "CoreMinimal.h"
#include "PickupActor.h"
#include "TimerManager.h"
#include "PickupActorAAARuneCanvascommonInstrument.generated.h"

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

UENUM(BlueprintType)
enum class EFlatCardFlightSpinAxis : uint8
{
    Pitch UMETA(DisplayName = "Pitch"),
    Yaw   UMETA(DisplayName = "Yaw"),
    Roll  UMETA(DisplayName = "Roll")
};

UCLASS()
class KONGBU_API APickupActorAAARuneCanvascommonInstrument : public APickupActor
{
    GENERATED_BODY()

public:
    APickupActorAAARuneCanvascommonInstrument();

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

    bool GetPreferredDrawStartScreenPosition(APlayerController *PC, FVector2D &OutScreenPosition) const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> DrawSurfaceComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UPointLightComponent> GlowLightComponent;

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
    FVector RecognitionGridPreviewNodeScale = FVector(0.007f, 0.007f, 0.012f);

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
    int32 HiddenNodeRows = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Recognition", meta = (ClampMin = "1", UIMin = "1"))
    int32 HiddenNodeColumns = 40;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight")
    bool bUseFlatCardFlight = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight", meta = (ClampMin = "0.0"))
    float FlatCardFlightDuration = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight", meta = (ClampMin = "0.0"))
    float FlatCardFlightSpeedMultiplier = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float FlatCardFlightVerticalAimInfluence = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight", meta = (ClampMin = "0.0"))
    float FlatCardFlightSpinRateDegrees = 2500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight")
    EFlatCardFlightSpinAxis FlatCardFlightSpinAxis = EFlatCardFlightSpinAxis::Yaw;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight")
    FRotator FlatCardFlightRotationOffset = FRotator(0.f, 90.f, 0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight|Curve")
    bool bEnableFlatCardFlightCurve = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight|Curve", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
    float FlatCardFlightSideCurveDegreesPerSecond = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight|Curve", meta = (ClampMin = "-45.0", ClampMax = "45.0"))
    float FlatCardFlightLiftCurveDegreesPerSecond = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight|Helix")
    bool bEnableFlatCardFlightHelix = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight|Helix", meta = (ClampMin = "0.0"))
    float FlatCardFlightHelixStartDistance = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight|Helix", meta = (ClampMin = "1.0"))
    float FlatCardFlightHelixBlendDistance = 800.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight|Helix", meta = (ClampMin = "0.0"))
    float FlatCardFlightHelixRadius = 60.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight|Helix")
    float FlatCardFlightHelixAngularSpeedDegreesPerSecond = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight|Helix|Face")
    bool bEnableFlatCardFlightHelixFaceWobble = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight|Helix|Face", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
    float FlatCardFlightHelixFaceWobbleAngleDegrees = 90.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight|Helix|Face")
    EFlatCardFlightSpinAxis FlatCardFlightHelixFaceWobbleAxis = EFlatCardFlightSpinAxis::Roll;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight|Helix|Face", meta = (ClampMin = "0.0"))
    float FlatCardFlightHelixFaceWobbleCyclesPerHelixTurn = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Flight|Helix|Face")
    float FlatCardFlightHelixFaceWobblePhaseOffsetDegrees = 90.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Visual")
    FLinearColor ActivationGlowColor = FLinearColor(0.12f, 0.85f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Visual", meta = (ClampMin = "0.0"))
    float ActivationGlowIntensity = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Visual", meta = (ClampMin = "0.0"))
    float ActivationLightIntensity = 2800.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Visual")
    bool bEnableActivationLight = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Visual", meta = (ClampMin = "50.0"))
    float ActivationLightRadius = 220.f;

    UFUNCTION(BlueprintImplementableEvent, Category = "RuneCanvas|Visual")
    void ReceiveRuneCanvasDrawStateChanged(const TArray<FVector2D> &ActiveUVPoints, bool bDrawingActive);

    UFUNCTION(BlueprintImplementableEvent, Category = "RuneCanvas|Recognition")
    void ReceiveRuneCanvasNodeSequenceChanged(const TArray<int32> &ActiveNodeSequence, int32 NewNodeId);

    UFUNCTION(BlueprintImplementableEvent, Category = "RuneCanvas|Gameplay")
    void ReceiveRuneCanvasPatternSolved(FName PatternId, AActor *SolvingActor);

    virtual void OnRuneCanvasAttachedToSurface();
    virtual void OnRuneCanvasDetachedFromSurface();
    void DisableAndHideRuneCanvasForLifetime(float LifeSpanSeconds);

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
    bool bFlatCardFlightActive = false;
    FVector FlatCardFlightDirection = FVector::ForwardVector;
    FVector FlatCardFlightAxisDirection = FVector::ForwardVector;
    FVector FlatCardFlightAxisRight = FVector::RightVector;
    FVector FlatCardFlightAxisUp = FVector::UpVector;
    FVector FlatCardFlightStartVisualCenter = FVector::ZeroVector;
    float FlatCardFlightElapsedTime = 0.f;
    float FlatCardFlightSpinAngle = 0.f;
    float FlatCardFlightSpeed = 0.f;
    float FlatCardFlightAxisDistance = 0.f;
    float FlatCardFlightHelixAngle = 0.f;
    float FlatCardFlightHelixAlpha = 0.f;

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
    void StartFlatCardFlight(const FVector &ThrowDirection, float ThrowForce);
    void UpdateFlatCardFlight(float DeltaTime);
    void UpdateFlatCardFlightCurve(float DeltaTime);
    void UpdateFlatCardFlightHelix(float DeltaTime);
    void StopFlatCardFlight(bool bRestoreGravity);
    FRotator BuildFlatCardFlightRotation() const;
    FVector GetFlatCardVisualCenterWorldLocation() const;
    void ApplyFlatCardFlightRotationPreservingVisualCenter(const FRotator &NewRotation);
    void UpdateThrownAttachTrace();
    void StickToImpact(const FHitResult &Hit, UPrimitiveComponent *HitComponent);
    void UpdateActivationVisualState();
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