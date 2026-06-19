#include "PickupActorAAARuneCanvasInstrument.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

APickupActorAAARuneCanvasInstrument::APickupActorAAARuneCanvasInstrument()
{
    PrimaryActorTick.bCanEverTick = false;

    HoldType = EHoldItemType::RuneGridInstrument;
    FP_SocketName = TEXT("RightSocket");
    FP_LocationOffset = FVector::ZeroVector;
    FP_RotationOffset = FRotator::ZeroRotator;
    TP_SocketName = TEXT("RightSocket");
    TP_LocationOffset = FVector::ZeroVector;
    TP_RotationOffset = FRotator::ZeroRotator;

    ItemMassKg = 1.2f;
    ItemThrowForceMultiplier = 0.85f;
    ItemLinearDamping = 0.12f;
    ItemAngularDamping = 0.45f;
    ItemThrowSpinRateDegrees = 720.f;

    Tags.Add(FName("RuneCanvasInstrument"));
    Tags.Add(FName("Pickup"));
    Tags.Add(FName("Rune"));

    DrawSurfaceComponent = CreateDefaultSubobject<USceneComponent>(TEXT("DrawSurfaceComponent"));
    DrawSurfaceComponent->SetupAttachment(VisualMeshRootComponent);
}

void APickupActorAAARuneCanvasInstrument::BeginPlay()
{
    Super::BeginPlay();
    EnsureDrawResources();
}

void APickupActorAAARuneCanvasInstrument::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
}

void APickupActorAAARuneCanvasInstrument::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DrawnUVPoints.Reset();
    RecognizedNodeSequence.Reset();
    DrawRenderTarget = nullptr;
    CardDynamicMaterial = nullptr;

    Super::EndPlay(EndPlayReason);
}

void APickupActorAAARuneCanvasInstrument::OnPickedUp()
{
    Super::OnPickedUp();
    ResetRuneState();
}

void APickupActorAAARuneCanvasInstrument::OnPutDown(FVector PlaceLocation, FRotator PlaceRotation)
{
    Super::OnPutDown(PlaceLocation, PlaceRotation);
    ResetRuneState();
}

void APickupActorAAARuneCanvasInstrument::OnThrown(FVector ThrowDirection, float ThrowForce)
{
    Super::OnThrown(ThrowDirection, ThrowForce);
    ResetRuneState();
}

bool APickupActorAAARuneCanvasInstrument::BeginRuneDraw(APlayerController* UsingController)
{
    if (!UsingController || !EnsureDrawResources())
    {
        return false;
    }

    bRuneDrawActive = true;
    bPatternSolved = false;
    SolvedPatternId = NAME_None;
    DrawnUVPoints.Reset();
    RecognizedNodeSequence.Reset();

    if (bClearTextureOnBeginDraw)
    {
        ClearDrawTexture();
    }

    ReceiveRuneCanvasDrawStateChanged(DrawnUVPoints, true);
    ReceiveRuneCanvasNodeSequenceChanged(RecognizedNodeSequence, INDEX_NONE);
    return true;
}

void APickupActorAAARuneCanvasInstrument::UpdateRuneDrawFromScreenPosition(
    APlayerController* UsingController,
    const FVector2D& ScreenPosition)
{
    if (!bRuneDrawActive || !UsingController)
    {
        return;
    }

    FVector2D UV = FVector2D::ZeroVector;
    if (!ResolveDrawUVFromScreenPosition(UsingController, ScreenPosition, UV))
    {
        return;
    }

    AddDrawPoint(UV);

    if (bEnableHiddenNodeRecognition)
    {
        TryAppendRecognizedNode(ResolveHiddenNodeFromUV(UV));
    }
}

TArray<int32> APickupActorAAARuneCanvasInstrument::EndRuneDraw(APlayerController* UsingController)
{
    (void)UsingController;

    if (!bRuneDrawActive)
    {
        return RecognizedNodeSequence;
    }

    bRuneDrawActive = false;
    SolvedPatternId = NAME_None;
    bPatternSolved = TryResolveAcceptedPattern(RecognizedNodeSequence, SolvedPatternId);

    ReceiveRuneCanvasDrawStateChanged(DrawnUVPoints, false);

    if (bPatternSolved)
    {
        ReceiveRuneCanvasPatternSolved(SolvedPatternId, UsingController ? UsingController->GetPawn() : nullptr);
    }

    return RecognizedNodeSequence;
}

void APickupActorAAARuneCanvasInstrument::ResetRuneState()
{
    bRuneDrawActive = false;
    bPatternSolved = false;
    SolvedPatternId = NAME_None;
    DrawnUVPoints.Reset();
    RecognizedNodeSequence.Reset();
    ClearDrawTexture();
    ReceiveRuneCanvasDrawStateChanged(DrawnUVPoints, false);
    ReceiveRuneCanvasNodeSequenceChanged(RecognizedNodeSequence, INDEX_NONE);
}

void APickupActorAAARuneCanvasInstrument::ClearDrawTexture()
{
    if (EnsureDrawResources() && DrawRenderTarget)
    {
        UKismetRenderingLibrary::ClearRenderTarget2D(this, DrawRenderTarget, DrawTextureClearColor);
    }
}

void APickupActorAAARuneCanvasInstrument::ReinitializeDrawResources()
{
    DrawRenderTarget = nullptr;
    CardDynamicMaterial = nullptr;
    EnsureDrawResources();
    ClearDrawTexture();
}

bool APickupActorAAARuneCanvasInstrument::GetPreferredDrawStartScreenPosition(
    APlayerController* PC,
    FVector2D& OutScreenPosition) const
{
    OutScreenPosition = FVector2D::ZeroVector;
    return PC && DrawSurfaceComponent
        && PC->ProjectWorldLocationToScreen(DrawSurfaceComponent->GetComponentLocation(), OutScreenPosition, true);
}

bool APickupActorAAARuneCanvasInstrument::EnsureDrawResources()
{
    if (!DrawRenderTarget)
    {
        const int32 SafeResolution = FMath::Clamp(DrawTextureResolution, 64, 4096);
        DrawRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("RuneCanvasDrawRenderTarget"));
        if (!DrawRenderTarget)
        {
            return false;
        }

        DrawRenderTarget->ClearColor = DrawTextureClearColor;
        DrawRenderTarget->InitAutoFormat(SafeResolution, SafeResolution);
        DrawRenderTarget->UpdateResourceImmediate(true);
        UKismetRenderingLibrary::ClearRenderTarget2D(this, DrawRenderTarget, DrawTextureClearColor);
    }

    if (!CardDynamicMaterial && MeshComponent)
    {
        CardDynamicMaterial = MeshComponent->CreateDynamicMaterialInstance(
            FMath::Max(0, CardMaterialSlotIndex),
            CardBaseMaterial.Get());
    }

    if (CardDynamicMaterial && DrawRenderTarget && !DrawTextureParameterName.IsNone())
    {
        CardDynamicMaterial->SetTextureParameterValue(DrawTextureParameterName, DrawRenderTarget);
    }

    return DrawRenderTarget != nullptr;
}

bool APickupActorAAARuneCanvasInstrument::ResolveDrawUVFromScreenPosition(
    APlayerController* UsingController,
    const FVector2D& ScreenPosition,
    FVector2D& OutUV) const
{
    OutUV = FVector2D::ZeroVector;
    if (!UsingController || !DrawSurfaceComponent)
    {
        return false;
    }

    FVector RayOrigin = FVector::ZeroVector;
    FVector RayDirection = FVector::ZeroVector;
    if (!UsingController->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, RayOrigin, RayDirection))
    {
        return false;
    }

    const FTransform SurfaceTransform = DrawSurfaceComponent->GetComponentTransform();
    const FVector PlaneOrigin = SurfaceTransform.GetLocation();
    const FVector PlaneNormal = SurfaceTransform.GetUnitAxis(EAxis::Z);
    const float Denominator = FVector::DotProduct(RayDirection, PlaneNormal);

    if (FMath::IsNearlyZero(Denominator))
    {
        return false;
    }

    const float DistanceAlongRay = FVector::DotProduct(PlaneOrigin - RayOrigin, PlaneNormal) / Denominator;
    if (DistanceAlongRay < 0.f)
    {
        return false;
    }

    const FVector LocalHitLocation = SurfaceTransform.InverseTransformPosition(RayOrigin + RayDirection * DistanceAlongRay);
    const float SafeWidth = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.X);
    const float SafeHeight = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.Y);

    const float U = (LocalHitLocation.Y / SafeWidth) + 0.5f;
    const float RawW = (LocalHitLocation.X / SafeHeight) + 0.5f;
    const float V = bInvertDrawW ? 1.f - RawW : RawW;

    if (U < 0.f || U > 1.f || V < 0.f || V > 1.f)
    {
        return false;
    }

    OutUV = FVector2D(U, V);
    return true;
}

void APickupActorAAARuneCanvasInstrument::AddDrawPoint(const FVector2D& UV)
{
    if (DrawnUVPoints.Num() > 0)
    {
        const FVector2D PreviousUV = DrawnUVPoints.Last();
        if (FVector2D::DistSquared(PreviousUV, UV) < FMath::Square(MinDrawPointDistanceUV))
        {
            return;
        }
        DrawStrokeSegment(PreviousUV, UV);
    }
    DrawnUVPoints.Add(UV);
    ReceiveRuneCanvasDrawStateChanged(DrawnUVPoints, bRuneDrawActive);
}

void APickupActorAAARuneCanvasInstrument::DrawStrokeSegment(const FVector2D& StartUV, const FVector2D& EndUV)
{
    if (!EnsureDrawResources() || !DrawRenderTarget)
    {
        return;
    }

    UCanvas* Canvas = nullptr;
    FVector2D CanvasSize = FVector2D::ZeroVector;
    FDrawToRenderTargetContext DrawContext;
    UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, DrawRenderTarget, Canvas, CanvasSize, DrawContext);

    if (Canvas)
    {
        const FVector2D StartPixel(StartUV.X * CanvasSize.X, StartUV.Y * CanvasSize.Y);
        const FVector2D EndPixel(EndUV.X * CanvasSize.X, EndUV.Y * CanvasSize.Y);
        Canvas->K2_DrawLine(StartPixel, EndPixel, StrokeThicknessPixels, StrokeColor);
    }

    UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, DrawContext);
}

int32 APickupActorAAARuneCanvasInstrument::ResolveHiddenNodeFromUV(const FVector2D& UV) const
{
    const int32 SafeRows = FMath::Max(1, HiddenNodeRows);
    const int32 SafeColumns = FMath::Max(1, HiddenNodeColumns);
    const float SafePadding = FMath::Clamp(HiddenNodeEdgePaddingUV, 0.f, 0.45f);
    const float Span = FMath::Max(UE_KINDA_SMALL_NUMBER, 1.f - SafePadding * 2.f);

    const float NormalizedColumn = (UV.X - SafePadding) / Span;
    const float NormalizedRow = (UV.Y - SafePadding) / Span;
    if (NormalizedColumn < 0.f || NormalizedColumn > 1.f || NormalizedRow < 0.f || NormalizedRow > 1.f)
    {
        return INDEX_NONE;
    }

    const int32 ColumnIndex = SafeColumns == 1
        ? 0
        : FMath::RoundToInt(NormalizedColumn * static_cast<float>(SafeColumns - 1));
    const int32 RowIndex = SafeRows == 1
        ? 0
        : FMath::RoundToInt(NormalizedRow * static_cast<float>(SafeRows - 1));

    const FVector2D NodeUV(
        SafeColumns == 1 ? 0.5f : SafePadding + (static_cast<float>(ColumnIndex) / static_cast<float>(SafeColumns - 1)) * Span,
        SafeRows == 1 ? 0.5f : SafePadding + (static_cast<float>(RowIndex) / static_cast<float>(SafeRows - 1)) * Span);

    if (FVector2D::DistSquared(UV, NodeUV) > FMath::Square(HiddenNodeHitRadiusUV))
    {
        return INDEX_NONE;
    }

    return RowIndex * SafeColumns + ColumnIndex + 1;
}

void APickupActorAAARuneCanvasInstrument::TryAppendRecognizedNode(int32 NodeId)
{
    if (NodeId == INDEX_NONE)
    {
        return;
    }

    if (RecognizedNodeSequence.Num() > 0 && RecognizedNodeSequence.Last() == NodeId)
    {
        return;
    }

    if (!bAllowNodeRepeat && RecognizedNodeSequence.Contains(NodeId))
    {
        return;
    }

    RecognizedNodeSequence.Add(NodeId);
    ReceiveRuneCanvasNodeSequenceChanged(RecognizedNodeSequence, NodeId);
}

bool APickupActorAAARuneCanvasInstrument::TryResolveAcceptedPattern(
    const TArray<int32>& NodeSequence,
    FName& OutPatternId) const
{
    OutPatternId = NAME_None;

    for (const FRuneCanvasPattern& Pattern : AcceptedPatterns)
    {
        if (Pattern.NodeSequence.Num() == 0)
        {
            continue;
        }

        if (Pattern.NodeSequence == NodeSequence)
        {
            OutPatternId = Pattern.PatternId;
            return true;
        }
    }

    return false;
}