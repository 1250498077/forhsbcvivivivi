// GhostCharacter.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GhostCharacter.generated.h"

class UCapsuleComponent;
class UPrimitiveComponent;
class AMyAIController;

// 鬼 AI 专用角色基类。
// 蓝图鬼角色建议直接继承这个类，然后在蓝图组件面板里调整 GhostAttachZoneComponent 的位置、半径和半高。
UCLASS()
class KONGBU_API AGhostCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AGhostCharacter();

    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    // 统一的道具吸附区域。慢符 / 符文器触发这个区域后，会挂到这个组件上。
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ghost|Attach")
    TObjectPtr<UCapsuleComponent> GhostAttachZoneComponent;

    // 调试用: 开启后可在蓝图/PIE 中实时看到鬼的道具吸附触碰区域。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Attach|Debug")
    bool bShowGhostAttachZoneDebug = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost|Attach|Debug", meta = (EditCondition = "bShowGhostAttachZoneDebug"))
    FColor GhostAttachZoneDebugColor = FColor::Cyan;

    UFUNCTION(BlueprintPure, Category = "Ghost|Attach")
    UCapsuleComponent* GetGhostAttachZoneComponent() const;

    UFUNCTION(BlueprintPure, Category = "Ghost|Attach")
    bool IsWorldLocationInsideGhostAttachZone(const FVector& WorldLocation, float Tolerance = 12.f) const;

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandleGhostAttachZoneBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

private:
    void ConfigureGhostAttachZoneCollision() const;
    void UpdateGhostAttachZoneDebugVisibility() const;

    AMyAIController* ResolveMyAIController() const;
};