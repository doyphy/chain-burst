// project
#include "AbilitySystem/Abilities/Movement/CBGAChangeSpeed.h"
#include "Characters/CBBaseCharacter.h"
#include "DataAssets/Movement/CBCharacterMovementData.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "CBGameplayTags.h"

// engine
#include "GameplayEffect.h"

UCBGAChangeSpeed::UCBGAChangeSpeed()
{
	// 어빌리티 인스턴싱 정책 설정
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UCBGAChangeSpeed::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	ACBBaseCharacter* BaseChar = Cast<ACBBaseCharacter>(GetAvatarActorFromActorInfo());

	if (!ASC || !BaseChar || !MovementModifierGEClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 기존에 적용된 '이동 오버라이드' GE가 있다면 먼저 제거.
	// Status.Movement.Overridden 태그를 가진 모든 GE를 찾아서 제거.
	FGameplayTagContainer CleanUpContainer;
	CleanUpContainer.AddTag(CBGameplayTags::Status_Movement_Overridden);
	ASC->RemoveActiveEffectsWithGrantedTags(CleanUpContainer);

	// 데이터 에셋에서 태그에 맞는 속도 값 가져오기
	float TargetSpeed = BaseChar->GetMovementDataAsset()->GetSpeedForTag(SpeedDataTag);

	// GE Spec 생성
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(MovementModifierGEClass);
	if (SpecHandle.IsValid())
	{
		// SetByCaller를 통해 데이터 설정
		SpecHandle.Data.Get()->SetSetByCallerMagnitude(
			CBGameplayTags::Data_Movement_Speed, 
			TargetSpeed
		);

		// 캐릭터에 속도 태그 부여 (애니메이션 블루프린트에서 이 태그를 보고 애니메이션 상태를 변경할 수 있도록)
		SpecHandle.Data.Get()->DynamicGrantedTags.AddTag(SpeedDataTag);
		
		// 캐릭터에게 효과 적용 및 핸들 저장
		ActiveGEHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UCBGAChangeSpeed::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 어빌리티 종료 시 적용했던 GE를 제거 (자동으로 기본 속도인 Run으로 복구됨)
	if (ActiveGEHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveActiveGameplayEffect(ActiveGEHandle);
		}
		// 핸들 무효화 (안전하게)
		ActiveGEHandle.Invalidate();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCBGAChangeSpeed::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	if (IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}
