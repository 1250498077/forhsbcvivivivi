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
    AWomenCharacter();

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void RefreshModularMeshes();
    FVector CalculateFirstPersonCameraMovementOffset(float DeltaTime);
    FVector SmoothedFirstPersonCameraMovementOffset = FVector::ZeroVector;

// ===== Debug: Runtime Held Item Offset Tuning =====
#if WITH_EDITORONLY_DATA
    UPROPERTY()
    APickupActor* DebugCurrentHeldItem = nullptr;
#endif

#if WITH_EDITOR || UE_BUILD_DEVELOPMENT
    void Debug_TickOffsetTuning();
private:
    bool bDebugOffsetTuningActive = false;
#endif

public:

    // ======================================================================
    // 相机
    // ======================================================================

    // 第一人称相机根节点。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    class USceneComponent* FirstPersonCameraRoot;

    // 第一人称相机。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    class UCameraComponent* FirstPersonCameraComponent;

    // 持物挂点。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    class USceneComponent* HoldPoint;

    // ======================================================================
    // 第一人称骨骼网格
    // ======================================================================

    // 第一人称上半身主骨骼。挂在相机下，跟随视角 Pitch。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "First Person")
    class USkeletalMeshComponent* FirstPersonMesh;

    // 第一人称下半身骨骼。挂在角色身体坐标系下，不跟随相机 Pitch。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "First Person")
    class USkeletalMeshComponent* FirstPersonLowerBodyMesh;

    // ======================================================================
    // 第一人称模块化部件（仅本地玩家可见，跟随 FirstPersonMesh 骨骼）
    // 在蓝图 Details 面板里直接指定 Skeletal Mesh 资产即可，和第三人称一样。
    // ======================================================================

    // 第一人称手臂部件。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "First Person|Modular")
    class USkeletalMeshComponent* FPArmsMesh;

    // 第一人称上衣部件。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "First Person|Modular")
    class USkeletalMeshComponent* FPTopMesh;

    // 第一人称手部/手套部件。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "First Person|Modular")
    class USkeletalMeshComponent* FPHandMesh;

    // ======================================================================
    // 第三人称骨骼网格
    // ======================================================================

    // 开发调试用的第三人称手持物代理网格。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug")
    class UStaticMeshComponent* ThirdPersonHeldItemDebugMesh;

    // ======================================================================
    // 第三人称模块化部件（其他玩家可见，跟随主骨骼）
    // ======================================================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Modular Mesh")
    class USkeletalMeshComponent* HeadMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Modular Mesh")
    class USkeletalMeshComponent* HairMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Modular Mesh")
    class USkeletalMeshComponent* TopMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Modular Mesh")
    class USkeletalMeshComponent* HandMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Modular Mesh")
    class USkeletalMeshComponent* PantsMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Modular Mesh")
    class USkeletalMeshComponent* ShoesMesh;

    // ======================================================================
    // 相机参数
    // ======================================================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Crouch")
    float StandingCameraHeight = 64.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Crouch")
    float CrouchingCameraHeight = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Crouch", meta = (ClampMin = "0.0"))
    float CameraCrouchInterpSpeed = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Offset")
    FVector FirstPersonCameraLocationOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Offset")
    FRotator FirstPersonCameraRotationOffset = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anti Clip")
    bool bEnableFirstPersonCameraMovementOffset = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anti Clip", meta = (ClampMin = "0.0"))
    float FirstPersonCameraMovementOffsetSpeedThreshold = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anti Clip")
    FVector FirstPersonCameraMovementOffset = FVector(20.f, 0.f, -10.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Anti Clip", meta = (ClampMin = "0.0"))
    float FirstPersonCameraMovementOffsetInterpSpeed = 8.f;

    // ======================================================================
    // 第一人称偏移参数
    // ======================================================================

    // 第一人称上半身相对摄像机的位置偏移。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Offsets")
    FVector FirstPersonUpperBodyLocationOffset = FVector::ZeroVector;

    // 第一人称上半身相对摄像机的旋转偏移。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Offsets")
    FRotator FirstPersonUpperBodyRotationOffset = FRotator::ZeroRotator;

    // 第一人称下半身相对基础身体网格的位置偏移。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Offsets")
    FVector FirstPersonLowerBodyLocationOffset = FVector::ZeroVector;

    // 第一人称下半身相对基础身体网格的旋转偏移。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First Person|Offsets")
    FRotator FirstPersonLowerBodyRotationOffset = FRotator::ZeroRotator;

    // ======================================================================
    // 公共方法
    // ======================================================================

    UFUNCTION(BlueprintCallable, Category = "Teleport")
    void TeleportToLocation(FVector NewLocation, FRotator NewRotation);

    UFUNCTION(BlueprintCallable, Category = "Teleport")
    void TeleportToTarget(AActor* TargetActor);

    UFUNCTION(BlueprintCallable, Category = "Modular Mesh")
    void RebuildModularMeshes();

    void ShowHeldItemThirdPersonDebugMesh(APickupActor* PickupActor);
    void HideHeldItemThirdPersonDebugMesh();

    // ======================================================================
    // 移动状态
    // ======================================================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Movement")
    bool IsSquat = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Movement")
    bool IsSprint = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Movement")
    bool IsSquatThrowing = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Movement")
    bool IsStandThrowing = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "Movement")
    bool IsMiddleHandleTime = false;
};