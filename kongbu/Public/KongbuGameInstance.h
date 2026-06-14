#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "KongbuGameInstance.generated.h"

class FOnlineSessionSearch;

UCLASS()
class KONGBU_API UKongbuGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;
    virtual void Shutdown() override;

    UFUNCTION(BlueprintCallable, Category = "Multiplayer")
    bool HostSession(int32 MaxPlayers = 2);

    UFUNCTION(BlueprintCallable, Category = "Multiplayer")
    bool FindSessions(int32 MaxResults = 20);

    UFUNCTION(BlueprintCallable, Category = "Multiplayer")
    bool JoinSessionByIndex(int32 SessionIndex);

    UFUNCTION(BlueprintPure, Category = "Multiplayer")
    int32 GetCachedSessionCount() const;

    UFUNCTION(BlueprintPure, Category = "Multiplayer")
    FString GetCachedSessionOwnerName(int32 SessionIndex) const;

private:
    IOnlineSessionPtr GetSessionInterface() const;
    bool IsUsingLanFallback() const;
    FString GetListenMapPath() const;

    void HandleCreateSessionCompleted(FName SessionName, bool bWasSuccessful);
    void HandleFindSessionsCompleted(bool bWasSuccessful);
    void HandleJoinSessionCompleted(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void ClearSessionDelegates();

    TSharedPtr<FOnlineSessionSearch> SessionSearch;

    FDelegateHandle CreateSessionCompleteHandle;
    FDelegateHandle FindSessionsCompleteHandle;
    FDelegateHandle JoinSessionCompleteHandle;
};