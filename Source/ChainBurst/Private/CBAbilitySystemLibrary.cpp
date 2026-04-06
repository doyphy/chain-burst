// project
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"

// engine
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"

UAbilitySystemComponent* UCBAbilitySystemLibrary::GetASC(const AActor* InActor)
{
	if (!InActor) return nullptr;

	// IAbilitySystemInterface 를 구현한 액터라면 ASC 반환
	const IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(InActor);
	if (!ASCInterface) return nullptr;

	return ASCInterface->GetAbilitySystemComponent();
}

bool UCBAbilitySystemLibrary::HasGameplayTag(const AActor* InActor, const FGameplayTag& InTag)
{
	if (!InTag.IsValid()) return false;

	UAbilitySystemComponent* ASC = GetASC(InActor);
	if (!ASC) return false;

	return ASC->HasMatchingGameplayTag(InTag);
}

bool UCBAbilitySystemLibrary::IsCombatMode(const AActor* InActor)
{
	return HasGameplayTag(InActor, CBGameplayTags::Shared_Status_Combat_InCombat);
}
