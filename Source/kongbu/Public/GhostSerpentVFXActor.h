#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GhostSerpentVFXActor.generated.h"

class UPoseableMeshComponent;
class USceneComponent;
class USkeletalMesh;
class AWomenCharacter;
class AGhostCharacter;
class UCharacterMovementComponent;

UENUM(BlueprintType)
enum class EGhostSerpentBehaviorState : uint8
{
    FlyingToGhost UMETA(DisplayName = "Flying To Ghost"),
    Wandering UMETA(DisplayName = "Wandering"),
    OrbitingPlayer UMETA(DisplayName = "Orbiting Player")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGhostSerpentAbsorbedSignature, AGhostSerpentVFXActor*, SerpentActor, AActor*, TargetActor);

// 程序化蛇形鬼魂特效: 从裂缝飞出, 沿曲线路径游向目标 Actor, 并用骨骼延迟形成蛇形身体。
UCLASS(Blueprintable)
class KONGBU_API AGhostSerpentVFXActor : public AActor
{
    GENERATED_BODY()

public:
    AGhostSerpentVFXActor();

    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost Serpent|Components")
    TObjectPtr<USceneComponent> SceneRootComponent = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost Serpent|Components")
    TObjectPtr<UPoseableMeshComponent> SerpentMeshComponent = nullptr;

public:
    // 给这里填你的 10 节骨骼网格体。运行时会复制到 PoseableMeshComponent 上。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost Serpent|Mesh")
    TObjectPtr<USkeletalMesh> SerpentSkeletalMesh = nullptr;

    // 从头到尾填写骨骼名, 例如 Head, spine_01, spine_02 ... spine_10.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Mesh")
    TArray<FName> BodyBoneNames;

    // 如果骨骼默认朝向不是 X+, 用这个修正整体骨骼朝向。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Mesh")
    FRotator BoneRotationOffset = FRotator::ZeroRotator;

    // 有些头骨和脊椎骨的本地轴不一致，这里只修正第一个骨骼的朝向偏移。 [0,0,90]
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Mesh")
    FRotator HeadBoneRotationOffset = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Target")
    TObjectPtr<AActor> TargetActor = nullptr;

    // 建议填 GhostCharacter 身上的 chest/head/spine Socket。为空时飞向 Actor 位置。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Target")
    FName TargetSocketName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Target")
    FVector TargetLocationOffset = FVector(0.f, 0.f, 80.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Flight", meta = (ClampMin = "0.1"))
    float FlightDuration = 2.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Flight", meta = (ClampMin = "1.0"))
    float ArrivalRadius = 45.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Flight")
    bool bAutoStartOnBeginPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Flight")
    bool bDestroyOnAbsorbed = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Behavior")
    bool bUseAutonomousBehavior = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Behavior", meta = (ClampMin = "0.0"))
    float PlayerSenseRadius = 1200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Behavior", meta = (ClampMin = "0.0"))
    float GhostSenseRadius = 1600.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Behavior", meta = (ClampMin = "0.0"))
    float PlayerForgetRadius = 1700.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Behavior")
    bool bRequireLineOfSight = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float WanderSpeed = 220.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float OrbitSpeed = 360.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float FlyToGhostSpeed = 720.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float WanderRadius = 650.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float WanderTargetAcceptanceRadius = 70.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float OrbitRadius = 260.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float OrbitHeight = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float OrbitAngularSpeedDegrees = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Effects", meta = (ClampMin = "0.0"))
    float PlayerSlowTouchRadius = 130.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Effects", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float PlayerSlowMultiplier = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Effects", meta = (ClampMin = "0.0"))
    float GhostAbsorbRadius = 80.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Effects", meta = (ClampMin = "0.1"))
    float GhostSpeedBoostMultiplier = 1.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Effects", meta = (ClampMin = "0.0"))
    float GhostSpeedBoostDuration = 5.f;

    // 裂缝到目标之间的整体弧线高度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Path", meta = (ClampMin = "0.0"))
    float PathLift = 220.f;

    // 每条鬼魂生成时控制点随机偏移, 让多条不会飞成同一条线。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Path", meta = (ClampMin = "0.0"))
    float PathRandomRadius = 260.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Path", meta = (ClampMin = "0.0"))
    float SpiralAmplitude = 95.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Path", meta = (ClampMin = "0.0"))
    float SpiralFrequency = 2.2f;

    // 身体每节沿历史轨迹间隔多远。越大身体拉得越长。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Body", meta = (ClampMin = "1.0"))
    float BodySegmentSpacing = 36.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Body", meta = (ClampMin = "0.0"))
    float WaveYawAmplitudeDegrees = 18.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Body", meta = (ClampMin = "0.0"))
    float WavePitchAmplitudeDegrees = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Body", meta = (ClampMin = "0.0"))
    float WaveRollAmplitudeDegrees = 28.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Body", meta = (ClampMin = "0.0"))
    float WaveSpeed = 7.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Body", meta = (ClampMin = "0.0"))
    float BonePhaseOffset = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Debug")
    bool bShowDebugPath = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Debug")
    FColor DebugPathColor = FColor::Cyan;

    UPROPERTY(BlueprintAssignable, Category = "Ghost Serpent|Events")
    FGhostSerpentAbsorbedSignature OnSerpentAbsorbed;

    UFUNCTION(BlueprintCallable, Category = "Ghost Serpent")
    void SetSerpentMesh(USkeletalMesh* NewMesh);

    UFUNCTION(BlueprintCallable, Category = "Ghost Serpent")
    void StartFlight(AActor* NewTargetActor, FName NewTargetSocketName = NAME_None);

    UFUNCTION(BlueprintCallable, Category = "Ghost Serpent")
    void StartFlightFromLocation(FVector RiftLocation, AActor* NewTargetActor, FName NewTargetSocketName = NAME_None);

    UFUNCTION(BlueprintCallable, Category = "Ghost Serpent")
    void StartAutonomousFromLocation(FVector RiftLocation);

    UFUNCTION(BlueprintCallable, Category = "Ghost Serpent")
    void StopFlight();

    UFUNCTION(BlueprintPure, Category = "Ghost Serpent")
    bool IsFlying() const { return bIsFlying; }

    UFUNCTION(BlueprintPure, Category = "Ghost Serpent")
    EGhostSerpentBehaviorState GetBehaviorState() const { return BehaviorState; }

protected:
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Ghost Serpent|Events")
    void ReceiveSerpentAbsorbed(AActor* AbsorbedTarget);

private:
    bool bIsFlying = false;
    EGhostSerpentBehaviorState BehaviorState = EGhostSerpentBehaviorState::FlyingToGhost;
    float FlightTime = 0.f;
    FVector FlightStartLocation = FVector::ZeroVector;
    FVector SpawnOriginLocation = FVector::ZeroVector;
    FVector WanderTargetLocation = FVector::ZeroVector;
    FVector PathControlPointA = FVector::ZeroVector;
    FVector PathControlPointB = FVector::ZeroVector;
    TArray<FVector> TrailLocations;
    TArray<FVector> RuntimeBoneWorldLocations;
    float OrbitAngleDegrees = 0.f;
    TObjectPtr<AWomenCharacter> OrbitTargetPlayer = nullptr;
    TObjectPtr<AGhostCharacter> AbsorbTargetGhost = nullptr;
    TObjectPtr<AWomenCharacter> SlowedPlayer = nullptr;
    float CachedSlowedPlayerWalkSpeed = 0.f;
    bool bHasCachedSlowedPlayerWalkSpeed = false;

    void InitializeMesh();
    void BuildDefaultBoneNamesIfNeeded();
    void GeneratePathControlPoints();
    void ResetTrail();
    void UpdateFlight(float DeltaTime);
    void UpdateAutonomousBehavior(float DeltaTime);
    void UpdateWander(float DeltaTime);
    void UpdateOrbitPlayer(float DeltaTime);
    void UpdateFlyToGhostBehavior(float DeltaTime);
    void MoveSerpentToward(const FVector& DesiredLocation, float Speed, float DeltaTime);
    void UpdateTrail(const FVector& HeadLocation);
    void UpdateBodyPose(float DeltaTime);
    void FinishAbsorption();
    void FinishGhostAbsorption(AGhostCharacter* GhostTarget);
    AWomenCharacter* FindVisiblePlayer() const;
    AGhostCharacter* FindVisibleGhost() const;
    bool CanSeeActor(const AActor* Candidate, float SenseRadius) const;
    void ChooseNewWanderTarget();
    void ApplyPlayerSlow(AWomenCharacter* Player);
    void ClearPlayerSlow();
    FVector GetSerpentTargetLocation() const;
    FVector EvaluateHeadLocation(float Alpha) const;
    FVector GetTrailLocationAtDistance(float DistanceBack) const;
    FVector GetTrailDirectionAtDistance(float DistanceBack) const;
};