#include "PickupActorAAAStunSpeaker.h"

#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "MyAIController.h"

APickupActorAAAStunSpeaker::APickupActorAAAStunSpeaker()
{
    PrimaryActorTick.bCanEverTick = true;

    HoldType = EHoldItemType::Speaker;
    FP_SocketName = TEXT("RightSocket");
    FP_LocationOffset = FVector::ZeroVector;
    FP_RotationOffset = FRotator::ZeroRotator;
    TP_SocketName = TEXT("RightSocket");
    TP_LocationOffset = FVector::ZeroVector;
    TP_RotationOffset = FRotator::ZeroRotator;

    ItemMassKg = 3.5f;
    ItemThrowForceMultiplier = 1.0f;
    ItemLinearDamping = 0.18f;
    ItemAngularDamping = 0.65f;
    ItemThrowSpinRateDegrees = 350.f;

    bAllowPawnCollision = true;
    bAllowPhysicsBodyCollision = true;
    ApplyReleasedCollisionProfile();

    Tags.Add(FName("StunSpeaker"));
    Tags.Add(FName("Pickup"));
    Tags.Add(FName("Collectible"));
}

void APickupActorAAAStunSpeaker::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bSpeakerActive)
    {
        return;
    }

    PulseVisualTime += DeltaTime;
    PulseAccumulator += DeltaTime;

    if (bEnableSpeakerDebug)
    {
        DrawSpeakerDebug();
    }

    if (PulseAccumulator >= StunPulseInterval)
    {
        PulseAccumulator = 0.f;
        EmitStunPulse();
    }
}

void APickupActorAAAStunSpeaker::ActivateSpeaker()
{
    if (IsDisabledByRage())
    {
        DeactivateSpeaker();
        return;
    }

    bClosedByPlayer = false;
    bSpeakerActive = true;
    PulseVisualTime = 0.f;
    PulseAccumulator = 0.f;

    GetWorldTimerManager().ClearTimer(DelayedSpeakerActivationHandle);

    EmitStunPulse();
    UE_LOG(LogTemp, Log, TEXT("Stun Speaker Activated"));
}

void APickupActorAAAStunSpeaker::DeactivateSpeaker()
{
    bSpeakerActive = false;
    PulseAccumulator = 0.f;
    GetWorldTimerManager().ClearTimer(DelayedSpeakerActivationHandle);
    UE_LOG(LogTemp, Log, TEXT("Stun Speaker Deactivated"));
}

void APickupActorAAAStunSpeaker::EmitStunPulse()
{
    if (!bSpeakerActive || !GetWorld())
    {
        return;
    }

    if (!bAffectAIControllers)
    {
        return;
    }

    FFearStimulus Stimulus;
    Stimulus.SourceLocation = GetActorLocation();
    Stimulus.Duration = AIStunDuration;
    Stimulus.StimulusTag = SpeakerStimulusTag;
    Stimulus.SourceActor = this;
    Stimulus.RageAngerAmount = IsHeldByPlayer()
        ? FMath::Max(1, HeldFearAngerAmount)
        : FMath::Max(1, PlacedFearAngerAmount);

    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        APawn* TargetPawn = *It;
        if (!IsValid(TargetPawn))
        {
            continue;
        }

        AMyAIController* AIController = Cast<AMyAIController>(TargetPawn->GetController());
        if (!AIController)
        {
            continue;
        }

        if (!IsActorInsideStunRadius(TargetPawn))
        {
            continue;
        }

        if (bRequireLineOfSightToAffectTargets && !HasLineOfSightToTarget(TargetPawn))
        {
            continue;
        }

        AIController->ApplyFearAngerStimulus(Stimulus);
        AIController->ApplyStunFromSource(AIStunDuration, this);
    }
}

void APickupActorAAAStunSpeaker::OnPickedUp()
{
    Super::OnPickedUp();
    DeactivateSpeaker();

    UE_LOG(LogTemp, Log, TEXT("APickupActorAAAStunSpeaker Picked Up"));
}

void APickupActorAAAStunSpeaker::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    Super::OnPutDown(PlaceLocation, PlaceRotation);

    if (bClosedByPlayer)
    {
        DeactivateSpeaker();
        UE_LOG(LogTemp, Log, TEXT("APickupActorAAAStunSpeaker remains closed after put down"));
        return;
    }

    if (bAutoActivateSpeakerWhenPlaced)
    {
        ActivateSpeaker();
    }
    else
    {
        DeactivateSpeaker();
    }

    UE_LOG(LogTemp, Log, TEXT("APickupActorAAAStunSpeaker Put Down"));
}

void APickupActorAAAStunSpeaker::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    Super::OnThrown(ThrowDirection, ThrowForce);

    if (bClosedByPlayer || !bAutoActivateSpeakerWhenThrown)
    {
        DeactivateSpeaker();
        return;
    }

    if (ThrowSpeakerActivationDelay <= 0.f)
    {
        ActivateSpeaker();
        return;
    }

    GetWorldTimerManager().SetTimer(
        DelayedSpeakerActivationHandle,
        this,
        &APickupActorAAAStunSpeaker::ActivateSpeaker,
        ThrowSpeakerActivationDelay,
        false);
}

bool APickupActorAAAStunSpeaker::CanBeClosedByPlayer_Implementation() const
{
    return true;
}

void APickupActorAAAStunSpeaker::CloseByPlayer_Implementation(AActor* ClosingActor)
{
    bClosedByPlayer = true;
    DeactivateSpeaker();

    UE_LOG(LogTemp, Log, TEXT("%s stun speaker closed by %s"),
        *GetName(),
        *GetNameSafe(ClosingActor));
}

bool APickupActorAAAStunSpeaker::IsClosedByPlayer_Implementation() const
{
    return bClosedByPlayer || !bSpeakerActive;
}

void APickupActorAAAStunSpeaker::OpenByPlayer_Implementation(AActor* OpeningActor)
{
    ActivateSpeaker();

    if (!bSpeakerActive)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("%s stun speaker opened by %s"),
        *GetName(),
        *GetNameSafe(OpeningActor));
}

void APickupActorAAAStunSpeaker::DisableByRage_Implementation(AActor* DisablingActor)
{
    Super::DisableByRage_Implementation(DisablingActor);
    bClosedByPlayer = true;
    DeactivateSpeaker();

    UE_LOG(LogTemp, Warning, TEXT("%s stun speaker disabled by Rage actor %s"),
        *GetName(),
        *GetNameSafe(DisablingActor));
}

void APickupActorAAAStunSpeaker::DrawSpeakerDebug()
{
    if (!GetWorld())
    {
        return;
    }

    const FVector SpeakerLocation = GetActorLocation();
    const float PulseAlpha = FMath::Abs(FMath::Sin(PulseVisualTime * 3.f));
    const float AnimatedRadius = FMath::Max(24.f, StunRadius * PulseAlpha);

    DrawDebugSphere(
        GetWorld(),
        SpeakerLocation,
        StunRadius,
        24,
        SpeakerDebugColor,
        false,
        -1.f,
        0,
        1.5f);

    DrawDebugSphere(
        GetWorld(),
        SpeakerLocation,
        AnimatedRadius,
        20,
        FColor::White,
        false,
        -1.f,
        0,
        1.0f);

    DrawDebugSphere(
        GetWorld(),
        SpeakerLocation,
        18.f,
        12,
        SpeakerDebugColor,
        false,
        -1.f,
        0,
        2.f);
}

bool APickupActorAAAStunSpeaker::IsActorInsideStunRadius(const AActor* TargetActor) const
{
    if (!IsValid(TargetActor))
    {
        return false;
    }

    FVector ToTarget = TargetActor->GetActorLocation() - GetActorLocation();
    ToTarget.Z = 0.f;

    return ToTarget.SizeSquared() <= FMath::Square(StunRadius);
}

bool APickupActorAAAStunSpeaker::HasLineOfSightToTarget(const AActor* TargetActor) const
{
    if (!GetWorld() || !IsValid(TargetActor))
    {
        return false;
    }

    FVector TargetOrigin;
    FVector TargetExtent;
    TargetActor->GetActorBounds(true, TargetOrigin, TargetExtent);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(StunSpeakerLineOfSight), false, this);
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(TargetActor);

    FHitResult HitResult;
    const FVector TraceStart = GetActorLocation() + FVector(0.f, 0.f, 20.f);
    const FVector TraceEnd = TargetOrigin;

    return !GetWorld()->LineTraceSingleByChannel(
        HitResult,
        TraceStart,
        TraceEnd,
        SpeakerBlockTraceChannel,
        QueryParams);
}