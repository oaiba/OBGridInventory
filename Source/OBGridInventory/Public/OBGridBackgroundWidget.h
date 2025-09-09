// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OBGridBackgroundWidget.generated.h"

USTRUCT(BlueprintType)
struct FOBGridInventoryConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Grid|Config",
		meta = (ClampMin = "1", UIMin = "1"))
	int32 NumRows = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Grid|Config",
		meta = (ClampMin = "1", UIMin = "1"))
	int32 NumColumns = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Grid|Config",
		meta = (ClampMin = "1.0", UIMin = "1.0"))
	float CellSize = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Grid|Config")
	FLinearColor GridLineColor = FLinearColor(0.1f, 0.1f, 0.1f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Grid|Config",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float GridLineThickness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Grid|Config",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BorderLineThickness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Grid|Config",
		meta = (DisplayName = "Show Name in Editor", Tooltip =
			"Show name of widget owner in top left corner during editor/designer time"))
	bool bIsShowNameOnTopLeftCorner = true;

	FOBGridInventoryConfig() = default;

	FOBGridInventoryConfig(const int32 InNumRows, const int32 InNumColumns, const float InCellSize,
	                       const FLinearColor InGridLineColor, const float InGridLineThickness)
		: NumRows(InNumRows), NumColumns(InNumColumns), CellSize(InCellSize),
		  GridLineColor(InGridLineColor), GridLineThickness(InGridLineThickness)
	{
		NumRows = FMath::Max(NumRows, 1);
		NumColumns = FMath::Max(NumColumns, 1);
		CellSize = FMath::Max(CellSize, 1.0f);
		GridLineThickness = FMath::Max(GridLineThickness, 0.0f);
		bIsShowNameOnTopLeftCorner = true;
	}
};

/**
 * 
 */
UCLASS()
class OBGRIDINVENTORY_API UOBGridBackgroundWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Updates the parameters used for drawing the grid lines. */
	UFUNCTION(BlueprintCallable, Category="Grid Background")
	void UpdateGridParameters(const FOBGridInventoryConfig InGridConfig);

protected:
	// Override NativePaint to draw the grid lines ONLY.
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	                          const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	                          const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// Store the grid parameters locally
	UPROPERTY(Transient)
	bool bIsShowNameOnTopLeftCorner = false;

	UPROPERTY(Transient)
	int32 NumRows = 10;

	UPROPERTY(Transient)
	int32 NumColumns = 10;

	UPROPERTY(Transient)
	float CellSize = 50.0f;

	UPROPERTY(Transient)
	FLinearColor GridLineColor = FLinearColor(0.1f, 0.1f, 0.1f, 0.5f);

	UPROPERTY(Transient)
	float GridLineThickness = 1.0f;

	UPROPERTY(Transient)
	float BorderLineThickness = 2.0f;
};
