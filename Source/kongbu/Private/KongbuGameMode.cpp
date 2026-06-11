#include "KongbuGameMode.h"

#include "ExorcismSubsystem.h"
#include "Engine/GameInstance.h"

void AKongbuGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UExorcismSubsystem* Subsystem = GI->GetSubsystem<UExorcismSubsystem>())
        {
            Subsystem->GenerateRandomMappings();
            UE_LOG(LogTemp, Log, TEXT("KongbuGameMode: exorcism mappings generated — %d slots"),
                Subsystem->GetBookSlots().Num());
        }
    }
}
