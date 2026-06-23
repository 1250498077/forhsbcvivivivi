#include "GhostThrowablePropComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "PickupActor.h"

UGhostThrowablePropComponent::UGhostThrowablePropComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UGhostThrowablePropComponent::CanBeUsedByGhost() const
{
    const AActor* OwnerActor = GetOwner();
    if (!bEnabled || bGhostTelekinesisActive || !IsValid(OwnerActor))
    {
        return false;
    }

    if (const APickupActor* PickupActor = Cast<APickupActor>(OwnerActor))
    {
        if (PickupActor->IsHeldByPlayer())
        {
            return false;
        }
    }

    return IsValid(ResolveThrowablePrimitive());
}

UPrimitiveComponent* UGhostThrowablePropComponent::ResolveThrowablePrimitive() const
{
    AActor* OwnerActor = GetOwner();
    if (!IsValid(OwnerActor))
    {
        return nullptr;
    }

    TArray<UPrimitiveComponent*> PrimitiveComponents;
    OwnerActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

    // 如果指定了名字，则优先查找该名字的组件
    if (!ThrowablePrimitiveComponentName.IsNone())
    {
        for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
        {
            if (IsValid(PrimitiveComponent)
                && PrimitiveComponent->GetFName() == ThrowablePrimitiveComponentName
                && IsPrimitiveUsable(PrimitiveComponent))
            {
                return PrimitiveComponent;
            }
        }
        return nullptr;
    }

    // 没指定名字，检查根组件是否可用
    if (UPrimitiveComponent* RootPrimitiveComponent = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent()))
    {
        if (IsPrimitiveUsable(RootPrimitiveComponent))
        {
            return RootPrimitiveComponent;
        }
    }

    // 最后找第一个可用的Primitive组件
    for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
    {
        if (IsPrimitiveUsable(PrimitiveComponent))
        {
            return PrimitiveComponent;
        }
    }

    return nullptr;
}

FVector UGhostThrowablePropComponent::GetThrowableLocation() const
{
    if (IsValid(ActivePrimitiveComponent))
    {
        return ActivePrimitiveComponent->GetComponentLocation();
    }
    if (UPrimitiveComponent* PrimitiveComponent = ResolveThrowablePrimitive())
    {
        return PrimitiveComponent->GetComponentLocation();
    }
    return IsValid(GetOwner()) ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

bool UGhostThrowablePropComponent::BeginGhostTelekinesis(AActor* controllingGhost)
{
    if (!CanBeUsedByGhost())
    {
        return false;
    }

    UPrimitiveComponent* PrimitiveComponent = ResolveThrowablePrimitive();
    AActor* OwnerActor = GetOwner();
    if (!IsValid(PrimitiveComponent) || !IsValid(OwnerActor))
    {
        return false;
    }

    ActivePrimitiveComponent = PrimitiveComponent;
    bGhostTelekinesisActive = true;
    
    // 缓存当前物理状态
    bCachedSimulatePhysics = PrimitiveComponent->IsSimulatingPhysics();
    bCachedEnableGravity = PrimitiveComponent->IsGravityEnabled();
    bCachedGenerateOverlapEvents = PrimitiveComponent->GetGenerateOverlapEvents();
    CachedCollisionEnabled = PrimitiveComponent->GetCollisionEnabled();

    // 解绑并切换控制权
    OwnerActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    OwnerActor->SetOwner(controllingGhost);
    OwnerActor->SetInstigator(Cast<APawn>(controllingGhost));

    // 禁用物理模拟并重置运动
    PrimitiveComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
    PrimitiveComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    PrimitiveComponent->SetSimulatePhysics(false);
    PrimitiveComponent->SetEnableGravity(false);
    PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PrimitiveComponent->SetGenerateOverlapEvents(false);
    
    return true;
}

void UGhostThrowablePropComponent::UpdateGhostTelekinesisTransform(FVector NewLocation, FRotator NewRotation)
{
    if (!bGhostTelekinesisActive || !IsValid(GetOwner()))
    {
        return;
    }

    // 传送物体位置（由于已禁用物理，直接移动Actor）
    GetOwner()->SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

void UGhostThrowablePropComponent::LaunchFromGhostTelekinesis(FVector ThrowDirection, float ThrowForce, APawn* ThrowInstigator)
{
    if (!bGhostTelekinesisActive || !IsValid(ActivePrimitiveComponent))
    {
        return;
    }

    if (AActor* OwnerActor = GetOwner())
    {
        OwnerActor->SetInstigator(ThrowInstigator);
        OwnerActor->SetOwner(ThrowInstigator);
    }

    UPrimitiveComponent* PrimitiveComponent = ActivePrimitiveComponent;
    
    // 恢复物理属性
    RestorePrimitiveAfterTelekinesis(true);
    
    // 施加投掷冲量
    PrimitiveComponent->AddImpulse(ThrowDirection.GetSafeNormal() * ThrowForce * ThrowForceMultiplier, NAME_None, false);
}

void UGhostThrowablePropComponent::CancelGhostTelekinesis()
{
    if (!bGhostTelekinesisActive)
    {
        return;
    }

    // 取消念力时恢复物理，并唤醒刚体
    RestorePrimitiveAfterTelekinesis(true);
}

bool UGhostThrowablePropComponent::IsPrimitiveUsable(UPrimitiveComponent* PrimitiveComponent) const
{
    if (!IsValid(PrimitiveComponent))
    {
        return false;
    }

    // 如果开启了必须模拟物理的检查，则组件必须正处于物理模拟状态
    return !bRequireSimulatingPhysics || PrimitiveComponent->IsSimulatingPhysics();
}

void UGhostThrowablePropComponent::RestorePrimitiveAfterTelekinesis(bool bWakeRigidBody)
{
    UPrimitiveComponent* PrimitiveComponent = ActivePrimitiveComponent;
    ActivePrimitiveComponent = nullptr;
    bGhostTelekinesisActive = false;

    if (!IsValid(PrimitiveComponent))
    {
        return;
    }

    // 还原之前的物理、碰撞、重力设置
    PrimitiveComponent->SetCollisionEnabled(CachedCollisionEnabled);
    PrimitiveComponent->SetGenerateOverlapEvents(bCachedGenerateOverlapEvents);
    PrimitiveComponent->SetSimulatePhysics(bCachedSimulatePhysics);
    PrimitiveComponent->SetEnableGravity(bCachedEnableGravity);
    
    if (bWakeRigidBody && bCachedSimulatePhysics)
    {
        PrimitiveComponent->WakeAllRigidBodies();
    }
}