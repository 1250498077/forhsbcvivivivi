#include "PickupActorAAAExorcismBook.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "ExorcismSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"

APickupActorAAAExorcismBook::APickupActorAAAExorcismBook()
{
    PrimaryActorTick.bCanEverTick = false;

    HoldType = EHoldItemType::Book;
    FP_SocketName = TEXT("RightSocket");
    FP_LocationOffset = FVector::ZeroVector;
    FP_RotationOffset = FRotator::ZeroRotator;
    TP_SocketName = TEXT("RightSocket");
    TP_LocationOffset = FVector::ZeroVector;
    TP_RotationOffset = FRotator::ZeroRotator;

    ItemMassKg = 0.8f;
    ItemThrowForceMultiplier = 0.55f;
    ItemLinearDamping = 0.12f;
    ItemAngularDamping = 0.55f;
    ItemThrowSpinRateDegrees = 450.f;

    Tags.Add(FName("ExorcismBook"));
    Tags.Add(FName("Pickup"));

    bAllowPawnCollision = false;
    bAllowPhysicsBodyCollision = false;
    ApplyReleasedCollisionProfile();
}

void APickupActorAAAExorcismBook::BeginPlay()
{
    Super::BeginPlay();
    LoadBookData();
}

void APickupActorAAAExorcismBook::LoadBookData()
{
    CachedRuneTextures.Reset();
    CachedGhostTextures.Reset();
    CurrentPageIndex = 0;
    TotalPageCount = 0;

    const UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const UGameInstance* GI = World->GetGameInstance();
    if (!GI)
    {
        return;
    }

    const UExorcismSubsystem* Subsystem = GI->GetSubsystem<UExorcismSubsystem>();
    if (!Subsystem || !Subsystem->HasMappings())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: ExorcismSubsystem has no mappings yet"), *GetName());
        return;
    }

    TArray<FExorcismBookSlot> Slots = Subsystem->GetBookSlots();
    // 打印每个槽位的详细映射（方便调试：符咒纹理路径、鬼类型与书中鬼图）
    UE_LOG(LogTemp, Log, TEXT("%s: BookSlots count=%d"), *GetName(), Slots.Num());
    for (const FExorcismBookSlot& Slot : Slots)
    {
        FString NodeSeqStr;
        for (int32 Idx = 0; Idx < Slot.Rune.NodeSequence.Num(); ++Idx)
        {
            NodeSeqStr += FString::Printf(TEXT("%d%s"), Slot.Rune.NodeSequence[Idx], (Idx + 1 < Slot.Rune.NodeSequence.Num()) ? TEXT(",") : TEXT(""));
        }

        UE_LOG(LogTemp, Log,
            TEXT("Slot %d: GhostType=%d GhostMesh=%s BookGhostImage=%s RunePattern=%s RuneTexture=%s Nodes=[%s]"),
            Slot.SlotIndex,
            Slot.Ghost.GhostTypeIndex,
            *Slot.Ghost.GhostMeshPath.ToString(),
            *Slot.Ghost.BookGhostImagePath.ToString(),
            *Slot.Rune.PatternId.ToString(),
            *Slot.Rune.TexturePath.ToString(),
            *NodeSeqStr);
    }

    TotalPageCount = Slots.Num();

    if (TotalPageCount == 0)
    {
        return;
    }

    CachedRuneTextures.SetNum(TotalPageCount);
    CachedGhostTextures.SetNum(TotalPageCount);

    for (int32 i = 0; i < TotalPageCount; ++i)
    {
        // 加载符咒纹理
        if (Slots[i].Rune.TexturePath.IsValid())
        {
            CachedRuneTextures[i] = Cast<UTexture2D>(Slots[i].Rune.TexturePath.TryLoad());
        }

        // 加载鬼图纹理
        if (Slots[i].Ghost.BookGhostImagePath.IsValid())
        {
            CachedGhostTextures[i] = Cast<UTexture2D>(Slots[i].Ghost.BookGhostImagePath.TryLoad());
        }
    }

    // 创建动态材质实例
    if (MeshComponent)
    {
        const int32 NumMats = MeshComponent->GetNumMaterials();
        const int32 RequiredSlot = FMath::Max(RuneMaterialSlotIndex, GhostMaterialSlotIndex);
        UE_LOG(LogTemp, Warning, TEXT("%s: MeshComponent has %d materials, RuneSlot=%d GhostSlot=%d RequiredMax=%d"),
            *GetName(), NumMats, RuneMaterialSlotIndex, GhostMaterialSlotIndex, RequiredSlot);

        if (NumMats > RequiredSlot)
        {
            UMaterialInterface* RuneBaseMaterial = MeshComponent->GetMaterial(RuneMaterialSlotIndex);
            if (RuneBaseMaterial)
            {
                RuneDynamicMaterial = UMaterialInstanceDynamic::Create(RuneBaseMaterial, this);
                MeshComponent->SetMaterial(RuneMaterialSlotIndex, RuneDynamicMaterial);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("%s: RuneBaseMaterial at slot %d is null!"), *GetName(), RuneMaterialSlotIndex);
            }

            UMaterialInterface* GhostBaseMaterial = MeshComponent->GetMaterial(GhostMaterialSlotIndex);
            if (GhostBaseMaterial)
            {
                GhostDynamicMaterial = UMaterialInstanceDynamic::Create(GhostBaseMaterial, this);
                MeshComponent->SetMaterial(GhostMaterialSlotIndex, GhostDynamicMaterial);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("%s: GhostBaseMaterial at slot %d is null!"), *GetName(), GhostMaterialSlotIndex);
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("%s: Mesh only has %d materials but needs slot %d! Dynamic materials NOT created."),
                *GetName(), NumMats, RequiredSlot);
        }
    }

    ApplyPageVisuals();

    UE_LOG(LogTemp, Log, TEXT("%s: loaded %d book pages"), *GetName(), TotalPageCount);
}

void APickupActorAAAExorcismBook::ApplyPageVisuals()
{
    if (TotalPageCount == 0)
    {
        return;
    }

    const int32 PageIndex = FMath::Clamp(CurrentPageIndex, 0, TotalPageCount - 1);

    if (RuneDynamicMaterial && CachedRuneTextures.IsValidIndex(PageIndex))
    {
        UTexture2D* RuneTexture = CachedRuneTextures[PageIndex];
        if (RuneTexture)
        {
            RuneDynamicMaterial->SetTextureParameterValue(RuneTextureParameterName, RuneTexture);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("%s: CachedRuneTexture[%d] is null!"), *GetName(), PageIndex);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("%s: RuneDynamicMaterial=%s CachedRuneTextures valid=%s for page %d"),
            *GetName(),
            RuneDynamicMaterial ? TEXT("OK") : TEXT("NULL"),
            CachedRuneTextures.IsValidIndex(PageIndex) ? TEXT("YES") : TEXT("NO"),
            PageIndex);
    }

    if (GhostDynamicMaterial && CachedGhostTextures.IsValidIndex(PageIndex))
    {
        UTexture2D* GhostTexture = CachedGhostTextures[PageIndex];
        if (GhostTexture)
        {
            GhostDynamicMaterial->SetTextureParameterValue(GhostTextureParameterName, GhostTexture);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("%s: CachedGhostTexture[%d] is null!"), *GetName(), PageIndex);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("%s: GhostDynamicMaterial=%s CachedGhostTextures valid=%s for page %d"),
            *GetName(),
            GhostDynamicMaterial ? TEXT("OK") : TEXT("NULL"),
            CachedGhostTextures.IsValidIndex(PageIndex) ? TEXT("YES") : TEXT("NO"),
            PageIndex);
    }

    UE_LOG(LogTemp, Log, TEXT("%s: showing page %d / %d"), *GetName(), PageIndex + 1, TotalPageCount);
}

void APickupActorAAAExorcismBook::FlipToNextPage()
{
    if (TotalPageCount == 0)
    {
        return;
    }

    CurrentPageIndex = (CurrentPageIndex + 1) % TotalPageCount;
    ApplyPageVisuals();
}

// 右键 = 翻页。利用 CanBeClosedByPlayer / CloseByPlayer 机制来接收右键事件。
// TryTogglePickupActor 会先调用 IsClosedByPlayer，
// 如果返回 true 就 OpenByPlayer，否则 CloseByPlayer。
// 我们让 IsClosedByPlayer 始终返回 false，这样右键永远走 CloseByPlayer = 翻页。

bool APickupActorAAAExorcismBook::CanBeClosedByPlayer_Implementation() const
{
    return true;
}

void APickupActorAAAExorcismBook::CloseByPlayer_Implementation(AActor* ClosingActor)
{
    FlipToNextPage();
}

bool APickupActorAAAExorcismBook::IsClosedByPlayer_Implementation() const
{
    return false;
}

void APickupActorAAAExorcismBook::OpenByPlayer_Implementation(AActor* OpeningActor)
{
    FlipToNextPage();
}
