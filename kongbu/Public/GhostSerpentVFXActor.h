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

    // Actor 的根节点, 一般不需要手动调整, 仅用于挂载蛇身组件。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost Serpent|Components")
    TObjectPtr<USceneComponent> SceneRootComponent = nullptr;

    // 实际驱动骨骼姿态的组件。代码会逐节设置骨骼位置和旋转。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost Serpent|Components")
    TObjectPtr<UPoseableMeshComponent> SerpentMeshComponent = nullptr;

public:
    // 给这里指定蛇身的 Skeletal Mesh。运行时会复制到 PoseableMeshComponent 上。
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost Serpent|Mesh")
    TObjectPtr<USkeletalMesh> SerpentSkeletalMesh = nullptr;

    // 从头到尾填写要驱动的骨骼名。顺序必须是蛇头到蛇尾，例如 Head, spine_01, spine_02 ...
    // 如果这里留空，代码会尝试按连续命名自动生成骨骼名。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Mesh")
    TArray<FName> BodyBoneNames;

    // 如果模型骨骼前向不是 X+，在这里统一补一个整体旋转偏移。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Mesh")
    FRotator BoneRotationOffset = FRotator::ZeroRotator;

    // 只额外修正第一个骨骼的朝向。常用于头骨局部轴和后续脊椎骨不一致的模型。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Mesh")
    FRotator HeadBoneRotationOffset = FRotator(0.f, 0.f, 180.f);

    // 只修正头部第一节骨骼的额外旋转。用于头模型侧着时单独校正，不影响身体和尾部。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Mesh")
    FRotator HeadOnlyRotationOffset = FRotator::ZeroRotator;

    // 非自主模式下的飞行目标。为空时 StartFlight 不会真正开始追踪。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Target")
    TObjectPtr<AActor> TargetActor = nullptr;

    // 目标身上的 Socket 名。建议填 chest/head/spine；为空时飞向目标 Actor 的位置加偏移。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Target")
    FName TargetSocketName = NAME_None;

    // 目标点的世界偏移。常用来把命中点抬高到胸口或头部附近。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Target")
    FVector TargetLocationOffset = FVector(0.f, 0.f, 80.f);

    // 非自主模式下，从起点飞到目标所需的总时长。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Flight", meta = (ClampMin = "0.1"))
    float FlightDuration = 2.4f;

    // 到目标多近算吸收完成。值越大，越早结束飞行。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Flight", meta = (ClampMin = "1.0"))
    float ArrivalRadius = 45.f;

    // BeginPlay 时是否自动启动。开启后会根据行为模式自动开始飞行或游荡。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Flight")
    bool bAutoStartOnBeginPlay = true;

    // 吸收完成后是否直接销毁 Actor。关掉后可以复用或手动控制后续表现。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Flight")
    bool bDestroyOnAbsorbed = true;

    // 是否启用自主行为。开启后会自动游荡、找鬼、绕玩家；关闭后只按 StartFlight 的目标飞行。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Behavior")
    bool bUseAutonomousBehavior = true;

    // 感知玩家的半径。自主模式下进入这个范围后才可能绕玩家飞行。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Behavior", meta = (ClampMin = "0.0"))
    float PlayerSenseRadius = 1200.f;

    // 感知 GhostCharacter 的半径。自主模式下会优先追最近且可见的鬼。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Behavior", meta = (ClampMin = "0.0"))
    float GhostSenseRadius = 1600.f;

    // 绕玩家时的遗忘半径。玩家离太远后会放弃并回到游荡。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Behavior", meta = (ClampMin = "0.0"))
    float PlayerForgetRadius = 1700.f;

    // 是否要求视线可达。开启后会做可见性射线检测，隔墙时不会锁定目标。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Behavior")
    bool bRequireLineOfSight = true;

    // 打开时按当前逻辑绕着玩家旋转；关闭时反复穿过玩家模型，做成试图附身的表现。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Behavior")
    bool bOrbitAroundPlayer = true;

    // 游荡状态的移动速度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float WanderSpeed = 220.f;

    // 绕玩家状态的移动速度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float orbitspeed = 360.f;

    // 追鬼吸收状态的移动速度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float FlyToGhostSpeed = 720.f;

    // 以出生点为中心的游荡范围半径。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float WanderRadius = 650.f;

    // 游荡目标的到达判定半径。进入范围后会重新选一个游荡点。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float WanderTargetAcceptanceRadius = 70.f;

    // 绕玩家时与玩家保持的水平半径。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float OrbitRadius = 260.f;

    // 绕玩家时相对玩家的高度偏移。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float OrbitHeightMin = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float OrbitHeightMax = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.01"))
    float OrbitHeightRetargetMinInterval = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.01"))
    float OrbitHeightRetargetMaxInterval = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float OrbitHeightInterpSpeed = 3.5f;

    // 绕玩家时每秒转多少度。值越大，环绕越快。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float OrbitAngularSpeedDegrees = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float PossessionPassThroughDistance = 360.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Movement", meta = (ClampMin = "0.0"))
    float PossessionPassAcceptanceRadius = 70.f;

    // 玩家减速倍率。0.55 表示最大移动速度降到原来的 55%。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Effects", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float PlayerSlowMultiplier = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Effects", meta = (ClampMin = "0.0"))
    float PlayerSlowDuration = 4.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Effects", meta = (ClampMin = "0.0"))
    float PlayerMeshPassThroughPadding = 12.f;

    // 与鬼靠近到这个半径内时触发吸收和加速增益。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Effects", meta = (ClampMin = "0.0"))
    float GhostAbsorbRadius = 80.f;

    // 吸收鬼后给 GhostCharacter 的临时移速倍率。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Effects", meta = (ClampMin = "0.1"))
    float GhostSpeedBoostMultiplier = 1.25f;

    // 吸收鬼后移速增益持续的秒数。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Effects", meta = (ClampMin = "0.0"))
    float GhostSpeedBoostDuration = 5.f;

    // 非自主模式下，裂缝到目标的贝塞尔路径整体抬升高度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Path", meta = (ClampMin = "0.0"))
    float PathLift = 220.f;

    // 贝塞尔控制点的随机偏移半径。用来打散多条鬼魂的飞行路径。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Path", meta = (ClampMin = "0.0"))
    float PathRandomRadius = 260.f;

    // 飞行路径上的螺旋摆动半径。值越大，蛇头绕路径摆动越明显。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Path", meta = (ClampMin = "0.0"))
    float SpiralAmplitude = 95.f;

    // 飞行路径上的螺旋频率。值越大，路径上的盘旋圈数越多。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Path", meta = (ClampMin = "0.0"))
    float SpiralFrequency = 2.2f;

    // 身体每节沿历史轨迹间隔多远。越大身体拉得越长，骨骼之间也会更稀。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Body", meta = (ClampMin = "1.0"))
    float BodySegmentSpacing = 36.f;

    // 预留的身体横摆幅度参数。当前版本未实际参与骨骼姿态计算。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Body", meta = (ClampMin = "0.0"))
    float WaveYawAmplitudeDegrees = 18.f;

    // 预留的身体俯仰摆动参数。当前版本未实际参与骨骼姿态计算。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Body", meta = (ClampMin = "0.0"))
    float WavePitchAmplitudeDegrees = 8.f;

    // 预留的身体翻滚摆动参数。当前版本未实际参与骨骼姿态计算。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Body", meta = (ClampMin = "0.0"))
    float WaveRollAmplitudeDegrees = 28.f;

    // 波动动画速度。当前主要影响位移中的轻微摆动，不直接驱动每节骨骼波形。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Body", meta = (ClampMin = "0.0"))
    float WaveSpeed = 7.f;

    // 预留的骨骼相位偏移参数。当前版本未实际参与骨骼姿态计算。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Body", meta = (ClampMin = "0.0"))
    float BonePhaseOffset = 0.55f;

    // 是否在世界里绘制调试飞行路径。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Debug")
    bool bShowDebugPath = false;

    // 调试路径的绘制颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost Serpent|Debug")
    FColor DebugPathColor = FColor::Cyan;

    // 吸收完成时广播的事件，可在蓝图里接粒子、音效或销毁逻辑。
    UPROPERTY(BlueprintAssignable, Category = "Ghost Serpent|Events")
    FGhostSerpentAbsorbedSignature OnSerpentAbsorbed;

    // 运行时切换蛇身网格。设置后会重新初始化 PoseableMeshComponent。
    UFUNCTION(BlueprintCallable, Category = "Ghost Serpent")
    void SetSerpentMesh(USkeletalMesh* NewMesh);

    // 从当前 Actor 位置开始朝目标飞行。适合手动触发一次性吸收表现。
    UFUNCTION(BlueprintCallable, Category = "Ghost Serpent")
    void StartFlight(AActor* NewTargetActor, FName NewTargetSocketName = NAME_None);

    // 从指定世界位置开始朝目标飞行。常用于裂缝、传送门或生成点发射蛇形鬼魂。
    UFUNCTION(BlueprintCallable, Category = "Ghost Serpent")
    void StartFlightFromLocation(FVector RiftLocation, AActor* NewTargetActor, FName NewTargetSocketName = NAME_None);

    // 从指定世界位置启动自主行为。启动后会先游荡，再根据感知结果切换状态。
    UFUNCTION(BlueprintCallable, Category = "Ghost Serpent")
    void StartAutonomousFromLocation(FVector RiftLocation);

    // 立即停止飞行并清空身体轨迹缓存。
    UFUNCTION(BlueprintCallable, Category = "Ghost Serpent")
    void StopFlight();

    // 返回当前是否处于活动飞行/游荡状态。
    UFUNCTION(BlueprintPure, Category = "Ghost Serpent")
    bool IsFlying() const { return bIsFlying; }

    // 返回当前自主行为状态，方便蓝图做 UI、音效或调试显示。
    UFUNCTION(BlueprintPure, Category = "Ghost Serpent")
    EGhostSerpentBehaviorState GetBehaviorState() const { return BehaviorState; }

protected:
    virtual void BeginPlay() override;

    // 蓝图事件: 当目标被吸收时触发，可在蓝图里补表现层逻辑。
    UFUNCTION(BlueprintImplementableEvent, Category = "Ghost Serpent|Events")
    void ReceiveSerpentAbsorbed(AActor* AbsorbedTarget);

private:
    bool bIsFlying = false;
    EGhostSerpentBehaviorState BehaviorState = EGhostSerpentBehaviorState::FlyingToGhost;
    float FlightTime = 0.f;
    FVector FlightStartLocation = FVector::ZeroVector;
    FVector SpawnOriginLocation = FVector::ZeroVector;
    FVector WanderTargetLocation = FVector::ZeroVector;
    FVector PossessionPassTargetLocation = FVector::ZeroVector;
    FVector LastHeadMoveDirection = FVector::ForwardVector;
    FVector PathControlPointA = FVector::ZeroVector;
    FVector PathControlPointB = FVector::ZeroVector;
    TArray<FName> RuntimeBodyBoneNames;
    TArray<FVector> TrailLocations;
    TArray<FVector> RuntimeBoneWorldLocations;
    TArray<FVector> ReferenceBoneComponentLocations;
    TArray<FQuat> ReferenceBoneComponentRotations;
    TArray<FVector> ReferenceBoneComponentForwardDirections;
    float OrbitAngleDegrees = 0.f;
    float CurrentOrbitHeight = 120.f;
    float TargetOrbitHeight = 120.f;
    float OrbitHeightRetargetTimeRemaining = 0.f;
    float PlayerSlowTimeRemaining = 0.f;
    bool bHasPossessionPassTarget = false;
    TObjectPtr<AWomenCharacter> OrbitTargetPlayer = nullptr;
    TObjectPtr<AGhostCharacter> AbsorbTargetGhost = nullptr;
    TObjectPtr<AWomenCharacter> SlowedPlayer = nullptr;

    void InitializeMesh();
    void BuildDefaultBoneNamesIfNeeded();
    void CacheReferenceBoneComponentTransforms();
    FQuat MakeBoneComponentRotationFromDirection(int32 BoneIndex, const FVector& ComponentDirection) const;
    void GeneratePathControlPoints();
    void ResetTrail();
    void UpdateFlight(float DeltaTime);
    void UpdateAutonomousBehavior(float DeltaTime);
    void UpdateWander(float DeltaTime);
    void UpdateOrbitPlayer(float DeltaTime);
    void UpdatePossessionPassThroughPlayer(float DeltaTime);
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
    void ChooseNewPossessionPassTarget(const AWomenCharacter* Player);
    float PickRandomOrbitHeight() const;
    bool DidPassThroughPlayerMesh(const AWomenCharacter* Player, const FVector& PreviousLocation, const FVector& NewLocation) const;
    void ApplyPlayerSlow(AWomenCharacter* Player);
    void UpdatePlayerSlowTimer(float DeltaTime);
    void ClearPlayerSlow();
    FVector GetSerpentTargetLocation() const;
    FVector EvaluateHeadLocation(float Alpha) const;
    FVector GetTrailLocationAtDistance(float DistanceBack) const;
    FVector GetTrailDirectionAtDistance(float DistanceBack) const;
};