#include "MainUI.h"

#include "KongbuGameInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UMainUI::NativeConstruct()
{
    Super::NativeConstruct();

    if (HostButton)
    {
        HostButton->OnClicked.AddDynamic(this, &UMainUI::OnHostClicked);
    }

    if (FindButton)
    {
        FindButton->OnClicked.AddDynamic(this, &UMainUI::OnFindClicked);
    }

    if (JoinFirstButton)
    {
        JoinFirstButton->OnClicked.AddDynamic(this, &UMainUI::OnJoinFirstClicked);
    }

    UpdateStatusText(TEXT("Ready. Click Host or Find."));
    UpdateJoinButtonState();
}

void UMainUI::OnHostClicked()
{
    UKongbuGameInstance* KongbuGameInstance = GetKongbuGameInstance();
    if (!KongbuGameInstance)
    {
        UpdateStatusText(TEXT("Host failed: GameInstance is not KongbuGameInstance."));
        return;
    }

    const bool bHostStarted = KongbuGameInstance->HostSession(HostMaxPlayers);
    UpdateStatusText(bHostStarted
        ? FString::Printf(TEXT("Hosting %d-player session..."), HostMaxPlayers)
        : TEXT("Host failed: could not create session."));
}

void UMainUI::OnFindClicked()
{
    UKongbuGameInstance* KongbuGameInstance = GetKongbuGameInstance();
    if (!KongbuGameInstance)
    {
        UpdateStatusText(TEXT("Find failed: GameInstance is not KongbuGameInstance."));
        return;
    }

    const bool bSearchStarted = KongbuGameInstance->FindSessions(SessionSearchMaxResults);
    if (!bSearchStarted)
    {
        UpdateStatusText(TEXT("Find failed: session search did not start."));
        UpdateJoinButtonState();
        return;
    }

    UpdateStatusText(TEXT("Searching sessions... please wait 2 seconds, then Join."));

    if (UWorld* World = GetWorld())
    {
        FTimerHandle RefreshHandle;
        World->GetTimerManager().SetTimer(
            RefreshHandle,
            this,
            &UMainUI::RefreshSearchStatus,
            SearchResultRefreshDelay,
            false);
    }
}

void UMainUI::OnJoinFirstClicked()
{
    UKongbuGameInstance* KongbuGameInstance = GetKongbuGameInstance();
    if (!KongbuGameInstance)
    {
        UpdateStatusText(TEXT("Join failed: GameInstance is not KongbuGameInstance."));
        return;
    }

    if (KongbuGameInstance->GetCachedSessionCount() <= 0)
    {
        UpdateStatusText(TEXT("Join failed: no cached session. Click Find first."));
        UpdateJoinButtonState();
        return;
    }

    const bool bJoinStarted = KongbuGameInstance->JoinSessionByIndex(0);
    UpdateStatusText(bJoinStarted
        ? TEXT("Joining first found session...")
        : TEXT("Join failed: could not start session join."));
}

UKongbuGameInstance* UMainUI::GetKongbuGameInstance() const
{
    return GetWorld() ? GetWorld()->GetGameInstance<UKongbuGameInstance>() : nullptr;
}

void UMainUI::RefreshSearchStatus()
{
    UKongbuGameInstance* KongbuGameInstance = GetKongbuGameInstance();
    if (!KongbuGameInstance)
    {
        UpdateStatusText(TEXT("Search refresh failed: GameInstance unavailable."));
        UpdateJoinButtonState();
        return;
    }

    const int32 SessionCount = KongbuGameInstance->GetCachedSessionCount();
    if (SessionCount <= 0)
    {
        UpdateStatusText(TEXT("Find complete: no session found."));
        UpdateJoinButtonState();
        return;
    }

    const FString OwnerName = KongbuGameInstance->GetCachedSessionOwnerName(0);
    UpdateStatusText(FString::Printf(
        TEXT("Found %d session(s). First owner: %s. Click Join."),
        SessionCount,
        OwnerName.IsEmpty() ? TEXT("Unknown") : *OwnerName));
    UpdateJoinButtonState();
}

void UMainUI::UpdateStatusText(const FString& InStatus) const
{
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(InStatus));
    }

    UE_LOG(LogTemp, Log, TEXT("MainUI status: %s"), *InStatus);
}

void UMainUI::UpdateJoinButtonState()
{
    if (!JoinFirstButton)
    {
        return;
    }

    const UKongbuGameInstance* KongbuGameInstance = GetKongbuGameInstance();
    JoinFirstButton->SetIsEnabled(KongbuGameInstance && KongbuGameInstance->GetCachedSessionCount() > 0);
}
