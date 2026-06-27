#include "PickupActorAAARuneDoorLockCard.h"
#include "ConfigurableDoorActor.h"
#include "Components/PrimitiveComponent.h"

APickupActorAAARuneDoorLockCard::APickupActorAAARuneDoorLockCard()
{
    Tags.Add(FName("RuneDoorLockCard"));
    Tags.Add(FName("Rune"));
    Tags.Add(FName("DoorLock"));

    FRuneCanvasPattern DefaultDoorLockPattern;
    DefaultDoorLockPattern.PatternId = TEXT("DoorLock");
    DefaultDoorLockPattern.NodeSequence = {
        250, 251, 252, 253, 254, 255, 256,
        296, 336, 376, 416, 456, 496,
        495, 494, 493, 492, 491, 490,
        450, 410, 370, 330, 290};
    DoorLockCardExpectedSequences.Add(DefaultDoorLockPattern);
}

void APickupActorAAARuneDoorLockCard::OnPickedUp()
{
    PendingDoorTarget.Reset();
    LockedDoorTarget.Reset();
    bHasCachedThrowSimilarityMultiplier = false;
    CachedThrowSimilarityMultiplier = 0.f;
    Super::OnPickedUp();
}

void APickupActorAAARuneDoorLockCard::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    PendingDoorTarget.Reset();
    LockedDoorTarget.Reset();
    bHasCachedThrowSimilarityMultiplier = false;
    CachedThrowSimilarityMultiplier = 0.f;
    Super::OnPutDown(PlaceLocation, PlaceRotation);
}

void APickupActorAAARuneDoorLockCard::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    PendingDoorTarget.Reset();
    LockedDoorTarget.Reset();
    CachedThrowSimilarityMultiplier = FMath::Clamp(GetCurrentCardSequenceSimilarityPercent() / 100.f, 0.f, 1.f);
    bHasCachedThrowSimilarityMultiplier = true;
    Super::OnThrown(ThrowDirection, ThrowForce);
}

void APickupActorAAARuneDoorLockCard::OnRuneCanvasAttachedToSurface()
{
    AConfigurableDoorActor *DoorActor = PendingDoorTarget.Get();
    if (!IsValid(DoorActor))
    {
        return;
    }

    const float ScaledLockDuration = GetScaledDoorLockDuration();
    if (ScaledLockDuration <= UE_KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s attached to door %s but rune similarity is 0%%; door lock has no effect"),
               *GetName(),
               *GetNameSafe(DoorActor));
        return;
    }

    LockedDoorTarget = DoorActor;
    DoorActor->LockDoorClosedForDuration(ScaledLockDuration);

    UE_LOG(LogTemp, Log, TEXT("%s locked door %s closed for %.2fs (similarity %.1f%%, max %.2fs)"),
           *GetName(),
           *GetNameSafe(DoorActor),
           ScaledLockDuration,
           GetSimilarityMultiplier() * 100.f,
           FMath::Max(0.f, DoorLockDuration));
}

void APickupActorAAARuneDoorLockCard::OnRuneCanvasDetachedFromSurface()
{
    PendingDoorTarget.Reset();
    LockedDoorTarget.Reset();
}

bool APickupActorAAARuneDoorLockCard::TryHandleRuneCanvasThrownImpact(const FHitResult &Hit, UPrimitiveComponent *HitComponent)
{
    PendingDoorTarget.Reset();

    AActor *HitActor = Hit.GetActor();
    AConfigurableDoorActor *DoorActor = Cast<AConfigurableDoorActor>(HitActor);
    if (!DoorActor && IsValid(HitComponent))
    {
        DoorActor = Cast<AConfigurableDoorActor>(HitComponent->GetOwner());
    }

    if (IsValid(DoorActor))
    {
        PendingDoorTarget = DoorActor;
    }

    return false;
}

const TArray<int32> &APickupActorAAARuneDoorLockCard::GetExpectedNodeSequenceForCurrentCard() const
{
    static const TArray<int32> EmptySequence;
    const int32 CardResourceIndex = GetCurrentCardResourceIndex();
    return DoorLockCardExpectedSequences.IsValidIndex(CardResourceIndex)
               ? DoorLockCardExpectedSequences[CardResourceIndex].NodeSequence
               : EmptySequence;
}

float APickupActorAAARuneDoorLockCard::GetSimilarityMultiplier() const
{
    if (bHasCachedThrowSimilarityMultiplier)
    {
        return FMath::Clamp(CachedThrowSimilarityMultiplier, 0.f, 1.f);
    }

    return FMath::Clamp(GetCurrentCardSequenceSimilarityPercent() / 100.f, 0.f, 1.f);
}

float APickupActorAAARuneDoorLockCard::GetScaledDoorLockDuration() const
{
    return FMath::Max(0.f, DoorLockDuration) * GetSimilarityMultiplier();
}
