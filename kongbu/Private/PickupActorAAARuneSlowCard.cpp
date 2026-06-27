#include "PickupActorAAARuneSlowCard.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GhostCharacter.h"
#include "MyAIController.h"
#include "TimerManager.h"

APickupActorAAARuneSlowCard::APickupActorAAARuneSlowCard()
{
    Tags.Add(FName("RuneSlowCard"));
    Tags.Add(FName("Rune"));
    Tags.Add(FName("Slow"));

    FRuneCanvasPattern DefaultsSlowPattern;
    DefaultsSlowPattern.PatternId = TEXT("Slow");
    DefaultsSlowPattern.NodeSequence = {
        370, 371, 372, 373, 374, 375, 376, 377,
        337, 297, 257, 217, 177, 137,
        178, 219, 260, 301, 342, 383,
        424, 465, 506, 547};
    SlowCardExpectedSequences.Add(DefaultsSlowPattern);
}

void APickupActorAAARuneSlowCard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(SlowDurationHandle);
    ClearAttachedSlowEffect();
    Super::EndPlay(EndPlayReason);
}

void APickupActorAAARuneSlowCard::OnPickedUp()
{
    GetWorldTimerManager().ClearTimer(SlowDurationHandle);
    ClearAttachedSlowEffect();
    bAttachedToGhost = false;
    bCanAttachToGhostAfterThrow = false;
    bHasCachedThrowSimilarityMultiplier = false;
    CachedThrowSimilarityMultiplier = 0.f;
    Super::OnPickedUp();
}

void APickupActorAAARuneSlowCard::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    GetWorldTimerManager().ClearTimer(SlowDurationHandle);
    ClearAttachedSlowEffect();
    bAttachedToGhost = false;
    bCanAttachToGhostAfterThrow = false;
    bHasCachedThrowSimilarityMultiplier = false;
    CachedThrowSimilarityMultiplier = 0.f;
    Super::OnPutDown(PlaceLocation, PlaceRotation);
}

void APickupActorAAARuneSlowCard::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    GetWorldTimerManager().ClearTimer(SlowDurationHandle);
    ClearAttachedSlowEffect();
    bAttachedToGhost = false;
    bCanAttachToGhostAfterThrow = true;
    CachedThrowSimilarityMultiplier = FMath::Clamp(GetCurrentCardSequenceSimilarityPercent() / 100.f, 0.f, 1.f);
    bHasCachedThrowSimilarityMultiplier = true;
    Super::OnThrown(ThrowDirection, ThrowForce);
}

bool APickupActorAAARuneSlowCard::TryAttachToGhostZone(
    APawn *TargetPawn,
    AMyAIController *TargetController,
    USceneComponent *AttachComponent)
{
    if (!bCanAttachToGhostAfterThrow || bAttachedToGhost || !IsValid(TargetPawn) || !IsValid(TargetController))
    {
        return false;
    }

    AttachToGhost(TargetPawn, TargetController, AttachComponent);
    return bAttachedToGhost;
}

bool APickupActorAAARuneSlowCard::TryHandleRuneCanvasThrownImpact(const FHitResult &Hit, UPrimitiveComponent *HitComponent)
{
    (void)HitComponent;

    APawn *TargetPawn = nullptr;
    AMyAIController *TargetController = nullptr;
    if (!TryResolveGhost(Hit.GetActor(), TargetPawn, TargetController))
    {
        UE_LOG(LogTemp, Verbose, TEXT("%s slow card hit non-ghost target %s and fizzled"),
               *GetName(),
               *GetNameSafe(Hit.GetActor()));
        Destroy();
        return true;
    }

    USceneComponent *AttachComponent = nullptr;
    if (AGhostCharacter *GhostCharacter = Cast<AGhostCharacter>(TargetPawn))
    {
        AttachComponent = GhostCharacter->GetGhostAttachZoneComponent();
    }

    AttachToGhost(TargetPawn, TargetController, AttachComponent ? AttachComponent : Hit.GetComponent());
    return true;
}

const TArray<int32> &APickupActorAAARuneSlowCard::GetExpectedNodeSequenceForCurrentCard() const
{
    static const TArray<int32> EmptySequence;
    const int32 CardResourceIndex = GetCurrentCardResourceIndex();
    return SlowCardExpectedSequences.IsValidIndex(CardResourceIndex)
               ? SlowCardExpectedSequences[CardResourceIndex].NodeSequence
               : EmptySequence;
}

bool APickupActorAAARuneSlowCard::TryResolveGhost(
    AActor *OtherActor,
    APawn *&OutPawn,
    AMyAIController *&OutController) const
{
    OutPawn = nullptr;
    OutController = nullptr;

    if (!IsValid(OtherActor) || OtherActor == this)
    {
        return false;
    }

    APawn *TargetPawn = Cast<APawn>(OtherActor);
    if (!IsValid(TargetPawn))
    {
        return false;
    }

    AMyAIController *TargetController = Cast<AMyAIController>(TargetPawn->GetController());
    if (!IsValid(TargetController))
    {
        return false;
    }

    OutPawn = TargetPawn;
    OutController = TargetController;
    return true;
}

float APickupActorAAARuneSlowCard::ResolveScaledSlowDuration() const
{
    const float SimilarityMultiplier = bHasCachedThrowSimilarityMultiplier
                                        ? FMath::Clamp(CachedThrowSimilarityMultiplier, 0.f, 1.f)
                                        : FMath::Clamp(GetCurrentCardSequenceSimilarityPercent() / 100.f, 0.f, 1.f);

    return FMath::Max(0.f, SlowDuration) * SimilarityMultiplier;
}

void APickupActorAAARuneSlowCard::AttachToGhost(
    APawn *TargetPawn,
    AMyAIController *TargetController,
    USceneComponent *AttachComponent)
{
    if (!MeshComponent || bAttachedToGhost || !IsValid(TargetPawn) || !IsValid(TargetController))
    {
        return;
    }

    const float ScaledSlowDuration = ResolveScaledSlowDuration();
    if (ScaledSlowDuration <= UE_KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s hit ghost %s but rune similarity is 0%%; slow card has no effect"),
               *GetName(),
               *GetNameSafe(TargetPawn));
        Destroy();
        return;
    }

    bCanAttachToGhostAfterThrow = false;

    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    MeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
    MeshComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    MeshComponent->SetSimulatePhysics(false);
    MeshComponent->SetEnableGravity(false);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetGenerateOverlapEvents(false);

    USceneComponent *ResolvedAttachComponent = AttachComponent;
    if (IsValid(ResolvedAttachComponent))
    {
        ResolvedAttachComponent = TargetPawn->GetRootComponent();
        if (const ACharacter *CharacterTarget = Cast<ACharacter>(TargetPawn))
        {
            if (IsValid(CharacterTarget->GetMesh()))
            {
                ResolvedAttachComponent = CharacterTarget->GetMesh();
            }
        }
    }

    if (IsValid(ResolvedAttachComponent))
    {
        ApplyGhostAttachPose(TargetPawn, ResolvedAttachComponent);
    }
    else
    {
        AttachToActor(TargetPawn, FAttachmentTransformRules::KeepWorldTransform);
    }

    bAttachedToGhost = true;
    AttachedController = TargetController;
    AttachedTargetActor = TargetPawn;
    TargetController->ApplySpeedReductionSource(this, SpeedReductionAmount);

    GetWorldTimerManager().ClearTimer(SlowDurationHandle);
    GetWorldTimerManager().SetTimer(
        SlowDurationHandle,
        this,
        &APickupActorAAARuneSlowCard::ExpireSlowCard,
        ScaledSlowDuration,
        false);

    UE_LOG(LogTemp, Log, TEXT("%s attached to ghost %s and slowed it for %.2fs (similarity %.1f%%, max %.2fs, reduction %.1f)"),
           *GetName(),
           *GetNameSafe(TargetPawn),
           ScaledSlowDuration,
           GetCurrentCardSequenceSimilarityPercent(),
           FMath::Max(0.f, SlowDuration),
           FMath::Max(0.f, SpeedReductionAmount));
}

void APickupActorAAARuneSlowCard::ApplyGhostAttachPose(APawn *TargetPawn, USceneComponent *AttachComponent)
{
    if (!MeshComponent || !IsValid(TargetPawn) || !IsValid(AttachComponent))
    {
        return;
    }

    FVector BoundsOrigin = FVector::ZeroVector;
    FVector BoundsExtent = FVector::ZeroVector;
    if (const UPrimitiveComponent *AttachPrimitive = Cast<UPrimitiveComponent>(AttachComponent))
    {
        BoundsOrigin = AttachPrimitive->Bounds.Origin;
        BoundsExtent = AttachPrimitive->Bounds.BoxExtent;
    }
    else
    {
        TargetPawn->GetActorBounds(true, BoundsOrigin, BoundsExtent);
    }

    const float HorizontalExtent = FMath::Max(BoundsExtent.X, BoundsExtent.Y);
    const float HorizontalRadius = FMath::Max(18.f, HorizontalExtent * GhostAttachHorizontalRadiusRatio);
    const float VerticalMin = -BoundsExtent.Z + BoundsExtent.Z * 2.f * GhostAttachVerticalMinRatio;
    const float VerticalMax = -BoundsExtent.Z + BoundsExtent.Z * 2.f * GhostAttachVerticalMaxRatio;

    const float RandomYawDegrees = FMath::FRandRange(0.f, 360.f);
    FVector HorizontalDirection = FRotator(0.f, RandomYawDegrees, 0.f).Vector().GetSafeNormal2D();
    if (HorizontalDirection.IsNearlyZero())
    {
        HorizontalDirection = TargetPawn->GetActorForwardVector().GetSafeNormal2D();
    }
    if (HorizontalDirection.IsNearlyZero())
    {
        HorizontalDirection = FVector::ForwardVector;
    }

    const float HeightOffset = FMath::FRandRange(FMath::Min(VerticalMin, VerticalMax), FMath::Max(VerticalMin, VerticalMax));
    FVector SurfacePoint = BoundsOrigin + HorizontalDirection * HorizontalRadius;
    SurfacePoint.Z += HeightOffset;

    FVector SurfaceNormal = SurfacePoint - BoundsOrigin;
    if (SurfaceNormal.IsNearlyZero())
    {
        SurfaceNormal = -TargetPawn->GetActorForwardVector();
    }
    SurfaceNormal.Normalize();

    const FRotator AttachRotation = (-SurfaceNormal).Rotation();

    FVector LocalBoundsMin = FVector::ZeroVector;
    FVector LocalBoundsMax = FVector::ZeroVector;
    MeshComponent->GetLocalBounds(LocalBoundsMin, LocalBoundsMax);
    const FVector LocalBoundsCenter = (LocalBoundsMin + LocalBoundsMax) * 0.5f;
    const FVector MeshScale = MeshComponent->GetComponentScale();
    const FVector MeshCenterOffset = AttachRotation.RotateVector(LocalBoundsCenter * MeshScale);
    const FVector AttachLocation = SurfacePoint - MeshCenterOffset + SurfaceNormal * GhostAttachSurfacePadding;

    SetActorLocationAndRotation(AttachLocation, AttachRotation, false, nullptr, ETeleportType::TeleportPhysics);
    AttachToComponent(AttachComponent, FAttachmentTransformRules::KeepWorldTransform);
}

void APickupActorAAARuneSlowCard::ExpireSlowCard()
{
    ClearAttachedSlowEffect();
    Destroy();
}

void APickupActorAAARuneSlowCard::ClearAttachedSlowEffect()
{
    if (AMyAIController *TargetController = AttachedController.Get())
    {
        TargetController->RemoveSpeedReductionSource(this);
    }

    AttachedController.Reset();
    AttachedTargetActor.Reset();
    bAttachedToGhost = false;
}
