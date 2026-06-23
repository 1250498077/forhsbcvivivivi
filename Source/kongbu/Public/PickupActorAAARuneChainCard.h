#pragma once

#include "CoreMinimal.h"
#include "PickupActorAAARuneCanvascommonInstrument.h"
#include "PickupActorAAARuneChainCard.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FHitResult;

/**
 * 链条边的数据结构，描述两个符文画布之间的一条连接线段。
 * EdgeKey 由两端 Canvas 的 PathName 拼接而成（字典序排序，保证唯一性）。
 */
struct FRuneCanvasLinkEdge
{
    /** 唯一标识此边的字符串键，格式为 "PathA|PathB"（PathA < PathB 字典序）。 */
    FString EdgeKey;

    /** 链条起点的世界坐标（对应本 Canvas 的锚点位置）。 */
    FVector Start = FVector::ZeroVector;

    /** 链条终点的世界坐标（对应另一端 Canvas 的锚点位置）。 */
    FVector End = FVector::ZeroVector;
};

/**
 * 断裂链条节点的纯视觉模拟状态（不使用物理引擎时）。
 * 在 UpdateBrokenChainLinkVisuals 中逐帧手动积分，模拟重力与阻尼。
 */
struct FBrokenChainLinkVisualState
{
    /** 当前线速度（cm/s），每帧受重力影响递减 Z 分量，并乘以阻尼因子。 */
    FVector Velocity = FVector::ZeroVector;

    /** 当前角速度（度/s），每帧乘以阻尼因子衰减。 */
    FRotator AngularVelocity = FRotator::ZeroRotator;
};

/**
 * APickupActorAAARuneChainCard
 *
 * 符文链卡拾取物。贴附到表面后，会自动检测附近同类 Canvas，
 * 并在两者之间生成可视化链条（由若干 StaticMeshComponent 组成）。
 * 玩家碰触链条后，链条会在延迟后断裂并飞散，同时触发 Canvas 消失逻辑。
 */
UCLASS()
class KONGBU_API APickupActorAAARuneChainCard : public APickupActorAAARuneCanvascommonInstrument
{
    GENERATED_BODY()

public:
    APickupActorAAARuneChainCard();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform &Transform) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnPickedUp() override;
    virtual void OnPutDown(FVector PlaceLocation, FRotator PlaceRotation) override;
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce) override;

    /**
     * 返回此 Canvas 当前是否具备参与链条连接的资格。
     * 条件：已贴附到表面 && 未被玩家持握 && 未被狂怒状态禁用。
     */
    bool CanParticipateInCanvasLinks() const;

    /** 返回蓝图配置的最大链接距离（LinkDistance）。 */
    float GetConfiguredCanvasLinkDistance() const;

    /** 返回链条锚点的世界坐标（基于 DrawSurfaceComponent + ChainLinkAnchorLocalOffset）。 */
    FVector GetCanvasLinkAnchorWorldLocation() const;

protected:
    /** Canvas 贴附到表面时调用：立即触发一次链条刷新。 */
    virtual void OnRuneCanvasAttachedToSurface() override;

    /** Canvas 从表面脱离时调用：清除所有活跃链条。 */
    virtual void OnRuneCanvasDetachedFromSurface() override;

    // =========================================================================
    // Components
    // =========================================================================

    /**
     * 链条节点的附着根组件，挂载在 VisualMeshRootComponent 下。
     * 所有动态生成的链条 StaticMeshComponent 都附着在此节点上，
     * 方便统一跟随 Canvas 移动，以及统一清理。
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> CanvasLinkRootComponent;

    // =========================================================================
    // RuneCanvas | Links — 链条连接基础设置
    // =========================================================================

    /**
     * 两个 Canvas 锚点之间允许建立链条的最大距离（单位：cm）。
     * 实际生效距离取两端 Canvas 中较小值（Min(A, B)），保证双向对称。
     * 最小值 50 cm，防止极小值导致链条刷新过于频繁。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "50.0"))
    float LinkDistance = 2000.f;

    /**
     * 是否在两个已连接的 Canvas 之间生成可见链条网格。
     * 关闭后链条逻辑（触碰检测、断裂）仍会运行，但不会生成 StaticMeshComponent。
     * 可用于调试或纯逻辑模式。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    bool bRenderLinkChains = true;

    // =========================================================================
    // RuneCanvas | Links — 链条外观资产
    // =========================================================================

    /**
     * 单个链节使用的静态网格体资产。
     * 为空时不生成任何链条节点（即使 bRenderLinkChains 为 true）。
     * 建议使用轴向对称的短柱形或环形网格，以便链条方向旋转后视觉自然。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    TObjectPtr<UStaticMesh> ChainLinkMesh = nullptr;

    /**
     * 链条节点的默认材质覆盖。
     * 为空时使用 ChainLinkMesh 的第 0 个材质槽。
     * 会被包装为 UMaterialInstanceDynamic，以支持发光脉冲动画参数的实时设置。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    TObjectPtr<UMaterialInterface> ChainLinkMaterialOverride = nullptr;

    /**
     * 玩家碰触链条节点时切换的材质覆盖（高亮/激活效果）。
     * 为空时：碰触不会切换材质，且链条节点不开启碰撞检测（节省性能）。
     * 不为空时：节点自动启用 QueryOnly + Pawn Overlap 碰撞，用于触发材质切换和断裂计时器。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    TObjectPtr<UMaterialInterface> ChainLinkTouchedMaterialOverride = nullptr;

    // =========================================================================
    // RuneCanvas | Links — 断裂行为
    // =========================================================================

    /**
     * 玩家触碰链条到链条实际断裂之间的延迟时间（秒）。
     * 设为 0 则立即断裂。
     * 断裂倒计时期间，链条显示为"已触碰"材质（ChainLinkTouchedMaterialOverride）。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.0"))
    float ChainLinkBreakDelay = 2.f;

    /**
     * 断裂链条节点在飞散后的存活时间（秒），超时后销毁组件。
     * 同时也是链条所属 Canvas 在断裂后等待销毁的基准时间
     * （DisableAndHideRuneCanvasForLifetime 传入 BrokenChainLinkLifetime + 0.05f）。
     * 设为 0 则断裂后立即销毁节点。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.0"))
    float BrokenChainLinkLifetime = 3.f;

    /**
     * 链条节点断裂时施加的初始冲量大小（cm/s）。
     * 用于物理模拟模式（bBrokenChainLinksSimulatePhysics=true）时的 AddImpulse。
     * 纯视觉模拟模式下，初始速度 = FMath::Max(120.f, BrokenChainLinkImpulse * 2.6f)，
     * 系数 2.6 补偿无质量模拟时视觉飞散感。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.0"))
    float BrokenChainLinkImpulse = 200.f;

    /**
     * 纯视觉模拟模式下断裂节点受到的虚拟重力加速度（cm/s²）。
     * 仅在 bBrokenChainLinksSimulatePhysics=false 时生效。
     * 独立于世界重力设置，可以调得比真实重力大以增强戏剧感。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.0"))
    float BrokenChainLinkVisualGravity = 650.f;

    /**
     * 纯视觉模拟模式下线速度和角速度的阻尼系数（0~1）。
     * 每帧速度乘以 pow(1 - Damping, DeltaTime)，近似指数衰减。
     * 0 = 无阻尼（永远飞散），1 = 立即停止（不推荐）。
     * 建议范围：0.05~0.15，呈现自然减速效果。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BrokenChainLinkVisualDamping = 0.08f;

    /**
     * 断裂后的链条节点是否启用 UE 物理引擎模拟（SetSimulatePhysics）。
     * true：使用引擎刚体物理，需要 ChainLinkMesh 有碰撞体，性能开销较高。
     * false：使用手写的轻量视觉模拟（UpdateBrokenChainLinkVisuals），
     *        无需碰撞体，性能更好，但不与世界几何体交互。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    bool bBrokenChainLinksSimulatePhysics = false;

    /**
     * 断裂后的链条节点是否与世界几何体发生碰撞。
     * true：开启 QueryAndPhysics + ECR_Block（需配合 bBrokenChainLinksSimulatePhysics）。
     * false：节点穿透所有物体（视觉效果，无碰撞开销）。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    bool bBrokenChainLinksCollideWithWorld = false;

    // =========================================================================
    // RuneCanvas | Links — 链条生成几何参数
    // =========================================================================

    /**
     * 链条锚点相对于 DrawSurfaceComponent 的局部偏移（cm）。
     * 默认 ZeroVector 即使用 DrawSurfaceComponent 原点。
     * 可用于将锚点偏移到卡片边缘或特定装饰位置。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    FVector ChainLinkAnchorLocalOffset = FVector::ZeroVector;

    /**
     * 相邻链条节点中心点之间的间距（cm）。
     * 节点数量 = Ceil(链条总长 / ChainLinkSpacing)，上限 MaxChainLinksPerEdge。
     * 值越小链条越密集，生成的组件数越多，性能开销越高。
     * 最小值限制为 1.0（内部实际 clamp 到 max(1, value)）。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "1.0"))
    float ChainLinkSpacing = 10.f;

    /**
     * 单条链条边允许生成的最大节点数量。
     * 防止两个 Canvas 距离过远时生成海量 StaticMeshComponent 导致卡顿。
     * 建议根据 LinkDistance / ChainLinkSpacing 的比值来估算合理上限。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "1", UIMin = "1"))
    int32 MaxChainLinksPerEdge = 1000;

    /**
     * 每个链条节点的世界缩放（相对于 Actor 的缩放叠加）。
     * X/Y/Z 分别控制网格在局部空间的三个轴向尺寸。
     * 最小分量限制为 0.001，防止缩放到零导致渲染异常。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.001"))
    FVector ChainLinkScale = FVector(1.f, 1.f, 1.f);

    /**
     * 链条节点在对齐链条方向旋转之后额外叠加的旋转偏移。
     * 用于修正静态网格体的局部朝向，使其在链条中呈现正确的视觉姿态。
     * 例如若网格体默认朝向 Z 轴但链条沿 X 轴延伸，可在此添加 90° Pitch 修正。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    FRotator ChainLinkRotationOffset = FRotator::ZeroRotator;

    /**
     * 是否对奇数索引（0-based）的链条节点额外绕链条轴旋转。
     * 开启后相邻节点互相垂直，形成类似金属链的交替环形效果。
     * 旋转角度由 AlternateChainLinkRollDegrees 控制。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    bool bAlternateChainLinkRoll = true;

    /**
     * bAlternateChainLinkRoll 为 true 时，奇数节点额外叠加的 Roll 角度（度）。
     * 90° 产生标准金属链效果；其他值可产生风车、螺旋等变体。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    float AlternateChainLinkRollDegrees = 90.f;

    // =========================================================================
    // RuneCanvas | Links — 链条刷新频率
    // =========================================================================

    /**
     * 链条连接关系的刷新间隔（秒）。
     * 每帧累加 DeltaTime，达到此间隔后重新扫描世界中的 Canvas 并更新链条。
     * 值越小响应越及时，但性能开销越高（每次刷新需要 TActorIterator 遍历）。
     * 内部强制下限为 0.02s（50Hz）防止每帧刷新。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.02"))
    float LinkRefreshInterval = 0.25f;

    // =========================================================================
    // RuneCanvas | Links — 发光脉冲动画
    // =========================================================================

    /**
     * 是否启用链条节点的发光强度脉冲动画。
     * 关闭时所有节点固定使用 LinkPulseMax 作为 Pulse 参数值。
     * 开启时 Pulse 参数在 LinkPulseMin ~ LinkPulseMax 之间以正弦函数周期振荡。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    bool bAnimateLinkPulse = true;

    /**
     * 脉冲动画的振荡频率（rad/s）。
     * 实际周期 = 2π / LinkPulseSpeed 秒。
     * 值越大闪烁越快；0 则停止动画（等价于关闭 bAnimateLinkPulse）。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.0"))
    float LinkPulseSpeed = 1.6f;

    /**
     * Pulse 材质参数的最小值（对应正弦波谷）。
     * 控制链条最暗时的脉冲强度。建议保持大于 0 以避免链条完全熄灭。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.0"))
    float LinkPulseMin = 0.35f;

    /**
     * Pulse 材质参数的最大值（对应正弦波峰）。
     * 控制链条最亮时的脉冲强度。同时也是 bAnimateLinkPulse=false 时的固定值。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.0"))
    float LinkPulseMax = 1.0f;

    /**
     * 链条节点的基础发光强度乘数，传入材质的 GlowIntensity 等参数。
     * 脉冲动画开启时实际强度 = LinkGlowIntensity * PulseValue（随脉冲振荡）。
     * 数值依赖材质的 HDR 设置；典型范围 1~20。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links", meta = (ClampMin = "0.0"))
    float LinkGlowIntensity = 6.f;

    /**
     * 链条节点的发光颜色（HDR Linear Color，Alpha 通常设为 1）。
     * 同时写入材质的 GlowColor 和 EmissiveColor 两个参数名，
     * 兼容不同命名规范的材质。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    FLinearColor LinkGlowColor = FLinearColor(0.65f, 0.95f, 1.0f, 1.0f);

    // =========================================================================
    // RuneCanvas | Links — 材质参数名称
    // =========================================================================

    /**
     * 材质中控制脉冲强度的标量参数名称。
     * 每帧由 UpdateChainLinkPulseVisuals 写入。
     * 需与链条材质中的参数名完全一致（区分大小写）。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    FName LinkPulseScalarParameterName = TEXT("Pulse");

    /**
     * 材质中控制发光强度的标量参数名称。
     * 每帧由 UpdateChainLinkPulseVisuals 写入（值为 LinkGlowIntensity * PulseValue）。
     * 需与链条材质中的参数名完全一致。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    FName LinkGlowScalarParameterName = TEXT("GlowIntensity");

    /**
     * 材质中控制发光颜色的向量参数名称。
     * 每帧由 UpdateChainLinkPulseVisuals 写入 LinkGlowColor。
     * 需与链条材质中的参数名完全一致。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Links")
    FName LinkColorParameterName = TEXT("GlowColor");

private:
    // =========================================================================
    // 运行时状态（Transient，不序列化）
    // =========================================================================

    /** 当前所有活跃链条节点组件的列表（按生成顺序）。ClearChainLinks 时销毁或转移。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> ActiveChainLinkComponents;

    /** 已断裂并进入飞散阶段的链条节点组件。FinishBrokenChainLinkComponent 超时后销毁。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> BrokenChainLinkComponents;

    /** 所有活跃链条节点的 UMaterialInstanceDynamic 列表，用于统一更新脉冲参数。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> ActiveChainLinkMaterialInstances;

    /** 链条节点 → 默认材质实例的映射（ChainLinkMaterialOverride 或网格体原材质派生）。 */
    UPROPERTY(Transient)
    TMap<TObjectPtr<UStaticMeshComponent>, TObjectPtr<UMaterialInstanceDynamic>> ChainLinkDefaultMaterialInstances;

    /** 链条节点 → 触碰材质实例的映射（ChainLinkTouchedMaterialOverride 派生）。 */
    UPROPERTY(Transient)
    TMap<TObjectPtr<UStaticMeshComponent>, TObjectPtr<UMaterialInstanceDynamic>> ChainLinkTouchedMaterialInstances;

    /** 链条节点 → 所属边 EdgeKey 的映射，用于 Overlap 事件中快速定位边。 */
    UPROPERTY(Transient)
    TMap<TObjectPtr<UStaticMeshComponent>, FString> ChainLinkEdgeKeys;

    /** EdgeKey → 该边所有链条节点列表的映射，用于按边批量操作（触碰高亮、断裂）。 */
    TMap<FString, TArray<TObjectPtr<UStaticMeshComponent>>> ActiveChainLinkComponentsByEdge;

    /** 当前正处于断裂倒计时中的边 EdgeKey 集合（已触碰但尚未调用 BreakChainLinkEdge）。 */
    TSet<FString> BreakingChainLinkEdges;

    /** EdgeKey → 断裂延迟 Timer 句柄的映射，支持在断裂前取消定时器。 */
    TMap<FString, FTimerHandle> ChainLinkBreakTimerHandles;

    /** 断裂节点 → 视觉模拟状态的映射（仅在 bBrokenChainLinksSimulatePhysics=false 时填充）。 */
    TMap<TObjectPtr<UStaticMeshComponent>, FBrokenChainLinkVisualState> BrokenChainLinkVisualStates;

    /**
     * 上一次成功调用 RebuildChainLinks 时的构建签名字符串。
     * 包含链条间距、节点上限、网格体名称及所有边的起终点坐标（网格对齐到 1cm）。
     * 每次刷新时与新签名对比，相同则跳过重建，避免无意义的组件销毁与重创建。
     */
    FString ActiveChainLinkBuildSignature;

    /** 链条刷新间隔的时间累加器（秒），达到 LinkRefreshInterval 后重置并触发刷新。 */
    float LinkRefreshAccumulator = 0.f;

    /** 脉冲动画的时间累加器（rad），每帧累加 DeltaTime * LinkPulseSpeed，传入 sin 函数。 */
    float LinkPulseTimeAccumulator = 0.f;

    // =========================================================================
    // 内部函数
    // =========================================================================

    UFUNCTION()
    void HandleChainLinkBeginOverlap(
        UPrimitiveComponent *OverlappedComponent,
        AActor *OtherActor,
        UPrimitiveComponent *OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult &SweepResult);

    UFUNCTION()
    void HandleChainLinkEndOverlap(
        UPrimitiveComponent *OverlappedComponent,
        AActor *OtherActor,
        UPrimitiveComponent *OtherComp,
        int32 OtherBodyIndex);

    void RefreshCanvasLinks();
    void RebuildChainLinks(const TArray<FRuneCanvasLinkEdge> &LinkEdges);
    FString BuildChainLinkBuildSignature(const TArray<FRuneCanvasLinkEdge> &LinkEdges) const;
    void ClearChainLinks(bool bPreserveBreakingEdges = false);
    void DestroyBrokenChainLinks();
    void ApplyChainLinkDefaultMaterial(UStaticMeshComponent *ChainLinkComponent);
    void ApplyChainLinkTouchedMaterial(UStaticMeshComponent *ChainLinkComponent);
    void MarkChainLinkEdgeTouched(const FString &EdgeKey);
    void BreakChainLinkEdge(FString EdgeKey);
    void FinishBrokenChainLinkComponent(UStaticMeshComponent *ChainLinkComponent);
    void DestroyIfNoActiveCanvasLinks(const FString &BrokenEdgeKey);
    void UpdateChainLinkPulseVisuals(float DeltaTime);
    void UpdateBrokenChainLinkVisuals(float DeltaTime);
};