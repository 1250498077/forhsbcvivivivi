// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "TeleportTrigger.generated.h"

UCLASS()
class KONGBU_API ATeleportTrigger : public AActor
{
    GENERATED_BODY()

public:
    ATeleportTrigger();

    // 重叠检测盒，玩家进入后触发传送。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* TriggerBox;

    // 传送目标标签。玩家进入触发盒后，会把角色传送到关卡里带这个 Tag 的目标点。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport")
    FName TeleportPointTag;

protected:

    // 在关卡中查找带指定 Tag 的传送目标。
    AActor* FindTeleportPoint(FName PointName);

    // 玩家进入触发盒时执行传送。
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);
};