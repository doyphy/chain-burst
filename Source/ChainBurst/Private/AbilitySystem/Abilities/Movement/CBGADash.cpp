// project
#include "AbilitySystem/Abilities/Movement/CBGADash.h"
#include "CBGameplayTags.h"

// engine
#include "AbilitySystemComponent.h"

UCBGADash::UCBGADash()
{
	// 로컬 입력으로 발동되어 즉시 반응해야 하므로 예측 실행
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 어빌리티 식별 태그 (외부에서 이 어빌리티를 태그로 식별·취소·차단할 때 사용)
	SetAssetTags(FGameplayTagContainer(CBGameplayTags::Ability_Movement_Dash));

	// 어빌리티 활성 동안 "대시 중" 상태 태그 자동 부여/제거
	// (CBLocomotionProcessor의 대시 전용 감속 판별 + GA_Sprint의 ActivationRequiredTags 활성화 조건에 사용)
	ActivationOwnedTags.AddTag(CBGameplayTags::Status_Movement_Dashing);

	// 대시 성공 시 함께 활성화할 Sprint 어빌리티 태그 기본값
	SprintAbilityTag = CBGameplayTags::Ability_Movement_Sprint;
}

void UCBGADash::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 몽타주 재생 가능한지 확인 (베이스가 액션 컴포넌트/액션 태그 검사 후 세팅)
	if (bCanPlayMontage == false)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 커밋 — 쿨다운 GE 적용 (쿨다운 중 재활성화는 CanActivateAbility가 이미 차단)
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 몽타주 재생 (비콤보라 베이스가 인덱스 0 = 전방 대시를 재생)
	PlayActionMontage();

	// 대시 성공 → Sprint 어빌리티 활성화
	// PlayActionMontage 내부 실패로 이미 종료됐으면 스킵. (IsActive())
	// 로컬 컨트롤에서만 시도 — 예측 활성화가 서버로 자동 복제되므로 서버 인스턴스가 중복 시도할 필요 없음. (IsLocallyControlled())
	if (IsActive() && ActorInfo->IsLocallyControlled() && SprintAbilityTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(SprintAbilityTag));
		}
	}
}
