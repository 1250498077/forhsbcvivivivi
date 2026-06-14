#pragma once

#include "CoreMinimal.h"
#include "PickupActor.h"
#include "PickupActorAAAGlassBottle.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class UStaticMesh;
struct FHitResult;

// APickupActorAAAGlassBottle
// 玻璃瓶拾取物类：继承自 APickupActor。
// 功能概述：
// - 支持被玩家拾取、放下和投掷；投掷时可施加自旋与力度。
// - 在合适碰撞（满足 Arm 延迟且速度/冲量足够）时会碎裂为碎片。
// - 外观和碎片行为均可在编辑器调整（材质、颜色、碎片数量、冲量等）。
// 蓝图对接说明文档
// 1. 创建 BP_GlassBottle，父类选 APickupActorAAAGlassBottle。
// 2. 给 MeshComponent 指定玻璃瓶模型；如果瓶子碰撞形状不合适，优先在静态网格资源里调碰撞，而不是在这里额外乱加碰撞组件。
// 3. 如果你的瓶子材质本身就是玻璃，可以把 GlassMaterialOverride 留空先测试；若想强制统一玻璃外观，再指定自定义材质。
// 4. 先只调三个参数做第一轮测试：BottleMassKg、ThrowForceMultiplier、ShatterArmDelay，确认投掷手感和落地后是否会碎。
// 5. 如果碎片太夸张，就降低 ShardCount 和 ShardBurstImpulse；如果碎片太少不像碎掉，就提高这两个值。
// 6. 如果美术上想要特定碎片形状，给 ShardMeshOverride 指定一个小碎片网格；不指定时会使用默认立方体碎片。
// 7. 新手测试步骤建议：拿起 -> 左键投掷 -> 观察是否碎裂 -> 调整质量和碎片数量 -> 再做第二轮测试。

UCLASS()
class KONGBU_API APickupActorAAAGlassBottle : public APickupActor
{
    GENERATED_BODY()

public:
    // 构造函数：初始化默认资源和标签（Bottle / Pickup / Breakable / Glass）。
    APickupActorAAAGlassBottle();

    // 生命周期钩子：用于运行时/构造时应用材质与物理参数。
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // 玩家交互相关：被玩家拿起、放下或投掷时被调用。
    virtual void OnPickedUp() override;
    virtual void OnPutDown(FVector PlaceLocation, FRotator PlaceRotation) override;
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce) override;

protected:
    // 瓶子物理质量（千克）。在构造和 BeginPlay 时会通过 MeshComponent 应用。
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bottle|Physics", meta = (ClampMin = "0.01"))
    // float BottleMassKg = 1.0f;

    // 可在编辑器替换的材质（优先），如果为空会使用默认内置材质。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bottle|Visual")
    TObjectPtr<UMaterialInterface> GlassMaterialOverride = nullptr;

    // 用于动态材质的颜色参数（R,G,B,A）。A 分量可作为不透明度候选项。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bottle|Visual")
    FLinearColor GlassTint = FLinearColor(0.72f, 0.9f, 1.0f, 1.0f);

    // 玻璃的不透明度控制（0.02 - 1.0），会同时写入若干常见材质参数名以保证兼容性。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bottle|Visual", meta = (ClampMin = "0.02", ClampMax = "1.0"))
    float GlassOpacity = 0.18f;

    // 投掷时自旋速率（角速度，单位：度/秒）。
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bottle|Throw", meta = (ClampMin = "0.0"))
    // float ThrowSpinRateDegrees = 2400.f;

    // // 自旋轴的偏向量，投掷时会在此基础上加入随机扰动以避免完全相同的旋转。
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bottle|Throw")
    // FVector ThrowSpinAxisBias = FVector(0.35f, 1.0f, 0.2f);

    // // 瓶子本地的投掷力度倍率：最终投掷冲量 = PlayerController.ThrowForce * ThrowForceMultiplier。
    // // 这样可以单独调节玻璃瓶的投掷距离而不影响其他可拾取物。
    // UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bottle|Throw", meta = (ClampMin = "0.0"))
    // float ThrowForceMultiplier = 1.0f;

    // 投掷后延迟允许碎裂的时间窗口（秒），避免刚投掷即碎。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bottle|Shatter", meta = (ClampMin = "0.0"))
    float ShatterArmDelay = 0.06f;

    // 瓶子碎裂后生成多少片碎片。越多越像真碎，但性能开销也更高。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bottle|Shatter", meta = (ClampMin = "1", ClampMax = "32"))
    int32 ShardCount = 12;

    // 自定义碎片网格。不填就走代码里的默认碎片模型。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bottle|Shatter")
    TObjectPtr<UStaticMesh> ShardMeshOverride = nullptr;

    // 单个碎片最小尺寸范围，用来控制碎片不要全都一样大。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bottle|Shatter")
    FVector ShardBoxExtentMin = FVector(1.5f, 0.15f, 2.5f);

    // 单个碎片最大尺寸范围。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bottle|Shatter")
    FVector ShardBoxExtentMax = FVector(4.0f, 0.45f, 8.0f);

    // 碎片初始爆散冲量强度
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bottle|Shatter", meta = (ClampMin = "0.0"))
    float ShardBurstImpulse = 220.f;

    // 碎片冲量中继承瓶子投掷速度的比例
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bottle|Shatter", meta = (ClampMin = "0.0"))
    float ImpactVelocityInheritance = 0.25f;

    // 每个碎片的生存时长（秒）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bottle|Shatter", meta = (ClampMin = "0.2"))
    float ShardLifetime = 6.f;

private:
    // 运行时缓存：默认材质和默认网格（当 override 未提供时使用）。
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> DefaultGlassMaterial = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> DefaultShardMesh = nullptr;

    // 运行时材质实例（动态参数写入使用）
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> GlassMaterialInstance = nullptr;

    // 用于碎裂延迟的定时器句柄
    FTimerHandle ShatterArmHandle;

    // 状态位：是否已允许撞击碎裂 / 是否已经碎裂
    bool bCanShatterOnImpact = false;
    bool bHasShattered = false;

    // 瓶子碰撞回调（注册到 MeshComponent->OnComponentHit）
    UFUNCTION()
    void HandleBottleHit(
        UPrimitiveComponent* HitComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        FVector NormalImpulse,
        const FHitResult& Hit);

    // 内部工具函数
    //  - ApplyBottlePhysicsTuning(): 应用基类物理参数并调整质量、阻尼等以适合玻璃瓶特性
    //  - ApplyGlassMaterial(): 创建/设置动态材质参数
    //  - Arm/Disarm 碎裂判断
    //  - ShatterBottle / SpawnGlassShards: 实现碎裂与碎片生成
    void ApplyBottlePhysicsTuning();
    void ApplyGlassMaterial();
    void ArmBottleForImpactShatter();
    void DisarmBottleShatter();
    void ShatterBottle(const FHitResult& Hit, const FVector& NormalImpulse);
    void SpawnGlassShards(const FVector& ImpactPoint, const FVector& ImpactNormal, const FVector& BottleVelocity);

    // 解析使用的材质/网格（优先使用 Override）
    UMaterialInterface* ResolveGlassMaterial() const;
    UStaticMesh* ResolveShardMesh() const;
};