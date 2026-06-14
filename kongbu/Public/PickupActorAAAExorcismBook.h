// =====================================================================
// 驱魔书籍
// =====================================================================
//
// 功能：
//   玩家手持书籍后，按右键翻页查看不同"符咒 ↔ 鬼"的对应关系。
//   书的 Mesh 有 3 个材质插槽：
//     slot 0 = 符咒纹理材质
//     slot 1 = 书本框架材质（不变）
//     slot 2 = 鬼图材质
//   翻到最后一页再按右键回到第一页。
//
// 操作方式（与其他道具完全一致）：
//   E     = 捡起 / 放下
//   左键  = 投掷
//   右键  = 翻页
//  
// 技术细节：
//   1. 书籍数据（符咒纹理和鬼图纹理）通过 DataTable 维护，设计师可在编辑器里方便地增删改。DataTable 的行结构由 FExorcismBookPage 定义，包含符咒纹理和鬼图纹理两个字段。
//   2. 书籍 Actor 在 BeginPlay 时加载 DataTable 中的所有页数据并缓存纹理引用。翻页时直接从缓存里取纹理设置到材质上，性能较好。
//   3. 书籍 Actor 通过实现 CanBeClosedByPlayer / CloseByPlayer / IsClosedByPlayer / OpenByPlayer 接口来接收右键事件，从而触发翻页逻辑。
//   4. 书籍的符咒材质和鬼图材质需要在编辑器里指定，并且材质里要有对应的 TextureParameter（默认参数名分别是 "BaseTexture"）以供动态替换纹理。
// =====================================================================

#pragma once

#include "CoreMinimal.h"
#include "PickupActor.h"
#include "PickupActorAAAExorcismBook.generated.h"

class UMaterialInstanceDynamic;
class UTexture2D;

UCLASS()
class KONGBU_API APickupActorAAAExorcismBook : public APickupActor
{
    GENERATED_BODY()

public:
    APickupActorAAAExorcismBook();

    virtual void BeginPlay() override;

    // 翻到下一页（最后一页循环回第一页）。由 PlayerController 右键调用。
    UFUNCTION(BlueprintCallable, Category = "ExorcismBook")
    void FlipToNextPage();

    // 当前正在显示第几页（0-based）
    UFUNCTION(BlueprintPure, Category = "ExorcismBook")
    int32 GetCurrentPageIndex() const { return CurrentPageIndex; }

    // 总页数
    UFUNCTION(BlueprintPure, Category = "ExorcismBook")
    int32 GetTotalPageCount() const { return TotalPageCount; }

    // 右键翻页的开关
    virtual bool CanBeClosedByPlayer_Implementation() const override;
    virtual void CloseByPlayer_Implementation(AActor* ClosingActor) override;
    virtual bool IsClosedByPlayer_Implementation() const override;
    virtual void OpenByPlayer_Implementation(AActor* OpeningActor) override;

protected:
    // 符咒材质插槽索引（默认 0）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExorcismBook")
    int32 RuneMaterialSlotIndex = 0;

    // 鬼图材质插槽索引（默认 2）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExorcismBook")
    int32 GhostMaterialSlotIndex = 2;

    // 符咒材质中用于替换纹理的参数名（需要材质里有这个 TextureParameter）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExorcismBook")
    FName RuneTextureParameterName = TEXT("BaseTexture");

    // 鬼图材质中用于替换纹理的参数名
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExorcismBook")
    FName GhostTextureParameterName = TEXT("BaseTexture");

private:
    void LoadBookData();
    void ApplyPageVisuals();

    UPROPERTY(Transient)
    UMaterialInstanceDynamic* RuneDynamicMaterial = nullptr;

    UPROPERTY(Transient)
    UMaterialInstanceDynamic* GhostDynamicMaterial = nullptr;

    int32 CurrentPageIndex = 0;
    int32 TotalPageCount = 0;

    // 每页缓存的纹理引用
    UPROPERTY(Transient)
    TArray<UTexture2D*> CachedRuneTextures;

    UPROPERTY(Transient)
    TArray<UTexture2D*> CachedGhostTextures;
};
