#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "KongbuGameMode.generated.h"

UCLASS()
class KONGBU_API AKongbuGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
};
