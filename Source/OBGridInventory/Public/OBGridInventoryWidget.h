// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InstancedStruct.h" // Required for FInstancedStruct
#include "OBGridBackgroundWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/SizeBox.h"
#include "StructUtils/InstancedStruct.h"
#include "OBGridInventoryWidget.generated.h"

class UGridPanel;
class UOverlay;

/**
 * Structure representing the complete metadata of an item within the grid.
 * It now encapsulates position, size, the source data asset, and a flexible custom data payload.
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

	/** The static data source object (e.g., a UDataAsset) associated with this item instance. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="OB|Grid Item", Transient)
	TObjectPtr<UObject> ItemDataSource = nullptr;

	/** A flexible payload for any custom, dynamic data (e.g., durability, stats). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OB|Grid Item")
	FInstancedStruct ItemPayload;

	/** The last recorded central position of the item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OB|Grid Item")
	FVector2D LastCenter = FVector2D(-1.0f, -1.0f);

	FOBGridItemInfo() = default;

	FOBGridItemInfo(const int32 InRow, const int32 InCol, const int32 InRowSpan, const int32 InColSpan,
					UObject* InDataSource, const FInstancedStruct& InPayload)
		: Row(InRow), Column(InCol), RowSpan(InRowSpan), ColumnSpan(InColSpan), ItemDataSource(InDataSource),
		  ItemPayload(InPayload)
	{
	}

	bool ContainsCell(const int32 CheckRow, const int32 CheckCol) const
	{
		return CheckRow >= Row && CheckRow < (Row + RowSpan) &&
			CheckCol >= Column && CheckCol < (Column + ColumnSpan);
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOBGridItemAdded, UUserWidget*, ItemWidget, const FOBGridItemInfo&,
											 ItemInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOBGridItemRemoved, UUserWidget*, ItemWidget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOBGridItemMoved, UUserWidget*, ItemWidget, const FOBGridItemInfo&,
											 NewItemInfo);

UCLASS()
class OBGRIDINVENTORY_API UOBGridInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Overrides ---
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnInitialized() override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry,
	                                            const FNavigationEvent& InNavigationEvent,
	                                            const FNavigationReply& InDefaultReply) override;

public:
	// --- Grid Configuration ---
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Configuration")
	virtual void SetGridRows(const int32 NewGridRows);

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Configuration")
	virtual void SetGridColumns(const int32 NewGridColumns);

	// --- Item Management ---
	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Items", meta=(DisplayName="Add Item Widget (Auto-Placement)"))
	UUserWidget* AddItemWidget(UObject* ItemDataSource, const FInstancedStruct& ItemPayload, int32 ItemRows = 1,
							   int32 ItemCols = 1, TSubclassOf<UUserWidget> CustomItemWidgetClass = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Items", meta=(DisplayName="Add Item Widget at Slot"))
	UUserWidget* AddItemWidgetAt(UObject* ItemDataSource, const FInstancedStruct& ItemPayload, int32 ItemRows,
								 int32 ItemCols, int32 RowTopLeft, int32 ColTopLeft,
								 TSubclassOf<UUserWidget> CustomItemWidgetClass = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Items")
	bool RemoveItemWidget(UUserWidget* ItemWidgetToRemove);

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Items")
	void ClearGrid();

	UFUNCTION(BlueprintCallable, Category = "Grid Inventory|Items")
	bool MoveItemWidget(UUserWidget* ItemWidgetToMove, int32 NewRowTopLeft, int32 NewColTopLeft);

	// --- Querying ---
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Querying")
	bool IsAreaClear(int32 TopLeftRow, int32 TopLeftCol, int32 ItemRows, int32 ItemCols) const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Querying")
	void GetAllItemWidgets(TArray<UUserWidget*>& OutItemWidgets) const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Querying")
	UUserWidget* GetItemWidgetFromDataSource(UObject* DataSource) const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Querying")
	bool GetItemInfo(UUserWidget* ItemWidget, FOBGridItemInfo& OutItemInfo) const;

	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Querying")
	bool GetItemAt(int32 TopLeftRow, int32 TopLeftCol, UUserWidget*& OutItemWidget, UObject*& OutItemDataSource,
				   FInstancedStruct& OutItemPayload) const;

	/** NEW: Gets the custom data payload for a specific item widget. */
	UFUNCTION(BlueprintPure, Category = "Grid Inventory|Querying")
	bool GetItemPayload(UUserWidget* ItemWidget, FInstancedStruct& OutItemPayload) const;

public:
	// --- Events ---
	UPROPERTY(BlueprintAssignable, Category = "Grid Inventory|Events")
	FOnOBGridItemAdded OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category = "Grid Inventory|Events")
	FOnOBGridItemRemoved OnItemRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Grid Inventory|Events")
	FOnOBGridItemMoved OnItemMoved;

protected:
	// --- Internal ---
	bool ValidateAddItemInputs(UObject* ItemDataSource, int32 ItemRows, int32 ItemCols,
	                           TSubclassOf<UUserWidget> CustomItemWidgetClass) const;

	UUserWidget* AddItemWidgetInternal(UObject* ItemDataSource, const FInstancedStruct& ItemPayload, int32 ItemRows,
									   int32 ItemCols, int32 RowTopLeft, int32 ColTopLeft,
									   TSubclassOf<UUserWidget> CustomItemWidgetClass);

protected:
	// --- Configuration Properties ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Inventory|Config")
	FOBGridInventoryConfig GridConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid Inventory|Config")
	TSubclassOf<UUserWidget> ItemWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid Inventory|Config")
	TSubclassOf<UUserWidget> DummyCellWidgetClass;

	// --- Bound Widgets ---
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UOBGridBackgroundWidget> GridBackground = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Grid Inventory")
	TObjectPtr<USizeBox> GridSizeBox = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Grid Inventory")
	TObjectPtr<UOverlay> GridOverlay = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Grid Inventory")
	TObjectPtr<UGridPanel> ItemGridPanel = nullptr;

private:
	// --- Runtime Data ---
	UPROPERTY(Transient)
	TMap<TObjectPtr<UUserWidget>, FOBGridItemInfo> PlacedItemInfoMap;

	UPROPERTY(Transient)
	TMap<FIntPoint, TWeakObjectPtr<UUserWidget>> DummyCellWidgetsMap;

	float CurrentGridScale = 1.0f;
	FVector2D LastKnownAllocatedSize = FVector2D(-1.0f, -1.0f);

private:
	// --- Helpers ---
	bool FindFreeSlot(int32 ItemRows, int32 ItemCols, int32& OutRow, int32& OutCol) const;
	bool IsAreaClearForMove(int32 TopLeftRow, int32 TopLeftCol, int32 ItemRows, int32 ItemCols,
	                        UUserWidget* IgnoredWidget) const;
	void UpdateGridBackground() const;
	bool CalculateCurrentScale(const FGeometry& CurrentGeometry);
	void UpdateSizeBoxOverride() const;
	void RecalculateScaleAndRefreshLayout(const FGeometry& CurrentGeometry);
	void UpdateDummyCells();
	bool TryAddDummyWidgetAt(int32 Row, int32 Column);
	void RemoveDummyWidgetAt(const FIntPoint& Coord);
	void SetupGridPanelDimensions();
};