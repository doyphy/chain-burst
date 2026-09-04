// project
#include "AbilitySystem/Abilities/Combat/CBHitReactAbility.h"
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"

UCBHitReactAbility::UCBHitReactAbility()
{
	// 서버 권위 데미지로 발동되는 반응이므로 서버 주도 실행 (엣지 클래스에서 명시)
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// 재생할 피격 몽타주 태그
	BoundActionTag = CBGameplayTags::Action_Combat_HitReact;

	// 피격 시 진행 중인 전투 액션(공격 등) 캔슬
	CancelActionTag = CBGameplayTags::Action_Combat;

	// 슈퍼아머(스킬 시전 등) 중에는 피격 반응 무시.
	// 데미지 GE는 그대로 적용되고 반응 모션만 생략.
	ActivationBlockedTags.AddTag(CBGameplayTags::Status_Combat_SuperArmor);

	// Event.Combat.HitReact 이벤트로 자동 발동되도록 트리거 등록
	RegisterEventTrigger(CBGameplayTags::Event_Combat_HitReact);
}

int32 UCBHitReactAbility::SelectActionMontageIndex()
{
	// 전투 상태(Status.Combat.InCombat)면 전투 피격(인덱스 1), 비전투면 일반 피격(인덱스 0)
	// 태그가 전 클라 복제(TagAndCountToAll)되므로 예측 클라/서버가 같은 인덱스를 계산한다
	return UCBAbilitySystemLibrary::IsCombatMode(GetAvatarActorFromActorInfo()) ? 1 : 0;
}
