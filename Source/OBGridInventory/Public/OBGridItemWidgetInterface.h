// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OBGridInventoryWidget.h"
#include "UObject/Interface.h"
#include "OBGridItemWidgetInterface.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UOBGridItemWidgetInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class OBGRIDINVENTORY_API IOBGridItemWidgetInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	/**
	 * Called by the Grid Inventory right after this widget is created and placed.
	 * This is the primary entry point for the widget to receive its data and initialize its appearance.
	 *
	 * @param ItemInfo The complete information about the item, including its data source and custom payload.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Grid Item Widget")
	void OnItemInitialized(const FOBGridItemInfo& ItemInfo);
};
