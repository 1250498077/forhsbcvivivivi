#pragma once

#include "CoreMinimal.h"
#include "PickupActor.h"
#include "Components/SplineMeshComponent.h"
#include "PickupActorAAAJesusCross.generated.h"

// --- 新增的前向声明 ---
class AMyAIController;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USceneComponent;
class USplineMeshComponent;
class UStaticMesh;
// -----------------------

/**
 * 蓝图对接说明文档
 * 1. 创建 BP_JesusCross，父类选 APickupActorAAAJesusCross。
 * 2. 给 MeshComponent 指定十字架模型；如果十字架底座、发光圈等是额外零件，挂到 VisualMeshRootComponent 下。
 * 3. 这个物品不是单独生效，而是至少 3 个十字架一起放在地上形成区域，所以测试时不要只摆 1 个。
 * 4. 先在关卡里摆 3 个以上十字架，彼此间距离小于 LinkDistance，再运行观察是否能形成连线和三角区域。
 * 5. 第一次调试强烈建议打开 bEnableCrossDebug，这样能直接看到哪些十字架已连线、哪些区域被判为有效弱化区。
 * 6. 如果明明摆了 3 个却没形成区域，优先检查：距离是否过远、其中某个是不是还在玩家手上、或者 MinimumTriangleArea 设得太大。
 * 7. GhostSlowMultiplier 是鬼在区域内的减速倍率，数值越小越慢；先用默认值跑通，再微调强度。
 * 8. 这类物品通常需要多实例联动，新手调试时建议开一个空场景只放十字架和鬼，先把区域逻辑跑通再放进正式关卡。
 */
UCLASS()
class KONGBU_API APickupActorAAAJesusCross : public APickupActor
{
    GENERATED_BODY()

public:
    APickupActorAAAJesusCross();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;

    virtual void OnPickedUp() override;
    virtual void OnPutDown(FVector PlaceLocation, FRotator PlaceRotation) override;
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce) override;
    virtual void DisableByRage_Implementation(AActor* DisablingActor) override;
    virtual void RestoreAfterRageDisable_Implementation() override;

    bool CanParticipateInWeaknessZone() const;
    float GetConfiguredLinkDistance() const;

protected:
    // --- 新增的组件引用 ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* CrossLinkRootComponent;
    // -----------------------

    // 两个十字架在水平面距离不超过这个值时，才允许视为“连线成立”。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Zone", meta = (ClampMin = "50.0"))
    float LinkDistance = 550.f;

    // 一个连通区域里至少要有多少个十字架，才开始尝试生成有效区域。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Zone", meta = (ClampMin = "3"))
    int32 MinimumCrossCount = 3;

    // 三角形面积过小时容易出现“几乎一条线也算区域”，这里用最小面积做保护。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Zone", meta = (ClampMin = "1.0"))
    float MinimumTriangleArea = 1200.f;

    // 是否要求十字架必须保持基本竖直，才允许参与区域连线。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Zone")
    bool bRequireUprightForWeaknessZone = true;

    // 十字架朝上的方向与世界 Up 的点乘阈值。
    // 1 表示完全竖直，0 表示完全横倒。0.75 约等于允许最多 41 度倾斜。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Zone", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float UprightDotThreshold = 0.75f;

    // 鬼待在十字架区域内时的减速倍率。
    // 0.15 表示只保留 15% 的原始速度，1.0 表示不减速。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Effect", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float GhostSlowMultiplier = 0.15f;

    // 每次刷新时给鬼续上的虚弱持续时间。
    // 只要鬼还在区域里，这个时间会不断被刷新；离开后会自然结束。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Effect", meta = (ClampMin = "0.05"))
    float WeaknessRefreshDuration = 0.2f;

    // 多久重新评估一次区域。
    // 值越小反应越及时，但调试和扫描也会更频繁。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Effect", meta = (ClampMin = "0.02"))
    float ZoneRefreshInterval = 0.1f;

    // 是否显示十字架之间的可连线和有效三角区调试图。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Debug")
    bool bEnableCrossDebug = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Debug")
    FColor LinkedDebugColor = FColor(255, 220, 120);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Debug")
    FColor TriangleDebugColor = FColor(120, 255, 160);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Debug")
    FColor InactiveDebugColor = FColor(255, 96, 96);

    // --- 新增的视觉配置变量 ---
    // 是否使用传入的网格渲染十字架之间的连线。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Visual")
    bool bRenderLinkMeshes = true;

    // 用于渲染十字架连线的网格，通常传一个细长圆柱体或线段模型。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Visual")
    UStaticMesh* LinkSegmentMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Visual")
    UMaterialInterface* LinkMaterialOverride = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Visual", meta = (ClampMin = "0.01"))
    float LinkThickness = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Visual")
    TEnumAsByte<ESplineMeshAxis::Type> LinkForwardAxis = ESplineMeshAxis::Z;

    // 是否给连线材质写入脉冲参数，做出结界呼吸感。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Visual")
    bool bAnimateLinkPulse = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Visual", meta = (ClampMin = "0.0"))
    float LinkPulseSpeed = 1.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Visual", meta = (ClampMin = "0.0"))
    float LinkPulseMin = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Visual", meta = (ClampMin = "0.0"))
    float LinkPulseMax = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Visual", meta = (ClampMin = "0.0"))
    float LinkGlowIntensity = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Visual")
    FLinearColor LinkGlowColor = FLinearColor(0.65f, 0.95f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Visual")
    FName LinkPulseScalarParameterName = TEXT("Pulse");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Visual")
    FName LinkGlowScalarParameterName = TEXT("GlowIntensity");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Visual")
    FName LinkColorParameterName = TEXT("GlowColor");
    // -----------------------

    // 鬼显形时撞到十字架，额外补一小段冲量，帮助把十字架撞倒。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Physics")
    bool bApplyImpulseWhenRevealedGhostHits = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Physics", meta = (ClampMin = "0.0"))
    float RevealedGhostPushImpulse = 140.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Physics", meta = (ClampMin = "0.0"))
    float RevealedGhostUpwardImpulse = 45.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Physics", meta = (ClampMin = "0.0"))
    float RevealedGhostImpulseCooldown = 0.2f;

    // 如果角色碰撞事件没有稳定触发，则用这个额外距离作为“显形鬼已靠近十字架”的兜底判定。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Physics", meta = (ClampMin = "0.0"))
    float RevealedGhostProximityPadding = 50.f;


    // 只给线性推力时，底盘较宽的十字架往往只会平移不容易翻倒。
    // 这里额外补一个角冲量，帮助它更明显地向侧面倾倒。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Physics", meta = (ClampMin = "0.0"))
    float RevealedGhostAngularImpulseDegrees = 900.f;

    // 线性冲量施加在十字架高度的哪个比例位置。
    // 值越高，越接近顶部，越容易形成翻倒力矩。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cross|Physics", meta = (ClampMin = "0.0", ClampMax = "1.5"))
    float RevealedGhostImpulseHeightRatio = 0.65f;

private:
    float ZoneRefreshAccumulator = 0.f;
    float LinkPulseTimeAccumulator = 0.f;
    float LastRevealedGhostImpulseTime = -1000.f;
    TArray<TWeakObjectPtr<AMyAIController>> AffectedControllers;
    TArray<TObjectPtr<USplineMeshComponent>> ActiveLinkComponents;
    TArray<TObjectPtr<UMaterialInstanceDynamic>> ActiveLinkMaterialInstances;

    void RefreshCrossZone();
    void ClearAllAffectedControllers(bool bNotifyControllers = true);
    void EnsureReleasedWorldPhysicsState(bool bWakeRigidBodies);
    void RebuildLinkMeshes(const TArray<TPair<FVector, FVector>>& LinkEdges);
    void ClearLinkMeshes();
    void UpdateLinkMeshPulseVisuals(float DeltaTime);
    void TryApplyRevealedGhostProximityImpulse();
    void ApplyRevealedGhostImpulseFromPawn(APawn* OtherPawn, float CurrentTime);

    UFUNCTION()
    void HandleCrossMeshHit(
        UPrimitiveComponent* HitComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        FVector NormalImpulse,
        const FHitResult& Hit);
    };

