#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "CBGameplayAbility.generated.h"

class UCBCombatComponent;
class UCBAbilitySystemComponent;


/** 활성화 정책 열거형 */
UENUM(BlueprintType)
enum class ECBAbilityActivationPolicy : uint8
{
	/**
	 * 어빌리티를 부여받아도 대기하다가, 특정 이벤트(입력, 게임플레이 태그 등)가 발생했을 때 활성화.
	 * 액티브(Active) 스킬에 사용.
	 */
	OnTrigger UMETA(DisplayName = "On Trigger"),
	/**
	 * 어빌리티를 부여받는 즉시 자동으로 활성화.
	 * 패시브(Passive) 스킬이나 항상 켜져 있어야 하는 오라(Aura) 같은 어빌리티에 사용.
	 */
	OnGiven UMETA(DisplayName = "On Given"),
};



UCLASS()
class CHAINBURST_API UCBGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UCBGameplayAbility();
	
protected:
	//~ Begin UGameplayAbility Interface
		/** 어빌리티가 부여될 때 호출하는 함수 */
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
		/** 어빌리티가 종료될 때 호출하는 함수 */
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

	UFUNCTION(BlueprintPure, Category = "ChainBurst|Ability")
	UCBCombatComponent* GetCBCombatComponentFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "ChainBurst|Ability")
	UCBAbilitySystemComponent* GetCBAbilitySystemComponentFromActorInfo() const;
	
protected:
	/** 어빌리티의 활성화 정책 (기본값 : OnTrigger) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|Ability")
	ECBAbilityActivationPolicy AbilityActivationPolicy = ECBAbilityActivationPolicy::OnTrigger;
};
