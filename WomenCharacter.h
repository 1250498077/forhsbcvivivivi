// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WomenCharacter.generated.h"

class FLifetimeProperty;
class APickupActor;

UCLASS()
class KONGBU_API AWomenCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    // 构造函数中会初始化第一人称相机和手持物体的挂点。
    AWomenCharacter();

protected:
    // 角色生成后进入场景时调用，适合放运行时初始化逻辑。
    virtual void BeginPlay() override;

    // 在编辑器和运行时构造阶段同步模块化部件到自身骨架。
    virtual void OnConstruction(const FTransform& Transform) override;

    // 每帧平滑更新第一人称摄像机高度，例如下蹲时降低视角。
    virtual void Tick(float DeltaTime) override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void RefreshModularMeshes();

    void UpdateFirstPersonCameraTransform(float DeltaTime, bool bUseRuntimeSmoothing);

    void UpdateFirstPersonMeshRootTransform();

    FVector CalculateFirstPersonCameraMovementOffset(float DeltaTime);

    FVector SmoothedFirstPersonCameraMovementOffset = FVector::ZeroVector;

// ===== Debug: Runtime Held Item Offset Tuning =====
#if WITH_EDITORONLY_DATA
    UPROPERTY()
    APickupActor* DebugCurrentHeldItem = nullptr;
#endif

#if WITH_EDITOR || UE_BUILD_DEVELOPMENT
    void Debug_TickOffsetTuning();
    // void Debug_NudgeHeldItemOffset(FVector DeltaLoc, FRotator DeltaRot);
    // void Debug_PrintHeldItemOffset() const;
private:
    bool bDebugOffsetTuningActive = false;
#endif

public:
    
    // 第一人称相机根节点。用于承载蓝图可调的相机位置/旋转偏移。
    // 相机本体仍然使用控制器旋转，所以不要直接拖 Camera 组件来调最终视角。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    class USceneComponent* FirstPersonCameraRoot;

    // 第一人称相机。运行时保持本地零位；高度和偏移由 FirstPersonCameraRoot 以及 Camera 参数统一控制。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    class UCameraComponent* FirstPersonCameraComponent;

    // 持物挂点。附着在相机下，便于让拾取物始终出现在屏幕前方合适位置。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    class USceneComponent* HoldPoint;

    // 第一人称模型根节点。用于把第一人称身体/手臂位置和相机高度解耦。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "First Person")
    class USceneComponent* FirstPersonMeshRoot;

    // 第一人称主骨架模型。挂在 FirstPersonMeshRoot 下，作为第一人称模块化部件的姿态驱动源，
    // 也保留手持物挂接所需的骨骼 / Socket。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "First Person")
    class USkeletalMeshComponent* FirstPersonMesh;

    // 第一人称头部部件。通常建议关闭可见或不指定资源，避免挡住镜头
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "First Person|Modular Mesh")
    class USkeletalMeshComponent* FirstPersonHeadMesh;

    // 第一人称头发部件。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "First Person|Modular Mesh")
    class USkeletalMeshComponent* FirstPersonHairMesh;

    // 第一人称上衣部件。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "First Person|Modular Mesh")
    class USkeletalMeshComponent* FirstPersonTopMesh;

    // 第一人称手部部件。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "First Person|Modular Mesh")
    class USkeletalMeshComponent* FirstPersonHandMesh;

    // 第一人称裤子部件。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "First Person|Modular Mesh")
    class USkeletalMeshComponent* FirstPersonPantsMesh;

    // 第一人称鞋子部件。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "First Person|Modular Mesh")
    class USkeletalMeshComponent* FirstPersonShoesMesh;

    // 开发调试用的第三人称手持物代理网格
    // 本地玩家把真实道具挂到第一人称手上时，用它同步显示一个第三人称版本，便于检查 TP Socket。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug")
    class UStaticMeshComponent* ThirdPersonHeldItemDebugMesh;

    // 第三人称头部部件。可在蓝图子类中直接指定头部骨骼网格。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Modular Mesh")
    class USkeletalMeshComponent* HeadMesh;

    // 第三人称头发部件。可在蓝图子类中直接指定头发骨骼网格。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Modular Mesh")
    class USkeletalMeshComponent* HairMesh;

    // 第三人称上衣部件。可在蓝图子类中直接指定衣服骨骼网格。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Modular Mesh")
    class USkeletalMeshComponent* TopMesh;

    // 第三人称手部部件。可在蓝图子类中直接指定手部骨骼网格。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Modular Mesh")
    class USkeletalMeshComponent* HandMesh;

    // 第三人称裤子部件。可在蓝图子类中直接指定裤子骨骼网格。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Modular Mesh")
    class USkeletalMeshComponent* PantsMesh;

    // 第三人称鞋子部件。可在蓝图子类中直接指定鞋子骨骼网格。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Modular Mesh")
    class USkeletalMeshComponent* ShoesMesh;

    // 是否显示第三人称基础身体网格。
    // 即使关闭显示，基础网格仍会保留为模块化子网格提供骨骼姿态驱动。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modular Mesh")
    bool bShowBaseThirdPersonMesh = false;

    // ======================================================================

    // 站立时第一人称相机高度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Crouch")
    float StandingCameraHeight = 64.f;

    // 下蹲时第一人称相机高度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Crouch")
    float CrouchingCameraHeight = 34.f;

    // 相机从站立高度过渡到下蹲高度的平滑速度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Crouch", meta = (ClampMin = "0.0"))
    float CameraCrouchInterpSpeed = 10.f;

    // 第一人称相机额外位置偏移。蓝图里要调视角位置，请改这个值。
    // Z 会叠加 StandingCameraHeight / CrouchingCameraHeight。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Offset")
    FVector FirstPersonCameraLocationOffset = FVector::ZeroVector;

    // 第一人称相机根节点额外旋转偏移。蓝图里要调基础视角角度，请改这个值。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Offset")
    FRotator FirstPersonCameraRotationOffset = FRotator::ZeroRotator;


    // 简单防穿模: 移动速度超过阈值时，把相机按本地坐标平滑推到指定偏移；停下后回到原始位置。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anti Clip")
    bool bEnableFirstPersonCameraMovementOffset = true;

    // 速度大于这个值时启用相机偏移。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anti Clip", meta = (ClampMin = "0.0"))
    float FirstPersonCameraMovementOffsetSpeedThreshold = 100.f;

    // 移动时相机额外本地偏移。X=向前/向后，Y=左右，Z=上下。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anti Clip")
    FVector FirstPersonCameraMovementOffset = FVector(20.f, 0.f, -10.f);

    // 偏移切换速度，越大越快，越小越慢。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anti Clip", meta = (ClampMin = "0.0"))
    float FirstPersonCameraMovementOffsetInterpSpeed = 8.f;

    // 第一人称整体模型相对基础身体网格的额外偏移。
    // 因为代码会在运行时自动同步 FirstPersonMesh 到基础网格，
    // 所以如果要在蓝图里微调位置，需要改这个值，而不是直接拖组件 Transform。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Offset")
    FVector FirstPersonMeshLocationOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Offset")
    FRotator FirstPersonMeshRotationOffset = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Camera Follow")
    bool bFirstPersonMeshFollowsCameraPitch = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Camera Follow", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
    float FirstPersonMeshMinCameraPitch = -80.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Camera Follow", meta = (ClampMin = "-89.0", ClampMax = "89.0"))
    float FirstPersonMeshMaxCameraPitch = 80.f;

    // 第一人称骨骼比较特殊时建议保持关闭，并在蓝图里单独指定 FirstPersonMesh。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Mesh")
    bool bCopyThirdPersonMeshToFirstPersonMesh = false;

    // 第一人称模块子网格没有指定资源时，是否复用第三人称对应模块资源。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Mesh")
    bool bCopyThirdPersonModularMeshesToFirstPerson = false;

    // 是否让第一人称主网格直接跟随第三人称主网格姿态。
    // 第一人称和第三人称使用不同 AnimBP 时应保持关闭，让 FirstPersonMesh 使用自己的动画蓝图。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "First Person|Animation")
    bool bUseThirdPersonPoseForFirstPersonMesh = false;

    // 将角色传送到指定世界坐标和朝向。
    // 适合蓝图或触发器直接传入目标位置使用。
    UFUNCTION(BlueprintCallable, Category = "Teleport")
    void TeleportToLocation(FVector NewLocation, FRotator NewRotation);

    // 将角色传送到目标 Actor 所在的位置和旋转。
    // 常用于把角色传送到某个传送点、门或检查点。
    UFUNCTION(BlueprintCallable, Category = "Teleport")
    void TeleportToTarget(AActor* TargetActor);

    // 刷新模块化部件，让头发、衣服、裤子、鞋子自动跟随主身体骨架动作。
    UFUNCTION(BlueprintCallable, Category = "Modular Mesh")
    void RebuildModularMeshes();

    void ShowHeldItemThirdPersonDebugMesh(APickupActor* PickupActor);
    void HideHeldItemThirdPersonDebugMesh();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "First Person|Editor")
    void CaptureCurrentFirstPersonMeshRootOffset();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "First Person|Editor")
    void PreviewFirstPersonMeshRootOffset();

    // 是否处于下蹲状态。通常供动画蓝图或移动逻辑读取。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Movement")
    bool IsSquat = false;

    // 是否处于冲刺状态。可用于区分跑步动画、速度或体力逻辑。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Movement")
    bool IsSprint = false;

    // 是否正在下蹲投掷。用于驱动对应的下蹲投掷动画状态。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Movement")
    bool IsSquatThrowing = false;
    

    // 是否正在站立投掷。用于驱动对应的站立投掷动画状态。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Movement")
    bool IsStandThrowing = false;

    // 是否处于投掷动作中段的关键处理时机。
    // 这类标记常用于在动画通知之间控制真正的抛出或状态切换。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Movement")
    bool IsMiddleHandleTime = false;

    
};
