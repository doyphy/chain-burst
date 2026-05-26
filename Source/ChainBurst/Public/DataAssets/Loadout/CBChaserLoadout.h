#pragma once

#include "CoreMinimal.h"
#include "DataAssets/Loadout/CBCharacterLoadout.h"
#include "GameplayTagContainer.h"
#include "CBChaserLoadout.generated.h"

class UCBGameplayAbility;
class UCBAbilitySystemComponent;

USTRUCT(BlueprintType)
struct FCBChaserAbilitySet
{
	GENERATED_BODY()

	/** 어빌리티 입력 태그 (어떤 입력 태그와 연결된 어빌리티인지 구분) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Input"))
	FGameplayTag InputTag;
	
	/** 어빌리티 클래스 (입력 태그로 구분) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UCBGameplayAbility> AbilityToGrant;

	/** InputTag와 AbilityToGrant가 유효한지 확인하는 함수 */
	bool IsValid() const;
};

UCLASS()
class CHAINBURST_API UCBChaserLoadout : public UCBCharacterLoadout
{
	GENERATED_BODY()
	
private:
	/** 추격자의 어빌리티 세트 배열. 각 세트는 입력 태그와 어빌리티 클래스를 포함 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout", meta = (TitleProperty = "InputTag"))
	TArray<FCBChaserAbilitySet> ChaserAbilitySets;
	
public:
	/** 어빌리티 시스템 컴포넌트에 어빌리티를 부여하는 함수 (부모 함수 재정의, 추격자 전용) */
	virtual void Auth_GrantAbilitiesToASC(UCBAbilitySystemComponent* InASCToGive, int32 ApplyLevel = 1) override;
};
