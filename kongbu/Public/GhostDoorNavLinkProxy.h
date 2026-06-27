#pragma once

#include "CoreMinimal.h"
#include "Navigation/NavLinkProxy.h"
#include "GhostDoorNavLinkProxy.generated.h"

class AConfigurableDoorActor;
class AMyAIController;
class UArrowComponent;
class UBoxComponent;
class UPrimitiveComponent;

UCLASS(Blueprintable)
class KONGBU_API AGhostDoorNavLinkProxy : public ANavLinkProxy
{
    GENERATED_BODY()

public:
    AGhostDoorNavLinkProxy();

protected:
    virtual void OnConstruction(const FTransform &Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost Door Link|Components")
    TObjectPtr<UArrowComponent> LinkStartComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost Door Link|Components")
    TObjectPtr<UArrowComponent> LinkEndComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost Door Link|Components")
    TObjectPtr<UBoxComponent> ProximityOpenBoxComponent = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Door Link")
    TObjectPtr<AConfigurableDoorActor> LinkedDoor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Door Link", meta = (ClampMin = "0.0"))
    float ResumeDelay = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Door Link|Fallback")
    bool bEnableProximityOpenFallback = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Door Link|Fallback", meta = (ClampMin = "0.0"))
    float ProximityOpenPadding = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Door Link|Fallback", meta = (ClampMin = "0.0"))
    float ProximityOpenHalfHeight = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Door Link|AI State")
    bool bAllowChase = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Door Link|AI State")
    bool bAllowInvestigate = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Door Link|AI State")
    bool bAllowRage = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Door Link|Animation")
    bool bPlayGhostOpenDoorAnimation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Door Link|Debug")
    bool bLogDoorLinkDebug = true;

    UFUNCTION()
    void HandleSmartLinkReached(AActor *MovingActor, const FVector &DestinationPoint);

    UFUNCTION()
    void ResumeAgentPathFollowing(AActor *MovingActor);

    UFUNCTION()
    void HandleProximityOpenBoxBeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor,
                                            UPrimitiveComponent *OtherComp, int32 OtherBodyIndex,
                                            bool bFromSweep, const FHitResult &SweepResult);

private:
    void SyncSmartLinkFromComponents();
    void ScanProximityOpenFallback();
    bool IsActorInsideProximityOpenBox(const AActor *Actor) const;
    bool TryOpenLinkedDoorForActor(AActor *MovingActor, const TCHAR *Reason);
    bool CanControllerOpenDoor(const AMyAIController *AIController) const;
};