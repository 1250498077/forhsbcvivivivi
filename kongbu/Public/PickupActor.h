#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PickupActor.generated.h"


class USceneComponent;
class UStaticMeshComponent;
class FLifetimeProperty;
class UPhysicalMaterial;
class UAnimMontage;
class UAnimSequenceBase;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EHoldItemType : uint8
{
    None UMETA(DisplayName = "None"),
    Flashlight UMETA(DisplayName = "Flashlight"),
    Book UMETA(DisplayName = "Book"),
    Cross UMETA(DisplayName = "Cross"),
    Bottle UMETA(DisplayName = "Bottle"),
    LightBulb UMETA(DisplayName = "Light Bulb"),
    Brick UMETA(DisplayName = "Brick"),
    ItemX UMETA(DisplayName = "Item X"),
    Talisman UMETA(DisplayName = "Talisman"),
    Speaker UMETA(DisplayName = "Speaker"),
    GhostLure UMETA(DisplayName = "Ghost Lure"),
    RuneInstrument UMETA(DisplayName = "Rune Instrument"),
    RuneGridInstrument UMETA(DisplayName = "Rune Grid Instrument")
};


// 结构体定义
USTRUCT(BlueprintType)
struct FPickupHeldAnimationSet
{
    GENERATED_BODY()

    // --- Locomotion Animations (移动动画) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|Locomotion")
    TObjectPtr<UAnimSequenceBase> IdlePose = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|Locomotion")
    TObjectPtr<UAnimSequenceBase> WalkAnimation = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|Locomotion")
    TObjectPtr<UAnimSequenceBase> RunAnimation = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|Locomotion")
    TObjectPtr<UAnimSequenceBase> CrouchIdleAnimation = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|Locomotion")
    TObjectPtr<UAnimSequenceBase> CrouchWalkAnimation = nullptr;

    // --- Throw Animations (投掷动画) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|Throw")
    TObjectPtr<UAnimSequenceBase> StandThrowUpperBodyAnimation = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|Throw")
    TObjectPtr<UAnimSequenceBase> RunThrowUpperBodyAnimation = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|Throw")
    TObjectPtr<UAnimSequenceBase> SquatThrowUpperBodyAnimation = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|Throw")
    bool bFallbackToDefaultThrowAnimationWhenUnset = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|Throw")
    TObjectPtr<UAnimMontage> StandThrowMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|Throw")
    TObjectPtr<UAnimMontage> RunThrowMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|Throw")
    TObjectPtr<UAnimMontage> SquatThrowMontage = nullptr;

    // --- Action Animations (动作动画) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|Action")
    TObjectPtr<UAnimMontage> EquipMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|Action")
    TObjectPtr<UAnimMontage> UseMontage = nullptr;
};

UCLASS()
class KONGBU_API APickupActor : public AActor
{
    GENERATED_BODY()

public:
    APickupActor();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    bool TryInterruptGhostsSoulSuckOnHit(AActor* otherActor, const FHitResult& Hit, const FVector& NormalImpulse);

    // 物体本体网格。拾取、放下和投掷都会围绕它切换物理与碰撞状态。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComponent;


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Ghost")
    bool bCanInterruptGhostsSoulSuckOnHit = true;

    // 需要多个外观部件时，把额外 StaticMeshComponent 挂到这个节点下面。
    // 基类会统一把这些附加网格限制为“只显示，不参与物理和碰撞”。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* VisualMeshRootComponent;

    // 放到世界里后是否阻挡 Pawn 通道。
    // 目前玩家和鬼默认都走 Pawn，所以关掉后两者都不会再把这个物体顶走。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Collision")
    bool bAllowPawnCollision = false;

    // 放到世界里后是否和其他 PhysicsBody 物体发生刚体碰撞。
    // 关掉后，像其他可拾取物、碎片等物理体就不会把它撞翻。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Collision")
    bool bAllowPhysicsBodyCollision = false;

    // 统一物理质量，单位千克。会影响投掷后的速度、碰撞反馈和阻性。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Physics", meta = (ClampMin = "0.01"))
    float ItemMassKg = 1.0f;

    // 投掷力度倍率。最终力度 = PlayerController.ThrowForce * ItemThrowForceMultiplier。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Physics", meta = (ClampMin = "0.0"))
    float ItemThrowForceMultiplier = 1.0f;

    // 线性阻尼。越大，物品飞行/滑动越快停下来。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Physics", meta = (ClampMin = "0.0"))
    float ItemLinearDamping = 0.08f;

    // 角阻尼。越大，物品旋转越快停止下来。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Physics", meta = (ClampMin = "0.0"))
    float ItemAngularDamping = 0.25f;

    // 物理材质。摩擦力、弹性等建议在 Physical Material 资源里调，然后指定到这里。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Physics")
    TObjectPtr<UPhysicalMaterial> ItemPhysicalMaterial = nullptr;

    // 投掷时是否给物品额外自旋。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Physics|Throw")
    bool bApplyThrowSpin = true;

    // 投掷时自旋速率，单位：度/秒。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Physics|Throw", meta = (ClampMin = "0.0"))
    float ItemThrowSpinRateDegrees = 900.f;

    // 投掷自旋轴偏向量。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Physics|Throw")
    FVector ItemThrowSpinAxisBias = FVector(0.35f, 1.0f, 0.2f);

    // 是否给自旋轴加入少量随机化扰动。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Physics|Throw")
    bool bRandomizeThrowSpinAxis = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_IsHeldByPlayer, Category = "Pickup")
    bool bIsHeldByPlayer = false;

    // 当前物品的持有类型。动画蓝图可根据它切换到第一人称/第三人称持有姿势。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Hold")
    EHoldItemType HoldType = EHoldItemType::None;

    // 手持这个道具时是否允许玩家冲刺。需要限制特定道具时，在对应道具蓝图里关闭即可。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Hold|Restrictions")
    bool bAllowSprintWhileHeld = true;

    // 手持这个道具时是否允许玩家投掷。需要限制特定道具时，在对应道具蓝图里关闭即可。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Hold|Restrictions")
    bool bAllowThrowWhileHeld = true;

    // 是否为这个道具单独覆盖“按下投掷后多久真正飞出”的时间。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Throw")
    bool bOverrideThrowReleaseDelay = false;

    // 单独覆盖的投掷飞出延迟。只在 bOverrideThrowReleaseDelay 开启时生效。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Throw", meta = (ClampMin = "0.0", EditCondition = "bOverrideThrowReleaseDelay"))
    float ThrowReleaseDelayOverride = 0.35f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|First Person")
    FPickupHeldAnimationSet FirstPersonHeldAnimationSet;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Held Animation|Third Person")
    FPickupHeldAnimationSet ThirdPersonHeldAnimationSet;

    // 第一人称挂载点。可以是 FirstPersonMesh 上的 Socket 名，也可以直接填骨骼名，例如 RightSocket。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Hold|First Person")
    FName FP_SocketName = NAME_None;

    // 第一人称挂上去以后的额外位置微调。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Hold|First Person")
    FVector FP_LocationOffset = FVector::ZeroVector;

    // 第一人称挂上去以后的额外旋转微调。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Hold|First Person")
    FRotator FP_RotationOffset = FRotator::ZeroRotator;

    // 第三人称挂载点。通常是 GetMesh() 上的手部 Socket 或骨骼名。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Hold|Third Person")
    FName TP_SocketName = NAME_None;

    // 第三人称挂上去以后的额外位置微调。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Hold|Third Person")
    FVector TP_LocationOffset = FVector::ZeroVector;

    // 第三人称挂上去以后的额外旋转微调。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Hold|Third Person")
    FRotator TP_RotationOffset = FRotator::ZeroRotator;

    // 这个道具是否允许被 Rage 状态的鬼禁用。
    // 开启后，具体怎么失效由各个子类自己实现。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Disable")
    bool bCanBeDisabledByRage = true;

    // 只有开启的道具，投掷命中“正在吸食人类”的鬼时，才允许打断吸食。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Ghost Interrupt")
    bool bCanInterruptGhostSoulSuckOnHit = false;

    // 打断吸食后，鬼额外进入多久的倒地/眩晕恢复窗口。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Ghost Interrupt", meta = (ClampMin = "0.0"))
    float GhostSoulSuckInterruptStunDuration = 1.25f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Pickup|Disable")
    bool bDisabledByRage = false;

    void ApplyReleasedCollisionProfile();
    void ApplyPickupPhysicsTuning();
    FVector BuildThrowAngularVelocityInDegrees() const;
    void CacheInitialActorScale();
    void RestoreInitialActorScale();
    void ConfigureAttachedDisplayMeshes();
    void ConfigureDisplayMeshComponent(UStaticMeshComponent* DisplayMeshComponent) const;
    void GatherAttachedDisplayMeshComponents(TArray<UStaticMeshComponent*>& OutDisplayMeshComponents) const;

    // 记录关卡里原始摆放/蓝图默认的缩放，避免道具在附着到角色组件后
    // 把父组件缩放污染到自己身上，出现"越扔越大"的累积问题。
    UPROPERTY(Transient)
    FVector CachedInitialActorScale3D = FVector::OneVector;

    UPROPERTY(Transient)
    bool bHasCachedInitialActorScale = false;

    UFUNCTION()
    void OnRep_IsHeldByPlayer();

public:

#if WITH_EDITOR || UE_BUILD_DEVELOPMENT
public:
    void Debug_AddFPLocationOffset(FVector Delta) { FP_LocationOffset += Delta; }
    void Debug_AddFPRotationOffset(FRotator Delta)
    {
        FP_RotationOffset.Pitch += Delta.Pitch;
        FP_RotationOffset.Yaw   += Delta.Yaw;
        FP_RotationOffset.Roll  += Delta.Roll;
    }
    FVector  Debug_GetFPLocationOffset() const { return FP_LocationOffset; }
    FRotator Debug_GetFPRotationOffset() const { return FP_RotationOffset; }
#endif


    // 被玩家拿起时调用：关闭物理、重力和碰撞，进入“手持”状态。
    virtual void OnPickedUp();

    // 被放到地面时调用：恢复世界空间位置并重新开启物理。
    virtual void OnPutDown(FVector PlaceLocation, FRotator PlaceRotation);

    // 被投掷时调用：恢复物理后施加一次冲量。
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce);

    UFUNCTION(BlueprintPure, Category = "Pickup|Disable")
    bool IsDisabledByRage() const
    {
        return bDisabledByRage;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup")
    bool IsHeldByPlayer() const
    {
        return bIsHeldByPlayer;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Hold|Restrictions")
    bool AllowsSprintWhileHeld() const
    {
        return bAllowSprintWhileHeld;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Hold|Restrictions")
    bool AllowsThrowWhileHeld() const
    {
        return bAllowThrowWhileHeld;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Throw")
    float ResolveThrowReleaseDelay(float DefaultThrowReleaseDelay) const
    {
        return bOverrideThrowReleaseDelay
            ? FMath::Max(0.f, ThrowReleaseDelayOverride)
            : FMath::Max(0.f, DefaultThrowReleaseDelay);
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Ghost Interrupt")
    bool CanInterruptGhostSoulSuckOnHit() const
    {
        return bCanInterruptGhostSoulSuckOnHit;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Ghost Interrupt")
    float GetGhostSoulSuckInterruptStunDuration() const
    {
        return GhostSoulSuckInterruptStunDuration;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Hold")
    EHoldItemType GetHoldType() const
    {
        return HoldType;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Held Animation")
    const FPickupHeldAnimationSet& GetHeldAnimationSet() const
    {
        return ThirdPersonHeldAnimationSet;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Held Animation|First Person")
    const FPickupHeldAnimationSet& GetFirstPersonHeldAnimationSet() const
    {
        return FirstPersonHeldAnimationSet;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Held Animation|Third Person")
    const FPickupHeldAnimationSet& GetThirdPersonHeldAnimationSet() const
    {
        return ThirdPersonHeldAnimationSet;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Held Animation")
    const FPickupHeldAnimationSet& GetHeldAnimationSetForView(bool bFirstPersonView) const
    {
        return bFirstPersonView ? GetFirstPersonHeldAnimationSet() : GetThirdPersonHeldAnimationSet();
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Components")
    UStaticMeshComponent* GetPickupMeshComponent() const
    {
        return MeshComponent;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Hold|First Person")
    FName GetFirstPersonSocketName() const
    {
        return FP_SocketName;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Hold|First Person")
    FVector GetFirstPersonLocationOffset() const
    {
        return FP_LocationOffset;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Hold|First Person")
    FRotator GetFirstPersonRotationOffset() const
    {
        return FP_RotationOffset;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Hold|Third Person")
    FName GetThirdPersonSocketName() const
    {
        return TP_SocketName;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Hold|Third Person")
    FVector GetThirdPersonLocationOffset() const
    {
        return TP_LocationOffset;
    }

    UFUNCTION(BlueprintPure, Category = "Pickup|Hold|Third Person")
    FRotator GetThirdPersonRotationOffset() const
    {
        return TP_RotationOffset;
    }


    // 不是所有拾取物都支持“主动关闭功能”；默认返回 false，由具体子类自行开启。
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Pickup|Close")
    bool CanBeClosedByPlayer() const;
    virtual bool CanBeClosedByPlayer_Implementation() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Pickup|Close")
    void CloseByPlayer(AActor* ClosingActor);
    virtual void CloseByPlayer_Implementation(AActor* ClosingActor);

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Pickup|Close")
    bool IsClosedByPlayer() const;
    virtual bool IsClosedByPlayer_Implementation() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Pickup|Close")
    void OpenByPlayer(AActor* OpeningActor);
    virtual void OpenByPlayer_Implementation(AActor* OpeningActor);

    // 所有可交互道具统一暴露一个“能否被 Rage 禁用”的入口，便于后续扩展更多道具类型。
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Pickup|Disable")
    bool CanBeDisabledByRage() const;
    virtual bool CanBeDisabledByRage_Implementation() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Pickup|Disable")
    void DisableByRage(AActor* DisablingActor);
    virtual void DisableByRage_Implementation(AActor* DisablingActor);

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Pickup|Disable")
    void RestoreAfterRageDisable();
    virtual void RestoreAfterRageDisable_Implementation();

    // 原生代码路径使用这个接口，避免 Live Coding 重载后走 Blueprint 分发导致脚本函数指针失效。
    bool CanBeClosedByPlayerNative() const;
    void CloseByPlayerNative(AActor* ClosingActor);
    bool IsClosedByPlayerNative() const;
    void OpenByPlayerNative(AActor* OpeningActor);
    bool CanBeDisabledByRageNative() const;
    void DisableByRageNative(AActor* DisablingActor);
    void RestoreAfterRageDisableNative();

private:
    UFUNCTION()
    void HandlePickupMeshHit(
        UPrimitiveComponent* HitComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        FVector NormalImpulse,
        const FHitResult& Hit);

    bool TryInterruptGhostSoulSuckOnHit(AActor* OtherActor, const FHitResult& Hit, const FVector& NormalImpulse);
};