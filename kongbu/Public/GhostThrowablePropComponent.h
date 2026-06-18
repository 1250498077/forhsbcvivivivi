#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GhostThrowablePropComponent.generated.h"

class UPrimitiveComponent;

UCLASS(ClassGroup=(Ghost), meta=(BlueprintSpawnableComponent))
class KONGBU_API UGhostThrowablePropComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGhostThrowablePropComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Throwable")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Throwable")
    bool bRequireSimulatingPhysics = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Throwable")
    FName ThrowablePrimitiveComponentName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Throwable", meta = (ClampMin = "0.01"))
    float ThrowForceMultiplier = 1.f;

    UFUNCTION(BlueprintPure, Category = "Ghost Throwable")
    bool CanBeUsedByGhost() const;

    UFUNCTION(BlueprintPure, Category = "Ghost Throwable")
    UPrimitiveComponent* ResolveThrowablePrimitive() const;

    UFUNCTION(BlueprintPure, Category = "Ghost Throwable")
    bool IsGhostTelekinesisActive() const
    {
        return bGhostTelekinesisActive;
    }

    UFUNCTION(BlueprintPure, Category = "Ghost Throwable")
    FVector GetThrowableLocation() const;

    UFUNCTION(BlueprintCallable, Category = "Ghost Throwable")
    bool BeginGhostTelekinesis(AActor* ControllingGhost);

    UFUNCTION(BlueprintCallable, Category = "Ghost Throwable")
    void UpdateGhostTelekinesisTransform(FVector NewLocation, FRotator NewRotation);

    UFUNCTION(BlueprintCallable, Category = "Ghost Throwable")
    void LaunchFromGhostTelekinesis(FVector ThrowDirection, float ThrowForce, APawn* ThrowInstigator = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Ghost Throwable")
    void CancelGhostTelekinesis();

private:
    bool IsPrimitiveUsable(UPrimitiveComponent* PrimitiveComponent) const;
    void RestorePrimitiveAfterTelekinesis(bool bWakeRigidBody);

    UPROPERTY(Transient)
    TObjectPtr<UPrimitiveComponent> ActivePrimitiveComponent = nullptr;

    bool bGhostTelekinesisActive = false;
    bool bCachedSimulatePhysics = false;
    bool bCachedEnableGravity = true;
    bool bCachedGenerateOverlapEvents = true;
    ECollisionEnabled::Type CachedCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
};