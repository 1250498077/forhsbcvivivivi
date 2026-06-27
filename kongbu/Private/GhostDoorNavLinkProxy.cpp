#include "GhostDoorNavLinkProxy.h"

#include "ConfigurableDoorActor.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "GhostCharacter.h"
#include "MyAIController.h"
#include "NavLinkCustomComponent.h"
#include "TimerManager.h"

AGhostDoorNavLinkProxy::AGhostDoorNavLinkProxy()
{
	PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;
	bSmartLinkIsRelevant = true;
    PointLinks.Reset();

    LinkStartComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("LinkStartComponent"));
    LinkStartComponent->SetupAttachment(RootComponent);
    LinkStartComponent->SetRelativeLocation(FVector(-100.f, 0.f, 0.f));
    LinkStartComponent->ArrowColor = FColor::Green;
    LinkStartComponent->ArrowSize = 1.2f;

    LinkEndComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("LinkEndComponent"));
    LinkEndComponent->SetupAttachment(RootComponent);
    LinkEndComponent->SetRelativeLocation(FVector(100.f, 0.f, 0.f));
    LinkEndComponent->ArrowColor = FColor::Blue;
    LinkEndComponent->ArrowSize = 1.2f;
	
	// ProximityOpenBoxComponent 初始化与配置
    ProximityOpenBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("ProximityOpenBoxComponent"));
    ProximityOpenBoxComponent->SetupAttachment(RootComponent);
    ProximityOpenBoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ProximityOpenBoxComponent->SetCollisionObjectType(ECC_WorldDynamic);
    ProximityOpenBoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    ProximityOpenBoxComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    ProximityOpenBoxComponent->SetGenerateOverlapEvents(true);
    ProximityOpenBoxComponent->SetHiddenInGame(false);
    ProximityOpenBoxComponent->ShapeColor = FColor::Yellow;

    SyncSmartLinkFromComponents();
}

void AGhostDoorNavLinkProxy::OnConstruction(const FTransform &Transform)
{
    Super::OnConstruction(Transform);
    SyncSmartLinkFromComponents();
}

void AGhostDoorNavLinkProxy::BeginPlay()
{
	Super::BeginPlay();

    SyncSmartLinkFromComponents();
	OnSmartLinkReached.AddDynamic(this, &AGhostDoorNavLinkProxy::HandleSmartLinkReached);
	SetSmartLinkEnabled(true);

	if (ProximityOpenBoxComponent)
    {
        ProximityOpenBoxComponent->OnComponentBeginOverlap.AddDynamic(this, &AGhostDoorNavLinkProxy::HandleProximityOpenBoxBeginOverlap);
        ProximityOpenBoxComponent->SetCollisionEnabled(bEnableProximityOpenFallback ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    }


    if (bLogDoorLinkDebug)
    {
        UE_LOG(LogTemp, Log, TEXT("%s smart door link ready. Door=%s Start=%s End=%s ProximityFallback=%d"),
        *GetName(),
        *GetNameSafe(LinkedDoor.Get()),
        LinkStartComponent ? *LinkStartComponent->GetComponentLocation().ToCompactString() : TEXT("None"),
        LinkEndComponent ? *LinkEndComponent->GetComponentLocation().ToCompactString() : TEXT("None"),
        bEnableProximityOpenFallback);
    }
}

void AGhostDoorNavLinkProxy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ScanProximityOpenFallback();
}

void AGhostDoorNavLinkProxy::HandleSmartLinkReached(AActor *MovingActor, const FVector &DestinationPoint)
{
	(void)DestinationPoint;

	if (!TryOpenLinkedDoorForActor(MovingActor, TEXT("smart link")))
    {
        ResumeAgentPathFollowing(MovingActor);
        return;
    }

    if (ResumeDelay <= 0.f || !GetWorld())
    {
        ResumeAgentPathFollowing(MovingActor);
        return;
    }

    FTimerDelegate ResumeDelegate;
    ResumeDelegate.BindUObject(this, &AGhostDoorNavLinkProxy::ResumeAgentPathFollowing, MovingActor);

    FTimerHandle ResumeTimerHandle;
    GetWorldTimerManager().SetTimer(
        ResumeTimerHandle,
        ResumeDelegate,
        ResumeDelay,
        false);
}

bool AGhostDoorNavLinkProxy::TryOpenLinkedDoorForActor(AActor *MovingActor, const TCHAR *Reason)
{

	AGhostCharacter *Ghost = Cast<AGhostCharacter>(MovingActor);
	if (!IsValid(Ghost))
	{
        if (bLogDoorLinkDebug)
        {
            UE_LOG(LogTemp, Verbose, TEXT("%s %s reached by non-ghost %s"), *GetName(), Reason, *GetNameSafe(MovingActor));
        }
		return false;
	}

	AMyAIController *AIController = Cast<AMyAIController>(Ghost->GetController());
	if (!IsValid(AIController) || !CanControllerOpenDoor(AIController))
	{
        if (bLogDoorLinkDebug)
        {
            UE_LOG(LogTemp, Verbose, TEXT("%s %s reached by ghost %s but state is not allowed"), *GetName(), Reason, *GetNameSafe(Ghost));
        }
		return false;
	}

	AConfigurableDoorActor *Door = LinkedDoor.Get();
	if (!IsValid(Door) || Door->IsDoorLockedClosed())
	{
        if (bLogDoorLinkDebug)
        {
            UE_LOG(LogTemp, Verbose, TEXT("%s cannot open linked door %s locked=%d"),
                   *GetName(),
                   *GetNameSafe(Door),
                   IsValid(Door) ? Door->IsDoorLockedClosed() : false);
        }
		return false;
	}

	if (!Door->IsDoorOpen())
	{
        if (bLogDoorLinkDebug)
        {
            UE_LOG(LogTemp, Log, TEXT("%s opening door %s for ghost %s by %s"), *GetName(), *GetNameSafe(Door), *GetNameSafe(Ghost), Reason);
        }
		if (bPlayGhostOpenDoorAnimation)
		{
			Ghost->PlayGhostOpenDoorAnimation();
		}

		Door->OpenDoor();
	}

	return true;
}

void AGhostDoorNavLinkProxy::ResumeAgentPathFollowing(AActor *MovingActor)
{
	if (!IsValid(MovingActor))
	{
		return;
	}

	ResumePathFollowing(MovingActor);
}

void AGhostDoorNavLinkProxy::SyncSmartLinkFromComponents()
{
    PointLinks.Reset();

    if (!LinkStartComponent || !LinkEndComponent)
    {
        return;
    }

    // 用父类公开方法获取，而不是直接访问 SmartLinkComp
    UNavLinkCustomComponent* SmartLink = GetSmartLinkComp();
    if (!SmartLink)
    {
        return;
    }

    SmartLink->SetLinkData(
        LinkStartComponent->GetRelativeLocation(),
        LinkEndComponent->GetRelativeLocation(),
        ENavLinkDirection::BothWays);

	if (ProximityOpenBoxComponent)
    {
        const FVector StartLocation = LinkStartComponent->GetRelativeLocation();
        const FVector EndLocation = LinkEndComponent->GetRelativeLocation();
        const FVector Delta = EndLocation - StartLocation;
        const FVector BoxCenter = StartLocation + Delta * 0.5f;
        const FVector BoxExtent(
            FMath::Abs(Delta.X) * 0.5f + ProximityOpenPadding,
            FMath::Abs(Delta.Y) * 0.5f + ProximityOpenPadding,
            FMath::Max(FMath::Abs(Delta.Z) * 0.5f + ProximityOpenPadding, ProximityOpenHalfHeight));

        ProximityOpenBoxComponent->SetRelativeLocation(BoxCenter);
        ProximityOpenBoxComponent->SetBoxExtent(BoxExtent.ComponentMax(FVector(20.f, 20.f, 20.f)));
        ProximityOpenBoxComponent->SetCollisionEnabled(bEnableProximityOpenFallback ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    }
}

void AGhostDoorNavLinkProxy::HandleProximityOpenBoxBeginOverlap(UPrimitiveComponent *OverlappedComponent, AActor *OtherActor,
                                                                UPrimitiveComponent *OtherComp, int32 OtherBodyIndex,
                                                                bool bFromSweep, const FHitResult &SweepResult)
{
    (void)OverlappedComponent;
    (void)OtherComp;
    (void)OtherBodyIndex;
    (void)bFromSweep;
    (void)SweepResult;

    if (!bEnableProximityOpenFallback)
    {
        return;
    }

    TryOpenLinkedDoorForActor(OtherActor, TEXT("proximity fallback"));
}

void AGhostDoorNavLinkProxy::ScanProximityOpenFallback()
{
    if (!bEnableProximityOpenFallback || !GetWorld() || !ProximityOpenBoxComponent)
    {
        return;
    }

    AConfigurableDoorActor *Door = LinkedDoor.Get();
    if (!IsValid(Door) || Door->IsDoorOpen() || Door->IsDoorLockedClosed())
    {
        return;
    }

    for (TActorIterator<AGhostCharacter> It(GetWorld()); It; ++It)
    {
        AGhostCharacter *Ghost = *It;
        if (IsValid(Ghost) && IsActorInsideProximityOpenBox(Ghost))
        {
            TryOpenLinkedDoorForActor(Ghost, TEXT("proximity scan"));
            return;
        }
    }
}

bool AGhostDoorNavLinkProxy::IsActorInsideProximityOpenBox(const AActor *Actor) const
{
    if (!IsValid(Actor) || !ProximityOpenBoxComponent)
    {
        return false;
    }

    const FVector LocalLocation = ProximityOpenBoxComponent->GetComponentTransform().InverseTransformPosition(Actor->GetActorLocation());
    const FVector BoxExtent = ProximityOpenBoxComponent->GetUnscaledBoxExtent();

    return FMath::Abs(LocalLocation.X) <= BoxExtent.X
           && FMath::Abs(LocalLocation.Y) <= BoxExtent.Y
           && FMath::Abs(LocalLocation.Z) <= BoxExtent.Z;
}

bool AGhostDoorNavLinkProxy::CanControllerOpenDoor(const AMyAIController *AIController) const
{
	if (!IsValid(AIController))
	{
		return false;
	}
    
	switch (AIController->GetCurrentAIState())
	{
	case EEnemyAIState::Chase:
		return bAllowChase;
	case EEnemyAIState::Investigate:
		return bAllowInvestigate;
	case EEnemyAIState::Rage:
		return bAllowRage;
	default:
		return false;
	}
}
