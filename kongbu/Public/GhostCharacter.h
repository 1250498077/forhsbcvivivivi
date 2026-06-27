// GhostCharacter.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "GhostCharacter.generated.h"

class UCapsuleComponent;
class UPrimitiveComponent;
class AMyAIController;
class AWomenCharacter;
class UGhostThrowablePropComponent;
class UAnimMontage;
class UAnimSequenceBase;

USTRUCT(BlueprintType)
struct KONGBU_API FGhostLocomotionAnimation
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation")
    TObjectPtr<UAnimSequenceBase> Animation = nullptr;
};

USTRUCT(BlueprintType)
struct KONGBU_API FGhostActionAnimation
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation")
    TObjectPtr<UAnimSequenceBase> Animation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation")
    TObjectPtr<UAnimMontage> Montage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation", meta = (ClampMin = "0.0"))
    float LockDuration = 0.f;
};
// 鬼 AI 专用角色基类。
// 蓝图鬼角色建议直接继承这个类，然后在蓝图组件面板里调整 GhostAttachZoneComponent 的位置、半径和半高。
UCLASS()
class KONGBU_API AGhostCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AGhostCharacter();

    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    // 一个鬼蓝图对应一个驱魔鬼类型。比如 BP_Ghost_1 填 1，BP_Ghost_2 填 2。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Exorcism")
    int32 ExorcismGhostTypeId = INDEX_NONE;

    // 统一的道具吸附区域。慢符 / 符文器触发这个区域后，会挂到这个组件上。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost|Attach")
    TObjectPtr<UCapsuleComponent> GhostAttachZoneComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost|Soul Suck")
    TObjectPtr<UCapsuleComponent> SoulSuckTriggerComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost|Telekinesis")
    TObjectPtr<UCapsuleComponent> TelekinesisTriggerComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Locomotion")
    FGhostLocomotionAnimation IdleAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Locomotion")
    FGhostLocomotionAnimation WalkAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Locomotion")
    FGhostLocomotionAnimation RunAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Action")
    FGhostActionAnimation OpenDoorAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Action")
    FGhostActionAnimation SoulSuckAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Action")
    FGhostActionAnimation KnockdownAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Slots")
    FName LocomotionSlotName = TEXT("FullBody");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Slots")
    FName ActionSlotName = TEXT("FullBody");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Blend", meta = (ClampMin = "0.0"))
    float AnimationBlendInTime = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Blend", meta = (ClampMin = "0.0"))
    float AnimationBlendOutTime = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Locomotion", meta = (ClampMin = "0.0"))
    float MoveSpeedThreshold = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Turn")
    bool bEnableGhostTurnSmoothing = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Turn", meta = (ClampMin = "0.0"))
    float GhostBodyRotationRateYaw = 180.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Turn")
    bool bEnableUpperBodyTurnLead = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Turn", meta = (ClampMin = "0.0", ClampMax = "90.0"))
    float UpperBodyTurnMaxYaw = 45.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Turn", meta = (ClampMin = "0.0"))
    float UpperBodyTurnInterpSpeed = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Animation|Turn", meta = (ClampMin = "0.0"))
    float UpperBodyTurnReturnSpeed = 6.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Soul Suck")
    bool bEnableSoulSuckOnPlayerTouch = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Soul Suck", meta = (ClampMin = "0.0"))
    float SoulSuckDuration = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Soul Suck", meta = (ClampMin = "0.0"))
    float SoulSuckLiftHeight = 50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Soul Suck", meta = (ClampMin = "0.0"))
    float SoulSuckLiftInterpSpeed = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Soul Suck")
    bool bFreezeVictimDuringSoulSuck = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Soul Suck")
    bool bStopAIMovementDuringSoulSuck = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Soul Suck", meta = (ClampMin = "0.0"))
    float SoulSuckBreakRecoveryDuration = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Telekinesis")
    bool bEnableTelekineticThrowableThrow = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Telekinesis", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float TelekineticThrowActivationChance = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Telekinesis", meta = (ClampMin = "0.0"))
    float TelekineticHoverDuration = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Telekinesis", meta = (ClampMin = "0.0"))
    float TelekineticHoverHeight = 180.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Telekinesis", meta = (ClampMin = "0.0"))
    float TelekineticHoverBobAmplitude = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Telekinesis", meta = (ClampMin = "0.0"))
    float TelekineticHoverBobFrequency = 1.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Telekinesis", meta = (ClampMin = "0.0"))
    float TelekineticThrowForce = 3200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Telekinesis")
    bool bStopAIMovementDuringTelekineticThrow = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Telekinesis|Debug")
    bool bShowTelekinesisTriggerDebug = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Telekinesis|Debug", meta = (EditCondition = "bShowTelekinesisTriggerDebug"))
    FColor TelekinesisTriggerDebugColor = FColor::Purple;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Soul Suck|Debug")
    bool bShowSoulSuckTriggerDebug = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Soul Suck|Debug", meta = (EditCondition = "bShowSoulSuckTriggerDebug"))
    FColor SoulSuckTriggerDebugColor = FColor::Red;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ghost|Soul Suck")
    bool bIsSoulSucking = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ghost|Soul Suck")
    TObjectPtr<AWomenCharacter> CurrentSoulSuckVictim = nullptr;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ghost|Soul Suck")
    bool bIsSoulSuckBreakRecoveryActive = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ghost|Telekinesis")
    bool bIsTelekineticThrowActive = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ghost|Telekinesis")
    TObjectPtr<UGhostThrowablePropComponent> CurrentTelekineticThrowable = nullptr;


    // 调试用: 开启后可在蓝图/PIE 中实时看到鬼的道具吸附触碰区域。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Attach|Debug")
    bool bShowGhostAttachZoneDebug = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Attach|Debug", meta = (EditCondition = "bShowGhostAttachZoneDebug"))
    FColor GhostAttachZoneDebugColor = FColor::Cyan;

    UFUNCTION(BlueprintPure, Category = "Ghost|Soul Suck")
    UCapsuleComponent* GetSoulSuckTriggerComponent() const;

    UFUNCTION(BlueprintPure, Category = "Ghost|Telekinesis")
    UCapsuleComponent* GetTelekinesisTriggerComponent() const;

    UFUNCTION(BlueprintPure, Category = "Ghost|Attach")
    UCapsuleComponent* GetGhostAttachZoneComponent() const;

    UFUNCTION(BlueprintPure, Category = "Ghost|Attach")
    bool IsWorldLocationInsideGhostAttachZone(const FVector& WorldLocation, float Tolerance = 12.f) const;

    UFUNCTION(BlueprintCallable, Category = "Ghost|Animation")
    bool PlayGhostOpenDoorAnimation();

    UFUNCTION(BlueprintCallable, Category = "Ghost|Soul Suck")
    bool StartSoulSuck(AWomenCharacter* Victim);

    UFUNCTION(BlueprintCallable, Category = "Ghost|Soul Suck")
    void StopSoulSuck();

    UFUNCTION(BlueprintCallable, Category = "Ghost|Soul Suck")
    void StopSoulSuckAfterVictimEscape();

    UFUNCTION(BlueprintPure, Category = "Ghost|Soul Suck")
    bool IsSoulSuckBreakRecoveryActive() const;

    UFUNCTION(BlueprintCallable, Category = "Ghost|Soul Suck")
    bool InterruptSoulSuckWithKnockdown(AActor* ImpactSourceActor, FVector ImpactDirection, float StunDuration = -1.f);

    UFUNCTION(BlueprintCallable, Category = "Ghost|Telekinesis")
    bool StartTelekineticThrowableThrow(UGhostThrowablePropComponent* ThrowableProp);

    UFUNCTION(BlueprintCallable, Category = "Ghost|Telekinesis")
    void StopTelekineticThrowableThrow(bool bLaunchThrowable = true);

    UFUNCTION(BlueprintCallable, Category = "Ghost|Effects")
    void ApplyTemporaryMoveSpeedMultiplier(float SpeedMultiplier = 1.25f, float Duration = 5.f);
    
protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandleGhostAttachZoneBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);


    UFUNCTION()
    void HandleSoulSuckTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void HandleTelekinesisTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION(NetMulticast, Reliable)
    void MulticastPlayGhostSoulSuckAnimation();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastStopGhostSoulSuckAnimation();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastPlayGhostKnockdownAnimation();

private:
    void ConfigureGhostAttachZoneCollision() const;
    void ConfigureSoulSuckTriggerCollision() const;
    void ConfigureTelekinesisTriggerCollision() const;
    void UpdateGhostAttachZoneDebugVisibility() const;
    void UpdateSoulSuckTriggerDebugVisibility() const;
    void UpdateTelekinesisTriggerDebugVisibility() const;
    void ApplyGhostTurnSettings() const;
    void OrientSoulSuckVictimTowardGhost(AWomenCharacter* Victim) const;
    void FreezeSoulSuckVictim(AWomenCharacter* Victim) const;
    void UnfreezeSoulSuckVictim(AWomenCharacter* Victim) const;
    void UpdateSoulSuckVictimLift(float DeltaTime);
    void StartSoulSuckBreakRecovery();
    void UpdateSoulSuckBreakRecovery(float DeltaTime);
    float ResolveSoulSuckDuration() const;
    float ResolveGhostKnockdownDuration() const;
    AActor* ResolveTelekineticTargetActor() const;
    bool RefreshTelekineticTargetLocation(FVector& OutLocation);
    void UpdateTelekineticThrowableThrow(float DeltaTime);
    FTimerHandle TemporaryMoveSpeedMultiplierTimerHandle;
    float CachedMoveSpeedBeforeTemporaryMultiplier = 0.f;
    bool bHasCachedMoveSpeedBeforeTemporaryMultiplier = false;

    void RestoreTemporaryMoveSpeedMultiplier();
    AMyAIController* ResolveMyAIController() const;

    FTimerHandle SoulSuckTimerHandle;
    FVector SoulSuckVictimStartLocation = FVector::ZeroVector;
    FVector SoulSuckVictimTargetLocation = FVector::ZeroVector;
    float SoulSuckBreakRecoveryTimeRemaining = 0.f;
    TObjectPtr<AActor> CurrentTelekineticTargetActor = nullptr;
    FVector TelekineticStartLocation = FVector::ZeroVector;
    FVector LastTrackedTelekineticTargetLocation = FVector::ZeroVector;
    float TelekineticElapsedTime = 0.f;
};