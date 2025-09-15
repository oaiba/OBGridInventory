// Copyright (c) 2024. All rights reserved.

#include "OBGridInventoryWidget.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"

// --- Overrides ---

void UOBGridInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetupGridPanelDimensions();
	UpdateGridBackground();
}

void UOBGridInventoryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
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
	if (GridSizeBox && GridConfig.NumColumns > 0 && GridConfig.NumRows > 0 && GridConfig.CellSize > KINDA_SMALL_NUMBER)
	{
		const float InitialWidth = static_cast<float>(GridConfig.NumColumns) * GridConfig.CellSize;
		const float InitialHeight = static_cast<float>(GridConfig.NumRows) * GridConfig.CellSize;
		GridSizeBox->SetWidthOverride(InitialWidth);
		GridSizeBox->SetHeightOverride(InitialHeight);
		CurrentGridScale = 1.0f;
	}
	else
	{
		if (GridSizeBox)
		{
			GridSizeBox->SetWidthOverride(0.0f);
			GridSizeBox->SetHeightOverride(0.0f);
		}
		CurrentGridScale = 1.0f;
	}
	LastKnownAllocatedSize = FVector2D(-1.0f, -1.0f);
	UpdateGridBackground();
	SetupGridPanelDimensions();
	UpdateDummyCells();
}

FNavigationReply UOBGridInventoryWidget::NativeOnNavigation(const FGeometry& MyGeometry,
															const FNavigationEvent& InNavigationEvent,
															const FNavigationReply& InDefaultReply)
{
	return Super::NativeOnNavigation(MyGeometry, InNavigationEvent, InDefaultReply);
}

// --- Grid Configuration ---

void UOBGridInventoryWidget::SetGridRows(const int32 NewGridRows)
{
	GridConfig.NumRows = NewGridRows;
	// Consider refreshing the grid layout after this change
}

void UOBGridInventoryWidget::SetGridColumns(const int32 NewGridColumns)
{
	GridConfig.NumColumns = NewGridColumns;
	// Consider refreshing the grid layout after this change
}

// --- Item Management ---

UUserWidget* UOBGridInventoryWidget::AddItemWidget(UObject* ItemDataSource, const FInstancedStruct& ItemPayload,
												   const int32 ItemRows, const int32 ItemCols,
												   const TSubclassOf<UUserWidget> CustomItemWidgetClass)
{
	if (!ValidateAddItemInputs(ItemDataSource, ItemRows, ItemCols, CustomItemWidgetClass))
	{
		return nullptr;
	}

	int32 FoundRow = -1;
	int32 FoundCol = -1;
	if (!FindFreeSlot(ItemRows, ItemCols, FoundRow, FoundCol))
	{
		UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - No available space found for item size %dx%d."), *GetNameSafe(this),
			   __FUNCTION__, ItemRows, ItemCols);
		return nullptr;
	}

	return AddItemWidgetInternal(ItemDataSource, ItemPayload, ItemRows, ItemCols, FoundRow, FoundCol,
								 CustomItemWidgetClass);
}

UUserWidget* UOBGridInventoryWidget::AddItemWidgetAt(UObject* ItemDataSource, const FInstancedStruct& ItemPayload,
													 const int32 ItemRows, const int32 ItemCols,
													 const int32 RowTopLeft, const int32 ColTopLeft,
													 const TSubclassOf<UUserWidget> CustomItemWidgetClass)
{
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

	return AddItemWidgetInternal(ItemDataSource, ItemPayload, ItemRows, ItemCols, RowTopLeft, ColTopLeft,
								 CustomItemWidgetClass);
}

bool UOBGridInventoryWidget::RemoveItemWidget(UUserWidget* ItemWidgetToRemove)
{
	if (!ItemWidgetToRemove || !ItemGridPanel) return false;

	if (const int32 NumRemovedFromMap = PlacedItemInfoMap.Remove(ItemWidgetToRemove); NumRemovedFromMap > 0)
	{
		ItemGridPanel->RemoveChild(ItemWidgetToRemove);
		UpdateDummyCells();
		OnItemRemoved.Broadcast(ItemWidgetToRemove);
		return true;
	}
	return false;
}

void UOBGridInventoryWidget::ClearGrid()
{
	if (!ItemGridPanel) return;
	TArray<UUserWidget*> AllWidgets;
	GetAllItemWidgets(AllWidgets);
	for (UUserWidget* Widget : AllWidgets)
	{
		RemoveItemWidget(Widget);
	}
	UpdateDummyCells();
	UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Grid cleared of all items."), *GetNameSafe(this), __FUNCTION__);
}

bool UOBGridInventoryWidget::MoveItemWidget(UUserWidget* ItemWidgetToMove, const int32 NewRowTopLeft,
											const int32 NewColTopLeft)
{
	if (!ItemWidgetToMove || !ItemGridPanel) return false;
	FOBGridItemInfo* ItemInfo = PlacedItemInfoMap.Find(ItemWidgetToMove);
	if (!ItemInfo) return false;

	if (!IsAreaClearForMove(NewRowTopLeft, NewColTopLeft, ItemInfo->RowSpan, ItemInfo->ColumnSpan, ItemWidgetToMove))
	{
		return false;
	}

	ItemInfo->Row = NewRowTopLeft;
	ItemInfo->Column = NewColTopLeft;

	if (UGridSlot* GridSlot = Cast<UGridSlot>(ItemWidgetToMove->Slot))
	{
		GridSlot->SetRow(NewRowTopLeft);
		GridSlot->SetColumn(NewColTopLeft);
		UpdateDummyCells();
		OnItemMoved.Broadcast(ItemWidgetToMove, *ItemInfo);
		return true;
	}
	return false;
}

// --- Querying ---

bool UOBGridInventoryWidget::IsAreaClear(const int32 TopLeftRow, const int32 TopLeftCol, const int32 ItemRows,
										 const int32 ItemCols) const
{
	const FIntRect NewItemRect(TopLeftCol, TopLeftRow, TopLeftCol + ItemCols, TopLeftRow + ItemRows);
	for (const TPair<TObjectPtr<UUserWidget>, FOBGridItemInfo>& Pair : PlacedItemInfoMap)
	{
		if (!Pair.Key) continue;
		const FOBGridItemInfo& ExistingInfo = Pair.Value;
		const FIntRect ExistingItemRect(ExistingInfo.Column, ExistingInfo.Row,
										ExistingInfo.Column + ExistingInfo.ColumnSpan,
										ExistingInfo.Row + ExistingInfo.RowSpan);
		if (NewItemRect.Intersect(ExistingItemRect))
		{
			return false;
		}
	}
	return true;
}

void UOBGridInventoryWidget::GetAllItemWidgets(TArray<UUserWidget*>& OutItemWidgets) const
{
	OutItemWidgets.Empty();
	for (const auto& Pair : PlacedItemInfoMap)
	{
		if (Pair.Key)
		{
			OutItemWidgets.Add(Pair.Key);
		}
	}
}

UUserWidget* UOBGridInventoryWidget::GetItemWidgetFromDataSource(UObject* DataSource) const
{
	if (!DataSource) return nullptr;
	for (const TPair<TObjectPtr<UUserWidget>, FOBGridItemInfo>& Pair : PlacedItemInfoMap)
	{
		if (Pair.Value.ItemDataSource == DataSource)
		{
			return Pair.Key;
		}
	}
	return nullptr;
}

bool UOBGridInventoryWidget::GetItemInfo(UUserWidget* ItemWidget, FOBGridItemInfo& OutItemInfo) const
{
	if (!ItemWidget) return false;
	if (const FOBGridItemInfo* FoundInfo = PlacedItemInfoMap.Find(ItemWidget))
	{
		OutItemInfo = *FoundInfo;
		return true;
	}
	return false;
}

bool UOBGridInventoryWidget::GetItemAt(const int32 TopLeftRow, const int32 TopLeftCol, UUserWidget*& OutItemWidget,
									   UObject*& OutItemDataSource, FInstancedStruct& OutItemPayload) const
{
	OutItemWidget = nullptr;
	OutItemDataSource = nullptr;
	OutItemPayload.Reset();

	for (const TPair<TObjectPtr<UUserWidget>, FOBGridItemInfo>& Pair : PlacedItemInfoMap)
	{
		const FOBGridItemInfo& Info = Pair.Value;
		if (Info.Row == TopLeftRow && Info.Column == TopLeftCol)
		{
			OutItemWidget = Pair.Key;
			OutItemDataSource = Info.ItemDataSource;
			OutItemPayload = Info.ItemPayload;
			return true;
		}
	}
	return false;
}

bool UOBGridInventoryWidget::GetItemPayload(UUserWidget* ItemWidget, FInstancedStruct& OutItemPayload) const
{
	OutItemPayload.Reset();
	if (!ItemWidget) return false;

	if (const FOBGridItemInfo* FoundInfo = PlacedItemInfoMap.Find(ItemWidget))
	{
		OutItemPayload = FoundInfo->ItemPayload;
		return true;
	}
	return false;
}


// --- Internal Implementation ---

bool UOBGridInventoryWidget::ValidateAddItemInputs(UObject* ItemDataSource, const int32 ItemRows,
												   const int32 ItemCols,
												   const TSubclassOf<UUserWidget> CustomItemWidgetClass) const
{
	if (!ItemGridPanel)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s::%hs] - ItemGridPanel is null."), *GetNameSafe(this), __FUNCTION__);
		return false;
	}
	if (!ItemWidgetClass && !CustomItemWidgetClass)
	{
		UE_LOG(LogTemp, Error,
			   TEXT(
				   "[%s::%hs] - Default ItemWidgetClass is not set, and no CustomItemWidgetClass was provided."
			   ), *GetNameSafe(this), __FUNCTION__);
		return false;
	}
	if (ItemRows < 1 || ItemCols < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s::%hs] - ItemRows/ItemCols must be at least 1."), *GetNameSafe(this),
			   __FUNCTION__);
		return false;
	}
	return true;
}

UUserWidget* UOBGridInventoryWidget::AddItemWidgetInternal(UObject* ItemDataSource,
														   const FInstancedStruct& ItemPayload, const int32 ItemRows,
														   const int32 ItemCols, const int32 RowTopLeft,
														   const int32 ColTopLeft,
														   const TSubclassOf<UUserWidget> CustomItemWidgetClass)
{
	const TSubclassOf<UUserWidget> WidgetClassToCreate =
		CustomItemWidgetClass ? CustomItemWidgetClass : ItemWidgetClass;
	if (!WidgetClassToCreate) return nullptr;

	UUserWidget* NewItemWidget = CreateWidget<UUserWidget>(this, WidgetClassToCreate);
	if (!NewItemWidget) return nullptr;

	if (UGridSlot* GridSlot = ItemGridPanel->AddChildToGrid(NewItemWidget, RowTopLeft, ColTopLeft))
	{
		GridSlot->SetRowSpan(ItemRows);
		GridSlot->SetColumnSpan(ItemCols);
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);

		const FOBGridItemInfo NewItemInfo(RowTopLeft, ColTopLeft, ItemRows, ItemCols, ItemDataSource, ItemPayload);
		PlacedItemInfoMap.Add(NewItemWidget, NewItemInfo);

		UE_LOG(LogTemp, Log, TEXT("[%s::%hs] - Added '%s' at (Row:%d, Col:%d), Span(Rows:%d, Cols:%d)"),
			   *GetNameSafe(this), __FUNCTION__, *NewItemWidget->GetName(), RowTopLeft, ColTopLeft, ItemRows, ItemCols);

		UpdateDummyCells();
		OnItemAdded.Broadcast(NewItemWidget, NewItemInfo);
		return NewItemWidget;
	}

	return nullptr;
}


// --- Helpers ---

bool UOBGridInventoryWidget::FindFreeSlot(const int32 ItemRows, const int32 ItemCols, int32& OutRow,
										  int32& OutCol) const
{
	if (GridConfig.NumRows < ItemRows || GridConfig.NumColumns < ItemCols) return false;

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
												const int32 ItemCols, UUserWidget* IgnoredWidget) const
{
	const FIntRect NewItemRect(TopLeftCol, TopLeftRow, TopLeftCol + ItemCols, TopLeftRow + ItemRows);
	for (const TPair<TObjectPtr<UUserWidget>, FOBGridItemInfo>& Pair : PlacedItemInfoMap)
	{
		if (!Pair.Key || Pair.Key == IgnoredWidget) continue;
		const FOBGridItemInfo& ExistingInfo = Pair.Value;
		const FIntRect ExistingItemRect(ExistingInfo.Column, ExistingInfo.Row,
										ExistingInfo.Column + ExistingInfo.ColumnSpan,
										ExistingInfo.Row + ExistingInfo.RowSpan);
		if (NewItemRect.Intersect(ExistingItemRect))
		{
			return false;
		}
	}
	return true;
}

void UOBGridInventoryWidget::UpdateGridBackground() const
{
	if (GridBackground != nullptr)
	{
		GridBackground->UpdateGridParameters(GridConfig);
	}
}

bool UOBGridInventoryWidget::CalculateCurrentScale(const FGeometry& CurrentGeometry)
{
	if (GridConfig.NumRows <= 0 || GridConfig.NumColumns <= 0 || GridConfig.CellSize <= KINDA_SMALL_NUMBER)
		return
			false;
	const FVector2D AllocatedSize = CurrentGeometry.GetLocalSize();
	if (AllocatedSize.X <= KINDA_SMALL_NUMBER || AllocatedSize.Y <= KINDA_SMALL_NUMBER) return false;
	const float TargetWidth = static_cast<float>(GridConfig.NumColumns) * GridConfig.CellSize;
	const float TargetHeight = static_cast<float>(GridConfig.NumRows) * GridConfig.CellSize;
	if (TargetWidth <= KINDA_SMALL_NUMBER || TargetHeight <= KINDA_SMALL_NUMBER) return false;

	const float ScaleX = AllocatedSize.X / TargetWidth;
	const float ScaleY = AllocatedSize.Y / TargetHeight;
	CurrentGridScale = FMath::Min(ScaleX, ScaleY);
	return true;
}

void UOBGridInventoryWidget::UpdateSizeBoxOverride() const
{
	if (!GridSizeBox) return;
	const float ScaledWidth = static_cast<float>(GridConfig.NumColumns) * GridConfig.CellSize * CurrentGridScale;
	const float ScaledHeight = static_cast<float>(GridConfig.NumRows) * GridConfig.CellSize * CurrentGridScale;
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
	if (!CalculateCurrentScale(CurrentGeometry)) return;
	UpdateSizeBoxOverride();
	if (ItemGridPanel)
	{
		ItemGridPanel->InvalidateLayoutAndVolatility();
	}
}

void UOBGridInventoryWidget::UpdateDummyCells()
{
	if (!ItemGridPanel || !DummyCellWidgetClass || GridConfig.NumRows <= 0 || GridConfig.NumColumns <= 0)
	{
		return;
	}

	TSet<FIntPoint> OccupiedCells;
	for (const auto& Pair : PlacedItemInfoMap)
	{
		if (!Pair.Key) continue;
		const auto& Info = Pair.Value;
		for (int32 r = Info.Row; r < Info.Row + Info.RowSpan; ++r)
		{
			for (int32 c = Info.Column; c < Info.Column + Info.ColumnSpan; ++c)
			{
				OccupiedCells.Add(FIntPoint(c, r));
			}
		}
	}

	TSet<FIntPoint> DummiesToRemove;
	for (const auto& Pair : DummyCellWidgetsMap)
	{
		if (OccupiedCells.Contains(Pair.Key) || !Pair.Value.IsValid())
		{
			DummiesToRemove.Add(Pair.Key);
		}
	}

	for (const FIntPoint& Coord : DummiesToRemove)
	{
		RemoveDummyWidgetAt(Coord);
	}

	for (int32 r = 0; r < GridConfig.NumRows; ++r)
	{
		for (int32 c = 0; c < GridConfig.NumColumns; ++c)
		{
			const FIntPoint CurrentCoord(c, r);
			if (!OccupiedCells.Contains(CurrentCoord) && !DummyCellWidgetsMap.Contains(CurrentCoord))
			{
				TryAddDummyWidgetAt(r, c);
			}
		}
	}
}

bool UOBGridInventoryWidget::TryAddDummyWidgetAt(const int32 Row, const int32 Column)
{
	if (!ItemGridPanel || !DummyCellWidgetClass) return false;
	const FIntPoint Coord(Column, Row);
	if (DummyCellWidgetsMap.Contains(Coord) && DummyCellWidgetsMap[Coord].IsValid()) return true;

	if (UUserWidget* NewDummyWidget = CreateWidget<UUserWidget>(this, DummyCellWidgetClass))
	{
		if (UGridSlot* GridSlot = ItemGridPanel->AddChildToGrid(NewDummyWidget, Row, Column))
		{
			GridSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
			GridSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
			DummyCellWidgetsMap.Add(Coord, NewDummyWidget);
			return true;
		}
	}
	return false;
}

void UOBGridInventoryWidget::RemoveDummyWidgetAt(const FIntPoint& Coord)
{
	if (const TWeakObjectPtr<UUserWidget>* FoundWidgetPtr = DummyCellWidgetsMap.Find(Coord))
	{
		if (UUserWidget* DummyWidget = FoundWidgetPtr->Get())
		{
			if (ItemGridPanel)
			{
				ItemGridPanel->RemoveChild(DummyWidget);
			}
		}
		DummyCellWidgetsMap.Remove(Coord);
	}
}

void UOBGridInventoryWidget::SetupGridPanelDimensions()
{
	if (!ItemGridPanel) return;
	ItemGridPanel->ClearChildren();
	PlacedItemInfoMap.Empty();
	DummyCellWidgetsMap.Empty();
	for (int32 c = 0; c < GridConfig.NumColumns; ++c)
	{
		ItemGridPanel->SetColumnFill(c, 1.0f);
	}
	for (int32 r = 0; r < GridConfig.NumRows; ++r)
	{
		ItemGridPanel->SetRowFill(r, 1.0f);
	}
}
