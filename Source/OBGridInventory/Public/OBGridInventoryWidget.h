// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBGridBackgroundWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/SizeBox.h"
#include "OBGridInventoryWidget.generated.h"

class UGridPanel;
class UOverlay;
/**
 * Structure representing the metadata of an item within a grid system.
 * Encapsulates information about the item's position and size in the grid
 * and provides utility to check if a specific cell is contained within the
 * defined bounds of the item.
 */
USTRUCT(BlueprintType)
struct FOBGridItemInfo
{
	GENERATED_BODY()

	// Row of the top-left corner
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="OB|Grid Item")
	int32 Row = 0;

	// Column of the top-left corner
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="OB|Grid Item")
	int32 Column = 0;

	// Number of rows the item occupies
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="OB|Grid Item")
	int32 RowSpan = 1;

	// Number of columns the item occupies
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="OB|Grid Item")
	int32 ColumnSpan = 1;

	/**
	 * Represents the last recorded central position of an item in grid coordinates.
	 * Used to store the center point of the item relative to the grid system.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OB|Grid Item")
	FVector2D LastCenter = FVector2D(-1.0f, -1.0f);

	FOBGridItemInfo() = default;

	FOBGridItemInfo(const int32 InRow, const int32 InCol, const int32 InRowSpan, const int32 InColSpan)
		: Row(InRow), Column(InCol), RowSpan(InRowSpan), ColumnSpan(InColSpan)
	{
	}

	bool ContainsCell(const int32 CheckRow, const int32 CheckCol) const
	{
		return CheckRow >= Row && CheckRow < (Row + RowSpan) &&
			CheckCol >= Column && CheckCol < (Column + ColumnSpan);
	}
};

/**
 * 
 */
UCLASS()
class OBGRIDINVENTORY_API UOBGridInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:

protected: // --- Overrides ---

	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnInitialized() override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry,
	                                            const FNavigationEvent& InNavigationEvent,
	                                            const FNavigationReply& InDefaultReply) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory Grid")
	virtual void SetGridRows(const int32 NewGridRows);

	UFUNCTION(BlueprintCallable, Category = "Inventory Grid")
	virtual void SetGridColumns(const int32 NewGridColumns);

	UFUNCTION(BlueprintCallable, Category = "Inventory Grid")
	UUserWidget* AddItemWidget(UObject* ItemDataSource, const int32 ItemRows = 1, const int32 ItemCols = 1);

	UFUNCTION(BlueprintCallable, Category = "Inventory Grid")
	UUserWidget* AddItemWidgetAt(UObject* ItemDataSource, const int32 ItemRows, const int32 ItemCols,
	                             const int32 RowTopLeft, const int32 ColTopLeft);
	bool ValidateAddItemInputs(UObject* ItemDataSource, int32 ItemRows, int32 ItemCols) const;
	UUserWidget* AddItemWidgetInternal(UObject* ItemDataSource, int32 ItemRows, int32 ItemCols, int32 RowTopLeft,
	                                   int32 ColTopLeft);

	UFUNCTION(BlueprintCallable, Category = "Inventory Grid|Items")
	bool RemoveItemWidget(UUserWidget* ItemWidgetToRemove);

	UFUNCTION(BlueprintCallable, Category = "Inventory Grid")
	void ClearGrid();
	bool IsAreaClear(int32 TopLeftRow, int32 TopLeftCol, int32 ItemRows, int32 ItemCols) const;

protected: // --- Configuration Properties ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Grid|Config")
	FOBGridInventoryConfig GridConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory Grid|Config")
	TSubclassOf<UUserWidget> ItemWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory Grid|Config")
	TSubclassOf<UUserWidget> DummyCellWidgetClass;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UOBGridBackgroundWidget> GridBackground = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory Grid")
	TObjectPtr<USizeBox> GridSizeBox = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory Grid")
	TObjectPtr<UOverlay> GridOverlay = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory Grid")
	TObjectPtr<UGridPanel> ItemGridPanel = nullptr;

private:
	/**
	 * Transient mapping associating user widget objects with their corresponding
	 * grid item metadata. This map is used to track and manage the placement
	 * of items within a grid system during runtime.
	 */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UUserWidget>, FOBGridItemInfo> PlacedItemInfoMap;

	/**
	 * Map to keep track of currently active placeholder cell widgets.
	 * Key: Coordinate (Column, Row) of the placeholder cell.
	 * Value: Weak pointer to the placeholder widget instance.
	 * Marked Transient as its runtime visual state.
	 */
	UPROPERTY(Transient)
	TMap<FIntPoint, TWeakObjectPtr<UUserWidget>> DummyCellWidgetsMap;

	float CurrentGridScale = 1.0f;
	FVector2D LastKnownAllocatedSize = FVector2D(-1.0f, -1.0f);

private: //Helper

	/**
	 * Attempts to find a free slot in the inventory grid where an item of the specified dimensions can fit.
	 * Iterates through available grid cells to determine a valid position for the item's placement.
	 *
	 * @param ItemRows The number of rows the item occupies.
	 * @param ItemCols The number of columns the item occupies.
	 * @param OutRow Reference to a variable where the starting row index of the free slot (if found) will be stored.
	 * @param OutCol Reference to a variable where the starting column index of the free slot (if found) will be stored.
	 * @return true if a suitable free slot is found; false otherwise.
	 */
	bool FindFreeSlot(const int32 ItemRows, const int32 ItemCols, int32& OutRow, int32& OutCol) const;

private: // Helper Grid Paint

	virtual void UpdateGridBackground() const;
	bool CalculateCurrentScale(const FGeometry& CurrentGeometry);
	void UpdateSizeBoxOverride() const;
	void RecalculateScaleAndRefreshLayout(const FGeometry& CurrentGeometry);
	void UpdateDummyCells();
	bool TryAddDummyWidgetAt(const int32 Row, const int32 Column);
	void RemoveDummyWidgetAt(const FIntPoint& Coord);
	void SetupGridPanelDimensions();

protected:
	TPair<TWeakObjectPtr<UWidget>, FOBGridItemInfo> LastFocusedItemInfo;
};
