#pragma once

#include "CoreMinimal.h"
#include "PickupActor.h"
#include "PickupActorAAARuneCanvasInstrument.generated.h"

class APlayerController;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UTextureRenderTarget2D;

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
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnPickedUp() override;
    virtual void OnPutDown(FVector PlaceLocation, FRotator PlaceRotation) override;
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce) override;

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas")
    bool BeginRuneDraw(APlayerController* UsingController);

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas")
    void UpdateRuneDrawFromScreenPosition(APlayerController* UsingController, const FVector2D& ScreenPosition);

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas")
    TArray<int32> EndRuneDraw(APlayerController* UsingController);

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas")
    void ResetRuneState();

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas")
    void CommitRuneSequenceAuthority(const TArray<int32>& NodeSequence, AActor* SolvingActor);

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas|RenderTarget")
    void ClearDrawTexture();

    UFUNCTION(BlueprintCallable, Category = "RuneCanvas|RenderTarget")
    void ReinitializeDrawResources();

    UFUNCTION(BlueprintPure, Category = "RuneCanvas")
    bool IsRuneDrawActive() const
    {
        return bRuneDrawActive;
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
    UTextureRenderTarget2D* GetDrawRenderTarget() const
    {
        return DrawRenderTarget;
    }

    bool GetPreferredDrawStartScreenPosition(APlayerController* PC, FVector2D& OutScreenPosition) const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> DrawSurfaceComponent;

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
    bool bInvertDrawW = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|RenderTarget", meta = (ClampMin = "64", ClampMax = "4096"))
    int32 DrawTextureResolution = 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|RenderTarget")
    bool bMatchDrawTextureAspectToSurface = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|RenderTarget")
    FLinearColor DrawTextureClearColor = FLinearColor::Transparent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|RenderTarget")
    FName DrawTextureParameterName = TEXT("DrawTexture");

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
    bool bCompensateStrokeAspectRatio = true;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Patterns", meta = (TitleProperty = "PatternId"))
    TArray<FRuneCanvasPattern> AcceptedPatterns;

    UFUNCTION(BlueprintImplementableEvent, Category = "RuneCanvas|Visual")
    void ReceiveRuneCanvasDrawStateChanged(const TArray<FVector2D>& ActiveUVPoints, bool bDrawingActive);

    UFUNCTION(BlueprintImplementableEvent, Category = "RuneCanvas|Recognition")
    void ReceiveRuneCanvasNodeSequenceChanged(const TArray<int32>& ActiveNodeSequence, int32 NewNodeId);

    UFUNCTION(BlueprintImplementableEvent, Category = "RuneCanvas|Gameplay")
    void ReceiveRuneCanvasPatternSolved(FName PatternId, AActor* SolvingActor);

private:
    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> DrawRenderTarget = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> CardDynamicMaterial = nullptr;

    TArray<FVector2D> DrawnUVPoints;
    TArray<int32> RecognizedNodeSequence;

    bool bRuneDrawActive = false;
    bool bPatternSolved = false;
    bool bCanConnectNextDrawPoint = false;
    FName SolvedPatternId = NAME_None;

    bool EnsureDrawResources();
    void EnableMouseTraceCollisionIfNeeded();
    void RestoreHeldCollisionAfterMouseTrace();
    bool IsUVInsideRecognitionArea(const FVector2D& UV) const;
    FVector2D NormalizeUVToRecognitionArea(const FVector2D& UV) const;
    FVector2D DenormalizeRecognitionAreaUV(const FVector2D& AreaUV) const;
    void DrawRecognitionGridGuide();
    bool ResolveDrawUVFromMouseTrace(APlayerController* UsingController, const FVector2D& ScreenPosition, FVector2D& OutUV) const;
    bool ResolveDrawUVFromScreenPosition(APlayerController* UsingController, const FVector2D& ScreenPosition, FVector2D& OutUV) const;
    bool ResolveDrawUVFromWorldLocation(const FVector& WorldLocation, FVector2D& OutUV) const;
    void AddDrawPoint(const FVector2D& UV);
    void DrawStrokeSegment(const FVector2D& StartUV, const FVector2D& EndUV);
    int32 ResolveHiddenNodeFromUV(const FVector2D& UV) const;
    bool GetHiddenNodeCoordinatesForId(int32 NodeId, int32& OutRow, int32& OutColumn) const;
    void TryAppendRecognizedNode(int32 NodeId);
    bool TryAppendSingleRecognizedNode(int32 NodeId);
    void AppendInterpolatedNodesBetween(int32 FromNodeId, int32 ToNodeId);
    FString BuildRecognizedNodeSequenceString() const;
    void LogRecognizedNodeSequence(const TCHAR* Context) const;
    bool TryResolveAcceptedPattern(const TArray<int32>& NodeSequence, FName& OutPatternId) const;
};