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

UUserWidget* UOBGridInventoryWidget::AddItemWidget(UObject* ItemDataSource, const int32 ItemRows, const int32 ItemCols,
												   const TSubclassOf<UUserWidget> CustomItemWidgetClass)
{
	// Validate inputs before proceeding with item addition 
	if (!ValidateAddItemInputs(ItemDataSource, ItemRows, ItemCols, CustomItemWidgetClass))
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

	return AddItemWidgetInternal(ItemDataSource, ItemRows, ItemCols, FoundRow, FoundCol, CustomItemWidgetClass);
}

UUserWidget* UOBGridInventoryWidget::AddItemWidgetAt(UObject* ItemDataSource, const int32 ItemRows,
													 const int32 ItemCols, const int32 RowTopLeft,
													 const int32 ColTopLeft,
													 const TSubclassOf<UUserWidget> CustomItemWidgetClass)
{
	// Validate inputs before proceeding
	if (!ValidateAddItemInputs(ItemDataSource, ItemRows, ItemCols, CustomItemWidgetClass))
	{
		return nullptr;
	}

	if (!IsAreaClear(RowTopLeft, ColTopLeft, ItemRows, ItemCols))
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("[%s::%hs] - Failed to add item. Target area at [%d, %d] with size [%d, %d] is not clear."),
			   *GetNameSafe(this), __FUNCTION__, RowTopLeft, ColTopLeft, ItemRows, ItemCols);
		return nullptr;
	}

	return AddItemWidgetInternal(ItemDataSource, ItemRows, ItemCols, RowTopLeft, ColTopLeft, CustomItemWidgetClass);
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

	// Find the data source before removing the item from the map
	UObject* DataSource = nullptr;
	for (auto It = DataSourceToWidgetMap.CreateConstIterator(); It; ++It)
	{
		if (It.Value() == ItemWidgetToRemove)
		{
			DataSource = It.Key();
			break;
		}
	}

	// Remove from the tracking map
	if (const int32 NumRemovedFromMap = PlacedItemInfoMap.Remove(ItemWidgetToRemove); NumRemovedFromMap > 0)
	{
		if (DataSource)
		{
			DataSourceToWidgetMap.Remove(DataSource);
		}
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
		OnItemRemoved.Broadcast(ItemWidgetToRemove);

		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Widget '%s' was not found in PlacedItemInfoMap."),
		   *GetNameSafe(this), __FUNCTION__, *GetNameSafe(ItemWidgetToRemove));
	return false;
}

void UOBGridInventoryWidget::ClearGrid()
{
	if (!ItemGridPanel)
	{
		return;
	}

	// Create a copy of the keys to iterate over, as RemoveItemWidget will modify the map
	TArray<UUserWidget*> AllWidgets;
	GetAllItemWidgets(AllWidgets);

	for (UUserWidget* Widget : AllWidgets)
	{
		// Use the existing RemoveItemWidget function to ensure all cleanup logic is run
		RemoveItemWidget(Widget);
	}

	// Although RemoveItemWidget calls UpdateDummyCells, call it once at the end for certainty
	UpdateDummyCells();

	UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Grid cleared of all items."), *GetNameSafe(this), __FUNCTION__);
}

bool UOBGridInventoryWidget::MoveItemWidget(UUserWidget* ItemWidgetToMove, const int32 NewRowTopLeft,
											const int32 NewColTopLeft)
{
	if (!ItemWidgetToMove || !ItemGridPanel)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Invalid input: ItemWidgetToMove or ItemGridPanel is null."),
			   *GetNameSafe(this), __FUNCTION__);
		return false;
	}

	FOBGridItemInfo* ItemInfo = PlacedItemInfoMap.Find(ItemWidgetToMove);
	if (!ItemInfo)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - Widget '%s' is not tracked by this inventory."), *GetNameSafe(this),
			   __FUNCTION__, *ItemWidgetToMove->GetName());
		return false;
	}

	// Check if the new area is clear, ignoring the item we are currently moving
	if (!IsAreaClearForMove(NewRowTopLeft, NewColTopLeft, ItemInfo->RowSpan, ItemInfo->ColumnSpan, ItemWidgetToMove))
	{
		UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Cannot move '%s'. Target area is not clear."), *GetNameSafe(this),
			   __FUNCTION__, *ItemWidgetToMove->GetName());
		return false;
	}

	// Update the item's info
	ItemInfo->Row = NewRowTopLeft;
	ItemInfo->Column = NewColTopLeft;

	// Update the widget's slot in the grid panel
	if (UGridSlot* GridSlot = Cast<UGridSlot>(ItemWidgetToMove->Slot))
	{
		GridSlot->SetRow(NewRowTopLeft);
		GridSlot->SetColumn(NewColTopLeft);

		UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Moved '%s' to (Row:%d, Col:%d)."), *GetNameSafe(this), __FUNCTION__,
			   *ItemWidgetToMove->GetName(), NewRowTopLeft, NewColTopLeft);

		// The occupied cells have changed, so we need to update the dummy cells
		UpdateDummyCells();

		// Broadcast the move event
		OnItemMoved.Broadcast(ItemWidgetToMove, *ItemInfo);

		return true;
	}

	return false;
}

void UOBGridInventoryWidget::GetAllItemWidgets(TArray<UUserWidget*>& OutItemWidgets) const
{
	TArray<TObjectPtr<UUserWidget>> Keys;
	PlacedItemInfoMap.GetKeys(Keys);
	OutItemWidgets.Reset(Keys.Num());
	for (const TObjectPtr<UUserWidget>& Key : Keys)
	{
		if (IsValid(Key.Get()))
		{
			OutItemWidgets.Add(Key.Get());
		}
	}
}

UUserWidget* UOBGridInventoryWidget::GetItemWidgetFromDataSource(UObject* DataSource) const
{
	if (!DataSource)
	{
		return nullptr;
	}

	const TObjectPtr<UUserWidget>* FoundWidget = DataSourceToWidgetMap.Find(DataSource);
	return FoundWidget ? *FoundWidget : nullptr;
}

bool UOBGridInventoryWidget::GetItemInfo(UUserWidget* ItemWidget, FOBGridItemInfo& OutItemInfo) const
{
	if (!ItemWidget)
	{
		return false;
	}

	if (const FOBGridItemInfo* FoundInfo = PlacedItemInfoMap.Find(ItemWidget))
	{
		OutItemInfo = *FoundInfo;
		return true;
	}

	return false;
}

bool UOBGridInventoryWidget::GetItemAt(const int32 TopLeftRow, const int32 TopLeftCol, UUserWidget*& OutItemWidget,
									   UObject*& OutItemDataSource) const
{
	// Initialize output parameters to a safe default state.
	OutItemWidget = nullptr;
	OutItemDataSource = nullptr;

	// Iterate through all placed items to find a match.
	for (const TPair<TObjectPtr<UUserWidget>, FOBGridItemInfo>& Pair : PlacedItemInfoMap)
	{
		const FOBGridItemInfo& Info = Pair.Value;
		UUserWidget* CurrentWidget = Pair.Key;

		// Check if the item's top-left corner is exactly at the specified coordinate.
		if (Info.Row == TopLeftRow && Info.Column == TopLeftCol)
		{
			// We found the item that starts at this location.
			OutItemWidget = CurrentWidget;

			// Now we need to find the data source associated with this widget.
			// We do this by performing a reverse lookup in our DataSourceToWidgetMap.
			for (const TPair<TObjectPtr<UObject>, TObjectPtr<UUserWidget>>& DataSourcePair : DataSourceToWidgetMap)
			{
				if (DataSourcePair.Value == OutItemWidget)
				{
					OutItemDataSource = DataSourcePair.Key;
					break; // Found the data source, no need to continue this inner loop.
				}
			}

			// Return true as we've successfully found the widget and its (optional) data source.
			return true;
		}
	}

	// If the loop completes without finding a match, no item starts at this coordinate.
	return false;
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

bool UOBGridInventoryWidget::ValidateAddItemInputs(UObject* ItemDataSource, const int32 ItemRows, const int32 ItemCols,
												   const TSubclassOf<UUserWidget> CustomItemWidgetClass) const
{
	// Check grid panel exists
	if (!ItemGridPanel)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - ItemGridPanel is null."), *GetNameSafe(this), __FUNCTION__);
		return false;
	}

	// Verify that we have a widget class to create, either the default one or a custom one.
	if (!ItemWidgetClass && !CustomItemWidgetClass)
	{
		UE_LOG(LogTemp, Error,
			   TEXT(
				   "[%s::%hs] - Default ItemWidgetClass is not set, and no CustomItemWidgetClass was provided. Cannot create item widget."
			   ),
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
														   const int32 ItemCols, const int32 RowTopLeft,
														   const int32 ColTopLeft,
														   const TSubclassOf<UUserWidget> CustomItemWidgetClass)
{
	// Decide which widget class to use. Prioritize the custom class if it's valid.
	const TSubclassOf<UUserWidget> WidgetClassToCreate =
		CustomItemWidgetClass ? CustomItemWidgetClass : ItemWidgetClass;

	// This final check is redundant if ValidateAddItemInputs was called, but it's good practice for an internal function.
	if (!WidgetClassToCreate)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - No valid widget class to create."), *GetNameSafe(this), __FUNCTION__);
		return nullptr;
	}

	// Create the item widget instance
	UUserWidget* NewItemWidget = CreateWidget<UUserWidget>(this, WidgetClassToCreate);
	if (!NewItemWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - Failed to create widget from class %s."), *GetNameSafe(this),
			   __FUNCTION__, *WidgetClassToCreate->GetName());
		return nullptr;
	}

	// Add a widget to the grid with a proper span and alignment
	if (UGridSlot* GridSlot = ItemGridPanel->AddChildToGrid(NewItemWidget, RowTopLeft, ColTopLeft))
	{
		GridSlot->SetRowSpan(ItemRows);
		GridSlot->SetColumnSpan(ItemCols);
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);

		// Store item info for tracking
		const FOBGridItemInfo NewItemInfo(RowTopLeft, ColTopLeft, ItemRows, ItemCols);
		PlacedItemInfoMap.Add(NewItemWidget, NewItemInfo);

		if (ItemDataSource)
		{
			DataSourceToWidgetMap.Add(ItemDataSource, NewItemWidget);
		}
		UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Added '%s' to GridPanel at (Row:%d, Col:%d), Span(Rows:%d, Cols:%d)"),
			   *GetNameSafe(this), __FUNCTION__, *NewItemWidget->GetName(), RowTopLeft, ColTopLeft, ItemRows, ItemCols);

		// Update dummy cells after item added
		UpdateDummyCells();
		OnItemAdded.Broadcast(NewItemWidget, NewItemInfo);
		return NewItemWidget;
	}

	UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - Failed to add widget to ItemGridPanel at specified slot."),
		   *GetNameSafe(this), __FUNCTION__);

	// If adding to the grid fails, the created widget should be cleaned up by garbage collection.
	return nullptr;
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

bool UOBGridInventoryWidget::IsAreaClearForMove(const int32 TopLeftRow, const int32 TopLeftCol, const int32 ItemRows,
												const int32 ItemCols,
												UUserWidget* IgnoredWidget) const
{
	const FIntRect NewItemRect(TopLeftCol, TopLeftRow, TopLeftCol + ItemCols, TopLeftRow + ItemRows);

	for (const TPair<TObjectPtr<UUserWidget>, FOBGridItemInfo>& Pair : PlacedItemInfoMap)
	{
		// Skip the widget we are currently moving
		if (!Pair.Key || Pair.Key == IgnoredWidget)
		{
			continue;
		}

		const FOBGridItemInfo& ExistingInfo = Pair.Value;
		const FIntRect ExistingItemRect(
			ExistingInfo.Column,
			ExistingInfo.Row,
			ExistingInfo.Column + ExistingInfo.ColumnSpan,
			ExistingInfo.Row + ExistingInfo.RowSpan
		);

		if (NewItemRect.Intersect(ExistingItemRect))
		{
			return false; // Found an intersection with another item
		}
	}

	return true; // No intersections found
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
