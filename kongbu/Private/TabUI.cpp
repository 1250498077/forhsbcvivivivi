// Fill out your copyright notice in the Description page of Project Settings.


#include "TabUI.h"

void UTabUI::NativeConstruct()
{
    Super::NativeConstruct();

    // 这个 UI 很轻量，当前只需要绑定一个关闭按钮即可。
    if (CloseButton)
        CloseButton->OnClicked.AddDynamic(this, &UTabUI::OnCloseClicked);
}

void UTabUI::OnCloseClicked()
{
    UE_LOG(LogTemp, Warning, TEXT("关闭 Tab UI"));
    RemoveFromParent();
}
