#include "PickupActorAAAJesusCross.h"

// --- 新增的头文件引用 (来自图 1) ---
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MyAIController.h"

namespace
{
    const FName JesusCrossTag(TEXT("JesusCross"));
    constexpr float DebugVerticalOffset = 10.f;
    constexpr float DebugPointSize = 14.f;

    struct FCrossTriangle
    {
        FVector A = FVector::ZeroVector;
        FVector B = FVector::ZeroVector;
        FVector C = FVector::ZeroVector;
    };

    bool AreLocationsEquivalent(const FVector& A, const FVector& B)
    {
        return A.Equals(B, 0.1f);
    }

    bool HasMatchingEdge(const TArray<TPair<FVector, FVector>>& Edges, const FVector& First, const FVector& Second)
    {
        return Edges.ContainsByPredicate(
            [&](const TPair<FVector, FVector>& ExistingEdge)
            {
                return (AreLocationsEquivalent(ExistingEdge.Key, First) && AreLocationsEquivalent(ExistingEdge.Value, Second))
                    || (AreLocationsEquivalent(ExistingEdge.Key, Second) && AreLocationsEquivalent(ExistingEdge.Value, First));
            });
    }

    void AddUniqueEdge(TArray<TPair<FVector, FVector>>& Edges, const FVector& First, const FVector& Second)
    {
        if (AreLocationsEquivalent(First, Second) || HasMatchingEdge(Edges, First, Second))
        {
            return;
        }
        Edges.Emplace(First, Second);
    }

    void BuildUniqueTriangleEdges(const TArray<FCrossTriangle>& Triangles, TArray<TPair<FVector, FVector>>& OutEdges)
    {
        OutEdges.Reset();
        for (const FCrossTriangle& Triangle : Triangles)
        {
            AddUniqueEdge(OutEdges, Triangle.A, Triangle.B);
            AddUniqueEdge(OutEdges, Triangle.B, Triangle.C);
            AddUniqueEdge(OutEdges, Triangle.C, Triangle.A);
        }
    }


    FVector GetCrossAnchorLocation(const APickupActorAAAJesusCross* Cross)
    {
        return IsValid(Cross) ? Cross->GetActorLocation() : FVector::ZeroVector;
    }

    float GetEffectiveLinkDistance(const APickupActorAAAJesusCross* A, const APickupActorAAAJesusCross* B)
    {
        if (!IsValid(A) || !IsValid(B))
        {
            return 0.f;
        }

        return FMath::Min(A->GetConfiguredLinkDistance(), B->GetConfiguredLinkDistance());
    }

    bool AreCrossesLinked(const APickupActorAAAJesusCross* A, const APickupActorAAAJesusCross* B)
    {
        if (!IsValid(A) || !IsValid(B) || A == B)
        {
            return false;
        }

        if (!A->ActorHasTag(JesusCrossTag) || !B->ActorHasTag(JesusCrossTag))
        {
            return false;
        }

        if (!A->CanParticipateInWeaknessZone() || !B->CanParticipateInWeaknessZone())
        {
            return false;
        }

        return FVector::DistSquared2D(GetCrossAnchorLocation(A), GetCrossAnchorLocation(B))
            <= FMath::Square(GetEffectiveLinkDistance(A, B));
    }

    bool ShouldUseActorAsDeterministicLeaderCandidate(const APickupActorAAAJesusCross* Actor, const APickupActorAAAJesusCross* Other)
    {
        if (!IsValid(Actor))
        {
            return false;
        }

        if (!IsValid(Other))
        {
            return true;
        }

        return Actor->GetPathName() < Other->GetPathName();
    }

    APickupActorAAAJesusCross* ResolveComponentLeader(const TArray<APickupActorAAAJesusCross*>& Component)
    {
        APickupActorAAAJesusCross* Leader = nullptr;

        for (APickupActorAAAJesusCross* Candidate : Component)
        {
            if (ShouldUseActorAsDeterministicLeaderCandidate(Candidate, Leader))
            {
                Leader = Candidate;
            }
        }

        return Leader;
    }

    float GetTriangleArea2D(const FVector& A, const FVector& B, const FVector& C)
    {
        const FVector2D AB(B.X - A.X, B.Y - A.Y);
        const FVector2D AC(C.X - A.X, C.Y - A.Y);
        return FMath::Abs((AB.X * AC.Y) - (AB.Y * AC.X)) * 0.5f;
    }

    bool IsPointInsideTriangle2D(const FVector& Point, const FCrossTriangle& Triangle)
    {
        const FVector2D P(Point.X, Point.Y);
        const FVector2D A(Triangle.A.X, Triangle.A.Y);
        const FVector2D B(Triangle.B.X, Triangle.B.Y);
        const FVector2D C(Triangle.C.X, Triangle.C.Y);

        const FVector2D V0 = C - A;
        const FVector2D V1 = B - A;
        const FVector2D V2 = P - A;

        const float Dot00 = FVector2D::DotProduct(V0, V0);
        const float Dot01 = FVector2D::DotProduct(V0, V1);
        const float Dot02 = FVector2D::DotProduct(V0, V2);
        const float Dot11 = FVector2D::DotProduct(V1, V1);
        const float Dot12 = FVector2D::DotProduct(V1, V2);
        const float Denominator = (Dot00 * Dot11) - (Dot01 * Dot01);

        if (FMath::IsNearlyZero(Denominator))
        {
            return false;
        }

        const float InverseDenominator = 1.f / Denominator;
        const float U = ((Dot11 * Dot02) - (Dot01 * Dot12)) * InverseDenominator;
        const float V = ((Dot00 * Dot12) - (Dot01 * Dot02)) * InverseDenominator;
        const float Epsilon = 0.001f;

        return U >= -Epsilon && V >= -Epsilon && (U + V) <= (1.f + Epsilon);
    }

    void GatherPlacedCrosses(UWorld* World, TArray<APickupActorAAAJesusCross*>& OutCrosses)
    {
        OutCrosses.Reset();

        if (!World)
        {
            return;
        }

        for (TActorIterator<APickupActorAAAJesusCross> It(World); It; ++It)
        {
            APickupActorAAAJesusCross* Cross = *It;
            if (!IsValid(Cross) || !Cross->ActorHasTag(JesusCrossTag) || !Cross->CanParticipateInWeaknessZone())
            {
                continue;
            }

            OutCrosses.Add(Cross);
        }
    }

    void GatherConnectedComponent(
        APickupActorAAAJesusCross* StartCross,
        const TArray<APickupActorAAAJesusCross*>& AllCrosses,
        TArray<APickupActorAAAJesusCross*>& OutComponent)
    {
        OutComponent.Reset();

        if (!IsValid(StartCross))
        {
            return;
        }

        TArray<APickupActorAAAJesusCross*> Pending;
        Pending.Add(StartCross);

        while (Pending.Num() > 0)
        {
            APickupActorAAAJesusCross* Current = Pending.Pop(EAllowShrinking::No);
            if (!IsValid(Current) || OutComponent.Contains(Current))
            {
                continue;
            }

            OutComponent.Add(Current);

            for (APickupActorAAAJesusCross* Candidate : AllCrosses)
            {
                if (!IsValid(Candidate) || Candidate == Current || OutComponent.Contains(Candidate))
                {
                    continue;
                }

                if (AreCrossesLinked(Current, Candidate))
                {
                    Pending.Add(Candidate);
                }
            }
        }
    }

    bool TryBuildClosedPolygonTriangles(
        const TArray<APickupActorAAAJesusCross*>& Component,
        const float MinimumTriangleArea,
        TArray<FCrossTriangle>& OutTriangles)
    {
        OutTriangles.Reset();

        if (Component.Num() < 3)
        {
            return false;
        }

        FVector2D Centroid(0.f, 0.f);
        for (const APickupActorAAAJesusCross* Cross : Component)
        {
            const FVector Location = GetCrossAnchorLocation(Cross);
            Centroid.X += Location.X;
            Centroid.Y += Location.Y;
        }
        Centroid /= static_cast<float>(Component.Num());

        TArray<APickupActorAAAJesusCross*> OrderedComponent = Component;
        OrderedComponent.Sort([Centroid](const APickupActorAAAJesusCross& Left, const APickupActorAAAJesusCross& Right)
        {
            const FVector LeftLocation = Left.GetActorLocation();
            const FVector RightLocation = Right.GetActorLocation();
            const float LeftAngle = FMath::Atan2(LeftLocation.Y - Centroid.Y, LeftLocation.X - Centroid.X);
            const float RightAngle = FMath::Atan2(RightLocation.Y - Centroid.Y, RightLocation.X - Centroid.X);
            return LeftAngle < RightAngle;
        });

        for (int32 Index = 0; Index < OrderedComponent.Num(); ++Index)
        {
            APickupActorAAAJesusCross* Current = OrderedComponent[Index];
            APickupActorAAAJesusCross* Next = OrderedComponent[(Index + 1) % OrderedComponent.Num()];
            if (!AreCrossesLinked(Current, Next))
            {
                return false;
            }
        }

        const FVector Anchor = GetCrossAnchorLocation(OrderedComponent[0]);
        bool bAddedAnyTriangle = false;

        for (int32 Index = 1; Index < OrderedComponent.Num() - 1; ++Index)
        {
            const FVector B = GetCrossAnchorLocation(OrderedComponent[Index]);
            const FVector C = GetCrossAnchorLocation(OrderedComponent[Index + 1]);
            if (GetTriangleArea2D(Anchor, B, C) < MinimumTriangleArea)
            {
                continue;
            }

            FCrossTriangle& Triangle = OutTriangles.AddDefaulted_GetRef();
            Triangle.A = Anchor;
            Triangle.B = B;
            Triangle.C = C;
            bAddedAnyTriangle = true;
        }

        return bAddedAnyTriangle;
    }

    void BuildCliqueTriangles(
        const TArray<APickupActorAAAJesusCross*>& Component,
        const float MinimumTriangleArea,
        TArray<FCrossTriangle>& OutTriangles)
    {
        OutTriangles.Reset();

        for (int32 FirstIndex = 0; FirstIndex < Component.Num(); ++FirstIndex)
        {
            APickupActorAAAJesusCross* First = Component[FirstIndex];
            if (!IsValid(First))
            {
                continue;
            }

            for (int32 SecondIndex = FirstIndex + 1; SecondIndex < Component.Num(); ++SecondIndex)
            {
                APickupActorAAAJesusCross* Second = Component[SecondIndex];
                if (!IsValid(Second) || !AreCrossesLinked(First, Second))
                {
                    continue;
                }

                for (int32 ThirdIndex = SecondIndex + 1; ThirdIndex < Component.Num(); ++ThirdIndex)
                {
                    APickupActorAAAJesusCross* Third = Component[ThirdIndex];
                    if (!IsValid(Third)
                        || !AreCrossesLinked(First, Third)
                        || !AreCrossesLinked(Second, Third))
                    {
                        continue;
                    }

                    const FVector A = GetCrossAnchorLocation(First);
                    const FVector B = GetCrossAnchorLocation(Second);
                    const FVector C = GetCrossAnchorLocation(Third);
                    if (GetTriangleArea2D(A, B, C) < MinimumTriangleArea)
                    {
                        continue;
                    }

                    FCrossTriangle& Triangle = OutTriangles.AddDefaulted_GetRef();
                    Triangle.A = A;
                    Triangle.B = B;
                    Triangle.C = C;
                }
            }
        }
    }

    float GetCrossPawnCollisionRadius(const APawn* Pawn)
    {
        if (!IsValid(Pawn))
        {
            return 0.f;
        }

        if (const ACharacter* Character = Cast<ACharacter>(Pawn))
        {
            if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
            {
                return Capsule->GetScaledCapsuleRadius();
            }
        }
        return Pawn->GetSimpleCollisionRadius();
    }
}

APickupActorAAAJesusCross::APickupActorAAAJesusCross()
{
    PrimaryActorTick.bCanEverTick = true;

    HoldType = EHoldItemType::Cross;
    FP_SocketName = TEXT("RightSocket");
    FP_LocationOffset = FVector::ZeroVector;
    FP_RotationOffset = FRotator::ZeroRotator;
    TP_SocketName = TEXT("RightSocket");
    TP_LocationOffset = FVector::ZeroVector;
    TP_RotationOffset = FRotator::ZeroRotator;

    ItemMassKg = 1.2f;
    ItemThrowForceMultiplier = 0.75f;
    ItemLinearDamping = 0.08f;
    ItemAngularDamping = 0.25f;
    ItemThrowSpinRateDegrees = 1200.f;

    // 十字架默认允许和 Pawn 发生阻挡。
    // 这样鬼在显形恢复实体碰撞后，靠近十字架时就能把它顶倒；
    // 隐身时鬼对 PhysicsBody 仍是 Overlap，所以不会在平时把十字架乱顶走。
    bAllowPawnCollision = true;
    ApplyReleasedCollisionProfile();

    CrossLinkRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("CrossLinkRootComponent"));
    CrossLinkRootComponent->SetupAttachment(VisualMeshRootComponent);

    if (MeshComponent)
    {
        MeshComponent->SetNotifyRigidBodyCollision(true);
        MeshComponent->OnComponentHit.AddDynamic(this, &APickupActorAAAJesusCross::HandleCrossMeshHit);
    }

    Tags.AddUnique(JesusCrossTag);
    Tags.AddUnique(FName("Cross"));
    Tags.AddUnique(FName("Pickup"));
    Tags.AddUnique(FName("WeaknessZone"));
}

void APickupActorAAAJesusCross::BeginPlay()
{
    Super::BeginPlay();

    if (!HasAuthority())
    {
        return;
    }

    EnsureReleasedWorldPhysicsState(true);
    ZoneRefreshAccumulator = ZoneRefreshInterval;
    RefreshCrossZone();
}

void APickupActorAAAJesusCross::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    EnsureReleasedWorldPhysicsState(false);
}

void APickupActorAAAJesusCross::EndPlay(const EEndPlayReason::Type EndPlayReason)
{

    // 清理网格
    ClearLinkMeshes();

    if (HasAuthority())
    {
        ClearAllAffectedControllers(EndPlayReason == EEndPlayReason::Destroyed);
    }

    Super::EndPlay(EndPlayReason);
}

void APickupActorAAAJesusCross::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 更新脉冲视觉效果
    UpdateLinkMeshPulseVisuals(DeltaTime);

    if (!HasAuthority())
    {
        return;
    }

    TryApplyRevealedGhostProximityImpulse();

    ZoneRefreshAccumulator += DeltaTime;
    if (ZoneRefreshAccumulator < FMath::Max(ZoneRefreshInterval, 0.02f))
    {
        return;
    }

    ZoneRefreshAccumulator = 0.f;
    RefreshCrossZone();
}

void APickupActorAAAJesusCross::OnPickedUp()
{
    Super::OnPickedUp();

    // 清理链接网格
    ClearLinkMeshes();

    if (!HasAuthority())
    {
        return;
    }

    ClearAllAffectedControllers();
}

void APickupActorAAAJesusCross::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    Super::OnPutDown(PlaceLocation, PlaceRotation);

    if (!HasAuthority())
    {
        return;
    }

    EnsureReleasedWorldPhysicsState(true);
    ZoneRefreshAccumulator = ZoneRefreshInterval;
    RefreshCrossZone();
}

void APickupActorAAAJesusCross::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    Super::OnThrown(ThrowDirection, ThrowForce);

    if (!HasAuthority())
    {
        return;
    }

    EnsureReleasedWorldPhysicsState(true);
    ZoneRefreshAccumulator = ZoneRefreshInterval;
    RefreshCrossZone();
}

void APickupActorAAAJesusCross::DisableByRage_Implementation(AActor* DisablingActor)
{
    Super::DisableByRage_Implementation(DisablingActor);
    // 清理链接网格
    ClearLinkMeshes();

    if (!HasAuthority())
    {
        return;
    }

    ClearAllAffectedControllers();
}

void APickupActorAAAJesusCross::RestoreAfterRageDisable_Implementation()
{
    Super::RestoreAfterRageDisable_Implementation();

    if (!HasAuthority())
    {
        return;
    }

    EnsureReleasedWorldPhysicsState(true);
    ZoneRefreshAccumulator = ZoneRefreshInterval;
    RefreshCrossZone();
}

bool APickupActorAAAJesusCross::CanParticipateInWeaknessZone() const
{
    if (IsHeldByPlayer() || IsDisabledByRage())
    {
        return false;
    }

    if (!bRequireUprightForWeaknessZone)
    {
        return true;
    }

    const float UprightDot = FVector::DotProduct(GetActorUpVector().GetSafeNormal(), FVector::UpVector);
    return UprightDot >= FMath::Clamp(UprightDotThreshold, 0.f, 1.f);
}

float APickupActorAAAJesusCross::GetConfiguredLinkDistance() const
{
    return LinkDistance;
}

void APickupActorAAAJesusCross::EnsureReleasedWorldPhysicsState(bool bWakeRigidBodies)
{
    UWorld* World = GetWorld();
    if (!MeshComponent || IsHeldByPlayer() || !World || !World->IsGameWorld() || IsActorBeingDestroyed())
    {
        return;
    }

    MeshComponent->SetMobility(EComponentMobility::Movable);
    ApplyReleasedCollisionProfile();
    MeshComponent->SetSimulatePhysics(true);
    MeshComponent->SetEnableGravity(true);

    if (bWakeRigidBodies && MeshComponent->IsRegistered())
    {
        MeshComponent->WakeAllRigidBodies();
    }
}

void APickupActorAAAJesusCross::RefreshCrossZone()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        // 清理链接网格
        ClearLinkMeshes();
        ClearAllAffectedControllers();
        return;
    }

    TArray<APickupActorAAAJesusCross*> AllCrosses;
    GatherPlacedCrosses(World, AllCrosses);

    const float DebugDuration = FMath::Max(ZoneRefreshInterval * 1.2f, 0.05f);
    const FVector ThisLocation = GetActorLocation() + FVector(0.f, 0.f, DebugVerticalOffset);

    if (bEnableCrossDebug)
    {
        DrawDebugSphere(
            World,
            ThisLocation,
            14.f,
            12,
            CanParticipateInWeaknessZone() ? LinkedDebugColor : InactiveDebugColor,
            false,
            DebugDuration,
            0,
            1.5f);
    }

    if (!CanParticipateInWeaknessZone())
    {
        // 清理链接网格
        ClearLinkMeshes();
        ClearAllAffectedControllers();
        return;
    }

    for (APickupActorAAAJesusCross* Candidate : AllCrosses)
    {
        if (!IsValid(Candidate) || Candidate == this || !AreCrossesLinked(this, Candidate))
        {
            continue;
        }

        if (bEnableCrossDebug && GetPathName() < Candidate->GetPathName())
        {
            DrawDebugLine(
                World,
                ThisLocation,
                Candidate->GetActorLocation() + FVector(0.f, 0.f, DebugVerticalOffset),
                LinkedDebugColor,
                false,
                DebugDuration,
                0,
                2.f);
        }
    }

    TArray<APickupActorAAAJesusCross*> ConnectedComponent;
    GatherConnectedComponent(this, AllCrosses, ConnectedComponent);

    if (ConnectedComponent.Num() < FMath::Max(3, MinimumCrossCount))
    {
        ClearLinkMeshes();
        ClearAllAffectedControllers();
        return;
    }

    TArray<FCrossTriangle> ValidTriangles;
    if (!TryBuildClosedPolygonTriangles(ConnectedComponent, MinimumTriangleArea, ValidTriangles))
    {
        BuildCliqueTriangles(ConnectedComponent, MinimumTriangleArea, ValidTriangles);
    }

    APickupActorAAAJesusCross* ComponentLeader = ResolveComponentLeader(ConnectedComponent);
    if (ComponentLeader != this)
    {
        ClearLinkMeshes();
        ClearAllAffectedControllers();
        return;
    }

    if (ValidTriangles.IsEmpty())
    {
        ClearLinkMeshes();
        ClearAllAffectedControllers();
        return;
    }

    TArray<TPair<FVector, FVector>> TriangleEdges;
    BuildUniqueTriangleEdges(ValidTriangles, TriangleEdges);
    RebuildLinkMeshes(TriangleEdges);

    if (bEnableCrossDebug)
    {
        for (const TPair<FVector, FVector>& Edge : TriangleEdges)
        {
            const FVector Start = Edge.Key + FVector(0.f, 0.f, DebugVerticalOffset);
            const FVector End = Edge.Value + FVector(0.f, 0.f, DebugVerticalOffset);

            DrawDebugLine(World, Start, End, TriangleDebugColor, false, DebugDuration, 0, 4.f);
            DrawDebugPoint(World, (Start + End) * 0.5f, DebugPointSize * 0.7f, TriangleDebugColor, false, DebugDuration);
        }
    }

    TArray<TWeakObjectPtr<AMyAIController>> ControllersInsideZone;
    for (TActorIterator<APawn> It(World); It; ++It)
    {
        APawn* TargetPawn = *It;
        if (!IsValid(TargetPawn))
        {
            continue;
        }

        AMyAIController* TargetController = Cast<AMyAIController>(TargetPawn->GetController());
        if (!TargetController)
        {
            continue;
        }

        const FVector PawnLocation = TargetPawn->GetActorLocation();
        bool bInsideAnyTriangle = false;
        for (const FCrossTriangle& Triangle : ValidTriangles)
        {
            if (IsPointInsideTriangle2D(PawnLocation, Triangle))
            {
                bInsideAnyTriangle = true;
                break;
            }
        }

        if (!bInsideAnyTriangle)
        {
            continue;
        }

        ControllersInsideZone.Add(TargetController);
        TargetController->ApplyWeakness(WeaknessRefreshDuration);
        TargetController->ApplySlowSource(this, GhostSlowMultiplier);

        if (bEnableCrossDebug)
        {
            DrawDebugLine(
                World,
                ThisLocation,
                PawnLocation + FVector(0.f, 0.f, 24.f),
                TriangleDebugColor,
                false,
                DebugDuration,
                0,
                1.2f);
        }
    }

    for (const TWeakObjectPtr<AMyAIController>& ControllerPtr : AffectedControllers)
    {
        AMyAIController* Controller = ControllerPtr.Get();
        if (!Controller)
        {
            continue;
        }

        const bool bStillInsideZone = ControllersInsideZone.ContainsByPredicate(
            [Controller](const TWeakObjectPtr<AMyAIController>& Candidate)
            {
                return Candidate.Get() == Controller;
            });

        if (!bStillInsideZone)
        {
            Controller->RemoveSlowSource(this);
        }
    }

    AffectedControllers = ControllersInsideZone;
}

void APickupActorAAAJesusCross::ClearAllAffectedControllers(bool bNotifyControllers)
{
    TArray<TWeakObjectPtr<AMyAIController>> ControllersToClear = MoveTemp(AffectedControllers);
    AffectedControllers.Reset();

    if (!bNotifyControllers)
    {
        return;
    }

    for (const TWeakObjectPtr<AMyAIController>& ControllerPtr : ControllersToClear)
    {
        if (!ControllerPtr.IsValid())
        {
            continue;
        }

        if (AMyAIController* Controller = ControllerPtr.Get(); IsValid(Controller) && !Controller->IsActorBeingDestroyed())
        {
            Controller->RemoveSlowSource(this);
        }
    }
}

// 生成或重建连接线网格
void APickupActorAAAJesusCross::RebuildLinkMeshes(const TArray<TPair<FVector, FVector>>& LinkEdges)
{
    ClearLinkMeshes();

    // 前置检查：确保渲染开关开启、根组件存在、网格存在且有点位数据
    if (!bRenderLinkMeshes || !CrossLinkRootComponent || !LinkSegmentMesh || LinkEdges.IsEmpty())
    {
        return;
    }

    const FTransform RootTransform = CrossLinkRootComponent->GetComponentTransform();

    for (const TPair<FVector, FVector>& LinkEdge : LinkEdges)
    {
        // 创建 Spline Mesh 组件
        USplineMeshComponent* LinkComponent = NewObject<USplineMeshComponent>(this);
        if (!LinkComponent)
        {
            continue;
        }

        // 坐标转换：将世界坐标转换为相对于根组件的局部坐标
        const FVector StartWorld = LinkEdge.Key;
        const FVector EndWorld = LinkEdge.Value;
        const FVector StartLocal = RootTransform.InverseTransformPosition(StartWorld);
        const FVector EndLocal = RootTransform.InverseTransformPosition(EndWorld);
        const FVector Tangent = (EndLocal - StartLocal) * 0.5f;

        // 设置组件属性
        LinkComponent->SetupAttachment(CrossLinkRootComponent);
        LinkComponent->SetMobility(EComponentMobility::Movable);
        LinkComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        LinkComponent->SetGenerateOverlapEvents(false);
        LinkComponent->SetCanEverAffectNavigation(false);
        LinkComponent->SetCastShadow(false);
        LinkComponent->SetVisibleInRayTracing(false);
        LinkComponent->SetStaticMesh(LinkSegmentMesh);
        LinkComponent->SetForwardAxis(LinkForwardAxis, false);
        LinkComponent->RegisterComponent();

        // 设置样条起点和终点（形成直线段）
        LinkComponent->SetStartScale(FVector2D(LinkThickness, LinkThickness), false);
        LinkComponent->SetEndScale(FVector2D(LinkThickness, LinkThickness), false);
        LinkComponent->SetStartAndEnd(StartLocal, Tangent, EndLocal, Tangent, true);

        // --- 材质处理逻辑 ---
        const int32 MaterialSlotCount = FMath::Max(LinkSegmentMesh->GetStaticMaterials().Num(), 1);
        for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
        {
            // 获取基础材质（优先使用覆盖材质）
            UMaterialInterface* BaseMaterial = LinkMaterialOverride ? LinkMaterialOverride : LinkComponent->GetMaterial(MaterialIndex);
            if (!BaseMaterial)
            {
                continue;
            }

            // 创建动态材质实例 (MID)
            UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
            if (!DynamicMaterial)
            {
                continue;
            }

            DynamicMaterial->SetVectorParameterValue(LinkColorParameterName, LinkGlowColor);
            DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), LinkGlowColor);
            DynamicMaterial->SetScalarParameterValue(LinkGlowScalarParameterName, LinkGlowIntensity);
            DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveIntensity"), LinkGlowIntensity);
            DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), LinkGlowIntensity);
            DynamicMaterial->SetScalarParameterValue(LinkPulseScalarParameterName, bAnimateLinkPulse ? LinkPulseMax : 1.f);
            DynamicMaterial->SetScalarParameterValue(TEXT("EmissivePulse"), bAnimateLinkPulse ? LinkPulseMax : 1.f);

            LinkComponent->SetMaterial(MaterialIndex, DynamicMaterial);
            ActiveLinkMaterialInstances.Add(DynamicMaterial);
        }

        // 保存组件引用
        ActiveLinkComponents.Add(LinkComponent);
    }
}

// 清除现有的连接线网格
void APickupActorAAAJesusCross::ClearLinkMeshes()
{
    for (USplineMeshComponent* LinkComponent : ActiveLinkComponents)
    {
        if (!IsValid(LinkComponent))
        {
            continue;
        }
        LinkComponent->DestroyComponent();
    }
    ActiveLinkComponents.Reset();
    ActiveLinkMaterialInstances.Reset();
}

// 更新连接线的脉冲视觉效果 (Tick 中调用)
void APickupActorAAAJesusCross::UpdateLinkMeshPulseVisuals(float DeltaTime)
{
    // 如果没有材质实例，重置计时器并返回
    if (ActiveLinkMaterialInstances.IsEmpty())
    {
        LinkPulseTimeAccumulator = 0.f;
        return;
    }

    // 如果不运行动画，则设置固定状态
    if (!bAnimateLinkPulse)
    {
        for (UMaterialInstanceDynamic* DynamicMaterial : ActiveLinkMaterialInstances)
        {
            if (!IsValid(DynamicMaterial))
            {
                continue;
            }

            DynamicMaterial->SetVectorParameterValue(LinkColorParameterName, LinkGlowColor);
            DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), LinkGlowColor);
            DynamicMaterial->SetScalarParameterValue(LinkGlowScalarParameterName, LinkGlowIntensity);
            DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveIntensity"), LinkGlowIntensity);
            DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), LinkGlowIntensity);
            DynamicMaterial->SetScalarParameterValue(LinkPulseScalarParameterName, 1.f);
            DynamicMaterial->SetScalarParameterValue(TEXT("EmissivePulse"), 1.f);
        }
        return;
    }

    // --- 脉冲动画计算 ---
    LinkPulseTimeAccumulator += DeltaTime * FMath::Max(LinkPulseSpeed, 0.f);

    // 使用 Sin 函数生成 0 到 1 的循环 Alpha 值
    const float PulseAlpha = (FMath::Sin(LinkPulseTimeAccumulator) + 1.f) * 0.5f;
    // 根据 Alpha 在 Min 和 Max 之间插值
    const float PulseValue = FMath::Lerp(LinkPulseMin, LinkPulseMax, PulseAlpha);
    // 计算最终发光强度
    const float EffectiveGlowIntensity = LinkGlowIntensity * FMath::Max(PulseValue, 0.f);

    // 应用到所有材质实例
    for (UMaterialInstanceDynamic* DynamicMaterial : ActiveLinkMaterialInstances)
    {
        if (!IsValid(DynamicMaterial))
        {
            continue;
        }

        DynamicMaterial->SetVectorParameterValue(LinkColorParameterName, LinkGlowColor);
        DynamicMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), LinkGlowColor);
        DynamicMaterial->SetScalarParameterValue(LinkGlowScalarParameterName, EffectiveGlowIntensity);
        DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveIntensity"), EffectiveGlowIntensity);
        DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), EffectiveGlowIntensity);
        DynamicMaterial->SetScalarParameterValue(LinkPulseScalarParameterName, PulseValue);
        DynamicMaterial->SetScalarParameterValue(TEXT("EmissivePulse"), PulseValue);
    }
}

void APickupActorAAAJesusCross::HandleCrossMeshHit(
    UPrimitiveComponent* HitComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    FVector NormalImpulse,
    const FHitResult& Hit)
{
    if (!bApplyImpulseWhenRevealedGhostHits || !HasAuthority() || !MeshComponent || IsHeldByPlayer())
    {
        return;
    }

    if (!IsValid(HitComponent) || HitComponent != MeshComponent || !MeshComponent->IsSimulatingPhysics())
    {
        return;
    }

    APawn* OtherPawn = Cast<APawn>(OtherActor);
    if (!IsValid(OtherPawn))
    {
        return;
    }

    AMyAIController* AIController = Cast<AMyAIController>(OtherPawn->GetController());
    if (!IsValid(AIController) || !AIController->IsGhostRevealedByEffect())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();
    if ((CurrentTime - LastRevealedGhostImpulseTime) < FMath::Max(RevealedGhostImpulseCooldown, 0.f))
    {
        return;
    }
    ApplyRevealedGhostImpulseFromPawn(OtherPawn, CurrentTime);
}

void APickupActorAAAJesusCross::ApplyRevealedGhostImpulseFromPawn(APawn* OtherPawn, float CurrentTime)
{
    if (!IsValid(OtherPawn) || !MeshComponent)
    {
        return;
    }

    FVector PushDirection = GetActorLocation() - OtherPawn->GetActorLocation();
    PushDirection.Z = 0.f;
    PushDirection = PushDirection.GetSafeNormal();
    if (PushDirection.IsNearlyZero())
    {
        PushDirection = GetActorForwardVector().GetSafeNormal2D();
    }

    const FBoxSphereBounds MeshBounds = MeshComponent->Bounds;
    const FVector BoundsOrigin = MeshBounds.Origin;
    const FVector BoundsExtent = MeshBounds.BoxExtent;
    const FVector ImpulseApplicationLocation = BoundsOrigin  + (FVector::UpVector * BoundsExtent.Z * FMath::Clamp(RevealedGhostImpulseHeightRatio, 0.f, 1.5f));

    const FVector Impulse = (PushDirection * RevealedGhostPushImpulse) + (FVector::UpVector * RevealedGhostUpwardImpulse);
    
    const FVector AngularImpulseAxis = FVector::CrossProduct(FVector::UpVector, PushDirection).GetSafeNormal();
    const FVector AngularImpulse = AngularImpulseAxis * RevealedGhostAngularImpulseDegrees;


    MeshComponent->WakeAllRigidBodies();
    MeshComponent->AddImpulseAtLocation(Impulse, ImpulseApplicationLocation, NAME_None);

    if (!AngularImpulse.IsNearlyZero())
    {
        MeshComponent->AddAngularImpulseInDegrees(AngularImpulse, NAME_None, false);
    }
    LastRevealedGhostImpulseTime = CurrentTime;
}

void APickupActorAAAJesusCross::TryApplyRevealedGhostProximityImpulse()
{
    if (!bApplyImpulseWhenRevealedGhostHits || !MeshComponent || IsHeldByPlayer() || !MeshComponent->IsSimulatingPhysics())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();
    if ((CurrentTime - LastRevealedGhostImpulseTime) < FMath::Max(RevealedGhostImpulseCooldown, 0.f))
    {
        return;
    }

    const FBoxSphereBounds MeshBounds = MeshComponent->Bounds;
    const float CrossRadius = FMath::Max(MeshBounds.SphereRadius, 8.f);
    const FVector CrossLocation = GetActorLocation();

    for (TActorIterator<APawn> It(World); It; ++It)
    {
        APawn* OtherPawn = *It;
        if (!IsValid(OtherPawn))
        {
            continue;
        }

        AMyAIController* AIController = Cast<AMyAIController>(OtherPawn->GetController());
        if (!IsValid(AIController) || !AIController->IsGhostRevealedByEffect())
        {
            continue;
        }

        const float PawnRadius = GetCrossPawnCollisionRadius(OtherPawn);
        const float MaxDistance = CrossRadius + PawnRadius + FMath::Max(RevealedGhostProximityPadding, 0.f);
        if (FVector::DistSquared2D(CrossLocation, OtherPawn->GetActorLocation()) > FMath::Square(MaxDistance))
        {
            continue;
        }

        // FVector PushDirection = CrossLocation - OtherPawn->GetActorLocation();
        // PushDirection.Z = 0.f;
        // PushDirection = PushDirection.GetSafeNormal();
        // if (PushDirection.IsNearlyZero())
        // {
        //     PushDirection = GetActorForwardVector().GetSafeNormal2D();
        // }

        // const FVector Impulse = (PushDirection * RevealedGhostPushImpulse) + (FVector::UpVector * RevealedGhostUpwardImpulse);
        // MeshComponent->WakeAllRigidBodies();
        // MeshComponent->AddImpulse(Impulse, NAME_None, false);
        // LastRevealedGhostImpulseTime = CurrentTime;
        ApplyRevealedGhostImpulseFromPawn(OtherPawn, CurrentTime);
        break;
    }
}