#pragma once

#include "CoreMinimal.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "PickupActor.h"
#include "PickupActorAAARuneGridInstrument.generated.h"

class UArrowComponent;
class APlayerController;
struct FPropertyChangedEvent;
class UMaterialInterface;
class USceneComponent;
class USplineMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

//=============================================================================
// 蓝图使用说明 (矩阵版画符)
//=============================================================================
//
// 1. 直接创建一个基于本类的蓝图，把本体 Mesh 设成你想拿在手里的法器、木板、镜子或卷轴。
// 2. 本类会自动在 GridRootComponent 下生成一个可调的矩阵，默认 10x10。
// 3. 通过 GridRows、GridColumns、CellSpacing、CellMesh、CellScale 调整格子外观。
// 4. 如果只想“滑过高亮”，把 bEnableLineRendering 关掉即可。
// 5. 如果想识别某个固定轨迹，在 AcceptedPatterns 中填写格子序列编号。
//    编号规则：从左上到右下，按行递增。例如 10x10 时，第一行是 1~10，第二行是 11~20。
// 6. 输入逻辑可以沿用 RuneInstrument 的 Begin/Update/End 调用方式；
//    后续如果你要接到玩家控制器，只需要像原来一样在按住右键时持续喂屏幕坐标即可。
// 7. 右键按下时，控制器会调用 GetPreferredDrawStartScreenPosition()，
//    自动把鼠标吸附到“起始定位点附近最近的格子中心”对应的屏幕位置。
//    你可以在蓝图里直接移动 DrawStartAnchorComponent，来决定鼠标一按下时优先从哪一...（文字被截断）
// 8. 如果不想用自定义起点，也可以关闭 bUseDrawStartAnchor，
//    代码就会退回到左上角第一个格子作为默认起点。
// 9. DrawStartAnchorMarkerComponent 是给编辑器看的小标记，方便你在蓝图视口里定位起...（文字被截断）
//    它默认隐藏于游戏中，不会影响实际运行表现。
// ===========================================================================

USTRUCT(BlueprintType)
struct FRuneGridPattern
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid")
    FName PatternId = TEXT("Default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid")
    TArray<int32> CellSequence;
};

UENUM(BlueprintType)
enum class ERuneGridVisualPreset : uint8
{
    Custom UMETA(DisplayName = "Custom"),
    RuneMirror UMETA(DisplayName = "符镜版"),
    WoodenBoard UMETA(DisplayName = "木盘版"),
    GroundArray UMETA(DisplayName = "地面法阵版")
};

UCLASS()
class KONGBU_API APickupActorAAARuneGridInstrument : public APickupActor
{
    GENERATED_BODY()

public:
    APickupActorAAARuneGridInstrument();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnPickedUp() override;
    
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    UFUNCTION(BlueprintCallable, Category = "RuneGrid")
    bool BeginRuneDraw(APlayerController* UsingController);

    UFUNCTION(BlueprintCallable, Category = "RuneGrid")
    void UpdateRuneDrawFromScreenPosition(APlayerController* UsingController, const FVector2D& ScreenPosition);

    UFUNCTION(BlueprintCallable, Category = "RuneGrid")
    TArray<int32> EndRuneDraw(APlayerController* UsingController);

    UFUNCTION(BlueprintCallable, Category = "RuneGrid")
    void ResetRuneState();

    UFUNCTION(BlueprintCallable, Category = "RuneGrid")
    void CommitRuneSequenceAuthority(const TArray<int32>& CellSequence, AActor* SolvingActor);

    UFUNCTION(BlueprintCallable, Category = "RuneGrid")
    void RebuildGrid();

    UFUNCTION(BlueprintCallable, Category = "RuneGrid|Preset")
    void ApplyVisualPreset();

    UFUNCTION(BlueprintPure, Category = "RuneGrid")
    bool IsRuneDrawActive() const
    {
        return bRuneDrawActive;
    }

    UFUNCTION(BlueprintPure, Category = "RuneGrid")
    bool IsRuneSolved() const
    {
        return bPatternSolved;
    }

    UFUNCTION(BlueprintPure, Category = "RuneGrid")
    TArray<int32> GetCommittedRuneSequence() const
    {
        return CommittedSequence;
    }

    UFUNCTION(BlueprintPure, Category = "RuneGrid")
    TArray<int32> GetActiveRuneSequence() const
    {
        return bRuneDrawActive ? CurrentDrawSequence : CommittedSequence;
    }

    UFUNCTION(BlueprintPure, Category = "RuneGrid")
    int32 GetHoveredCellId() const
    {
        return HoveredCellId;
    }

    UFUNCTION(BlueprintPure, Category = "RuneGrid")
    int32 GetGridCellId(int32 Row, int32 Column) const;

    bool GetPreferredDrawStartScreenPosition(APlayerController* PC, FVector2D& OutScreenPosition) const;
    void GetCellScreenPositions(APlayerController* PC, TArray<TPair<int32, FVector2D>>& OutPositions) const;

protected:

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* GridRootComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* GridLineRootComponent;
    
    // 右键按下开始画符时，鼠标优先吸附到这个组件投影到屏幕上的位置。
    // 现在代码会以它为“参考点”，自动寻找离它最近的格子中心作为真正起笔点。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* DrawStartAnchorComponent;

    // 仅用于编辑器中可视化 DrawStartAnchorComponent 的小标记。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UArrowComponent* DrawStartAnchorMarkerComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Layout", meta = (ClampMin = "1", UIMin = "1"))
    int32 GridRows = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Layout", meta = (ClampMin = "1", UIMin = "1"))
    int32 GridColumns = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Layout")
    FVector2D CellSpacing = FVector2D(2.f, 2.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Layout")
    FVector CellScale = FVector(0.07f, 0.07f, 0.07f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Layout")
    FRotator CellRelativeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Layout")
    bool bCenterGridAroundOrigin = true;

    // 是否优先使用 DrawStartAnchorComponent 作为右键按下时的鼠标起始定位点。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|DrawStart")
    bool bUseDrawStartAnchor = true;

    // 传统“按中心点距离判定”的半径；当投影边界不可用时会退回用它。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Draw", meta = (ClampMin = "1.0"))
    float CellSelectionScreenRadius = 24.f;

    // 是否优先使用格子投影到屏幕后的边界框来做命中判定。
    // 开启后，鼠标只要视觉上进入格子范围附近，就更容易高亮，不必非常靠中心。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Draw")
    bool bUseProjectedCellBoundsSelection = true;

    // 在屏幕投影边界的基础上额外放大的像素边距。
    // 手持法器有轻微倾斜时，适当调大可提升“看起来已经碰到格子就能选中”的手感。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Draw", meta = (ClampMin = "0.0"))
    float CellScreenBoundsPadding = 12.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Draw")
    bool bAllowCellRepeat = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Draw")
    bool bRequireExactPatternMatch = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Visual")
    bool bEnableLineRendering = false;

//=======================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Preset")
    ERuneGridVisualPreset VisualPreset = ERuneGridVisualPreset::RuneMirror;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Preset")
    bool bAutoApplyPresetOnConstruction = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Patterns", meta = (TitleProperty = "PatternId"))
    TArray<FRuneGridPattern> AcceptedPatterns;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Cell")
    UStaticMesh* CellMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Cell")
    UMaterialInterface* CellMaterialOverride = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Cell")
    UMaterialInterface* HoveredCellMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Cell")
    UMaterialInterface* TracedCellMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Cell")
    UMaterialInterface* SolvedCellMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Cell|DefaultStyle")
    bool bUseGeneratedDefaultMaterials = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Cell|DefaultStyle")
    FLinearColor IdleCellTint = FLinearColor(0.07f, 0.12f, 0.18f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Cell|DefaultStyle")
    FLinearColor HoveredCellTint = FLinearColor(0.40f, 0.90f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Cell|DefaultStyle")
    FLinearColor TracedCellTint = FLinearColor(0.14f, 0.70f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Cell|DefaultStyle")
    FLinearColor SolvedCellTint = FLinearColor(1.0f, 0.86f, 0.30f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Cell|DefaultStyle", meta = (ClampMin = "0.0"))
    float IdleGlowStrength = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Cell|DefaultStyle", meta = (ClampMin = "0.0"))
    float HoveredGlowStrength = 3.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Cell|DefaultStyle", meta = (ClampMin = "0.0"))
    float TracedGlowStrength = 1.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Cell|DefaultStyle", meta = (ClampMin = "0.0"))
    float SolvedGlowStrength = 4.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Line")
    UStaticMesh* LineSegmentMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Line")
    UMaterialInterface* LineMaterialOverride = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Line|DefaultStyle")
    FLinearColor LineTint = FLinearColor(0.22f, 0.80f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Line|DefaultStyle", meta = (ClampMin = "0.0"))
    float LineGlowStrength = 2.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Line", meta = (ClampMin = "0.01"))
    float LineThickness = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneGrid|Line")
    TEnumAsByte<ESplineMeshAxis::Type> LineForwardAxis = ESplineMeshAxis::Z;

    UFUNCTION(BlueprintImplementableEvent, Category = "RuneGrid|Visual")
    void ReceiveRuneGridVisualStateChanged(const TArray<int32>& ActiveSequence, bool bSolved, bool bDrawingActive);

    UFUNCTION(BlueprintImplementableEvent, Category = "RuneGrid|Visual")
    void ReceiveRuneGridHoverStateChanged(int32 InHoveredCellId, bool bDrawingActive);

    UFUNCTION(BlueprintImplementableEvent, Category = "RuneGrid|Gameplay")
    void ReceiveRuneGridPatternSolved(FName PatternId, AActor* SolvingActor);

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> GeneratedCellComponents;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USplineMeshComponent>> ActiveLineComponents;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> DefaultCellMesh = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> DefaultLineMesh = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> DefaultVisualBaseMaterial = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> GeneratedIdleCellMaterial = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> GeneratedHoveredCellMaterial = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> GeneratedTracedCellMaterial = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> GeneratedSolvedCellMaterial = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> GeneratedLineMaterial = nullptr;

    TMap<int32, TWeakObjectPtr<UStaticMeshComponent>> CellComponentById;
    TMap<int32, TArray<UMaterialInterface*>> OriginalCellMaterials;
    TArray<int32> CurrentDrawSequence;
    TArray<int32> CommittedSequence;
    
    int32 HoveredCellId = INDEX_NONE;
    bool bRuneDrawActive = false;
    bool bPatternSolved = false;
    FName SolvedPatternId = NAME_None;

    void ClearGeneratedGridComponents();
    void GenerateGridComponents();
    FVector GetCellRelativeLocation(int32 RowIndex, int32 ColumnIndex) const;
    FVector GetCellWorldLocationById(int32 CellId) const;
    int32 FindNearestCellIdToWorldLocation(const FVector& WorldLocation) const;
    bool GetGridCoordinatesForCellId(int32 CellId, int32& OutRow, int32& OutColumn) const;
    int32 ResolveHoveredCellFromScreenPosition(APlayerController* UsingController, const FVector2D& ScreenPosition) const;
    bool TryAppendSingleResolvedCell(int32 CellId);
    void AppendInterpolatedCellsBetween(int32 FromCellId, int32 ToCellId);
    bool TryAppendResolvedCell(int32 CellId);
    bool TryResolveAcceptedPattern(const TArray<int32>& CellSequence, FName& OutPatternId) const;
    void ApplySequenceToVisuals(const TArray<int32>& CellSequence, bool bSolved, bool bDrawingActive);
    void ApplyCellVisuals(const TArray<int32>& CellSequence, bool bSolved, bool bDrawingActive);
    void RefreshGeneratedDefaultMaterials();
    UMaterialInstanceDynamic* CreateGeneratedStyleMaterial(const TCHAR* MaterialName, const FLinearColor& Tint, float GlowStrength);
    void ApplyColorParametersToMaterial(UMaterialInstanceDynamic* MaterialInstance, const FLinearColor& Tint, float GlowStrength) const;
    void ApplyMaterialToCell(UStaticMeshComponent* CellComponent, UMaterialInterface* Material) const;
    void SetHoveredCellId(int32 NewHoveredCellId, bool bForceBroadcast = false);
    void RebuildLineMeshes(const TArray<int32>& CellSequence);
    void ClearLineMeshes();
    void CacheOriginalCellMaterials(int32 CellId, UStaticMeshComponent* CellComponent);
};