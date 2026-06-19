
#include "MyPlayerController.h"
#include "WomenCharacter.h" 
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "ConfigurableDoorActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "InputCoreTypes.h"
#include "Net/UnrealNetwork.h"
#include "PickupActor.h"
#include "PickupActorAAARuneCanvasInstrument.h"
#include "PickupActorAAARuneGridInstrument.h"
#include "PickupActorAAARuneInstrument.h"
#include "PickupActorAAASlowTalisman.h"
#include "TabUI.h"

AMyPlayerController::AMyPlayerController()
{
    // 玩家控制器需要 Tick 来处理平滑跑步和手持物体动画。
    PrimaryActorTick.bCanEverTick = true;
}

void AMyPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(AMyPlayerController, HeldActor, COND_OwnerOnly);
}

void AMyPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController())
    {
        return;
    }

    if (UEnhancedInputLocalPlayerSubsystem *Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            // 在 BeginPlay 注入默认输入映射，后续各个 InputAction 才会生效。
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("DefaultMappingContext is not assigned on %s"), *GetName());
        }
    }

    // if (MainUIClass)
    //{
    //     MainUI = CreateWidget(this, MainUIClass);
    //     if (MainUI)
    //     {
    //         MainUI->AddToViewport();

    //       FInputModeGameAndUI InputMode;
    //       SetInputMode(InputMode);

    //       bShowMouseCursor = true;
    //   }
    //}
}

void AMyPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent *EI = Cast<UEnhancedInputComponent>(InputComponent))
    {
        // 这里把 Enhanced Input 资源绑定到本类的行为函数，所有玩家交互入口都集中在这里。
        if (MoveForwardAction)
        {
            EI->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &AMyPlayerController::MoveForward);
        }
        // D键位
        if (MoveRightAction)
        {
            EI->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &AMyPlayerController::MoveRight);
        }
        // D键位
        if (JumpAction)
        {
            EI->BindAction(JumpAction, ETriggerEvent::Started, this, &AMyPlayerController::Jump);
            EI->BindAction(JumpAction, ETriggerEvent::Completed, this, &AMyPlayerController::StopJump);
        }
        // 鼠标上键位
        if (LookUpAction)
        {
            EI->BindAction(LookUpAction, ETriggerEvent::Triggered, this, &AMyPlayerController::LookUp);
        }

        // 鼠标右键位
        if (LookRightAction)
        {
            EI->BindAction(LookRightAction, ETriggerEvent::Triggered, this, &AMyPlayerController::LookRight);
        }

        // E键位
        if (PickupAction)
        {
            EI->BindAction(PickupAction, ETriggerEvent::Started, this, &AMyPlayerController::TryPickup);
        }
        else if (InputComponent)
        {
            InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AMyPlayerController::TryPickup);
            UE_LOG(LogTemp, Warning, TEXT("PickupAction is not assigned on %s, falling back to raw E key binding"), *GetName());
        }

        // 鼠标左键位
        if (ThrowAction)
        {
            EI->BindAction(ThrowAction, ETriggerEvent::Started, this, &AMyPlayerController::ThrowHeldActor);
        }

        if (CloseItemAction)
        {
            EI->BindAction(CloseItemAction, ETriggerEvent::Started, this, &AMyPlayerController::HandleRightClickPressed);
            EI->BindAction(CloseItemAction, ETriggerEvent::Completed, this, &AMyPlayerController::HandleRightClickReleased);
        }
        else if (InputComponent)
        {
            InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AMyPlayerController::HandleRightClickPressed);
            InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AMyPlayerController::HandleRightClickReleased);
        }

        // 左Shift键位
        if (SprintAction)
        {
            EI->BindAction(SprintAction, ETriggerEvent::Started, this, &AMyPlayerController::SprintStart);
            EI->BindAction(SprintAction, ETriggerEvent::Completed, this, &AMyPlayerController::SprintStop);
        }

        // Ctrl键位
        if (SquatAction)
        {
            EI->BindAction(SquatAction, ETriggerEvent::Started, this, &AMyPlayerController::SquatTriggered);
        }
        else if (InputComponent)
        {
            InputComponent->BindKey(EKeys::C, IE_Pressed, this, &AMyPlayerController::SquatTriggered);
            UE_LOG(LogTemp, Warning, TEXT("SquatAction is not assigned on %s, falling back to raw C key binding"), *GetName());
        }
        // Tab键位
        if (ToggleTabAction)
        {
            EI->BindAction(ToggleTabAction, ETriggerEvent::Started, this, &AMyPlayerController::ToggleTabUI);
        }
    }
}

void AMyPlayerController::ToggleTabUI()
{
    // 同一个按键既负责打开也负责关闭 Tab 界面，保持简单的切换语义。
    if (TabUI && TabUI->IsInViewport())
    {
        TabUI->RemoveFromParent();
        TabUI = nullptr;

        UE_LOG(LogTemp, Warning, TEXT("Close Tab UI"));
    }
    else
    {
        if (TabUIClass)
        {
            TabUI = CreateWidget(this, TabUIClass);
            if (TabUI)
            {
                TabUI->AddToViewport(10);

                UE_LOG(LogTemp, Warning, TEXT("Close Tab UI"));
            }
        }
    }
}

void AMyPlayerController::HandleRightClickPressed()
{
    if (APickupActorAAARuneInstrument *RuneInstrument = Cast<APickupActorAAARuneInstrument>(HeldActor))
    {
        if (RuneInstrument->BeginRuneDraw(this))
        {
            // 画符期间显示鼠标光标并切换到 UI+Game 输入模式，
            // 这样玩家能看到鼠标指针在屏幕上的位置，同时不会旋转视角。
            bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
            InputMode.SetHideCursorDuringCapture(false);
            SetInputMode(InputMode);

            FVector2D StartScreenPosition = FVector2D::ZeroVector;
            if (RuneInstrument->GetPreferredDrawStartScreenPosition(this, StartScreenPosition))
            {
                SetMouseLocation(
                    FMath::RoundToInt(StartScreenPosition.X),
                    FMath::RoundToInt(StartScreenPosition.Y));
                RuneInstrument->UpdateRuneDrawFromScreenPosition(this, StartScreenPosition);
            }
        }
        return;
    }

    if (APickupActorAAARuneGridInstrument *RuneGridInstrument = Cast<APickupActorAAARuneGridInstrument>(HeldActor))
    {
        if (RuneGridInstrument->BeginRuneDraw(this))
        {
            // 画符期间显示鼠标光标并切换到 UI+Game 输入模式，
            // 这样玩家能看到鼠标指针在屏幕上的位置，同时不会旋转视角。
            bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
            InputMode.SetHideCursorDuringCapture(false);
            SetInputMode(InputMode);

            FVector2D StartScreenPosition = FVector2D::ZeroVector;
            if (RuneGridInstrument->GetPreferredDrawStartScreenPosition(this, StartScreenPosition))
            {
                SetMouseLocation(
                    FMath::RoundToInt(StartScreenPosition.X),
                    FMath::RoundToInt(StartScreenPosition.Y));
                RuneGridInstrument->UpdateRuneDrawFromScreenPosition(this, StartScreenPosition);
            }
        }
        return;
    }

    if (APickupActorAAARuneCanvasInstrument *RuneCanvasInstrument = Cast<APickupActorAAARuneCanvasInstrument>(HeldActor))
    {
        if (RuneCanvasInstrument->BeginRuneDraw(this))
        {
            bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
            InputMode.SetHideCursorDuringCapture(false);
            SetInputMode(InputMode);

            FVector2D StartScreenPosition = FVector2D::ZeroVector;
            if (RuneCanvasInstrument->GetPreferredDrawStartScreenPosition(this, StartScreenPosition))
            {
                SetMouseLocation(
                    FMath::RoundToInt(StartScreenPosition.X),
                    FMath::RoundToInt(StartScreenPosition.Y));
                RuneCanvasInstrument->UpdateRuneDrawFromScreenPosition(this, StartScreenPosition);
            }
        }
        return;
    }

    TryCloseHeldActorBehavior();
}

// void AMyPlayerController::HandleRightClickReleased()
// {
//     APickupActorAAARuneInstrument* RuneInstrument = Cast<APickupActorAAARuneInstrument>(HeldActor);
//     if (RuneInstrument && !RuneInstrument->IsRuneDrawActive())
//     {
//         return;
//     }

//     // 画符结束，恢复光标隐藏和纯游戏输入模式。
//     bShowMouseCursor = false;
//     SetInputMode(FInputModeGameOnly());

//     const TArray<int32> FinalSequence = RuneInstrument->EndRuneDraw(this);
//     if (!HasAuthority())
//     {
//         ServerSubmitRuneSequence(RuneInstrument, FinalSequence);
//         return;
//     }

//     AActor* SolvingActor = GetPawn();
//     if (!IsValid(SolvingActor))
//     {
//         SolvingActor = this;
//     }

//     RuneInstrument->CommitRuneSequenceAuthority(FinalSequence, SolvingActor);
// }

void AMyPlayerController::HandleRightClickReleased()
{
    APickupActorAAARuneInstrument *RuneInstrument = Cast<APickupActorAAARuneInstrument>(HeldActor);
    if (RuneInstrument && RuneInstrument->IsRuneDrawActive())
    {
        // 画符结束，恢复光标隐藏和纯游戏输入模式。
        bShowMouseCursor = false;
        SetInputMode(FInputModeGameOnly());

        const TArray<int32> FinalSequence = RuneInstrument->EndRuneDraw(this);
        if (!HasAuthority())
        {
            ServerSubmitRuneSequence(RuneInstrument, FinalSequence);
            return;
        }

        AActor *SolvingActor = GetPawn();
        if (!IsValid(SolvingActor))
        {
            SolvingActor = this;
        }

        RuneInstrument->CommitRuneSequenceAuthority(FinalSequence, SolvingActor);
        return;
    }

    APickupActorAAARuneCanvasInstrument *RuneCanvasInstrument = Cast<APickupActorAAARuneCanvasInstrument>(HeldActor);
    if (RuneCanvasInstrument && RuneCanvasInstrument->IsRuneDrawActive())
    {
        bShowMouseCursor = false;
        SetInputMode(FInputModeGameOnly());

        const TArray<int32> FinalSequence = RuneCanvasInstrument->EndRuneDraw(this);
        if (!HasAuthority())
        {
            ServerSubmitRuneSequence(RuneCanvasInstrument, FinalSequence);
            return;
        }
        return;
    }

    APickupActorAAARuneGridInstrument *RuneGridInstrument = Cast<APickupActorAAARuneGridInstrument>(HeldActor);
    if (!RuneGridInstrument || !RuneGridInstrument->IsRuneDrawActive())
    {
        return;
    }

    bShowMouseCursor = false;
    SetInputMode(FInputModeGameOnly());

    const TArray<int32> FinalSequence = RuneGridInstrument->EndRuneDraw(this);
    if (!HasAuthority())
    {
        ServerSubmitRuneSequence(RuneGridInstrument, FinalSequence);
        return;
    }

    AActor *SolvingActor = GetPawn();
    if (!IsValid(SolvingActor))
    {
        SolvingActor = this;
    }

    RuneGridInstrument->CommitRuneSequenceAuthority(FinalSequence, SolvingActor);
}

void AMyPlayerController::TryCloseHeldActorBehavior()
{
    if (HeldActor)
    {
        TryTogglePickupActor(HeldActor);
        return;
    }

    APickupActor *TargetPickupActor = nullptr;
    FHitResult HitResult;
    if (!TracePickupActorFromView(TargetPickupActor, HitResult, false))
    {
        return;
    }

    TryTogglePickupActor(TargetPickupActor);
}

// Movement -----------------------------------------------------------------

void AMyPlayerController::MoveForward(const FInputActionValue &Value)
{
    float AxisValue = Value.Get<float>();
    if (APawn *ControlledPawn = GetPawn())
        ControlledPawn->AddMovementInput(ControlledPawn->GetActorForwardVector(), AxisValue);
}

void AMyPlayerController::MoveRight(const FInputActionValue &Value)
{
    float AxisValue = Value.Get<float>();
    if (APawn *ControlledPawn = GetPawn())
        ControlledPawn->AddMovementInput(ControlledPawn->GetActorRightVector(), AxisValue);
}

void AMyPlayerController::Jump()
{
    if (AWomenCharacter *MyChar = Cast<AWomenCharacter>(GetPawn()))
    {
        if (MyChar->IsSquat)
        {
            SquatTriggered();
            return;
        }
    }
    
    if (ACharacter *Char = Cast<ACharacter>(GetPawn()))
        Char->Jump();
}

void AMyPlayerController::StopJump()
{
    if (ACharacter *Char = Cast<ACharacter>(GetPawn()))
        Char->StopJumping();
}

void AMyPlayerController::LookUp(const FInputActionValue &Value)
{
    // 画符期间不旋转视角，鼠标只用于在屏幕上扫过节点。
    if (APickupActorAAARuneInstrument *RuneInstrument = Cast<APickupActorAAARuneInstrument>(HeldActor))
    {
        if (RuneInstrument->IsRuneDrawActive())
        {
            return;
        }
    }

    // ==== 新增：矩阵法器绘制期间也锁定视角俯仰 ====
    // 防止鼠标移动既在画格子又带着镜头一起上下转动。
    if (APickupActorAAARuneGridInstrument *RuneGridInstrument = Cast<APickupActorAAARuneGridInstrument>(HeldActor))
    {
        if (RuneGridInstrument->IsRuneDrawActive())
        {
            return;
        }
    }
    // ==== 新增结束：矩阵法器绘制期间也锁定视角俯仰 ====
    if (APickupActorAAARuneCanvasInstrument *RuneCanvasInstrument = Cast<APickupActorAAARuneCanvasInstrument>(HeldActor))
    {
        if (RuneCanvasInstrument->IsRuneDrawActive())
        {
            return;
        }
    }

    float AxisValue = Value.Get<float>();
    AddPitchInput(AxisValue * MouseSensitivity);
}

void AMyPlayerController::LookRight(const FInputActionValue &Value)
{
    // 画符期间不旋转视角。
    if (APickupActorAAARuneInstrument *RuneInstrument = Cast<APickupActorAAARuneInstrument>(HeldActor))
    {
        if (RuneInstrument->IsRuneDrawActive())
        {
            return;
        }
    }
    // ==== 新增：矩阵法器绘制期间也锁定视角左右旋转 ====
    // 作用和 LookUp 里的新增逻辑一致，只是这里控制的是 Yaw。
    if (APickupActorAAARuneGridInstrument *RuneGridInstrument = Cast<APickupActorAAARuneGridInstrument>(HeldActor))
    {
        if (RuneGridInstrument->IsRuneDrawActive())
        {
            return;
        }
    }
    // ==== 新增结束：矩阵法器绘制期间也锁定视角左右旋转 ====

    if (APickupActorAAARuneCanvasInstrument *RuneCanvasInstrument = Cast<APickupActorAAARuneCanvasInstrument>(HeldActor))
    {
        if (RuneCanvasInstrument->IsRuneDrawActive())
        {
            return;
        }
    }

    float AxisValue = Value.Get<float>();
    AddYawInput(AxisValue * MouseSensitivity);
}

// Pickup / PutDown ---------------------------------------------------------
void AMyPlayerController::TryPickup()
{

    if (const AWomenCharacter *MyChar = Cast<AWomenCharacter>(GetPawn()))
    {
        if (MyChar->IsMiddleHandleTime || MyChar->IsSquatThrowing || MyChar->IsStandThrowing)
        {
            UE_LOG(LogTemp, Warning, TEXT("TryPickup blocked: currently in middle of throw animation"));
            return;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("TryPickup call��HeldActor=%s"),
           HeldActor ? *HeldActor->GetName() : TEXT("nullptr"));

    if (TryToggleNearbyDoor())
    {
        return;
    }
    // 如果已经拿着东西，再按一次交互键就改为尝试放下。
    if (HeldActor)
    {
        TryPutDown();
        return;
    }

    FHitResult HitResult;
    APickupActor *PickupActor = nullptr;
    if (!TracePickupActorFromView(PickupActor, HitResult, true))
        return;

    if (!HasAuthority())
    {
        ServerTryPickup(PickupActor);
        return;
    }

    TryPickupActor(PickupActor);
}

bool AMyPlayerController::TryToggleNearbyDoor()
{
    AConfigurableDoorActor* DoorActor = FindBestInteractableDoor();
    if (IsValid(DoorActor))
    {
        if (!HasAuthority())
        {
            ServerToggleDoor(DoorActor);
            return true;
        }

        DoorActor->ToggleDoor();
        return true;
    }

    return false;
}

AConfigurableDoorActor* AMyPlayerController::FindBestInteractableDoor() const
{
    UWorld* World = GetWorld();
    APawn* ControlledPawn = GetPawn();
    if (!World || !IsValid(ControlledPawn))
    {
        return nullptr;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    GetPlayerViewPoint(ViewLocation, ViewRotation);

    AConfigurableDoorActor* BestDoor = nullptr;
    float BestDistanceSq = TNumericLimits<float>::Max();

    for (TActorIterator<AConfigurableDoorActor> DoorIt(World); DoorIt; ++DoorIt)
    {
        AConfigurableDoorActor* CandidateDoor = *DoorIt;
        if (!IsValid(CandidateDoor) || !CandidateDoor->CanActorInteractFromView(ControlledPawn, ViewLocation, ViewRotation.Vector()))
        {
            continue;
        }

        const float DistanceSq = FVector::DistSquared(ControlledPawn->GetActorLocation(), CandidateDoor->GetActorLocation());
        if (!BestDoor || DistanceSq < BestDistanceSq)
        {
            BestDoor = CandidateDoor;
            BestDistanceSq = DistanceSq;
        }
    }

    return BestDoor;
}

void AMyPlayerController::ServerToggleDoor_Implementation(AConfigurableDoorActor* DoorActor)
{
    if (!IsValid(DoorActor))
    {
        return;
    }

    APawn* ControlledPawn = GetPawn();
    FVector ViewLocation;
    FRotator ViewRotation;
    GetPlayerViewPoint(ViewLocation, ViewRotation);

    if (!IsValid(ControlledPawn) || !DoorActor->CanActorInteractFromView(ControlledPawn, ViewLocation, ViewRotation.Vector()))
    {
        return;
    }

    DoorActor->ToggleDoor();
}

bool AMyPlayerController::TryPickupActor(APickupActor *PickupActor)
{
    if (!HasAuthority() || !IsValid(PickupActor) || PickupActor->IsHeldByPlayer())
    {
        return false;
    }

    if (!CanInteractWithPickupActor(PickupActor))
    {
        return false;
    }

    // 只有带 FirstPersonMesh 或 HoldPoint 的角色才能挂住拾物。
    AWomenCharacter *MyChar = Cast<AWomenCharacter>(GetPawn());
    if (!MyChar || (!MyChar->FirstPersonMesh && !MyChar->HoldPoint))
    {
        UE_LOG(LogTemp, Warning, TEXT("TryPickup failed: pawn is not AWomenCharacter or FirstPersonMesh/HoldPoint is missing"));
        return false;
    }

    // OnPickedUp 负责关闭物理；这里再把物体附着到第一人称挂点上。
    HeldActor = PickupActor;
    HeldActor->OnPickedUp();

    // HeldActor->AttachToComponent(
    //     MyChar->HoldPoint,
    //     FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    // HeldActor->SetActorRelativeLocation(FVector::ZeroVector);
    // HeldActor->SetActorRelativeRotation(FRotator::ZeroRotator);
    AttachHeldActorToThirdPersonView(HeldActor);

    // 如果是 listen server 的本地玩家，也立即切到第一人称挂点表现。
    // 普通客户端通过 OnRep_HeldActor 再执行第一人称挂接。
    if (IsLocalController())
    {
        AttachHeldActorToFirstPersonView(HeldActor);
    }

    // 重新拿起物体时，把 bob 计时清零，避免沿用上次的相位。
    BobTime = 0.f;
    ApplySprintAnimationState(MyChar);

    UE_LOG(LogTemp, Warning, TEXT("ʰ��: %s"), *HeldActor->GetName());

    return true;
}

void AMyPlayerController::TryPutDown()
{
    if (!HeldActor)
        return;

    if (const AWomenCharacter *MyChar = Cast<AWomenCharacter>(GetPawn()))
    {
        if (MyChar->IsMiddleHandleTime || MyChar->IsSquatThrowing || MyChar->IsStandThrowing)
        {
            UE_LOG(LogTemp, Warning, TEXT("TryPickup blocked: currently in middle of throw animation"));
            return;
        }
    }

    if (AWomenCharacter *MyChar = Cast<AWomenCharacter>(GetPawn()))
    {
        MyChar->HideHeldItemThirdPersonDebugMesh();
    }

    if (!HasAuthority())
    {
        ServerTryPutDown();
        return;
    }

    FVector PlaceLocation;
    FRotator PlaceRotation;

    if (GetPlaceLocation(PlaceLocation, PlaceRotation))
    {
        HeldActor->OnPutDown(PlaceLocation, PlaceRotation);
        UE_LOG(LogTemp, Warning, TEXT("����: %s"), *HeldActor->GetName());
        HeldActor = nullptr;

         if (AWomenCharacter *MyChar = Cast<AWomenCharacter>(GetPawn()))
        {
            ApplySprintAnimationState(MyChar);
        }
    }
}

bool AMyPlayerController::GetPlaceLocation(FVector &OutLocation, FRotator &OutRotation)
{
    APawn *MyPawn = GetPawn();
    if (!MyPawn)
        return false;

    FVector Forward = MyPawn->GetActorForwardVector();
    FVector StartPos = MyPawn->GetActorLocation() + Forward * 100.f;
    FVector EndPos = StartPos + FVector(0.f, 0.f, -200.f);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(MyPawn);
    Params.AddIgnoredActor(HeldActor);

    // 放下时优先检查地面静态世界，避免被角色自己或其他动态物体误导。
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit, StartPos, EndPos, ECC_WorldStatic, Params);

    if (bHit)
    {
        OutLocation = Hit.ImpactPoint + FVector(0.f, 0.f, 10.f);
        OutRotation = MyPawn->GetActorRotation();
        return true;
    }
    return false;
}

bool AMyPlayerController::TracePickupActorFromView(APickupActor *&OutPickupActor, FHitResult &OutHit, bool bDrawDebug) const
{
    OutPickupActor = nullptr;
    OutHit = FHitResult();

    if (!GetWorld())
    {
        return false;
    }

    FVector CameraLocation;
    FRotator CameraRotation;
    GetPlayerViewPoint(CameraLocation, CameraRotation);

    const FVector TraceStart = CameraLocation;
    const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * InteractDistance;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetPawn());

    FCollisionObjectQueryParams PickupObjectParams;
    PickupObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
    PickupObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

    auto FindBestPickupHit = [TraceStart](const TArray<FHitResult> &HitResults) -> const FHitResult *
    {
        const FHitResult *BestPickupHit = nullptr;
        float BestDistanceSq = TNumericLimits<float>::Max();

        for (const FHitResult &HitResult : HitResults)
        {
            AActor *HitActor = HitResult.GetActor();
            if (!IsValid(HitActor))
            {
                continue;
            }

            APickupActor *CandidatePickup = Cast<APickupActor>(HitActor);
            if (!IsValid(CandidatePickup) || CandidatePickup->IsHeldByPlayer())
            {
                continue;
            }

            const USceneComponent *PickupRootComponent = CandidatePickup->GetRootComponent();
            const UPrimitiveComponent *HitComponent = HitResult.GetComponent();
            if (IsValid(PickupRootComponent) && IsValid(HitComponent) && HitComponent != PickupRootComponent)
            {
                continue;
            }

            const float DistanceSq = FVector::DistSquared(TraceStart, HitResult.ImpactPoint);
            if (!BestPickupHit || DistanceSq < BestDistanceSq)
            {
                BestPickupHit = &HitResult;
                BestDistanceSq = DistanceSq;
            }
        }

        return BestPickupHit;
    };

    TArray<FHitResult> DirectHitResults;
    GetWorld()->LineTraceMultiByObjectType(
        DirectHitResults,
        TraceStart,
        TraceEnd,
        PickupObjectParams,
        Params);

    const FHitResult *BestPickupHit = FindBestPickupHit(DirectHitResults);

    const FCollisionShape BoxShape = FCollisionShape::MakeBox(TraceHalfExtent);
    TArray<FHitResult> SweepHitResults;
    GetWorld()->SweepMultiByObjectType(
        SweepHitResults,
        TraceStart,
        TraceEnd,
        CameraRotation.Quaternion(),
        PickupObjectParams,
        BoxShape,
        Params);

    if (!BestPickupHit)
    {
        BestPickupHit = FindBestPickupHit(SweepHitResults);
    }

    if (bDrawDebug)
    {
        const FColor DebugColor = BestPickupHit ? FColor::Green : FColor::Red;
        DrawDebugBox(GetWorld(), TraceStart, TraceHalfExtent, CameraRotation.Quaternion(), DebugColor, false, 2.f);
        DrawDebugBox(GetWorld(), TraceEnd, TraceHalfExtent, CameraRotation.Quaternion(), DebugColor, false, 2.f);
        DrawDebugLine(GetWorld(), TraceStart, TraceEnd, DebugColor, false, 2.f, 0, 1.f);
    }

    if (!BestPickupHit)
    {
        const AActor *FirstDirectActor = DirectHitResults.Num() > 0 ? DirectHitResults[0].GetActor() : nullptr;
        const AActor *FirstSweepActor = SweepHitResults.Num() > 0 ? SweepHitResults[0].GetActor() : nullptr;
        UE_LOG(LogTemp, Warning, TEXT("TryPickup trace missed: no pickup actor found. Direct=%s Sweep=%s"),
               *GetNameSafe(FirstDirectActor),
               *GetNameSafe(FirstSweepActor));
        return false;
    }

    OutHit = *BestPickupHit;
    OutPickupActor = Cast<APickupActor>(BestPickupHit->GetActor());

    return IsValid(OutPickupActor);
}

bool AMyPlayerController::TryClosePickupActor(APickupActor *PickupActor)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (!IsValid(PickupActor) || !PickupActor->CanBeClosedByPlayerNative())
    {
        return false;
    }

    AActor *ClosingActor = GetPawn();
    if (!IsValid(ClosingActor))
    {
        ClosingActor = this;
    }

    PickupActor->CloseByPlayerNative(ClosingActor);
    return true;
}

bool AMyPlayerController::TryTogglePickupActor(APickupActor *PickupActor)
{
    if (!HasAuthority())
    {
        ServerTogglePickupActor(PickupActor);
        return IsValid(PickupActor);
    }

    if (!IsValid(PickupActor) || !PickupActor->CanBeClosedByPlayerNative())
    {
        return false;
    }

    if (!CanInteractWithPickupActor(PickupActor) && PickupActor != HeldActor)
    {
        return false;
    }

    AActor *InteractingActor = GetPawn();
    if (!IsValid(InteractingActor))
    {
        InteractingActor = this;
    }

    if (PickupActor->IsClosedByPlayerNative())
    {
        PickupActor->OpenByPlayerNative(InteractingActor);
        return true;
    }

    PickupActor->CloseByPlayerNative(InteractingActor);
    return true;
}

bool AMyPlayerController::CanInteractWithPickupActor(const APickupActor *PickupActor) const
{
    const APawn *ControlledPawn = GetPawn();
    if (!IsValid(ControlledPawn) || !IsValid(PickupActor))
    {
        return false;
    }

    const float MaxInteractDistance = FMath::Max(InteractDistance + 75.f, 75.f);
    return FVector::DistSquared(ControlledPawn->GetActorLocation(), PickupActor->GetActorLocation()) <= FMath::Square(MaxInteractDistance);
}

void AMyPlayerController::OnRep_HeldActor()
{
    if (!IsLocalController())
    {
        return;
    }

    AWomenCharacter *MyChar = Cast<AWomenCharacter>(GetPawn());

    if (!HeldActor)
    {
        if (MyChar)
        {
            MyChar->HideHeldItemThirdPersonDebugMesh();
            ApplySprintAnimationState(MyChar);
        }
        return;
    }

    AttachHeldActorToFirstPersonView(HeldActor);

    if (MyChar)
    {
        MyChar->ShowHeldItemThirdPersonDebugMesh(HeldActor);
        ApplySprintAnimationState(MyChar);
    }
}

bool AMyPlayerController::AttachHeldActorToFirstPersonView(APickupActor *PickupActor)
{
    if (!IsValid(PickupActor))
    {
        return false;
    }

    AWomenCharacter *MyChar = Cast<AWomenCharacter>(GetPawn());
    if (!MyChar)
    {
        return false;
    }

    const FName SocketName = PickupActor->GetFirstPersonSocketName();

    if (MyChar->FirstPersonMesh)
    {
        PickupActor->AttachToComponent(
            MyChar->FirstPersonMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            SocketName);
        PickupActor->SetActorRelativeLocation(PickupActor->GetFirstPersonLocationOffset());
        PickupActor->SetActorRelativeRotation(PickupActor->GetFirstPersonRotationOffset());

        MyChar->ShowHeldItemThirdPersonDebugMesh(PickupActor);
        return true;
    }

    if (MyChar->HoldPoint)
    {
        PickupActor->AttachToComponent(
            MyChar->HoldPoint,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        PickupActor->SetActorRelativeLocation(PickupActor->GetFirstPersonLocationOffset());
        PickupActor->SetActorRelativeRotation(PickupActor->GetFirstPersonRotationOffset());

        MyChar->ShowHeldItemThirdPersonDebugMesh(PickupActor);
        return true;
    }

    return false;
}

bool AMyPlayerController::AttachHeldActorToThirdPersonView(APickupActor *PickupActor)
{
    if (!IsValid(PickupActor))
    {
        return false;
    }

    AWomenCharacter *MyChar = Cast<AWomenCharacter>(GetPawn());
    if (!MyChar || !MyChar->GetMesh())
    {
        return false;
    }

    PickupActor->AttachToComponent(
        MyChar->GetMesh(),
        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        PickupActor->GetThirdPersonSocketName());
    PickupActor->SetActorRelativeLocation(PickupActor->GetThirdPersonLocationOffset());
    PickupActor->SetActorRelativeRotation(PickupActor->GetThirdPersonRotationOffset());
    return true;
}

bool AMyPlayerController::CanSprintWithHeldActor() const
{
    return !HeldActor || HeldActor->AllowsSprintWhileHeld();
}

bool AMyPlayerController::CanThrowHeldActor() const
{
    return HeldActor && HeldActor->AllowsThrowWhileHeld();
}

void AMyPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 冲刺不是瞬间切换，而是逐帧平滑插值到目标速度。
    if (ACharacter *Char = Cast<ACharacter>(GetPawn()))
    {
        UCharacterMovementComponent *MoveComp = Char->GetCharacterMovement();
        float CurrentSpeed = MoveComp->MaxWalkSpeed;
        const AWomenCharacter* MyChar = Cast<AWomenCharacter>(Char);
        const bool bIsSquatting = MyChar && MyChar->IsSquat;
        // const bool bCanSprint = bWantsToSprint && (!MyChar || !MyChar->IsSquat);
        const bool bCanSprint = bWantsToSprint && CanSprintWithHeldActor() && !MyChar->IsSquat;

        if (bCanSprint)
        {
            float NewSpeed = FMath::FInterpConstantTo(CurrentSpeed, RunSpeed, DeltaTime, SpeedUpRate);
            MoveComp->MaxWalkSpeed = NewSpeed;
        }
        else
        {
            const float TargetWalkSpeed = bIsSquatting ? CrouchSpeed : WalkSpeed;
            float NewSpeed = FMath::FInterpConstantTo(CurrentSpeed, TargetWalkSpeed, DeltaTime, SpeedDownRate);
            MoveComp->MaxWalkSpeed = NewSpeed;
        }
    }

    if (!IsLocalController())
    {
        return;
    }

    if (!HeldActor)
    {
        return;
    }

    UpdateHeldRuneInstrumentDraw();

    APawn *MyPawn = GetPawn();
    if (!MyPawn)
        return;

    // 只有角色在平面上移动时才推进手持 bob 动画。
    float Speed = MyPawn->GetVelocity().Size2D();
    bool bIsMoving = Speed > 10.f;

    // bob 通过简单的正弦/余弦位移，让手持物在镜头前轻微上下和前后晃动。
    FVector TargetOffset = FVector::ZeroVector;

    if (bIsMoving)
    {
        BobTime += DeltaTime;
    }
    else
    {
        // 停止移动时平滑回零，不让物体突然定住。
        BobTime = FMath::FInterpTo(BobTime, 0.f, DeltaTime, BobInterpSpeed);
    }

    float BobZ = FMath::Sin(BobTime * BobSpeed) * BobAmplitude;
    float BobX = FMath::Cos(BobTime * BobSpeed * 0.5f) * (BobAmplitude * 0.5f);

    TargetOffset = FVector(BobX, 0.f, BobZ);

    // HeldActor->SetActorRelativeLocation(TargetOffset);
    HeldActor->SetActorRelativeLocation(HeldActor->GetFirstPersonLocationOffset() + TargetOffset);

    // 如果持物与角色之间被墙挡住，就暂时隐藏它，避免模型穿墙出现在镜头前。
    FVector PawnLoc = MyPawn->GetActorLocation();
    FVector HeldLoc = HeldActor->GetActorLocation();

    FHitResult WallHit;
    FCollisionQueryParams WallParams;
    WallParams.AddIgnoredActor(MyPawn);
    WallParams.AddIgnoredActor(HeldActor);

    bool bWallHit = GetWorld()->LineTraceSingleByChannel(
        WallHit, PawnLoc, HeldLoc, ECC_Visibility, WallParams);

    HeldActor->SetActorHiddenInGame(bWallHit);
}

void AMyPlayerController::ThrowHeldActor()
{
    if (!HeldActor)
        return;

    if (!CanThrowHeldActor())
        return;

    AWomenCharacter *MyChar = Cast<AWomenCharacter>(GetPawn());
    if (!MyChar)
        return;

    MyChar->HideHeldItemThirdPersonDebugMesh();

    // 中段处理时间或投掷动画播放期间，禁止再次触发投掷，避免状态重入。
    if (MyChar->IsMiddleHandleTime)
    {
        return;
    }

    if (MyChar->IsSquatThrowing || MyChar->IsStandThrowing)
    {
        return;
    }

    if (!HasAuthority())
    {

        StartThrowAnimationState(MyChar);

        const float LocalReleaseDelay = FMath::Max(0.f, ThrowReleaseDelay);
        const float LocalAnimationEndDelay = LocalReleaseDelay + FMath::Max(0.f, ThrowAnimationRecoveryDelay);
        const float LocalInputLockEndDelay = FMath::Max(LocalAnimationEndDelay, FMath::Max(0.f, ThrowInputLockDuration));

        FTimerHandle LocalAnimationHandle;
        GetWorldTimerManager().SetTimer(LocalAnimationHandle, [this, MyChar]()
            {
                FinishThrowAnimationState(MyChar);
            }, LocalAnimationEndDelay, false);

        FTimerHandle LocalMiddleHandle;
        GetWorldTimerManager().SetTimer(LocalMiddleHandle, [this, MyChar]()
            {
                ClearThrowMiddleWindow(MyChar);
            }, LocalInputLockEndDelay, false);


        ServerThrowHeldActor();
        return;
    }

    FVector CameraLocation;
    FRotator CameraRotation;
    GetPlayerViewPoint(CameraLocation, CameraRotation);

    FHitResult ThrowAimHit;
    FCollisionQueryParams ThrowAimParams;
    ThrowAimParams.AddIgnoredActor(GetPawn());
    ThrowAimParams.AddIgnoredActor(HeldActor);
    GetWorld()->LineTraceSingleByChannel(
        ThrowAimHit,
        CameraLocation,
        CameraLocation + CameraRotation.Vector() * 5000.f,
        ECC_Visibility,
        ThrowAimParams);

    // 先把 HeldActor 置空，避免定时器尚未执行前再次被系统当成“仍在手上”。
    float Force = ThrowForce;
    APickupActor *ActorToThrow = HeldActor;
    if (ActorToThrow)
    {
        ActorToThrow->SetOwner(GetPawn());
        ActorToThrow->SetInstigator(Cast<APawn>(GetPawn()));
    }

    bool bShouldCloseBeforeThrow = true;
    if (const APickupActorAAASlowTalisman *SlowTalisman = Cast<APickupActorAAASlowTalisman>(ActorToThrow))
    {
        bShouldCloseBeforeThrow = SlowTalisman->IsClosedByPlayerNative();
    }

    if (bShouldCloseBeforeThrow)
    {
        TryClosePickupActor(ActorToThrow);
    }

    if (APickupActorAAASlowTalisman *SlowTalisman = Cast<APickupActorAAASlowTalisman>(ActorToThrow))
    {
        const bool bHasAimHit = ThrowAimHit.bBlockingHit;
        const float AimHitDistanceSq = bHasAimHit
                                           ? FVector::DistSquared(CameraLocation, ThrowAimHit.ImpactPoint)
                                           : 0.f;
        const float MaxSnapDistanceSq = FMath::Square(FMath::Max(0.f, SlowTalismanAimSnapMaxDistance));
        const bool bShouldSnapToAimPoint = bHasAimHit && AimHitDistanceSq <= MaxSnapDistanceSq;

        if (bShouldSnapToAimPoint)
        {
            SlowTalisman->SetPreferredStickTarget(ThrowAimHit);
        }
        else
        {
            SlowTalisman->ClearPreferredStickTarget();

            if (bHasAimHit)
            {
                UE_LOG(LogTemp, Verbose, TEXT("Slow talisman aim snap ignored: hit distance %.1f exceeds max %.1f"),
                       FMath::Sqrt(AimHitDistanceSq),
                       SlowTalismanAimSnapMaxDistance);
            }
        }
    }

    // 真正施加抛出冲量的时机交给定时器，以便和角色投掷动画同步。
    StartThrowAnimationState(MyChar);

    const float ReleaseDelay = FMath::Max(0.f, ThrowReleaseDelay);
    const float AnimationEndDelay = ReleaseDelay + FMath::Max(0.f, ThrowAnimationRecoveryDelay);
    const float InputLockEndDelay = FMath::Max(AnimationEndDelay, FMath::Max(0.f, ThrowInputLockDuration));

    if (MyChar->IsSquat)
    {
        FTimerHandle SquatHandle;
        FTimerHandle MiddleHandle;
        FTimerHandle ThrowHandle;
        
        GetWorldTimerManager().SetTimer(SquatHandle, [this, MyChar]()
                                        { FinishThrowAnimationState(MyChar); }, AnimationEndDelay, false);
        GetWorldTimerManager().SetTimer(MiddleHandle, [this, MyChar]()
                                        { ClearThrowMiddleWindow(MyChar); }, InputLockEndDelay, false);

        GetWorldTimerManager().SetTimer(ThrowHandle, [this, MyChar, ActorToThrow, Force]()
                                        {
                if (MyChar && ActorToThrow)
                {
                    if (HeldActor == ActorToThrow)
                    {
                        HeldActor = nullptr;
                        ApplySprintAnimationState(MyChar);
                    }
                    FVector ReleaseCameraLocation = FVector::ZeroVector;
                    FRotator ReleaseCameraRotation = FRotator::ZeroRotator;
                    GetPlayerViewPoint(ReleaseCameraLocation, ReleaseCameraRotation);

                    ActorToThrow->OnThrown(ReleaseCameraRotation.Vector(), Force);
                } }, ReleaseDelay, false);
    }
    else
    {
        FTimerHandle StandHandle;
        FTimerHandle MiddleHandle;
        FTimerHandle ThrowHandle;

        GetWorldTimerManager().SetTimer(StandHandle, [this, MyChar]()
                                        { FinishThrowAnimationState(MyChar); }, AnimationEndDelay, false);
        GetWorldTimerManager().SetTimer(MiddleHandle, [this, MyChar]()
                                        { ClearThrowMiddleWindow(MyChar); }, InputLockEndDelay, false);
        GetWorldTimerManager().SetTimer(ThrowHandle, [this, MyChar, ActorToThrow, Force]()
                                        {
                if (MyChar && ActorToThrow)
                {

                    if (HeldActor == ActorToThrow)
                    {
                        HeldActor = nullptr;
                        ApplySprintAnimationState(MyChar);
                    }
                    FVector ReleaseCameraLocation = FVector::ZeroVector;
                    FRotator ReleaseCameraRotation = FRotator::ZeroRotator;
                    GetPlayerViewPoint(ReleaseCameraLocation, ReleaseCameraRotation);

                    ActorToThrow->OnThrown(ReleaseCameraRotation.Vector(), Force);
                } }, ReleaseDelay, false);
    }
}

// void AMyPlayerController::UpdateHeldRuneInstrumentDraw()
// {
//     APickupActorAAARuneInstrument* RuneInstrument = Cast<APickupActorAAARuneInstrument>(HeldActor);

//     {
//         return;
//     }

//     float MouseX = 0.f;
//     float MouseY = 0.f;
//     if (!GetMousePosition(MouseX, MouseY))
//     {
//         return;
//     }

//     RuneInstrument->UpdateRuneDrawFromScreenPosition(this, FVector2D(MouseX, MouseY));
// }
void AMyPlayerController::UpdateHeldRuneInstrumentDraw()
{
    APickupActorAAARuneInstrument *RuneInstrument = Cast<APickupActorAAARuneInstrument>(HeldActor);
    if (RuneInstrument && RuneInstrument->IsRuneDrawActive())
    {
        float MouseX = 0.f;
        float MouseY = 0.f;
        if (!GetMousePosition(MouseX, MouseY))
        {
            return;
        }

        RuneInstrument->UpdateRuneDrawFromScreenPosition(this, FVector2D(MouseX, MouseY));
        return;
    }

    APickupActorAAARuneCanvasInstrument *RuneCanvasInstrument = Cast<APickupActorAAARuneCanvasInstrument>(HeldActor);
    if (RuneCanvasInstrument && RuneCanvasInstrument->IsRuneDrawActive())
    {
        float MouseX = 0.f;
        float MouseY = 0.f;
        if (!GetMousePosition(MouseX, MouseY))
        {
            return;
        }

        RuneCanvasInstrument->UpdateRuneDrawFromScreenPosition(this, FVector2D(MouseX, MouseY));
        return;
    }

    // ==== 新增：Tick 中持续更新矩阵法器的鼠标滑格 ====
    // 每帧读取鼠标屏幕坐标，交给矩阵法器自行判断当前扫到了哪个格子。
    APickupActorAAARuneGridInstrument *RuneGridInstrument = Cast<APickupActorAAARuneGridInstrument>(HeldActor);
    if (!RuneGridInstrument || !RuneGridInstrument->IsRuneDrawActive())
    {
        return;
    }

    float MouseX = 0.f;
    float MouseY = 0.f;
    if (!GetMousePosition(MouseX, MouseY))
    {
        return;
    }

    RuneGridInstrument->UpdateRuneDrawFromScreenPosition(this, FVector2D(MouseX, MouseY));
    // ==== 新增结束：Tick 中持续更新矩阵法器的鼠标滑格 ====
}

void AMyPlayerController::StartThrowAnimationState(AWomenCharacter *MyChar) const
{
    if (!MyChar)
    {
        return;
    }

    MyChar->IsMiddleHandleTime = true;
    if (MyChar->IsSquat)
    {
        MyChar->IsSquatThrowing = true;
        MyChar->IsStandThrowing = false;
        return;
    }

    MyChar->IsStandThrowing = true;
    MyChar->IsSquatThrowing = false;
}


bool AMyPlayerController::IsThrowStateActive(const AWomenCharacter* MyChar) const
{
    return MyChar
        && (MyChar->IsMiddleHandleTime || MyChar->IsSquatThrowing || MyChar->IsStandThrowing);
}

void AMyPlayerController::ApplySprintAnimationState(AWomenCharacter* MyChar)
{
    if (!MyChar)
    {
        return;
    }

    const bool bCanSprint = bWantsToSprint && CanSprintWithHeldActor() && !MyChar->IsSquat;
    MyChar->IsSprint = bCanSprint;

    if (!HasAuthority())
    {
        ServerSetSprintState(bWantsToSprint);
    }
}



void AMyPlayerController::FinishThrowAnimationState(AWomenCharacter *MyChar) const
{
    if (!MyChar)
    {
        return;
    }

    MyChar->IsSquatThrowing = false;
    MyChar->IsStandThrowing = false;
}

void AMyPlayerController::ClearThrowMiddleWindow(AWomenCharacter *MyChar)
{
    if (!MyChar)
    {
        return;
    }

    MyChar->IsMiddleHandleTime = false;

    if (!IsThrowStateActive(MyChar))
    {
        if (bHasDeferredSprintInput)
        {
            bWantsToSprint = bDeferredWantsToSprint;
            bHasDeferredSprintInput = false;
        }
        ApplySprintAnimationState(MyChar);
    }
}

void AMyPlayerController::SprintStart()
{
    AWomenCharacter *MyChar = Cast<AWomenCharacter>(GetPawn());
    if (!CanSprintWithHeldActor())
    {
        bWantsToSprint = true;
        ApplySprintAnimationState(MyChar);
        return;
    }

    if (IsThrowStateActive(MyChar))
    {
        bHasDeferredSprintInput = true;
        bDeferredWantsToSprint = true;
        return;
    }

    bWantsToSprint = true;
    ApplySprintAnimationState(MyChar);
}

void AMyPlayerController::SprintStop()
{
    AWomenCharacter *MyChar = Cast<AWomenCharacter>(GetPawn());
    if (IsThrowStateActive(MyChar))
    {
        bHasDeferredSprintInput = true;
        bDeferredWantsToSprint = false;
        return;
    }

    bWantsToSprint = false;
    ApplySprintAnimationState(MyChar);
}

void AMyPlayerController::SquatTriggered()
{
    AWomenCharacter *MyChar = Cast<AWomenCharacter>(GetPawn());
    if (MyChar)
    {
        // 这里是纯状态切换；真正的动画表现由 AnimInstance 读取这个布尔值。
        MyChar->IsSquat = !MyChar->IsSquat;
        ApplySprintAnimationState(MyChar);
    }

    if (!HasAuthority())
    {
        ServerToggleSquatState();
    }
}

void AMyPlayerController::ServerTryPickup_Implementation(APickupActor *PickupActor)
{
    TryPickupActor(PickupActor);
}

void AMyPlayerController::ServerTryPutDown_Implementation()
{
    TryPutDown();
}

void AMyPlayerController::ServerTogglePickupActor_Implementation(APickupActor *PickupActor)
{
    TryTogglePickupActor(PickupActor);
}

// void AMyPlayerController::ServerSubmitRuneSequence_Implementation(
//     APickupActorAAARuneInstrument* RuneInstrument,
//     const TArray<int32>& NodeSequence)
// {
//     if (!IsValid(RuneInstrument) || RuneInstrument != HeldActor)
//     {
//         return;
//     }

//     AActor* SolvingActor = GetPawn();
//     if (!IsValid(SolvingActor))
//     {
//         SolvingActor = this;
//     }

//     RuneInstrument->CommitRuneSequenceAuthority(NodeSequence, SolvingActor);
// }
void AMyPlayerController::ServerSubmitRuneSequence_Implementation(
    APickupActor *RuneActor,
    const TArray<int32> &NodeSequence)
{
    // ==== 新增：服务端统一接收“原符器 / 矩阵法器”两种结果 ====
    // 这里把参数改成了 APickupActor*，这样服务端可以统一处理两种法器。
    if (!IsValid(RuneActor) || RuneActor != HeldActor)
    {
        return;
    }

    AActor *SolvingActor = GetPawn();
    if (!IsValid(SolvingActor))
    {
        SolvingActor = this;
    }

    // 先尝试按旧的 RuneInstrument 提交。
    if (APickupActorAAARuneInstrument *RuneInstrument = Cast<APickupActorAAARuneInstrument>(RuneActor))
    {
        RuneInstrument->CommitRuneSequenceAuthority(NodeSequence, SolvingActor);
        return;
    }

    // 再尝试按新的 RuneGridInstrument 提交。
    if (APickupActorAAARuneGridInstrument *RuneGridInstrument = Cast<APickupActorAAARuneGridInstrument>(RuneActor))
    {
        RuneGridInstrument->CommitRuneSequenceAuthority(NodeSequence, SolvingActor);
        return;
    }

    if (APickupActorAAARuneCanvasInstrument *RuneCanvasInstrument = Cast<APickupActorAAARuneCanvasInstrument>(RuneActor))
    {
        RuneCanvasInstrument->CommitRuneSequenceAuthority(NodeSequence, SolvingActor);
        // ==== 新增结束：服务端统一接收“原符器 / 矩阵法器”两种结果 ====
    }
}

void AMyPlayerController::ServerThrowHeldActor_Implementation()
{
    ThrowHeldActor();
}

void AMyPlayerController::ServerSetSprintState_Implementation(bool bNewWantsToSprint)
{
    bWantsToSprint = bNewWantsToSprint;

    if (AWomenCharacter *MyChar = Cast<AWomenCharacter>(GetPawn()))
    {
        MyChar->IsSprint = bNewWantsToSprint && CanSprintWithHeldActor() && !MyChar->IsSquat;
    }
}

void AMyPlayerController::ServerToggleSquatState_Implementation()
{
    if (AWomenCharacter *MyChar = Cast<AWomenCharacter>(GetPawn()))
    {
        MyChar->IsSquat = !MyChar->IsSquat;
        ApplySprintAnimationState(MyChar);
    }
}
