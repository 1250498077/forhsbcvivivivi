#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VaultableComponent.generated.h"

class ACharacter;
class UAnimMontage;
class UBoxComponent;

UENUM(BlueprintType)
enum class EVaultSide : uint8
{
    None,
    SideA,
    SideB
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class KONGBU_API UVaultableComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UVaultableComponent();

    virtual void OnRegister() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vault|Components")
    TObjectPtr<UBoxComponent> SideAZoneComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vault|Components")
    TObjectPtr<UBoxComponent> SideBZoneComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vault|Components")
    TObjectPtr<UBoxComponent> ViewTargetBoxComponent = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault|Movement", meta = (ClampMin = "0.05"))
    float VaultDuration = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault|Movement", meta = (ClampMin = "0.0"))
    float VaultArcHeight = 35.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault|Movement")
    bool bProjectLandingToGround = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault|Movement", meta = (ClampMin = "0.0"))
    float GroundTraceUpDistance = 90.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault|Movement", meta = (ClampMin = "0.0"))
    float GroundTraceDownDistance = 220.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault|Detection", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float FacingDotThreshold = 0.45f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault|Detection")
    bool bRequireViewTraceHit = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault|Detection", meta = (ClampMin = "0.0"))
    float ViewTraceDistance = 260.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault|Detection")
    bool bCheckLandingCapsule = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault|Animation")
    TObjectPtr<UAnimMontage> VaultMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vault|Debug")
    bool bShowDebugZones = false;

    bool CanVaultFromView(const ACharacter* Character, const FVector& ViewLocation, const FVector& ViewDirection, EVaultSide& OutStartSide, FVector& OutLandingLocation) const;
    FVector GetLandingLocationForSide(EVaultSide StartSide, const ACharacter* Character) const;

private:
    void ConfigureBoxComponent(UBoxComponent* BoxComponent, const FColor& ShapeColor, bool bBlockVisibility) const;
    void UpdateDebugVisibility() const;
    bool IsActorInsideBox(const AActor* Actor, const UBoxComponent* BoxComponent) const;
    bool IsViewFacingTarget(const FVector& ViewLocation, const FVector& ViewDirection) const;
    bool DoesViewTraceHitVaultTarget(const AActor* Viewer, const FVector& ViewLocation, const FVector& ViewDirection) const;
    bool IsLandingLocationClear(const ACharacter* Character, const FVector& LandingLocation) const;
    const UBoxComponent* GetOppositeSideZone(EVaultSide StartSide) const;
};