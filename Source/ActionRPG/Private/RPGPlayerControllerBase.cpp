// Copyright Epic Games, Inc. All Rights Reserved.

#include "RPGPlayerControllerBase.h"
#include "RPGCharacterBase.h"
#include "RPGGameInstanceBase.h"
#include "RPGSaveGame.h"
#include "Items/RPGItem.h"

bool ARPGPlayerControllerBase::AddInventoryItem(URPGItem* NewItem, int32 ItemCount, int32 ItemLevel, bool bAutoSlot)
{
	bool bChanged = false;
	if (!NewItem)
	{
		UE_LOG(LogActionRPG, Warning, TEXT("AddInventoryItem: Failed trying to add null item!"));
		return false;
	}

	if (ItemCount <= 0 || ItemLevel <= 0)
	{
		UE_LOG(LogActionRPG, Warning, TEXT("AddInventoryItem: Failed trying to add item %s with negative count or level!"), *NewItem->GetName());
		return false;
	}

	// Find current item data, which may be empty
	TMap<int32, FInventoryStruct> OldData;
	GetInventoryItemData(NewItem, OldData);

	ARPGCharacterBase* OwningCharacter = Cast<ARPGCharacterBase>(GetCharacter());
	int32 InventorySize = OwningCharacter->GetInventorySize();

	// Find modified data
	FRPGItemData NewData;
	for (TPair<int32, FInventoryStruct> Struct : OldData) {
		if (Struct.Key == INDEX_NONE)
		{
			if (InventoryData.Num() < InventorySize + 10)
			{

				NewData = Struct.Value.ItemData;

				NewData.UpdateItemData(FRPGItemData(ItemCount, ItemLevel), NewItem->MaxCount, NewItem->MaxLevel);

				NewData.ItemCount = NewData.ItemCount + ItemCount;

				InventoryData.Add(FInventoryStruct(NewItem, NewData));

				NotifyInventoryItemChanged(true, NewItem);
				bChanged = true;
				break;
			}
			else
			{
				for (int32 ArrayIndex = 10; ArrayIndex < InventoryData.Num(); ArrayIndex++) {
					if (InventoryData[ArrayIndex].Item == nullptr)
					{
						InventoryData[ArrayIndex] = FInventoryStruct(NewItem, NewData);
						NotifyInventoryItemChanged(true, NewItem);
						bChanged = true;
						break;
					}
				}
				break;
			}
		}
		else
		{
			NewData = InventoryData[Struct.Key].ItemData;

			NewData.UpdateItemData(FRPGItemData(ItemCount, ItemLevel), NewItem->MaxCount, NewItem->MaxLevel);

			if (NewData.ItemCount + ItemCount < NewItem->MaxCount)
			{
				NewData.ItemCount = NewData.ItemCount + ItemCount;

				InventoryData[Struct.Key] = FInventoryStruct(NewItem, NewData);
			}
			else 
			{
				if (InventoryData.Num() < InventorySize + 10)
				{
					InventoryData.Add(FInventoryStruct(NewItem, NewData));
				}
				else
				{
					for (int32 ArrayIndex = 10; ArrayIndex < InventoryData.Num(); ArrayIndex++) {
						if (InventoryData[ArrayIndex].Item == nullptr)
						{
							InventoryData[ArrayIndex] = FInventoryStruct(NewItem, NewData);
							NotifyInventoryItemChanged(true, NewItem);
							bChanged = true;
							break;
						}
					}
					break;
				}
			}
			NotifyInventoryItemChanged(true, NewItem);
			bChanged = true;
			break;
		}


	}

	if (bAutoSlot)
	{
		// Slot item if required
		bChanged |= FillEmptySlotWithItem(NewItem);
	}

	if (bChanged)
	{
		// If anything changed, write to save game
		SaveInventory();
		return true;
	}
	return false;
}

bool ARPGPlayerControllerBase::SetInventoryItem(URPGItem* SetItem, bool& Result, int32 ItemCount, int32 ItemLevel, int32 ArrayIndex)
{
	bool bChanged = false;
	ARPGCharacterBase* OwningCharacter = Cast<ARPGCharacterBase>(GetCharacter());
	int32 InventorySize = OwningCharacter->GetInventorySize();

	if (!InventoryData.IsValidIndex(ArrayIndex - 1))
	{
		if (InventoryData.Num() < InventorySize + 10)
		{
			InventoryData.SetNum(ArrayIndex);
			InventoryData[ArrayIndex - 1] = FInventoryStruct(SetItem, FRPGItemData(ItemCount, ItemLevel));
			NotifyInventoryItemChanged(true, SetItem);
			bChanged = true;
			Result = true;
		}
		else
		{
			Result = false;
		}

	}
	else 
	{
		FInventoryStruct OldData = InventoryData[ArrayIndex - 1];
		if (SetItem == InventoryData[ArrayIndex - 1].Item)
		{
			
			OldData.ItemData.UpdateItemData(FRPGItemData(ItemCount, ItemLevel), SetItem->MaxCount, SetItem->MaxLevel);
			if (OldData.ItemData.ItemCount + ItemCount < SetItem->MaxCount)
			{
				OldData.ItemData.ItemCount = OldData.ItemData.ItemCount + ItemCount;

				InventoryData[ArrayIndex - 1] = OldData;
				Result = true;
			}
			else {
				if (InventoryData.Num() < InventorySize + 10)
				{
					InventoryData.Add(FInventoryStruct(SetItem, OldData.ItemData));
					Result = true;
				}
				else
				{
					Result = false;
				}
			}
			
			
		}
		else {
			if (InventoryData.Num() < InventorySize + 10)
			{
				InventoryData.Add(FInventoryStruct(SetItem, OldData.ItemData));
				Result = true;
			}
			else
			{
				Result = false;
			}
		}
		NotifyInventoryItemChanged(true, SetItem);
		bChanged = true;

	}
	
	if (bChanged)
	{
		// If anything changed, write to save game
		SaveInventory();
		return true;
	}
	return false;

}

void ARPGPlayerControllerBase::SortInventoryToSlotArea()
{
	UWorld* World = GetWorld();
	URPGGameInstanceBase* GameInstance = World ? World->GetGameInstance<URPGGameInstanceBase>() : nullptr;


	int32 WeaponIndex = *GameInstance->ItemSlotsPerType.Find(FName(TEXT("Weapon")));
	int32 SkillIndex = *GameInstance->ItemSlotsPerType.Find(FName(TEXT("Skill")));
	int32 PosionIndex = *GameInstance->ItemSlotsPerType.Find(FName(TEXT("Potion")));
	
	bool bChanged = false;

	TMap<int32, FInventoryStruct> FindMap;
	int32 ItemCount;
	int32 ItemIndex;

	for (TPair<FRPGItemSlot, URPGItem* > Pair : SlottedItems)
	{
		if (Pair.Value != nullptr)
		{

			GetInventoryItemData(Pair.Value, FindMap);
			ItemCount = 0;
			ItemIndex = 0;
			for (TPair<int32, FInventoryStruct> findPair : FindMap)
			{

				if (ItemCount == 0 || ItemCount > findPair.Value.ItemData.ItemCount)
				{
					ItemCount = findPair.Value.ItemData.ItemCount;
					ItemIndex = findPair.Key;
				}
			}
			if (ItemIndex != 0)
			{
				if (Pair.Value->ItemType.GetName() == FName(TEXT("Weapon")))
				{
					InventoryData.Swap(0 + Pair.Key.SlotNumber, ItemIndex);
					bChanged = true;
				}
				else if (Pair.Value->ItemType.GetName() == FName(TEXT("Skill")))
				{
					InventoryData.Swap(1 + Pair.Key.SlotNumber, ItemIndex);
					bChanged = true;
				}
				else if (Pair.Value->ItemType.GetName() == FName(TEXT("Potion")))
				{
					InventoryData.Swap(6 + Pair.Key.SlotNumber, ItemIndex);
					bChanged = true;
				}
				
			}
				
			
		}
		if (bChanged)
		{
			SaveInventory();
		}
	}

}

bool ARPGPlayerControllerBase::RemoveInventoryItem(URPGItem* RemovedItem, int32 RemoveCount)
{
	if (!RemovedItem)
	{
		UE_LOG(LogActionRPG, Warning, TEXT("RemoveInventoryItem: Failed trying to remove null item!"));
		return false;
	}

	TMap<int32, FInventoryStruct> ItemData;
	// Find current item data, which may be empty
	FRPGItemData NewData;
	int32 Index = 0;
	GetInventoryItemData(RemovedItem, ItemData);

	int32 ItemCount = 0;
	for (TPair<int32, FInventoryStruct>& Struct : ItemData)
	{
		if (ItemCount == 0 || ItemCount > Struct.Value.ItemData.ItemCount)
		{
			ItemCount = Struct.Value.ItemData.ItemCount;
			NewData = Struct.Value.ItemData;
			Index = Struct.Key;
		}
	}

	if (!NewData.IsValid())
	{
		// Wasn't found
		return false;
	}

	// If RemoveCount <= 0, delete all
	if (RemoveCount <= 0)
	{
		NewData.ItemCount = 0;
	}
	else
	{
		NewData.ItemCount -= RemoveCount;
	}

	if (NewData.ItemCount > 0)
	{
		// Update data with new count
		InventoryData[Index] = FInventoryStruct(RemovedItem, NewData);
	}
	else
	{
		// Remove item entirely, make sure it is unslotted
		InventoryData.RemoveAt(Index);

		for (TPair<FRPGItemSlot, URPGItem*>& Pair : SlottedItems)
		{
			if (Pair.Value == RemovedItem)
			{
				Pair.Value = nullptr;
				NotifySlottedItemChanged(Pair.Key, Pair.Value);
			}
		}
	}

	// If we got this far, there is a change so notify and save
	NotifyInventoryItemChanged(false, RemovedItem);

	SaveInventory();
	return true;
}


bool ARPGPlayerControllerBase::SwapInventoryItem(int32 FromIndex, FInventoryStruct FromItem, int32 ToIndex, FInventoryStruct ToItem)
{
	if (!InventoryData.IsValidIndex(ToIndex))
	{
		InventoryData.SetNum(ToIndex);
	}
	InventoryData.Swap(FromIndex, ToIndex-1);

	SaveInventory();
	return true;

}

void ARPGPlayerControllerBase::GetInventoryItems(TArray<URPGItem*>& Items, FPrimaryAssetType ItemType)
{
	for (const FInventoryStruct& Pair : InventoryData)
	{
		if (Pair.Item)
		{
			FPrimaryAssetId AssetId = Pair.Item->GetPrimaryAssetId();

			// Filters based on item type
			if (AssetId.PrimaryAssetType == ItemType || !ItemType.IsValid())
			{
				Items.Add(Pair.Item);
			}
		}	
	}
}

bool ARPGPlayerControllerBase::SetSlottedItem(FRPGItemSlot ItemSlot, URPGItem* Item)
{
	// Iterate entire inventory because we need to remove from old slot
	bool bFound = false;
	for (TPair<FRPGItemSlot, URPGItem*>& Pair : SlottedItems)
	{
		if (Pair.Key == ItemSlot)
		{
			// Add to new slot
			bFound = true;
			Pair.Value = Item;
			NotifySlottedItemChanged(Pair.Key, Pair.Value);
		}
		/*else if (Item != nullptr && Pair.Value == Item)
		{
			// If this item was found in another slot, remove it
			Pair.Value = nullptr;
			NotifySlottedItemChanged(Pair.Key, Pair.Value);
		}*/
	}

	if (bFound)
	{
		SaveInventory();
		return true;
	}

	return false;
}

int32 ARPGPlayerControllerBase::GetInventoryItemCount(URPGItem* Item) const
{
	int32 ReturnValue = 0;
	for (FInventoryStruct Struct : InventoryData)
	{
		if (Struct.Item == Item) {
			ReturnValue = ReturnValue + Struct.ItemData.ItemCount;
		}
	}
	return ReturnValue;
}

bool ARPGPlayerControllerBase::GetInventoryItemData(URPGItem* Item, TMap<int32 , FInventoryStruct>& GetMap) const
{
	for (int32 Index = 0 ; Index != InventoryData.Num() ; ++Index)
	{
		if (Item == InventoryData[Index].Item )
		{
			GetMap.Add(Index , InventoryData[Index]);
		}
	}
	if (GetMap.Find(0))
	{
		return true;
	}
	GetMap.Add(INDEX_NONE , FInventoryStruct(Item ,FRPGItemData(0, 0)));
	return false;
}

URPGItem* ARPGPlayerControllerBase::GetSlottedItem(FRPGItemSlot ItemSlot) const
{
	URPGItem* const* FoundItem = SlottedItems.Find(ItemSlot);

	if (FoundItem)
	{
		return *FoundItem;
	}
	return nullptr;
}

void ARPGPlayerControllerBase::GetSlottedItems(TArray<URPGItem*>& Items, FPrimaryAssetType ItemType, bool bOutputEmptyIndexes)
{
	for (TPair<FRPGItemSlot, URPGItem*>& Pair : SlottedItems)
	{
		if (Pair.Key.ItemType == ItemType || !ItemType.IsValid())
		{
			Items.Add(Pair.Value);
		}
	}
}

void ARPGPlayerControllerBase::FillEmptySlots()
{
	bool bShouldSave = false;
	for (const FInventoryStruct& Pair : InventoryData)
	{
		if (Pair.Item)
		{
			bShouldSave |= FillEmptySlotWithItem(Pair.Item);
		}
	}

	if (bShouldSave)
	{
		SaveInventory();
	}
}

bool ARPGPlayerControllerBase::SaveInventory()
{
	UWorld* World = GetWorld();
	URPGGameInstanceBase* GameInstance = World ? World->GetGameInstance<URPGGameInstanceBase>() : nullptr;

	if (!GameInstance)
	{
		return false;
	}

	URPGSaveGame* CurrentSaveGame = GameInstance->GetCurrentSaveGame();
	if (CurrentSaveGame)
	{
		// Reset cached data in save game before writing to it
		CurrentSaveGame->InventoryIdData.Reset();
		CurrentSaveGame->SlottedItems.Reset();

		for (const FInventoryStruct& ItemPair : InventoryData)
		{
			FPrimaryAssetId AssetId;

			if (ItemPair.Item)
			{
				AssetId = ItemPair.Item->GetPrimaryAssetId();
				CurrentSaveGame->InventoryIdData.Add(FInventoryIdStruct(AssetId, ItemPair.ItemData));
			}
			else
			{
				CurrentSaveGame->InventoryIdData.Add(FInventoryIdStruct());
			}
		}

		for (const TPair<FRPGItemSlot, URPGItem*>& SlotPair : SlottedItems)
		{
			FPrimaryAssetId AssetId;

			if (SlotPair.Value)
			{
				AssetId = SlotPair.Value->GetPrimaryAssetId();
			}
			CurrentSaveGame->SlottedItems.Add(SlotPair.Key, AssetId);
		}

		// Now that cache is updated, write to disk
		GameInstance->WriteSaveGame();
		return true;
	}
	return false;
}

bool ARPGPlayerControllerBase::LoadInventory()
{
	InventoryData.Reset();
	SlottedItems.Reset();

	// Fill in slots from game instance
	UWorld* World = GetWorld();
	URPGGameInstanceBase* GameInstance = World ? World->GetGameInstance<URPGGameInstanceBase>() : nullptr;

	if (!GameInstance)
	{
		return false;
	}

	// Bind to loaded callback if not already bound
	if (!GameInstance->OnSaveGameLoadedNative.IsBoundToObject(this))
	{
		GameInstance->OnSaveGameLoadedNative.AddUObject(this, &ARPGPlayerControllerBase::HandleSaveGameLoaded);
	}

	for (const TPair<FPrimaryAssetType, int32>& Pair : GameInstance->ItemSlotsPerType)
	{
		for (int32 SlotNumber = 0; SlotNumber < Pair.Value; SlotNumber++)
		{
			SlottedItems.Add(FRPGItemSlot(Pair.Key, SlotNumber), nullptr);
		}
	}

	URPGSaveGame* CurrentSaveGame = GameInstance->GetCurrentSaveGame();
	URPGAssetManager& AssetManager = URPGAssetManager::Get();
	if (CurrentSaveGame)
	{
		// Copy from save game into controller data
		bool bFoundAnySlots = false;
		for (const FInventoryIdStruct& ItemPair : CurrentSaveGame->InventoryIdData)
		{
			URPGItem* LoadedItem = AssetManager.ForceLoadItem(ItemPair.ItemId);

			if (LoadedItem != nullptr)
			{
				InventoryData.Add(FInventoryStruct(LoadedItem, ItemPair.ItemData));
			}
			else
			{
				InventoryData.Add(FInventoryStruct());
			}
		}

		for (const TPair<FRPGItemSlot, FPrimaryAssetId>& SlotPair : CurrentSaveGame->SlottedItems)
		{
			if (SlotPair.Value.IsValid())
			{
				URPGItem* LoadedItem = AssetManager.ForceLoadItem(SlotPair.Value);
				if (GameInstance->IsValidItemSlot(SlotPair.Key) && LoadedItem)
				{
					SlottedItems.Add(SlotPair.Key, LoadedItem);
					bFoundAnySlots = true;
				}
			}
		}

		if (!bFoundAnySlots)
		{
			// Auto slot items as no slots were saved
			FillEmptySlots();
		}

		NotifyInventoryLoaded();

		return true;
	}

	// Load failed but we reset inventory, so need to notify UI
	NotifyInventoryLoaded();

	return false;
}

bool ARPGPlayerControllerBase::FillEmptySlotWithItem(URPGItem* NewItem)
{
	// Look for an empty item slot to fill with this item
	FPrimaryAssetType NewItemType = NewItem->GetPrimaryAssetId().PrimaryAssetType;
	FRPGItemSlot EmptySlot;
	for (TPair<FRPGItemSlot, URPGItem*>& Pair : SlottedItems)
	{
		if (Pair.Key.ItemType == NewItemType && NewItemType.GetName() == FName(TEXT("Weapon")))
		{
			if (Pair.Value == NewItem)
			{
				// Item is already slotted
				return false;
			}
			else if (Pair.Value == nullptr && (!EmptySlot.IsValid() || EmptySlot.SlotNumber > Pair.Key.SlotNumber))
			{
				// We found an empty slot worth filling
				EmptySlot = Pair.Key;
			}
		}
	}

	if (EmptySlot.IsValid())
	{
		SlottedItems[EmptySlot] = NewItem;
		NotifySlottedItemChanged(EmptySlot, NewItem);
		return true;
	}

	return false;
}

void ARPGPlayerControllerBase::NotifyInventoryItemChanged(bool bAdded, URPGItem* Item)
{
	// Notify native before blueprint
	OnInventoryItemChangedNative.Broadcast(bAdded, Item);
	OnInventoryItemChanged.Broadcast(bAdded, Item);

	// Call BP update event
	InventoryItemChanged(bAdded, Item);
}

void ARPGPlayerControllerBase::NotifySlottedItemChanged(FRPGItemSlot ItemSlot, URPGItem* Item)
{
	// Notify native before blueprint
	OnSlottedItemChangedNative.Broadcast(ItemSlot, Item);
	OnSlottedItemChanged.Broadcast(ItemSlot, Item);

	// Call BP update event
	SlottedItemChanged(ItemSlot, Item);
}

void ARPGPlayerControllerBase::NotifyInventoryLoaded()
{
	// Notify native before blueprint
	OnInventoryLoadedNative.Broadcast();
	OnInventoryLoaded.Broadcast();
}

void ARPGPlayerControllerBase::HandleSaveGameLoaded(URPGSaveGame* NewSaveGame)
{
	LoadInventory();
}

void ARPGPlayerControllerBase::BeginPlay()
{
	// Load inventory off save game before starting play
	LoadInventory();

	Super::BeginPlay();
}