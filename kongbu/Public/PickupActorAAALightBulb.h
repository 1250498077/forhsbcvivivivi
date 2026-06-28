#pragma once

#include "CoreMinimal.h"
#include "PickupActor.h"
#include "PickupActorAAALightBulb.generated.h"

class APawn;
class UPointLightComponent;
class USceneComponent;

// 蓝图对接说明文档
// 1. 创建 BP_LightBulb，父类选 APickupActorAAALightBulb。
// 2. 给 MeshComponent 指定灯泡或灯具模型；LightOriginComponent 用来表示发光位置，蓝图里把它挪到灯泡真正发亮的地方。
// 3. BulbLightComponent 已经在 C++ 里创建好，你通常不需要再新建一个主点光源替代它；除非是额外装饰光。
// 4. 这个物品的玩法是“鬼越近越亮”，所以第一次测试时建议让鬼从远到近走过来，观察亮度是否平滑变化。
// 5. 如果灯总是太暗或太亮，优先调 MaxLightIntensity、EffectRadius、FullBrightnessDistance，不要先怀疑逻辑出错。
// 6. 如果你希望放下后自动工作，保持 bAutoActivateLightBulbWhenPlaced 为 true；如果想做成需要手动开关的版本，就改成 false 并扩展交互。
// 7. 调试时可以暂时打开 bEnableLightBulbDebug，看影响半径和最近鬼距离显示是否符合预期。

UCLASS()
class KONGBU_API APickupActorAAALightBulb : public APickupActor
{
    GENERATED_BODY()

public:
    APickupActorAAALightBulb();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "LightBulb")
    void ActivateLightBulb();

    UFUNCTION(BlueprintCallable, Category = "LightBulb")
    void DeactivateLightBulb();

    virtual void OnPickedUp() override;
    virtual void OnPutDown(FVector PlaceLocation, FRotator PlaceRotation) override;
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce) override;
    virtual void DisableByRage_Implementation(AActor* DisablingActor) override;

protected:
    // 用于在蓝图子类里微调灯泡发光位置。
    // 直接改它的相对位置/朝向，就能把点光源挪到灯泡头部、灯罩内部等更合理的位置。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* LightOriginComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UPointLightComponent* BulbLightComponent;

    // 鬼进入这个半径后，灯泡会开始根据距离逐渐变亮。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightBulb|Effect", meta = (ClampMin = "10.0"))
    float EffectRadius = 320.f;

    // 进入这个距离后视为满亮。
    // 这样鬼贴得更近时不会继续无限抬高亮度，而是稳定保持在最大亮度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightBulb|Effect", meta = (ClampMin = "0.0"))
    float FullBrightnessDistance = 80.f;

    // 鬼靠近时的亮起速度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightBulb|Effect", meta = (ClampMin = "0.0"))
    float BrightenInterpSpeed = 4.5f;

    // 鬼离开后的熄灭速度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightBulb|Effect", meta = (ClampMin = "0.0"))
    float DimInterpSpeed = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightBulb|Visual")
    FLinearColor LightColor = FLinearColor(1.0f, 0.92f, 0.72f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightBulb|Visual", meta = (ClampMin = "0.0"))
    float MaxLightIntensity = 4200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightBulb|Visual", meta = (ClampMin = "50.0"))
    float LightAttenuationRadius = 300.f;

    // 放下后是否自动恢复感应发光。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightBulb")
    bool bAutoActivateLightBulbWhenPlaced = true;

    // 扔出去后是否自动恢复感应发光。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightBulb")
    bool bAutoActivateLightBulbWhenThrown = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightBulb", meta = (ClampMin = "0.0"))
    float ThrowLightBulbActivationDelay = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LightBulb|Debug")
    bool bEnableLightBulbDebug = false;

    UPROPERTY(BlueprintReadOnly, Category = "LightBulb")
    bool bLightBulbActive = true;

    UPROPERTY(BlueprintReadOnly, Category = "LightBulb")
    bool bClosedByPlayer = false;

private:
    float CurrentBrightnessAlpha = 0.f;
    FTimerHandle DelayedLightBulbActivationHandle;

    void SyncLightFromSettings();
    void UpdateLightVisualState();
    float ComputeTargetBrightnessAlpha(const APawn*& OutNearestGhost, float& OutNearestDistance) const;
    void DrawLightBulbDebug(const APawn* NearestGhost, float NearestDistance) const;
    FVector GetLightOriginLocation() const;
};