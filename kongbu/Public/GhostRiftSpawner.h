#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GhostRiftSpawner.generated.h"

class AGhostSerpentVFXActor;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

USTRUCT()
struct FRuntimeRiftCrack
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> MeshComponent = nullptr;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial = nullptr;

    FVector TargetScale = FVector::OneVector;
    float Age = 0.f;
    float GrowthDuration = 1.f;
    bool bFinishedGrowing = false;
};

UCLASS(Blueprintable)
class KONGBU_API AGhostRiftSpawner : public AActor
{
    GENERATED_BODY()

public:
    AGhostRiftSpawner();

    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost Rift|Components")
    TObjectPtr<USceneComponent> SceneRootComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost Rift|Components")
    TObjectPtr<UStaticMeshComponent> MainRiftMeshComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost Rift|Components")
    TObjectPtr<USceneComponent> ChildRiftRootComponent = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Visual")
    TObjectPtr<UStaticMesh> RiftStaticMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Visual")
    TObjectPtr<UMaterialInterface> RiftMaterial = nullptr;

    // 如果你的材质有这个 Scalar 参数，会随裂缝成长从 0 推到 1；没有也没关系，组件缩放仍会生效。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Visual")
    FName GrowthMaterialScalarName = TEXT("Growth");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Growth")
    bool bGrowRiftOnBeginPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Growth")
    bool bRandomizeMainRiftRotation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Growth")
    FRotator MainRiftRandomRotationRange = FRotator(25.f, 180.f, 180.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Growth", meta = (ClampMin = "0.01"))
    float MainRiftGrowthDuration = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Growth", meta = (ClampMin = "0.001"))
    float MainRiftStartScale = 0.02f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Growth", meta = (ClampMin = "0.001"))
    float MainRiftFinalScale = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Branching")
    bool bEnableChildRiftBranching = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Branching", meta = (ClampMin = "1"))
    int32 ChildRiftsPerBurst = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Branching", meta = (ClampMin = "0"))
    int32 MaxChildRifts = 24;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Branching", meta = (ClampMin = "0.05"))
    float ChildRiftBurstInterval = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Branching", meta = (ClampMin = "0.01"))
    float ChildRiftGrowthDuration = 1.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Branching", meta = (ClampMin = "0.0"))
    float ChildRiftMinDistanceFromParent = 80.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Branching", meta = (ClampMin = "0.0"))
    float ChildRiftMaxDistanceFromParent = 180.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Branching", meta = (ClampMin = "0.0"))
    float MaxChildRiftSpreadDistance = 650.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Branching", meta = (ClampMin = "0.001"))
    float ChildRiftMinScale = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Branching", meta = (ClampMin = "0.001"))
    float ChildRiftMaxScale = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Branching")
    FRotator ChildRiftRandomRotationRange = FRotator(35.f, 180.f, 180.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Placement")
    bool bAvoidSceneOverlapForChildRifts = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Placement", meta = (ClampMin = "1.0"))
    float ChildRiftOverlapCheckRadius = 45.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Placement", meta = (ClampMin = "1.0"))
    float MinDistanceBetweenRifts = 55.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Placement", meta = (ClampMin = "1"))
    int32 ChildRiftPlacementAttempts = 18;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Spawn")
    TSubclassOf<AGhostSerpentVFXActor> SerpentClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Target")
    TObjectPtr<AActor> TargetActor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Target")
    FName TargetSocketName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Spawn")
    bool bAutoSpawn = true;

    // 开启后，蛇形鬼魂等主裂缝长完再开始飞出。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Spawn")
    bool bSpawnSerpentsOnlyAfterMainGrowth = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Spawn", meta = (ClampMin = "0.05"))
    float SpawnInterval = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Spawn", meta = (ClampMin = "0.0"))
    float SpawnIntervalRandomness = 0.18f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Spawn", meta = (ClampMin = "0"))
    int32 MaxActiveSerpents = 12;

    // 0 表示无限生成；大于 0 时生成到数量后停止。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Spawn", meta = (ClampMin = "0"))
    int32 TotalSpawnCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Spawn", meta = (ClampMin = "0.0"))
    float SpawnRadius = 80.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Debug")
    bool bShowDebugSpawnRadius = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Rift|Debug")
    bool bShowDebugChildPlacement = false;

    UFUNCTION(BlueprintCallable, Category = "Ghost Rift")
    AGhostSerpentVFXActor* SpawnSerpent();

    UFUNCTION(BlueprintCallable, Category = "Ghost Rift")
    void StartSpawning();

    UFUNCTION(BlueprintCallable, Category = "Ghost Rift")
    void StopSpawning();

    UFUNCTION(BlueprintCallable, Category = "Ghost Rift")
    void RestartRiftGrowth();

    UFUNCTION(BlueprintPure, Category = "Ghost Rift")
    int32 GetActiveSerpentCount() const { return ActiveSerpents.Num(); }

protected:
    virtual void BeginPlay() override;

private:
    float SpawnTimeRemaining = 0.f;
    float ChildRiftBurstTimeRemaining = 0.f;
    int32 SpawnedCount = 0;
    int32 SpawnedChildRiftCount = 0;
    bool bMainRiftFinishedGrowing = false;
    TArray<TWeakObjectPtr<AGhostSerpentVFXActor>> ActiveSerpents;
    TArray<FRuntimeRiftCrack> RiftCracks;

    void CleanupActiveSerpents();
    void InitializeMainRiftVisual();
    void UpdateRiftGrowth(float DeltaTime);
    void TrySpawnChildRiftBurst();
    bool TryFindChildRiftTransform(FTransform& OutTransform) const;
    bool IsChildRiftLocationClear(const FVector& Location) const;
    FVector GetRandomBranchParentLocation() const;
    FRuntimeRiftCrack* AddRuntimeRiftCrack(UStaticMeshComponent* MeshComponent, const FVector& TargetScale, float GrowthDuration);
    UMaterialInstanceDynamic* ApplyRiftMaterial(UStaticMeshComponent* MeshComponent) const;
    FRotator GetRandomRotationInRange(const FRotator& Range) const;
    float GetNextSpawnDelay() const;
    FVector GetSpawnLocation() const;

    UFUNCTION()
    void HandleSerpentAbsorbed(AGhostSerpentVFXActor* SerpentActor, AActor* AbsorbedTarget);
};