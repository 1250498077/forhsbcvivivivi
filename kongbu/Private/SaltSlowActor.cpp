#include "SaltSlowActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "MyAIController.h"
#include "TimerManager.h"

ASaltSlowActor::ASaltSlowActor()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    RootComponent = MeshComponent;

    MeshComponent->SetMobility(EComponentMobility::Static);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetGenerateOverlapEvents(false);
    MeshComponent->SetCanEverAffectNavigation(false);

    SlowTriggerComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SlowTriggerComponent"));
    SlowTriggerComponent->SetupAttachment(MeshComponent);
    SlowTriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SlowTriggerComponent->SetCollisionObjectType(ECC_WorldDynamic);
    SlowTriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    SlowTriggerComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    SlowTriggerComponent->SetGenerateOverlapEvents(true);
    SlowTriggerComponent->SetCanEverAffectNavigation(false);

    SlowTriggerComponent->OnComponentBeginOverlap.AddDynamic(this, &ASaltSlowActor::HandleSlowTriggerBeginOverlap);
    SlowTriggerComponent->OnComponentEndOverlap.AddDynamic(this, &ASaltSlowActor::HandleSlowTriggerEndOverlap);

    Tags.Add(FName("Salt"));
    Tags.Add(FName("Slow"));
    Tags.Add(FName("Hazard"));

    UpdateSlowTriggerRadius();
}

void ASaltSlowActor::BeginPlay()
{
    Super::BeginPlay();

    UpdateSlowTriggerRadius();
    ApplySlowToCurrentOverlaps();

    // 关卡初始重叠时，Pawn 和 Controller 的初始化顺序可能比 overlap 事件更晚。
    // 下一帧再扫一次可确保开局就站在盐上的 AI 也能稳定吃到减速。
    GetWorldTimerManager().SetTimerForNextTick(this, &ASaltSlowActor::ApplySlowToCurrentOverlaps);
}

void ASaltSlowActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateSlowTriggerRadius();
}

void ASaltSlowActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (const TWeakObjectPtr<AMyAIController>& ControllerPtr : AffectedControllers)
    {
        if (AMyAIController* Controller = ControllerPtr.Get())
        {
            Controller->RemoveSlowSource(this);
        }
    }

    AffectedControllers.Reset();

    Super::EndPlay(EndPlayReason);
}

void ASaltSlowActor::HandleSlowTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (AMyAIController* Controller = ResolveAIControllerFromActor(OtherActor))
    {
        AffectedControllers.Add(Controller);
        Controller->ApplySlowSource(this, SlowMultiplier);
    }
}

void ASaltSlowActor::HandleSlowTriggerEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    if (AMyAIController* Controller = ResolveAIControllerFromActor(OtherActor))
    {
        AffectedControllers.Remove(Controller);
        Controller->RemoveSlowSource(this);
    }
}

void ASaltSlowActor::UpdateSlowTriggerRadius()
{
    if (SlowTriggerComponent)
    {
        SlowTriggerComponent->SetSphereRadius(FMath::Max(10.f, SlowTriggerRadius));
    }
}

void ASaltSlowActor::ApplySlowToCurrentOverlaps()
{
    if (!SlowTriggerComponent)
    {
        return;
    }

    TArray<AActor*> OverlappingActors;
    SlowTriggerComponent->GetOverlappingActors(OverlappingActors, APawn::StaticClass());

    for (AActor* OverlappingActor : OverlappingActors)
    {
        if (AMyAIController* Controller = ResolveAIControllerFromActor(OverlappingActor))
        {
            AffectedControllers.Add(Controller);
            Controller->ApplySlowSource(this, SlowMultiplier);
        }
    }
}

AMyAIController* ASaltSlowActor::ResolveAIControllerFromActor(AActor* OtherActor) const
{
    if (!IsValid(OtherActor) || OtherActor == this)
    {
        return nullptr;
    }

    if (const APawn* Pawn = Cast<APawn>(OtherActor))
    {
        return Cast<AMyAIController>(Pawn->GetController());
    }

    return Cast<AMyAIController>(OtherActor);
}