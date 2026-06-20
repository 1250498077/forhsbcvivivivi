#include "PickupActorAAARuneCanvasInstrument.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
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
    bCanConnectNextDrawPoint = false;
    SolvedPatternId = NAME_None;
    DrawnUVPoints.Reset();
    RecognizedNodeSequence.Reset();
    EnableMouseTraceCollisionIfNeeded();

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
        bCanConnectNextDrawPoint = false;
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
    bCanConnectNextDrawPoint = false;
    RestoreHeldCollisionAfterMouseTrace();
    SolvedPatternId = NAME_None;
    bPatternSolved = TryResolveAcceptedPattern(RecognizedNodeSequence, SolvedPatternId);
    LogRecognizedNodeSequence(TEXT("EndRuneDraw"));

    ReceiveRuneCanvasDrawStateChanged(DrawnUVPoints, false);

    return RecognizedNodeSequence;
}

void APickupActorAAARuneCanvasInstrument::ResetRuneState()
{
    bRuneDrawActive = false;
    bCanConnectNextDrawPoint = false;
    RestoreHeldCollisionAfterMouseTrace();
    bPatternSolved = false;
    SolvedPatternId = NAME_None;
    DrawnUVPoints.Reset();
    RecognizedNodeSequence.Reset();
    ClearDrawTexture();
    ReceiveRuneCanvasDrawStateChanged(DrawnUVPoints, false);
    ReceiveRuneCanvasNodeSequenceChanged(RecognizedNodeSequence, INDEX_NONE);
}

void APickupActorAAARuneCanvasInstrument::CommitRuneSequenceAuthority(
    const TArray<int32>& NodeSequence,
    AActor* SolvingActor)
{
    if (!HasAuthority())
    {
        return;
    }

    bRuneDrawActive = false;
    bCanConnectNextDrawPoint = false;
    RecognizedNodeSequence = NodeSequence;
    SolvedPatternId = NAME_None;
    bPatternSolved = TryResolveAcceptedPattern(RecognizedNodeSequence, SolvedPatternId);
    LogRecognizedNodeSequence(TEXT("CommitRuneSequenceAuthority"));

    ReceiveRuneCanvasDrawStateChanged(DrawnUVPoints, false);
    ReceiveRuneCanvasNodeSequenceChanged(
    RecognizedNodeSequence,
    RecognizedNodeSequence.Num() > 0 ? RecognizedNodeSequence.Last() : INDEX_NONE);

    if (bPatternSolved)
    {
        ReceiveRuneCanvasPatternSolved(SolvedPatternId, SolvingActor);
    }
}

void APickupActorAAARuneCanvasInstrument::ClearDrawTexture()
{
    if (EnsureDrawResources() && DrawRenderTarget)
    {
        UKismetRenderingLibrary::ClearRenderTarget2D(this, DrawRenderTarget, DrawTextureClearColor);
        DrawRecognitionGridGuide();
    }
}

void APickupActorAAARuneCanvasInstrument::ReinitializeDrawResources()
{
    DrawRenderTarget = nullptr;
    CardDynamicMaterial = nullptr;
    EnsureDrawResources();
    ClearDrawTexture();
}

int32 APickupActorAAARuneCanvasInstrument::GetHiddenNodeId(int32 Row, int32 Column) const
{
    const int32 SafeRows = FMath::Max(1, HiddenNodeRows);
    const int32 SafeColumns = FMath::Max(1, HiddenNodeColumns);
    if (Row < 0 || Row >= SafeRows || Column < 0 || Column >= SafeColumns)
    {
        return INDEX_NONE;
    }

    return Row * SafeColumns + Column + 1;
}

void APickupActorAAARuneCanvasInstrument::EnableMouseTraceCollisionIfNeeded()
{
    if (!bUseMouseTraceCollisionUV || !MeshComponent)
    {
        return;
    }

    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    MeshComponent->SetGenerateOverlapEvents(false);
}

void APickupActorAAARuneCanvasInstrument::RestoreHeldCollisionAfterMouseTrace()
{
    if (!bUseMouseTraceCollisionUV || !bIsHeldByPlayer || !MeshComponent)
    {
        return;
    }

    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    MeshComponent->SetGenerateOverlapEvents(false);
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
        int32 TargetWidth = SafeResolution;
        int32 TargetHeight = SafeResolution;

        if (bMatchDrawTextureAspectToSurface)
        {
            const float SafeSurfaceWidth = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.X);
            const float SafeSurfaceHeight = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.Y);

            if (SafeSurfaceWidth >= SafeSurfaceHeight)
            {
                TargetHeight = FMath::Clamp(
                    FMath::RoundToInt(static_cast<float>(SafeResolution) * (SafeSurfaceHeight / SafeSurfaceWidth)),
                    64,
                    4096);
            }
            else
            {
                TargetWidth = FMath::Clamp(
                    FMath::RoundToInt(static_cast<float>(SafeResolution) * (SafeSurfaceWidth / SafeSurfaceHeight)),
                    64,
                    4096);
            }
        }

        DrawRenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("RuneCanvasDrawRenderTarget"));
        if (!DrawRenderTarget)
        {
            return false;
        }

        DrawRenderTarget->ClearColor = DrawTextureClearColor;
        DrawRenderTarget->InitAutoFormat(TargetWidth, TargetHeight);
        DrawRenderTarget->UpdateResourceImmediate(true);
        UKismetRenderingLibrary::ClearRenderTarget2D(this, DrawRenderTarget, DrawTextureClearColor);
        DrawRecognitionGridGuide();
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

bool APickupActorAAARuneCanvasInstrument::IsUVInsideRecognitionArea(const FVector2D& UV) const
{
    const FVector2D SafeAreaSize(
        FMath::Clamp(RecognitionAreaSizeUV.X, 0.01f, 1.f),
        FMath::Clamp(RecognitionAreaSizeUV.Y, 0.01f, 1.f));
    const FVector2D AreaMin = RecognitionAreaCenterUV - SafeAreaSize * 0.5f;
    const FVector2D AreaMax = RecognitionAreaCenterUV + SafeAreaSize * 0.5f;

    return UV.X >= AreaMin.X && UV.X <= AreaMax.X && UV.Y >= AreaMin.Y && UV.Y <= AreaMax.Y;
}

FVector2D APickupActorAAARuneCanvasInstrument::NormalizeUVToRecognitionArea(const FVector2D& UV) const
{
    const FVector2D SafeAreaSize(
        FMath::Clamp(RecognitionAreaSizeUV.X, 0.01f, 1.f),
        FMath::Clamp(RecognitionAreaSizeUV.Y, 0.01f, 1.f));
    const FVector2D AreaMin = RecognitionAreaCenterUV - SafeAreaSize * 0.5f;

    return FVector2D(
        (UV.X - AreaMin.X) / SafeAreaSize.X,
        (UV.Y - AreaMin.Y) / SafeAreaSize.Y);
}

FVector2D APickupActorAAARuneCanvasInstrument::DenormalizeRecognitionAreaUV(const FVector2D& AreaUV) const
{
    const FVector2D SafeAreaSize(
        FMath::Clamp(RecognitionAreaSizeUV.X, 0.01f, 1.f),
        FMath::Clamp(RecognitionAreaSizeUV.Y, 0.01f, 1.f));
    const FVector2D AreaMin = RecognitionAreaCenterUV - SafeAreaSize * 0.5f;

    return AreaMin + FVector2D(AreaUV.X * SafeAreaSize.X, AreaUV.Y * SafeAreaSize.Y);
}

void APickupActorAAARuneCanvasInstrument::DrawRecognitionGridGuide()
{
    if (!bDrawRecognitionGridGuide || !DrawRenderTarget)
    {
        return;
    }

    UCanvas* Canvas = nullptr;
    FVector2D CanvasSize = FVector2D::ZeroVector;
    FDrawToRenderTargetContext DrawContext;
    UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, DrawRenderTarget, Canvas, CanvasSize, DrawContext);

    if (Canvas)
    {
        const FVector2D AreaMin = DenormalizeRecognitionAreaUV(FVector2D::ZeroVector);
        const FVector2D AreaMax = DenormalizeRecognitionAreaUV(FVector2D(1.f, 1.f));
        const FVector2D TopLeft(AreaMin.X * CanvasSize.X, AreaMin.Y * CanvasSize.Y);
        const FVector2D TopRight(AreaMax.X * CanvasSize.X, AreaMin.Y * CanvasSize.Y);
        const FVector2D BottomLeft(AreaMin.X * CanvasSize.X, AreaMax.Y * CanvasSize.Y);
        const FVector2D BottomRight(AreaMax.X * CanvasSize.X, AreaMax.Y * CanvasSize.Y);

        Canvas->K2_DrawLine(TopLeft, TopRight, RecognitionGridGuideThicknessPixels, RecognitionGridGuideColor);
        Canvas->K2_DrawLine(TopRight, BottomRight, RecognitionGridGuideThicknessPixels, RecognitionGridGuideColor);
        Canvas->K2_DrawLine(BottomRight, BottomLeft, RecognitionGridGuideThicknessPixels, RecognitionGridGuideColor);
        Canvas->K2_DrawLine(BottomLeft, TopLeft, RecognitionGridGuideThicknessPixels, RecognitionGridGuideColor);

        const int32 SafeRows = FMath::Max(1, HiddenNodeRows);
        const int32 SafeColumns = FMath::Max(1, HiddenNodeColumns);
        const float SafePadding = FMath::Clamp(HiddenNodeEdgePaddingUV, 0.f, 0.45f);
        const float Span = FMath::Max(UE_KINDA_SMALL_NUMBER, 1.f - SafePadding * 2.f);
        const float CrossHalfSizePixels = FMath::Max(4.f, RecognitionGridGuideThicknessPixels * 3.f);

        for (int32 RowIndex = 0; RowIndex < SafeRows; ++RowIndex)
        {
            for (int32 ColumnIndex = 0; ColumnIndex < SafeColumns; ++ColumnIndex)
            {
                const FVector2D AreaNodeUV(
                    SafeColumns == 1 ? 0.5f : SafePadding + (static_cast<float>(ColumnIndex) / static_cast<float>(SafeColumns - 1)) * Span,
                    SafeRows == 1 ? 0.5f : SafePadding + (static_cast<float>(RowIndex) / static_cast<float>(SafeRows - 1)) * Span);
                const FVector2D NodeUV = DenormalizeRecognitionAreaUV(AreaNodeUV);
                const FVector2D NodePixel(NodeUV.X * CanvasSize.X, NodeUV.Y * CanvasSize.Y);

                Canvas->K2_DrawLine(
                    NodePixel - FVector2D(CrossHalfSizePixels, 0.f),
                    NodePixel + FVector2D(CrossHalfSizePixels, 0.f),
                    RecognitionGridGuideThicknessPixels,
                    RecognitionGridGuideColor);
                Canvas->K2_DrawLine(
                    NodePixel - FVector2D(0.f, CrossHalfSizePixels),
                    NodePixel + FVector2D(0.f, CrossHalfSizePixels),
                    RecognitionGridGuideThicknessPixels,
                    RecognitionGridGuideColor);
            }
        }
    }

    UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, DrawContext);
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

    if (bUseMouseTraceCollisionUV && ResolveDrawUVFromMouseTrace(UsingController, ScreenPosition, OutUV))
    {
        return true;
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

    return ResolveDrawUVFromWorldLocation(RayOrigin + RayDirection * DistanceAlongRay, OutUV);
}
bool APickupActorAAARuneCanvasInstrument::ResolveDrawUVFromWorldLocation(
    const FVector& WorldLocation,
    FVector2D& OutUV) const
{
    OutUV = FVector2D::ZeroVector;
    if (!DrawSurfaceComponent)
    {
        return false;
    }

    const FTransform SurfaceTransform = DrawSurfaceComponent->GetComponentTransform();
    const FVector LocalHitLocation = SurfaceTransform.InverseTransformPosition(WorldLocation);

    const float SafeWidth = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.X);
    const float SafeHeight = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.Y);

    const float SurfaceHorizontal = bSwapDrawSurfaceAxes ? LocalHitLocation.X : LocalHitLocation.Y;
    const float SurfaceVertical = bSwapDrawSurfaceAxes ? LocalHitLocation.Y : LocalHitLocation.X;
    const float RawU = (SurfaceHorizontal / SafeWidth) + 0.5f;
    const float RawV = (SurfaceVertical / SafeHeight) + 0.5f;
    const float MappedU = bInvertDrawU ? 1.f - RawU : RawU;
    const float MappedV = bInvertDrawW ? 1.f - RawV : RawV;
    const float U = 0.5f + (MappedU - 0.5f) * DrawUVSensitivity.X;
    const float V = 0.5f + (MappedV - 0.5f) * DrawUVSensitivity.Y;

    if (U < 0.f || U > 1.f || V < 0.f || V > 1.f)
    {
        return false;
    }

    if (bRestrictDrawingToRecognitionArea && !IsUVInsideRecognitionArea(FVector2D(U, V)))
    {
        return false;
    }

    OutUV = FVector2D(U, V);
    return true;
}

bool APickupActorAAARuneCanvasInstrument::ResolveDrawUVFromMouseTrace(
    APlayerController* UsingController,
    const FVector2D& ScreenPosition,
    FVector2D& OutUV) const
{
    OutUV = FVector2D::ZeroVector;
    if (!UsingController || !MeshComponent)
    {
        return false;
    }

    FVector RayOrigin = FVector::ZeroVector;
    FVector RayDirection = FVector::ZeroVector;
    if (!UsingController->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, RayOrigin, RayDirection))
    {
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RuneCanvasMouseTrace), true);
    QueryParams.bReturnFaceIndex = true;
    if (APawn* Pawn = UsingController->GetPawn())
    {
        QueryParams.AddIgnoredActor(Pawn);
    }

    TArray<FHitResult> HitResults;
    const FVector RayEnd = RayOrigin + RayDirection.GetSafeNormal() * FMath::Max(100.f, MouseTraceDistance);
    if (!World->LineTraceMultiByChannel(HitResults, RayOrigin, RayEnd, ECC_Visibility, QueryParams))
    {
        return false;
    }

    for (const FHitResult& HitResult : HitResults)
    {
        const UActorComponent* HitComponent = HitResult.GetComponent();
        // 确保击中点属于当前 Actor 或其组件
        if (HitResult.GetActor() != this && (!HitComponent || HitComponent->GetOwner() != this))
        {
            continue;
        }

        FVector2D SurfaceUV = FVector2D::ZeroVector;
        const FVector SurfaceNormal = DrawSurfaceComponent->GetComponentTransform().GetUnitAxis(EAxis::Z);
        const float SurfaceNormalDot = FMath::Abs(FVector::DotProduct(HitResult.ImpactNormal.GetSafeNormal(), SurfaceNormal));
        if (SurfaceNormalDot < 0.5f || !ResolveDrawUVFromWorldLocation(HitResult.ImpactPoint, SurfaceUV))
        {
            continue;
        }

        FVector2D CollisionUV = FVector2D::ZeroVector;
        if (UGameplayStatics::FindCollisionUV(HitResult, FMath::Max(0, MouseTraceUVChannel), CollisionUV))
        {
            if (CollisionUV.X < 0.f || CollisionUV.X > 1.f || CollisionUV.Y < 0.f || CollisionUV.Y > 1.f)
            {
                continue;
            }

            if (bRestrictDrawingToRecognitionArea && !IsUVInsideRecognitionArea(CollisionUV))
            {
                continue;
            }

            OutUV = CollisionUV;
            return true;
        }
    }

    return false;
}

void APickupActorAAARuneCanvasInstrument::AddDrawPoint(const FVector2D& UV)
{
    if (DrawnUVPoints.Num() > 0 && bCanConnectNextDrawPoint)
    {
        const FVector2D PreviousUV = DrawnUVPoints.Last();
        if (FVector2D::DistSquared(PreviousUV, UV) < FMath::Square(MinDrawPointDistanceUV))
        {
            return;
        }
        DrawStrokeSegment(PreviousUV, UV);
    }
    DrawnUVPoints.Add(UV);
    bCanConnectNextDrawPoint = true;
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
        float AdjustedStrokeThicknessPixels = StrokeThicknessPixels;
        if (bCompensateStrokeAspectRatio)
        {
            const FVector2D SegmentDirection = (EndUV - StartUV).GetSafeNormal();
            const FVector2D SegmentNormal(-SegmentDirection.Y, SegmentDirection.X);
            const float SafeWidth = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.X);
            const float SafeHeight = FMath::Max(UE_KINDA_SMALL_NUMBER, DrawSurfaceSize.Y);
            const float NormalPhysicalScale = FMath::Sqrt(
                FMath::Square(SegmentNormal.X * SafeWidth) + FMath::Square(SegmentNormal.Y * SafeHeight));
            const float TargetPhysicalScale = FMath::Min(SafeWidth, SafeHeight);
            AdjustedStrokeThicknessPixels *= TargetPhysicalScale / FMath::Max(UE_KINDA_SMALL_NUMBER, NormalPhysicalScale);
        }
        Canvas->K2_DrawLine(StartPixel, EndPixel, FMath::Max(1.f, AdjustedStrokeThicknessPixels), StrokeColor);
    }

    UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, DrawContext);
}

int32 APickupActorAAARuneCanvasInstrument::ResolveHiddenNodeFromUV(const FVector2D& UV) const
{
    if (!IsUVInsideRecognitionArea(UV))
    {
        return INDEX_NONE;
    }

    const FVector2D AreaUV = NormalizeUVToRecognitionArea(UV);
    const int32 SafeRows = FMath::Max(1, HiddenNodeRows);
    const int32 SafeColumns = FMath::Max(1, HiddenNodeColumns);
    const float SafePadding = FMath::Clamp(HiddenNodeEdgePaddingUV, 0.f, 0.45f);
    const float Span = FMath::Max(UE_KINDA_SMALL_NUMBER, 1.f - SafePadding * 2.f);

    const float NormalizedColumn = (AreaUV.X - SafePadding) / Span;
    const float NormalizedRow = (AreaUV.Y - SafePadding) / Span;
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

    const FVector2D AreaNodeUV(
        SafeColumns == 1 ? 0.5f : SafePadding + (static_cast<float>(ColumnIndex) / static_cast<float>(SafeColumns - 1)) * Span,
        SafeRows == 1 ? 0.5f : SafePadding + (static_cast<float>(RowIndex) / static_cast<float>(SafeRows - 1)) * Span);

    if (FVector2D::DistSquared(AreaUV, AreaNodeUV) > FMath::Square(HiddenNodeHitRadiusUV))
    {
        return INDEX_NONE;
    }

    return GetHiddenNodeId(RowIndex, ColumnIndex);
}

bool APickupActorAAARuneCanvasInstrument::GetHiddenNodeCoordinatesForId(
    int32 NodeId,
    int32& OutRow,
    int32& OutColumn) const
{
    OutRow = INDEX_NONE;
    OutColumn = INDEX_NONE;

    const int32 SafeRows = FMath::Max(1, HiddenNodeRows);
    const int32 SafeColumns = FMath::Max(1, HiddenNodeColumns);
    const int32 ZeroBasedNodeId = NodeId - 1;
    if (ZeroBasedNodeId < 0)
    {
        return false;
    }

    OutRow = ZeroBasedNodeId / SafeColumns;
    OutColumn = ZeroBasedNodeId % SafeColumns;
    return OutRow >= 0 && OutRow < SafeRows && OutColumn >= 0 && OutColumn < SafeColumns;
}

void APickupActorAAARuneCanvasInstrument::TryAppendRecognizedNode(int32 NodeId)
{
    if (NodeId == INDEX_NONE)
    {
        return;
    }

    if (RecognizedNodeSequence.Num() == 0 || !bInterpolateSkippedRecognitionNodes)
    {
        TryAppendSingleRecognizedNode(NodeId);
        return;
    }

    const int32 LastNodeId = RecognizedNodeSequence.Last();
    if (LastNodeId == NodeId)
    {
        return;
    }

    AppendInterpolatedNodesBetween(LastNodeId, NodeId);
}

bool APickupActorAAARuneCanvasInstrument::TryAppendSingleRecognizedNode(int32 NodeId)
{
    if (NodeId == INDEX_NONE)
    {
        return false;
    }

    if (RecognizedNodeSequence.Num() > 0 && RecognizedNodeSequence.Last() == NodeId)
    {
        return false;
    }

    if (!bAllowNodeRepeat && RecognizedNodeSequence.Contains(NodeId))
    {
        return false;
    }

    RecognizedNodeSequence.Add(NodeId);
    ReceiveRuneCanvasNodeSequenceChanged(RecognizedNodeSequence, NodeId);
    return true;
}

void APickupActorAAARuneCanvasInstrument::AppendInterpolatedNodesBetween(int32 FromNodeId, int32 ToNodeId)
{
    int32 FromRow = INDEX_NONE;
    int32 FromColumn = INDEX_NONE;
    int32 ToRow = INDEX_NONE;
    int32 ToColumn = INDEX_NONE;

    if (!GetHiddenNodeCoordinatesForId(FromNodeId, FromRow, FromColumn)
        || !GetHiddenNodeCoordinatesForId(ToNodeId, ToRow, ToColumn))
    {
        TryAppendSingleRecognizedNode(ToNodeId);
        return;
    }

    int32 CurrentColumn = FromColumn;
    int32 CurrentRow = FromRow;
    const int32 DeltaColumn = FMath::Abs(ToColumn - FromColumn);
    const int32 DeltaRow = FMath::Abs(ToRow - FromRow);
    const int32 StepColumn = FromColumn < ToColumn ? 1 : -1;
    const int32 StepRow = FromRow < ToRow ? 1 : -1;
    int32 Error = DeltaColumn - DeltaRow;

    while (!(CurrentColumn == ToColumn && CurrentRow == ToRow))
    {
        const int32 DoubleError = Error * 2;
        if (DoubleError > -DeltaRow)
        {
            Error -= DeltaRow;
            CurrentColumn += StepColumn;
        }
        if (DoubleError < DeltaColumn)
        {
            Error += DeltaColumn;
            CurrentRow += StepRow;
        }

        const int32 InterpolatedNodeId = GetHiddenNodeId(CurrentRow, CurrentColumn);
        if (InterpolatedNodeId != INDEX_NONE)
        {
            TryAppendSingleRecognizedNode(InterpolatedNodeId);
        }
    }
}

FString APickupActorAAARuneCanvasInstrument::BuildRecognizedNodeSequenceString() const
{
    FString Result;
    for (int32 Index = 0; Index < RecognizedNodeSequence.Num(); ++Index)
    {
        if (Index > 0)
        {
            Result += TEXT(", ");
        }
        Result += FString::FromInt(RecognizedNodeSequence[Index]);
    }
    return Result;
}

void APickupActorAAARuneCanvasInstrument::LogRecognizedNodeSequence(const TCHAR* Context) const
{
    if (!bLogRecognizedNodeSequence)
    {
        return;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("%s RuneCanvas %s nodes: [%s], Solved: %s, Pattern: %s"),
        *GetName(),
        Context,
        *BuildRecognizedNodeSequenceString(),
        bPatternSolved ? TEXT("true") : TEXT("false"),
        *SolvedPatternId.ToString());
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

        if (bRequireExactPatternMatch)
        {
            if (Pattern.NodeSequence == NodeSequence)
            {
                OutPatternId = Pattern.PatternId;
                return true;
            }
            continue;
        }

        if (NodeSequence.Num() < Pattern.NodeSequence.Num())
        {
            continue;
        }

        bool bMatches = true;
        for (int32 Index = 0; Index < Pattern.NodeSequence.Num(); ++Index)
        {
            if (NodeSequence[Index] != Pattern.NodeSequence[Index])
            {
                bMatches = false;
                break;
            }
        }

        if (bMatches)
        {
            OutPatternId = Pattern.PatternId;
            return true;
        }
    }

    return false;
}