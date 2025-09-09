// Copyright (c) 2024. All rights reserved.

#include "OBGridInventoryWidget.h"

#include "Components/GridPanel.h"
#include "Components/GridSlot.h"

void UOBGridInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize the grid layout and background when the widget is constructed
	SetupGridPanelDimensions();
	UpdateGridBackground();
}

void UOBGridInventoryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Setup grid only in design time
	if (IsDesignTime())
	{
		SetupGridPanelDimensions();
		UpdateGridBackground();
		UpdateSizeBoxOverride();
	}
}

void UOBGridInventoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Check if the allocated size has changed significantly, if so recalculate layout
	if (const FVector2D CurrentAllocatedSize = MyGeometry.GetLocalSize();
		!CurrentAllocatedSize.Equals(LastKnownAllocatedSize, 0.5f))
	{
		RecalculateScaleAndRefreshLayout(MyGeometry);
		LastKnownAllocatedSize = CurrentAllocatedSize;
	}
}

void UOBGridInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Set up initial size box dimensions based on grid config
	if (GridSizeBox && GridConfig.NumColumns > 0 && GridConfig.NumRows > 0 && GridConfig.CellSize > KINDA_SMALL_NUMBER)
	{
		const float InitialWidth = static_cast<float>(GridConfig.NumColumns) * GridConfig.CellSize;
		const float InitialHeight = static_cast<float>(GridConfig.NumRows) * GridConfig.CellSize;

		GridSizeBox->SetWidthOverride(InitialWidth);
		GridSizeBox->SetHeightOverride(InitialHeight);

		CurrentGridScale = 1.0f;

		UE_LOG(LogTemp, Warning,
		       TEXT("[%s] NativeOnInitialized: Set initial SizeBox Override: W=%.2f, H=%.2f. Initial Scale=1.0"),
		       *GetName(), InitialWidth, InitialHeight);
	}
	else
	{
		// Handle invalid config or missing size box
		if (GridSizeBox)
		{
			GridSizeBox->SetWidthOverride(0.0f);
			GridSizeBox->SetHeightOverride(0.0f);
		}
		CurrentGridScale = 1.0f;

		UE_LOG(LogTemp, Warning,
		       TEXT("[%s] NativeOnInitialized: Invalid GridConfig or no SizeBox found. SizeBox override set to 0x0."),
		       *GetName());
	}

	// Initialize last known size to invalid value to force first update
	LastKnownAllocatedSize = FVector2D(-1.0f, -1.0f);

	// Initialize grid visual elements
	UpdateGridBackground();
	SetupGridPanelDimensions();

	// Validate widget classes are properly set
	if (DummyCellWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] NativeOnInitialized: DummyCellWidgetClass is VALID: %s"), *GetName(),
		       *DummyCellWidgetClass->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] NativeOnInitialized: DummyCellWidgetClass is NULL!"), *GetName());
	}
	if (ItemWidgetClass)
	{
		// Log ItemWidgetClass for comparison
		UE_LOG(LogTemp, Warning, TEXT("[%s] NativeOnInitialized: ItemWidgetClass is VALID: %s"), *GetName(),
		       *ItemWidgetClass->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] NativeOnInitialized: ItemWidgetClass is NULL!"), *GetName());
	}

	// Set up initial dummy cells
	UpdateDummyCells();
}

FNavigationReply UOBGridInventoryWidget::NativeOnNavigation(const FGeometry& MyGeometry,
                                                            const FNavigationEvent& InNavigationEvent,
                                                            const FNavigationReply& InDefaultReply)
{
	return Super::NativeOnNavigation(MyGeometry, InNavigationEvent, InDefaultReply);
}


void UOBGridInventoryWidget::SetGridRows(const int32 NewGridRows)
{
	GridConfig.NumRows = NewGridRows;
}

void UOBGridInventoryWidget::SetGridColumns(const int32 NewGridColumns)
{
	GridConfig.NumColumns = NewGridColumns;
}

UUserWidget* UOBGridInventoryWidget::AddItemWidget(UObject* ItemDataSource, const int32 ItemRows, const int32 ItemCols)
{
	// Validate inputs before proceeding with item addition 
	if (!ValidateAddItemInputs(ItemDataSource, ItemRows, ItemCols))
	{
		return nullptr;
	}

	// Find free slot in grid for item
	int32 FoundRow = -1;
	int32 FoundCol = -1;
	if (!FindFreeSlot(ItemRows, ItemCols, FoundRow, FoundCol))
	{
		UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - No available space found for item size %dx%d."), *GetNameSafe(this),
		       __FUNCTION__, ItemRows, ItemCols);
		return nullptr;
	}

	return AddItemWidgetInternal(ItemDataSource, ItemRows, ItemCols, FoundRow, FoundCol);
}

UUserWidget* UOBGridInventoryWidget::AddItemWidgetAt(UObject* ItemDataSource, const int32 ItemRows,
                                                     const int32 ItemCols,
                                                     const int32 RowTopLeft, const int32 ColTopLeft)
{
	// Validate inputs before proceeding
	if (!ValidateAddItemInputs(ItemDataSource, ItemRows, ItemCols))
	{
		return nullptr;
	}

	return AddItemWidgetInternal(ItemDataSource, ItemRows, ItemCols, RowTopLeft, ColTopLeft);
}

bool UOBGridInventoryWidget::ValidateAddItemInputs(UObject* ItemDataSource, const int32 ItemRows,
                                                   const int32 ItemCols) const
{
	// Check grid panel exists
	if (!ItemGridPanel)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - ItemGridPanel is null."), *GetNameSafe(this), __FUNCTION__);
		return false;
	}

	// Verify the item widget class is set
	if (!ItemWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - ItemWidgetClass is not set. Cannot create item widget."),
		       *GetNameSafe(this), __FUNCTION__);
		return false;
	}

	// Validate item dimensions
	if (ItemRows < 1 || ItemCols < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - ItemRows/ItemCols must be at least 1. Received Rows: %d, Cols: %d."),
		       *GetNameSafe(this), __FUNCTION__, ItemRows, ItemCols);
		return false;
	}

	return true;
}

UUserWidget* UOBGridInventoryWidget::AddItemWidgetInternal(UObject* ItemDataSource, const int32 ItemRows,
                                                           const int32 ItemCols,
                                                           const int32 RowTopLeft, const int32 ColTopLeft)
{
	// Create the item widget instance
	UUserWidget* NewItemWidget = CreateWidget<UUserWidget>(this, ItemWidgetClass);
	if (!NewItemWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - Failed to create widget from class %s."), *GetNameSafe(this),
		       __FUNCTION__, *ItemWidgetClass->GetName());
		return nullptr;
	}

	// Add widget to the grid with proper span and alignment
	if (UGridSlot* GridSlot = ItemGridPanel->AddChildToGrid(NewItemWidget, RowTopLeft, ColTopLeft))
	{
		GridSlot->SetRowSpan(ItemRows);
		GridSlot->SetColumnSpan(ItemCols);
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);

		// Store item info for tracking
		PlacedItemInfoMap.Add(NewItemWidget, FOBGridItemInfo(RowTopLeft, ColTopLeft, ItemRows, ItemCols));

		UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Added '%s' to GridPanel at (Row:%d, Col:%d), Span(Rows:%d, Cols:%d)"),
		       *GetNameSafe(this), __FUNCTION__, *NewItemWidget->GetName(), RowTopLeft, ColTopLeft, ItemRows, ItemCols);

		// Update dummy cells after item added
		UpdateDummyCells();
		return NewItemWidget;
	}

	UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - Failed to add widget to ItemGridPanel at specified slot."),
	       *GetNameSafe(this), __FUNCTION__);
	return nullptr;
}

bool UOBGridInventoryWidget::RemoveItemWidget(UUserWidget* ItemWidgetToRemove)
{
	// Input validation
	if (!ItemWidgetToRemove)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - ItemWidgetToRemove is null."),
		       *GetNameSafe(this), __FUNCTION__);
		return false;
	}
	if (!ItemGridPanel)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - ItemGridPanel is null. Cannot remove widget."),
		       *GetNameSafe(this), __FUNCTION__);
		return false;
	}

	// Remove from the tracking map
	if (const int32 NumRemovedFromMap = PlacedItemInfoMap.Remove(ItemWidgetToRemove); NumRemovedFromMap > 0)
	{
		// Remove widget from panel
		const bool bRemovedFromPanel = ItemGridPanel->RemoveChild(ItemWidgetToRemove);

		if (!bRemovedFromPanel)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("[%s::%hs] - Widget '%s' was in PlacedItemInfoMap but could not be removed from ItemGridPanel."
			       ),
			       *GetNameSafe(this), __FUNCTION__, *ItemWidgetToRemove->GetName());
		}

		UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Removed '%s'. From Map: %d, From Panel: %d"),
		       *GetNameSafe(this), __FUNCTION__, *GetNameSafe(ItemWidgetToRemove), NumRemovedFromMap,
		       bRemovedFromPanel);

		// Update dummy cells after item removed
		UpdateDummyCells();

		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Widget '%s' was not found in PlacedItemInfoMap."),
	       *GetNameSafe(this), __FUNCTION__, *GetNameSafe(ItemWidgetToRemove));
	return false;
}

void UOBGridInventoryWidget::ClearGrid()
{
}

bool UOBGridInventoryWidget::IsAreaClear(const int32 TopLeftRow, const int32 TopLeftCol, const int32 ItemRows,
                                         const int32 ItemCols) const
{
	// Create rectangle representing area to check
	const FIntRect NewItemRect(TopLeftCol, TopLeftRow, TopLeftCol + ItemCols, TopLeftRow + ItemRows);

	// Check for intersection with existing items
	for (const TPair<TObjectPtr<UUserWidget>, FOBGridItemInfo>& Pair : PlacedItemInfoMap)
	{
		if (!Pair.Key) continue;
		const FOBGridItemInfo& ExistingInfo = Pair.Value;
		const FIntRect ExistingItemRect(
			ExistingInfo.Column,
			ExistingInfo.Row,
			ExistingInfo.Column + ExistingInfo.ColumnSpan,
			ExistingInfo.Row + ExistingInfo.RowSpan
		);

		if (NewItemRect.Intersect(ExistingItemRect))
		{
			return false;
		}
	}

	return true;
}

bool UOBGridInventoryWidget::FindFreeSlot(const int32 ItemRows, const int32 ItemCols, int32& OutRow,
                                          int32& OutCol) const
{
	// Early exit if item too big for the grid
	if (GridConfig.NumRows < ItemRows || GridConfig.NumColumns < ItemCols) return false;

	// Search each possible position
	for (int32 TestRow = 0; TestRow <= GridConfig.NumRows - ItemRows; ++TestRow)
	{
		for (int32 TestCol = 0; TestCol <= GridConfig.NumColumns - ItemCols; ++TestCol)
		{
			if (IsAreaClear(TestRow, TestCol, ItemRows, ItemCols))
			{
				OutRow = TestRow;
				OutCol = TestCol;
				return true;
			}
		}
	}
	return false;
}

void UOBGridInventoryWidget::UpdateGridBackground() const
{
	// Update the grid background with the current configuration
	if (GridBackground != nullptr)
	{
		GridBackground->UpdateGridParameters(GridConfig);
	}
}

bool UOBGridInventoryWidget::CalculateCurrentScale(const FGeometry& CurrentGeometry)
{
	// Validate grid configuration
	if (GridConfig.NumRows <= 0 || GridConfig.NumColumns <= 0 || GridConfig.CellSize <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Invalid GridConfig. Scale calculation skipped."), *GetName(),
		       __FUNCTION__);
		return false;
	}

	// Validate allocated size
	const FVector2D AllocatedSize = CurrentGeometry.GetLocalSize();
	if (AllocatedSize.X <= KINDA_SMALL_NUMBER || AllocatedSize.Y <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Allocated size too small. Scale calculation skipped."), *GetName(),
		       __FUNCTION__);
		return false;
	}

	// Calculate target dimensions
	const float TargetWidth = static_cast<float>(GridConfig.NumColumns) * GridConfig.CellSize;
	const float TargetHeight = static_cast<float>(GridConfig.NumRows) * GridConfig.CellSize;

	if (TargetWidth <= KINDA_SMALL_NUMBER || TargetHeight <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Target size from config too small. Scale calculation skipped."),
		       *GetName(), __FUNCTION__);
		return false;
	}

	// Calculate and use minimum scale to maintain aspect ratio
	const float ScaleX = AllocatedSize.X / TargetWidth;
	const float ScaleY = AllocatedSize.Y / TargetHeight;

	CurrentGridScale = FMath::Min(ScaleX, ScaleY);

	return true;
}

void UOBGridInventoryWidget::UpdateSizeBoxOverride() const
{
	// Validate size box exists
	if (!GridSizeBox)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - GridSizeBox is null."), *GetName(), __FUNCTION__);
		return;
	}

	// Calculate scaled dimensions
	const float ScaledWidth = static_cast<float>(GridConfig.NumColumns) * GridConfig.CellSize * CurrentGridScale;
	const float ScaledHeight = static_cast<float>(GridConfig.NumRows) * GridConfig.CellSize * CurrentGridScale;

	// Update only if the size changed significantly
	constexpr float Tolerance = 0.5f;
	if (!FMath::IsNearlyEqual(GridSizeBox->GetWidthOverride(), ScaledWidth, Tolerance) ||
		!FMath::IsNearlyEqual(GridSizeBox->GetHeightOverride(), ScaledHeight, Tolerance))
	{
		GridSizeBox->SetWidthOverride(ScaledWidth);
		GridSizeBox->SetHeightOverride(ScaledHeight);
	}
}

void UOBGridInventoryWidget::RecalculateScaleAndRefreshLayout(const FGeometry& CurrentGeometry)
{
	// Calculate a new scale based on current geometry
	if (!CalculateCurrentScale(CurrentGeometry))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] RecalculateScaleAndRefreshLayout: Scale calculation failed."), *GetName());
		return;
	}

	// Apply new scale
	UpdateSizeBoxOverride();

	// Refresh grid layout
	if (ItemGridPanel)
	{
		ItemGridPanel->InvalidateLayoutAndVolatility();
	}
}

void UOBGridInventoryWidget::UpdateDummyCells()
{
	// Validate prerequisites
	if (!ItemGridPanel || !DummyCellWidgetClass || GridConfig.NumRows <= 0 || GridConfig.NumColumns <= 0)
	{
		if (!DummyCellWidgetsMap.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Prerequisites failed. Clearing %d tracked dummy widgets."),
			       *GetNameSafe(this), __FUNCTION__, DummyCellWidgetsMap.Num());
			TArray<FIntPoint> CoordsToRemove;
			for (const auto& Pair : DummyCellWidgetsMap) { CoordsToRemove.Add(Pair.Key); }
			for (const FIntPoint& Coord : CoordsToRemove) { RemoveDummyWidgetAt(Coord); }
			DummyCellWidgetsMap.Empty();
		}
		return;
	}

	// Calculate cells occupied by items
	TSet<FIntPoint> OccupiedCells;
	OccupiedCells.Reserve(PlacedItemInfoMap.Num() * 4);
	for (const TPair<TObjectPtr<UUserWidget>, FOBGridItemInfo>& Pair : PlacedItemInfoMap)
	{
		if (!Pair.Key) continue;
		const FOBGridItemInfo& Info = Pair.Value;
		const int32 EndRow = FMath::Min(Info.Row + Info.RowSpan, GridConfig.NumRows);
		const int32 EndCol = FMath::Min(Info.Column + Info.ColumnSpan, GridConfig.NumColumns);
		for (int32 r = FMath::Max(0, Info.Row); r < EndRow; ++r)
		{
			for (int32 c = FMath::Max(0, Info.Column); c < EndCol; ++c)
			{
				OccupiedCells.Add(FIntPoint(c, r));
			}
		}
	}

	// Track dummy cell changes
	int32 DummiesAdded = 0;
	int32 DummiesKept = 0;
	TSet<FIntPoint> DummiesToRemove;

	// Check existing dummies
	for (const auto& Pair : DummyCellWidgetsMap)
	{
		const FIntPoint& CurrentCoord = Pair.Key;
		const TWeakObjectPtr<UUserWidget>& DummyPtr = Pair.Value;

		if (OccupiedCells.Contains(CurrentCoord) || !DummyPtr.IsValid())
		{
			DummiesToRemove.Add(CurrentCoord);
		}
		else
		{
			DummiesKept++;
		}
	}

	// Remove obsolete dummies
	for (const FIntPoint& CoordToRemove : DummiesToRemove)
	{
		RemoveDummyWidgetAt(CoordToRemove);
	}

	// Add new dummies where needed
	for (int32 r = 0; r < GridConfig.NumRows; ++r)
	{
		for (int32 c = 0; c < GridConfig.NumColumns; ++c)
		{
			const FIntPoint CurrentCoord(c, r);
			if (!OccupiedCells.Contains(CurrentCoord) && !DummyCellWidgetsMap.Contains(CurrentCoord))
			{
				if (TryAddDummyWidgetAt(r, c))
				{
					DummiesAdded++;
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Update finished. Kept: %d, Removed: %d, Added: %d. Total Tracked: %d"),
	       *GetNameSafe(this), __FUNCTION__,
	       DummiesKept, DummiesToRemove.Num(), DummiesAdded,
	       DummyCellWidgetsMap.Num());
}

bool UOBGridInventoryWidget::TryAddDummyWidgetAt(const int32 Row, const int32 Column)
{
	// Validate prerequisites
	if (!ItemGridPanel || !DummyCellWidgetClass) return false;

	const FIntPoint Coord(Column, Row);
	if (DummyCellWidgetsMap.Contains(Coord))
	{
		if (!DummyCellWidgetsMap[Coord].IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Found stale dummy entry at [%d,%d]. Will recreate."),
			       *GetNameSafe(this), __FUNCTION__, Row, Column);
			DummyCellWidgetsMap.Remove(Coord);
		}
		else
		{
			return true;
		}
	}

	// Create and add a dummy widget
	if (UUserWidget* NewDummyWidget = CreateWidget<UUserWidget>(this, DummyCellWidgetClass))
	{
		if (UGridSlot* GridSlot = ItemGridPanel->AddChildToGrid(NewDummyWidget, Row, Column))
		{
			GridSlot->SetRowSpan(1);
			GridSlot->SetColumnSpan(1);
			GridSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
			GridSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
			DummyCellWidgetsMap.Add(Coord, TWeakObjectPtr<UUserWidget>(NewDummyWidget));
			return true;
		}

		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - Failed to add created dummy widget to GridPanel slot at [%d,%d]!"),
		       *GetNameSafe(this), __FUNCTION__, Row, Column);
		NewDummyWidget->ConditionalBeginDestroy();
		NewDummyWidget->SetVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - Failed to create dummy widget from class '%s'!"), *GetNameSafe(this),
		       __FUNCTION__, *GetNameSafe(DummyCellWidgetClass));
	}
	return false;
}

void UOBGridInventoryWidget::RemoveDummyWidgetAt(const FIntPoint& Coord)
{
	// Find and remove the dummy widget at coordinate
	if (const TWeakObjectPtr<UUserWidget>* FoundWidgetPtr = DummyCellWidgetsMap.Find(Coord))
	{
		if (UUserWidget* DummyWidget = FoundWidgetPtr->Get())
		{
			if (ItemGridPanel)
			{
				ItemGridPanel->RemoveChild(DummyWidget);
				UE_LOG(LogTemp, Warning, TEXT("  -> Removing dummy widget '%s' at [%d,%d]."), *GetNameSafe(DummyWidget),
				       Coord.Y, Coord.X);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("  -> Removing stale dummy map entry at [%d,%d]. Widget was already invalid."), Coord.Y,
			       Coord.X);
		}
		DummyCellWidgetsMap.Remove(Coord);
	}
}

void UOBGridInventoryWidget::SetupGridPanelDimensions()
{
	if (!ItemGridPanel) return;

	// Clear existing content
	ItemGridPanel->ClearChildren();
	PlacedItemInfoMap.Empty();
	DummyCellWidgetsMap.Empty();

	// Setup column and row fills
	for (int32 c = 0; c < GridConfig.NumColumns; ++c)
	{
		ItemGridPanel->SetColumnFill(c, 1.0f);
	}

	for (int32 r = 0; r < GridConfig.NumRows; ++r)
	{
		ItemGridPanel->SetRowFill(r, 1.0f);
	}

	UE_LOG(LogTemp, Log, TEXT("[%s] SetupGridPanelDimensions: Configured %d rows, %d columns."), *GetName(),
	       GridConfig.NumRows, GridConfig.NumColumns);
}
