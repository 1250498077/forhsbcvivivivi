// WomenCharacter.cpp
#include "WomenCharacter.h"
#include "PickupActor.h"
#include "MyPlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{

    static void CopySkeletalMeshSetup(USkeletalMeshComponent *TargetMesh, USkeletalMeshComponent *SourceMesh)
    {
        if (!TargetMesh || !SourceMesh)
        {
            return;
        }

        if (USkeletalMesh *SkeletalMesh = SourceMesh->GetSkeletalMeshAsset())
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

    static void ConfigureModularMeshComponent(USkeletalMeshComponent *MeshComponent, USkeletalMeshComponent *ParentMesh)
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

    static void ConfigureFirstPersonModularMeshComponent(USkeletalMeshComponent *MeshComponent, USkeletalMeshComponent *ParentMesh)
    {
        if (!MeshComponent || !ParentMesh)
        {
            return;
        }

        MeshComponent->SetupAttachment(ParentMesh);
        MeshComponent->SetOnlyOwnerSee(true);
        MeshComponent->SetOwnerNoSee(false);
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        MeshComponent->SetGenerateOverlapEvents(false);
        MeshComponent->SetCastShadow(false);
        MeshComponent->bCastDynamicShadow = false;
        MeshComponent->bCastHiddenShadow = false;
        MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    }

}

AWomenCharacter::AWomenCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);

    // Third-person full body: other players can see it,
    // but it still casts the owner's full-body shadow.
    GetMesh()->SetOwnerNoSee(false);
    GetMesh()->SetHiddenInGame(!bShowBaseThirdPersonMesh);
    GetMesh()->SetCastShadow(bShowBaseThirdPersonMesh);
    GetMesh()->bCastHiddenShadow = false;

    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    PrimaryActorTick.bCanEverTick = true;

    FirstPersonCameraRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FirstPersonCameraRoot"));
    FirstPersonCameraRoot->SetupAttachment(FirstPersonCameraComponent);
    FirstPersonCameraRoot->SetRelativeLocation(FVector(0.f, 0.f, StandingCameraHeight) + FirstPersonCameraLocationOffset);
    FirstPersonCameraRoot->SetRelativeRotation(FirstPersonCameraRotationOffset);

    FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCameraComponent->SetupAttachment(FirstPersonCameraRoot);

    FirstPersonCameraComponent->SetRelativeLocation(FVector::ZeroVector);
    FirstPersonCameraComponent->SetRelativeRotation(FRotator::ZeroRotator);

    FirstPersonCameraComponent->bUsePawnControlRotation = true;

    // 第一人称主骨架：挂在相机下，作为第一人称模块化部件的姿态驱动源。
    FirstPersonMeshRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FirstPersonMeshRoot"));
    FirstPersonMeshRoot->SetupAttachment(GetCapsuleComponent());
    FirstPersonMeshRoot->SetRelativeLocation(FirstPersonMeshLocationOffset);
    FirstPersonMeshRoot->SetRelativeRotation(FirstPersonMeshRotationOffset);
    FirstPersonMeshRoot->SetRelativeScale3D(FVector::OneVector);

    // 第一人称主骨架：挂在相机下，作为第一人称模块化部件的姿态驱动源。
    FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
    FirstPersonMesh->SetupAttachment(FirstPersonMeshRoot);
    FirstPersonMesh->SetRelativeLocation(FVector::ZeroVector);
    FirstPersonMesh->SetRelativeRotation(FRotator::ZeroRotator);
    FirstPersonMesh->SetRelativeScale3D(FVector::OneVector);
    FirstPersonMesh->SetOnlyOwnerSee(true);
    FirstPersonMesh->SetOwnerNoSee(false);
    FirstPersonMesh->SetHiddenInGame(false);
    FirstPersonMesh->SetCastShadow(false);
    FirstPersonMesh->bCastDynamicShadow = false;
    FirstPersonMesh->bCastHiddenShadow = false;
    FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FirstPersonMesh->SetGenerateOverlapEvents(false);

    FirstPersonHeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonHeadMesh"));
    ConfigureFirstPersonModularMeshComponent(FirstPersonHeadMesh, FirstPersonMesh);

    FirstPersonHairMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonHairMesh"));
    ConfigureFirstPersonModularMeshComponent(FirstPersonHairMesh, FirstPersonMesh);

    FirstPersonTopMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonTopMesh"));
    ConfigureFirstPersonModularMeshComponent(FirstPersonTopMesh, FirstPersonMesh);

    FirstPersonHandMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonHandMesh"));
    ConfigureFirstPersonModularMeshComponent(FirstPersonHandMesh, FirstPersonMesh);

    FirstPersonPantsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonPantsMesh"));
    ConfigureFirstPersonModularMeshComponent(FirstPersonPantsMesh, FirstPersonMesh);

    FirstPersonShoesMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonShoesMesh"));
    ConfigureFirstPersonModularMeshComponent(FirstPersonShoesMesh, FirstPersonMesh);

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

    HeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeadMesh"));
    ConfigureModularMeshComponent(HeadMesh, GetMesh());

    HairMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HairMesh"));
    ConfigureModularMeshComponent(HairMesh, GetMesh());

    TopMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Topmesh"));
    ConfigureModularMeshComponent(TopMesh, GetMesh());

    HandMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HandMesh"));
    ConfigureModularMeshComponent(HandMesh, GetMesh());

    PantsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PantsMesh"));
    ConfigureModularMeshComponent(PantsMesh, GetMesh());

    ShoesMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ShoesMesh"));
    ConfigureModularMeshComponent(ShoesMesh, GetMesh());

    HoldPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HoldPoint"));
    HoldPoint->SetupAttachment(FirstPersonCameraComponent);

    HoldPoint->SetRelativeLocation(FVector(80.f, 20.f, -15.f));
}

void AWomenCharacter::OnConstruction(const FTransform &Transform)
{
    Super::OnConstruction(Transform);

    UpdateFirstPersonCameraTransform(0.f, false);
    RefreshModularMeshes();
}

void AWomenCharacter::TeleportToLocation(FVector NewLocation, FRotator NewRotation)
{
    SetActorLocation(NewLocation);
    SetActorRotation(NewRotation);
}

void AWomenCharacter::TeleportToTarget(AActor *TargetActor)
{
    if (TargetActor)
    {
        FVector TargetLocation = TargetActor->GetActorLocation();
        FRotator TargetRotation = TargetActor->GetActorRotation();
        TeleportTo(TargetLocation, TargetRotation);
    }
}

void AWomenCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (FirstPersonMeshRoot)
    {
        FirstPersonMeshLocationOffset = FirstPersonMeshRoot->GetRelativeLocation();
        FirstPersonMeshRotationOffset = FirstPersonMeshRoot->GetRelativeRotation();
    }

    UpdateFirstPersonCameraTransform(0.f, false);
    RefreshModularMeshes();
}

void AWomenCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateFirstPersonCameraTransform(DeltaTime, true);

    #if WITH_EDITOR || UE_BUILD_DEVELOPMENT
        Debug_TickOffsetTuning();
    #endif
}

void AWomenCharacter::UpdateFirstPersonCameraTransform(float DeltaTime, bool bUseRuntimeSmoothing)
{

    if (!FirstPersonCameraRoot)
    {
        return;
    }

    FVector CameraRelativeLocation = FirstPersonCameraLocationOffset;
    const float TargetCameraHeight = IsSquat ? CrouchingCameraHeight : StandingCameraHeight;

    if (!bUseRuntimeSmoothing || CameraCrouchInterpSpeed <= 0.f)
    {
        CameraRelativeLocation.Z += TargetCameraHeight;
    }
    else
    {
        const float CurrentCameraHeight = FirstPersonCameraRoot->GetRelativeLocation().Z - FirstPersonCameraLocationOffset.Z - SmoothedFirstPersonCameraMovementOffset.Z;
        CameraRelativeLocation.Z = FMath::FInterpTo(
                                       CurrentCameraHeight,
                                       TargetCameraHeight,
                                       DeltaTime,
                                       CameraCrouchInterpSpeed) +
                                   FirstPersonCameraLocationOffset.Z;
    }

    if (bUseRuntimeSmoothing)
    {
        CameraRelativeLocation += CalculateFirstPersonCameraMovementOffset(DeltaTime);
    }
    else
    {
        SmoothedFirstPersonCameraMovementOffset = FVector::ZeroVector;
    }

    FirstPersonCameraRoot->SetRelativeLocation(CameraRelativeLocation);
    FirstPersonCameraRoot->SetRelativeRotation(FirstPersonCameraRotationOffset);
    // UpdateFirstPersonMeshRootTransform();
    if (GetWorld() && GetWorld()->IsGameWorld())
    {
        UpdateFirstPersonMeshRootTransform();
    }
}

void AWomenCharacter::UpdateFirstPersonMeshRootTransform()
{
    if (!FirstPersonMeshRoot || !FirstPersonCameraComponent)
    {
        return;
    }

    if (FirstPersonMeshRoot->GetAttachParent() != FirstPersonCameraComponent)
    {
        FirstPersonMeshRoot->AttachToComponent(FirstPersonCameraComponent, FAttachmentTransformRules::KeepRelativeTransform);
    }

    FirstPersonMeshRoot->SetRelativeLocation(FirstPersonMeshLocationOffset);
    FirstPersonMeshRoot->SetRelativeRotation(FirstPersonMeshRotationOffset);
    FirstPersonMeshRoot->SetRelativeScale3D(FVector::OneVector);
}


void AWomenCharacter::CaptureCurrentFirstPersonMeshRootOffset()
{
    if (!FirstPersonMeshRoot)
    {
        return;
    }

#if WITH_EDITOR
    Modify();
#endif

    FirstPersonMeshLocationOffset = FirstPersonMeshRoot->GetRelativeLocation();
    FirstPersonMeshRotationOffset = FirstPersonMeshRoot->GetRelativeRotation();

#if WITH_EDITOR
    MarkPackageDirty();
#endif
}

void AWomenCharacter::PreviewFirstPersonMeshRootOffset()
{
    UpdateFirstPersonMeshRootTransform();
}

#if WITH_EDITOR || UE_BUILD_DEVELOPMENT
void AWomenCharacter::Debug_TickOffsetTuning()
{
    if (!IsLocallyControlled())
        return;

    AMyPlayerController *PC = Cast<AMyPlayerController>(GetController());
    APickupActor *HeldItem = PC ? PC->GetHeldActor() : nullptr;
    if (!HeldItem)
        return;

    const float Step = 1.0f;
    const float RotStep = 5.0f;

    bool bChanged = false;
    FVector DeltaLoc = FVector::ZeroVector;
    FRotator DeltaRot = FRotator::ZeroRotator;

    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::NumPadEight))
    {
        DeltaLoc.X += Step;
        bChanged = true;
    }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::NumPadTwo))
    {
        DeltaLoc.X -= Step;
        bChanged = true;
    }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::NumPadFour))
    {
        DeltaLoc.Y -= Step;
        bChanged = true;
    }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::NumPadSix))
    {
        DeltaLoc.Y += Step;
        bChanged = true;
    }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::NumPadSeven))
    {
        DeltaLoc.Z += Step;
        bChanged = true;
    }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::NumPadNine))
    {
        DeltaLoc.Z -= Step;
        bChanged = true;
    }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::T))
    {
        DeltaRot.Yaw += RotStep;
        bChanged = true;
    }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::Y))
    {
        DeltaRot.Yaw -= RotStep;
        bChanged = true;
    }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::U))
    {
        DeltaRot.Pitch += RotStep;
        bChanged = true;
    }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::I))
    {
        DeltaRot.Pitch -= RotStep;
        bChanged = true;
    }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::O))
    {
        DeltaRot.Roll += RotStep;
        bChanged = true;
    }
    if (GetWorld()->GetFirstPlayerController()->IsInputKeyDown(EKeys::P))
    {
        DeltaRot.Roll -= RotStep;
        bChanged = true;
    }

    if (!bChanged)
        return;

    // ===== 第四步的内容在这里 =====
    HeldItem->Debug_AddFPLocationOffset(DeltaLoc);
    HeldItem->Debug_AddFPRotationOffset(DeltaRot);

    HeldItem->SetActorRelativeLocation(HeldItem->Debug_GetFPLocationOffset());
    HeldItem->SetActorRelativeRotation(HeldItem->Debug_GetFPRotationOffset());

    if (GEngine)
    {
        const FVector Loc = HeldItem->Debug_GetFPLocationOffset();
        const FRotator Rot = HeldItem->Debug_GetFPRotationOffset();
        GEngine->AddOnScreenDebugMessage(42, 3.f, FColor::Cyan,
                                         FString::Printf(
                                             TEXT("Loc: X=%.1f Y=%.1f Z=%.1f | Rot: P=%.1f Y=%.1f R=%.1f"),
                                             Loc.X, Loc.Y, Loc.Z, Rot.Roll, Rot.Pitch, Rot.Yaw));
    }
}
#endif

FVector AWomenCharacter::CalculateFirstPersonCameraMovementOffset(float DeltaTime)
{
    const float MoveSpeed = GetVelocity().Size2D();
    const FVector DesiredOffset = bEnableFirstPersonCameraMovementOffset && MoveSpeed > FirstPersonCameraMovementOffsetSpeedThreshold
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

void AWomenCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AWomenCharacter, IsSquat);
    DOREPLIFETIME(AWomenCharacter, IsSprint);
    DOREPLIFETIME(AWomenCharacter, IsSquatThrowing);
    DOREPLIFETIME(AWomenCharacter, IsStandThrowing);
    DOREPLIFETIME(AWomenCharacter, IsMiddleHandleTime);
}

void AWomenCharacter::RefreshModularMeshes()
{
    USkeletalMeshComponent *BaseMesh = GetMesh();
    if (!BaseMesh)
    {
        return;
    }

    BaseMesh->SetOnlyOwnerSee(false);
    BaseMesh->SetOwnerNoSee(false);
    BaseMesh->SetHiddenInGame(!bShowBaseThirdPersonMesh);
    BaseMesh->SetCastShadow(bShowBaseThirdPersonMesh);
    BaseMesh->bCastDynamicShadow = bShowBaseThirdPersonMesh;
    BaseMesh->bCastHiddenShadow = false;
    BaseMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

    // if (FirstPersonMeshRoot)
    // {
    //     FirstPersonMeshRoot->SetRelativeLocation(BaseMesh->GetRelativeLocation() + FirstPersonMeshLocationOffset);
    //     FirstPersonMeshRoot->SetRelativeRotation(BaseMesh->GetRelativeRotation() + FirstPersonMeshRotationOffset);
    //     FirstPersonMeshRoot->SetRelativeScale3D(BaseMesh->GetRelativeScale3D());
    // }

    if (FirstPersonMesh)
    {
        FirstPersonMesh->SetOnlyOwnerSee(true);
        FirstPersonMesh->SetOwnerNoSee(false);
        FirstPersonMesh->SetHiddenInGame(false);
        FirstPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

        if (bCopyThirdPersonMeshToFirstPersonMesh && !FirstPersonMesh->GetSkeletalMeshAsset())
        {
            CopySkeletalMeshSetup(FirstPersonMesh, BaseMesh);
        }

        FirstPersonMesh->SetLeaderPoseComponent(bUseThirdPersonPoseForFirstPersonMesh ? BaseMesh : nullptr);

        if (GetWorld() && GetWorld()->IsGameWorld())
        {
            FirstPersonMesh->SetRelativeLocation(FVector::ZeroVector);
            FirstPersonMesh->SetRelativeRotation(FRotator::ZeroRotator);
            FirstPersonMesh->SetRelativeScale3D(FVector::OneVector);
        }
    }

    TArray<USkeletalMeshComponent *> FirstPersonModularMeshes =
        {
            FirstPersonHeadMesh,
            FirstPersonHairMesh,
            FirstPersonTopMesh,
            FirstPersonHandMesh,
            FirstPersonPantsMesh,
            FirstPersonShoesMesh};

    for (USkeletalMeshComponent *FirstPersonModularMesh : FirstPersonModularMeshes)
    {
        if (!FirstPersonModularMesh || !FirstPersonMesh)
        {
            continue;
        }

        FirstPersonModularMesh->SetOnlyOwnerSee(true);
        FirstPersonModularMesh->SetOwnerNoSee(false);
        FirstPersonModularMesh->SetHiddenInGame(false);
        FirstPersonModularMesh->SetLeaderPoseComponent(FirstPersonMesh);
        FirstPersonModularMesh->SetRelativeLocation(FVector::ZeroVector);
        FirstPersonModularMesh->SetRelativeRotation(FRotator::ZeroRotator);
        FirstPersonModularMesh->SetRelativeScale3D(FVector::OneVector);
    }

    struct FFirstPersonModularMeshPair
    {
        USkeletalMeshComponent *FirstPersonComponent;
        USkeletalMeshComponent *ThirdPersonComponent;
    };

    const TArray<FFirstPersonModularMeshPair> FirstPersonModularMeshPairs =
        {
            {FirstPersonHeadMesh, HeadMesh},
            {FirstPersonHairMesh, HairMesh},
            {FirstPersonTopMesh, TopMesh},
            {FirstPersonHandMesh, HandMesh},
            {FirstPersonPantsMesh, PantsMesh},
            {FirstPersonShoesMesh, ShoesMesh}};

    for (const FFirstPersonModularMeshPair &MeshPair : FirstPersonModularMeshPairs)
    {
        if (!MeshPair.FirstPersonComponent || !FirstPersonMesh)
        {
            continue;
        }

        if (bCopyThirdPersonMeshToFirstPersonMesh && !MeshPair.FirstPersonComponent->GetSkeletalMeshAsset())
        {
            // if (FirstPersonMesh && FirstPersonMesh->GetSkeletalMeshAsset())
            if (MeshPair.ThirdPersonComponent && MeshPair.ThirdPersonComponent->GetSkeletalMeshAsset())
            {
                // CopySkeletalMeshSetup(FirstPersonLowerBodyMesh, FirstPersonMesh);
                CopySkeletalMeshSetup(MeshPair.FirstPersonComponent, MeshPair.ThirdPersonComponent);
            }
            else
            {

                CopySkeletalMeshSetup(MeshPair.FirstPersonComponent, BaseMesh);
            }
        }
    }

    TArray<USkeletalMeshComponent *> ModularMeshes =
        {
            HeadMesh,
            HairMesh,
            TopMesh,
            PantsMesh,
            ShoesMesh,
            HandMesh};

    for (USkeletalMeshComponent *ModularMesh : ModularMeshes)
    {
        if (!ModularMesh)
        {
            continue;
        }

        ModularMesh->SetOnlyOwnerSee(false);
        ModularMesh->SetOwnerNoSee(false);
        ModularMesh->SetHiddenInGame(false);
        ModularMesh->SetLeaderPoseComponent(BaseMesh);
        ModularMesh->SetRelativeLocation(FVector::ZeroVector);
        ModularMesh->SetRelativeRotation(FRotator::ZeroRotator);
        ModularMesh->SetRelativeScale3D(FVector::OneVector);
    }
}

void AWomenCharacter::RebuildModularMeshes()
{
    RefreshModularMeshes();
}

void AWomenCharacter::ShowHeldItemThirdPersonDebugMesh(APickupActor *PickupActor)
{
    if (!ThirdPersonHeldItemDebugMesh || !PickupActor || !GetMesh())
    {
        return;
    }

    UStaticMeshComponent *PickupMeshComponent = PickupActor->GetPickupMeshComponent();
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
