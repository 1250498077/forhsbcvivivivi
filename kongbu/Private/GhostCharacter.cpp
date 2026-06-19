#include "GhostCharacter.h"

#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GhostNativeAnimInstance.h"
#include "GhostThrowablePropComponent.h"
#include "MyAIController.h"
#include "PickupActor.h"
#include "PickupActorAAARuneInstrument.h"
#include "PickupActorAAASlowTalisman.h"
#include "WomenCharacter.h"

AGhostCharacter::AGhostCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    bUseControllerRotationYaw = false;
    if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
    {
        MovementComponent->bOrientRotationToMovement = true;
        MovementComponent->bUseControllerDesiredRotation = false;
        MovementComponent->RotationRate = FRotator(0.f, GhostBodyRotationRateYaw, 0.f);
    }

    GhostAttachZoneComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("GhostAttachZoneComponent"));
    GhostAttachZoneComponent->SetupAttachment(GetMesh());
    GhostAttachZoneComponent->SetRelativeLocation(FVector(0.f, 0.f, 76.f));
    GhostAttachZoneComponent->SetCapsuleRadius(42.f);
    GhostAttachZoneComponent->SetCapsuleHalfHeight(72.f);
    ConfigureGhostAttachZoneCollision();
    GhostAttachZoneComponent->SetCanEverAffectNavigation(false);
    GhostAttachZoneComponent->SetHiddenInGame(true);
    GhostAttachZoneComponent->SetVisibility(true);
    GhostAttachZoneComponent->OnComponentBeginOverlap.AddDynamic(
        this,
        &AGhostCharacter::HandleGhostAttachZoneBeginOverlap);

    SoulSuckTriggerComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("SoulSuckTriggerComponent"));
    SoulSuckTriggerComponent->SetupAttachment(GetCapsuleComponent());
    SoulSuckTriggerComponent->SetRelativeLocation(FVector(65.f, 0.f, 0.f));
    SoulSuckTriggerComponent->SetCapsuleRadius(70.f);
    SoulSuckTriggerComponent->SetCapsuleHalfHeight(96.f);
    ConfigureSoulSuckTriggerCollision();
    SoulSuckTriggerComponent->SetCanEverAffectNavigation(false);
    SoulSuckTriggerComponent->SetHiddenInGame(true);
    SoulSuckTriggerComponent->SetVisibility(true);
    SoulSuckTriggerComponent->OnComponentBeginOverlap.AddDynamic(
        this,
        &AGhostCharacter::HandleSoulSuckTriggerBeginOverlap);

    TelekinesisTriggerComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("TelekinesisTriggerComponent"));
    TelekinesisTriggerComponent->SetupAttachment(GetCapsuleComponent());
    TelekinesisTriggerComponent->SetRelativeLocation(FVector(0.f, 0.f, 24.f));
    TelekinesisTriggerComponent->SetCapsuleRadius(95.f);
    TelekinesisTriggerComponent->SetCapsuleHalfHeight(88.f);
    ConfigureTelekinesisTriggerCollision();
    TelekinesisTriggerComponent->SetCanEverAffectNavigation(false);
    TelekinesisTriggerComponent->SetHiddenInGame(true);
    TelekinesisTriggerComponent->SetVisibility(true);
    TelekinesisTriggerComponent->OnComponentBeginOverlap.AddDynamic(
        this,
        &AGhostCharacter::HandleTelekinesisTriggerBeginOverlap);
}

void AGhostCharacter::BeginPlay()
{
    Super::BeginPlay();
    ApplyGhostTurnSettings();

    if (GhostAttachZoneComponent)
    {
        ConfigureGhostAttachZoneCollision();
        UpdateGhostAttachZoneDebugVisibility();
    }
    if (SoulSuckTriggerComponent)
    {
        ConfigureSoulSuckTriggerCollision();
        UpdateSoulSuckTriggerDebugVisibility();
    }

    if (TelekinesisTriggerComponent)
    {
        ConfigureTelekinesisTriggerCollision();
        UpdateTelekinesisTriggerDebugVisibility();
    }
}

void AGhostCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsTelekineticThrowActive)
    {
        UpdateTelekineticThrowableThrow(DeltaTime);
    }

    if (bShowGhostAttachZoneDebug && GhostAttachZoneComponent)
    {
        DrawDebugCapsule(
            GetWorld(),
            GhostAttachZoneComponent->GetComponentLocation(),
            GhostAttachZoneComponent->GetScaledCapsuleHalfHeight(),
            GhostAttachZoneComponent->GetScaledCapsuleRadius(),
            GhostAttachZoneComponent->GetComponentQuat(),
            GhostAttachZoneDebugColor,
            false,
            0.f,
            0,
            2.f);
    }

    if (bShowSoulSuckTriggerDebug && SoulSuckTriggerComponent)
    {
        DrawDebugCapsule(
            GetWorld(),
            SoulSuckTriggerComponent->GetComponentLocation(),
            SoulSuckTriggerComponent->GetScaledCapsuleHalfHeight(),
            SoulSuckTriggerComponent->GetScaledCapsuleRadius(),
            SoulSuckTriggerComponent->GetComponentQuat(),
            SoulSuckTriggerDebugColor,
            false,
            0.f,
            0,
            2.f);
    }

    if (bShowTelekinesisTriggerDebug && TelekinesisTriggerComponent)
    {
        DrawDebugCapsule(
            GetWorld(),
            TelekinesisTriggerComponent->GetComponentLocation(),
            TelekinesisTriggerComponent->GetScaledCapsuleHalfHeight(),
            TelekinesisTriggerComponent->GetScaledCapsuleRadius(),
            TelekinesisTriggerComponent->GetComponentQuat(),
            TelekinesisTriggerDebugColor,
            false,
            0.f,
            0,
            2.f);
    }
}

void AGhostCharacter::OnConstruction(const FTransform &Transform)
{
    Super::OnConstruction(Transform);

    ApplyGhostTurnSettings();

    ConfigureGhostAttachZoneCollision();
    UpdateGhostAttachZoneDebugVisibility();
    ConfigureSoulSuckTriggerCollision();
    UpdateSoulSuckTriggerDebugVisibility();
    ConfigureTelekinesisTriggerCollision();
    UpdateTelekinesisTriggerDebugVisibility();
}

UCapsuleComponent *AGhostCharacter::GetGhostAttachZoneComponent() const
{
    return GhostAttachZoneComponent;
}
UCapsuleComponent* AGhostCharacter::GetSoulSuckTriggerComponent() const
{
    return SoulSuckTriggerComponent;
}

UCapsuleComponent* AGhostCharacter::GetTelekinesisTriggerComponent() const
{
    return TelekinesisTriggerComponent;
}
bool AGhostCharacter::IsWorldLocationInsideGhostAttachZone(const FVector &WorldLocation, float Tolerance) const
{
    if (!GhostAttachZoneComponent)
    {
        return false;
    }

    const FVector LocalLocation = GhostAttachZoneComponent->GetComponentTransform().InverseTransformPosition(WorldLocation);
    const float Radius = GhostAttachZoneComponent->GetScaledCapsuleRadius() + FMath::Max(0.f, Tolerance);
    const float HalfHeight = GhostAttachZoneComponent->GetScaledCapsuleHalfHeight() + FMath::Max(0.f, Tolerance);

    return FVector2D(LocalLocation.X, LocalLocation.Y).SizeSquared() <= FMath::Square(Radius) && FMath::Abs(LocalLocation.Z) <= HalfHeight;
}
bool AGhostCharacter::PlayGhostOpenDoorAnimation()
{
    if (USkeletalMeshComponent* MeshComponent = GetMesh())
    {
        if (UGhostNativeAnimInstance* GhostAnimInstance = Cast<UGhostNativeAnimInstance>(MeshComponent->GetAnimInstance()))
        {
            return GhostAnimInstance->PlayOpenDoorAction();
        }
    }

    return false;
}

bool AGhostCharacter::StartSoulSuck(AWomenCharacter* Victim)
{
    if (!bEnableSoulSuckOnPlayerTouch || bIsSoulSucking || !IsValid(Victim))
    {
        return false;
    }

    bIsSoulSucking = true;
    CurrentSoulSuckVictim = Victim;

    if (bStopAIMovementDuringSoulSuck)
    {
        if (AAIController* AIController = Cast<AAIController>(GetController()))
        {
            AIController->StopMovement();
        }
    }

    FreezeSoulSuckVictim(Victim);
    Victim->StartSoulSuckReaction();
    MulticastPlayGhostSoulSuckAnimation();

    const float ResolvedDuration = ResolveSoulSuckDuration();

    if (ResolvedDuration > 0.f)
    {
        GetWorldTimerManager().SetTimer(SoulSuckTimerHandle, this, &AGhostCharacter::StopSoulSuck, ResolvedDuration, false);
    }
    else
    {
        GetWorldTimerManager().SetTimerForNextTick(this, &AGhostCharacter::StopSoulSuck);
    }

    return true;
}

void AGhostCharacter::StopSoulSuck()
{
    GetWorldTimerManager().ClearTimer(SoulSuckTimerHandle);

    AWomenCharacter* Victim = CurrentSoulSuckVictim;
    bIsSoulSucking = false;
    CurrentSoulSuckVictim = nullptr;

    UnfreezeSoulSuckVictim(Victim);
    if (IsValid(Victim) && Victim->bIsSoulSucked)
    {
        Victim->ClearForcedReactionState();
    }
}

bool AGhostCharacter::InterruptSoulSuckWithKnockdown(AActor* ImpactSourceActor, FVector ImpactDirection, float StunDuration)
{
    if (!bIsSoulSucking)
    {
        return false;
    }

    AWomenCharacter* Victim = CurrentSoulSuckVictim;
    StopSoulSuck();

    FVector FacingDirection = FVector::ZeroVector;
    if (IsValid(ImpactSourceActor))
    {
        FacingDirection = ImpactSourceActor->GetActorLocation() - GetActorLocation();
    }
    else
    {
        FacingDirection = ImpactDirection;
    }

    FacingDirection.Z = 0.f;
    if (!FacingDirection.IsNearlyZero())
    {
        const FRotator KnockdownRotation = FacingDirection.Rotation();
        SetActorRotation(FRotator(0.f, KnockdownRotation.Yaw, 0.f));

        if (AController* MyController = GetController())
        {
            MyController->SetControlRotation(FRotator(0.f, KnockdownRotation.Yaw, 0.f));
        }
    }

    if (IsValid(Victim))
    {
        Victim->InterruptSoulSuckWithKnockdown();
    }

    MulticastPlayGhostKnockdownAnimation();

    const float ResolvedStunDuration = StunDuration > 0.f ? StunDuration : ResolveGhostKnockdownDuration();
    if (AMyAIController* AIController = ResolveMyAIController())
    {
        AIController->ApplyStunFromSource(ResolvedStunDuration, ImpactSourceActor);
    }

    return true;
}

bool AGhostCharacter::StartTelekineticThrowableThrow(UGhostThrowablePropComponent* ThrowableProp)
{
    if (!bEnableTelekineticThrowableThrow
        || bIsTelekineticThrowActive
        || bIsSoulSucking
        || !IsValid(ThrowableProp)
        || !ThrowableProp->CanBeUsedByGhost())
    {
        return false;
    }

    if (FMath::FRand() > FMath::Clamp(TelekineticThrowActivationChance, 0.01f, 1.f))
    {
        return false;
    }

    FVector TargetLocation = FVector::ZeroVector;
    CurrentTelekineticTargetActor = ResolveTelekineticTargetActor();
    if (!RefreshTelekineticTargetLocation(TargetLocation))
    {
        return false;
    }

    if (!ThrowableProp->BeginGhostTelekinesis(this))
    {
        return false;
    }

    CurrentTelekineticThrowable = ThrowableProp;
    TelekineticStartLocation = ThrowableProp->GetThrowableLocation();
    LastTrackedTelekineticTargetLocation = TargetLocation;
    TelekineticElapsedTime = 0.f;
    bIsTelekineticThrowActive = true;
    return true;
}

void AGhostCharacter::StopTelekineticThrowableThrow(bool bLaunchThrowable)
{
    UGhostThrowablePropComponent* ThrowableProp = CurrentTelekineticThrowable;
    CurrentTelekineticThrowable = nullptr;
    CurrentTelekineticTargetActor = nullptr;
    TelekineticElapsedTime = 0.f;
    bIsTelekineticThrowActive = false;

    if (!IsValid(ThrowableProp))
    {
        return;
    }

    if (bLaunchThrowable)
    {
        const FVector CurrentObjectLocation = ThrowableProp->GetThrowableLocation();
        FVector ThrowDirection = LastTrackedTelekineticTargetLocation - CurrentObjectLocation;
        ThrowDirection.Z = FMath::Max(ThrowDirection.Z, 10.f);
        if (ThrowDirection.IsNearlyZero())
        {
            ThrowDirection = GetActorForwardVector();
        }

        ThrowDirection = GetActorForwardVector(); // Note: This line appears in the image after the if block

        ThrowableProp->LaunchFromGhostTelekinesis(ThrowDirection.GetSafeNormal(), TelekineticThrowForce, this);

        if (AMyAIController* AIController = ResolveMyAIController())
        {
            if (!AIController->CanCurrentlySeePlayer())
            {
                AIController->ApplyInvestigationFromLocation(LastTrackedTelekineticTargetLocation, 1.f, TEXT("GhostTelekinesisThrow"));
            }
        }
    }
    else
    {
        ThrowableProp->CancelGhostTelekinesis();
    }
}
void AGhostCharacter::ConfigureGhostAttachZoneCollision() const
{
    if (!GhostAttachZoneComponent)
    {
        return;
    }

    GhostAttachZoneComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GhostAttachZoneComponent->SetCollisionObjectType(ECC_WorldDynamic);
    GhostAttachZoneComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    GhostAttachZoneComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    GhostAttachZoneComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
    GhostAttachZoneComponent->SetGenerateOverlapEvents(true);
}
void AGhostCharacter::ConfigureSoulSuckTriggerCollision() const
{
    if (!SoulSuckTriggerComponent)
    {
        return;
    }

    SoulSuckTriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SoulSuckTriggerComponent->SetCollisionObjectType(ECC_WorldDynamic);
    SoulSuckTriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    SoulSuckTriggerComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    SoulSuckTriggerComponent->SetGenerateOverlapEvents(bEnableSoulSuckOnPlayerTouch);
}

void AGhostCharacter::ConfigureTelekinesisTriggerCollision() const
{
    if (!TelekinesisTriggerComponent)
    {
        return;
    }

    TelekinesisTriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TelekinesisTriggerComponent->SetCollisionObjectType(ECC_WorldDynamic);
    TelekinesisTriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    TelekinesisTriggerComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    TelekinesisTriggerComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
    TelekinesisTriggerComponent->SetGenerateOverlapEvents(bEnableTelekineticThrowableThrow);
}
void AGhostCharacter::UpdateGhostAttachZoneDebugVisibility() const
{
    if (!GhostAttachZoneComponent)
    {
        return;
    }

    GhostAttachZoneComponent->SetHiddenInGame(!bShowGhostAttachZoneDebug);
    GhostAttachZoneComponent->SetVisibility(true);
    GhostAttachZoneComponent->ShapeColor = GhostAttachZoneDebugColor;
}
void AGhostCharacter::UpdateSoulSuckTriggerDebugVisibility() const
{
    if (!SoulSuckTriggerComponent)
    {
        return;
    }

    SoulSuckTriggerComponent->SetHiddenInGame(!bShowSoulSuckTriggerDebug);
    SoulSuckTriggerComponent->SetVisibility(true);
    SoulSuckTriggerComponent->ShapeColor = SoulSuckTriggerDebugColor;
}

void AGhostCharacter::UpdateTelekinesisTriggerDebugVisibility() const
{
    if (!TelekinesisTriggerComponent)
    {
        return;
    }

    TelekinesisTriggerComponent->SetHiddenInGame(!bShowTelekinesisTriggerDebug);
    TelekinesisTriggerComponent->SetVisibility(true);
    TelekinesisTriggerComponent->ShapeColor = TelekinesisTriggerDebugColor;
}

void AGhostCharacter::ApplyGhostTurnSettings() const
{
    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    if (!MovementComponent || !bEnableGhostTurnSmoothing)
    {
        return;
    }

    MovementComponent->bOrientRotationToMovement = true;
    MovementComponent->bUseControllerDesiredRotation = false;
    MovementComponent->RotationRate = FRotator(0.f, GhostBodyRotationRateYaw, 0.f);
}

void AGhostCharacter::FreezeSoulSuckVictim(AWomenCharacter* Victim) const
{
    if (!bFreezeVictimDuringSoulSuck || !IsValid(Victim))
    {
        return;
    }

    if (UCharacterMovementComponent* MovementComponent = Victim->GetCharacterMovement())
    {
        MovementComponent->StopMovementImmediately();
        MovementComponent->DisableMovement();
    }

    if (AController* VictimController = Victim->GetController())
    {
        VictimController->SetIgnoreMoveInput(true);
        VictimController->SetIgnoreLookInput(true);
    }
}

void AGhostCharacter::UnfreezeSoulSuckVictim(AWomenCharacter* Victim) const
{
    if (!bFreezeVictimDuringSoulSuck || !IsValid(Victim))
    {
        return;
    }

    if (UCharacterMovementComponent* MovementComponent = Victim->GetCharacterMovement())
    {
        MovementComponent->SetMovementMode(MOVE_Walking);
    }

    if (AController* VictimController = Victim->GetController())
    {
        VictimController->SetIgnoreMoveInput(false);
        VictimController->SetIgnoreLookInput(false);
    }
}
void AGhostCharacter::HandleGhostAttachZoneBeginOverlap(
    UPrimitiveComponent *OverlappedComponent,
    AActor *OtherActor,
    UPrimitiveComponent *OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult &SweepResult)
{
    (void)OverlappedComponent;
    (void)OtherComp;
    (void)OtherBodyIndex;
    (void)bFromSweep;
    (void)SweepResult;

    if (!IsValid(OtherActor) || OtherActor == this || !GhostAttachZoneComponent)
    {
        return;
    }

    AMyAIController *AIController = ResolveMyAIController();
    if (!IsValid(AIController) || !AIController->IsGhostRevealedByEffect())
    {
        return;
    }

    if (APickupActorAAASlowTalisman *SlowTalisman = Cast<APickupActorAAASlowTalisman>(OtherActor))
    {
        SlowTalisman->TryAttachToGhostZone(this, AIController, GhostAttachZoneComponent);
        return;
    }

    if (APickupActorAAARuneInstrument *RuneInstrument = Cast<APickupActorAAARuneInstrument>(OtherActor))
    {
        RuneInstrument->TryAttachToMatchedGhostZone(this, AIController, GhostAttachZoneComponent);
    }
}
void AGhostCharacter::HandleSoulSuckTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    (void)OverlappedComponent;
    (void)OtherComp;
    (void)OtherBodyIndex;
    (void)bFromSweep;
    (void)SweepResult;

    if (!bEnableSoulSuckOnPlayerTouch || bIsSoulSucking || !IsValid(OtherActor) || OtherActor == this)
    {
        return;
    }

    if (AWomenCharacter* Victim = Cast<AWomenCharacter>(OtherActor))
    {
        StartSoulSuck(Victim);
    }
}

void AGhostCharacter::HandleTelekinesisTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    (void)OverlappedComponent;
    (void)OtherComp;
    (void)OtherBodyIndex;
    (void)bFromSweep;
    (void)SweepResult;

    if (!bEnableTelekineticThrowableThrow || bIsTelekineticThrowActive || bIsSoulSucking || !IsValid(OtherActor) || OtherActor == this)
    {
        return;
    }

    if (UGhostThrowablePropComponent* ThrowableProp = OtherActor->FindComponentByClass<UGhostThrowablePropComponent>())
    {
        StartTelekineticThrowableThrow(ThrowableProp);
    }
}

void AGhostCharacter::MulticastPlayGhostKnockdownAnimation_Implementation()
{
    if (USkeletalMeshComponent* MeshComponent = GetMesh())
    {
        if (UGhostNativeAnimInstance* GhostAnimInstance = Cast<UGhostNativeAnimInstance>(MeshComponent->GetAnimInstance()))
        {
            GhostAnimInstance->PlayKnockdownAction();
        }
    }
}

void AGhostCharacter::MulticastPlayGhostSoulSuckAnimation_Implementation()
{
    if (USkeletalMeshComponent* MeshComponent = GetMesh())
    {
        if (UGhostNativeAnimInstance* GhostAnimInstance = Cast<UGhostNativeAnimInstance>(MeshComponent->GetAnimInstance()))
        {
            GhostAnimInstance->PlaySoulSuckAction();
        }
    }
}

float AGhostCharacter::ResolveSoulSuckDuration() const
{
    float ResolvedDuration = SoulSuckDuration > 0.f ? SoulSuckDuration : SoulSuckAnimation.LockDuration;
    if (ResolvedDuration <= 0.f && SoulSuckAnimation.Montage)
    {
        ResolvedDuration = SoulSuckAnimation.Montage->GetPlayLength();
    }
    else if (ResolvedDuration <= 0.f && SoulSuckAnimation.Animation)
    {
        ResolvedDuration = SoulSuckAnimation.Animation->GetPlayLength();
    }

    return ResolvedDuration;
}

float AGhostCharacter::ResolveGhostKnockdownDuration() const
{
    float ResolvedDuration = KnockdownAnimation.LockDuration;
    if (ResolvedDuration <= 0.f && KnockdownAnimation.Montage)
    {
        ResolvedDuration = KnockdownAnimation.Montage->GetPlayLength();
    }
    else if (ResolvedDuration <= 0.f && KnockdownAnimation.Animation)
    {
        ResolvedDuration = KnockdownAnimation.Animation->GetPlayLength();
    }

    return ResolvedDuration;
}

AActor* AGhostCharacter::ResolveTelekineticTargetActor() const
{
    if (const AMyAIController* AIController = ResolveMyAIController())
    {
        if (AActor* TrackedTarget = AIController->GetCurrentTargetPlayer())
        {
            return TrackedTarget;
        }
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    AActor* BestTarget = nullptr;
    float BestDistanceSq = TNumericLimits<float>::Max();
    for (TActorIterator<AWomenCharacter> It(World); It; ++It)
    {
        AWomenCharacter* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }

        const float DistanceSq = FVector::DistSquared(Candidate->GetActorLocation(), GetActorLocation());
        if (DistanceSq < BestDistanceSq)
        {
            BestDistanceSq = DistanceSq;
            BestTarget = Candidate;
        }
    }

    return BestTarget;
}

bool AGhostCharacter::RefreshTelekineticTargetLocation(FVector& OutLocation)
{
    if (IsValid(CurrentTelekineticTargetActor))
    {
        OutLocation = CurrentTelekineticTargetActor->GetActorLocation();
        LastTrackedTelekineticTargetLocation = OutLocation;
        return true;
    }

    CurrentTelekineticTargetActor = ResolveTelekineticTargetActor();
    if (IsValid(CurrentTelekineticTargetActor))
    {
        OutLocation = CurrentTelekineticTargetActor->GetActorLocation();
        LastTrackedTelekineticTargetLocation = OutLocation;
        return true;
    }

    if (const AMyAIController* AIController = ResolveMyAIController())
    {
        if (AIController->TryGetTrackedPlayerLocation(OutLocation))
        {
            LastTrackedTelekineticTargetLocation = OutLocation;
            return true;
        }
    }

    OutLocation = LastTrackedTelekineticTargetLocation;
    return !OutLocation.IsNearlyZero();
}

void AGhostCharacter::UpdateTelekineticThrowableThrow(float DeltaTime)
{
    if (!bIsTelekineticThrowActive)
    {
        return;
    }

    if (!IsValid(CurrentTelekineticThrowable) || !CurrentTelekineticThrowable->IsGhostTelekinesisActive())
    {
        StopTelekineticThrowableThrow(false);
        return;
    }

    FVector TargetLocation = LastTrackedTelekineticTargetLocation;
    RefreshTelekineticTargetLocation(TargetLocation);
    TelekineticElapsedTime += DeltaTime;
    const float HoverDuration = FMath::Max(TelekineticHoverDuration, KINDA_SMALL_NUMBER);
    const float HoverAlpha = FMath::Clamp(TelekineticElapsedTime / HoverDuration, 0.f, 1.f);
    const float BobOffset = FMath::Sin(TelekineticElapsedTime * TelekineticHoverBobFrequency * 2.f * PI) * TelekineticHoverBobAmplitude;

    FVector HoverLocation = TelekineticStartLocation;
    HoverLocation.Z += FMath::Lerp(0.f, TelekineticHoverHeight, HoverAlpha) + BobOffset;

    const FRotator HoverRotation(0.f, GetActorRotation().Yaw + TelekineticElapsedTime * 90.f, 0.f);
    CurrentTelekineticThrowable->UpdateGhostTelekinesisTransform(HoverLocation, HoverRotation);

    if (TelekineticElapsedTime >= HoverDuration)
    {
        StopTelekineticThrowableThrow(true);
    }
}
AMyAIController *AGhostCharacter::ResolveMyAIController() const
{
    return Cast<AMyAIController>(GetController());
}