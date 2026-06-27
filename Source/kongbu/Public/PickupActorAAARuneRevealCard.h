#pragma once

#include "CoreMinimal.h"
#include "PickupActorAAARuneCanvascommonInstrument.h"
#include "PickupActorAAARuneRevealCard.generated.h"

class AMyAIController;
class APawn;
class UPrimitiveComponent;
class USceneComponent;
struct FHitResult;

UCLASS()
class KONGBU_API APickupActorAAARuneRevealCard : public APickupActorAAARuneCanvascommonInstrument
{
    GENERATED_BODY()

public:
    APickupActorAAARuneRevealCard();

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnPickedUp() override;
    virtual void OnPutDown(FVector PlaceLocation, FRotator PlaceRotation) override;
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce) override;

    bool TryAttachToGhostZone(APawn *TargetPawn, AMyAIController *TargetController, USceneComponent *AttachComponent);

protected:
    virtual bool TryHandleRuneCanvasThrownImpact(const FHitResult &Hit, UPrimitiveComponent *HitComponent) override;
    virtual const TArray<int32> &GetExpectedNodeSequenceForCurrentCard() const override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Reveal|Card", meta = (TitleProperty = "PatternId"))
    TArray<FRuneCanvasPattern> RevealCardExpectedSequences;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Reveal", meta = (ClampMin = "0.0", DisplayName = "Max Reveal Duration"))
    float RevealDuration = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Reveal|Attach", meta = (ClampMin = "0.1", ClampMax = "1.0"))
    float GhostAttachHorizontalRadiusRatio = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Reveal|Attach", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GhostAttachVerticalMinRatio = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Reveal|Attach", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GhostAttachVerticalMaxRatio = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|Reveal|Attach", meta = (ClampMin = "0.0"))
    float GhostAttachSurfacePadding = 1.f;

private:
    bool bAttachedToGhost = false;
    bool bCanAttachToGhostAfterThrow = false;
    bool bHasCachedThrowSimilarityMultiplier = false;
    float CachedThrowSimilarityMultiplier = 0.f;

    TWeakObjectPtr<AMyAIController> AttachedController;
    TWeakObjectPtr<AActor> AttachedTargetActor;
    FTimerHandle RevealDurationHandle;

    bool TryResolveGhost(AActor *OtherActor, APawn *&OutPawn, AMyAIController *&OutController) const;
    float ResolveScaledRevealDuration() const;
    void AttachToGhost(APawn *TargetPawn, AMyAIController *TargetController, USceneComponent *AttachComponent);
    void ApplyGhostAttachPose(APawn *TargetPawn, USceneComponent *AttachComponent);
    void ExpireRevealCard();
    void ClearAttachedRevealEffect();
};