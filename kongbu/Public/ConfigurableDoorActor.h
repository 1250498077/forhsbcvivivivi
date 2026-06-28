#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "ConfigurableDoorActor.generated.h"

class UCurveFloat;
class UArrowComponent;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;


UENUM(BlueprintType)
enum class EDoorEditorPreviewState : uint8
{
    Closed,
    Open
};


UCLASS(Blueprintable)
class KONGBU_API AConfigurableDoorActor : public AActor
{
    GENERATED_BODY()

public:
    AConfigurableDoorActor();

    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    bool IsActorInInteractionRange(const AActor* InteractingActor) const;
    bool CanActorInteractFromView(const AActor* InteractingActor, const FVector& ViewLocation, const FVector& ViewDirection) const;

protected:
    virtual void BeginPlay() override;

    void ApplyDoorStateInstant(bool bOpenState);
    void StartDoorAnimation(bool bOpenState);
    void ConfigureDoorMeshBlocking();
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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> DoorBlockerComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> InteractionRangeComponent;

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


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Interaction")
    FVector InteractionBoxExtent = FVector(120.f, 90.f, 120.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Interaction")
    bool bShowInteractionRangeDebug = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Interaction", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float InteractionFacingDotThreshold = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Collision")
    FVector DoorBlockerExtent = FVector(12.f, 60.f, 110.f);

    // --- Door | Setup ---

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Setup")
    bool bStartOpened = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Editor")
    bool bEnableEditorPreview = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Editor", meta = (EditCondition = "bEnableEditorPreview"))
    EDoorEditorPreviewState EditorPreviewState = EDoorEditorPreviewState::Closed;

    // --- Door | State ---

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Door|State")
    bool bIsOpen = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Door|State")
    bool bIsAnimating = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Door|State")
    bool bIsLockedClosed = false;

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

    FTimerHandle DoorLockTimerHandle;

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

    UFUNCTION(BlueprintCallable, Category = "Door|Lock")
    void LockDoorClosedForDuration(float Duration);

    UFUNCTION(BlueprintCallable, Category = "Door|Lock")
    void UnlockDoor();

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

    UFUNCTION(BlueprintPure, Category = "Door|Lock")
    bool IsDoorLockedClosed() const
    {
        return bIsLockedClosed;
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