#pragma once

#include "CoreMinimal.h"
#include "PickupActor.h"
#include "PickupActorAAAFlashlight.generated.h"

class AActor;
class USceneComponent;
class USpotLightComponent;

// 蓝图对接说明文档
// 1. 创建 BP_Flashlight，父类选 APickupActorAAAFlashlight。
// 2. 给 MeshComponent 指定手电主体模型；FlashOriginComponent 用来表示闪光发射点，蓝图里把它挪到灯头前端并调好朝向。
// 3. FlashLightComponent 是 C++ 自动创建的聚光灯，你主要在蓝图细调 FlashOriginComponent 的位置，而不是自己再额外挂一个主灯替代它。
// 4. 如果手电还有额外装饰网格，挂到 VisualMeshRootComponent 下；这些网格会被当作纯显示部件，不参与物理碰撞。
// 5. 先调 FlashRange、FlashHalfAngleDegrees、FlashVisibleDuration，看鬼是否在正确范围内被照到并显形。
// 6. 如果关卡里有墙但鬼仍然会吃到闪光，检查 bRequireLineOfSightToAffectTargets 是否打开，以及墙体是否阻挡 FlashBlockTraceChannel。
// 7. 测试右键开关时，拿在手上关闭/打开、放下后自动闪、投掷后延迟闪，这三种情况都要分别测一次。
// 8. 如果视觉上想更亮、更蓝或更集中，优先改 FlashLightColor、FlashLightIntensity、FlashInnerConeAngleDegrees，而不是改代码。

UCLASS()
class KONGBU_API APickupActorAAAFlashlight : public APickupActor
{
    GENERATED_BODY()

public:
    APickupActorAAAFlashlight();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "Flashlight")
    void ActivateFlashlight();

    UFUNCTION(BlueprintCallable, Category = "Flashlight")
    void DeactivateFlashlight();

    UFUNCTION(BlueprintCallable, Category = "Flashlight")
    void EmitFlashPulse();

    virtual void OnPickedUp() override;
    virtual void OnPutDown(FVector PlaceLocation, FRotator PlaceRotation) override;
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce) override;
    virtual bool CanBeClosedByPlayer_Implementation() const override;
    virtual void CloseByPlayer_Implementation(AActor* ClosingActor) override;
    virtual bool IsClosedByPlayer_Implementation() const override;
    virtual void OpenByPlayer_Implementation(AActor* OpeningActor) override;
    virtual void DisableByRage_Implementation(AActor* DisablingActor) override;

protected:
    // 闪光发射原点组件。
    // 在蓝图子类里直接调整它的相对位置和朝向，就能配置闪光从哪里发出、朝哪个方向照。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* FlashOriginComponent;

    // 实际负责视觉闪光的聚光灯组件。
    // 它的开关与强度由 C++ 控制，蓝图主要通过下面这些参数改颜色、角度和亮度。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USpotLightComponent* FlashLightComponent;

    // 闪光对鬼生效的最远距离，单位厘米。
    // 只有在这个距离内的目标，才会继续做前方夹角和视线阻挡判定。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Effect", meta = (ClampMin = "50.0"))
    float FlashRange = 800.f;

    // 闪光判定的半角，单位度。
    // 例如填 28，表示最终命中范围是一个总角度约 56 度的前向锥形区域。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Effect", meta = (ClampMin = "1.0", ClampMax = "89.0"))
    float FlashHalfAngleDegrees = 28.f;

    // 两次闪光脉冲之间的时间间隔。
    // 值越小，闪得越快，对鬼施加刺激的频率也越高。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Effect", meta = (ClampMin = "0.05"))
    float FlashPulseInterval = 0.6f;

    // 单次闪光在视觉上保持亮起多久。
    // 这个值同时控制两件事：聚光灯保持点亮多久，以及被照到的鬼显形多久。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Effect", meta = (ClampMin = "0.01"))
    float FlashVisibleDuration = 0.5f;

    // 是否真的把闪光结果投递给 AI 控制器。
    // 关闭后仍然会保留灯光和调试绘制，但不会对鬼产生任何实际效果。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Effect")
    bool bAffectAIControllers = true;

    // 单次命中的闪光让鬼维持反应状态多久。
    // 这个值会写进 FFearStimulus::Duration，由 AI 控制器决定具体怎么消费。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Effect", meta = (ClampMin = "0.1"))
    float AIReactionDuration = 1.2f;

    // 闪光刺激发给 AI 时附带的标签名。
    // 方便你在控制器或行为树里区分这是“手电闪光”而不是别的道具效果。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Effect")
    FName FlashStimulusTag = TEXT("FlashlightFlash");

    // 手持状态下命中鬼时附加的怒气值。
    // 用于让鬼在玩家手里开闪光时更容易被激怒。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Effect", meta = (ClampMin = "1"))
    int32 HeldFearAngerAmount = 2;

    // 手电放置在地面或场景中时，单次命中鬼附加的怒气值。
    // 一般会比手持状态低一些，避免固定陷阱过强。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Effect", meta = (ClampMin = "1"))
    int32 PlacedFearAngerAmount = 1;

    // 是否要求闪光和目标之间必须没有遮挡。
    // 开启后，墙体、门或其他阻挡物会让鬼吃不到这次闪光效果。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Trace")
    bool bRequireLineOfSightToAffectTargets = true;

    // 视线阻挡检测所用的碰撞通道。
    // 默认用 Visibility，若项目里专门做了“鬼感知阻挡”通道，也可以在蓝图里改这里。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Trace")
    TEnumAsByte<ECollisionChannel> FlashBlockTraceChannel = ECC_Visibility;

    // 闪光灯的颜色。
    // 这个颜色同时用于聚光灯表现和调试范围绘制，方便你一眼看出当前配置。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Visual")
    FLinearColor FlashLightColor = FLinearColor(0.85f, 0.95f, 1.0f, 1.0f);

    // 闪光灯点亮时的亮度强度。
    // 值越大，单次闪光越刺眼，也更容易在场景里形成明确的灯感。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Visual", meta = (ClampMin = "0.0"))
    float FlashLightIntensity = 32000.f;

    // 聚光灯内锥角，单位度。
    // 用来控制中心最亮区域的集中程度，通常应小于或等于外层判定角度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Visual", meta = (ClampMin = "0.0"))
    float FlashInnerConeAngleDegrees = 18.f;

    // 聚光灯视觉衰减半径，单位厘米。
    // 如果你想让视觉上照得更远或更近，可以单独调这个值，而不一定要改 AI 判定距离。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Visual", meta = (ClampMin = "50.0"))
    float FlashLightAttenuationRadius = 900.f;

    // 手电被放下后是否自动开始持续闪光。
    // 打开时适合做地面诱饵，关闭时则需要玩家手动右键重新开启。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight")
    bool bAutoActivateFlashlightWhenPlaced = true;

    // 手电被扔出去后是否自动重新开始闪光。
    // 用于做“投出后吸引鬼注意”的玩法开关。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight")
    bool bAutoActivateFlashlightWhenThrown = true;

    // 投掷后延迟多久再重新开始闪光。
    // 留一点缓冲能避免刚脱手就立刻亮，手感会更像“落点附近开始闪”。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight", meta = (ClampMin = "0.0"))
    float ThrowFlashlightActivationDelay = 0.1f;

    // 是否绘制闪光作用范围和朝向调试图形。
    // 开着时会在世界里画出锥体、前向线和发射原点，方便你校准蓝图子类的朝向。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flashlight|Debug")
    bool bEnableFlashDebug = true;

    // 当前是否处于持续闪光状态。
    // 只要它为 true，Tick 就会按 FlashPulseInterval 周期触发新的闪光脉冲。
    UPROPERTY(BlueprintReadOnly, Category = "Flashlight")
    bool bFlashlightActive = false;

    // 是否被玩家手动关闭过。
    // 这个标志用于区分“暂时没亮”和“玩家明确要求保持关闭”。
    UPROPERTY(BlueprintReadOnly, Category = "Flashlight")
    bool bClosedByPlayer = true;

private:
    float FlashPulseAccumulator = 0.f;
    float FlashVisibleTimeRemaining = 0.f;
    // 玩家在手持状态下执行“打开”时，只记录开启意图，等离手后再真正恢复闪光效果。
    bool bPendingActivationWhileHeld = false;
    FTimerHandle DelayedFlashActivationHandle;

    void SyncFlashLightFromSettings();
    void UpdateFlashVisualState();
    void DrawFlashDebug();
    bool IsActorInsideFlashCone(const AActor* TargetActor) const;
    bool HasLineOfSightToTarget(const AActor* TargetActor) const;
    FVector GetFlashOriginLocation() const;
    FVector GetFlashForwardVector() const;
};