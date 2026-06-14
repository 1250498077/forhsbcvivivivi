// PickupActorAAAItemX.h
#pragma once

#include "CoreMinimal.h"
#include "PickupActor.h"
#include "PickupActorAAAItemX.generated.h"

// 蓝图对接说明文档
// 1. 创建 BP_ItemX，父类选 APickupActorAAAItemX。
// 2. 给 MeshComponent 指定你的道具模型；如果你有天线、显示器、喇叭头等额外零件，挂到 VisualMeshRootComponent 下。
// 3. 这个物品的核心是“前方扇形声波”，所以蓝图里最重要的是把模型朝向摆对：道具正前方必须和你希望的声波方向一致。
// 4. 第一次调试时建议开启 bEnableSonarDebug，这样进游戏能直接看到锥形范围，确认是不是朝前发射。
// 5. 如果鬼隔墙也能被影响，检查 bRequireLineOfSightToAffectTargets 和 SonarBlockTraceChannel；同时确认墙体真的阻挡这个通道。
// 6. 放下自动开和投掷自动开分别由 bAutoActivateSonarWhenPlaced、bAutoActivateSonarWhenThrown 控制，新手先都开着最容易看出效果。
// 7. 右键是开/关 ItemX，不是画符；如果你之后把这个物品和画符仪器一起测试，记得它们的右键行为不同。
// 8. 测试顺序建议：先确认调试锥体朝向正确，再确认鬼进入扇形后会响应，最后再微调怒气值和脉冲频率。

UCLASS()
class KONGBU_API APickupActorAAAItemX : public APickupActor
{
    GENERATED_BODY()

public:
    APickupActorAAAItemX();

    // Tick 只负责两件事：推进调试动画时间，以及按固定间隔触发下一次真正的声波脉冲。
    virtual void Tick(float DeltaTime) override;

    // 手动开启声波。
    // 这个入口会重置内部计时器、清掉投掷后的延迟激活，并立即发出第一发脉冲，
    // 因此它不仅是“切开关”，也是一次完整的重新开始发声流程。
    UFUNCTION(BlueprintCallable, Category = "Sonar")
    void ActivateSonar();

    // 手动关闭声波。
    // 关闭时会停止后续自动脉冲，并清理任何尚未触发的延迟激活定时器。
    UFUNCTION(BlueprintCallable, Category = "Sonar")
    void DeactivateSonar();

    // 立刻发出一次声波脉冲。
    // 该函数只负责“这次脉冲影响哪些 AI”，不会自动修改开关状态；
    // 正常情况下它由 ActivateSonar 和 Tick 定时调用。
    UFUNCTION(BlueprintCallable, Category = "Sonar")
    void EmitSonarPulse();

    // 覆盖父类的交互生命周期接口，在玩家拿起、放下、投掷或开关 ItemX 时同步声波状态。
    virtual void OnPickedUp() override;
    virtual void OnPutDown(FVector PlaceLocation, FRotator PlaceRotation) override;
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce) override;
    virtual bool CanBeClosedByPlayer_Implementation() const override;
    virtual void CloseByPlayer_Implementation(AActor* ClosingActor) override;
    virtual bool IsClosedByPlayer_Implementation() const override;
    virtual void OpenByPlayer_Implementation(AActor* OpeningActor) override;
    virtual void DisableByRage_Implementation(AActor* DisablingActor) override;

protected:
    // ItemX 的玩法核心不是额外 Mesh 逻辑，而是“被放置后持续朝前方发出扇形声波”。
    // 因此这里主要暴露的都是声波判定、AI 影响和调试可视化参数。

    // 声波覆盖半径，单位是厘米。
    // 只有目标与 ItemX 的水平距离不超过这个值，才有资格进入下一步夹角判定。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
    float SonarRadius = 500.f;

    // 声波正前方单侧夹角，单位是度。
    // 例如 45 表示最终覆盖的是一个前向 90 度扇形，而不是 45 度总角度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
    float SonarAngle = 45.f;

    // 声波多久发一次脉冲。做抓鬼/诱导玩法时，这个值决定 AI 多频繁被影响。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar", meta = (ClampMin = "0.1"))
    float SonarPulseInterval = 0.1f;

    // 声波调试显示颜色，只影响可视化，不影响实际命中范围。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
    FColor SonarColor = FColor::Cyan;

    // 是否显示声波调试图形。调试玩法范围时建议开启，正式关卡可关闭。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
    bool bEnableSonarDebug = true;

    // 是否真的把声波结果投递给 AI。
    // 关闭后，ItemX 仍然可以播放调试图形和维持发声节奏，但不会对任何 MyAIController 生效。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Fear")
    bool bAffectAIControllers = true;

    // 一次命中的声波让 AI 保持恐惧/反应状态多久。
    // 这个值会写入 FFearStimulus::Duration，由控制器决定如何消费它。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Fear", meta = (ClampMin = "0.1"))
    float AIReactionDuration = 2.5f;

    // 开启后，墙后面的鬼不会吃到声波。
    // 蓝图配置点：墙体/门/大障碍物需要阻挡下面这个 Trace Channel，默认是 Visibility。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Trace")
    bool bRequireLineOfSightToAffectTargets = true;

    // 视线检测使用的碰撞通道。通常保持 Visibility，除非你单独做了自定义通道。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Trace")
    TEnumAsByte<ECollisionChannel> SonarBlockTraceChannel = ECC_Visibility;

    // 声波事件标签。AI 收到刺激后可以靠这个名字区分是 ItemX 造成的反应。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Fear")
    FName SonarStimulusTag = TEXT("ItemXSonar");

    // ItemX 放在地上工作时，每次脉冲给 Rage 累积的基础怒气。
    // 这是“有效命中一次 AI”时追加到刺激里的数值，不是每帧都加。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Fear", meta = (ClampMin = "1"))
    int32 PlacedFearAngerAmount = 1;

    // 玩家手持时更容易激怒鬼，但只影响 Rage 累积，不改 Fear 主流程。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar|Fear", meta = (ClampMin = "1"))
    int32 HeldFearAngerAmount = 3;

    // 放下物品后是否自动开始发声。一般做陷阱/诱饵建议开着。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
    bool bAutoActivateSonarWhenPlaced = true;

    // 扔出去后是否自动重新发声。一般做“分散鬼注意力”建议开着。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
    bool bAutoActivateSonarWhenThrown = true;

    // 投掷落地后等待多久再重新发声。
    // 留一点延迟可以避免物体刚离手时立刻刺激 AI，让手感更像“扔出后开始诱导”。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar", meta = (ClampMin = "0.0"))
    float ThrowSonarActivationDelay = 0.15f;

    // 当前是否处于持续发声状态。
    // 只要它为 true，Tick 就会继续累计时间并周期性调用 EmitSonarPulse。
    UPROPERTY(BlueprintReadOnly, Category = "Sonar")
    bool bSonarActive = false;

    // 是否被玩家手动关闭过。
    // 这个标志用于区分“临时没在发声”和“玩家明确要求它保持关闭”。
    UPROPERTY(BlueprintReadOnly, Category = "Sonar")
    bool bClosedByPlayer = false;

private:
    // 仅用于调试动画的累计时间，让可视化锥体和波纹表现出持续脉冲感。
    float SonarPulseTime = 0.f;

    // 真正控制发射频率的累计器；到达 SonarPulseInterval 后会归零并触发一次脉冲。
    float SonarPulseAccumulator = 0.f;

    // 投掷后延迟重新激活声波时使用的定时器句柄。
    FTimerHandle DelayedSonarActivationHandle;

    // 调试绘制、锥体检测和可见性判断都拆成独立函数，便于以后复用或替换实现。
    void DrawSonarDebug();

    // 判断目标是否落在声波扇形内。
    // 这里只看水平面距离和朝向，不考虑 Z 轴高低差。
    bool IsActorInsideSonarCone(const AActor* TargetActor) const;

    // 判断 ItemX 与目标之间是否存在可通视路径。
    // 开启视线要求后，只有没有被指定碰撞通道阻挡的目标才会吃到声波。
    bool HasLineOfSightToTarget(const AActor* TargetActor) const;
};