#include "ExorcismSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "GhostCharacter.h"
#include "PickupActorAAARuneInstrument.h"

namespace
{
    const FString RuneTextureScanPath   = TEXT("/Game/item/book");
    const FString LegacyRuneTexturePrefix    = TEXT("book_fuzhou_tietu_");
    const FString EasyRuneTexturePrefix      = TEXT("book_easy_fuzhou_tietu_");
    const FString MediumRuneTexturePrefix    = TEXT("book_medium_fuzhou_tietu_");
    const FString HardRuneTexturePrefix      = TEXT("book_hard_fuzhou_tietu_");
    const FString NightmareRuneTexturePrefix = TEXT("book_nightmare_fuzhou_tietu_");
    const FString GhostClassScanPath  = TEXT("/Game/AI/gui");
    const FString GhostClassPrefix    = TEXT("BP_Ghost_");
    const FString BookGhostImageScanPath = TEXT("/Game/item/book");
    const FString BookGhostImagePrefix  = TEXT("book_gui_");

    FString GetRuneDifficultyPrefix(EExorcismRuneDifficulty Difficulty)
    {
        switch (Difficulty)
        {
        case EExorcismRuneDifficulty::Easy:
            return EasyRuneTexturePrefix;
        case EExorcismRuneDifficulty::Hard:
            return HardRuneTexturePrefix;
        case EExorcismRuneDifficulty::Nightmare:
            return NightmareRuneTexturePrefix;
        case EExorcismRuneDifficulty::Medium:
        default:
            return MediumRuneTexturePrefix;
        }
    }
}

void UExorcismSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UExorcismSubsystem::Deinitialize()
{
    ScannedRunes.Reset();
    ScannedGhosts.Reset();
    BookSlots.Reset();
    UnclaimedGhostTypes.Reset();
    Super::Deinitialize();
}

// ------------------------------------------------------------------ 扫描
void UExorcismSubsystem::ScanAssets()
{
    if (bScanned)
    {
        return;
    }
    bScanned = true;

    ScanRuneTextures();
    ScanGhostClasses();
    ScanBookGhostImages();

    UE_LOG(LogTemp, Log,
        TEXT("ExorcismSubsystem: scanned %d rune textures, %d ghost classes"),
        ScannedRunes.Num(), ScannedGhosts.Num());
}

void UExorcismSubsystem::ScanRuneTextures()
{
    ScannedRunes.Reset();

    IAssetRegistry& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*RuneTextureScanPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Assets;
    AssetRegistry.GetAssets(Filter, Assets);

    const FString SelectedDifficultyPrefix = GetRuneDifficultyPrefix(CurrentRuneDifficulty);
    TArray<FExorcismRuneEntry> DifficultyMatchedRunes;
    TArray<FExorcismRuneEntry> LegacyFallbackRunes;

    for (const FAssetData& Asset : Assets)
    {
        const FString AssetName = Asset.AssetName.ToString();

        FExorcismRuneEntry ParsedEntry;
        if (AssetName.StartsWith(SelectedDifficultyPrefix, ESearchCase::IgnoreCase))
        {
            if (TryBuildRuneEntryFromAssetName(AssetName, ParsedEntry))
            {
                ParsedEntry.TexturePath = Asset.GetSoftObjectPath();
                DifficultyMatchedRunes.Add(MoveTemp(ParsedEntry));
            }
            continue;
        }

        if (AssetName.StartsWith(LegacyRuneTexturePrefix, ESearchCase::IgnoreCase))
        {
            if (TryBuildRuneEntryFromAssetName(AssetName, ParsedEntry))
            {
                ParsedEntry.TexturePath = Asset.GetSoftObjectPath();
                LegacyFallbackRunes.Add(MoveTemp(ParsedEntry));
            }
            continue;
        }
    }

    if (DifficultyMatchedRunes.Num() > 0)
    {
        ScannedRunes = MoveTemp(DifficultyMatchedRunes);
    }
    else
    {
        ScannedRunes = MoveTemp(LegacyFallbackRunes);
    }

    for (const FExorcismRuneEntry& Entry : ScannedRunes)
    {
        UE_LOG(LogTemp, Log,
            TEXT("Exorcism: rune texture '%s' -> %d nodes"),
            *Entry.PatternId.ToString(), Entry.NodeSequence.Num());
    }
}

bool UExorcismSubsystem::TryBuildRuneEntryFromAssetName(
    const FString& AssetName,
    FExorcismRuneEntry& OutEntry) const
{
    OutEntry = FExorcismRuneEntry();

    FString NumberPart;
    if (AssetName.StartsWith(LegacyRuneTexturePrefix, ESearchCase::IgnoreCase))
    {
        NumberPart = AssetName.RightChop(LegacyRuneTexturePrefix.Len());
    }
    else if (AssetName.StartsWith(EasyRuneTexturePrefix, ESearchCase::IgnoreCase))
    {
        NumberPart = AssetName.RightChop(EasyRuneTexturePrefix.Len());
    }
    else if (AssetName.StartsWith(MediumRuneTexturePrefix, ESearchCase::IgnoreCase))
    {
        NumberPart = AssetName.RightChop(MediumRuneTexturePrefix.Len());
    }
    else if (AssetName.StartsWith(HardRuneTexturePrefix, ESearchCase::IgnoreCase))
    {
        NumberPart = AssetName.RightChop(HardRuneTexturePrefix.Len());
    }
    else if (AssetName.StartsWith(NightmareRuneTexturePrefix, ESearchCase::IgnoreCase))
    {
        NumberPart = AssetName.RightChop(NightmareRuneTexturePrefix.Len());
    }
    else
    {
        return false;
    }

    if (!ParseNodeSequenceFromSuffix(NumberPart, OutEntry.NodeSequence))
    {
        return false;
    }

    OutEntry.PatternId = FName(*AssetName);
    return true;
}

bool UExorcismSubsystem::ParseNodeSequenceFromSuffix(
    const FString& NumberPart,
    TArray<int32>& OutSequence) const
{
    OutSequence.Reset();
    if (NumberPart.IsEmpty())
    {
        return false;
    }

    const bool bHasUnderscore = NumberPart.Contains(TEXT("_"));
    if (!bHasUnderscore)
    {
        for (const TCHAR Ch : NumberPart)
        {
            if (!FChar::IsDigit(Ch))
            {
                continue;
            }

            const int32 NodeId = Ch - TEXT('0');
            if (NodeId > 0)
            {
                OutSequence.Add(NodeId);
            }
        }

        return OutSequence.Num() >= 2;
    }

    TArray<FString> Tokens;
    NumberPart.ParseIntoArray(Tokens, TEXT("_"), false);
    if (Tokens.Num() == 0)
    {
        return false;
    }

    // 首段兼容旧规则：按单数字解析。后续段按整体整数解析（支持 10、13、25...）。
    const FString& FirstToken = Tokens[0];
    for (const TCHAR Ch : FirstToken)
    {
        if (!FChar::IsDigit(Ch))
        {
            continue;
        }

        const int32 NodeId = Ch - TEXT('0');
        if (NodeId > 0)
        {
            OutSequence.Add(NodeId);
        }
    }

    for (int32 Index = 1; Index < Tokens.Num(); ++Index)
    {
        const FString& Token = Tokens[Index];
        if (Token.IsEmpty())
        {
            continue;
        }

        bool bTokenAllDigits = true;
        for (const TCHAR Ch : Token)
        {
            if (!FChar::IsDigit(Ch))
            {
                bTokenAllDigits = false;
                break;
            }
        }

        if (!bTokenAllDigits)
        {
            continue;
        }

        const int32 NodeId = FCString::Atoi(*Token);
        if (NodeId > 0)
        {
            OutSequence.Add(NodeId);
        }
    }

    return OutSequence.Num() >= 2;
}

void UExorcismSubsystem::ScanGhostClasses()
{
    ScannedGhosts.Reset();

    IAssetRegistry& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*GhostClassScanPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Assets;
    AssetRegistry.GetAssets(Filter, Assets);

    for (const FAssetData& Asset : Assets)
    {
        const FString AssetName = Asset.AssetName.ToString();
        if (!AssetName.StartsWith(GhostClassPrefix, ESearchCase::IgnoreCase))
        {
            continue;
        }

        const FString IndexStr = AssetName.RightChop(GhostClassPrefix.Len());
        if (IndexStr.IsEmpty())
        {
            continue;
        }

        bool bIsNumeric = true;
        for (const TCHAR Ch : IndexStr)
        {
            if (!FChar::IsDigit(Ch))
            {
                bIsNumeric = false;
                break;
            }
        }
        if (!bIsNumeric)
        {
            continue;
        }

        const int32 TypeIndex = FCString::Atoi(*IndexStr);
        if (TypeIndex <= 0)
        {
            continue;
        }

        bool bDuplicate = false;
        for (const FExorcismGhostEntry& Existing : ScannedGhosts)
        {
            if (Existing.GhostTypeIndex == TypeIndex)
            {
                bDuplicate = true;
                break;
            }
        }
        if (bDuplicate)
        {
            continue;
        }

        FExorcismGhostEntry Entry;
        Entry.GhostTypeIndex = TypeIndex;
        Entry.GhostClassPath = FSoftClassPath(Asset.GetSoftObjectPath().ToString() + TEXT("_C"));
        ScannedGhosts.Add(MoveTemp(Entry));

        UE_LOG(LogTemp, Log,
            TEXT("Exorcism: ghost class '%s' (type %d)"),
            *AssetName, TypeIndex);
    }

    ScannedGhosts.Sort([](const FExorcismGhostEntry& A, const FExorcismGhostEntry& B)
    {
        return A.GhostTypeIndex < B.GhostTypeIndex;
    });
}

void UExorcismSubsystem::ScanBookGhostImages()
{
    IAssetRegistry& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*BookGhostImageScanPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Assets;
    AssetRegistry.GetAssets(Filter, Assets);

    for (const FAssetData& Asset : Assets)
    {
        const FString AssetName = Asset.AssetName.ToString();
        if (!AssetName.StartsWith(BookGhostImagePrefix, ESearchCase::IgnoreCase))
        {
            continue;
        }

        const FString IndexStr = AssetName.RightChop(BookGhostImagePrefix.Len());
        if (IndexStr.IsEmpty())
        {
            continue;
        }

        bool bIsNumeric = true;
        for (const TCHAR Ch : IndexStr)
        {
            if (!FChar::IsDigit(Ch))
            {
                bIsNumeric = false;
                break;
            }
        }
        if (!bIsNumeric)
        {
            continue;
        }

        const int32 TypeIndex = FCString::Atoi(*IndexStr);
        if (TypeIndex <= 0)
        {
            continue;
        }

        for (FExorcismGhostEntry& Ghost : ScannedGhosts)
        {
            if (Ghost.GhostTypeIndex == TypeIndex)
            {
                Ghost.BookGhostImagePath = Asset.GetSoftObjectPath();
                UE_LOG(LogTemp, Log,
                    TEXT("Exorcism: book ghost image '%s' -> ghost type %d"),
                    *AssetName, TypeIndex);
                break;
            }
        }
    }
}

// ------------------------------------------------------------------ 映射生成
void UExorcismSubsystem::GenerateRandomMappings()
{
    // 支持重新调用：清空旧数据并重新扫描
    bScanned = false;
    ScanAssets();

    BookSlots.Reset();
    UnclaimedGhostTypes.Reset();

    if (ScannedGhosts.Num() == 0 || ScannedRunes.Num() == 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ExorcismSubsystem: cannot generate mappings — %d ghosts, %d runes found"),
            ScannedGhosts.Num(), ScannedRunes.Num());
        return;
    }

    const int32 MappingCount = FMath::Min(ScannedGhosts.Num(), ScannedRunes.Num());

    // 打乱符咒索引
    TArray<int32> RuneIndices;
    RuneIndices.Reserve(ScannedRunes.Num());
    for (int32 i = 0; i < ScannedRunes.Num(); ++i)
    {
        RuneIndices.Add(i);
    }
    for (int32 i = RuneIndices.Num() - 1; i > 0; --i)
    {
        const int32 j = FMath::RandRange(0, i);
        RuneIndices.Swap(i, j);
    }

    // 生成书页映射
    for (int32 i = 0; i < MappingCount; ++i)
    {
        FExorcismBookSlot Slot;
        Slot.SlotIndex = i;
        Slot.Ghost = ScannedGhosts[i];
        Slot.Rune = ScannedRunes[RuneIndices[i]];
        BookSlots.Add(MoveTemp(Slot));

        UE_LOG(LogTemp, Log,
            TEXT("Exorcism Slot %d: ghost type %d <-> rune '%s' (nodes: %d)"),
            i,
            BookSlots.Last().Ghost.GhostTypeIndex,
            *BookSlots.Last().Rune.PatternId.ToString(),
            BookSlots.Last().Rune.NodeSequence.Num());
    }

    // 初始化未领取鬼类型池并打乱
    UnclaimedGhostTypes.Reserve(BookSlots.Num());
    for (int32 i = 0; i < BookSlots.Num(); ++i)
    {
        UnclaimedGhostTypes.Add(BookSlots[i].Ghost.GhostTypeIndex);
    }
    for (int32 i = UnclaimedGhostTypes.Num() - 1; i > 0; --i)
    {
        const int32 j = FMath::RandRange(0, i);
        UnclaimedGhostTypes.Swap(i, j);
    }
}

// ------------------------------------------------------------------ 查询 API
TArray<FRuneInstrumentPattern> UExorcismSubsystem::GetAllAcceptedRunePatterns() const
{
    TArray<FRuneInstrumentPattern> Patterns;
    Patterns.Reserve(BookSlots.Num());

    for (const FExorcismBookSlot& Slot : BookSlots)
    {
        FRuneInstrumentPattern Pattern;
        Pattern.PatternId = Slot.Rune.PatternId;
        Pattern.NodeSequence = Slot.Rune.NodeSequence;
        Patterns.Add(MoveTemp(Pattern));
    }

    return Patterns;
}

int32 UExorcismSubsystem::GetGhostTypeForPatternId(FName PatternId) const
{
    for (const FExorcismBookSlot& Slot : BookSlots)
    {
        if (Slot.Rune.PatternId == PatternId)
        {
            return Slot.Ghost.GhostTypeIndex;
        }
    }
    return INDEX_NONE;
}

int32 UExorcismSubsystem::ClaimNextGhostType()
{
    if (UnclaimedGhostTypes.Num() == 0)
    {
        return INDEX_NONE;
    }

    const int32 TypeId = UnclaimedGhostTypes.Pop(EAllowShrinking::No);
    return TypeId;
}

FSoftClassPath UExorcismSubsystem::GetGhostClassPath(int32 GhostTypeIndex) const
{
    for (const FExorcismGhostEntry& Ghost : ScannedGhosts)
    {
        if (Ghost.GhostTypeIndex == GhostTypeIndex)
        {
            return Ghost.GhostClassPath;
        }
    }
    return FSoftClassPath();
}

TSubclassOf<AGhostCharacter> UExorcismSubsystem::LoadGhostClass(int32 GhostTypeIndex) const
{
    const FSoftClassPath GhostClassPath = GetGhostClassPath(GhostTypeIndex);
    if (!GhostClassPath.IsValid())
    {
        return nullptr;
    }

    UClass* LoadedClass = GhostClassPath.TryLoadClass<AGhostCharacter>();
    if (!IsValid(LoadedClass) || !LoadedClass->IsChildOf(AGhostCharacter::StaticClass()))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("ExorcismSubsystem: ghost type %d class '%s' is not a GhostCharacter class"),
            GhostTypeIndex,
            *GhostClassPath.ToString());
        return nullptr;
    }

    return LoadedClass;
}

FSoftObjectPath UExorcismSubsystem::GetBookGhostImagePath(int32 GhostTypeIndex) const
{
    for (const FExorcismBookSlot& Slot : BookSlots)
    {
        if (Slot.Ghost.GhostTypeIndex == GhostTypeIndex)
        {
            return Slot.Ghost.BookGhostImagePath;
        }
    }
    return FSoftObjectPath();
}

FSoftObjectPath UExorcismSubsystem::GetRuneTextureForGhostType(int32 GhostTypeIndex) const
{
    for (const FExorcismBookSlot& Slot : BookSlots)
    {
        if (Slot.Ghost.GhostTypeIndex == GhostTypeIndex)
        {
            return Slot.Rune.TexturePath;
        }
    }
    return FSoftObjectPath();
}
