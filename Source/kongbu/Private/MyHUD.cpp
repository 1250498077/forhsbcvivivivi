#include "MyHUD.h"
#include "Engine/Canvas.h"
#include "MyPlayerController.h"
#include "PickupActorAAARuneInstrument.h"

void AMyHUD::DrawHUD()
{
    Super::DrawHUD();

    // 先取屏幕中心点，后续四条线都围绕这个中心展开。
    float CX = Canvas->SizeX * 0.5f;
    float CY = Canvas->SizeY * 0.5f;

    // 用统一颜色绘制四个方向的线，只单独覆写 Alpha 作为透明度。
    FLinearColor Color = CrosshairColor;
    Color.A = CrosshairOpacity;

    DrawLine(CX, CY - CrosshairGap - CrosshairLineLen, CX, CY - CrosshairGap, Color, CrosshairThickness);
    DrawLine(CX, CY + CrosshairGap, CX, CY + CrosshairGap + CrosshairLineLen, Color, CrosshairThickness);
    DrawLine(CX - CrosshairGap - CrosshairLineLen, CY, CX - CrosshairGap, CY, Color, CrosshairThickness);
    DrawLine(CX + CrosshairGap, CY, CX + CrosshairGap + CrosshairLineLen, CY, Color, CrosshairThickness);

    DrawRuneOverlay();
}

void AMyHUD::DrawRuneOverlay()
{
    AMyPlayerController* PC = Cast<AMyPlayerController>(GetOwningPlayerController());
    if (!PC)
    {
        return;
    }

    APickupActorAAARuneInstrument* RuneInstrument = Cast<APickupActorAAARuneInstrument>(PC->GetHeldActor());
    if (!RuneInstrument || !RuneInstrument->IsRuneDrawActive())
    {
        return;
    }

    // 取所有节点的屏幕位置。
    TArray<TPair<int32, FVector2D>> NodePositions;
    RuneInstrument->GetNodeScreenPositions(PC, NodePositions);

    const int32 CurrentHoveredId = RuneInstrument->GetHoveredNodeId();
    const TArray<int32> ActiveSequence = RuneInstrument->GetActiveRuneSequence();

    // 绘制每个节点的屏幕指示器。
    const int32 Segments = 16;
    for (const TPair<int32, FVector2D>& NodePos : NodePositions)
    {
        const int32 NodeId = NodePos.Key;
        const FVector2D& Pos = NodePos.Value;

        // 根据状态选颜色：悬停 > 已连接 > 默认。
        FLinearColor NodeColor;
        float Radius = RuneNodeIndicatorRadius;

        if (NodeId == CurrentHoveredId)
        {
            NodeColor = RuneNodeHoveredColor;
            Radius *= 1.4f;
        }
        else if (ActiveSequence.Contains(NodeId))
        {
            NodeColor = RuneNodeActiveColor;
        }
        else
        {
            NodeColor = RuneNodeColor;
        }

        // 用线段画圆形指示器。
        for (int32 i = 0; i < Segments; ++i)
        {
            const float Angle0 = (2.f * PI * i) / Segments;
            const float Angle1 = (2.f * PI * (i + 1)) / Segments;
            DrawLine(
                Pos.X + FMath::Cos(Angle0) * Radius,
                Pos.Y + FMath::Sin(Angle0) * Radius,
                Pos.X + FMath::Cos(Angle1) * Radius,
                Pos.Y + FMath::Sin(Angle1) * Radius,
                NodeColor,
                2.f);
        }

        // 在节点旁显示编号。
        const FString NodeLabel = FString::FromInt(NodeId);
        DrawText(NodeLabel, NodeColor, Pos.X + Radius + 4.f, Pos.Y - 6.f);
    }
}