#pragma once

#include "CoreMinimal.h"
#include "PickupActor.h"
#include "PickupActorAAASlowTalisman.generated.h"

class AMyAIController;
class APawn;
class AActor;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UPrimitiveComponent;
class USphereComponent;
struct FHitResult;

// 蓝图对接说明文档
// 1. 创建 BP_SlowTalisman，父类选 APickupActorAAASlowTalisman。
// 2. 给 MeshComponent 指定符纸模型；如果模型特别薄，建议在静态网格资源里把碰撞做好，不然投掷命中会不稳定。
// 3. SlowTriggerComponent 和 GlowLightComponent 都是 C++ 已经创建好的组件；你在蓝图里主要做位置、半径观感和材质表现的细调。
// 4. 这个物品有两种关键结果：落地激活减速范围，或者贴到鬼身上触发减速，所以测试时这两条路径都要分别验证。
// 5. 如果你想让它像贴符一样更容易粘到鬼，先调 ItemThrowForceMultiplier 和 GhostAttachHorizontalRadiusRatio，再看是否需要改准星吸附距离。
// 6. 如果你想让它看起来“激活时发光”，在蓝图里给主体材质做可见的发光参数；C++ 会更新材质和 GlowLightComponent，但最终好不好看还是蓝图美术资源决定。
// 7. 新手建议的测试流程：先做“扔到地上后激活减速”，确认范围和发光正常，再做“投中鬼后附着”的第二轮测试。
// 8. 如果你发现它拿在手里就自己生效，先检查是不是在蓝图里又额外开了碰撞或 Overlap，正常情况下手持时不会触发地面减速区。

UCLASS()
class KONGBU_API APickupActorAAASlowTalisman : public APickupActor
{
    GENERATED_BODY()

public:
    APickupActorAAASlowTalisman();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;

    virtual void OnPickedUp() override;
    virtual void OnPutDown(FVector PlaceLocation, FRotator PlaceRotation) override;
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce) override;
    virtual bool CanBeClosedByPlayer_Implementation() const override;
    virtual void CloseByPlayer_Implementation(AActor* ClosingActor) override;
    virtual bool IsClosedByPlayer_Implementation() const override;
    virtual void OpenByPlayer_Implementation(AActor* OpeningActor) override;

    void SetPreferredStickTarget(const FHitResult& HitResult);
    void ClearPreferredStickTarget();

    bool TryAttachToGhostZone(APawn* TargetPawn, AMyAIController* TargetController, USceneComponent* AttachComponent);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* SlowTriggerComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UPointLightComponent* GlowLightComponent;

    // 慢符本体质量，会影响投掷手感和落地碰撞反馈。
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Throw", meta = (ClampMin = "0.01"))
    // float TalismanMassKg = 1.25f;

    // 慢符的额外投掷力度倍率。可以单独把符扔得更远或更近。
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Throw", meta = (ClampMin = "0.0"))
    // float ThrowForceMultiplier = 1.0f;

    // 激活后对鬼生效的范围，单位厘米。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Slow", meta = (ClampMin = "10.0"))
    float EffectRadius = 300.f;

    // 从当前状态原始速度中直接扣掉多少速度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Slow", meta = (ClampMin = "0.0"))
    float SpeedReductionAmount = 120.f;

    // 减速效果持续多久，超过后鬼会恢复原速。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Slow", meta = (ClampMin = "0.1"))
    float SlowDuration = 5.f;

    // 激活后符纸发光的颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Visual")
    FLinearColor ActivationGlowColor = FLinearColor(1.0f, 0.82f, 0.2f, 1.0f);

    // 激活后材质自发光强度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Visual", meta = (ClampMin = "0.0"))
    float ActivationGlowIntensity = 10.f;

    // 激活后点光源亮度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Visual", meta = (ClampMin = "0.0"))
    float ActivationLightIntensity = 3500.f;

    // 激活后点光源影响半径。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Visual", meta = (ClampMin = "50.0"))
    float ActivationLightRadius = 220.f;

    // 贴到鬼身上时，水平方向允许附着的身体半径比例。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Attach", meta = (ClampMin = "0.1", ClampMax = "1.0"))
    float GhostAttachHorizontalRadiusRatio = 0.65f;

    // 贴鬼时允许附着的最低高度比例，避免总贴到脚边。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Attach", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GhostAttachVerticalMinRatio = 0.2f;

    // 贴鬼时允许附着的最高高度比例，避免总贴到头顶外面。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Attach", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GhostAttachVerticalMaxRatio = 0.85f;

    // 附着到表面时额外向外偏移多少，避免穿模。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Attach", meta = (ClampMin = "0.0"))
    float GhostAttachSurfacePadding = 1.f;

    // 手持时纸符前后摆动的俯仰角度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Paper", meta = (ClampMin = "0.0"))
    float HeldFlutterPitchAngle = 6.f;

    // 手持时纸符左右摆动的滚转角度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Paper", meta = (ClampMin = "0.0"))
    float HeldFlutterRollAngle = 10.f;

    // 手持时纸符摆动速度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Paper", meta = (ClampMin = "0.0"))
    float HeldFlutterSpeed = 0.f;

    // 飞行速度超过这个值后，才会启用飞行飘动效果。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Paper", meta = (ClampMin = "0.0"))
    float FlightFlutterMinSpeed = 220.f;

    // 飞行时模拟纸符上扬的升力。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Paper", meta = (ClampMin = "0.0"))
    float FlightFlutterLiftForce = 240.f;

    // 飞行时模拟纸符翻转的扭矩大小。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Paper", meta = (ClampMin = "0.0"))
    float FlightFlutterTorque = 16000.f;

    // 飞行飘动频率，越大抖动越快。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talisman|Paper", meta = (ClampMin = "0.0"))
    float FlightFlutterFrequency = 0.f;

private:
    bool bTalismanActive = false;
    bool bClosedByPlayer = true;
    bool bAwaitingThrowImpact = false;
    bool bHasConsumedTarget = false;
    bool bPendingGroundActivation = false;
    bool bHasPreferredStickTarget = false;

    TWeakObjectPtr<AMyAIController> AttachedController;
    TWeakObjectPtr<AActor> AttachedTargetActor;
    TWeakObjectPtr<AActor> PreferredStickActor;
    TWeakObjectPtr<UPrimitiveComponent> PreferredStickComponent;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;
    FTimerHandle SlowDurationHandle;
    float HeldFlutterTime = 0.f;
    float FlightFlutterTime = 0.f;
    FVector PreferredStickImpactPoint = FVector::ZeroVector;
    FVector PreferredStickImpactNormal = FVector::ZeroVector;

    UFUNCTION()
    void HandleTalismanHit(
        UPrimitiveComponent* HitComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        FVector NormalImpulse,
        const FHitResult& Hit);

    UFUNCTION()
    void HandleSlowTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    void ActivateTalisman();
    void DeactivateTalisman();
    void UpdateSlowTriggerState();
    void UpdateSlowTriggerRadius();
    void ApplyThrowablePhysicsTuning();
    void CacheOriginalMaterials();
    void UpdateActivationVisualState();
    void RestoreDefaultThrowableCollision();
    void StickToImpact(const FHitResult& Hit, UPrimitiveComponent* HitComponent);
    void ApplyRandomGhostAttachPose(APawn* TargetPawn, USceneComponent* AttachComponent);
    void TryAttachToOverlappingAI(AActor* OtherActor);
    // void AttachToAITarget(APawn* TargetPawn, AMyAIController* TargetController);
    void AttachToAITarget(APawn* TargetPawn, AMyAIController* TargetController, USceneComponent* OverrideAttachComponent = nullptr);
    void ExpireAttachedSlow();
    void ClearAttachedSlowEffect();
};