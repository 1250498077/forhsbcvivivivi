#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "TabUI.generated.h"

UCLASS()
class KONGBU_API UTabUI : public UUserWidget
{
    GENERATED_BODY()

protected:
    // Widget 构建后绑定关闭按钮。
    virtual void NativeConstruct() override;

    // 关闭按钮，名字需要和蓝图中的控件名一致。
    UPROPERTY(meta = (BindWidget))
    UButton* CloseButton;

    // 点击后把当前 TabUI 从视口移除。
    UFUNCTION()
    void OnCloseClicked();
};
