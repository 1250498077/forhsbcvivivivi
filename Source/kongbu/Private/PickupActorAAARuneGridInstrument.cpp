#include "PickupActorAAARuneGridInstrument.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

APickupActorAAARuneGridInstrument::APickupActorAAARuneGridInstrument()
{
    PrimaryActorTick.bCanEverTick = false;

    HoldType = EHoldItemType::RuneGridInstrument;
    FP_SocketName = TEXT("RightSocket");
    FP_LocationOffset = FVector::ZeroVector;
    FP_RotationOffset = FRotator::ZeroRotator;
    TP_SocketName = TEXT("RightSocket");
    TP_LocationOffset = FVector::ZeroVector;
    TP_RotationOffset = FRotator::ZeroRotator;

    ItemMassKg = 2.0f;
    ItemThrowForceMultiplier = 0.85f;
    ItemLinearDamping = 0.12f;
    ItemAngularDamping = 0.45f;
    ItemThrowSpinRateDegrees = 850.f;

    Tags.Add(FName("RuneGridInstrument"));
    Tags.Add(FName("Pickup"));
    Tags.Add(FName("Rune"));

    GridRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("GridRootComponent"));
    GridRootComponent->SetupAttachment(VisualMeshRootComponent);

    GridLineRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("GridLineRootComponent"));
    GridLineRootComponent->SetupAttachment(GridRootComponent);

    DrawStartAnchorComponent = CreateDefaultSubobject<USceneComponent>(TEXT("DrawStartAnchorComponent"));
    DrawStartAnchorComponent->SetupAttachment(GridRootComponent);
    DrawStartAnchorComponent->SetRelativeLocation(FVector::ZeroVector);

    DrawStartAnchorMarkerComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("DrawStartAnchorMarkerComponent"));
    DrawStartAnchorMarkerComponent->SetupAttachment(DrawStartAnchorComponent);
    DrawStartAnchorMarkerComponent->ArrowColor = FColor(64, 220, 255);
    DrawStartAnchorMarkerComponent->ArrowSize = 0.18f;
    DrawStartAnchorMarkerComponent->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
    DrawStartAnchorMarkerComponent->SetHiddenInGame(true);
    DrawStartAnchorMarkerComponent->bTreatAsASprite = true;
    DrawStartAnchorMarkerComponent->bIsScreenSizeScaled = true;
    DrawStartAnchorMarkerComponent->SetUsingAbsoluteScale(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultPlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (DefaultPlaneMesh.Succeeded())
    {
        DefaultCellMesh = DefaultPlaneMesh.Object;
        CellMesh = DefaultPlaneMesh.Object;
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultCylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (DefaultCylinderMesh.Succeeded())
    {
        DefaultLineMesh = DefaultCylinderMesh.Object;
        LineSegmentMesh = DefaultCylinderMesh.Object;
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultVisualMaterial(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (DefaultVisualMaterial.Succeeded())
    {
        DefaultVisualBaseMaterial = DefaultVisualMaterial.Object;
    }

    ApplyVisualPreset();
}

void APickupActorAAARuneGridInstrument::BeginPlay()
{
    Super::BeginPlay();
    RefreshGeneratedDefaultMaterials();
    RebuildGrid();
    ApplySequenceToVisuals(CommittedSequence, bPatternSolved, bRuneDrawActive);
}


void APickupActorAAARuneGridInstrument::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // 注意：原截图中此处有语法错误 (if (T...))，已修正为标准的成员变量检查
    if (bAutoApplyPresetOnConstruction)
    {
        ApplyVisualPreset();
    }

    RefreshGeneratedDefaultMaterials();
    RebuildGrid();
    ApplySequenceToVisuals(CommittedSequence, bPatternSolved, bRuneDrawActive);
}

void APickupActorAAARuneGridInstrument::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearLineMeshes();
    ClearGeneratedGridComponents();
    OriginalCellMaterials.Reset();
    CellComponentById.Reset();
    CurrentDrawSequence.Reset();
    CommittedSequence.Reset();

    Super::EndPlay(EndPlayReason);
}

void APickupActorAAARuneGridInstrument::OnPickedUp()
{
    Super::OnPickedUp();

    // 玩家把已经放下的矩阵法器重新按 E 拿起时,
    // 清空上一次绘制/解谜留下的高亮与序列。
    ResetRuneState();
}

#if WITH_EDITOR
void APickupActorAAARuneGridInstrument::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.Property
        ? PropertyChangedEvent.Property->GetFName()
        : NAME_None;

    if (PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, VisualPreset))
    {
        if (bAutoApplyPresetOnConstruction && VisualPreset != ERuneGridVisualPreset::Custom)
        {
            ApplyVisualPreset();
            RebuildGrid();
        }
        return;
    }

    if (PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, bAutoApplyPresetOnConstruction))
    {
        return;
    }

    const bool bIsManualTuningProperty =
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, GridRows) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, GridColumns) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, CellSpacing) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, CellScale) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, CellRelativeRotation) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, bCenterGridAroundOrigin) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, CellSelectionScreenRadius) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, bUseProjectedCellBoundsSelection) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, CellScreenBoundsPadding) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, bAllowCellRepeat) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, bRequireExactPatternMatch) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, bEnableLineRendering) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, LineThickness) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, bUseDrawStartAnchor) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, IdleCellTint) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, HoveredCellTint) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, TracedCellTint) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, SolvedCellTint) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, IdleGlowStrength) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, HoveredGlowStrength) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, TracedGlowStrength) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, SolvedGlowStrength) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, LineTint) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(APickupActorAAARuneGridInstrument, LineGlowStrength);

    if (bIsManualTuningProperty && VisualPreset != ERuneGridVisualPreset::Custom)
    {
        VisualPreset = ERuneGridVisualPreset::Custom;
    }
}
#endif

void APickupActorAAARuneGridInstrument::ApplyVisualPreset()
{
    switch (VisualPreset)
    {
    case ERuneGridVisualPreset::Custom:
        break;

    case ERuneGridVisualPreset::RuneMirror:
        GridRows = 8;
        GridColumns = 8;
        CellSpacing = FVector2D(8.0f, 8.0f);
        CellScale = FVector(0.075f, 0.075f, 0.075f);
        CellRelativeRotation = FRotator::ZeroRotator;
        bCenterGridAroundOrigin = true;
        CellSelectionScreenRadius = 18.f;
        CellScreenBoundsPadding = 14.f;
        bAllowCellRepeat = false;
        bRequireExactPatternMatch = true;
        bEnableLineRendering = true;
        LineThickness = 0.05f;

        IdleCellTint = FLinearColor(0.07f, 0.12f, 0.18f, 1.0f);
        HoveredCellTint = FLinearColor(0.40f, 0.90f, 1.0f, 1.0f);
        TracedCellTint = FLinearColor(0.12f, 0.64f, 1.0f, 1.0f);
        SolvedCellTint = FLinearColor(0.95f, 0.95f, 1.0f, 1.0f);
        IdleGlowStrength = 0.25f;
        HoveredGlowStrength = 3.6f;
        TracedGlowStrength = 2.2f;
        SolvedGlowStrength = 4.8f;
        LineTint = FLinearColor(0.30f, 0.85f, 1.0f, 1.0f);
        LineGlowStrength = 3.2f;
        break;

    case ERuneGridVisualPreset::WoodenBoard:
        GridRows = 6;
        GridColumns = 6;
        CellSpacing = FVector2D(10.5f, 10.5f);
        CellScale = FVector(0.095f, 0.095f, 0.095f);
        CellRelativeRotation = FRotator::ZeroRotator;
        bCenterGridAroundOrigin = true;
        CellSelectionScreenRadius = 22.f;
        CellScreenBoundsPadding = 16.f;
        bAllowCellRepeat = false;
        bRequireExactPatternMatch = true;
        bEnableLineRendering = true;
        LineThickness = 0.07f;

        IdleCellTint = FLinearColor(0.20f, 0.12f, 0.06f, 1.0f);
        HoveredCellTint = FLinearColor(0.95f, 0.62f, 0.18f, 1.0f);
        TracedCellTint = FLinearColor(0.82f, 0.36f, 0.12f, 1.0f);
        SolvedCellTint = FLinearColor(1.0f, 0.86f, 0.42f, 1.0f);
        IdleGlowStrength = 0.08f;
        HoveredGlowStrength = 1.8f;
        TracedGlowStrength = 1.0f;
        SolvedGlowStrength = 2.4f;
        LineTint = FLinearColor(0.98f, 0.52f, 0.18f, 1.0f);
        LineGlowStrength = 1.6f;
        break;

    case ERuneGridVisualPreset::GroundArray:
        GridRows = 10;
        GridColumns = 10;
        CellSpacing = FVector2D(22.0f, 22.0f);
        CellScale = FVector(0.19f, 0.19f, 0.19f);
        CellRelativeRotation = FRotator::ZeroRotator;
        bCenterGridAroundOrigin = true;
        CellSelectionScreenRadius = 30.f;
        CellScreenBoundsPadding = 18.f;
        bAllowCellRepeat = false;
        bRequireExactPatternMatch = false;
        bEnableLineRendering = true;
        LineThickness = 0.16f;

        IdleCellTint = FLinearColor(0.10f, 0.02f, 0.02f, 1.0f);
        HoveredCellTint = FLinearColor(1.0f, 0.18f, 0.10f, 1.0f);
        TracedCellTint = FLinearColor(0.86f, 0.08f, 0.06f, 1.0f);
        SolvedCellTint = FLinearColor(1.0f, 0.66f, 0.18f, 1.0f);
        IdleGlowStrength = 0.15f;
        HoveredGlowStrength = 3.4f;
        TracedGlowStrength = 2.2f;
        SolvedGlowStrength = 5.0f;
        LineTint = FLinearColor(1.0f, 0.30f, 0.10f, 1.0f);
        LineGlowStrength = 3.8f;
        break;

    default:
        break;
    }

    RefreshGeneratedDefaultMaterials();
}

bool APickupActorAAARuneGridInstrument::BeginRuneDraw(APlayerController* UsingController)
{
    if (!UsingController)
    {
        return false;
    }

    if (CellComponentById.Num() == 0)
    {
        RebuildGrid();
    }

    if (CellComponentById.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s rune grid draw start failed: no generated cells"), *GetName());
        return false;
    }

    bRuneDrawActive = true;
    bPatternSolved = false;
    SolvedPatternId = NAME_None;
    CurrentDrawSequence.Reset();
    CommittedSequence.Reset();
    SetHoveredCellId(INDEX_NONE, true);
    ApplySequenceToVisuals(CurrentDrawSequence, false, true);
    return true;
}

void APickupActorAAARuneGridInstrument::UpdateRuneDrawFromScreenPosition(
    APlayerController* UsingController,
    const FVector2D& ScreenPosition)
{
    if (!bRuneDrawActive || !UsingController)
    {
        return;
    }

    const int32 NewHoveredCellId = ResolveHoveredCellFromScreenPosition(UsingController, ScreenPosition);
    SetHoveredCellId(NewHoveredCellId);
    const int32 PreviousSequenceNum = CurrentDrawSequence.Num();
    if (TryAppendResolvedCell(NewHoveredCellId))
    {
        AppendLineMeshes(CurrentDrawSequence, PreviousSequenceNum);
        ReceiveRuneGridVisualStateChanged(CurrentDrawSequence, false, true);
    }
    
}

TArray<int32> APickupActorAAARuneGridInstrument::EndRuneDraw(APlayerController* UsingController)
{
    (void)UsingController;

    if (!bRuneDrawActive)
    {
        return CommittedSequence;
    }

    bRuneDrawActive = false;
    SetHoveredCellId(INDEX_NONE, true);
    CommittedSequence = CurrentDrawSequence;
    CurrentDrawSequence.Reset();

    SolvedPatternId = NAME_None;
    bPatternSolved = TryResolveAcceptedPattern(CommittedSequence, SolvedPatternId);
    ApplySequenceToVisuals(CommittedSequence, bPatternSolved, false);

    return CommittedSequence;
}

void APickupActorAAARuneGridInstrument::ResetRuneState()
{
    bRuneDrawActive = false;
    bPatternSolved = false;
    SolvedPatternId = NAME_None;
    CurrentDrawSequence.Reset();
    CommittedSequence.Reset();
    SetHoveredCellId(INDEX_NONE, true);
    ApplySequenceToVisuals(CommittedSequence, false, false);
}

void APickupActorAAARuneGridInstrument::CommitRuneSequenceAuthority(
    const TArray<int32>& CellSequence,
    AActor* SolvingActor)
{
    if (!HasAuthority())
    {
        return;
    }

    CommittedSequence = CellSequence;
    SolvedPatternId = NAME_None;
    bPatternSolved = TryResolveAcceptedPattern(CommittedSequence, SolvedPatternId);
    ApplySequenceToVisuals(CommittedSequence, bPatternSolved, false);

    if (bPatternSolved)
    {
        ReceiveRuneGridPatternSolved(SolvedPatternId, SolvingActor);
    }
}

void APickupActorAAARuneGridInstrument::RebuildGrid()
{
    ClearLineMeshes();
    ClearGeneratedGridComponents();
    RefreshGeneratedDefaultMaterials();
    GenerateGridComponents();
}


int32 APickupActorAAARuneGridInstrument::GetGridCellId(int32 Row, int32 Column) const
{
	if (Row < 0 || Column < 0 || Row >= GridRows || Column >= GridColumns)
	{
		return INDEX_NONE;
	}

	return Row * GridColumns + Column + 1;
}

bool APickupActorAAARuneGridInstrument::GetGridCoordinatesForCellId(
    int32 CellId,
    int32& OutRow,
    int32& OutColumn) const
{
    OutRow = INDEX_NONE;
    OutColumn = INDEX_NONE;

    const int32 SafeColumns = FMath::Max(1, GridColumns);
    const int32 ZeroBasedCellId = CellId - 1;
    if (ZeroBasedCellId < 0)
    {
        return false;
    }

    OutRow = ZeroBasedCellId / SafeColumns;
    OutColumn = ZeroBasedCellId % SafeColumns;
    return OutRow >= 0 && OutRow < FMath::Max(1, GridRows)
        && OutColumn >= 0 && OutColumn < SafeColumns;
}


void APickupActorAAARuneGridInstrument::AppendInterpolatedCellsBetween(int32 FromCellId, int32 ToCellId)
{
    int32 FromRow = INDEX_NONE;
    int32 FromColumn = INDEX_NONE;
    int32 ToRow = INDEX_NONE;
    int32 ToColumn = INDEX_NONE;

    if (!GetGridCoordinatesForCellId(FromCellId, FromRow, FromColumn)
        || !GetGridCoordinatesForCellId(ToCellId, ToRow, ToColumn))
    {
        TryAppendSingleResolvedCell(ToCellId);
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

        const int32 InterpolatedCellId = GetGridCellId(CurrentRow, CurrentColumn);
        if (InterpolatedCellId != INDEX_NONE)
        {
            TryAppendSingleResolvedCell(InterpolatedCellId);
        }
    }
}

int32 APickupActorAAARuneGridInstrument::FindNearestCellIdToWorldLocation(const FVector& WorldLocation) const
{
    float BestDistanceSquared = TNumericLimits<float>::Max();
    int32 BestCellId = INDEX_NONE;

    for (const TPair<int32, TWeakObjectPtr<UStaticMeshComponent>>& CellPair : CellComponentById)
    {
        const UStaticMeshComponent* CellComponent = CellPair.Value.Get();
        if (!IsValid(CellComponent))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(CellComponent->GetComponentLocation(), WorldLocation);
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestCellId = CellPair.Key;
        }
    }

    return BestCellId;
}

bool APickupActorAAARuneGridInstrument::GetPreferredDrawStartScreenPosition(
	APlayerController* PC,
	FVector2D& OutScreenPosition) const
{
	OutScreenPosition = FVector2D::ZeroVector;
	if (!PC)
	{
		return false;
	}

    if (bUseDrawStartAnchor && IsValid(DrawStartAnchorComponent))
    {

        const int32 NearestCellId = FindNearestCellIdToWorldLocation(DrawStartAnchorComponent->GetComponentLocation());
        if (NearestCellId != INDEX_NONE)
        {
            const FVector NearestCellWorldLocation = GetCellWorldLocationById(NearestCellId);
            if (PC->ProjectWorldLocationToScreen(NearestCellWorldLocation, OutScreenPosition, true))
            {
                return true;
            }
        }


        if (PC->ProjectWorldLocationToScreen(
            DrawStartAnchorComponent->GetComponentLocation(),
            OutScreenPosition,
            true))
        {
            return true;
        }
    }

	const int32 StartCellId = GetGridCellId(0, 0);
	const TWeakObjectPtr<UStaticMeshComponent>* CellPtr = CellComponentById.Find(StartCellId);
	const UStaticMeshComponent* CellComponent = CellPtr ? CellPtr->Get() : nullptr;
	if (!IsValid(CellComponent))
	{
		return false;
	}

	return PC->ProjectWorldLocationToScreen(CellComponent->GetComponentLocation(), OutScreenPosition, true);
}

void APickupActorAAARuneGridInstrument::GetCellScreenPositions(
	APlayerController* PC,
	TArray<TPair<int32, FVector2D>>& OutPositions) const
{
	OutPositions.Reset();
	if (!PC)
	{
		return;
	}

	for (const TPair<int32, TWeakObjectPtr<UStaticMeshComponent>>& CellPair : CellComponentById)
	{
		const UStaticMeshComponent* CellComponent = CellPair.Value.Get();
		if (!IsValid(CellComponent))
		{
			continue;
		}

		FVector2D ScreenPos = FVector2D::ZeroVector;
		if (PC->ProjectWorldLocationToScreen(CellComponent->GetComponentLocation(), ScreenPos, true))
		{
			OutPositions.Emplace(CellPair.Key, ScreenPos);
		}
	}
}

void APickupActorAAARuneGridInstrument::ClearGeneratedGridComponents()
{
	TArray<UStaticMeshComponent*> ComponentsToDestroy = GeneratedCellComponents;

	if (ComponentsToDestroy.Num() == 0)
	{
		TArray<UStaticMeshComponent*> AttachedStaticMeshes;
		GetComponents(AttachedStaticMeshes);
		for (UStaticMeshComponent* StaticMeshComp : AttachedStaticMeshes)
		{
			if (IsValid(StaticMeshComp) && StaticMeshComp->ComponentHasTag(FName("GeneratedRuneGridCell")))
			{
				ComponentsToDestroy.Add(StaticMeshComp);
			}
		}
	}

	for (UStaticMeshComponent* CellComponent : ComponentsToDestroy)
	{
		if (!IsValid(CellComponent) || CellComponent == MeshComponent)
		{
			continue;
		}

		CellComponent->DestroyComponent();
	}

	GeneratedCellComponents.Reset();
	CellComponentById.Reset();
	OriginalCellMaterials.Reset();
}

void APickupActorAAARuneGridInstrument::GenerateGridComponents()
{
	if (!GridRootComponent)
	{
		return;
	}

	const int32 SafeRows = FMath::Max(1, GridRows);
	const int32 SafeColumns = FMath::Max(1, GridColumns);
	UStaticMesh* ResolvedCellMesh = CellMesh ? CellMesh : DefaultCellMesh.Get();

	for (int32 RowIndex = 0; RowIndex < SafeRows; ++RowIndex)
	{
		for (int32 ColumnIndex = 0; ColumnIndex < SafeColumns; ++ColumnIndex)
		{
			const int32 CellId = RowIndex * SafeColumns + ColumnIndex + 1;
			const FName ComponentName = *FString::Printf(TEXT("grid_cell_%d"), CellId);
			UStaticMeshComponent* CellComponent = NewObject<UStaticMeshComponent>(this, ComponentName);
			if (!CellComponent)
			{
				continue;
			}

			CellComponent->ComponentTags.Add(FName("GeneratedRuneGridCell"));
			CellComponent->SetupAttachment(GridRootComponent);
			CellComponent->SetMobility(EComponentMobility::Movable);
			CellComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			CellComponent->SetGenerateOverlapEvents(false);
			CellComponent->SetCanEverAffectNavigation(false);
			CellComponent->SetCastShadow(false);
            CellComponent->SetVisibleInRayTracing(false);
			CellComponent->SetRelativeLocation(GetCellRelativeLocation(RowIndex, ColumnIndex));
			CellComponent->SetRelativeRotation(CellRelativeRotation);
			CellComponent->SetRelativeScale3D(CellScale);
			if (ResolvedCellMesh)
			{
				CellComponent->SetStaticMesh(ResolvedCellMesh);
			}
			CellComponent->RegisterComponent();

			GeneratedCellComponents.Add(CellComponent);
			CellComponentById.Add(CellId, CellComponent);
			CacheOriginalCellMaterials(CellId, CellComponent);
			ApplyMaterialToCell(
				CellComponent,
				CellMaterialOverride ? CellMaterialOverride : GeneratedIdleCellMaterial.Get());
		}
	}
}

FVector APickupActorAAARuneGridInstrument::GetCellRelativeLocation(int32 RowIndex, int32 ColumnIndex) const
{
	const float TotalWidth = (FMath::Max(1, GridColumns) - 1) * CellSpacing.X;
	const float TotalHeight = (FMath::Max(1, GridRows) - 1) * CellSpacing.Y;

	const float XOffset = bCenterGridAroundOrigin ? -TotalHeight * 0.5f : 0.f;
	const float YOffset = bCenterGridAroundOrigin ? -TotalWidth * 0.5f : 0.f;

	return FVector(
		XOffset + RowIndex * CellSpacing.Y,
		YOffset + ColumnIndex * CellSpacing.X,
		0.f);
}

FVector APickupActorAAARuneGridInstrument::GetCellWorldLocationById(int32 CellId) const
{
	if (const TWeakObjectPtr<UStaticMeshComponent>* CellPtr = CellComponentById.Find(CellId))
	{
		if (const UStaticMeshComponent* CellComponent = CellPtr->Get())
		{
			return CellComponent->GetComponentLocation();
		}
	}

	return GetActorLocation();
}

int32 APickupActorAAARuneGridInstrument::ResolveHoveredCellFromScreenPosition(
	APlayerController* UsingController,
	const FVector2D& ScreenPosition) const
{
	if (!UsingController || !GridRootComponent)
	{
		return INDEX_NONE;
	}

	FVector RayOrigin = FVector::ZeroVector;
    FVector RayDirection = FVector::ZeroVector;
    if (!UsingController->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, RayOrigin, RayDirection))
    {
        return INDEX_NONE;
    }

    const FTransform GridTransform = GridRootComponent->GetComponentTransform();
    const FVector PlaneOrigin = GridTransform.GetLocation();
    const FVector PlaneNormal = GridTransform.GetUnitAxis(EAxis::Z);
    const float Denominator = FVector::DotProduct(RayDirection, PlaneNormal);
    if (FMath::IsNearlyZero(Denominator))
    {
        return INDEX_NONE;
    }

    const float DistanceAlongRay = FVector::DotProduct(PlaneOrigin - RayOrigin, PlaneNormal) / Denominator;
    if (DistanceAlongRay < 0.f)
    {
        return INDEX_NONE;
    }

    const FVector LocalHitLocation = GridTransform.InverseTransformPosition(RayOrigin + RayDirection * DistanceAlongRay);
    const int32 SafeRows = FMath::Max(1, GridRows);
    const int32 SafeColumns = FMath::Max(1, GridColumns);
    const float SafeRowSpacing = FMath::Max(UE_KINDA_SMALL_NUMBER, CellSpacing.Y);
    const float SafeColumnSpacing = FMath::Max(UE_KINDA_SMALL_NUMBER, CellSpacing.X);
    const float TotalWidth = (SafeColumns - 1) * SafeColumnSpacing;
    const float TotalHeight = (SafeRows - 1) * SafeRowSpacing;
    const float XOffset = bCenterGridAroundOrigin ? -TotalHeight * 0.5f : 0.f;
    const float YOffset = bCenterGridAroundOrigin ? -TotalWidth * 0.5f : 0.f;

    const int32 RowIndex = FMath::RoundToInt((LocalHitLocation.X - XOffset) / SafeRowSpacing);
    const int32 ColumnIndex = FMath::RoundToInt((LocalHitLocation.Y - YOffset) / SafeColumnSpacing);
    if (RowIndex < 0 || RowIndex >= SafeRows || ColumnIndex < 0 || ColumnIndex >= SafeColumns)
    {
        return INDEX_NONE;
    }

    return GetGridCellId(RowIndex, ColumnIndex);
}

bool APickupActorAAARuneGridInstrument::TryAppendResolvedCell(int32 CellId)
{
    if (CellId == INDEX_NONE)
    {
        return false;
    }

    if (CurrentDrawSequence.Num() == 0)
    {
        return TryAppendSingleResolvedCell(CellId);
    }

    const int32 LastCellId = CurrentDrawSequence.Last();
    if (LastCellId == CellId)
    {
        return false;
    }

    AppendInterpolatedCellsBetween(LastCellId, CellId);
    return CurrentDrawSequence.Num() > 0 && CurrentDrawSequence.Last() == CellId;
}

bool APickupActorAAARuneGridInstrument::TryAppendSingleResolvedCell(int32 CellId)
{
    if (CellId == INDEX_NONE)
    {
        return false;
    }

    if (CurrentDrawSequence.Num() > 0 && CurrentDrawSequence.Last() == CellId)
    {
        return false;
    }

    if (!bAllowCellRepeat && CurrentDrawSequence.Contains(CellId))
    {
        return false;
    }

    CurrentDrawSequence.Add(CellId);
    return true;
}

bool APickupActorAAARuneGridInstrument::TryResolveAcceptedPattern(
	const TArray<int32>& CellSequence,
	FName& OutPatternId) const
{
	OutPatternId = NAME_None;

	for (const FRuneGridPattern& Pattern : AcceptedPatterns)
	{
		if (Pattern.CellSequence.Num() == 0)
		{
			continue;
		}

		if (bRequireExactPatternMatch)
		{
			if (Pattern.CellSequence == CellSequence)
			{
				OutPatternId = Pattern.PatternId;
				return true;
			}
			continue;
		}

		if (CellSequence.Num() < Pattern.CellSequence.Num())
		{
			continue;
		}

		bool bMatches = true;
		for (int32 Index = 0; Index < Pattern.CellSequence.Num(); ++Index)
		{
			if (CellSequence[Index] != Pattern.CellSequence[Index])
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

void APickupActorAAARuneGridInstrument::ApplySequenceToVisuals(
	const TArray<int32>& CellSequence,
	bool bSolved,
	bool bDrawingActive)
{
	RebuildLineMeshes(CellSequence);
	ReceiveRuneGridVisualStateChanged(CellSequence, bSolved, bDrawingActive);
}

void APickupActorAAARuneGridInstrument::ApplyCellVisuals(
	const TArray<int32>& CellSequence,
	bool bSolved,
	bool bDrawingActive)
{
	TSet<int32> TracedCellIds(CellSequence);

	for (const TPair<int32, TWeakObjectPtr<UStaticMeshComponent>>& CellPair : CellComponentById)
	{
		UStaticMeshComponent* CellComponent = CellPair.Value.Get();
		if (!IsValid(CellComponent))
		{
			continue;
		}

		UMaterialInterface* TargetMaterial = CellMaterialOverride ? CellMaterialOverride : GeneratedIdleCellMaterial.Get();

		if (TracedCellIds.Contains(CellPair.Key))
		{
			TargetMaterial = bSolved
				                 ? (SolvedCellMaterial ? SolvedCellMaterial : GeneratedSolvedCellMaterial.Get())
				                 : (TracedCellMaterial ? TracedCellMaterial : GeneratedTracedCellMaterial.Get());
		}

		if (bDrawingActive && HoveredCellId == CellPair.Key)
		{
			TargetMaterial = HoveredCellMaterial ? HoveredCellMaterial : GeneratedHoveredCellMaterial.Get();
		}

		ApplyMaterialToCell(CellComponent, TargetMaterial);
	}
}

void APickupActorAAARuneGridInstrument::RefreshGeneratedDefaultMaterials()
{
	GeneratedIdleCellMaterial = nullptr;
	GeneratedHoveredCellMaterial = nullptr;
	GeneratedTracedCellMaterial = nullptr;
	GeneratedSolvedCellMaterial = nullptr;
	GeneratedLineMaterial = nullptr;

	if (!bUseGeneratedDefaultMaterials || !DefaultVisualBaseMaterial)
	{
		return;
	}

	GeneratedIdleCellMaterial = CreateGeneratedStyleMaterial(TEXT("RuneGridIdle"), IdleCellTint, IdleGlowStrength);
	GeneratedHoveredCellMaterial = CreateGeneratedStyleMaterial(TEXT("RuneGridHover"), HoveredCellTint, HoveredGlowStrength);
	GeneratedTracedCellMaterial = CreateGeneratedStyleMaterial(TEXT("RuneGridTrace"), TracedCellTint, TracedGlowStrength);
	GeneratedSolvedCellMaterial = CreateGeneratedStyleMaterial(TEXT("RuneGridSolved"), SolvedCellTint, SolvedGlowStrength);
	GeneratedLineMaterial = CreateGeneratedStyleMaterial(TEXT("RuneGridLine"), LineTint, LineGlowStrength);
}

UMaterialInstanceDynamic* APickupActorAAARuneGridInstrument::CreateGeneratedStyleMaterial(
	const TCHAR* MaterialName,
	const FLinearColor& Tint,
	float GlowStrength)
{
	if (!DefaultVisualBaseMaterial)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* MaterialInstance = UMaterialInstanceDynamic::Create(
		DefaultVisualBaseMaterial,
		this,
		FName(MaterialName));
	ApplyColorParametersToMaterial(MaterialInstance, Tint, GlowStrength);
	return MaterialInstance;
}

void APickupActorAAARuneGridInstrument::ApplyColorParametersToMaterial(
	UMaterialInstanceDynamic* MaterialInstance,
	const FLinearColor& Tint,
	float GlowStrength) const
{
	if (!MaterialInstance)
	{
		return;
	}

	const TArray<FName> ColorParameterNames = {
		TEXT("Color"),
		TEXT("BaseColor"),
		TEXT("Tint"),
		TEXT("TintColor"),
		TEXT("TintColorAndOpacity"),
		TEXT("EmissiveColor"),
		TEXT("GlowColor")
	};

    // ... 接上文 ApplyColorParametersToMaterial ...

    for (const FName& ParameterName : ColorParameterNames)
    {
        MaterialInstance->SetVectorParameterValue(ParameterName, Tint);
    }

    const TArray<FName> ScalarParameterNames = {
        TEXT("Glow"),
        TEXT("GlowStrength"),
        TEXT("GlowIntensity"),
        TEXT("EmissiveStrength"),
        TEXT("EmissiveIntensity"),
        TEXT("Intensity"),
        TEXT("Opacity")
    };

    for (const FName& ParameterName : ScalarParameterNames)
    {
        MaterialInstance->SetScalarParameterValue(ParameterName, GlowStrength);
    }
}

void APickupActorAAARuneGridInstrument::ApplyMaterialToCell(
    UStaticMeshComponent* CellComponent,
    UMaterialInterface* Material) const
{
    if (!IsValid(CellComponent))
    {
        return;
    }

    const int32 MaterialSlotCount = CellComponent->GetNumMaterials();
    if (Material)
    {
        const int32 SlotCountToUse = MaterialSlotCount > 0 ? MaterialSlotCount : 1;
        for (int32 MaterialIndex = 0; MaterialIndex < SlotCountToUse; ++MaterialIndex)
        {
            CellComponent->SetMaterial(MaterialIndex, Material);
        }
        return;
    }

    for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
    {
        CellComponent->SetMaterial(MaterialIndex, nullptr);
    }
}

void APickupActorAAARuneGridInstrument::SetHoveredCellId(int32 NewHoveredCellId, bool bForceBroadcast)
{
    if (!bForceBroadcast && HoveredCellId == NewHoveredCellId)
    {
        return;
    }

    HoveredCellId = NewHoveredCellId;
    ReceiveRuneGridHoverStateChanged(HoveredCellId, bRuneDrawActive);
}

void APickupActorAAARuneGridInstrument::RebuildLineMeshes(const TArray<int32>& CellSequence)
{
    ClearLineMeshes();

    AppendLineMeshes(CellSequence, 1);
}

void APickupActorAAARuneGridInstrument::AppendLineMeshes(const TArray<int32>& CellSequence, int32 FirstSegmentEndIndex)
{
    if (!GridLineRootComponent || !LineSegmentMesh || CellSequence.Num() < 2)
    {
        return;
    }

    const FTransform RootTransform = GridLineRootComponent->GetComponentTransform();

    for (int32 CellIndex = FMath::Max(1, FirstSegmentEndIndex); CellIndex < CellSequence.Num(); ++CellIndex)
    {
        const FVector StartWorld = GetCellWorldLocationById(CellSequence[CellIndex - 1]);
        const FVector EndWorld = GetCellWorldLocationById(CellSequence[CellIndex]);
        const FVector StartLocal = RootTransform.InverseTransformPosition(StartWorld);
        const FVector EndLocal = RootTransform.InverseTransformPosition(EndWorld);
        const FVector Tangent = (EndLocal - StartLocal) * 0.5f;

        USplineMeshComponent* LineComponent = NewObject<USplineMeshComponent>(this);
        if (!LineComponent)
        {
            continue;
        }

        LineComponent->SetupAttachment(GridLineRootComponent);
        LineComponent->SetMobility(EComponentMobility::Movable);
        LineComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        LineComponent->SetGenerateOverlapEvents(false);
        LineComponent->SetCanEverAffectNavigation(false);
        LineComponent->SetCastShadow(false);
        LineComponent->SetVisibleInRayTracing(false);
        LineComponent->SetStaticMesh(LineSegmentMesh);
        LineComponent->SetForwardAxis(LineForwardAxis, false);
        LineComponent->RegisterComponent();
        LineComponent->SetStartScale(FVector2D(LineThickness, LineThickness), false);
        LineComponent->SetEndScale(FVector2D(LineThickness, LineThickness), false);
        LineComponent->SetStartAndEnd(StartLocal, Tangent, EndLocal, Tangent, true);

        UMaterialInterface* EffectiveLineMaterial = LineMaterialOverride ? LineMaterialOverride : GeneratedLineMaterial.Get();
        if (EffectiveLineMaterial)
        {
            int32 MaterialSlotCount = LineSegmentMesh->GetStaticMaterials().Num();
            if (MaterialSlotCount <= 0)
            {
                MaterialSlotCount = 1;
            }

            for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
            {
                LineComponent->SetMaterial(MaterialIndex, EffectiveLineMaterial);
            }
        }

        ActiveLineComponents.Add(LineComponent);
    }
}

void APickupActorAAARuneGridInstrument::ClearLineMeshes()
{
    for (USplineMeshComponent* LineComp : ActiveLineComponents)
    {
        if (IsValid(LineComp))
        {
            LineComp->DestroyComponent();
        }
    }

    ActiveLineComponents.Reset();
}

void APickupActorAAARuneGridInstrument::CacheOriginalCellMaterials(
    int32 CellId,
    UStaticMeshComponent* CellComponent)
{
    if (!IsValid(CellComponent))
    {
        return;
    }

    TArray<UMaterialInterface*> Materials;

    const int32 MaterialCount = CellComponent->GetNumMaterials();
    for (int32 i = 0; i < MaterialCount; ++i)
    {
        Materials.Add(CellComponent->GetMaterial(i));
    }

    OriginalCellMaterials.Add(CellId, Materials);
}
