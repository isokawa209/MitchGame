// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActionRPG.h"
#include "Items/RPGItem.h"
#include "GameFramework/SaveGame.h"
#include "RPGSaveGame.generated.h"

/** List of versions, native code will handle fixups for any old versions */
namespace ERPGSaveGameVersion
{
	enum type
	{
		// Initial version
		Initial,
		// Added Inventory
		AddedInventory,
		// Added ItemData to store count/level
		AddedItemData,

		// -----<new versions must be added before this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
}

USTRUCT(BlueprintType)
struct FInventoryIdStruct
{
	GENERATED_USTRUCT_BODY()

		FInventoryIdStruct()
		:ItemData()
		{}

		FInventoryIdStruct(FPrimaryAssetId ItemId, FRPGItemData ItemData)
		:ItemId(ItemId),
		ItemData(ItemData)
		{}


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
		FPrimaryAssetId ItemId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
		FRPGItemData ItemData;

	bool operator==(const FInventoryIdStruct& Other) const
	{
		return ItemId == Other.ItemId && ItemData == Other.ItemData;
	}
	bool operator!=(const FInventoryIdStruct& Other) const
	{
		return !(*this == Other);
	}

};

/** Object that is written to and read from the save game archive, with a data version */
UCLASS(BlueprintType)
class ACTIONRPG_API URPGSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Constructor */
	URPGSaveGame()
	{
		// Set to current version, this will get overwritten during serialization when loading
		SavedDataVersion = ERPGSaveGameVersion::LatestVersion;
	}

	/** Map of items to item data */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = SaveGame)
	TArray<FInventoryIdStruct> InventoryIdData;

	/** Map of slotted items */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = SaveGame)
	TMap<FRPGItemSlot, FPrimaryAssetId> SlottedItems;

	/** User's unique id */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = SaveGame)
	FString UserId;

protected:
	/** Deprecated way of storing items, this is read in but not saved out */
	UPROPERTY()
	TArray<FPrimaryAssetId> InventoryItems_DEPRECATED;

	/** What LatestVersion was when the archive was saved */
	UPROPERTY()
	int32 SavedDataVersion;

	/** Overridden to allow version fixups */
	virtual void Serialize(FArchive& Ar) override;
};
