// Copyright Epic Games, Inc. All Rights Reserved.

#include "Abilities/RPGAbilitySystemComponent.h"
#include "RPGCharacterBase.h"
#include "Abilities/RPGGameplayAbility.h"
#include "AbilitySystemGlobals.h"

URPGAbilitySystemComponent::URPGAbilitySystemComponent() {}

void URPGAbilitySystemComponent::GetActiveAbilitiesWithTags(const FGameplayTagContainer& GameplayTagContainer, TArray<URPGGameplayAbility*>& ActiveAbilities)
{
	TArray<FGameplayAbilitySpec*> AbilitiesToActivate;
	GetActivatableGameplayAbilitySpecsByAllMatchingTags(GameplayTagContainer, AbilitiesToActivate, false);

	// Iterate the list of all ability specs
	for (FGameplayAbilitySpec* Spec : AbilitiesToActivate)
	{
		// Iterate all instances on this ability spec
		TArray<UGameplayAbility*> AbilityInstances = Spec->GetAbilityInstances();

		for (UGameplayAbility* ActiveAbility : AbilityInstances)
		{
			ActiveAbilities.Add(Cast<URPGGameplayAbility>(ActiveAbility));
		}
	}
}

int32 URPGAbilitySystemComponent::GetDefaultAbilityLevel() const
{
	ARPGCharacterBase* OwningCharacter = Cast<ARPGCharacterBase>(OwnerActor);

	if (OwningCharacter)
	{
		return OwningCharacter->GetCharacterLevel();
	}
	return 1;
}

URPGAbilitySystemComponent* URPGAbilitySystemComponent::GetAbilitySystemComponentFromActor(const AActor* Actor, bool LookForComponent)
{
	return Cast<URPGAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor, LookForComponent));
}



float URPGAbilitySystemComponent::GetCalculationModifiedMagnitude(TArray<TSubclassOf<UGameplayEffect>> SpecHandle)
{
	for (TSubclassOf<UGameplayEffect> Spec : SpecHandle)
	{
		if (Spec)
		{
			TArray<FGameplayEffectExecutionDefinition> Mod = GetCalculationModifiedMagnitude(*Spec.GetDefaultObject());
		}
	}

	return 1;
}

TArray<FGameplayEffectExecutionDefinition> URPGAbilitySystemComponent::GetCalculationModifiedMagnitude(const UGameplayEffect& Spec)
{
	
	TArray<FGameplayEffectExecutionDefinition> Mod = Spec.Executions;
	FGameplayEffectSpec InRelevantSpec;
	float OutCalculatedMagnitude;
	Mod[0].CalculationModifiers[0].ModifierMagnitude.AttemptCalculateMagnitude(InRelevantSpec, OutCalculatedMagnitude);
	
	return Mod;
}
