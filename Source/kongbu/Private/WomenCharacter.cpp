// WomenCharacter.cpp
#include "WomenCharacter.h"
#include "PickupActor.h"
#include "MyPlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/staticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "WomenNativeAnimInstance.h"

namespace
{
    static const TArray<FName>& GetFirstPersonUpperMeshHiddenBoneNames()
    {
        static const TArray<FName> BoneNames =
        {
            TEXT("LeftUpLeg"),
            TEXT("LeftLeg"),
            TEXT("LeftFoot"),
            TEXT("LeftToeBase"),
            TEXT("LeftToe_End"),
            TEXT("RightUpLeg"),
            TEXT("RightLeg"),
            TEXT("RightFoot"),
            TEXT("RightToeBase"),
            TEXT("RightToe_End"),
            TEXT("Neck"),
            TEXT("Head"),
            TEXT("HeadTop_End")
        };
        return BoneNames;
    }

    static const TArray<FName>& GetFirstPersonLowerMeshHiddenBoneNames()
    {
        static const TArray<FName> BoneNames =
        {
            TEXT("Spine"),
            TEXT("Spine1"),
            TEXT("Spine2"),
            TEXT("LeftShoulder"),
            TEXT("LeftArm"),
            TEXT("LeftForeArm"),
            TEXT("LeftHand"),
            TEXT("LeftHandIndex1"),
            TEXT("LeftHandIndex2"),
            TEXT("LeftHandIndex3"),
            TEXT("LeftHandIndex4"),
            TEXT("LeftHandMiddle1"),
            TEXT("LeftHandMiddle2"),
            TEXT("LeftHandMiddle3"),
            TEXT("LeftHandMiddle4"),
            TEXT("LeftHandPinky1"),
            TEXT("LeftHandPinky2"),
            TEXT("LeftHandPinky3"),
            TEXT("LeftHandPinky4"),
            TEXT("LeftHandRing1"),
            TEXT("LeftHandRing2"),
            TEXT("LeftHandRing3"),
            TEXT("LeftHandRing4"),
            TEXT("LeftHandThumb1"),
            TEXT("LeftHandThumb2"),
            TEXT("LeftHandThumb3"),
            TEXT("LeftHandThumb4"),
            TEXT("RightShoulder"),
            TEXT("RightArm"),
            TEXT("RightForeArm"),
            TEXT("RightHand"),
            TEXT("RightSocket"),
            TEXT("RightHandIndex1"),
            TEXT("RightHandIndex2"),
            TEXT("RightHandIndex3"),
            TEXT("RightHandIndex4"),
            TEXT("RightHandMiddle1"),
            TEXT("RightHandMiddle2"),
            TEXT("RightHandMiddle3"),
            TEXT("RightHandMiddle4"),
            TEXT("RightHandPinky1"),
            TEXT("RightHandPinky2"),
            TEXT("RightHandPinky3"),
            TEXT("RightHandPinky4"),
            TEXT("RightHandRing1"),
            TEXT("RightHandRing2"),
            TEXT("RightHandRing3"),
            TEXT("RightHandRing4"),
            TEXT("RightHandThumb1"),
            TEXT("RightHandThumb2"),
            TEXT("RightHandThumb3"),
            TEXT("RightHandThumb4"),
            TEXT("Neck"),
            TEXT("Head"),
            TEXT("HeadTop_End")
        };
        return BoneNames;
    }

    static void HideBonesByNames(USkeletalMeshComponent* MeshComponent, const TArray<FName>& BoneNames)
    {
        if (!MeshComponent)
        {
            return;
        }

        for (const FName BoneName : BoneNames)
        {
            if (MeshComponent->GetBoneIndex(BoneName) != INDEX_NONE)
            {
                MeshComponent->HideBoneByName(BoneName, EPhysBodyOp::PBO_None);
            }
        }
    }

    static void CopySkeletalMeshSetup(USkeletalMeshComponent* TargetMesh, USkeletalMeshComponent* SourceMesh)
    {
        if (!TargetMesh || !SourceMesh)
        {
            return;
        }

        if (USkeletalMesh* SkeletalMesh = SourceMesh->GetSkeletalMeshAsset())
        {
            for (int32 MaterialIndex = 0; MaterialIndex < TargetMesh->GetNumMaterials(); ++MaterialIndex)
            {
                TargetMesh->SetMaterial(MaterialIndex, nullptr);
            }

            TargetMesh->SetSkeletalMeshAsset(SkeletalMesh);

            const int32 MaterialCount = SourceMesh->GetNumMaterials();
            for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
            {
                TargetMesh->SetMaterial(MaterialIndex, SourceMesh->GetMaterial(MaterialIndex));
            }
        }
    }

    // 第三人称模块化部件通用配置：挂在主骨骼下，其他玩家可见，投射阴影。
    static void ConfigureThirdPersonModularMesh(USkeletalMeshComponent* MeshComponent, USkeletalMeshComponent* ParentMesh)
    {
        if (!MeshComponent || !ParentMesh)
        {
            return;
        }

        MeshComponent->SetupAttachment(ParentMesh);
        MeshComponent->SetOnlyOwnerSee(false);
        MeshComponent->SetOwnerNoSee(true);
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        MeshComponent->SetGenerateOverlapEvents(false);
        MeshComponent->SetCastShadow(true);
        MeshComponent->bCastDynamicShadow = true;
        MeshComponent->bCastHiddenShadow = true;
    }

    // 第一人称模块化部件通用配置：挂在 FP 主骨骼下，仅本地玩家可见，不投射阴影。
    static void ConfigureFirstPersonModularMesh(USkeletalMeshComponent* MeshComponent, USkeletalMeshComponent* ParentMesh)
    {
        if (!MeshComponent || !ParentMesh)
        {
            return;
        }

        MeshComponent->SetupAttachment(ParentMesh);
        MeshComponent->SetOnlyOwnerSee(true);
        MeshComponent->SetOwnerNoSee(false);
        MeshComponent->SetCastShadow(false);
        MeshComponent->bCastDynamicShadow = false;
        MeshComponent->bCastHiddenShadow = false;
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        MeshComponent->SetGenerateOverlapEvents(false);
        MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    }
}


AWomenCharacter::AWomenCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);

    GetMesh()->SetOwnerNoSee(false);
    GetMesh()->SetHiddenInGame(false);
    GetMesh()->SetCastShadow(true);
    GetMesh()->bCastHiddenShadow = true;
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    PrimaryActorTick.bCanEverTick = true;

    // ------------------------------------------------------------------
    // 相机
    // ------------------------------------------------------------------
    FirstPersonCameraRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FirstPersonCameraRoot"));
    FirstPersonCameraRoot->SetupAttachment(GetCapsuleComponent());
    FirstPersonCameraRoot->SetRelativeLocation(FVector(0.f, 0.f, StandingCameraHeight) + FirstPersonCameraLocationOffset);
    FirstPersonCameraRoot->SetRelativeRotation(FirstPersonCameraRotationOffset);

    FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCameraComponent->SetupAttachment(FirstPersonCameraRoot);
    FirstPersonCameraComponent->SetRelativeLocation(FVector::ZeroVector);
    FirstPersonCameraComponent->SetRelativeRotation(FRotator::ZeroRotator);
    FirstPersonCameraComponent->bUsePawnControlRotation = true;

    // ------------------------------------------------------------------
    // 第一人称主骨骼（上半身，挂在相机下，跟随视角 Pitch）
    // RelativeLocation 直接就是相对摄像机的偏移，不要混入 BaseMesh 的位置。
    // ------------------------------------------------------------------
    FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
    FirstPersonMesh->SetupAttachment(FirstPersonCameraComponent);
    FirstPersonMesh->SetRelativeLocation(FirstPersonUpperBodyLocationOffset);
    FirstPersonMesh->SetRelativeRotation(FirstPersonUpperBodyRotationOffset);
    FirstPersonMesh->SetOnlyOwnerSee(true);
    FirstPersonMesh->SetOwnerNoSee(false);
    FirstPersonMesh->SetHiddenInGame(true);
    FirstPersonMesh->SetCastShadow(false);
    FirstPersonMesh->bCastDynamicShadow = false;
    FirstPersonMesh->bCastHiddenShadow = false;
    FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FirstPersonMesh->SetGenerateOverlapEvents(false);

    // ------------------------------------------------------------------
    // 第一人称下半身骨骼（挂在 Capsule 下，不跟随相机 Pitch）
    // ------------------------------------------------------------------
    FirstPersonLowerBodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonLowerBodyMesh"));
    FirstPersonLowerBodyMesh->SetupAttachment(GetCapsuleComponent());
    FirstPersonLowerBodyMesh->SetRelativeLocation(GetMesh()->GetRelativeLocation() + FirstPersonLowerBodyLocationOffset);
    FirstPersonLowerBodyMesh->SetRelativeRotation(GetMesh()->GetRelativeRotation() + FirstPersonLowerBodyRotationOffset);
    FirstPersonLowerBodyMesh->SetRelativeScale3D(GetMesh()->GetRelativeScale3D());
    FirstPersonLowerBodyMesh->SetOnlyOwnerSee(true);
    FirstPersonLowerBodyMesh->SetOwnerNoSee(false);
    FirstPersonLowerBodyMesh->SetHiddenInGame(true);
    FirstPersonLowerBodyMesh->SetCastShadow(false);
    FirstPersonLowerBodyMesh->bCastDynamicShadow = false;
    FirstPersonLowerBodyMesh->bCastHiddenShadow = false;
    FirstPersonLowerBodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FirstPersonLowerBodyMesh->SetGenerateOverlapEvents(false);
    FirstPersonLowerBodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    // ------------------------------------------------------------------
    // 第一人称模块化部件（挂在 FirstPersonMesh 下，跟随 FP 骨骼）
    // 编译后在蓝图 Details 面板直接指定 Skeletal Mesh 资产即可。
    // ------------------------------------------------------------------
    FPArmsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FPArmsMesh"));
    ConfigureFirstPersonModularMesh(FPArmsMesh, FirstPersonMesh);

    FPTopMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FPTopMesh"));
    ConfigureFirstPersonModularMesh(FPTopMesh, FirstPersonMesh);

    FPHandMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FPHandMesh"));
    ConfigureFirstPersonModularMesh(FPHandMesh, FirstPersonMesh);

    // ------------------------------------------------------------------
    // 第三人称模块化部件（挂在主骨骼下，其他玩家可见）
    // ------------------------------------------------------------------
    HeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeadMesh"));
    ConfigureThirdPersonModularMesh(HeadMesh, GetMesh());

    HairMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HairMesh"));
    ConfigureThirdPersonModularMesh(HairMesh, GetMesh());

    TopMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Topmesh"));
    ConfigureThirdPersonModularMesh(TopMesh, GetMesh());

    HandMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandMesh"));
    ConfigureThirdPersonModularMesh(HandMesh, GetMesh());

    PantsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PantsMesh"));
    ConfigureThirdPersonModularMesh(PantsMesh, GetMesh());

    ShoesMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ShoesMesh"));
    ConfigureThirdPersonModularMesh(ShoesMesh, GetMesh());

    // ------------------------------------------------------------------
    // 调试用手持物代理网格
    // ------------------------------------------------------------------
    ThirdPersonHeldItemDebugMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThirdPersonHeldItemDebugMesh"));
    ThirdPersonHeldItemDebugMesh->SetupAttachment(GetMesh());
    ThirdPersonHeldItemDebugMesh->SetOnlyOwnerSee(true);
    ThirdPersonHeldItemDebugMesh->SetOwnerNoSee(false);
    ThirdPersonHeldItemDebugMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ThirdPersonHeldItemDebugMesh->SetGenerateOverlapEvents(false);
    ThirdPersonHeldItemDebugMesh->SetCastShadow(false);
    ThirdPersonHeldItemDebugMesh->bCastDynamicShadow = false;
    ThirdPersonHeldItemDebugMesh->bCastHiddenShadow = false;
    ThirdPersonHeldItemDebugMesh->SetHiddenInGame(true);

    // ------------------------------------------------------------------
    // 持物挂点
    // ------------------------------------------------------------------
    HoldPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HoldPoint"));
    HoldPoint->SetupAttachment(FirstPersonCameraComponent);
    HoldPoint->SetRelativeLocation(FVector(80.f, 20.f, -15.f));
}

void AWomenCharacter::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshModularMeshes();
}

void AWomenCharacter::BeginPlay()
{
    Super::BeginPlay();
    ThirdPersonMeshBaseRelativeRotation = GetMesh() ? GetMesh()->GetRelativeRotation() : FRotator::ZeroRotator;
    RefreshModularMeshes();
}

void AWomenCharacter::TeleportToLocation(FVector NewLocation, FRotator NewRotation)
{
    SetActorLocation(NewLocation);
    SetActorRotation(NewRotation);
}

void AWomenCharacter::TeleportToTarget(AActor* TargetActor)
{
    if (TargetActor)
    {
        TeleportTo(TargetActor->GetActorLocation(), TargetActor->GetActorRotation());
    }
}

void AWomenCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!FirstPersonCameraRoot)
    {
        return;
    }

    FVector CameraRelativeLocation = FirstPersonCameraLocationOffset;
    const float TargetCameraHeight = IsSquat ? CrouchingCameraHeight : StandingCameraHeight;

    if (CameraCrouchInterpSpeed <= 0.f)
    {
        CameraRelativeLocation.Z += TargetCameraHeight;
    }
    else
    {
        const float CurrentCameraHeight = FirstPersonCameraRoot->GetRelativeLocation().Z
            - FirstPersonCameraLocationOffset.Z
            - SmoothedFirstPersonCameraMovementOffset.Z;

        CameraRelativeLocation.Z = FMath::FInterpTo(
            CurrentCameraHeight,
            TargetCameraHeight,
            DeltaTime,
            CameraCrouchInterpSpeed
        ) + FirstPersonCameraLocationOffset.Z;
    }

    CameraRelativeLocation += CalculateFirstPersonCameraMovementOffset(DeltaTime);

    FirstPersonCameraRoot->SetRelativeLocation(CameraRelativeLocation);
    FirstPersonCameraRoot->SetRelativeRotation(FirstPersonCameraRotationOffset);

    UpdateThirdPersonDiagonalYaw(DeltaTime);

#if WITH_EDITOR || UE_BUILD_DEVELOPMENT
    Debug_TickOffsetTuning();
#endif
}


void AWomenCharacter::UpdateThirdPersonDiagonalYaw(float DeltaTime)
{
    USkeletalMeshComponent* ThirdPersonMesh = GetMesh();
    if (!ThirdPersonMesh)
    {
        return;
    }

    float TargetYaw = 0.f;

    if (bEnableThirdPersonDiagonalYaw)
    {
        const FVector LocalVelocity = GetActorTransform().InverseTransformVectorNoScale(GetVelocity());
        const float ForwardSpeed = LocalVelocity.X;
        const float RightSpeed = LocalVelocity.Y;

        if (ForwardSpeed > ThirdPersonDiagonalYawMinForwardSpeed && FMath::Abs(RightSpeed) > ThirdPersonDiagonalYawMinForwardSpeed)
        {
            TargetYaw = RightSpeed > 0.f ? ThirdPersonDiagonalYawAngle : -ThirdPersonDiagonalYawAngle;
        }
    }

    if (ThirdPersonDiagonalYawInterpSpeed <= 0.f)
    {
        SmoothedThirdPersonDiagonalYaw = TargetYaw;
    }
    else
    {
        SmoothedThirdPersonDiagonalYaw = FMath::FInterpTo(
            SmoothedThirdPersonDiagonalYaw,
            TargetYaw,
            DeltaTime,
            ThirdPersonDiagonalYawInterpSpeed);
    }

    FRotator TargetRelativeRotation = ThirdPersonMeshBaseRelativeRotation;
    TargetRelativeRotation.Yaw += SmoothedThirdPersonDiagonalYaw;
    ThirdPersonMesh->SetRelativeRotation(TargetRelativeRotation);

    for (USkeletalMeshComponent* ModularMesh : GetThirdPersonModularMeshes())
    {
        if (!ModularMesh || ModularMesh->GetAttachParent() == ThirdPersonMesh)
        {
            continue;
        }

        ModularMesh->SetRelativeRotation(TargetRelativeRotation);
    }
}

TArray<USkeletalMeshComponent*> AWomenCharacter::GetThirdPersonModularMeshes() const
{
    return
    {
        HeadMesh,
        HairMesh,
        TopMesh,
        PantsMesh,
        ShoesMesh,
        HandMesh
    };
}


#if WITH_EDITOR || UE_BUILD_DEVELOPMENT
void AWomenCharacter::Debug_TickOffsetTuning()
{
    if (!IsLocallyControlled()) return;

    AMyPlayerController* PC = Cast<AMyPlayerController>(GetController());
    APickupActor* HeldItem = PC ? PC->GetHeldActor() : nullptr;
    if (!HeldItem) return;

    const float Step    = 1.0f;
    const float RotStep = 5.0f;

    bool bChanged = false;
    FVector  DeltaLoc = FVector::ZeroVector;
    FRotator DeltaRot = FRotator::ZeroRotator;

    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::F)) { DeltaLoc.X += Step;  bChanged = true; }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::G))   { DeltaLoc.X -= Step;  bChanged = true; }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::H))  { DeltaLoc.Y -= Step;  bChanged = true; }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::J))   { DeltaLoc.Y += Step;  bChanged = true; }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::K)) { DeltaLoc.Z += Step;  bChanged = true; }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::L))  { DeltaLoc.Z -= Step;  bChanged = true; }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::T)) { DeltaRot.Yaw   += RotStep; bChanged = true; }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::Y)) { DeltaRot.Yaw   -= RotStep; bChanged = true; }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::U)) { DeltaRot.Pitch += RotStep; bChanged = true; }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::I)) { DeltaRot.Pitch -= RotStep; bChanged = true; }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::O)) { DeltaRot.Roll  += RotStep; bChanged = true; }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::P)) { DeltaRot.Roll  -= RotStep; bChanged = true; }

    if (!bChanged) return;

    HeldItem->Debug_AddFPLocationOffset(DeltaLoc);
    HeldItem->Debug_AddFPRotationOffset(DeltaRot);
    HeldItem->SetActorRelativeLocation(HeldItem->Debug_GetFPLocationOffset());
    HeldItem->SetActorRelativeRotation(HeldItem->Debug_GetFPRotationOffset());

    if (GEngine)
    {
        const FVector  Loc = HeldItem->Debug_GetFPLocationOffset();
        const FRotator Rot = HeldItem->Debug_GetFPRotationOffset();
        GEngine->AddOnScreenDebugMessage(42, 3.f, FColor::Cyan,
            FString::Printf(
                TEXT("Loc: X=%.1f Y=%.1f Z=%.1f | Rot: P=%.1f Y=%.1f R=%.1f"),
                Loc.X, Loc.Y, Loc.Z, Rot.Roll, Rot.Pitch, Rot.Yaw
            )
        );
    }
}
#endif

FVector AWomenCharacter::CalculateFirstPersonCameraMovementOffset(float DeltaTime)
{
    const float MoveSpeed = GetVelocity().Size2D();
    const FVector DesiredOffset = bEnableFirstPersonCameraMovementOffset
        && MoveSpeed > FirstPersonCameraMovementOffsetSpeedThreshold
        ? FirstPersonCameraMovementOffset
        : FVector::ZeroVector;

    if (FirstPersonCameraMovementOffsetInterpSpeed <= 0.f)
    {
        SmoothedFirstPersonCameraMovementOffset = DesiredOffset;
    }
    else
    {
        SmoothedFirstPersonCameraMovementOffset = FMath::VInterpTo(
            SmoothedFirstPersonCameraMovementOffset,
            DesiredOffset,
            DeltaTime,
            FirstPersonCameraMovementOffsetInterpSpeed);
    }

    return SmoothedFirstPersonCameraMovementOffset;
}

void AWomenCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AWomenCharacter, IsSquat);
    DOREPLIFETIME(AWomenCharacter, IsSprint);
    DOREPLIFETIME(AWomenCharacter, IsSquatThrowing);
    DOREPLIFETIME(AWomenCharacter, IsStandThrowing);
    DOREPLIFETIME(AWomenCharacter, IsMiddleHandleTime);
    DOREPLIFETIME(AWomenCharacter, bIsSoulSucked);
    DOREPLIFETIME(AWomenCharacter, bIsKnockedDown);
}
void AWomenCharacter::StartSoulSuckReaction()
{
    GetWorldTimerManager().ClearTimer(ForcedReactionTimerHandle);

    bIsSoulSucked = true;
    bIsKnockedDown = false;
    IsMiddleHandleTime = false;
    IsSquatThrowing = false;
    IsStandThrowing = false;
    IsSprint = false;

    if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
    {
        MovementComponent->StopMovementImmediately();
        MovementComponent->DisableMovement();
    }

    MulticastPlaySoulSuckReactionAnimation();
}
void AWomenCharacter::InterruptSoulSuckWithKnockdown(float KnockdownDuration)
{
    GetWorldTimerManager().ClearTimer(ForcedReactionTimerHandle);

    bIsSoulSucked = false;
    bIsKnockedDown = true;
    IsMiddleHandleTime = false;
    IsSquatThrowing = false;
    IsStandThrowing = false;
    IsSprint = false;

    if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
    {
        MovementComponent->StopMovementImmediately();
        MovementComponent->DisableMovement();
    }

    MulticastPlayKnockdownReactionAnimation();

    const float ResolvedDuration = KnockdownDuration > 0.f ? KnockdownDuration : ResolveKnockdownReactionDuration();
    if (ResolvedDuration > 0.f)
    {
        GetWorldTimerManager().SetTimer(ForcedReactionTimerHandle, this, &AWomenCharacter::ClearForcedReactionState, ResolvedDuration, false);
    }
    else
    {
        ClearForcedReactionState();
    }
}
void AWomenCharacter::ClearForcedReactionState()
{
    GetWorldTimerManager().ClearTimer(ForcedReactionTimerHandle);
    bIsSoulSucked = false;
    bIsKnockedDown = false;

    if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
    {
        MovementComponent->SetMovementMode(MOVE_Walking);
    }
}

void AWomenCharacter::MulticastPlaySoulSuckReactionAnimation_Implementation()
{
    if (UWomenNativeAnimInstance* AnimInstance = ResolveWomenNativeAnimInstance())
    {
        AnimInstance->PlaySoulSuckedAction();
    }
}

void AWomenCharacter::MulticastPlayKnockdownReactionAnimation_Implementation()
{
    if (UWomenNativeAnimInstance* AnimInstance = ResolveWomenNativeAnimInstance())
    {
        AnimInstance->PlayKnockdownAction();
    }
}

UWomenNativeAnimInstance* AWomenCharacter::ResolveWomenNativeAnimInstance() const
{
    if (!GetMesh())
    {
        return nullptr;
    }

    return Cast<UWomenNativeAnimInstance>(GetMesh()->GetAnimInstance());
}

float AWomenCharacter::ResolveKnockdownReactionDuration() const
{
    if (const UWomenNativeAnimInstance* AnimInstance = ResolveWomenNativeAnimInstance())
    {
        return AnimInstance->GetKnockdownActionDuration();
    }

    return 0.f;
}
void AWomenCharacter::RefreshModularMeshes()
{
    USkeletalMeshComponent* BaseMesh = GetMesh();
    if (!BaseMesh)
    {
        return;
    }

    // ------------------------------------------------------------------
    // 第三人称主骨骼
    // ------------------------------------------------------------------
    BaseMesh->SetOnlyOwnerSee(false);
    BaseMesh->SetOwnerNoSee(false);
    BaseMesh->SetHiddenInGame(false);
    BaseMesh->SetCastShadow(true);
    BaseMesh->bCastDynamicShadow = true;
    BaseMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    // ------------------------------------------------------------------
    // 第一人称上半身主骨骼
    // 挂在 Camera 下，RelativeLocation 就是相对摄像机的偏移，
    // 不要用 BaseMesh->GetRelativeLocation() 做基准（父节点不同）。
    // ------------------------------------------------------------------
    if (FirstPersonMesh)
    {
        FirstPersonMesh->SetOnlyOwnerSee(true);
        FirstPersonMesh->SetOwnerNoSee(false);
        FirstPersonMesh->SetHiddenInGame(false);

        if (!FirstPersonMesh->GetSkeletalMeshAsset())
        {
            CopySkeletalMeshSetup(FirstPersonMesh, BaseMesh);
        }

        FirstPersonMesh->SetRelativeLocation(FirstPersonUpperBodyLocationOffset);
        FirstPersonMesh->SetRelativeRotation(FirstPersonUpperBodyRotationOffset);
        FirstPersonMesh->SetRelativeScale3D(BaseMesh->GetRelativeScale3D());
        HideBonesByNames(FirstPersonMesh, GetFirstPersonUpperMeshHiddenBoneNames());
    }

    // ------------------------------------------------------------------
    // 第一人称下半身骨骼
    // ------------------------------------------------------------------
    if (FirstPersonLowerBodyMesh)
    {
        if (!FirstPersonLowerBodyMesh->GetSkeletalMeshAsset())
        {
            if (FirstPersonMesh && FirstPersonMesh->GetSkeletalMeshAsset())
            {
                CopySkeletalMeshSetup(FirstPersonLowerBodyMesh, FirstPersonMesh);
            }
            else
            {
                CopySkeletalMeshSetup(FirstPersonLowerBodyMesh, BaseMesh);
            }
        }

        FirstPersonLowerBodyMesh->SetOnlyOwnerSee(true);
        FirstPersonLowerBodyMesh->SetOwnerNoSee(false);
        FirstPersonLowerBodyMesh->SetHiddenInGame(false);
        FirstPersonLowerBodyMesh->SetRelativeLocation(BaseMesh->GetRelativeLocation() + FirstPersonLowerBodyLocationOffset);
        FirstPersonLowerBodyMesh->SetRelativeRotation(BaseMesh->GetRelativeRotation() + FirstPersonLowerBodyRotationOffset);
        FirstPersonLowerBodyMesh->SetRelativeScale3D(BaseMesh->GetRelativeScale3D());
        FirstPersonLowerBodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        FirstPersonLowerBodyMesh->SetLeaderPoseComponent(BaseMesh);
        HideBonesByNames(FirstPersonLowerBodyMesh, GetFirstPersonLowerMeshHiddenBoneNames());
    }

    // ------------------------------------------------------------------
    // 第一人称模块化部件（跟随 FirstPersonMesh 骨骼）
    // ------------------------------------------------------------------
    TArray<USkeletalMeshComponent*> FPModularMeshes =
    {
        FPArmsMesh,
        FPTopMesh,
        FPHandMesh
    };

    for (USkeletalMeshComponent* FPMesh : FPModularMeshes)
    {
        if (!FPMesh)
        {
            continue;
        }

        FPMesh->SetOnlyOwnerSee(true);
        FPMesh->SetOwnerNoSee(false);
        FPMesh->SetHiddenInGame(false);
        FPMesh->SetLeaderPoseComponent(FirstPersonMesh);
        FPMesh->SetRelativeLocation(FVector::ZeroVector);
        FPMesh->SetRelativeRotation(FRotator::ZeroRotator);
        FPMesh->SetRelativeScale3D(FVector::OneVector);
    }

    // ------------------------------------------------------------------
    // 第三人称模块化部件（跟随主骨骼）
    // ------------------------------------------------------------------

    for (USkeletalMeshComponent* TPMesh : GetThirdPersonModularMeshes())
    {
        if (!TPMesh)
        {
            continue;
        }

        if (TPMesh->GetAttachParent() != BaseMesh)
        {
            TPMesh->AttachToComponent(BaseMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        }

        TPMesh->SetOnlyOwnerSee(false);
        TPMesh->SetOwnerNoSee(false);
        TPMesh->SetHiddenInGame(false);
        TPMesh->SetLeaderPoseComponent(BaseMesh);
        TPMesh->SetRelativeLocation(FVector::ZeroVector);
        TPMesh->SetRelativeRotation(FRotator::ZeroRotator);
        TPMesh->SetRelativeScale3D(FVector::OneVector);
    }
}

void AWomenCharacter::RebuildModularMeshes()
{
    RefreshModularMeshes();
}

// ==============================================================================
// 第三人称调试用手持物代理网格
// ==============================================================================

void AWomenCharacter::ShowHeldItemThirdPersonDebugMesh(APickupActor* PickupActor)
{
    if (!ThirdPersonHeldItemDebugMesh || !PickupActor || !GetMesh())
    {
        return;
    }

    UStaticMeshComponent* PickupMeshComponent = PickupActor->GetPickupMeshComponent();
    if (!PickupMeshComponent)
    {
        HideHeldItemThirdPersonDebugMesh();
        return;
    }

    ThirdPersonHeldItemDebugMesh->SetStaticMesh(PickupMeshComponent->GetStaticMesh());
    ThirdPersonHeldItemDebugMesh->EmptyOverrideMaterials();

    const int32 MaterialCount = PickupMeshComponent->GetNumMaterials();
    for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
    {
        ThirdPersonHeldItemDebugMesh->SetMaterial(MaterialIndex, PickupMeshComponent->GetMaterial(MaterialIndex));
    }

    ThirdPersonHeldItemDebugMesh->AttachToComponent(
        GetMesh(),
        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        PickupActor->GetThirdPersonSocketName());
    ThirdPersonHeldItemDebugMesh->SetRelativeLocation(PickupActor->GetThirdPersonLocationOffset());
    ThirdPersonHeldItemDebugMesh->SetRelativeRotation(PickupActor->GetThirdPersonRotationOffset());
    ThirdPersonHeldItemDebugMesh->SetRelativeScale3D(PickupMeshComponent->GetRelativeScale3D());
    ThirdPersonHeldItemDebugMesh->SetHiddenInGame(false);
}

void AWomenCharacter::HideHeldItemThirdPersonDebugMesh()
{
    if (!ThirdPersonHeldItemDebugMesh)
    {
        return;
    }

    ThirdPersonHeldItemDebugMesh->SetStaticMesh(nullptr);
    ThirdPersonHeldItemDebugMesh->EmptyOverrideMaterials();
    ThirdPersonHeldItemDebugMesh->SetHiddenInGame(true);
}