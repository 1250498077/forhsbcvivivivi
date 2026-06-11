// Fill out your copyright notice in the Description page of Project Settings.
#include "TeleportTrigger.h"

#include "WomenCharacter.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"



ATeleportTrigger::ATeleportTrigger()
{
    // 用 BoxComponent 做最简单稳定的关卡触发器。
    TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
    RootComponent = TriggerBox;

    TriggerBox->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
    TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATeleportTrigger::OnOverlapBegin);
}

AActor* ATeleportTrigger::FindTeleportPoint(FName PointName)
{
    // 当前做法是按 Tag 全场景扫描，适合少量传送点；后续多了可以改成直接引用。
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor->ActorHasTag(PointName))
        {
            return Actor;
        }
    }
    return nullptr;
}

void ATeleportTrigger::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    AWomenCharacter* Player = Cast<AWomenCharacter>(OtherActor);
    if (Player)
    {
        // 只允许玩家角色触发，把目标点的位置和朝向整体复制过去。
        AActor* TeleportPoint = FindTeleportPoint(TeleportPointTag);
        if (TeleportPoint)
        {
            Player->TeleportToTarget(TeleportPoint);
        }
    }
}