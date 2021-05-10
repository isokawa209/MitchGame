// Copyright Epic Games, Inc. All Rights Reserved.

#include "RPGCharacterBase.h"
#include "Items/RPGItem.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/RPGGameplayAbility.h"

ARPGCharacterBase::ARPGCharacterBase()
{
	// Create ability system component, and set it to be explicitly replicated
	AbilitySystemComponent = CreateDefaultSubobject<URPGAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// Create the attribute set, this replicates by default
	AttributeSet = CreateDefaultSubobject<URPGAttributeSet>(TEXT("AttributeSet"));

	CharacterLevel = 1;
	bAbilitiesInitialized = false;
}

UAbilitySystemComponent* ARPGCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ARPGCharacterBase::AddStartupGameplayAbilities()
{
	check(AbilitySystemComponent);
	
	if (GetLocalRole() == ROLE_Authority && !bAbilitiesInitialized)
	{
		// Grant abilities, but only on the server	
		for (TSubclassOf<URPGGameplayAbility>& StartupAbility : GameplayAbilities)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(StartupAbility, GetCharacterLevel(), INDEX_NONE, this));
		}

		// Now apply passives
		for (TSubclassOf<UGameplayEffect>& GameplayEffect : PassiveGameplayEffects)
		{
			FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
			EffectContext.AddSourceObject(this);

			FGameplayEffectSpecHandle NewHandle = AbilitySystemComponent->MakeOutgoingSpec(GameplayEffect, GetCharacterLevel(), EffectContext);
			if (NewHandle.IsValid())
			{
				FActiveGameplayEffectHandle ActiveGEHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*NewHandle.Data.Get(), AbilitySystemComponent);
			}
		}

		AddSlottedGameplayAbilities();

		bAbilitiesInitialized = true;
	}
}

void ARPGCharacterBase::RemoveStartupGameplayAbilities()
{
	check(AbilitySystemComponent);

	if (GetLocalRole() == ROLE_Authority && bAbilitiesInitialized)
	{
		// Remove any abilities added from a previous call
		TArray<FGameplayAbilitySpecHandle> AbilitiesToRemove;
		for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
		{
			if ((Spec.SourceObject == this) && GameplayAbilities.Contains(Spec.Ability->GetClass()))
			{
				AbilitiesToRemove.Add(Spec.Handle);
			}
		}

		// Do in two passes so the removal happens after we have the full list
		for (int32 i = 0; i < AbilitiesToRemove.Num(); i++)
		{
			AbilitySystemComponent->ClearAbility(AbilitiesToRemove[i]);
		}

		// Remove all of the passive gameplay effects that were applied by this character
		FGameplayEffectQuery Query;
		Query.EffectSource = this;
		AbilitySystemComponent->RemoveActiveEffects(Query);

		RemoveSlottedGameplayAbilities(true);

		bAbilitiesInitialized = false;
	}
}

void ARPGCharacterBase::OnItemSlotChanged(FRPGItemSlot ItemSlot, URPGItem* Item)
{
	RefreshSlottedGameplayAbilities();
}

void ARPGCharacterBase::RefreshSlottedGameplayAbilities()
{
	if (bAbilitiesInitialized)
	{
		// Refresh any invalid abilities and adds new ones
		RemoveSlottedGameplayAbilities(false);
		AddSlottedGameplayAbilities();
	}
}

void ARPGCharacterBase::GetAbilityHandles(URPGItem* Item, TArray<TSubclassOf<UGameplayEffect>>& Handles)
{
	TArray<URPGGameplayAbility*> SpecArray;
	for (auto& Abilitys : Item->Abilites)
	{
		SpecArray.Add(Abilitys.GrantedAbility.GetDefaultObject());
	}

	for (auto& Specs : SpecArray) {
		for (TPair<FGameplayTag, FRPGGameplayEffectContainer> Pair : Specs->EffectContainerMap) {
			for (TSubclassOf<UGameplayEffect> Array : Pair.Value.TargetGameplayEffectClasses) {
				Handles.Add(Array);
			}
			
		}
	}
}

void ARPGCharacterBase::FillSlottedAbilitySpecs(TMap<FRPGItemSlot, FSpecStruct>& SlottedAbilitySpecs)
{
	FSpecStruct SpecStruct;
	// First add default ones
	for (const TPair<FRPGItemSlot, TSubclassOf<URPGGameplayAbility>>& DefaultPair : DefaultSlottedAbilities)
	{
		if (DefaultPair.Value.Get())
		{
			SpecStruct.SpecArray.Add(FGameplayAbilitySpec(DefaultPair.Value, GetCharacterLevel(), INDEX_NONE, this));
			SlottedAbilitySpecs.Add(DefaultPair.Key, SpecStruct);
		}
	}

	// Now potentially override with inventory
	if (InventorySource)
	{
		const TMap<FRPGItemSlot, URPGItem*>& SlottedItemMap = InventorySource->GetSlottedItemMap();

		for (const TPair<FRPGItemSlot, URPGItem*>& ItemPair : SlottedItemMap)
		{
			URPGItem* SlottedItem = ItemPair.Value;

			// Use the character level as default
			int32 AbilityLevel = GetCharacterLevel();

			if (SlottedItem)
			{
				SpecStruct.SpecArray.Empty();
				for (auto& AbilityStruct : SlottedItem->Abilites) {
					

					if (SlottedItem && SlottedItem->ItemType.GetName() == FName(TEXT("Weapon")))
					{
						// Override the ability level to use the data from the slotted item
						AbilityLevel = AbilityStruct.AbilityLevel;
					}

					if (SlottedItem && AbilityStruct.GrantedAbility)
					{
						SpecStruct.SpecArray.Add(FGameplayAbilitySpec(AbilityStruct.GrantedAbility, AbilityLevel, INDEX_NONE, SlottedItem));

					}

				}
				if (SpecStruct.SpecArray.IsValidIndex(0))
					// This will override anything from default
					SlottedAbilitySpecs.Add(ItemPair.Key, SpecStruct);
			}
		}
	}
}

void ARPGCharacterBase::AddSlottedGameplayAbilities()
{
	TMap<FRPGItemSlot, FSpecStruct> SlottedAbilitySpecs;
	FillSlottedAbilitySpecs(SlottedAbilitySpecs);

	FGameplayAbilitySpecHandle NoneHandle;
	
	TArray<FGameplayAbilitySpecHandle> HandleArray;
	// Now add abilities if needed
	for (const TPair<FRPGItemSlot, FSpecStruct>& SpecPair : SlottedAbilitySpecs)
	{
		HandleArray.Empty();
		FHandleStruct SpecHandle = SlottedAbilities.FindOrAdd(SpecPair.Key);

		if (!SpecHandle.HandleArray.IsValidIndex(0) || SpecHandle.HandleArray.Contains(NoneHandle))
		{
			SpecHandle.HandleArray.Empty();
			for (auto& Array : SpecPair.Value.SpecArray) {


				SpecHandle.HandleArray.Add(AbilitySystemComponent->GiveAbility(Array));
			}
			SlottedAbilities.Add(SpecPair.Key, SpecHandle);
		}
	}
}

void ARPGCharacterBase::RemoveSlottedGameplayAbilities(bool bRemoveAll)
{
	TMap<FRPGItemSlot, FSpecStruct> SlottedAbilitySpecs;

	if (!bRemoveAll)
	{
		// Fill in map so we can compare
		FillSlottedAbilitySpecs(SlottedAbilitySpecs);
	}

	for (TPair<FRPGItemSlot, FHandleStruct>& ExistingPair : SlottedAbilities)
	{
		for (auto& Array : ExistingPair.Value.HandleArray)
		{
			FGameplayAbilitySpec* FoundSpec = AbilitySystemComponent->FindAbilitySpecFromHandle(Array);
			bool bShouldRemove = bRemoveAll || !FoundSpec;

			if (!bShouldRemove)
			{
				// Need to check desired ability specs, if we got here FoundSpec is valid
				if (SlottedAbilitySpecs.Find(ExistingPair.Key))
				{
					FSpecStruct FindSpecStruct(*SlottedAbilitySpecs.Find(ExistingPair.Key));
					for (auto& DesiredSpec : FindSpecStruct.SpecArray) {
						if (!&DesiredSpec || DesiredSpec.Ability != FoundSpec->Ability || DesiredSpec.SourceObject != FoundSpec->SourceObject)
						{
							bShouldRemove = true;
						}
						else
						{
							bShouldRemove = false;
							break;
						}
					}
				}
				else
				{
					bShouldRemove = true;
				}

			}

			if (bShouldRemove)
			{
				if (FoundSpec)
				{
					// Need to remove registered ability
					AbilitySystemComponent->ClearAbility(Array);
				}
				// Make sure handle is cleared even if ability wasn't found
				Array = FGameplayAbilitySpecHandle();

			}
		}
		
	}
}

void ARPGCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Try setting the inventory source, this will fail for AI
	InventorySource = NewController;

	if (InventorySource)
	{
		InventoryUpdateHandle = InventorySource->GetSlottedItemChangedDelegate().AddUObject(this, &ARPGCharacterBase::OnItemSlotChanged);
		InventoryLoadedHandle = InventorySource->GetInventoryLoadedDelegate().AddUObject(this, &ARPGCharacterBase::RefreshSlottedGameplayAbilities);
	}

	// Initialize our abilities
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		AddStartupGameplayAbilities();
	}
}

void ARPGCharacterBase::UnPossessed()
{
	// Unmap from inventory source
	if (InventorySource && InventoryUpdateHandle.IsValid())
	{
		InventorySource->GetSlottedItemChangedDelegate().Remove(InventoryUpdateHandle);
		InventoryUpdateHandle.Reset();

		InventorySource->GetInventoryLoadedDelegate().Remove(InventoryLoadedHandle);
		InventoryLoadedHandle.Reset();
	}

	InventorySource = nullptr;
}

void ARPGCharacterBase::OnRep_Controller()
{
	Super::OnRep_Controller();

	// Our controller changed, must update ActorInfo on AbilitySystemComponent
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RefreshAbilityActorInfo();
	}
}

void ARPGCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARPGCharacterBase, CharacterLevel);
}

float ARPGCharacterBase::GetHealth() const
{
	if (!AttributeSet)
	return 1.f;

	return AttributeSet->GetHealth();
}

float ARPGCharacterBase::GetMaxHealth() const
{
	return AttributeSet->GetMaxHealth();
}

float ARPGCharacterBase::GetMana() const
{
	return AttributeSet->GetMana();
}

float ARPGCharacterBase::GetMaxMana() const
{
	return AttributeSet->GetMaxMana();
}

float ARPGCharacterBase::GetMoveSpeed() const
{
	return AttributeSet->GetMoveSpeed();
}

float ARPGCharacterBase::GetInventorySize() const
{
	return AttributeSet->GetInventorySize();
}

int32 ARPGCharacterBase::GetCharacterLevel() const
{
	return CharacterLevel;
}

bool ARPGCharacterBase::SetCharacterLevel(int32 NewLevel)
{
	if (CharacterLevel != NewLevel && NewLevel > 0)
	{
		// Our level changed so we need to refresh abilities
		RemoveStartupGameplayAbilities();
		CharacterLevel = NewLevel;
		AddStartupGameplayAbilities();

		return true;
	}
	return false;
}

bool ARPGCharacterBase::ActivateAbilitiesWithItemSlot(FRPGItemSlot ItemSlot, int32 ArrayIndex, bool bAllowRemoteActivation)
{
		if (SlottedAbilities.Find(ItemSlot))
	{
		TArray<FGameplayAbilitySpecHandle> Array = SlottedAbilities.Find(ItemSlot)->HandleArray;

		FGameplayAbilitySpecHandle FoundHandle = Array[ArrayIndex];

		if (FoundHandle.IsValid() && AbilitySystemComponent)
		{
			return AbilitySystemComponent->TryActivateAbility(FoundHandle, bAllowRemoteActivation);
		}
	}

	return false;
}

void ARPGCharacterBase::GetActiveAbilitiesWithItemSlot(FRPGItemSlot ItemSlot, TArray<URPGGameplayAbility*>& ActiveAbilities , int32 ArrayIndex)
{
	FGameplayAbilitySpecHandle* FoundHandle = &SlottedAbilities.Find(ItemSlot)->HandleArray[ArrayIndex];

	if (FoundHandle && AbilitySystemComponent)
	{
		FGameplayAbilitySpec* FoundSpec = AbilitySystemComponent->FindAbilitySpecFromHandle(*FoundHandle);

		if (FoundSpec)
		{
			TArray<UGameplayAbility*> AbilityInstances = FoundSpec->GetAbilityInstances();

			// Find all ability instances executed from this slot
			for (UGameplayAbility* ActiveAbility : AbilityInstances)
			{
				ActiveAbilities.Add(Cast<URPGGameplayAbility>(ActiveAbility));
			}
		}
	}
}

bool ARPGCharacterBase::ActivateAbilitiesWithTags(FGameplayTagContainer AbilityTags, bool bAllowRemoteActivation)
{
	if (AbilitySystemComponent)
	{
		return AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags, bAllowRemoteActivation);
	}

	return false;
}

void ARPGCharacterBase::GetActiveAbilitiesWithTags(FGameplayTagContainer AbilityTags, TArray<URPGGameplayAbility*>& ActiveAbilities)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetActiveAbilitiesWithTags(AbilityTags, ActiveAbilities);
	}
}

bool ARPGCharacterBase::GetCooldownRemainingForTag(FGameplayTagContainer CooldownTags, float& TimeRemaining, float& CooldownDuration)
{
	if (AbilitySystemComponent && CooldownTags.Num() > 0)
	{
		TimeRemaining = 0.f;
		CooldownDuration = 0.f;

		FGameplayEffectQuery const Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
		TArray< TPair<float, float> > DurationAndTimeRemaining = AbilitySystemComponent->GetActiveEffectsTimeRemainingAndDuration(Query);
		if (DurationAndTimeRemaining.Num() > 0)
		{
			int32 BestIdx = 0;
			float LongestTime = DurationAndTimeRemaining[0].Key;
			for (int32 Idx = 1; Idx < DurationAndTimeRemaining.Num(); ++Idx)
			{
				if (DurationAndTimeRemaining[Idx].Key > LongestTime)
				{
					LongestTime = DurationAndTimeRemaining[Idx].Key;
					BestIdx = Idx;
				}
			}

			TimeRemaining = DurationAndTimeRemaining[BestIdx].Key;
			CooldownDuration = DurationAndTimeRemaining[BestIdx].Value;

			return true;
		}
	}
	return false;
}

void ARPGCharacterBase::HandleDamage(float DamageAmount, const FHitResult& HitInfo, const struct FGameplayTagContainer& DamageTags, ARPGCharacterBase* InstigatorPawn, AActor* DamageCauser)
{
	OnDamaged(DamageAmount, HitInfo, DamageTags, InstigatorPawn, DamageCauser);	
}

void ARPGCharacterBase::HandleHealthChanged(float DeltaValue, const struct FGameplayTagContainer& EventTags)
{
	// We only call the BP callback if this is not the initial ability setup
	if (bAbilitiesInitialized)
	{
		OnHealthChanged(DeltaValue, EventTags);
	}
}

void ARPGCharacterBase::HandleManaChanged(float DeltaValue, const struct FGameplayTagContainer& EventTags)
{
	if (bAbilitiesInitialized)
	{
		OnManaChanged(DeltaValue, EventTags);
	}
}

void ARPGCharacterBase::HandleMoveSpeedChanged(float DeltaValue, const struct FGameplayTagContainer& EventTags)
{
	// Update the character movement's walk speed
	GetCharacterMovement()->MaxWalkSpeed = GetMoveSpeed();

	if (bAbilitiesInitialized)
	{
		OnMoveSpeedChanged(DeltaValue, EventTags);
	}
}


void ARPGCharacterBase::HandleInventorySizeChanged(float DeltaValue, const struct FGameplayTagContainer& EventTags)
{
	if (bAbilitiesInitialized)
	{
		OnInventorySizeChanged(DeltaValue, EventTags);
	}
}