#pragma once

#include "CoreMinimal.h"
#include "PickupActor.h"
#include "PickupActorAAAGhostLure.generated.h"

UCLASS()
class KONGBU_API APickupActorAAAGhostLure : public APickupActor
{
    GENERATED_BODY()

public:
    APickupActorAAAGhostLure();

    virtual void Tick(float DeltaTime) override;
    virtual void OnPickedUp() override;
    virtual void OnPutDown(FVector PlaceLocation, FRotator PlaceRotation) override;
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce) override;
    virtual bool CanBeClosedByPlayer_Implementation() const override;
    virtual void CloseByPlayer_Implementation(AActor* ClosingActor) override;
    virtual bool IsClosedByPlayer_Implementation() const override;
    virtual void OpenByPlayer_Implementation(AActor* OpeningActor) override;

    UFUNCTION(BlueprintCallable, Category = "Ghost Lure")
    void ActivateLure();

    UFUNCTION(BlueprintCallable, Category = "Ghost Lure")
    void DeactivateLure();

    UFUNCTION(BlueprintCallable, Category = "Ghost Lure")
    void EmitLurePulse();

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Lure", meta = (ClampMin = "100.0"))
    float LureRadius = 1600.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Lure", meta = (ClampMin = "30.0"))
    float LureAcceptanceRadius = 120.f;

    // 鬼靠近诱饵后原地发呆多久。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Lure", meta = (ClampMin = "0.1"))
    float DazeDuration = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Lure", meta = (ClampMin = "0.05"))
    float LurePulseInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Lure")
    bool bAutoActivateWhenPlaced = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Lure")
    bool bAutoActivateWhenThrown = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Lure", meta = (ClampMin = "0.0"))
    float ThrowActivationDelay = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Lure|Trace")
    bool bRequireLineOfSightToAffectTargets = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Lure|Trace")
    TEnumAsByte<ECollisionChannel> BlockTraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Lure|Debug")
    bool bEnableDebug = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Lure|Debug")
    FColor DebugColor = FColor(180, 80, 255);

    UPROPERTY(BlueprintReadOnly, Category = "Ghost Lure")
    bool bLureActive = false;

    UPROPERTY(BlueprintReadOnly, Category = "Ghost Lure")
    bool bClosedByPlayer = false;

private:
    float PulseAccumulator = 0.f;
    float PulseVisualTime = 0.f;
    FTimerHandle DelayedActivationHandle;

    bool IsActorInsideLureRadius(const AActor* TargetActor) const;
    bool HasLineOfSightToTarget(const AActor* TargetActor) const;
    void DrawLureDebug() const;
};