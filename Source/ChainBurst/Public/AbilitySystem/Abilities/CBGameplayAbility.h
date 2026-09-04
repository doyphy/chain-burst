#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "CBGameplayAbility.generated.h"

class UCBActionComponent;
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
		/** 어빌리티가 활성화될 수 있는지 여부를 결정하는 함수. ActivateAbility 함수 호출하기 전에 호출됨. */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
		/** 태그 요구·차단 검사. bIgnoreAbilityBlocking 이면 검사 자체를 건너뜀. */
	virtual bool DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	//~ End UGameplayAbility Interface

	UFUNCTION(BlueprintPure, Category = "ChainBurst|Ability")
	UCBCombatComponent* GetCBCombatComponentFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "ChainBurst|Ability")
	UCBAbilitySystemComponent* GetCBAbilitySystemComponentFromActorInfo() const;

	UFUNCTION(BlueprintPure, Category = "ChainBurst|Ability")
	UCBActionComponent* GetCBActionComponentFromActorInfo() const;
	
protected:
	/** 어빌리티의 활성화 정책 (기본값 : OnTrigger) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|Ability")
	ECBAbilityActivationPolicy AbilityActivationPolicy = ECBAbilityActivationPolicy::OnTrigger;

	/**
	 * 사망 상태(Status.Dead)에서도 활성화할 수 있는지 여부 (기본값 : false).
	 * 사망·시체 관련 어빌리티만 true로 설정. 차단 검사는 CanActivateAbility에서 일괄 수행.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|Ability")
	bool bActivatableWhileDead = false;

	/**
	 * 다른 어빌리티가 걸어둔 차단(BlockAbilitiesWithTag)을 무시하고 활성화할지 여부 (기본값 : false).
	 * 반드시 발동해야 하는 어빌리티(사망)만 true로 설정.
	 *
	 * 주의: 켜면 태그 요구·차단 검사를 통째로 건너뛴다. 즉 이 어빌리티의 ActivationRequiredTags /
	 * ActivationBlockedTags / SourceBlockedTags 등도 함께 무시되므로, BP에서 설정해도 걸리지 않음.
	 * (엔진이 요구·차단을 한 함수에서 한꺼번에 판정하므로 차단만 골라 빼낼 수 없음)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|Ability")
	bool bIgnoreAbilityBlocking = false;
};
