#include "WomenAnimInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WomenCharacter.h"
#include "Engine/Engine.h"
#include "Components/SkeletalMeshComponent.h"

void UWomenAnimInstance::NativeUpdateAnimation(float DeltaTime)
{
    Super::NativeUpdateAnimation(DeltaTime);

    // 动画实例每帧都从当前 Pawn 拉取最新状态；拿不到 Pawn 时直接退出。
    ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
    if (!Character) return;

    // 只统计平面速度，忽略跳跃/下落带来的 Z 速度，避免空中时跑步值异常增大。
    Speed = Character->GetVelocity().Size2D();
    AWomenCharacter* WomenChar = Cast<AWomenCharacter>(Character);
    if (WomenChar)
    {
        bIsSquat = WomenChar->IsSquat;
        bIsSprint = WomenChar->IsSprint;
        bIsSquatThrowing = WomenChar->IsSquatThrowing;
        bIsStandThrowing = WomenChar->IsStandThrowing;
    }


    // 读取控制器朝向，把视角旋转同步给动画蓝图做瞄准偏移。
    AController* Controller = Character->GetController();
    if (Controller)
    {
        FRotator ControlRot = Controller->GetControlRotation();

        // 把 Pitch 规整到更适合动画使用的区间，并按当前蓝图需求取反。
        Pitch = ControlRot.Pitch;
        if (Pitch > 180.f) Pitch -= 360.f;
        Pitch = -Pitch;
    }


    // 空中状态直接读取 CharacterMovement，供跳跃/落地状态机使用。
    bIsInAir = Character->GetCharacterMovement()->IsFalling();

    if (GEngine)
    {
        // 这些屏幕调试信息适合在调动画状态同步时临时观察。
        GEngine->AddOnScreenDebugMessage(0, 0.f, FColor::Yellow, FString::Printf(TEXT("Speed: %.1f"), Speed));
        GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Cyan, FString::Printf(TEXT("bIsSquat: %s"), bIsSquat ? TEXT("true") : TEXT("false")));
        GEngine->AddOnScreenDebugMessage(2, 0.f, FColor::Green, FString::Printf(TEXT("bIsInAir: %s"), bIsInAir ? TEXT("true") : TEXT("false")));
        GEngine->AddOnScreenDebugMessage(3, 0.f, FColor::Orange, FString::Printf(TEXT("bIsSquatThrowing: %s"), bIsSquatThrowing ? TEXT("true") : TEXT("false")));
        GEngine->AddOnScreenDebugMessage(4, 0.f, FColor::Red, FString::Printf(TEXT("bIsStandThrowing: %s"), bIsStandThrowing ? TEXT("true") : TEXT("false")));

        GEngine->AddOnScreenDebugMessage(5, 0.f, FColor::Yellow, FString::Printf(TEXT("Pitch: %.1f"), Pitch));
        //GEngine->AddOnScreenDebugMessage(6, 0.f, FColor::Yellow, FString::Printf(TEXT("Roll: %.1f"), Roll));
        //GEngine->AddOnScreenDebugMessage(7, 0.f, FColor::Yellow, FString::Printf(TEXT("Yaw: %.1f"), Yaw));
    }
}