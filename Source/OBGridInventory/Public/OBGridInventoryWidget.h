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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOBGridItemAdded, UUserWidget*, ItemWidget, const FOBGridItemInfo&, ItemInfo);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOBGridItemRemoved, UUserWidget*, ItemWidget);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOBGridItemMoved, UUserWidget*, ItemWidget, const FOBGridItemInfo&,
											 NewItemInfo);

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
	// --- Grid Configuration ---
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory")
	virtual void SetGridRows(const int32 NewGridRows);

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory")
	virtual void SetGridColumns(const int32 NewGridColumns);

	// --- Item Management ---

	/**
	 * Adds an item widget to the first available free slot in the grid.
	 * @param ItemDataSource The data object associated with this item.
	 * @param ItemRows The number of rows the item occupies.
	 * @param ItemCols The number of columns the item occupies.
	 * @param CustomItemWidgetClass (Optional) A specific widget class to use for this item. If null, the default ItemWidgetClass will be used.
	 * @return The created widget, or nullptr if unsuccessful.
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory")
	UUserWidget* AddItemWidget(UObject* ItemDataSource, const int32 ItemRows = 1, const int32 ItemCols = 1,
							   TSubclassOf<UUserWidget> CustomItemWidgetClass = nullptr);

	/**
	 * Adds an item widget to a specific slot in the grid.
	 * @param ItemDataSource The data object associated with this item.
	 * @param ItemRows The number of rows the item occupies.
	 * @param ItemCols The number of columns the item occupies.
	 * @param RowTopLeft The top-left row to place the item.
	 * @param ColTopLeft The top-left column to place the item.
	 * @param CustomItemWidgetClass (Optional) A specific widget class to use for this item. If null, the default ItemWidgetClass will be used.
	 * @return The created widget, or nullptr if unsuccessful.
	 */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory")
	UUserWidget* AddItemWidgetAt(UObject* ItemDataSource, const int32 ItemRows, const int32 ItemCols,
								 const int32 RowTopLeft, const int32 ColTopLeft,
								 TSubclassOf<UUserWidget> CustomItemWidgetClass = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Items")
	bool RemoveItemWidget(UUserWidget* ItemWidgetToRemove);

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Items")
	void ClearGrid();

	/** NEW: Moves an existing item widget to a new location on the grid. */
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Items")
	bool MoveItemWidget(UUserWidget* ItemWidgetToMove, int32 NewRowTopLeft, int32 NewColTopLeft);

	// --- Querying ---
	bool IsAreaClear(int32 TopLeftRow, int32 TopLeftCol, int32 ItemRows, int32 ItemCols) const;

	/** Gets all item widgets currently in the grid. */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Querying")
	void GetAllItemWidgets(TArray<UUserWidget*>& OutItemWidgets) const;

	/** Finds the widget associated with a given data source. */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Querying")
	UUserWidget* GetItemWidgetFromDataSource(UObject* DataSource) const;

	/** Gets the grid information for a specific item widget. */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Querying")
	bool GetItemInfo(UUserWidget* ItemWidget, FOBGridItemInfo& OutItemInfo) const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Querying")
	bool GetItemAt(int32 TopLeftRow, int32 TopLeftCol, UUserWidget*& OutItemWidget, UObject*& OutItemDataSource) const;

public: // --- Events ---

	UPROPERTY(BlueprintAssignable, Category = "Grid Inventory|Events")
	FOnOBGridItemAdded OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category = "Grid Inventory|Events")
	FOnOBGridItemRemoved OnItemRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Grid Inventory|Events")
	FOnOBGridItemMoved OnItemMoved;

protected:
	/**
	 * Internal function that validates inputs for adding an item.
	 */
	bool ValidateAddItemInputs(UObject* ItemDataSource, const int32 ItemRows, const int32 ItemCols,
							   TSubclassOf<UUserWidget> CustomItemWidgetClass) const;

	/**
	 * The core internal function that creates and places the widget on the grid.
	 */
	UUserWidget* AddItemWidgetInternal(UObject* ItemDataSource, const int32 ItemRows, const int32 ItemCols,
									   const int32 RowTopLeft, const int32 ColTopLeft,
									   TSubclassOf<UUserWidget> CustomItemWidgetClass);

protected: // --- Configuration Properties ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Inventory|Config")
	FOBGridInventoryConfig GridConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid Inventory|Config")
	TSubclassOf<UUserWidget> ItemWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid Inventory|Config")
	TSubclassOf<UUserWidget> DummyCellWidgetClass;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UOBGridBackgroundWidget> GridBackground = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Grid Inventory")
	TObjectPtr<USizeBox> GridSizeBox = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Grid Inventory")
	TObjectPtr<UOverlay> GridOverlay = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Grid Inventory")
	TObjectPtr<UGridPanel> ItemGridPanel = nullptr;

private:
	/**
	 * Transient mapping associating user widget objects with their corresponding
	 * grid item metadata. This map is used to track and manage the placement
	 * of items within a grid system during runtime.
	 */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UUserWidget>, FOBGridItemInfo> PlacedItemInfoMap;

	/** NEW: Maps a data source to its corresponding widget for quick lookups. */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UObject>, TObjectPtr<UUserWidget>> DataSourceToWidgetMap;

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
	 * Attempts to find a free slot in the Grid Inventory where an item of the specified dimensions can fit.
	 * Iterates through available grid cells to determine a valid position for the item's placement.
	 *
	 * @param ItemRows The number of rows the item occupies.
	 * @param ItemCols The number of columns the item occupies.
	 * @param OutRow Reference to a variable where the starting row index of the free slot (if found) will be stored.
	 * @param OutCol Reference to a variable where the starting column index of the free slot (if found) will be stored.
	 * @return true if a suitable free slot is found; false otherwise.
	 */
	bool FindFreeSlot(const int32 ItemRows, const int32 ItemCols, int32& OutRow, int32& OutCol) const;

	bool IsAreaClearForMove(int32 TopLeftRow, int32 TopLeftCol, int32 ItemRows, int32 ItemCols,
							UUserWidget* IgnoredWidget) const;

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
