#include "KongbuGameInstance.h"

#include "Engine/World.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Online/OnlineSessionNames.h"

namespace
{
    const FName KongbuSessionTagKey(TEXT("KONGBU_SESSION"));
    const FString KongbuSessionTagValue(TEXT("COOP_V1"));
}

void UKongbuGameInstance::Init()
{
    Super::Init();
}

void UKongbuGameInstance::Shutdown()
{
    ClearSessionDelegates();
    SessionSearch.Reset();

    Super::Shutdown();
}

bool UKongbuGameInstance::HostSession(int32 MaxPlayers)
{
    IOnlineSessionPtr SessionInterface = GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        return false;
    }

    if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("HostSession skipped: NAME_GameSession already exists"));
        return false;
    }

    TSharedRef<FOnlineSessionSettings> SessionSettings = MakeShared<FOnlineSessionSettings>();
    SessionSettings->bIsLANMatch = IsUsingLanFallback();
    SessionSettings->NumPublicConnections = FMath::Max(2, MaxPlayers);
    SessionSettings->NumPrivateConnections = 0;
    SessionSettings->bAllowJoinInProgress = true;
    SessionSettings->bAllowJoinViaPresence = true;
    SessionSettings->bShouldAdvertise = true;
    SessionSettings->bUsesPresence = true;
    SessionSettings->bUseLobbiesIfAvailable = true;
    SessionSettings->bUseLobbiesVoiceChatIfAvailable = false;
    SessionSettings->Set(SETTING_MAPNAME, GetListenMapPath(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    SessionSettings->Set(KongbuSessionTagKey, KongbuSessionTagValue, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

    CreateSessionCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateWeakLambda(this,
            [this](FName SessionName, bool bWasSuccessful)
            {
                HandleCreateSessionCompleted(SessionName, bWasSuccessful);
            }));

    constexpr int32 LocalUserNum = 0;
    if (!SessionInterface->CreateSession(LocalUserNum, NAME_GameSession, *SessionSettings))
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
        CreateSessionCompleteHandle.Reset();
        return false;
    }

    return true;
}

bool UKongbuGameInstance::FindSessions(int32 MaxResults)
{
    IOnlineSessionPtr SessionInterface = GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        return false;
    }

    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->bIsLanQuery = IsUsingLanFallback();
    SessionSearch->MaxSearchResults = FMath::Max(1, MaxResults);
    SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
    SessionSearch->QuerySettings.Set(KongbuSessionTagKey, KongbuSessionTagValue, EOnlineComparisonOp::Equals);

    FindSessionsCompleteHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateWeakLambda(this,
            [this](bool bWasSuccessful)
            {
                HandleFindSessionsCompleted(bWasSuccessful);
            }));

    constexpr int32 LocalUserNum = 0;
    if (!SessionInterface->FindSessions(LocalUserNum, SessionSearch.ToSharedRef()))
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
        FindSessionsCompleteHandle.Reset();
        return false;
    }

    return true;
}

bool UKongbuGameInstance::JoinSessionByIndex(int32 SessionIndex)
{
    IOnlineSessionPtr SessionInterface = GetSessionInterface();
    if (!SessionInterface.IsValid() || !SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(SessionIndex))
    {
        return false;
    }

    JoinSessionCompleteHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateWeakLambda(this,
            [this](FName SessionName, EOnJoinSessionCompleteResult::Type Result)
            {
                HandleJoinSessionCompleted(SessionName, Result);
            }));

    constexpr int32 LocalUserNum = 0;
    if (!SessionInterface->JoinSession(LocalUserNum, NAME_GameSession, SessionSearch->SearchResults[SessionIndex]))
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
        JoinSessionCompleteHandle.Reset();
        return false;
    }

    return true;
}

int32 UKongbuGameInstance::GetCachedSessionCount() const
{
    return SessionSearch.IsValid() ? SessionSearch->SearchResults.Num() : 0;
}

FString UKongbuGameInstance::GetCachedSessionOwnerName(int32 SessionIndex) const
{
    if (!SessionSearch.IsValid() || !SessionSearch->SearchResults.IsValidIndex(SessionIndex))
    {
        return FString();
    }

    return SessionSearch->SearchResults[SessionIndex].Session.OwningUserName;
}

IOnlineSessionPtr UKongbuGameInstance::GetSessionInterface() const
{
    if (IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get())
    {
        return OnlineSubsystem->GetSessionInterface();
    }

    return nullptr;
}

bool UKongbuGameInstance::IsUsingLanFallback() const
{
    const IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
    return !OnlineSubsystem || OnlineSubsystem->GetSubsystemName() == NAME_None || OnlineSubsystem->GetSubsystemName() == TEXT("NULL");
}

FString UKongbuGameInstance::GetListenMapPath() const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return TEXT("/Game/level/level_1");
    }

    return World->GetOutermost()->GetName();
}

void UKongbuGameInstance::HandleCreateSessionCompleted(FName SessionName, bool bWasSuccessful)
{
    IOnlineSessionPtr SessionInterface = GetSessionInterface();
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
    }
    CreateSessionCompleteHandle.Reset();

    if (!bWasSuccessful)
    {
        UE_LOG(LogTemp, Warning, TEXT("CreateSession failed for %s"), *SessionName.ToString());
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->ServerTravel(GetListenMapPath() + TEXT("?listen"));
    }
}

void UKongbuGameInstance::HandleFindSessionsCompleted(bool bWasSuccessful)
{
    IOnlineSessionPtr SessionInterface = GetSessionInterface();
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
    }
    FindSessionsCompleteHandle.Reset();

    UE_LOG(LogTemp, Log, TEXT("FindSessions completed. Success=%s Results=%d"),
        bWasSuccessful ? TEXT("true") : TEXT("false"),
        SessionSearch.IsValid() ? SessionSearch->SearchResults.Num() : 0);
}

void UKongbuGameInstance::HandleJoinSessionCompleted(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    IOnlineSessionPtr SessionInterface = GetSessionInterface();
    if (SessionInterface.IsValid())
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
    }
    JoinSessionCompleteHandle.Reset();

    if (!SessionInterface.IsValid() || Result != EOnJoinSessionCompleteResult::Success)
    {
        UE_LOG(LogTemp, Warning, TEXT("JoinSession failed for %s with result %d"), *SessionName.ToString(), static_cast<int32>(Result));
        return;
    }

    FString ConnectString;
    if (!SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString))
    {
        UE_LOG(LogTemp, Warning, TEXT("JoinSession succeeded but no connect string was resolved"));
        return;
    }

    if (APlayerController* PlayerController = GetFirstLocalPlayerController())
    {
        PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
    }
}

void UKongbuGameInstance::ClearSessionDelegates()
{
    IOnlineSessionPtr SessionInterface = GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        return;
    }

    if (CreateSessionCompleteHandle.IsValid())
    {
        SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
        CreateSessionCompleteHandle.Reset();
    }

    if (FindSessionsCompleteHandle.IsValid())
    {
        SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
        FindSessionsCompleteHandle.Reset();
    }

    if (JoinSessionCompleteHandle.IsValid())
    {
        SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
        JoinSessionCompleteHandle.Reset();
    }
}