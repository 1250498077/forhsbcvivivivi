// MyAIController.h
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TimerManager.h"
#include "MyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class APickupActor;

USTRUCT(BlueprintType)
struct KONGBU_API FFearStimulus
{
    GENERATED_BODY()

    // 恐惧源的位置。AI 会以这里作为逃离方向的参考。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fear")
    FVector SourceLocation = FVector::ZeroVector;

    // 本次刺激持续多久。小于等于 0 时会回退到组件默认时长。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fear")
    float Duration = -1.f;

    // 刺激标签，方便区分不同来源的恐惧事件。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fear")
    FName StimulusTag = NAME_None;

    // 可选的来源 Actor，用于需要追踪具体来源对象的场景。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fear")
    TObjectPtr<AActor> SourceActor = nullptr;

    // 这次刺激额外给 Rage 累积多少怒气；小于等于 0 时回退到 Controller 默认值。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fear", meta = (ClampMin = "0"))
    int32 RageAngerAmount = 0;
};

UENUM(BlueprintType)
enum class EEnemyAIState : uint8
{
    // 在导航网格上随机寻找点位进行巡逻。
    Patrol UMETA(DisplayName = "Patrol"),

    // 前往某个声音或事件源位置进行调查。
    Investigate UMETA(DisplayName = "Investigate"),

    // 视觉锁定玩家后直接追击。
    Chase UMETA(DisplayName = "Chase"),

    // 受到恐惧刺激后远离刺激源。
    Flee UMETA(DisplayName = "Flee"),

    // 被特殊道具击中后短暂失去行动目标，原地缓慢转圈。
    Stunned UMETA(DisplayName = "Stunned"),

    // 长时间持续受惊后的愤怒反扑状态。
    Rage UMETA(DisplayName = "Rage")
};

UCLASS()
class KONGBU_API AMyAIController : public AAIController
{
    GENERATED_BODY()

public:
    // 构造函数里创建感知组件并设置默认参数；这里只做组件级初始化，不启动具体行为。
    AMyAIController();

    // 返回 AI 视线起点和方向；调试绘制与部分感知查询会以这里作为“眼睛位置”。
    virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

    // 接管 Pawn 后重置运行时状态，并启动首轮巡逻。
    virtual void OnPossess(APawn* InPawn) override;

    // 每帧驱动状态机。优先级固定为 Fear > 实时追逐 > 丢失后搜查/调查 > 巡逻。
    virtual void Tick(float DeltaTime) override;

    // 处理移动结束后的续动作，例如继续丢失目标搜查、进入原地环顾或恢复巡逻。
    virtual void OnMoveCompleted(FAIRequestID RequestID,
        const FPathFollowingResult& Result) override;

    // AI 感知系统的统一入口。这里会根据 Sense 类型转发到视觉或听觉分支。
    UFUNCTION()
    void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

    // 供外部系统直接注入恐惧刺激，触发逃跑状态。
    UFUNCTION(BlueprintCallable, Category = "AI|Fear")
    void ApplyFearStimulus(const FFearStimulus& Stimulus);

    // 以世界坐标快速构造一条恐惧刺激，方便蓝图或机关事件直接调用。
    UFUNCTION(BlueprintCallable, Category = "AI|Fear")
    void ApplyFearFromLocation(FVector SourceLocation, float Duration = -1.f);

    // 以一个世界坐标发起调查，用于听觉、机关或其它非视觉事件。
    UFUNCTION(BlueprintCallable, Category = "AI|Investigate")
    void ApplyInvestigationFromLocation(FVector SourceLocation, float WaitDuration = -1.f,
        FName StimulusTag = NAME_None);

    // 让鬼被诱饵吸引至位置，直到倒计时结束或被打断
    UFUNCTION(BlueprintCallable, Category = "AI|Lure")
    void ApplyLureFromLocation(FVector SourceLocation, float DazeDuration = 3.f, float AcceptanceRadius = 120.f, AActor* SourceActor = nullptr);

    // 由外部机关或地形给当前 AI 挂上一层减速来源。
    // 减速会统一叠加到 Patrol / Investigate / Chase / Flee / Rage 的最终移动速度上。
    UFUNCTION(BlueprintCallable, Category = "AI|Effects|Slow")
    void ApplySlowSource(AActor* SourceActor, float SlowMultiplier = 0.5f);

    // 移除某个外部减速来源；如果这是最后一个来源，AI 会恢复当前状态应有的速度。
    UFUNCTION(BlueprintCallable, Category = "AI|Effects|Slow")
    void RemoveSlowSource(AActor* SourceActor);

    UFUNCTION(BlueprintPure, Category = "AI|Effects|Slow")
    float GetActiveSlowMultiplier() const;

    // 给当前 AI 追加一个“固定减多少速度”的来源。
    // 该值会直接从当前状态原始目标速度里扣除，而不是做乘法。
    UFUNCTION(BlueprintCallable, Category = "AI|Effects|Slow")
    void ApplySpeedReductionSource(AActor* SourceActor, float SpeedReduction);

    UFUNCTION(BlueprintCallable, Category = "AI|Effects|Slow")
    void RemoveSpeedReductionSource(AActor* SourceActor);

    UFUNCTION(BlueprintPure, Category = "AI|Effects|Slow")
    float GetActiveSpeedReductionAmount() const;
    // 查询鬼当前是否因手电显形或虚弱闪烁等效果处于"可见"状态。
    // 外部物品（瓶子、符纸、符文乐器等）用这个判断能否击中鬼。
    UFUNCTION(BlueprintPure, Category = "AI|Visual")
    bool IsGhostRevealedByEffect() const;
    // 被闪光灯命中一次后，短暂进入“显形”状态。
    // 每次新命中都会刷新计时，因此连续闪中时鬼会表现为一闪一闪地显形。
    UFUNCTION(BlueprintCallable, Category = "AI|Visual")
    void ApplyFlashlightReveal(float RevealDuration = -1.f);

    // 让鬼进入“虚弱”状态：按控制器频率在显形和隐形之间切换。
    // 持续时间由外部调用时传入，方便道具按自己的配置控制。
    UFUNCTION(BlueprintCallable, Category = "AI|Effects|Weakness")
    void ApplyWeakness(float Duration);

    // 提前结束虚弱状态，立刻回到默认隐身表现。
    UFUNCTION(BlueprintCallable, Category = "AI|Effects|Weakness")
    void ClearWeakness();

    UFUNCTION(BlueprintPure, Category = "AI|Effects|Weakness")
    bool IsWeaknessActive() const;

    // 只累计 Rage 怒气，不触发 Fear 逃跑。
    // 适合手电这类“会激怒鬼，但不会把鬼吓跑”的道具。
    UFUNCTION(BlueprintCallable, Category = "AI|Fear|Rage")
    void ApplyFearAngerStimulus(const FFearStimulus& Stimulus);

    // 为当前鬼确定驱魔类型。优先使用 GhostCharacter 蓝图里配置的类型编号，未配置时再从 ExorcismSubsystem 领取。
    UFUNCTION(BlueprintCallable, Category = "AI|Exorcism")
    void AssignExorcismGhostType();

    // 当前鬼被分配的驱魔类型编号。INDEX_NONE 表示尚未分配。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Exorcism")
    int32 ExorcismGhostTypeId = INDEX_NONE;

    UFUNCTION(BlueprintPure, Category = "AI|State")
    EEnemyAIState GetCurrentAIState() const
    {
        return CurrentState;
    }

    UFUNCTION(BlueprintPure, Category = "AI|State")
    bool IsCurrentAIState(EEnemyAIState State) const
    {
        return CurrentState == State;
    }

    UFUNCTION(BlueprintPure, Category = "AI|State")
    bool CanCurrentlySeePlayer() const
    {
        return bCanSeePlayer && IsValid(TargetPlayer);
    }

    UFUNCTION(BlueprintPure, Category = "AI|State")
    AActor* GetCurrentTargetPlayer() const
    {
        return TargetPlayer;
    }

    bool TryGetTrackedPlayerLocation(FVector& OutLocation) const;

    // 让鬼短暂进入眩晕：原地缓慢转圈、立刻丢失当前仇恨，并在结束后回到巡逻。
    UFUNCTION(BlueprintCallable, Category = "AI|Effects|Stun")
    void ApplyStun(float Duration = -1.f);

    // 带来源版本的眩晕入口。
    // 用于像音响这类持续脉冲源，让控制器能识别“是不是同一个来源在重复眩晕”。
    void ApplyStunFromSource(float Duration, AActor* SourceActor);

    // 给鬼挂上一层持续禁用来源；只要来源还在，鬼就持续处于 Stunned。
    void ApplyAttachedDisableSource(AActor* SourceActor);

    // 移除一个持续禁用来源；最后一个来源移除后，鬼才允许恢复。
    void RemoveAttachedDisableSource(AActor* SourceActor);


protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAIPerceptionComponent* AIPerception;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAISenseConfig_Sight* SightConfig;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAISenseConfig_Hearing* HearingConfig;

    // 视觉/调试线优先尝试读取的眼睛 Socket 名称。
    // 例如可填 head、Head、eyes 或你的骨骼里实际存在的 socket 名。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Perception")
    FName SightOriginSocketName = NAME_None;

    // 当没有可用的眼睛 Socket 时，额外在视点基础上加的本地偏移。
    // 例如把 Z 调高一点，就能把视觉锥从肚子抬到头部附近。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Perception")
    FVector SightOriginOffset = FVector::ZeroVector;



    // 移动失败后多久重试一次。值越小，AI 越快重新找路。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement", meta = (ClampMin = "0.0"))
    float FailedMoveRetryDelay = 0.2f;

    // 调查目标的到达判定半径。越大越容易判定“已经走到声音点附近”。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Investigate", meta = (ClampMin = "10.0"))
    float InvestigateAcceptanceRadius = 80.f;

    // 到达调查点后默认原地停留多久。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Investigate", meta = (ClampMin = "0.1"))
    float DefaultInvestigateWaitDuration = 1.75f;

    // 正在看见玩家时是否忽略新的噪音事件。开启后视觉追逐优先级更高。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Investigate")
    bool bIgnoreNoiseWhileSeeingPlayer = true;

    // 在关卡里放几个空 Actor 或 TargetPoint，并给它们加上这个 Tag，
    // 鬼受惊后会从这些点里挑一个尝试逃去躲藏。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear")
    FName FearHidePointTag = TEXT("GhostHidePoint");

    // Fear 的第一段反应：先缓慢后退一小段时间，模拟“被吓到先缩一下”。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "0.1"))
    float FearRetreatDuration = 0.55f;

    // 第一段后退的目标距离。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "50.0"))
    float FearRetreatDistance = 260.f;

    // 第一段后退时的速度倍率，相对于当前基础速度做乘法。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "0.1", ClampMax = "2.0"))
    float FearRetreatSpeedMultiplier = 0.45f;

    // 第一段后退时允许的最低速度，避免乘完倍率后慢得几乎不动。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "50.0"))
    float FearRetreatMinWalkSpeed = 120.f;

    // Fear 的第二段反应：朝躲藏点加速逃跑。
    // 躲藏逃跑状态最多持续多久，超时后会重新评估行为。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "0.1"))
    float FearHideEscapeDuration = 20.f;

    // 朝躲藏点冲刺时的速度倍率。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float FearHideRunSpeedMultiplier = 1.2f;

    // 朝躲藏点冲刺时的最低速度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "100.0"))
    float FearHideRunMinWalkSpeed = 260.f;

    // 进入躲藏点多近时算到达。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "10.0"))
    float FearHideAcceptanceRadius = 70.f;

    // 逃向躲藏点时，如果恐惧源仍然挡在前方且距离足够近，就强制重新进入“缓慢后退”。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "50.0"))
    float FearEscapeBlockSourceDistance = 420.f;

    // 恐惧源与前进方向的点积阈值。越大越要求“正前方挡路”才算阻挡。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float FearEscapeBlockDirectionDot = 0.35f;

    // 即使恐惧源不完全在正前方，只要它贴近当前逃跑路径，也认为这条路被挡住了。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "10.0"))
    float FearEscapePathBlockRadius = 180.f;

    // 被恐惧源挡住时，先左右试探绕开，而不是立刻又回到同一条前冲路径。
    // 绕路时侧向试探距离。越大越愿意明显地往旁边绕开。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "50.0"))
    float FearEscapeDetourDistance = 260.f;

    // 绕路时允许稍微往后退开的偏置量，避免卡在恐惧源脸上。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "0.0"))
    float FearEscapeDetourBackBias = 120.f;

    // 一次绕路尝试持续多久，结束后会重新寻路或重算目标。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "0.1"))
    float FearEscapeDetourDuration = 1.1f;

    // Controller 自己持有受惊状态，不再依赖外部 FearReceiverComponent。
    // 是否响应外部注入的恐惧刺激。关闭后鬼不会因为 ItemX 或机关进入 Fear。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear")
    bool bReactToFearStimuli = true;

    // 恐惧刺激默认持续时间。外部没单独指定 Duration 时使用这个值。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "0.1"))
    float DefaultFearDuration = 2.5f;

    // 恐惧逃离的目标距离。越大越会往更远的地方跑。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "100.0"))
    float FearEscapeDistance = 900.f;

    // Fear 状态下多久重新寻路一次，避免一直沿过期路径乱跑。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear", meta = (ClampMin = "0.1"))
    float FearRepathInterval = 0.35f;

    // 一旦真正跑进躲藏点范围，就直接结束受惊并回到巡逻。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear")
    bool bReturnToPatrolAfterHideReached = true;

    // 同一个恐惧源每隔多久最多累计一次怒气，避免高频脉冲瞬间把 Rage 打满。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear|Rage", meta = (ClampMin = "0.1"))
    float FearAngerHitCooldown = 0.75f;

    // 一次有效恐惧命中增加多少怒气值。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear|Rage", meta = (ClampMin = "1"))
    int32 FearAngerPerStimulus = 1;

    // Rage 怒气衰减检查的时间间隔。越短，怒气回落得越频繁。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear|Rage", meta = (ClampMin = "0.1"))
    float FearAngerDecayInterval = 1.5f;

    // 怒气达到多少时进入 Rage 状态。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear|Rage", meta = (ClampMin = "1"))
    int32 FearAngerThreshold = 10;

    // Rage 状态持续多久。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear|Rage", meta = (ClampMin = "0.1"))
    float RageDuration = 10.f;

    // Rage 期间的速度倍率。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear|Rage", meta = (ClampMin = "0.1", ClampMax = "4.0"))
    float RageSpeedMultiplier = 1.8f;

    // Rage 期间的最低移动速度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear|Rage", meta = (ClampMin = "100.0"))
    float RageMinWalkSpeed = 500.f;

    // Rage 状态下接近道具到多近时，才允许去执行禁用道具的动作。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear|Rage", meta = (ClampMin = "50.0"))
    float RageDisableItemAcceptanceRadius = 120.f;

    // Rage 调查恐惧源位置时会停留多久。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Fear|Rage", meta = (ClampMin = "0.1"))
    float RageSourceInvestigateDuration = 2.5f;

    // 鬼平时隐身时使用的不透明度。
    // 例如 0.35 表示默认半透明；数值越低越接近完全隐身。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Visual", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HiddenGhostOpacity = 0.35f;

    // 被闪光灯照到时使用的不透明度。
    // 一般保持 1，表示完全显形。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Visual", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RevealedGhostOpacity = 1.f;

    // 单次闪光命中后，鬼维持显形多久。
    // 如果闪光灯没额外传入时长，就回退到这里。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Visual", meta = (ClampMin = "0.01"))
    float DefaultFlashlightRevealDuration = 0.12f;

    // 虚弱期间每秒切换多少次显形/隐形。
    // 例如 4 表示每秒切换 4 次，值越高闪烁越快。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Effects|Weakness", meta = (ClampMin = "0.1"))
    float WeaknessRevealToggleFrequency = 4.f;

    // 单次眩晕默认持续时长。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Effects|Stun", meta = (ClampMin = "0.1"))
    float DefaultStunDuration = 3.f;

    // 眩晕时每秒旋转多少度；120 度约等于 3 秒转一整圈。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Effects|Stun", meta = (ClampMin = "0.0"))
    float StunTurnRateDegrees = 120.f;

    // 鬼因为连续眩晕而进入 Rage 后，再额外获得多久的抗眩晕时间。
    // 这样它有机会摆脱音响范围，而不是刚结束 Rage 又被原地重新眩晕。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Effects|Stun", meta = (ClampMin = "0.0"))
    float RageStunImmunityDuration = 2.75f;

    // 鬼显隐时写给材质的颜色参数。
    // 默认白色代表只改透明度，不额外偏色；如果你的鬼材质支持，也可以在蓝图里改成冷色或青色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Visual")
    FLinearColor GhostTintColor = FLinearColor::White;

    // 隐身状态使用的材质覆盖列表。
    // 如果原始人物材质是 Opaque，单改 Opacity 不会生效；这时请在蓝图里给这里填一套半透明材质。
    // 数组下标对应 Mesh 的材质槽；没填的槽位会回退到原材质参数写法。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Visual")
    TArray<TObjectPtr<UMaterialInterface>> HiddenGhostMaterialOverrides;

    // 刚丢失玩家视野后，最后已知位置还会被持续跟踪多久。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Investigate", meta = (ClampMin = "0.0"))
    float LostSightTrackDuration = 1.f;

    // 丢失视野后最长搜查时长。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Investigate", meta = (ClampMin = "0.1"))
    float LostSightMaxSearchDuration = 4.f;

    // 原地左右扫视时的总半角范围。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Investigate", meta = (ClampMin = "0.0", ClampMax = "180.0"))
    float LostSightLookAroundSweepDegrees = 70.f;

    // 原地扫视的转头速度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Investigate", meta = (ClampMin = "0.0"))
    float LostSightLookAroundTurnRateDegrees = 240.f;

    // 是否显示视野范围调试图。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowDebugSight = true;

    // 是否显示侦测连线，例如看见玩家或听见事件时的辅助线。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowDetectionLines = true;

    // 是否显示 Fear 调试信息。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowFearDebug = true;

    // 是否在世界里显示当前状态文字。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowStateText = true;

    // 是否在鬼头顶常驻显示驱魔类型编号（不依赖 bShowDebugSight）。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowExorcismDebug = true;

    // 是否显示 Investigate 相关调试信息。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowInvestigateDebug = true;

    // 是否把 Fear 的退避与绕路信息打印到日志里。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bLogFearRetreat = true;

    // 是否把 Rage 威胁列表的登记、切换和移除过程打印到日志里。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bLogRageThreats = true;

    // 视野范围调试颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FColor SightColor = FColor::Green;

    // 丢失视野后的辅助显示颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FColor LoseSightColor = FColor::Yellow;

    // 检测到目标时的辅助显示颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FColor DetectedColor = FColor::Red;

    // Fear 调试线和范围的颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FColor FearDebugColor = FColor(255, 64, 255);

    // Patrol 状态文字或调试图颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FColor PatrolStateColor = FColor::Cyan;

    // Investigate 状态文字或调试图颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FColor InvestigateStateColor = FColor::Orange;

    // Chase 状态文字或调试图颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FColor ChaseStateColor = FColor::Red;

    // Flee 状态文字或调试图颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FColor FleeStateColor = FColor(255, 64, 255);

    // Stunned 状态文字或调试图颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FColor StunnedStateColor = FColor(160, 200, 255);

    // Rage 状态文字或调试图颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FColor RageStateColor = FColor(255, 128, 32);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
    AActor* TargetPlayer = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
    bool bCanSeePlayer = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
    bool bIsMoving = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
    EEnemyAIState CurrentState = EEnemyAIState::Patrol;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
    FVector PatrolTarget = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
    FVector InvestigateTarget = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
    FVector CurrentFleeTarget = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
    FVector LastSeenPlayerLocation = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
    AActor* LostSightTrackedPlayer = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
    AActor* CurrentFearHidePoint = nullptr;

    // 当前是否正处于被闪光灯显形的状态。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Visual")
    bool bFlashlightRevealActive = false;

    // 当前是否正处于虚弱状态。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Effects|Weakness")
    bool bWeaknessActive = false;

    // 虚弱状态这一拍是否处于显形相位。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Effects|Weakness")
    bool bWeaknessRevealPhaseVisible = false;

    // 随机选择一个导航点并发起巡逻；在没有更高优先级事件时作为默认行为。
    void Patrol();

    // 对当前锁定的目标玩家持续发起追逐请求；只要仍可见，就保持该逻辑不变。
    void ChasePlayer();

    // 在当前 Pawn 附近取一个可达随机点，作为巡逻候选位置。
    FVector GetRandomPatrolPoint();

    // 绘制视觉范围、调查点、逃跑方向和状态文本等调试信息。
    void DrawDebugSight();

private:
    enum class ELostSightLookAroundPhase : uint8
    {
        FaceLastKnown,
        SweepLeft,
        SweepRight,
        ReturnCenter,
        Completed
    };

    enum class EFearResponseMode : uint8
    {
        None,
        Retreat,
        EscapeToHide,
        Rage
    };

    // 记录受惊前的移动速度，Flee 结束后再恢复。
    float CachedWalkSpeedBeforeFear = 0.f;
    bool bCachedWalkSpeedValid = false;

    // Investigate/Flee 的内部状态缓存。
    bool bHasInvestigateTarget = false;
    bool bHasLastSeenPlayerData = false;
    bool bPendingFearRepath = false;
    bool bUseLookAroundAfterInvestigateMove = false;
    bool bIsLookAroundActive = false;
    bool bFearRetreatInputActive = false;
    bool bFearHoldingAtHidePoint = false;
    bool bFearDetourActive = false;
    bool bForcePatrolAfterFearEnds = false;
    float InvestigateWaitTimeRemaining = 0.f;
    float FearDetourTimeRemaining = 0.f;
    float FearHideEscapeTimeRemaining = 0.f;
    float FearRetreatTimeRemaining = 0.f;
    float FearAngerHitCooldownRemaining = 0.f;
    float FearAngerDecayTimeRemaining = 1.5f;
    float FearRetreatLogTimeRemaining = 0.f;
    float RageTimeRemaining = 0.f;
    float LostSightSearchTimeRemaining = 0.f;
    float LostSightTrackTimeRemaining = 0.f;
    float LookAroundCenterYaw = 0.f;
    float LookAroundTargetYaw = 0.f;
    float LookAroundYawAccumulated = 0.f;
    float FearTimeRemaining = 0.f;
    float FearRepathTimeRemaining = 0.f;
    float FlashlightRevealTimeRemaining = 0.f;
    float WeaknessTimeRemaining = 0.f;
    float WeaknessRevealToggleTimeAccumulator = 0.f;
    float StunTimeRemaining = 0.f;
    float StunImmunityTimeRemaining = 0.f;
    float SuppressedFearAngerTimeRemaining = 0.f;
    float SuppressedStunTimeRemaining = 0.f;
    float StunTurnDirection = 1.f;
    int32 FearAngerLevel = 0;
    bool bFearActive = false;
    bool bFearShouldRepath = false;
    bool bPendingRageEnter = false;
    bool bHasRageSourceLocation = false;
    bool bRageSourceDisabled = false;
    bool bGhostVisualStateApplied = false;
    FName CurrentInvestigateStimulusTag = NAME_None;
    FName LastFearAngerStimulusTag = NAME_None;
    EFearResponseMode FearResponseMode = EFearResponseMode::None;
    ELostSightLookAroundPhase LookAroundPhase = ELostSightLookAroundPhase::Completed;
    FAIRequestID ActiveMoveRequestId = FAIRequestID::InvalidRequest;
    FFearStimulus ActiveFearStimulus;
    FVector FearDetourTarget = FVector::ZeroVector;
    FVector RageSourceLocation = FVector::ZeroVector;
    FVector LastFearAngerSourceLocation = FVector::ZeroVector;
    FVector SuppressedFearAngerSourceLocation = FVector::ZeroVector;
    FVector SuppressedStunSourceLocation = FVector::ZeroVector;
    FVector LureTargetLocation = FVector::ZeroVector;
    TObjectPtr<AActor> RageSourceActor = nullptr;
    TObjectPtr<AActor> LastFearAngerSourceActor = nullptr;
    TObjectPtr<AActor> SuppressedFearAngerSourceActor = nullptr;
    TObjectPtr<AActor> SuppressedStunSourceActor = nullptr;
    TObjectPtr<AActor> LureSourceActor = nullptr;
    bool bLureActive = false;
    bool bLureReachedTarget = false;
    float LureDazeTimeRemaining = 0.f;
    float LureAcceptanceRadius = 120.f;
    TArray<TWeakObjectPtr<AActor>> ActiveRageThreatSources;
    TWeakObjectPtr<AActor> LoggedRageMoveSubmitFailureSource;
    TWeakObjectPtr<AActor> ActiveRageMoveSourceActor;
    FVector ActiveRageMoveGoalLocation = FVector::ZeroVector;
    bool bHasActiveRageMoveGoalLocation = false;
    float ActiveRageDisableAcceptanceRadius = 0.f;
    TMap<TWeakObjectPtr<AActor>, float> ActiveSlowSources;
    TMap<TWeakObjectPtr<AActor>, float> ActiveSpeedReductionSources;
    TArray<TWeakObjectPtr<AActor>> ActiveAttachedDisableSources;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInterface>> OriginalGhostMaterials;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> GhostVisualMaterials;

    // 记录当前状态下“未乘减速”的目标速度。
    // 外部减速变化时，会以它为基准重新计算最终 MaxWalkSpeed。
    float TrackedUnmodifiedWalkSpeed = 0.f;
    bool bHasTrackedUnmodifiedWalkSpeed = false;

    bool bHasCachedGhostCapsuleCollisionResponses = false;
    bool bHasCachedGhostMeshCollisionResponses = false;
    ECollisionResponse CachedGhostCapsuleWorldDynamicResponse = ECR_Block;
    ECollisionResponse CachedGhostCapsulePhysicsBodyResponse = ECR_Block;
    ECollisionResponse CachedGhostMeshWorldDynamicResponse = ECR_Block;
    ECollisionResponse CachedGhostMeshPhysicsBodyResponse = ECR_Block;

    FTimerHandle PatrolRetryTimerHandle;

    // 收到恐惧刺激后的回调：缓存速度、切到 Flee，并强制打断当前移动。
    UFUNCTION()
    void HandleFearApplied(FFearStimulus Stimulus);

    // 恐惧结束后的回调：恢复速度，并回到追逐/调查/巡逻中的合适状态。
    UFUNCTION()
    void HandleFearEnded();

    // 处理视觉刺激。看见玩家时立即追逐；丢失玩家时启动“1 秒真实坐标跟踪 + 搜查”。
    void HandleSightStimulus(AActor* Actor, const FAIStimulus& Stimulus);

    // 处理听觉刺激。若当前没有更高优先级追逐，则转入调查状态。
    void HandleHearingStimulus(AActor* Actor, const FAIStimulus& Stimulus);

    // 记录最近一次可靠的玩家位置，用于丢失视野后的继续搜查。
    void RecordLastSeenPlayerData(AActor* Actor, const FAIStimulus* Stimulus = nullptr);

    // 在多人场景下重新检查当前视野里是否还有其它玩家可追；有则立即切换目标。
    bool AcquireVisiblePlayerTarget();

    // 当当前追逐目标脱离视野时，启动短暂真实坐标跟踪并转入搜查状态。
    void StartLostSightInvestigation(AActor* LostActor);

    // 推进调查/搜查状态：包括移动到最后位置、更新时间窗内的真实坐标，以及是否切入环顾。
    void UpdateInvestigation(float DeltaTime);

    // 推进诱饵吸引状态：先移动到诱饵附近，到达后地发起倒计时。
    void UpdateLureState(float DeltaTime);
    void ClearLureState();

    // 向当前调查目标点发起一次导航移动。
    void MoveToInvestigateTarget();

    // 到达最后已知位置后，开始按“看向目标点 -> 左扫 -> 右扫 -> 回正”的顺序搜查。
    void BeginLookAround();

    // 推进分段式环顾动作；扫视结束且仍未发现目标时，回到默认逻辑。
    void UpdateLookAround(float DeltaTime);

    // 将分段式环顾推进到下一段，并更新新的朝向目标。
    void AdvanceLookAroundPhase();

    // 清空调查/搜查/环顾相关的运行时状态，但不影响 Fear 和追逐缓存。
    void ClearInvestigationState();

    // Fear 主状态机：根据当前子阶段决定是后退、逃向躲藏点，还是进入 Rage。
    void UpdateFearState(float DeltaTime, bool bShouldRepath);

    // Fear 第一阶段：短暂向恐惧源反方向慢速后退。
    void UpdateFearRetreat(float DeltaTime, bool bShouldRepath);

    // Fear 第二阶段：朝用户预设的躲藏点加速逃跑。
    void UpdateFearHideEscape(float DeltaTime, bool bShouldRepath);

    // 切入 Fear 后退阶段，并生成一个新的后退目标。
    void EnterFearRetreatState(bool bChooseNewHidePoint);

    // 切入 Fear 躲藏点逃跑阶段。
    void EnterFearHideEscapeState(bool bChooseNewHidePoint);

    // 从关卡里带指定 Tag 的点位中随机选一个当前可用的躲藏点。
    AActor* ChooseFearHidePoint() const;

    // 收集当前关卡里所有可作为躲藏点候选的 Actor。
    void GatherFearHidePointCandidates(TArray<AActor*>& OutCandidates) const;

    // 判断一个 Actor 是否带有躲藏点标签，支持 Actor Tag 和组件 Tag。
    bool HasFearHidePointTag(const AActor* Candidate) const;

    // 把躲藏点 Actor 转成真正可走的导航目标点。
    bool ResolveFearHideDestination(AActor* HidePoint, FVector& OutDestination) const;

    // 对当前躲藏点发起导航移动；失败时返回 false。
    bool MoveToFearHidePoint(bool bChooseNewHidePoint);

    // 逃向躲藏点时，检查当前目标方向是否会把鬼重新送向恐惧源。
    bool ShouldInterruptHideEscapeForFearSource(const FVector& HideDestination) const;

    // 只要还有有效 GhostHidePoint 且尚未抵达，就不允许提前结束逃点状态。
    bool HasPendingFearHideTarget() const;

    // 被恐惧源挡路时，先尝试朝侧向/斜后方短距离绕开。
    bool TryStartFearDetour(const FVector& HideDestination);

    // 清空当前绕行动作，让 AI 恢复正常逃点流程。
    void ClearFearDetour();

    // 由恐惧源计算一个短距离后退点。
    FVector GetFearRetreatLocation() const;

    // 推进怒气命中冷却，避免同一个高频恐惧源每帧都加怒气。
    void UpdateFearAnger(float DeltaTime);

    // 一次有效恐惧命中后，按“累计受惊量”规则增加怒气。
    void RegisterFearAngerStimulus(const FFearStimulus& Stimulus);

    // 脱离 Fear/Rage 后让怒气值按时间逐步下降，而不是瞬间清零。
    void UpdateFearAngerDecay(float DeltaTime);

    // 清空怒气累计，让下一轮 Fear 重新开始计数。
    void ResetFearAnger();

    // Controller 内部直接维护 Fear 数据，不再依赖单独组件。
    void ClearFear();
    bool AdvanceFear(float DeltaTime);
    bool ConsumeFearShouldRepath();
    bool IsFearActive() const;
    FVector GetFearSourceLocation() const;
    float GetFearTimeRemaining() const;
    float GetFearEscapeDistance() const;
    float GetResolvedFearDuration(const FFearStimulus& Stimulus) const;

    // 进入 Rage 状态并清空 Fear 行为。
    void EnterRageState();

    // Rage 持续期间的更新逻辑：高速度冲向可见玩家或最后已知玩家位置。
    void UpdateRageState(float DeltaTime);

    // 统一进入当前项目使用的“禁止/眩晕”状态。
    void EnterStunnedState();

    // 眩晕期间保持站桩并缓慢转圈。
    void UpdateStunnedState(float DeltaTime);

    // 眩晕结束后统一回到正常巡逻入口。
    void HandleStunEnded();

    // 清理已经失效的持续禁用来源，避免被销毁的符咒残留状态。
    void CleanupInvalidAttachedDisableSources();

    // 当前是否仍有有效的持续禁用来源。
    bool HasAttachedDisableSource() const;

    // Rage 结束后恢复正常速度和普通状态机。
    void ExitRageState(bool bInvestigateFearSource = false);

    // 进入 Rage 前缓存恐惧源，这样 Rage 无目标时也能去“拆掉”声呐/道具。
    void CacheRageSourceFromFear();

    // Rage 用的道具禁用逻辑；只要冲到恐惧源附近，就尝试让其功能失效。
    bool HasActiveRageDisableTarget() const;
    bool TryDisableRageSource();
    bool TryRetargetRageSourceFromLatestStimulus(AActor* IgnoredSourceActor = nullptr);
    void RegisterRageThreatSource(AActor* SourceActor);
    void CleanupInvalidRageThreatSources();
    void LogRageThreatState(const TCHAR* Context) const;
    void ClearRageSourceState();

    // 根据恐惧源重新发起一次逃跑路径规划。
    void MoveAwayFromFearSource();

    // 由恐惧源反推出一个远离刺激的可达位置。
    FVector GetFleeLocationFromSource(FVector SourceLocation) const;

    // 根据受惊前的基础速度，计算不同 Fear 子阶段应该使用的移动速度。
    float GetBaseWalkSpeedForState() const;
    float ResolveFearRetreatWalkSpeed() const;
    float ResolveFearHideRunWalkSpeed() const;
    float ResolveRageWalkSpeed() const;

    // 将最近一次记录的玩家位置投射到导航网格，得到搜查阶段真正要走的目标点。
    FVector GetLostSightInvestigateLocation() const;

    // 包装 MoveToActor，统一返回“是否成功提交请求”。
    bool RequestMoveToActor(AActor* GoalActor, float AcceptanceRadius);

    // 包装 MoveToLocation，统一返回“是否成功提交请求”。
    bool RequestMoveToLocation(const FVector& GoalLocation, float AcceptanceRadius);

    // 巡逻失败时做延迟重试，避免在回调里进入高频死循环。
    void SchedulePatrolRetry(float Delay = -1.f);

    // 清掉尚未执行的巡逻重试计时器。
    void ClearPatrolRetry();

    // 直接修改当前受控角色的 MaxWalkSpeed。
    void SetControlledWalkSpeed(float NewWalkSpeed);

    // Fear 结束后把移动速度还原到受惊前的值。
    void RestoreWalkSpeedAfterFear();

    // 首次受惊时缓存基础移动速度，避免后续恢复不到正确值。
    void CacheWalkSpeedIfNeeded();

    // 外部减速进入/离开时，按当前状态重新应用一次目标速度。
    void RefreshControlledWalkSpeedForCurrentState();

    // 读取“剥离当前减速乘区后”的原始速度，避免状态切换时把慢速值误当成基础值。
    float GetCurrentUnmodifiedWalkSpeed() const;

    // 清理已经失效的减速来源，避免被销毁的机关把减速残留在控制器里。
    void CleanupInvalidSlowSources();

    void CleanupInvalidSpeedReductionSources();

    // 取得当前鬼真正用于表现的 Mesh。
    // 优先走 Character Mesh；如果以后换成普通 Pawn，也会退回查找第一个 MeshComponent。
    UMeshComponent* ResolveControlledVisualMesh() const;

    // 缓存鬼原始材质槽，用于从“隐藏态覆盖材质”恢复回正常显形材质。
    void CacheOriginalGhostMaterials(UMeshComponent* VisualMesh);


    // 把当前“隐身/显形”状态真正写回材质参数。
    void ApplyGhostVisualState();

    // 把当前“隐身/显形”状态同步到角色碰撞上。
    // 隐身时穿过动态物体和拾取物，显形时恢复原本碰撞响应。
    void ApplyGhostCollisionState();

    // 推进显形倒计时，并在计时结束时恢复默认半透明状态。
    void UpdateGhostVisualState(float DeltaTime);

    // 统一设置枚举状态，便于以后扩展状态切换埋点。
    void SetAIState(EEnemyAIState NewState);

    // 生成当前状态对应的调试文本，用于头顶 DebugString 显示。
    FString GetStateDebugText() const;

    // 为当前状态选择对应的调试颜色。
    FColor GetStateDebugColor() const;
};