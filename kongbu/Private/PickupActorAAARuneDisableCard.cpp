#include "PickupActorAAARuneDisableCard.h"

#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GhostCharacter.h"
#include "MyAIController.h"
#include "TimerManager.h"

APickupActorAAARuneDisableCard::APickupActorAAARuneDisableCard()
{
    Tags.Add(FName("RuneDisableCard"));
    Tags.Add(FName("Rune"));
    Tags.Add(FName("Disable"));

    FRuneCanvasPattern DefaultDisablePattern;
    DefaultDisablePattern.PatternId = TEXT("Disable");
    DefaultDisablePattern.NodeSequence = {69, 68, 67, 106, 105, 104, 103, 102, 141, 140, 139, 138, 137, 176, 175, 174, 173, 172, 211, 210, 209, 208, 207, 206, 84, 85, 86, 87, 88, 89, 90, 91, 92, 133, 134, 135, 136, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 229, 230, 231, 232, 233, 234, 235, 276};
    DisableCardExpectedSequences.Add(DefaultDisablePattern);
}

void APickupActorAAARuneDisableCard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(DisableDurationHandle);
    ClearAttachedDisableEffect();
    Super::EndPlay(EndPlayReason);
}

void APickupActorAAARuneDisableCard::OnPickedUp()
{
    GetWorldTimerManager().ClearTimer(DisableDurationHandle);
    ClearAttachedDisableEffect();
    bAttachedToGhost = false;
    bCanAttachToGhostAfterThrow = false;
    bHasCachedThrowSimilarityMultiplier = false;
    CachedThrowSimilarityMultiplier = 0.f;
    Super::OnPickedUp();
}

void APickupActorAAARuneDisableCard::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    GetWorldTimerManager().ClearTimer(DisableDurationHandle);
    ClearAttachedDisableEffect();
    bAttachedToGhost = false;
    bCanAttachToGhostAfterThrow = false;
    bHasCachedThrowSimilarityMultiplier = false;
    CachedThrowSimilarityMultiplier = 0.f;
    Super::OnPutDown(PlaceLocation, PlaceRotation);
}

void APickupActorAAARuneDisableCard::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    GetWorldTimerManager().ClearTimer(DisableDurationHandle);
    ClearAttachedDisableEffect();
    bAttachedToGhost = false;
    bCanAttachToGhostAfterThrow = true;
    CachedThrowSimilarityMultiplier = FMath::Clamp(GetCurrentCardSequenceSimilarityPercent() / 100.f, 0.f, 1.f);
    bHasCachedThrowSimilarityMultiplier = true;
    Super::OnThrown(ThrowDirection, ThrowForce);
}

bool APickupActorAAARuneDisableCard::TryAttachToGhostZone(
    APawn *TargetPawn,
    AMyAIController *TargetController,
    UPrimitiveComponent *AttachComponent)
{
    if (!bCanAttachToGhostAfterThrow || bAttachedToGhost || !IsValid(TargetPawn) || !IsValid(TargetController))
    {
        return false;
    }

    AttachToGhost(TargetPawn, TargetController, AttachComponent);
    return bAttachedToGhost;
}

bool APickupActorAAARuneDisableCard::TryHandleRuneCanvasThrownImpact(const FHitResult &Hit, UPrimitiveComponent *HitComponent)
{
    (void)HitComponent;

    APawn *TargetPawn = nullptr;
    AMyAIController *TargetController = nullptr;
    if (!TryResolveGhost(Hit.GetActor(), TargetPawn, TargetController))
    {
        UE_LOG(LogTemp, Verbose, TEXT("%s disable card hit non-ghost target %s and fizzled"),
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

const TArray<int32> &APickupActorAAARuneDisableCard::GetExpectedNodeSequenceForCurrentCard() const
{
    static const TArray<int32> EmptySequence;
    const int32 CardResourceIndex = GetCurrentCardResourceIndex();
    return DisableCardExpectedSequences.IsValidIndex(CardResourceIndex)
               ? DisableCardExpectedSequences[CardResourceIndex].NodeSequence
               : EmptySequence;
}

bool APickupActorAAARuneDisableCard::TryResolveGhost(
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

float APickupActorAAARuneDisableCard::ResolveScaledDisableDuration() const
{
    const float SimilarityMultiplier = bHasCachedThrowSimilarityMultiplier
                                           ? FMath::Clamp(CachedThrowSimilarityMultiplier, 0.f, 1.f)
                                           : FMath::Clamp(GetCurrentCardSequenceSimilarityPercent() / 100.f, 0.f, 1.f);
    return FMath::Max(0.f, DisableDuration) * SimilarityMultiplier;
}

void APickupActorAAARuneDisableCard::AttachToGhost(
    APawn *TargetPawn,
    AMyAIController *TargetController,
    USceneComponent *AttachComponent)
{
    if (!MeshComponent || bAttachedToGhost || !IsValid(TargetPawn) || !IsValid(TargetController))
    {
        return;
    }

    const float ScaledDisableDuration = ResolveScaledDisableDuration();
    if (ScaledDisableDuration <= UE_KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s hit ghost %s but rune similarity is 0%%; disable card has no effect"),
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
    if (!IsValid(ResolvedAttachComponent))
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
    TargetController->ApplyAttachedDisableSource(this);

    GetWorldTimerManager().ClearTimer(DisableDurationHandle);
    GetWorldTimerManager().SetTimer(
        DisableDurationHandle,
        this,
        &APickupActorAAARuneDisableCard::ExpireDisableCard,
        ScaledDisableDuration,
        false);

    UE_LOG(LogTemp, Log, TEXT("%s attached to ghost %s and disabled it for %.2fs (similarity %.1f%%, max %.2fs)"),
           *GetName(),
           *GetNameSafe(TargetPawn),
           ScaledDisableDuration,
           GetCurrentCardSequenceSimilarityPercent(),
           FMath::Max(0.f, DisableDuration));
}

void APickupActorAAARuneDisableCard::ApplyGhostAttachPose(APawn *TargetPawn, USceneComponent *AttachComponent)
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

void APickupActorAAARuneDisableCard::ExpireDisableCard()
{
    ClearAttachedDisableEffect();
    Destroy();
}

void APickupActorAAARuneDisableCard::ClearAttachedDisableEffect()
{
    if (AMyAIController *TargetController = AttachedController.Get())
    {
        TargetController->RemoveAttachedDisableSource(this);
    }

    AttachedController.Reset();
    AttachedTargetActor.Reset();
    bAttachedToGhost = false;
}