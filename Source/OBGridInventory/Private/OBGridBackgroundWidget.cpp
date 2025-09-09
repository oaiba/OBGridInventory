// Fill out your copyright notice in the Description page of Project Settings.


#include "OBGridBackgroundWidget.h"

#include "Components/PanelWidget.h"

void UOBGridBackgroundWidget::UpdateGridParameters(const FOBGridInventoryConfig InGridConfig)
{
	NumRows = FMath::Max(1, InGridConfig.NumRows);
	NumColumns = FMath::Max(1, InGridConfig.NumColumns);
	CellSize = FMath::Max(1.0f, InGridConfig.CellSize);
	GridLineColor = InGridConfig.GridLineColor;
	GridLineThickness = FMath::Max(0.0f, InGridConfig.GridLineThickness);
	BorderLineThickness = FMath::Max(0.0f, InGridConfig.BorderLineThickness);
	bIsShowNameOnTopLeftCorner = InGridConfig.bIsShowNameOnTopLeftCorner;
	// Trigger a repaint when configuration changes
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 UOBGridBackgroundWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                                           const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                                           int32 LayerId,
                                           const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// --- Basic validation ---
	if (NumRows <= 0 || NumColumns <= 0 || CellSize <= KINDA_SMALL_NUMBER)
		return LayerId;

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	if (LocalSize.X <= KINDA_SMALL_NUMBER || LocalSize.Y <= KINDA_SMALL_NUMBER)
		return LayerId;

	// --- Scaling logic ---
	const float TargetWidth = NumColumns * CellSize;
	const float TargetHeight = NumRows * CellSize;
	const float Scale = FMath::Min(LocalSize.X / TargetWidth, LocalSize.Y / TargetHeight);

	const float ScaledCellSize = CellSize * Scale;
	const float ScaledMaxX = NumColumns * ScaledCellSize;
	const float ScaledMaxY = NumRows * ScaledCellSize;

	// Choose appropriate thickness for grid and borderlines
	const float ScaledGridLineThickness = FMath::Max(1.0f, GridLineThickness * Scale);
	const float ScaledBorderThickness = FMath::Max(1.0f, BorderLineThickness * Scale);

	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();
	const int32 CurrentLayerId = LayerId;

	// Check if we have any lines to draw
	if (GridLineColor.A <= 0)
	{
		return CurrentLayerId;
	}

	TArray<FVector2D> LinePoints;
	LinePoints.SetNumUninitialized(2);

	// --- Draw Vertical Lines & Left/Right Border ---
	for (int32 X = 0; X <= NumColumns; ++X)
	{
		const float LineX = X * ScaledCellSize;
		LinePoints[0] = FVector2D(LineX, 0);
		LinePoints[1] = FVector2D(LineX, ScaledMaxY);

		if (const float CurrentThickness = (X == 0 || X == NumColumns) ? ScaledBorderThickness : ScaledGridLineThickness
			; CurrentThickness > 0)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, CurrentLayerId, PaintGeometry, LinePoints,
			                             ESlateDrawEffect::None, GridLineColor, false, CurrentThickness);
		}
	}

	// --- Draw Horizontal Lines & Top/Bottom Border ---
	for (int32 Y = 0; Y <= NumRows; ++Y)
	{
		const float LineY = Y * ScaledCellSize;
		LinePoints[0] = FVector2D(0, LineY);
		LinePoints[1] = FVector2D(ScaledMaxX, LineY);

		if (const float CurrentThickness = (Y == 0 || Y == NumRows) ? ScaledBorderThickness : ScaledGridLineThickness;
			CurrentThickness > 0)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, CurrentLayerId, PaintGeometry, LinePoints,
			                             ESlateDrawEffect::None, GridLineColor, false, CurrentThickness);
		}
	}

	// --- Draw Debug Text (Design-time only) ---
	if (IsDesignTime() && bIsShowNameOnTopLeftCorner)
	{
		static int32 ShowTextLayerId = CurrentLayerId;
		FString OwnerName = TEXT("Owner: N/A");
		FString SizeInfo = FString::Printf(TEXT("WxH: %.1f x %.1f"), NumColumns * CellSize, NumRows * CellSize);

		if (GetParent() && GetParent()->GetOuter() && GetParent()->GetOuter()->GetOuter())
		{
			OwnerName = GetParent()->GetOuter()->GetOuter()->GetName();
		}
		FString FullText = FString::Printf(TEXT("%s\n%s"), *OwnerName, *SizeInfo);
		const FSlateFontInfo FontInfo = FCoreStyle::Get().GetFontStyle("SmallFont");
		constexpr FLinearColor TextColor = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f); // Gray

		const FVector2D Position(5.0f, 5.0f);
		// // move position to the center of this

		// const FVector2D Position = AllottedGeometry.GetAbsolutePositionAtCoordinates(FVector2f(0.5f, 0.5f));

		// Create a render transform to move to center
		const FSlateRenderTransform CenteredTransform(Position);
		FPaintGeometry TextGeometry = PaintGeometry;
		TextGeometry.SetRenderTransform(Concatenate(PaintGeometry.GetAccumulatedRenderTransform(), CenteredTransform));

		FSlateDrawElement::MakeText(
			OutDrawElements,
			++ShowTextLayerId,
			TextGeometry,
			FullText,
			FontInfo,
			ESlateDrawEffect::None,
			TextColor
		);
	}

	return CurrentLayerId;
}
