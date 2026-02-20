#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CBGameplayAbility.h"
#include "CBGAChangeSpeed.generated.h"

/**
 * 전력 질주, 걷기 등 캐릭터의 기본 속도를 일시적으로 변경하는 어빌리티의 베이스 클래스
 */
UCLASS()
class CHAINBURST_API UCBGAChangeSpeed : public UCBGameplayAbility
{
	GENERATED_BODY()
public:
	UCBGAChangeSpeed();

protected:
	//~ Begin UGameplayAbility Interface
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	//~ End UGameplayAbility Interface

protected:
	/** 블루프린트 자식(GA_Sprint, GA_Walk)에서 설정할 속도 구분 태그 */
	UPROPERTY(EditDefaultsOnly, Category = "CB|Movement")
	FGameplayTag SpeedDataTag;

	/** 공용으로 사용할 속도 변경용 GE 클래스 (GE_MovementModifier) */
	UPROPERTY(EditDefaultsOnly, Category = "CB|Movement")
	TSubclassOf<UGameplayEffect> MovementModifierGEClass;

	/** 적용된 GE를 나중에 제거하기 위해 저장하는 핸들 */
	FActiveGameplayEffectHandle ActiveGEHandle;
};
