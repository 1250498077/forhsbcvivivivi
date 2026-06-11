#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MyHUD.generated.h"

class APickupActorAAARuneInstrument;

UCLASS()
class KONGBU_API AMyHUD : public AHUD
{
    GENERATED_BODY()
public:
    // 每帧在屏幕中央绘制准星。
    virtual void DrawHUD() override;

    // 准星透明度。0 为完全透明，1 为完全不透明。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair")
    float CrosshairOpacity = 1.0f;

    // 准星四条线段的长度。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair")
    float CrosshairLineLen = 10.f;

    // 准星中心留白距离。调大后准星会更散开。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair")
    float CrosshairGap = 4.f;

    // 准星线条粗细。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair")
    float CrosshairThickness = 1.5f;

    // 准星颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crosshair")
    FLinearColor CrosshairColor = FLinearColor::White;

    // 符文节点指示器半径。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune")
    float RuneNodeIndicatorRadius = 10.f;

    // 符文节点默认颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune")
    FLinearColor RuneNodeColor = FLinearColor(0.4f, 0.6f, 1.0f, 0.8f);

    // 符文节点被鼠标悬停时的高亮颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune")
    FLinearColor RuneNodeHoveredColor = FLinearColor(1.0f, 0.9f, 0.2f, 1.0f);

    // 符文已经连接的节点颜色。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rune")
    FLinearColor RuneNodeActiveColor = FLinearColor(0.2f, 1.0f, 0.3f, 1.0f);

private:
    void DrawRuneOverlay();
};