#pragma once

#include "CoreMinimal.h"
#include "PickupActorAAARuneCanvascommonInstrument.h"
#include "PickupActorAAARuneGlowSensorCard.generated.h"

class APawn;
class UPointlightComponent;
class USceneComponent;
class UPrimitiveComponent;
struct FHitResult;

UCLASS()
class KONGBU_API APickupActorAAARuneGlowSensorCard : public APickupActorAAARuneCanvascommonInstrument
{
    GENERATED_BODY()

public:
    APickupActorAAARuneGlowSensorCard();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform &Transform) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnPickedUp() override;
    virtual void OnPutDown(FVector Placelocation, FRotator PlaceRotation) override;
    virtual void OnThrown(FVector ThrowDirection, float ThrowForce) override;

protected:
    virtual void OnRuneCanvasAttachedToSurface() override;
    virtual void OnRuneCanvasDetachedFromSurface() override;
    virtual bool TryHandleRuneCanvasThrownImpact(const FHitResult &Hit, UPrimitiveComponent *HitComponent) override;
    virtual const TArray<int32> &GetExpectedNodeSequenceForCurrentCard() const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
    TObjectPtr<USceneComponent> SensorLightOriginComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
    TObjectPtr<UPointLightComponent> SensorLightComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|GlowSensor|Card", meta = (TitleProperty = "PatternId"))
    TArray<FRuneCanvasPattern> GlowSensorCardExpectedSequences;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|GlowSensor|Effect", meta = (ClampMin = "10.0", DisplayName = "Max Effect Radius"))
    float MaxEffectRadius = 800.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|GlowSensor|Effect", meta = (ClampMin = "0.0", DisplayName = "Full Brightness Distance At Max Match"))
    float MaxFullBrightnessDistance = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|GlowSensor|Effect", meta = (ClampMin = "0.0", DisplayName = "Max Wall lifetime"))
    float MaxWallLifetime = 60.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|GlowSensor|Effect", meta = (ClampMin = "0.0"))
    float BrightenInterpSpeed = 7.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|GlowSensor|Effect", meta = (ClampMin = "0.0"))
    float DimInterpSpeed = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|GlowSensor|Visual")
    FLinearColor SensorLightColor = FLinearColor(0.35f, 0.95f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|GlowSensor|Visual", meta = (ClampMin = "0.0"))
    float MaxLightIntensity = 5200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|GlowSensor|Visual", meta = (ClampMin = "50.0"))
    float LightAttenuationRadius = 650.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RuneCanvas|GlowSensor|Debug")
    bool bEnableGlowSensorDebug = false;

private:
    bool bGlowSensorActive = false;
    bool bHasCachedThrowSimilarityMultiplier = false;
    float CachedThrowSimilarityMultiplier = 0.f;
    float CurrentBrightnessAlpha = 0.f;
    FTimerHandle WallLifetimeHandle;

    float GetSimilarityMultiplier() const;
    float GetScaledEffectRadius() const;
    float GetScaledFullBrightnessDistance() const;
    float GetScaledWallLifetime() const;
    bool IsGhostActor(const AActor *Actor) const;
    float ComputeTargetBrightnessAlpha(const APawn *&OutNearestGhost, float &OutNearestDistance) const;
    FVector GetSensorLightOriginLocation() const;
    void SyncSensorLightFromSettings();
    void UpdateSensorLightVisualState();
    void UpdateSensorMaterialVisualState();
    void DrawGlowSensorDebug(const APawn *NearestGhost, float NearestDistance) const;
    void ExpireGlowSensorCard();
};
