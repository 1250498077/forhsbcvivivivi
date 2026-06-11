#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Engine/HitResult.h"
#include "Blueprint/UserWidget.h"
#include "MyPlayerController.generated.h"

class FLifetimeProperty;
class UInputMappingContext;
class UInputAction;
class APickupActor;
class APickupActorAAARuneInstrument;
class PickupActorAAARuneGridInstrument;
class AWomenCharacter;

UCLASS()
class KONGBU_API AMyPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AMyPlayerController();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    // 默认输入映射表。要把你创建的 Input Mapping Context 配到这里，角色才能响应下面这些输入动作。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext;

    // 前进输入动作，一般对应键盘 W 或手柄左摇杆前推。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveForwardAction;

    // 左右移动输入动作，一般对应 A/D 或摇杆横向。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveRightAction;

    // 跳跃输入动作。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* JumpAction;

    // 上下视角输入动作，通常绑定鼠标 Y 轴。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* LookUpAction;

    // 左右视角输入动作，通常绑定鼠标 X 轴。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* LookRightAction;

    // 拾取或放下物品的交互输入动作。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* PickupAction;

    // 下蹲输入动作。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* SquatAction;

    // 打开或关闭 Tab 菜单的输入动作。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* ToggleTabAction;
    

    // 鼠标灵敏度倍率。调大后镜头旋转更快。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
    float MouseSensitivity = 1.0f;

    // 手持物体 bob 晃动速度，越大晃得越快。
    UPROPERTY(EditAnywhere, Category = "Pickup|Bob")
    float BobSpeed = 8.f;

    // 手持物体 bob 晃动幅度，越大晃动越明显。
    UPROPERTY(EditAnywhere, Category = "Pickup|Bob")
    float BobAmplitude = 0.f;

    // 手持物体偏移回正的插值速度，越大越跟手。
    UPROPERTY(EditAnywhere, Category = "Pickup|Bob")
    float BobInterpSpeed = 5.f;

    // 交互检测距离和盒扫掠半尺寸。
    // 这里不用单纯射线，是为了让拾取判定更宽容，鼠标不必精准点在很小的物体中心上。
    // 交互最大距离。超过这个距离的道具不能被拾取或操作。
    UPROPERTY(EditAnywhere, Category = "Pickup")
    float InteractDistance = 200.f;

    // 交互盒扫掠半尺寸。调大后准星附近更容易判到小物体。
    UPROPERTY(EditAnywhere, Category = "Pickup")
    FVector TraceHalfExtent = FVector(3.f, 3.f, 3.f);

    // 当前手里拿着的物体；为空表示现在没有持物。
    UPROPERTY(ReplicatedUsing = OnRep_HeldActor)
    APickupActor* HeldActor = nullptr;

public:
    APickupActor* GetHeldActor() const { return HeldActor; }

protected:
    // 内部手持动画状态。
    float BobTime = 0.f;
    FVector HeldActorBaseOffset = FVector(50.f, 30.f, -25.f);

    // 默认投掷力度。数值越大，道具会被扔得更远。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw")
    float ThrowForce = 1500.f;

    // 从按下投掷到真正释放物体的延迟。
    // 应该与投掷动画里"出手"时机对齐。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw", meta = (ClampMin = "0.0"))
    float ThrowReleaseDelay = 0.35f;

    // 物体释放后，投掷动作还额外保持多久才结束。
    // 用来避免动画先收回，物体才飞出去。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw", meta = (ClampMin = "0.0"))
    float ThrowAnimationRecoveryDelay = 0.5f;

    // 投掷输入锁总时长。期间不允许再次触发投掷。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw", meta = (ClampMin = "0.0"))
    float ThrowInputLockDuration = 0.9f;

    // 慢符近距离瞄准吸附的最大距离，越大越容易贴准星命中点。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throw|SlowTalisman", meta = (ClampMin = "0.0"))
    float SlowTalismanAimSnapMaxDistance = 200.f;

    // 投掷输入动作。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    UInputAction* ThrowAction;

    // 关闭道具输入动作，例如右键关闭当前手持或准星指向的可关闭物品。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
    UInputAction* CloseItemAction;


    // 正常走路速度。
    UPROPERTY(EditAnywhere, Category = "Movement")
    float WalkSpeed = 200.f;

    // 冲刺目标速度。
    UPROPERTY(EditAnywhere, Category = "Movement")
    float RunSpeed = 400.f;

    // 加速到目标速度的变化速率。
    UPROPERTY(EditAnywhere, Category = "Movement")
    float SpeedUpRate = 100000.f;

    // 降速回目标速度的变化速率。
    UPROPERTY(EditAnywhere, Category = "Movement")
    float SpeedDownRate = 200.f;

    // 冲刺输入动作。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* SprintAction;


    // 主界面 Widget 蓝图类。游戏开始时会实例化它。
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> MainUIClass;

    // Tab 菜单 Widget 蓝图类。按菜单键时会显示它。
    UPROPERTY(EditAnywhere, Category = "UI")
    TSubclassOf<UUserWidget> TabUIClass;

    UUserWidget* MainUI;
    UUserWidget* TabUI;

    void ToggleTabUI();
    void HandleRightClickPressed();
    void HandleRightClickReleased();
    void TryCloseHeldActorBehavior();
    bool TryTogglePickupActor(APickupActor* PickupActor);
    bool TryPickupActor(APickupActor* PickupActor);
    bool CanInteractWithPickupActor(const APickupActor* PickupActor) const;
    bool AttachHeldActorToFirstPersonView(APickupActor* PickupActor);
    bool AttachHeldActorToThirdPersonView(APickupActor* PickupActor);
    void StartThrowAnimationState(AWomenCharacter* MyChar) const;
    void FinishThrowAnimationState(AWomenCharacter* MyChar) const;
    void ClearThrowMiddleWindow(AWomenCharacter* MyChar) const;

    UFUNCTION()
    void OnRep_HeldActor();

    UFUNCTION(Server, Reliable)
    void ServerTryPickup(APickupActor* PickupActor);

    UFUNCTION(Server, Reliable)
    void ServerTryPutDown();

    UFUNCTION(Server, Reliable)
    void ServerTogglePickupActor(APickupActor* PickupActor);

    UFUNCTION(Server, Reliable)
    void ServerSubmitRuneSequence(APickupActor* RuneActor, const TArray<int32>& NodeSequence);

    UFUNCTION(Server, Reliable)
    void ServerThrowHeldActor();

    UFUNCTION(Server, Reliable)
    void ServerSetSprintState(bool bNewWantsToSprint);

    UFUNCTION(Server, Reliable)
    void ServerToggleSquatState();

    // 是否正在按住冲刺键。真正速度变化在 Tick 里平滑插值完成。
    bool bWantsToSprint = false;

    void SprintStart();
    void SprintStop();

    void SquatTriggered();

    // 输入处理函数。
    void MoveForward(const FInputActionValue& Value);
    void MoveRight(const FInputActionValue& Value);
    void Jump();
    void StopJump();
    void LookUp(const FInputActionValue& Value);
    void LookRight(const FInputActionValue& Value);

    // 拾取、放下和投掷辅助逻辑。
    void TryPickup();
    void TryPutDown();
    void ThrowHeldActor();
    void UpdateHeldRuneInstrumentDraw();
    bool GetPlaceLocation(FVector& OutLocation, FRotator& OutRotation);
    bool TracePickupActorFromView(APickupActor*& OutPickupActor, FHitResult& OutHit, bool bDrawDebug) const;
    bool TryClosePickupActor(APickupActor* PickupActor);

    // Tick 负责平滑速度变化、手持物 bob 动画和遮挡隐藏。
    virtual void Tick(float DeltaTime) override;



private:



};