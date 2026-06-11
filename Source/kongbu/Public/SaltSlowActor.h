#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaltSlowActor.generated.h"

class AMyAIController;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class KONGBU_API ASaltSlowActor : public AActor
{
    GENERATED_BODY()

public:
    ASaltSlowActor();

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* SlowTriggerComponent;

    // 盐堆触发减速的有效范围，单位厘米。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salt|Slow", meta = (ClampMin = "10.0"))
    float SlowTriggerRadius = 90.f;

    // AI 进入盐范围后的速度乘区。
    // 0.5 表示最终移动速度减半；1.0 表示不减速。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salt|Slow", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float SlowMultiplier = 0.2f;

private:
    TSet<TWeakObjectPtr<AMyAIController>> AffectedControllers;

    UFUNCTION()
    void HandleSlowTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void HandleSlowTriggerEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex);

    void UpdateSlowTriggerRadius();
    void ApplySlowToCurrentOverlaps();
    AMyAIController* ResolveAIControllerFromActor(AActor* OtherActor) const;
};