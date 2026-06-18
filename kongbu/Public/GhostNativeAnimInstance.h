#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MyAIController.h"
#include "GhostNativeAnimInstance.generated.h"

class AGhostCharacter;
class UAnimMontage;
class UAnimSequenceBase;

UENUM(BlueprintType)
enum class EGhostNativeLocomotionState : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Walk UMETA(DisplayName = "Walk"),
    Run UMETA(DisplayName = "Run")
};

UCLASS(Transient, Blueprintable, BlueprintType)
class KONGBU_API UGhostNativeAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaTime) override;

    UPROPERTY(BlueprintReadOnly, Category = "Ghost Animation|State")
    float Speed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Ghost Animation|State")
    EEnemyAIState CurrentAIState = EEnemyAIState::Patrol;

    UPROPERTY(BlueprintReadOnly, Category = "Ghost Animation|State")
    EGhostNativeLocomotionState CurrentLocomotionState = EGhostNativeLocomotionState::Idle;

    UPROPERTY(BlueprintReadOnly, Category = "Ghost Animation|State")
    EGhostNativeLocomotionState PreviousLocomotionState = EGhostNativeLocomotionState::Idle;

    UPROPERTY(BlueprintReadOnly, Category = "Ghost Animation|Action")
    bool bActionLocked = false;

    UPROPERTY(BlueprintReadOnly, Category = "Ghost Animation|Action")
    bool bIsOpeningDoor = false;

    UPROPERTY(BlueprintReadOnly, Category = "Ghost Animation|Action")
    bool bIsSoulSucking = false;

    UPROPERTY(BlueprintReadOnly, Category = "Ghost Animation|Action")
    float ActionLockTimeRemaining = 0.f;

    UFUNCTION(BlueprintCallable, Category = "Ghost Animation|Action")
    bool PlayOpenDoorAction();

    UFUNCTION(BlueprintCallable, Category = "Ghost Animation|Action")
    bool PlaySoulSuckAction();

    UFUNCTION(BlueprintCallable, Category = "Ghost Animation|Action")
    bool PlayKnockdownAction();

    UFUNCTION(BlueprintPure, Category = "Ghost Animation|Action")
    float GetKnockdownActionDuration() const;

protected:
    AGhostCharacter* GetGhostOwner() const;
    void UpdateActionLock(float DeltaTime);
    EGhostNativeLocomotionState ResolveLocomotionState(const AGhostCharacter* GhostOwner) const;
    void HandleLocomotionStateChanged(const AGhostCharacter* GhostOwner);
    bool PlayLocomotionAnimation(const AGhostCharacter* GhostOwner, EGhostNativeLocomotionState LocomotionState);
    bool PlayAction(UAnimSequenceBase* Animation, UAnimMontage* Montage, FName SlotName, float BlendInTime, float BlendOutTime, float LockDuration);
    void StartActionLock(float LockDuration);

private:
    UPROPERTY(Transient)
    TObjectPtr<UAnimMontage> ActiveActionDynamicMontage = nullptr;

    bool bHasPlayedLocomotion = false;
};