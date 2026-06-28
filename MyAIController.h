#include "GhostSerpentVFXActor.h"

#include "Components/PoseableMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Logging/LogMacros.h"
#include "GhostCharacter.h"
#include "WomenCharacter.h"

namespace
{
    constexpr int32 DefaultGeneratedBoneCount = 100;
    constexpr int32 GMaxTrailAllocationCount = 4096;
    constexpr int32 MaxAutoDetectedSpineBoneCount = 512;
    constexpr float TrailSampleMinDistance = 2.f;

    bool IsSequentialSpineBoneNameValid(const UPoseableMeshComponent *MeshComponent, int32 BoneIndex)
    {
        if (!MeshComponent || BoneIndex <= 0)
        {
            return false;
        }

        const FName TwoDigitName(*FString::Printf(TEXT("spine_%02d"), BoneIndex));
        if (MeshComponent->GetBoneIndex(TwoDigitName) != INDEX_NONE)
        {
            return true;
        }

        const FName PlainName(*FString::Printf(TEXT("spine_%d"), BoneIndex));
        return MeshComponent->GetBoneIndex(PlainName) != INDEX_NONE;
    }

    FName MakeSequentialSpineBoneName(const UPoseableMeshComponent *MeshComponent, int32 BoneIndex)
    {
        const FName TwoDigitName(*FString::Printf(TEXT("spine_%02d"), BoneIndex));
        if (MeshComponent && MeshComponent->GetBoneIndex(TwoDigitName) != INDEX_NONE)
        {
            return TwoDigitName;
        }

        return FName(*FString::Printf(TEXT("spine_%d"), BoneIndex));
    }

    FVector CubicBezier(const FVector &P0, const FVector &P1, const FVector &P2, const FVector &P3, float Alpha)
    {
        const float T = FMath::Clamp(Alpha, 0.f, 1.f);
        const float OneMinusT = 1.f - T;
        return OneMinusT * OneMinusT * OneMinusT * P0 + 3.f * OneMinusT * OneMinusT * T * P1 + 3.f * OneMinusT * T * T * P2 + T * T * T * P3;
    }

    FVector CatmullRomInterpolate(const FVector &P0, const FVector &P1, const FVector &P2, const FVector &P3, float Alpha)
    {
        const float T = FMath::Clamp(Alpha, 0.f, 1.f);
        const float T2 = T * T;
        const float T3 = T2 * T;
        return 0.5f * ((2.f * P1) + (-P0 + P2) * T + (2.f * P0 - 5.f * P1 + 4.f * P2 - P3) * T2 + (-P0 + 3.f * P1 - 3.f * P2 + P3) * T3);
    }

}

AGhostSerpentVFXActor::AGhostSerpentVFXActor()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRootComponent"));
    RootComponent = SceneRootComponent;

    SerpentMeshComponent = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("SerpentMeshComponent"));
    SerpentMeshComponent->SetupAttachment(SceneRootComponent);
    SerpentMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SerpentMeshComponent->SetGenerateOverlapEvents(false);
    SerpentMeshComponent->SetCanEverAffectNavigation(false);
}

void AGhostSerpentVFXActor::BeginPlay()
{
    Super::BeginPlay();

    InitializeMesh();
    BuildDefaultBoneNamesIfNeeded();
    // UE_LOG(LogTemp, Log, TEXT("GhostSerpentVFXActor BeginPlay: Actor=%s, Mesh=%s, RuntimeBodyBoneNames=%d, AutoStart=%s, Autonomous=%s"),
    // *GetName(),
    // SerpentSkeletalMesh ? *SerpentSkeletalMesh->GetName() : TEXT("None"),
    // RuntimeBodyBoneNames.Num(),
    // bAutoStartOnBeginPlay ? TEXT("true") : TEXT("false"),
    // bUseAutonomousBehavior ? TEXT("true") : TEXT("false"));

    if (bAutoStartOnBeginPlay)
    {
        if (bUseAutonomousBehavior)
        {
            StartAutonomousFromLocation(GetActorLocation());
        }
        else if (IsValid(TargetActor))
        {
            StartFlight(TargetActor, TargetSocketName);
        }
    }
}

void AGhostSerpentVFXActor::OnConstruction(const FTransform &Transform)
{
    Super::OnConstruction(Transform);
    InitializeMesh();
    BuildDefaultBoneNamesIfNeeded();
}

void AGhostSerpentVFXActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsFlying)
    {
        return;
    }

    if (bUseAutonomousBehavior)
    {
        UpdateAutonomousBehavior(DeltaTime);
    }
    else
    {
        UpdateFlight(DeltaTime);
    }

    UpdateBodyPose(DeltaTime);
}

void AGhostSerpentVFXActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearPlayerSlow();
    Super::EndPlay(EndPlayReason);
}

void AGhostSerpentVFXActor::SetSerpentMesh(USkeletalMesh *NewMesh)
{
    SerpentSkeletalMesh = NewMesh;
    InitializeMesh();
    BuildDefaultBoneNamesIfNeeded();
}

void AGhostSerpentVFXActor::StartFlight(AActor *NewTargetActor, FName NewTargetSocketName)
{
    StartFlightFromLocation(GetActorLocation(), NewTargetActor, NewTargetSocketName);
}

void AGhostSerpentVFXActor::StartFlightFromLocation(FVector RiftLocation, AActor *NewTargetActor, FName NewTargetSocketName)
{
    TargetActor = NewTargetActor;
    if (!NewTargetSocketName.IsNone())
    {
        TargetSocketName = NewTargetSocketName;
    }

    FlightStartLocation = RiftLocation;
    SpawnOriginLocation = RiftLocation;
    SetActorLocation(FlightStartLocation);
    FlightTime = 0.f;
    BehaviorState = EGhostSerpentBehaviorState::FlyingToGhost;
    bIsFlying = IsValid(TargetActor);

    GeneratePathControlPoints();
    ResetTrail();
    UpdateTrail(FlightStartLocation);
}

void AGhostSerpentVFXActor::StartAutonomousFromLocation(FVector RiftLocation)
{
    TargetActor = nullptr;
    AbsorbTargetGhost = nullptr;
    OrbitTargetPlayer = nullptr;
    ClearPlayerSlow();

    FlightStartLocation = RiftLocation;
    SpawnOriginLocation = RiftLocation;
    SetActorLocation(RiftLocation);
    FlightTime = 0.f;
    bIsFlying = true;
    BehaviorState = EGhostSerpentBehaviorState::Wandering;

    ResetTrail();
    UpdateTrail(RiftLocation);
    ChooseNewWanderTarget();
}

void AGhostSerpentVFXActor::StopFlight()
{
    bIsFlying = false;
    FlightTime = 0.f;
    ClearPlayerSlow();
    ResetTrail();
}

void AGhostSerpentVFXActor::InitializeMesh()
{
    if (!SerpentMeshComponent)
    {
        return;
    }

    if (!SerpentSkeletalMesh)
    {
        if (USkeletalMesh *ExistingMesh = Cast<USkeletalMesh>(SerpentMeshComponent->GetSkinnedAsset()))
        {
            SerpentSkeletalMesh = ExistingMesh;
            UE_LOG(LogTemp, Log, TEXT("GhostSerpentVFXActor InitializeMesh: adopted mesh from component: %s"), *ExistingMesh->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("GhostSerpentVFXActor InitializeMesh: no SerpentSkeletalMesh assigned and PoseableMeshComponent has no mesh asset."));
        }
    }

    if (SerpentSkeletalMesh)
    {
        SerpentMeshComponent->SetSkinnedAssetAndUpdate(SerpentSkeletalMesh);
    }
}

void AGhostSerpentVFXActor::BuildDefaultBoneNamesIfNeeded()
{
    RuntimeBodyBoneNames.Reset();
    int32 AvailableSequentialBoneCount = 0;
    if (SerpentMeshComponent && SerpentMeshComponent->GetSkinnedAsset())
    {
        for (int32 BoneIndex = 1; BoneIndex <= MaxAutoDetectedSpineBoneCount; ++BoneIndex)
        {
            if (!IsSequentialSpineBoneNameValid(SerpentMeshComponent, BoneIndex))
            {
                break;
            }

            ++AvailableSequentialBoneCount;
        }
    }


    // BodyBoneNames.Reserve(DefaultGeneratedBoneCount);
    // for (int32 BoneIndex = 1; BoneIndex <= DefaultGeneratedBoneCount; ++BoneIndex)
    int32 BoneCountToGenerate = DefaultGeneratedBoneCount;
    if (AvailableSequentialBoneCount > 0)
    {
        BoneCountToGenerate = AvailableSequentialBoneCount;
    }

    BoneCountToGenerate = FMath::Max(BoneCountToGenerate, 1);
    RuntimeBodyBoneNames.Reserve(BoneCountToGenerate);
    for (int32 BoneIndex = 1; BoneIndex <= BoneCountToGenerate; ++BoneIndex)
    {
        RuntimeBodyBoneNames.Add(MakeSequentialSpineBoneName(SerpentMeshComponent, BoneIndex));
    }

    UE_LOG(LogTemp, Log, TEXT("GhostSerpentVFXActor BuildDefaultBoneNamesIfNeeded: generated %d bones"), RuntimeBodyBoneNames.Num());
}

void AGhostSerpentVFXActor::GeneratePathControlPoints()
{
    const FVector TargetLocation = GetSerpentTargetLocation();
    FVector ToTarget = TargetLocation - FlightStartLocation;
    if (ToTarget.IsNearlyZero())
    {
        ToTarget = GetActorForwardVector();
    }

    const FVector Forward = ToTarget.GetSafeNormal();
    FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
    if (Right.IsNearlyZero())
    {
        Right = FVector::RightVector;
    }
    const FVector Up = FVector::CrossProduct(Forward, Right).GetSafeNormal();

    const float SideSign = FMath::RandBool() ? 1.f : -1.f;
    const FVector RandomA = Right * FMath::FRandRange(-PathRandomRadius, PathRandomRadius) + Up * FMath::FRandRange(0.f, PathRandomRadius);
    const FVector RandomB = Right * FMath::FRandRange(-PathRandomRadius, PathRandomRadius) * SideSign + Up * FMath::FRandRange(-PathRandomRadius * 0.35f, PathRandomRadius);

    PathControlPointA = FlightStartLocation + ToTarget * 0.28f + FVector::UpVector * PathLift + RandomA;
    PathControlPointB = FlightStartLocation + ToTarget * 0.72f + FVector::UpVector * (PathLift * 0.45f) + RandomB;
}

void AGhostSerpentVFXActor::ResetTrail()
{
    TrailLocations.Reset();
    RuntimeBoneWorldLocations.Reset();
}

void AGhostSerpentVFXActor::UpdateFlight(float DeltaTime)
{
    if (!IsValid(TargetActor))
    {
        StopFlight();
        return;
    }

    FlightTime += DeltaTime;
    const float Alpha = FlightDuration <= KINDA_SMALL_NUMBER ? 1.f : FMath::Clamp(FlightTime / FlightDuration, 0.f, 1.f);
    const FVector HeadLocation = EvaluateHeadLocation(Alpha);

    SetActorLocation(HeadLocation);
    UpdateTrail(HeadLocation);

    if (bShowDebugPath && GetWorld())
    {
        FVector PreviousPoint = FlightStartLocation;
        for (int32 Step = 1; Step <= 24; ++Step)
        {
            const float StepAlpha = static_cast<float>(Step) / 24.f;
            const FVector Point = EvaluateHeadLocation(StepAlpha);
            DrawDebugLine(GetWorld(), PreviousPoint, Point, DebugPathColor, false, 0.f, 0, 1.5f);
            PreviousPoint = Point;
        }
    }

    if (Alpha >= 1.f || FVector::DistSquared(HeadLocation, GetSerpentTargetLocation()) <= FMath::Square(ArrivalRadius))
    {
        FinishAbsorption();
    }
}

void AGhostSerpentVFXActor::UpdateTrail(const FVector &HeadLocation)
{
    if (TrailLocations.IsEmpty() || FVector::DistSquared(TrailLocations[0], HeadLocation) > FMath::Square(TrailSampleMinDistance))
    {
        TrailLocations.Insert(HeadLocation, 0);
    }

    const float TotalBodyLength = FMath::Max(0.f, (RuntimeBodyBoneNames.Num() - 1) * BodySegmentSpacing);
    const int32 RequiredTrailSampleCount = FMath::Clamp(
        FMath::CeilToInt(TotalBodyLength / TrailSampleMinDistance) + 8,
        2,
        GMaxTrailAllocationCount);

    if (TrailLocations.Num() > RequiredTrailSampleCount)
    {
        TrailLocations.SetNum(RequiredTrailSampleCount);
    }
}

void AGhostSerpentVFXActor::UpdateBodyPose(float DeltaTime)
{
    (void)DeltaTime;

    if (!SerpentMeshComponent || RuntimeBodyBoneNames.IsEmpty())
    {
        return;
    }

    const FTransform ActorTransform = GetActorTransform();
    const float RunningTime = GetWorld() ? GetWorld()->GetTimeSeconds() : FlightTime;

    const int32 BoneCount = RuntimeBodyBoneNames.Num();
    FVector HeadForwardDirection = GetActorForwardVector();
    if (TrailLocations.Num() > 1)
    {
        const FVector TrailForward = TrailLocations[0] - TrailLocations[1];
        if (!TrailForward.IsNearlyZero())
        {
            HeadForwardDirection = TrailForward.GetSafeNormal();
        }
    }

    if (RuntimeBoneWorldLocations.Num() != BoneCount)
    {
        RuntimeBoneWorldLocations.SetNum(BoneCount);
    }

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const float DistanceBack = BodySegmentSpacing * BoneIndex;
        RuntimeBoneWorldLocations[BoneIndex] = GetTrailLocationAtDistance(DistanceBack);
    }

    FVector PreviousBoneDirection = HeadForwardDirection;
    int32 ValidBoneCount = 0;

    for (int32 BoneIndex = 0; BoneIndex < RuntimeBodyBoneNames.Num(); ++BoneIndex)
    {
        const FName BoneName = RuntimeBodyBoneNames[BoneIndex];
        if (BoneName.IsNone() || SerpentMeshComponent->GetBoneIndex(BoneName) == INDEX_NONE)
        {
            continue;
        }
        ++ValidBoneCount;

        const FVector WorldLocation = RuntimeBoneWorldLocations[BoneIndex];

        FVector WorldDirection = FVector::ZeroVector;
        if (BoneCount > 1)
        {
            if (BoneIndex == 0)
            {
                WorldDirection = RuntimeBoneWorldLocations[0] - RuntimeBoneWorldLocations[1];
            }
            else if (BoneIndex == BoneCount - 1)
            {
                WorldDirection = RuntimeBoneWorldLocations[BoneIndex - 1] - RuntimeBoneWorldLocations[BoneIndex];
            }
            else
            {
                WorldDirection = RuntimeBoneWorldLocations[BoneIndex - 1] - RuntimeBoneWorldLocations[BoneIndex + 1];
            }
        }
        
        PreviousBoneDirection = WorldDirection;
        FRotator WorldRotation = WorldDirection.Rotation();
        WorldRotation += BoneRotationOffset;
        WorldRotation += HeadBoneRotationOffset;

        const FVector ComponentLocation = ActorTransform.InverseTransformPosition(WorldLocation);
        const FRotator ComponentRotation = ActorTransform.InverseTransformRotation(WorldRotation.Quaternion()).Rotator();

        SerpentMeshComponent->SetBoneLocationByName(BoneName, ComponentLocation, EBoneSpaces::ComponentSpace);
        SerpentMeshComponent->SetBoneRotationByName(BoneName, ComponentRotation, EBoneSpaces::ComponentSpace);
    }

    if (ValidBoneCount < RuntimeBodyBoneNames.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("GhostSerpentVFXActor valid bones: %d / %d. Unmatched bone names are skipped."), ValidBoneCount, BodyBoneNames.Num());
    }
}

void AGhostSerpentVFXActor::FinishAbsorption()
{
    if (!bIsFlying)
    {
        return;
    }

    bIsFlying = false;
    OnSerpentAbsorbed.Broadcast(this, TargetActor);
    ReceiveSerpentAbsorbed(TargetActor);

    if (bDestroyOnAbsorbed)
    {
        Destroy();
    }
}

FVector AGhostSerpentVFXActor::GetSerpentTargetLocation() const
{
    if (!IsValid(TargetActor))
    {
        return GetActorLocation();
    }

    if (!TargetSocketName.IsNone())
    {
        if (const USkeletalMeshComponent *TargetMesh = TargetActor->FindComponentByClass<USkeletalMeshComponent>())
        {
            if (TargetMesh->DoesSocketExist(TargetSocketName))
            {
                return TargetMesh->GetSocketLocation(TargetSocketName);
            }
        }
    }

    return TargetActor->GetActorLocation() + TargetLocationOffset;
}

FVector AGhostSerpentVFXActor::EvaluateHeadLocation(float Alpha) const
{
    const float T = FMath::Clamp(Alpha, 0.f, 1.f);
    const FVector TargetLocation = GetSerpentTargetLocation();
    const FVector BaseLocation = CubicBezier(FlightStartLocation, PathControlPointA, PathControlPointB, TargetLocation, T);

    FVector PathDirection = TargetLocation - FlightStartLocation;
    if (PathDirection.IsNearlyZero())
    {
        PathDirection = FVector::ForwardVector;
    }

    FVector Right = FVector::CrossProduct(FVector::UpVector, PathDirection.GetSafeNormal()).GetSafeNormal();
    if (Right.IsNearlyZero())
    {
        Right = FVector::RightVector;
    }

    const FVector Up = FVector::CrossProduct(PathDirection.GetSafeNormal(), Right).GetSafeNormal();

    const float SpiralPhase = T * SpiralFrequency * UE_TWO_PI + FlightTime * 2.f;
    const float FadeInOut = FMath::Sin(T * UE_PI);
    const FVector SpiralOffset = (Right * FMath::Cos(SpiralPhase) + Up * FMath::Sin(SpiralPhase)) * SpiralAmplitude * FadeInOut;

    return BaseLocation + SpiralOffset;
}

FVector AGhostSerpentVFXActor::GetTrailLocationAtDistance(float DistanceBack) const
{
    if (TrailLocations.IsEmpty())
    {
        return GetActorLocation();
    }

    if (TrailLocations.Num() == 1 || DistanceBack <= 0.f)
    {
        return TrailLocations[0];
    }

    float AccumulatedDistance = 0.f;
    for (int32 Index = 1; Index < TrailLocations.Num(); ++Index)
    {
        const FVector NewerPoint = TrailLocations[Index - 1];
        const FVector OlderPoint = TrailLocations[Index];
        const float SegmentLength = FVector::Dist(NewerPoint, OlderPoint);

        if (AccumulatedDistance + SegmentLength >= DistanceBack)
        {
            const float SegmentAlpha = SegmentLength <= KINDA_SMALL_NUMBER
                                           ? 0.f
                                           : (DistanceBack - AccumulatedDistance) / SegmentLength;
            const int32 P0Index = FMath::Max(Index - 2, 0);
            const int32 P1Index = Index - 1;
            const int32 P2Index = Index;
            const int32 P3Index = FMath::Min(Index + 1, TrailLocations.Num() - 1);
            return CatmullRomInterpolate(
                TrailLocations[P0Index],
                TrailLocations[P1Index],
                TrailLocations[P2Index],
                TrailLocations[P3Index],
                SegmentAlpha);
        }

        AccumulatedDistance += SegmentLength;
    }

    const FVector TailLocation = TrailLocations.Last();
    if (TrailLocations.Num() == 1)
    {
        return TailLocation;
    }

    FVector TailForward = TrailLocations[TrailLocations.Num() - 2] - TailLocation;
    if (TailForward.IsNearlyZero())
    {
        TailForward = GetActorForwardVector();
    }
    else
    {
        TailForward = TailForward.GetSafeNormal();
    }

    return TailLocation - TailForward * FMath::Max(0.f, DistanceBack - AccumulatedDistance);
}

FVector AGhostSerpentVFXActor::GetTrailDirectionAtDistance(float DistanceBack) const
{
    const FVector FrontLocation = GetTrailLocationAtDistance(DistanceBack);
    const FVector BackLocation = GetTrailLocationAtDistance(DistanceBack + FMath::Max(BodySegmentSpacing * 0.5f, 12.f));
    return (FrontLocation - BackLocation).GetSafeNormal();
}

void AGhostSerpentVFXActor::UpdateAutonomousBehavior(float DeltaTime)
{
    FlightTime += DeltaTime;

    if (AGhostCharacter *VisibleGhost = FindVisibleGhost())
    {
        AbsorbTargetGhost = VisibleGhost;
        OrbitTargetPlayer = nullptr;
        ClearPlayerSlow();
        BehaviorState = EGhostSerpentBehaviorState::FlyingToGhost;
    }
    else if (BehaviorState != EGhostSerpentBehaviorState::OrbitingPlayer)
    {
        if (AWomenCharacter *VisiblePlayer = FindVisiblePlayer())
        {
            OrbitTargetPlayer = VisiblePlayer;
            OrbitAngleDegrees = FMath::FRandRange(0.f, 360.f);
            BehaviorState = EGhostSerpentBehaviorState::OrbitingPlayer;
        }
    }

    switch (BehaviorState)
    {
    case EGhostSerpentBehaviorState::FlyingToGhost:
        UpdateFlyToGhostBehavior(DeltaTime);
        break;
    case EGhostSerpentBehaviorState::OrbitingPlayer:
        UpdateOrbitPlayer(DeltaTime);
        break;
    case EGhostSerpentBehaviorState::Wandering:
    default:
        UpdateWander(DeltaTime);
        break;
    }
}

void AGhostSerpentVFXActor::UpdateWander(float DeltaTime)
{
    ClearPlayerSlow();

    if (FVector::DistSquared(GetActorLocation(), WanderTargetLocation) <= FMath::Square(WanderTargetAcceptanceRadius))
    {
        ChooseNewWanderTarget();
    }

    MoveSerpentToward(WanderTargetLocation, WanderSpeed, DeltaTime);
}

void AGhostSerpentVFXActor::UpdateOrbitPlayer(float DeltaTime)
{
    AWomenCharacter *Player = OrbitTargetPlayer;
    if (!IsValid(Player) || FVector::DistSquared(GetActorLocation(), Player->GetActorLocation()) > FMath::Square(PlayerForgetRadius))
    {
        OrbitTargetPlayer = nullptr;
        ClearPlayerSlow();
        BehaviorState = EGhostSerpentBehaviorState::Wandering;
        ChooseNewWanderTarget();
        return;
    }

    OrbitAngleDegrees = FRotator::NormalizeAxis(OrbitAngleDegrees + OrbitAngularSpeedDegrees * DeltaTime);
    const float OrbitRadians = FMath::DegreesToRadians(OrbitAngleDegrees);
    const FVector OrbitOffset(
        FMath::Cos(OrbitRadians) * OrbitRadius,
        FMath::Sin(OrbitRadians) * OrbitRadius,
        OrbitHeight + FMath::Sin(FlightTime * 2.3f) * 35.f);
    const FVector DesiredLocation = Player->GetActorLocation() + OrbitOffset;

    MoveSerpentToward(DesiredLocation, OrbitSpeed, DeltaTime);

    if (FVector::DistSquared(GetActorLocation(), Player->GetActorLocation()) <= FMath::Square(PlayerSlowTouchRadius))
    {
        ApplyPlayerSlow(Player);
    }
    else if (SlowedPlayer == Player)
    {
        ClearPlayerSlow();
    }
}

void AGhostSerpentVFXActor::UpdateFlyToGhostBehavior(float DeltaTime)
{
    AGhostCharacter *Ghost = AbsorbTargetGhost;
    if (!IsValid(Ghost))
    {
        Ghost = Cast<AGhostCharacter>(TargetActor.Get());
    }

    if (!IsValid(Ghost))
    {
        BehaviorState = EGhostSerpentBehaviorState::Wandering;
        ChooseNewWanderTarget();
        return;
    }

    const FVector GhostLocation = Ghost->GetActorLocation() + TargetLocationOffset;
    MoveSerpentToward(GhostLocation, FlyToGhostSpeed, DeltaTime);

    if (FVector::DistSquared(GetActorLocation(), GhostLocation) <= FMath::Square(GhostAbsorbRadius))
    {
        FinishGhostAbsorption(Ghost);
    }
}

void AGhostSerpentVFXActor::MoveSerpentToward(const FVector &DesiredLocation, float Speed, float DeltaTime)
{
    const FVector CurrentLocation = GetActorLocation();
    const FVector ToDesired = DesiredLocation - CurrentLocation;
    const float Distance = ToDesired.Size();
    if (Distance <= KINDA_SMALL_NUMBER)
    {
        UpdateTrail(CurrentLocation);
        return;
    }

    const FVector Direction = ToDesired / Distance;
    const float Step = FMath::Min(Distance, FMath::Max(0.f, Speed) * DeltaTime);
    FVector NewLocation = CurrentLocation + Direction * Step;

    const FVector Side = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();
    NewLocation += (Side * FMath::Sin(FlightTime * WaveSpeed) * 18.f + FVector::UpVector * FMath::Sin(FlightTime * 3.1f) * 12.f) * DeltaTime;

    SetActorLocation(NewLocation);
    UpdateTrail(NewLocation);
}

void AGhostSerpentVFXActor::FinishGhostAbsorption(AGhostCharacter *GhostTarget)
{
    if (!IsValid(GhostTarget))
    {
        return;
    }

    ClearPlayerSlow();
    GhostTarget->ApplyTemporaryMoveSpeedMultiplier(GhostSpeedBoostMultiplier, GhostSpeedBoostDuration);
    TargetActor = GhostTarget;
    FinishAbsorption();
}

AWomenCharacter *AGhostSerpentVFXActor::FindVisiblePlayer() const
{
    if (!GetWorld())
    {
        return nullptr;
    }

    AWomenCharacter *BestPlayer = nullptr;
    float BestDistanceSquared = TNumericLimits<float>::Max();

    for (TActorIterator<AWomenCharacter> It(GetWorld()); It; ++It)
    {
        AWomenCharacter *Candidate = *It;
        if (!IsValid(Candidate) || !CanSeeActor(Candidate, PlayerSenseRadius))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
        if (!BestPlayer || DistanceSquared < BestDistanceSquared)
        {
            BestPlayer = Candidate;
            BestDistanceSquared = DistanceSquared;
        }
    }

    return BestPlayer;
}

AGhostCharacter *AGhostSerpentVFXActor::FindVisibleGhost() const
{
    if (!GetWorld())
    {
        return nullptr;
    }

    AGhostCharacter *BestGhost = nullptr;
    float BestDistanceSquared = TNumericLimits<float>::Max();

    for (TActorIterator<AGhostCharacter> It(GetWorld()); It; ++It)
    {
        AGhostCharacter *Candidate = *It;
        if (!IsValid(Candidate) || !CanSeeActor(Candidate, GhostSenseRadius))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
        if (!BestGhost || DistanceSquared < BestDistanceSquared)
        {
            BestGhost = Candidate;
            BestDistanceSquared = DistanceSquared;
        }
    }

    return BestGhost;
}

bool AGhostSerpentVFXActor::CanSeeActor(const AActor *Candidate, float SenseRadius) const
{
    if (!IsValid(Candidate) || SenseRadius <= 0.f)
    {
        return false;
    }

    const FVector StartLocation = GetActorLocation();
    const FVector EndLocation = Candidate->GetActorLocation();
    if (FVector::DistSquared(StartLocation, EndLocation) > FMath::Square(SenseRadius))
    {
        return false;
    }

    if (!bRequireLineOfSight || !GetWorld())
    {
        return true;
    }

    FHitResult HitResult;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ghostSerpentSight), false, this);
    QueryParams.AddIgnoredActor(Candidate);
    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        StartLocation,
        EndLocation,
        ECC_Visibility,
        QueryParams);

    return !bHit;
}

void AGhostSerpentVFXActor::ChooseNewWanderTarget()
{
    const FVector RandomOffset = FMath::VRand() * FMath::FRandRange(WanderRadius * 0.25f, WanderRadius);
    WanderTargetLocation = SpawnOriginLocation + RandomOffset;
    WanderTargetLocation.Z = SpawnOriginLocation.Z + FMath::FRandRange(-120.f, 180.f);
}

void AGhostSerpentVFXActor::ApplyPlayerSlow(AWomenCharacter *Player)
{
    if (!IsValid(Player))
    {
        ClearPlayerSlow();
        return;
    }

    if (SlowedPlayer != Player)
    {
        ClearPlayerSlow();
        SlowedPlayer = Player;
        if (UCharacterMovementComponent *MovementComponent = Player->GetCharacterMovement())
        {
            CachedSlowedPlayerWalkSpeed = MovementComponent->MaxWalkSpeed;
            bHasCachedSlowedPlayerWalkSpeed = true;
        }
    }

    if (UCharacterMovementComponent *MovementComponent = Player->GetCharacterMovement())
    {
        const float BaseSpeed = bHasCachedSlowedPlayerWalkSpeed ? CachedSlowedPlayerWalkSpeed : MovementComponent->MaxWalkSpeed;
        MovementComponent->MaxWalkSpeed = FMath::Min(MovementComponent->MaxWalkSpeed, BaseSpeed * PlayerSlowMultiplier);
    }
}

void AGhostSerpentVFXActor::ClearPlayerSlow()
{
    if (IsValid(SlowedPlayer) && bHasCachedSlowedPlayerWalkSpeed)
    {
        if (UCharacterMovementComponent *MovementComponent = SlowedPlayer->GetCharacterMovement())
        {
            MovementComponent->MaxWalkSpeed = CachedSlowedPlayerWalkSpeed;
        }
    }

    SlowedPlayer = nullptr;
    CachedSlowedPlayerWalkSpeed = 0.f;
    bHasCachedSlowedPlayerWalkSpeed = false;
}