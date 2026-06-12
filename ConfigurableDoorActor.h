#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConfigurableDoorActor.generated.h"

class UCurveFloat;
class UArrowComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class KONGBU_API AConfigurableDoorActor : public AActor
{
    GENERATED_BODY()

public:
    AConfigurableDoorActor();

    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    virtual void BeginPlay() override;

    void ApplyDoorStateInstant(bool bOpenState);
    void StartDoorAnimation(bool bOpenState);
    float EvaluateAnimationAlpha(float NormalizedAlpha) const;
    FTransform GetClosedDoorTransform() const;
    FTransform GetOpenedDoorTransform() const;
    // --- Components ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRootComponent;

    // 额外的门框、柜体、装饰网格建议挂到这个节点下面；它们不会跟着门一起动。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> StaticMeshesRootComponent;

    // 只有这个节点会做开关过渡；门本体网格挂在这里
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> DoorMotionRootComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> DoorMeshComponent;

    // --- Door | Transform ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Transform")
    TObjectPtr<UArrowComponent> ClosedStateTransformComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Transform")
    TObjectPtr<UArrowComponent> OpenStateTransformComponent;

    // --- Door | Animation ---

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Animation", meta = (ClampMin = "0.0"))
    float TransitionDuration = 0.5f;

    // 可选曲线，输入时间范围建议为 0~1，输出也建议为 0~1。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Animation")
    TObjectPtr<UCurveFloat> TransitionCurve = nullptr;

    // --- Door | Setup ---

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Setup")
    bool bStartOpened = false;

    // --- Door | State ---

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Door|State")
    bool bIsOpen = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Door|State")
    bool bIsAnimating = false;

    // --- Transient Properties (Runtime Only) ---

    UPROPERTY(Transient)
    bool bAnimationTargetOpen = false;

    UPROPERTY(Transient)
    float CurrentAnimationTime = 0.f;

    UPROPERTY(Transient)
    FVector AnimationStartLocation = FVector::ZeroVector;

    UPROPERTY(Transient)
    FQuat AnimationStartRotation = FQuat::Identity;

    UPROPERTY(Transient)
    FVector AnimationTargetLocation = FVector::ZeroVector;

    UPROPERTY(Transient)
    FQuat AnimationTargetRotation = FQuat::Identity;

public:
    // --- Public API Functions ---

    UFUNCTION(BlueprintCallable, Category = "Door")
    void OpenDoor();

    UFUNCTION(BlueprintCallable, Category = "Door")
    void CloseDoor();

    UFUNCTION(BlueprintCallable, Category = "Door")
    void ToggleDoor();

    UFUNCTION(BlueprintCallable, Category = "Door")
    void SetDoorOpenState(bool bNewOpenState, bool bInstant = false);

    UFUNCTION(BlueprintPure, Category = "Door")
    bool IsDoorOpen() const
    {
        return bIsOpen;
    }

    UFUNCTION(BlueprintPure, Category = "Door")
    bool IsDoorAnimating() const
    {
        return bIsAnimating;
    }

    UFUNCTION(BlueprintPure, Category = "Door|Components")
    UStaticMeshComponent* GetDoorMeshComponent() const
    {
        return DoorMeshComponent;
    }

    UFUNCTION(BlueprintPure, Category = "Door|Components")
    USceneComponent* GetStaticMeshesRootComponent() const
    {
        return StaticMeshesRootComponent;
    }

    // --- Editor Utilities ---

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Door|Editor")
    void CaptureCurrentAsClosedState();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Door|Editor")
    void CaptureCurrentAsOpenState();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Door|Editor")
    void PreviewClosedState();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "Door|Editor")
    void PreviewOpenState();
};