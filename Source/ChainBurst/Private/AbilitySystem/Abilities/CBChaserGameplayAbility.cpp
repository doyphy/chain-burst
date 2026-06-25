// project
#include "AbilitySystem/Abilities/CBChaserGameplayAbility.h"
#include "Characters/CBChaserCharacter.h"
#include "Controllers/CBChaserController.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "CBGameplayTags.h"

ACBChaserCharacter* UCBChaserGameplayAbility::GetChaserCharacterFromActorInfo()
{
	if (!CachedChaserCharacter.IsValid())
	{
		CachedChaserCharacter = Cast<ACBChaserCharacter>(CurrentActorInfo->AvatarActor);
	}
	return CachedChaserCharacter.IsValid() ? CachedChaserCharacter.Get() : nullptr;
}

UCBChaserCombatComponent* UCBChaserGameplayAbility::GetChaserCombatComponentFromActorInfo()
{
	if (GetChaserCharacterFromActorInfo() == nullptr)
	{
		return nullptr;
	}
	return GetChaserCharacterFromActorInfo()->GetChaserCombatComponent();
}

ACBChaserController* UCBChaserGameplayAbility::GetChaserControllerFromActorInfo()
{
	if (!CachedChaserController.IsValid())
	{
		CachedChaserController = Cast<ACBChaserController>(CurrentActorInfo->PlayerController);
	}
	return CachedChaserController.IsValid() ? CachedChaserController.Get() : nullptr;
}

FGameplayEffectSpecHandle UCBChaserGameplayAbility::MakeDamageEffectSpecHandle(TSubclassOf<UGameplayEffect> EffectClass,
	float InDamage, FGameplayTag InAttackTag, int32 InComboCount)
{
	// 적용할 GameplayEffect 클래스가 유효한지 확인
	check(EffectClass);

	// 이펙트 컨텍스트 핸들 생성
	FGameplayEffectContextHandle ContextHandle = GetCBAbilitySystemComponentFromActorInfo()->MakeEffectContext();
	
	// 이펙트 컨텍스트에 어빌리티, 소스 오브젝트, 인스티게이터 정보 추가
	ContextHandle.SetAbility(this);
	ContextHandle.AddSourceObject(GetAvatarActorFromActorInfo());
	ContextHandle.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());

	// 이펙트 스펙 핸들 생성 (이펙트 클래스, 어빌리티 레벨, 컨텍스트 정보 포함)
	FGameplayEffectSpecHandle EffectSpecHandle = GetCBAbilitySystemComponentFromActorInfo()->MakeOutgoingSpec(
		EffectClass,
		GetAbilityLevel(),
		ContextHandle
	);

	// SetByCaller 방식으로 무기 기본 데미지 값을 핸들이 가리키는 스펙에 '태그-값' 으로 기록
	EffectSpecHandle.Data->SetSetByCallerMagnitude(
		CBGameplayTags::Data_Damage,
		InDamage
	);

	// 공격 타입 태그가 유효하면, 해당 태그로 콤보 카운트 값을 기록
	if (InAttackTag.IsValid())
	{
		EffectSpecHandle.Data->SetSetByCallerMagnitude(InAttackTag, InComboCount);
	}

	// 완성된 이펙트 스펙 핸들 반환
	return EffectSpecHandle;
}
