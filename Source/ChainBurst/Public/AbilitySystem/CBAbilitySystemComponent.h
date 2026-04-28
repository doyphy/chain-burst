#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "CBAbilitySystemComponent.generated.h"

UCLASS()
class CHAINBURST_API UCBAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	UCBAbilitySystemComponent();
	
	void OnAbilityInputPressed(const FGameplayTag& InInputTag);
	void OnAbilityInputReleased(const FGameplayTag& InInputTag);

	/** 현재 눌려있는 입력 태그 모음 */
	FGameplayTagContainer HeldInputTags;
};
