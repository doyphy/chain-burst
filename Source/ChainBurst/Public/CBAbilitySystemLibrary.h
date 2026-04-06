#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CBAbilitySystemLibrary.generated.h"

class UAbilitySystemComponent;

UCLASS()
class CHAINBURST_API UCBAbilitySystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * 대상 액터의 ASC 에서 전투 상태 태그를 검사해 반환
	 * @param InActor 검사할 액터 (ACBBaseCharacter 또는 그 자식 클래스 권장)
	 * @return 전투 상태면 true
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Combat", meta = (DefaultToSelf = "InActor"))
	static bool IsCombatMode(const AActor* InActor);

	/**
	 * 대상 액터의 ASC 에서 특정 GameplayTag 보유 여부를 반환
	 * @param InActor    검사할 액터
	 * @param InTag      검사할 태그
	 * @return 태그 보유 시 true
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|AbilitySystem", meta = (DefaultToSelf = "InActor"))
	static bool HasGameplayTag(const AActor* InActor, const FGameplayTag& InTag);

private:
	/** InActor 로부터 ASC 를 안전하게 가져오는 내부 헬퍼 함수 */
	static UAbilitySystemComponent* GetASC(const AActor* InActor);
};
