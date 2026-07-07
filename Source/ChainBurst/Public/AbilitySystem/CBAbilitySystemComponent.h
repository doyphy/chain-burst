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

	/** 어빌리티	입력이 눌렀을 때 호출하는 함수 (어빌리티 활성화 담당) */
	void OnAbilityInputPressed(const FGameplayTag& InInputTag);

	/** 어빌리티 입력이 떼졌을 때 호출하는 함수 (어빌리티 비활성화 담당) */
	void OnAbilityInputReleased(const FGameplayTag& InInputTag);
};
