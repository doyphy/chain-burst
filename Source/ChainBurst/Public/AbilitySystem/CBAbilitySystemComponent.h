#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "CBAbilitySystemComponent.generated.h"

/** 입력 태그에 대한 변경을 알리는 델리게이트 (입력감지) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityInputChange, const FGameplayTag&, InputTag);

UCLASS()
class CHAINBURST_API UCBAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	UCBAbilitySystemComponent();
	
	void OnAbilityInputPressed(const FGameplayTag& InInputTag);
	void OnAbilityInputReleased(const FGameplayTag& InInputTag);

	/** 현재 눌려있는 입력 태그 모음 (입력감지) */
	FGameplayTagContainer HeldInputTags;

	/** 입력 태그가 눌렸을 때 호출되는 델리게이트 (입력감지) */
	FOnAbilityInputChange OnAbilityInputTagPressed;
};
