// ExorcismSubsystem.h
// =====================================================================
// 驱魔随机匹配子系统
// =====================================================================
//
// 功能概述：
//   游戏开始时自动扫描资源，将符咒纹理和鬼的外形随机配对。
//   玩家通过查看驱魔书籍来了解"哪个符咒对应哪个鬼"，然后画出正确的符咒。
//
// 资产命名约定：
//   兼容旧符咒纹理：book_fuzhou_tietu_12345
//   新符咒纹理：    book_easy_fuzhou_tietu_12345、book_medium_fuzhou_tietu_12345、
//                 book_hard_fuzhou_tietu_12345、book_nightmare_fuzhou_tietu_12345
//   节点解析规则：
//     - 纯数字（无下划线）按单个数字解析：12345 -> {1,2,3,4,5}
//     - 含下划线时：首段按单数字解析，后续段按整体整数解析：1341243_13_13 -> {1,3,4,1,2,4,3,13,13}
//   鬼网格体：  gui_mesh_1, gui_mesh_2 …  （后缀数字 = 鬼类型编号）
//   书本鬼图：  book_gui_1, book_gui_2 …  （编号与 gui_mesh 对应）
//
// 蓝图使用方法：
//   1. 在 GameMode 的 BeginPlay 里调用 GenerateRandomMappings()。
//   2. 驱魔书 UI 调用 GetBookSlots() 获取当前局的所有"符咒 ↔ 鬼"映射。
//   3. 符咒道具（RuneInstrument）在 BeginPlay 里自动加载映射，无需手动操作。
//   4. 鬼的 AI 控制器（MyAIController）Possess 时自动领取外形并替换网格体。
//   5. 符咒画对后，ReceiveRunePatternSolved 里的 PatternId 可通过
//      GetGhostTypeForPatternId() 查到它对应的鬼类型编号。
// =====================================================================

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ExorcismSubsystem.generated.h"

class UTexture2D;
class AGhostCharacter;
struct FRuneInstrumentPattern;

UENUM(BlueprintType)
enum class EExorcismRuneDifficulty : uint8
{
    Easy UMETA(DisplayName = "Easy"),
    Medium UMETA(DisplayName = "Medium"),
    Hard UMETA(DisplayName = "Hard"),
    Nightmare UMETA(DisplayName = "Nightmare")
};

// 一条扫描到的符咒纹理记录
USTRUCT(BlueprintType)
struct FExorcismRuneEntry
{
    GENERATED_BODY()

    // 纹理资产路径（软引用，按需加载）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exorcism")
    FSoftObjectPath TexturePath;

    // 从纹理名称后缀解析出的节点绘制顺序
    // 例如：
    //   book_fuzhou_tietu_1236748 -> {1,2,3,6,7,4,8}
    //   book_hard_fuzhou_tietu_1341243_13_13 -> {1,3,4,1,2,4,3,13,13}
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exorcism")
    TArray<int32> NodeSequence;

    // 自动生成的模式标识符（= 纹理资产名）
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exorcism")
    FName PatternId;
};

// 一条扫描到的鬼类型记录
USTRUCT(BlueprintType)
struct FExorcismGhostEntry
{
    GENERATED_BODY()

    // 鬼类型编号 (从 BP_Ghost_X 的 X 解析)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exorcism")
    int32 GhostTypeIndex = 0;

    // 鬼角色蓝图类路径
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exorcism")
    FSoftClassPath GhostClassPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exorcism")
    FSoftObjectPath BookGhostImagePath;
};

// 驱魔书的一个页面槽位：左侧符咒 + 右侧鬼图
USTRUCT(BlueprintType)
struct FExorcismBookSlot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exorcism")
    int32 SlotIndex = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exorcism")
    FExorcismGhostEntry Ghost;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exorcism")
    FExorcismRuneEntry Rune;
};

UCLASS()
class KONGBU_API UExorcismSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 扫描资源并生成随机映射。每次调用都会重新洗牌。
    // 建议在 GameMode 的 BeginPlay 中调用一次。
    UFUNCTION(BlueprintCallable, Category = "Exorcism")
    void GenerateRandomMappings();

    // 获取当前局的所有书页槽位（供 UI 显示）
    UFUNCTION(BlueprintCallable, Category = "Exorcism")
    TArray<FExorcismBookSlot> GetBookSlots() const { return BookSlots; }

    // 获取所有已映射的符咒模式，可直接赋值给 RuneInstrument 的 AcceptedPatterns
    UFUNCTION(BlueprintCallable, Category = "Exorcism")
    TArray<FRuneInstrumentPattern> GetAllAcceptedRunePatterns() const;

    // 根据符咒 PatternId 查询它对应的鬼类型编号
    UFUNCTION(BlueprintPure, Category = "Exorcism")
    int32 GetGhostTypeForPatternId(FName PatternId) const;

    // 为一个鬼领取一个随机类型。返回鬼类型型编号；池耗尽时返回 INDEX_NONE。
    UFUNCTION(BlueprintCallable, Category = "Exorcism")
    int32 ClaimNextGhostType();

    // 根据鬼类型编号获取鬼角色蓝图类路径
    UFUNCTION(BlueprintPure, Category = "Exorcism")
    FSoftClassPath GetGhostClassPath(int32 GhostTypeIndex) const;

    // 根据鬼类型编号加载鬼角色蓝图类，供 GameMode 或 GhostSpawner 直接 SpawnActor 使用。
    UFUNCTION(BlueprintCallable, Category = "Exorcism")
    TSubclassOf<AGhostCharacter> LoadGhostClass(int32 GhostTypeIndex) const;

    // 根据鬼类型编号获取书中对应的鬼图纹理路径
    UFUNCTION(BlueprintPure, Category = "Exorcism")
    FSoftObjectPath GetBookGhostImagePath(int32 GhostTypeIndex) const;

    // 根据鬼类型编号获取对应的符咒纹理路径
    UFUNCTION(BlueprintPure, Category = "Exorcism")
    FSoftObjectPath GetRuneTextureForGhostType(int32 GhostTypeIndex) const;

    // 是否已生成映射
    UFUNCTION(BlueprintPure, Category = "Exorcism")
    bool HasMappings() const { return BookSlots.Num() > 0; }

    // 扫描到的鬼类型数量
    UFUNCTION(BlueprintPure, Category = "Exorcism")
    int32 GetAvailableGhostTypeCount() const { return ScannedGhosts.Num(); }

    // 扫描到的符咒纹理数量
    UFUNCTION(BlueprintPure, Category = "Exorcism")
    int32 GetAvailableRuneCount() const { return ScannedRunes.Num(); }

private:
    bool TryBuildRuneEntryFromAssetName(const FString& AssetName, FExorcismRuneEntry& OutEntry) const;
    bool ParseNodeSequenceFromSuffix(const FString& NumberPart, TArray<int32>& OutSequence) const;
    void ScanAssets();
    void ScanRuneTextures();
    void ScanGhostClasses();
    void ScanBookGhostImages();

    // TODO(UI): 当前先写死难度，后续由玩家 UI 选择后赋值。
    EExorcismRuneDifficulty CurrentRuneDifficulty = EExorcismRuneDifficulty::Medium;

    bool bScanned = false;
    TArray<FExorcismRuneEntry> ScannedRunes;
    TArray<FExorcismGhostEntry> ScannedGhosts;
    TArray<FExorcismBookSlot> BookSlots;
    TArray<int32> UnclaimedGhostTypes;
};
