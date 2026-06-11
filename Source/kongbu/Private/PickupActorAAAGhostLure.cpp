#include "PickupActorAAAGhostLure.h"

#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "MyAIController.h"

APickupActorAAAGhostLure::APickupActorAAAGhostLure()
{
    PrimaryActorTick.bCanEverTick = true;

    HoldType = EHoldItemType::GhostLure;
    FP_SocketName = TEXT("RightSocket");
    FP_LocationOffset = FVector::ZeroVector;
    FP_RotationOffset = FRotator::ZeroRotator;
    TP_SocketName = TEXT("RightSocket");
    TP_LocationOffset = FVector::ZeroVector;
    TP_RotationOffset = FRotator::ZeroRotator;

    ItemMassKg = 1.0f;
    ItemThrowForceMultiplier = 0.7f;
    ItemLinearDamping = 0.1f;
    ItemAngularDamping = 0.35f;
    ItemThrowSpinRateDegrees = 800.f;

    bAllowPawnCollision = true;
    bAllowPhysicsBodyCollision = true;
    ApplyReleasedCollisionProfile();

    Tags.Add(FName("GhostLure"));
    Tags.Add(FName("Pickup"));
    Tags.Add(FName("Collectible"));
}

void APickupActorAAAGhostLure::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bLureActive)
    {
        return;
    }

    PulseAccumulator += DeltaTime;
    PulseVisualTime += DeltaTime;

    if (bEnableDebug)
    {
        DrawLureDebug();
    }

    if (PulseAccumulator >= LurePulseInterval)
    {
        PulseAccumulator = 0.f;
        EmitLurePulse();
    }
}

void APickupActorAAAGhostLure::ActivateLure()
{
    if (IsDisabledByRage())
    {
        DeactivateLure();
        return;
    }

    bClosedByPlayer = false;
    bLureActive = true;
    PulseAccumulator = 0.f;
    PulseVisualTime = 0.f;
    GetWorldTimerManager().ClearTimer(DelayedActivationHandle);
    EmitLurePulse();
}

void APickupActorAAAGhostLure::DeactivateLure()
{
    bLureActive = false;
    PulseAccumulator = 0.f;
    GetWorldTimerManager().ClearTimer(DelayedActivationHandle);
}

void APickupActorAAAGhostLure::EmitLurePulse()
{
    if (!bLureActive || !GetWorld())
    {
        return;
    }

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

        if (IsActorInsideLureRadius(TargetPawn))
        {
            continue;
        }

        if (bRequireLineOfSightToAffectTargets && !HasLineOfSightToTarget(TargetPawn))
        {
            continue;
        }

        AIController->ApplyLureFromLocation(GetActorLocation(), DazeDuration, LureAcceptanceRadius, this);
    }
}

void APickupActorAAAGhostLure::OnPickedUp()
{
    Super::OnPickedUp();
    DeactivateLure();
}

void APickupActorAAAGhostLure::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    Super::OnPutDown(PlaceLocation, PlaceRotation);

    if (bClosedByPlayer || !bAutoActivateWhenPlaced)
    {
        DeactivateLure();
        return;
    }

    ActivateLure();
}

void APickupActorAAAGhostLure::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    Super::OnThrown(ThrowDirection, ThrowForce);

    if (bClosedByPlayer || !bAutoActivateWhenThrown)
    {
        DeactivateLure();
        return;
    }

    if (ThrowActivationDelay <= 0.f)
    {
        ActivateLure();
        return;
    }

    GetWorldTimerManager().SetTimer(
        DelayedActivationHandle,
        this,
        &APickupActorAAAGhostLure::ActivateLure,
        ThrowActivationDelay,
        false
    );
}

bool APickupActorAAAGhostLure::CanBeClosedByPlayer_Implementation() const
{
    return true;
}

void APickupActorAAAGhostLure::CloseByPlayer_Implementation(AActor* ClosingActor)
{
    bClosedByPlayer = true;
    DeactivateLure();
}

bool APickupActorAAAGhostLure::IsClosedByPlayer_Implementation() const
{
    return bClosedByPlayer || !bLureActive;
}

void APickupActorAAAGhostLure::OpenByPlayer_Implementation(AActor* OpeningActor)
{
    ActivateLure();
}

bool APickupActorAAAGhostLure::IsActorInsideLureRadius(const AActor* TargetActor) const
{
    if (!IsValid(TargetActor))
    {
        return false;
    }

    FVector ToTarget = TargetActor->GetActorLocation() - GetActorLocation();
    ToTarget.Z = 0.f;
    return ToTarget.SizeSquared() <= FMath::Square(LureRadius);
}

// bool APickupActorAAAGhostLure::HasLineOfSightToTarget(const AActor* TargetActor) const
// {
//     if (!GetWorld() || !IsValid(TargetActor))
//     {
//         return false;
//     }

//     FVector TargetOrigin;
//     // Additional logic for line of sight can be added here
//     return true; // Placeholder return value
// }

bool APickupActorAAAGhostLure::HasLineOfSightToTarget(const AActor* TargetActor) const
{
    if (!GetWorld() || !IsValid(TargetActor))
    {
        return false;
    }

    FVector TargetOrigin;
    FVector TargetExtent;
    TargetActor->GetActorBounds(true, TargetOrigin, TargetExtent);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GhostLureLineOfSight), false, this);
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(TargetActor);

    FHitResult HitResult;
    return !GetWorld()->LineTraceSingleByChannel(
        HitResult,
        GetActorLocation() + FVector(0.f, 0.f, 20.f),
        TargetOrigin,
        BlockTraceChannel,
        QueryParams
    );
}

void APickupActorAAAGhostLure::DrawLureDebug() const
{
    if (!GetWorld())
    {
        return;
    }

    const float PulseAlpha = FMath::Abs(FMath::Sin(PulseVisualTime * 3.f));
    const float AnimatedRadius = FMath::Max(24.f, LureRadius * PulseAlpha);

    DrawDebugSphere(GetWorld(), GetActorLocation(), LureRadius, 24, DebugColor, false, -1.f, 0, 1.5f);
    DrawDebugSphere(GetWorld(), GetActorLocation(), AnimatedRadius, 20, FColor::White, false, -1.f, 0, 1.0f);
    DrawDebugSphere(GetWorld(), GetActorLocation(), LureAcceptanceRadius, 16, FColor::Yellow, false, -1.f, 0, 1.5f);
}