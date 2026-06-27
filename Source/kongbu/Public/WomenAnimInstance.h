// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "WomenAnimInstance.generated.h"

UCLASS()
class KONGBU_API UWomenAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
    // 每帧从角色读取移动和动作状态，写回动画蓝图变量。
    virtual void NativeUpdateAnimation(float DeltaTime) override;

    // 平面移动速度。常用于切换待机、走路、跑步混合动画。
    UPROPERTY(BlueprintReadOnly, Category = "State")
    float Speed = 0.f;

    // 是否处于空中状态。
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsInAir = false;

    // 是否下蹲。
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsSquat = false;

    // 是否冲刺。
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsSprint = false;

    // 是否正在下蹲投掷。
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsSquatThrowing = false;


    // 是否正在站立投掷。
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsStandThrowing = false;

    // 供瞄准偏移或上半身朝向使用的旋转参数。
    UPROPERTY(BlueprintReadWrite, Category = "Anim")
    float Pitch = 0.f;
    UPROPERTY(BlueprintReadWrite, Category = "Anim")
    float Roll = 0.f;
    UPROPERTY(BlueprintReadWrite, Category = "Anim")
    float Yaw = 0.f;

};
