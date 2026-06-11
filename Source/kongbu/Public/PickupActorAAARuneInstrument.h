#pragma once

#include "CoreMinimal.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "PickupActor.h"
#include "PickupActorAAARuneInstrument.generated.h"

class APlayerController;
class UMaterialInterface;
class ULightComponent;
class UMeshComponent;
class UPrimitiveComponent;
class USceneComponent;
class USplineMeshComponent;
class UStaticMesh;

// =====================================================================
// 蓝图使用说明（零基础教程）
// =====================================================================
//
// 【第一步：搭建节点】
//   在蓝图"组件"面板里点击"添加"，加入若干 SceneComponent 或 StaticMeshComponent。
//   将它们依次命名为 line_1、line_2、line_3……（数字即节点编号，从 1 开始）。
//   把这些组件拖到 VisualMeshRootComponent 下面，成为它的子级。
//   把每个 line_X 摆放到你希望"符文接触点"所在的位置。
//
// 【第二步：搭建灯光（可选）】
//   在"组件"面板里添加灯光组件（PointLight、SpotLight 等皆可）。
//   命名规则：light_1 对应 line_1 节点，light_2 对应 line_2，以此类推。
//   灯光的位置、旋转、颜色、强度完全由你在蓝图里决定，代码只管开关。
//   ★ 重要：把每盏灯的"可见性（Visible）"默认取消勾选，即初始为"关"。
//
// 【第三步：配置正确路径】
//   选中蓝图 Actor，在右侧"细节"面板找到 Rune | Patterns > AcceptedPatterns。
//   点击"+"新增一条，填写 PatternId（给这条路径起个名字，如 Cross）。
//   在 NodeSequence 里按顺序填写节点编号，例如 1,3,5,7,9，
//   表示玩家必须按这个顺序依次划过对应节点才算成功。
//
// 【第四步：配置解密后节点材质（可选）】
//   在细节面板 Rune | SolvedNode > SolvedNodeMaterial 里指定一个材质。
//   解密成功后，被经过的 line_X（StaticMeshComponent）会自动切换到这个材质。
//   放下道具、投掷或重新绘制时会自动还原原始材质。
//   如果节点是普通 SceneComponent（没有网格），此项无效果，可不填。
//
// 【第五步：连接蓝图事件（可选进阶）】
//   ReceiveRunePatternSolved(PatternId, SolvingActor)
//     -> 路径完全正确时触发，可在这里开门、播音效、触发剧情。
//   ReceiveRuneVisualStateChanged(ActiveSequence, bSolved, bDrawingActive)
//     -> 每次序列或状态变化都会触发，可用来更新 UI 或连线高亮。
//   ReceiveRuneHoverStateChanged(HoveredNodeId, bDrawingActive)
//     -> 光标悬停到某个节点时触发，可用来做节点高亮提示。
// =====================================================================

USTRUCT(BlueprintType)
struct FRuneInstrumentPattern
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune")
    FName PatternId = TEXT("Default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune")
    TArray<int32> NodeSequence;
};

UCLASS()
class KONGBU_API APickupActorAAARuneInstrument : public APickupActor
{
    GENERATED_BODY()

public:
    APickupActorAAARuneInstrument();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void OnPickedUp() override;
    virtual void OnPutDown(FVector PlaceLocation, FRotator PlaceRotation) override;
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce) override;

    bool BeginRuneDraw(APlayerController* UsingController);
    void UpdateRuneDrawFromScreenPosition(APlayerController* UsingController, const FVector2D& ScreenPosition);
    TArray<int32> EndRuneDraw(APlayerController* UsingController);
    void ResetRuneState();
    void CommitRuneSequenceAuthority(const TArray<int32>& NodeSequence, AActor* SolvingActor);
    bool TryAttachToMatchedGhostZone(APawn* TargetPawn, class AMyAIController* TargetController, UPrimitiveComponent* AttachComponent);

    // 从 ExorcismSubsystem 加载当前局的符咒模式，覆盖 AcceptedPatterns。
    // BeginPlay 时自动调用；也可在蓝图里手动调用以刷新。
    UFUNCTION(BlueprintCallable, Category = "Rune|Exorcism")
    void LoadExorcismPatterns();

    UFUNCTION(BlueprintPure, Category = "Rune")
    bool IsRuneDrawActive() const
    {
        return bRuneDrawActive;
    }

    UFUNCTION(BlueprintPure, Category = "Rune")
    bool IsRuneSolved() const
    {
        return bPatternSolved;
    }

    UFUNCTION(BlueprintPure, Category = "Rune")
    TArray<int32> GetCommittedRuneSequence() const
    {
        return CommittedSequence;
    }

    UFUNCTION(BlueprintPure, Category = "Rune")
    TArray<int32> GetActiveRuneSequence() const
    {
        return bRuneDrawActive ? CurrentDrawSequence : CommittedSequence;
    }

    UFUNCTION(BlueprintPure, Category = "Rune")
    int32 GetHoveredNodeId() const
    {
        return HoveredNodeId;
    }

    bool GetPreferredDrawStartScreenPosition(APlayerController* PC, FVector2D& OutScreenPosition) const;
    void GetNodeScreenPositions(APlayerController* PC, TArray<TPair<int32, FVector2D>>& OutPositions) const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RuneLineRootComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Nodes")
    FString NodeNamePrefix = TEXT("line_");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Nodes", meta = (ClampMin = "1.0"))
    float NodeSelectionScreenRadius = 44.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Draw")
    bool bAllowNodeRepeat = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Patterns", meta = (TitleProperty = "PatternId"))
    TArray<FRuneInstrumentPattern> AcceptedPatterns;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Line")
    UStaticMesh* LineSegmentMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Line")
    UMaterialInterface* LineMaterialOverride;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Line", meta = (ClampMin = "0.05"))
    float LineThickness = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Line")
    TEnumAsByte<ESplineMeshAxis::Type> LineForwardAxis = ESplineMeshAxis::Z;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Light")
    FLinearColor SolvedLightColor = FLinearColor(0.2f, 0.6f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Light")
    FString LightNamePrefix = TEXT("light_");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Light", meta = (ClampMin = "0.0"))
    float SolvedLightIntensity = 5000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Light", meta = (ClampMin = "0.0"))
    float SolvedLightRadius = 50.f;

    // 当画线连接到最后一个节点时，序列中经过的 line_X 节点（StaticMeshComponent）切换为此材质。
    // 留空则不替换。放下 / 投掷 / 重新绘制时自动还原原始材质。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|SolvedNode")
    UMaterialInterface* SolvedNodeMaterial = nullptr;

    // 符文器本体质量（千克）。
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Throw", meta = (ClampMin = "0.01"))
    // float RuneMassKg = 1.0f;

    // 投掷力度倍率。
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Throw", meta = (ClampMin = "0.0"))
    // float ThrowForceMultiplier = 1.0f;

    // 投掷时自旋速率（度/秒）。
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Throw", meta = (ClampMin = "0.0"))
    // float ThrowSpinRateDegrees = 2400.f;

    // 自旋轴偏向量，投掷时会加入随机扰动。
    // 当 bUseLocalSpaceSpinAxis 为 true 时，此向量按物体本地空间解释。
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Throw")
    // FVector ThrowSpinAxisBias = FVector(0.35f, 1.0f, 0.2f);

    // 是否使用物体本地空间来解释 ItemThrowSpinAxisBias。
    // 开启后，不同朝向的道具可拥有更自然且可控的旋转方向。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Throw")
    bool bUseLocalSpaceSpinAxis = true;

    // 投掷瞬间附加的初始旋转偏移（度）。
    // 便于不同道具修正“出手时看起来歪”的初始姿态。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Throw")
    FRotator ThrowInitialRotationOffset = FRotator::ZeroRotator;

    // 是否启用 ThrowInitialRotationOffset。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Throw")
    bool bApplyThrowInitialRotationOffset = true;

    // 投掷后延迟允许碎裂的时间窗口（秒）。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Shatter", meta = (ClampMin = "0.0"))
    float ShatterArmDelay = 0.06f;

    // 落地后是否进入稳定状态。开启后仍保留物理和重力，只使用 Pickup 的碰撞配置。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Throw")
    bool bSettleIntoGroundStateAfterImpact = false;

    // 碎裂后生成多少碎片。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Shatter", meta = (ClampMin = "1", ClampMax = "32"))
    int32 ShardCount = 10;

    // 自定义碎片网格，不填使用默认立方体。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Shatter")
    TObjectPtr<UStaticMesh> ShardMeshOverride = nullptr;

    // 碎片最小尺寸。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Shatter")
    FVector ShardBoxExtentMin = FVector(1.5f, 0.15f, 2.5f);

    // 碎片最大尺寸。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Shatter")
    FVector ShardBoxExtentMax = FVector(4.0f, 0.45f, 8.0f);

    // 碎片爆散冲量。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Shatter", meta = (ClampMin = "0.0"))
    float ShardBurstImpulse = 200.f;

    // 碎片继承速度比例。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Shatter", meta = (ClampMin = "0.0"))
    float ImpactVelocityInheritance = 0.25f;

    // 碎片存活时长（秒）。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|Shatter", meta = (ClampMin = "0.2"))
    float ShardLifetime = 5.f;

    // 画对符咒后，必须把法器丢出去并命中“显形且匹配”的鬼，才会真正驱魔。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|ExorcismThrow")
    bool bRequireThrownImpactForExorcism = true;

    // 法器贴到鬼身上后，延迟多久再炸裂并击杀目标。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|ExorcismThrow", meta = (ClampMin = "0.0"))
    float ExorcismDetonationDelay = 2.0f;

    // 贴附期间，额外给鬼补多少显形时间缓冲。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune|ExorcismThrow", meta = (ClampMin = "0.0"))
    float ExorcismRevealExtraTime = 0.25f;

    UFUNCTION(BlueprintImplementableEvent, Category = "Rune|Visual")
    void ReceiveRuneVisualStateChanged(const TArray<int32>& ActiveSequence, bool bSolved, bool bDrawingActive);

    UFUNCTION(BlueprintImplementableEvent, Category = "Rune|Visual")
    void ReceiveRuneHoverStateChanged(int32 InHoveredNodeId, bool bDrawingActive);

    UFUNCTION(BlueprintImplementableEvent, Category = "Rune|Gameplay")
    void ReceiveRunePatternSolved(FName PatternId, AActor* SolvingActor);

private:
    TArray<USplineMeshComponent*> ActiveLineComponents;

    TMap<int32, TWeakObjectPtr<USceneComponent>> ResolvedNodeComponents;
    TMap<int32, TWeakObjectPtr<ULightComponent>> ResolvedLightComponents;
    TMap<int32, TArray<UMaterialInterface*>> OriginalNodeMaterials;
    TArray<int32> PersistentVisualSequence;
    TArray<int32> CurrentDrawSequence;
    TArray<int32> CommittedSequence;
    int32 HoveredNodeId = INDEX_NONE;
    bool bRuneDrawActive = false;
    bool bPatternSolved = false;
    FName SolvedPatternId = NAME_None;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> DefaultShardMesh = nullptr;

    FTimerHandle ShatterArmHandle;
    FTimerHandle ExorcismDetonationHandle;
    bool bCanShatterOnImpact = false;
    bool bHasShattered = false;
    bool bRuneArmedForExorcism = false;
    bool bAwaitingArmedThrowFirstImpact = false;
    bool bAttachedToExorcismGhost = false;
    int32 ArmedGhostTypeId = INDEX_NONE;
    TWeakObjectPtr<AActor> ArmedSolvingActor;
    TWeakObjectPtr<APawn> AttachedGhostPawn;
    TWeakObjectPtr<class AMyAIController> AttachedGhostController;

    UFUNCTION()
    void HandleRuneHit(
        UPrimitiveComponent* HitComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        FVector NormalImpulse,
        const FHitResult& Hit);

    void ArmRuneForImpactShatter();
    void DisarmRuneShatter();
    void SetRuneGroundedState();
    void ShatterRune(const FHitResult& Hit, const FVector& NormalImpulse);
    void SpawnRuneShards(const FVector& ImpactPoint, const FVector& ImpactNormal, const FVector& RuneVelocity);
    void ApplyRunePhysicsTuning();
    UStaticMesh* ResolveShardMesh() const;
    
    bool TryResolveMatchedRevealedGhost(AActor* OtherActor, APawn*& OutPawn, class AMyAIController*& OutAIController) const;
    void AttachRuneToGhost(APawn* HitPawn, UPrimitiveComponent* HitComponent, const FHitResult& Hit);
    void AttachRuneToGhostZone(APawn* HitPawn, class AMyAIController* AIController, UPrimitiveComponent* AttachComponent);
    void DetonateAttachedExorcism();

    void ResolveNodeBindings();
    void ResolveLightBindings();
    int32 ResolveHoveredNodeFromScreenPosition(APlayerController* UsingController, const FVector2D& ScreenPosition) const;
    bool TryAppendResolvedNode(int32 NodeId);
    bool TryResolveAcceptedPattern(const TArray<int32>& NodeSequence, FName& OutPatternId) const;
    bool HasReachedVisualFinalNode(const TArray<int32>& NodeSequence) const;
    FVector GetNodeWorldLocationById(int32 NodeId) const;
    void ApplySequenceToVisuals(const TArray<int32>& NodeSequence, bool bSolved, bool bDrawingActive);
    void SetHoveredNodeId(int32 NewHoveredNodeId, bool bForceBroadcast = false);
    void RebuildLineMeshes(const TArray<int32>& NodeSequence);
    void ClearLineMeshes();
    void SpawnSolvedLights(const TArray<int32>& NodeSequence);
    void ClearSolvedLights();
    void ApplySolvedNodeMaterials(const TArray<int32>& NodeSequence);
    void ClearSolvedNodeMaterials();
};
