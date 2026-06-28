#pragma once

#include "CoreMinimal.h"
#include "PickupActor.h"
#include "PickupActorAAAStunSpeaker.generated.h"

// 蓝图对接说明文档
// 1. 创建 BP_StunSpeaker，父类选 APickupActorAAAStunSpeaker。
// 2. 给 MeshComponent 指定音响或警报器模型；如果有天线、灯圈等额外部件，挂到 VisualMeshRootComponent 下。
// 3. 这个物品的核心不是方向，而是半径脉冲，所以先用默认球形范围测试最直观。
// 4. 第一次调试建议开启 bEnableSpeakerDebug，进游戏后能看到范围球和动态脉冲球，方便判断半径是否过大或过小。
// 5. 如果鬼没有被眩晕，先检查 StunRadius 和 AIStunDuration 是否太小，再检查 bAffectAIControllers 是否被关掉。
// 6. 如果隔墙不该生效，就把 bRequireLineOfSightToAffectTargets 打开，并确认场景阻挡体会挡住 SpeakerBlockTraceChannel。
// 7. 放下自动开、投掷自动开分别都可以在蓝图里调；新手测试建议先全部打开，这样最容易看出效果链路是否通了。

UCLASS()
class KONGBU_API APickupActorAAAStunSpeaker : public APickupActor
{
    GENERATED_BODY()

public:
    APickupActorAAAStunSpeaker();

    // Tick 负责推进调试动画和脉冲累积器；到时间后发出下一次眩晕脉冲。
    virtual void Tick(float DeltaTime) override;

    // 手动开启音响脉冲。
    UFUNCTION(BlueprintCallable, Category = "Speaker")
    void ActivateSpeaker();

    // 手动关闭音响脉冲。
    UFUNCTION(BlueprintCallable, Category = "Speaker")
    void DeactivateSpeaker();

    // 立刻发出一次眩晕脉冲。
    UFUNCTION(BlueprintCallable, Category = "Speaker")
    void EmitStunPulse();

    virtual void OnPickedUp() override;
    virtual void OnPutDown(FVector PlaceLocation, FRotator PlaceRotation) override;
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce) override;
    virtual bool CanBeClosedByPlayer_Implementation() const override;
    virtual void CloseByPlayer_Implementation(AActor* ClosingActor) override;
    virtual bool IsClosedByPlayer_Implementation() const override;
    virtual void OpenByPlayer_Implementation(AActor* OpeningActor) override;
    virtual void DisableByRage_Implementation(AActor* DisablingActor) override;

protected:
    // 音响脉冲生效半径，单位厘米。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker|Stun", meta = (ClampMin = "50.0"))
    float StunRadius = 450.f;

    // 两次脉冲之间的时间间隔，类似手电的闪光频率。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker|Stun", meta = (ClampMin = "0.05"))
    float StunPulseInterval = 0.4f;

    // 单次命中后，鬼维持眩晕多久。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker|Stun", meta = (ClampMin = "0.1"))
    float AIStunDuration = 0.9f;

    // 音响放在地面工作时，每次脉冲给 Rage 累积多少怒气。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker|Stun", meta = (ClampMin = "1"))
    int32 PlacedFearAngerAmount = 3;

    // 玩家手持音响时，每次脉冲额外给 Rage 累积多少怒气。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker|Stun", meta = (ClampMin = "1"))
    int32 HeldFearAngerAmount = 3;

    // 音响刺激写给 AI 的标签名，便于区分这是“眩晕音响”而不是其它恐惧源。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker|Stun")
    FName SpeakerStimulusTag = TEXT("StunSpeakerPulse");

    // 是否把脉冲真正投递给 AI 控制器。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker|Stun")
    bool bAffectAIControllers = true;

    // 是否要求音响与目标之间必须没有遮挡。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker|Trace")
    bool bRequireLineOfSightToAffectTargets = false;

    // 视线阻挡检测使用的碰撞通道。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker|Trace")
    TEnumAsByte<ECollisionChannel> SpeakerBlockTraceChannel = ECC_Visibility;

    // 调试绘制颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker|Debug")
    FColor SpeakerDebugColor = FColor(120, 210, 255);

    // 是否显示音响范围与脉冲调试图。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker|Debug")
    bool bEnableSpeakerDebug = true;

    // 放下后是否自动开始脉冲。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker")
    bool bAutoActivateSpeakerWhenPlaced = true;

    // 投掷后是否自动开始脉冲。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker")
    bool bAutoActivateSpeakerWhenThrown = true;

    // 投掷后延迟多久再重新开始脉冲。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speaker", meta = (ClampMin = "0.0"))
    float ThrowSpeakerActivationDelay = 0.1f;

    // 当前是否处于持续脉冲状态。
    UPROPERTY(BlueprintReadOnly, Category = "Speaker")
    bool bSpeakerActive = false;

    // 是否被玩家手动关闭过。
    UPROPERTY(BlueprintReadOnly, Category = "Speaker")
    bool bClosedByPlayer = false;

private:
    float PulseVisualTime = 0.f;
    float PulseAccumulator = 0.f;
    FTimerHandle DelayedSpeakerActivationHandle;

    void DrawSpeakerDebug();
    bool IsActorInsideStunRadius(const AActor* TargetActor) const;
    bool HasLineOfSightToTarget(const AActor* TargetActor) const;
};