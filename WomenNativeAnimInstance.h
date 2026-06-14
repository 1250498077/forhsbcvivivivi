#pragma once

#include "CoreMinimal.h"
#include "PickupActor.h"
#include "WomenAnimInstance.h"
#include "WomenNativeAnimInstance.generated.h"

class UAnimMontage;
class UAnimSequenceBase;

UENUM(BlueprintType)
enum class EWomenNativeLocomotionState : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Walk UMETA(DisplayName = "Walk"),
    Run UMETA(DisplayName = "Run"),
    JumpStart UMETA(DisplayName = "Jump Start"),
    FallLoop UMETA(DisplayName = "Fall Loop"),
    CrouchEnter UMETA(DisplayName = "Crouch Enter"),
    CrouchExit UMETA(DisplayName = "Crouch Exit"),
    CrouchIdle UMETA(DisplayName = "Crouch Idle"),
    CrouchWalk UMETA(DisplayName = "Crouch Walk"),
};

UENUM(BlueprintType)
enum class EWomenNativeAnimationInterruptPolicy : uint8
{
    // 可以随时被新状态打断。适合 Idle / Walk / Run / FallLoop 这类循环动作。
    Interruptible UMETA(DisplayName = "Interruptible"),

    // 必须等动作锁时间结束后才允许切到下一个状态。适合 JumpStart / 受击 / 开门等关键起手动作。
    MustFinish UMETA(DisplayName = "Must Finish"),
};

USTRUCT(BlueprintType)
struct FNativeLocomotionAnimationSet
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion")
    TObjectPtr<UAnimSequenceBase> Animation = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion")
    EWomenNativeAnimationInterruptPolicy InterruptPolicy = EWomenNativeAnimationInterruptPolicy::Interruptible;

    // MustFinish 时使用。<= 0 时会尝试使用动画自身长度。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion", meta = (ClampMin = "0.0"))
    float LockDuration = 0.f;
};

USTRUCT(BlueprintType)
struct FNativeHoldAnimationSet
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation")
    TObjectPtr<UAnimMontage> EquipMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation")
    TObjectPtr<UAnimMontage> UseMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation")
    TObjectPtr<UAnimSequenceBase> IdlePose = nullptr;
};

USTRUCT(BlueprintType)
struct FNativeLookBoneRotation
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Look")
    FName BoneName = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Look")
    float PitchWeight = 1.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Look")
    float YawWeight = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Look")
    float RollWeight = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Native Animation|Look")
    FRotator ComputedRotation = FRotator::ZeroRotator;
};

UCLASS(Transient, Blueprintable, BlueprintType)
class KONGBU_API UWomenNativeAnimInstance : public UWomenAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaTime) override;

    UPROPERTY(BlueprintReadOnly, Category = "Native Animation|Hold")
    EHoldItemType CurrentHoldType = EHoldItemType::None;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "Native Animation|Hold")
    TObjectPtr<APickupActor> CurrentHeldActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Native Animation|Hold")
    EHoldItemType PreviousHoldType = EHoldItemType::None;

    UPROPERTY(BlueprintReadOnly, Category = "Native Animation|Hold")
    TObjectPtr<UAnimSequenceBase> CurrentIdlePose = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Hold")
    TMap<EHoldItemType, FNativeHoldAnimationSet> HoldAnimations;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Throw")
    TObjectPtr<UAnimMontage> StandThrowMontage = nullptr;

    // 跑步时投掷的完整/上半身 Montage。用于边跑边投掷的专用动作。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Throw")
    TObjectPtr<UAnimMontage> RunThrowMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Throw")
    TObjectPtr<UAnimMontage> SquatThrowMontage = nullptr;

    // 上半身 Slot。跑步、走路继续投掷；投掷、使用物品等动作播放到 UpperBody。
    // 动画蓝图里需要用 Layered Blend Per Bone 把 UpperBody Slot 混到 spine_01 / clavicle_r / upperarm_r 等骨骼上。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Upper Body")
    FName UpperBodySlotName = TEXT("UpperBody");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Upper Body", meta = (ClampMin = "0.0"))
    float UpperBodyBlendInTime = 0.08f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Upper Body", meta = (ClampMin = "0.0"))
    float UpperBodyBlendOutTime = 0.12f;

    // 对于投掷这类"先抬手、再真正放出物体"的流程，
    // 如果使用的是普通 Sequence，默认保持循环到投掷状态结束，
    // 避免 Sequence 播完一瞬间就掉回普通持物姿势。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Throw")
    bool bLoopThrowUpperBodySequenceUntilThrowEnds = true;

    // 开启后，投掷会像你手动把 throw 填进 Run 一样，
    // 直接走 FullBody 槽，而不是走 UpperBody 分层混合。
    // 这样更适合当前只有 walk / run / throw / stand4 这类整套全身序列的项目。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Throw")
    bool bUseFullBodySlotForThrow = true;

    // 如果这里设置了 Sequence，会优先用 UpperBody Slot 动态播放。
    // 如果为空，就回退到 StandThrowMontage / SquatThrowMontage。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Upper Body")
    TObjectPtr<UAnimSequenceBase> StandThrowUpperBodyAnimation = nullptr;

    // 跑步投掷专用动画。角色正在 Run 状态时，优先播放这个。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Upper Body")
    TObjectPtr<UAnimSequenceBase> RunThrowUpperBodyAnimation = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Upper Body")
    TObjectPtr<UAnimSequenceBase> SquatThrowUpperBodyAnimation = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Action")
    TObjectPtr<UAnimMontage> StandOpenDoorMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Action")
    TObjectPtr<UAnimMontage> SquatOpenDoorMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Action")
    TObjectPtr<UAnimMontage> StandHitMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Action")
    TObjectPtr<UAnimMontage> SquatHitMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Action")
    TObjectPtr<UAnimSequenceBase> StandOpenDoorUpperBodyAnimation = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Action")
    TObjectPtr<UAnimSequenceBase> SquatOpenDoorUpperBodyAnimation = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Action")
    TObjectPtr<UAnimSequenceBase> StandHitUpperBodyAnimation = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Action")
    TObjectPtr<UAnimSequenceBase> SquatHitUpperBodyAnimation = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Native Animation|Action")
    bool bOneShotActionLocked = false;

    UPROPERTY(BlueprintReadOnly, Category = "Native Animation|Action")
    float OneShotActionLockTimeRemaining = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Native Animation|Locomotion")
    EWomenNativeLocomotionState CurrentLocomotionState = EWomenNativeLocomotionState::Idle;

    UPROPERTY(BlueprintReadOnly, Category = "Native Animation|Locomotion")
    EWomenNativeLocomotionState PreviousLocomotionState = EWomenNativeLocomotionState::Idle;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion")
    FName LocomotionSlotName = TEXT("FullBody");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion", meta = (ClampMin = "0.0"))
    float MoveSpeedThreshold = 10.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion", meta = (ClampMin = "0.0"))
    float FirstPersonHeldMovementStartDelay = 0.08f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion", meta = (ClampMin = "0.0"))
    float JumpStartDuration = 0.35f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion", meta = (ClampMin = "0.0"))
    float LocomotionBlendInTime = 0.15f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion", meta = (ClampMin = "0.0"))
    float LocomotionBlendOutTime = 0.15f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion")
    FNativeLocomotionAnimationSet IdleAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion")
    FNativeLocomotionAnimationSet WalkAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion")
    FNativeLocomotionAnimationSet RunAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion")
    FNativeLocomotionAnimationSet JumpStartAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion")
    FNativeLocomotionAnimationSet FallLoopAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion")
    FNativeLocomotionAnimationSet CrouchEnterAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion")
    FNativeLocomotionAnimationSet CrouchExitAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion")
    FNativeLocomotionAnimationSet CrouchIdleAnimation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Locomotion")
    FNativeLocomotionAnimationSet CrouchWalkAnimation;

    // 当前是否被一个 MustFinish 动作锁住。
    // 锁住期间不会响应 Walk/Run/Idle 等新状态，直到 AnimationLockTimeRemaining 归零。
    UPROPERTY(BlueprintReadOnly, Category = "Native Animation|Locomotion")
    bool bAnimationTransitionLocked = false;

    UPROPERTY(BlueprintReadOnly, Category = "Native Animation|Locomotion")
    float AnimationLockTimeRemaining = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Look")
    bool bEnableNativeLookBoneCalculation = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Look", meta = (ClampMin = "0.0"))
    float LookRotationInterpSpeed = 15.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Look")
    float MaxLookUpPitch = 65.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Look")
    float MaxLookDownPitch = -65.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Look")
    float MaxLookYaw = 75.f;

    UPROPERTY(BlueprintReadOnly, Category = "Native Animation|Look")
    FRotator SmoothedLookRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly, Category = "Native Animation|Look")
    FRotator Spine2LookRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly, Category = "Native Animation|Look")
    FRotator NeckLookRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly, Category = "Native Animation|Look")
    FRotator HeadLookRotation = FRotator::ZeroRotator;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Native Animation|Look")
    TArray<FNativeLookBoneRotation> LookBoneRotations;

    UFUNCTION(BlueprintCallable, Category = "Native Animation|Hold")
    bool PlayEquipMontageForHoldType(EHoldItemType HoldItemType);

    UFUNCTION(BlueprintCallable, Category = "Native Animation|Hold")
    bool PlayUseMontageForHoldType(EHoldItemType HoldItemType);

    UFUNCTION(BlueprintCallable, Category = "Native Animation|Upper Body")
    bool PlayUpperBodyAnimation(UAnimSequenceBase* Animation, bool bLoop = false);

    UFUNCTION(BlueprintCallable, Category = "Native Animation|Action")
    bool PlayOpenDoorAction();

    UFUNCTION(BlueprintCallable, Category = "Native Animation|Action")
    bool PlayHitAction();

protected:
    void InitializeDefaultLookBoneRotations();
    bool HasSkeletonBone(FName BoneName) const;
    void UpdateOneShotActionLock(float DeltaTime);
    void UpdateNativeLookBoneRotations(float DeltaTime);
    FRotator CalculateControlRotationDelta() const;
    void UpdateLocomotionState(float DeltaTime);
    EWomenNativeLocomotionState ResolveLocomotionState() const;
    void HandleLocomotionStateChanged();
    const FNativeLocomotionAnimationSet* GetAnimationSetForLocomotionState(EWomenNativeLocomotionState LocomotionState) const;
    bool HasPlayableLocomotionAnimation(EWomenNativeLocomotionState LocomotionState) const;
    
    bool PlayLocomotionAnimation(EWomenNativeLocomotionState LocomotionState);
    bool CanInterruptCurrentLocomotion(EWomenNativeLocomotionState RequestedState) const;
    void StartAnimationLockIfNeeded(const FNativeLocomotionAnimationSet& AnimationSet);
    void UpdateHoldTypeFromOwner();
    void HandleHoldTypeChanged();
    void HandleThrowMontageState();
    bool IsFirstPersonAnimationInstance() const;
    const FPickupHeldAnimationSet* GetCurrentHeldAnimationSet() const;
    UAnimSequenceBase* GetHeldLocomotionAnimation(EWomenNativeLocomotionState LocomotionState) const;
    UAnimSequenceBase* GetHeldThrowUpperBodyAnimation(bool bSquatThrow, bool bRunThrow) const;
    UAnimMontage* GetHeldThrowMontage(bool bSquatThrow, bool bRunThrow) const;
    bool PlayThrowAction(bool bSquatThrow);
    void StopThrowAction();
    bool IsRunningThrow() const;
    bool IsCrouchAction() const;
    bool CanPlayOneShotAction() const;
    bool PlayOneShotAction(UAnimSequenceBase* UpperBodyAnimation, UAnimMontage* Montage);
    void StartOneShotActionLock(float LockDuration);

private:
    bool bWasStandThrowing = false;
    bool bWasSquatThrowing = false;
    bool bWasInAir = false;
    bool bWasSquat = false;
    bool bPendingCrouchEnter = false;
    bool bPendingCrouchExit = false;
    bool bHasMovementInput = false;
    float JumpStartTimeRemaining = 0.f;
    float FirstPersonHeldMovingTime = 0.f;

    UPROPERTY(Transient)
    TObjectPtr<UAnimMontage> ActiveThrowDynamicMontage = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<APickupActor> PreviousHeldActor = nullptr;

    bool bThrowUsesLoopingUpperBodySequence = false;
    bool bThrowUsesFullBodySlot = false;
};