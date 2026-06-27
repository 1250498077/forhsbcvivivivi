// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainUI.generated.h"

class UButton;
class UTextBlock;
class UKongbuGameInstance;

UCLASS()
class KONGBU_API UMainUI : public UUserWidget
{
	GENERATED_BODY()

protected:
    // Widget 创建完成后绑定按钮事件。
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    UButton* HostButton;

    UPROPERTY(meta = (BindWidget))
    UButton* FindButton;

    UPROPERTY(meta = (BindWidget))
    UButton* JoinFirstButton;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* StatusText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Multiplayer", meta = (ClampMin = "2"))
    int32 HostMaxPlayers = 2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Multiplayer", meta = (ClampMin = "1"))
    int32 SessionSearchMaxResults = 20;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Multiplayer", meta = (ClampMin = "0.1"))
    float SearchResultRefreshDelay = 2.0f;

    UFUNCTION()
    void OnHostClicked();

    UFUNCTION()
    void OnFindClicked();

    UFUNCTION()
    void OnJoinFirstClicked();

private:
    UKongbuGameInstance* GetKongbuGameInstance() const;
    void RefreshSearchStatus();
    void UpdateStatusText(const FString& InStatus) const;
    void UpdateJoinButtonState();
};
