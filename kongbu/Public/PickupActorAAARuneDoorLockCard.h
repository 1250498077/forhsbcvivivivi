#pragma once

#include "CoreMinimal.h"
#include "PickupActorAAARuneCanvascommonInstrument.h"
#include "PickupActorAAARuneDoorLockCard.generated.h"

class AConfigurableDoorActor;
class UPrimitiveComponent;
struct FHitResult;

UCLASS()
class KONGBU_API APickupActorAAARuneDoorLockCard : public APickupActorAAARuneCanvascommonInstrument
{
    GENERATED_BODY()

public:
    APickupActorAAARuneDoorLockCard();

    virtual void OnPickedUp() override;
    virtual void OnPutDown(FVector PlaceLocation, FRotator PlaceRotation) override;
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce) override;

protected:
    virtual void OnRuneCanvasAttachedToSurface() override;
    virtual void OnRuneCanvasDetachedFromSurface() override;
    virtual bool TryHandleRuneCanvasThrownImpact(const FHitResult &Hit, UPrimitiveComponent *HitComponent) override;
    virtual const TArray<int32> &GetExpectedNodeSequenceForCurrentCard() const override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|DoorLock|Card", meta = (TitleProperty = "Pattern"))
    TArray<FRuneCanvasPattern> DoorLockCardExpectedSequences;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|DoorLock", meta = (ClampMin = "0.0", DisplayName = ""))
    float DoorLockDuration = 20.f;

private:
    bool bHasCachedThrowSimilarityMultiplier = false;
    float CachedThrowSimilarityMultiplier = 0.f;
    TWeakObjectPtr<AConfigurableDoorActor> PendingDoorTarget;
    TWeakObjectPtr<AConfigurableDoorActor> LockedDoorTarget;

    float GetSimilarityMultiplier() const;
    float GetScaledDoorLockDuration() const;
};