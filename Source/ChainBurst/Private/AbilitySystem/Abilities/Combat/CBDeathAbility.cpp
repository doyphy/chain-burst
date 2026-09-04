// project
#include "AbilitySystem/Abilities/Combat/CBDeathAbility.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "CBGameplayTags.h"

UCBDeathAbility::UCBDeathAbility()
{
	// 서버 권위 데미지로 발동되는 반응이므로 서버 주도 실행 (엣지 클래스에서 명시)
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	// 이 어빌리티를 식별하는 태그 (사망 어빌리티는 종류가 하나뿐이라 C++에서 지정)
	SetAssetTags(FGameplayTagContainer(CBGameplayTags::Ability_Combat_Death));

	// 재생할 사망 몽타주 태그
	BoundActionTag = CBGameplayTags::Action_Combat_Death;

	// 사망은 종류를 가리지 않고 전부 끊으므로 베이스의 부분 캔슬(CancelActionTag)은 쓰지 않음
	CancelActionTag = FGameplayTag::EmptyTag;

	// Event.Combat.Death 이벤트로 자동 발동되도록 트리거 등록
	RegisterEventTrigger(CBGameplayTags::Event_Combat_Death);

	// 사망 상태에서도 활성화되어야 하는 유일한 어빌리티 (공통 차단 게이트의 예외)
	bActivatableWhileDead = true;

	// 공격 어빌리티가 BlockAbilitiesWithTag 로 Ability.Combat 을 막고 있으면 그 하위인 사망(Ability.Combat.Death)까지 걸린다.
	// 그러면 시전 중 죽었을 때 사망 어빌리티가 아예 발동하지 못하고, 사망 이벤트는 1회성이라 다시 오지도 않아 체력 0인 채로 살아남음.
	// 죽음은 어떤 차단도 뚫어야 하므로 차단 검사를 무시함
	bIgnoreAbilityBlocking = true;
}

void UCBDeathAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 진행 중인 다른 어빌리티를 전부 캔슬 (공격·대시·질주 등. 자기 자신은 제외)
	if (UCBAbilitySystemComponent* CBASC = GetCBAbilitySystemComponentFromActorInfo())
	{
		CBASC->CancelAllAbilities(this);
	}

	// 사망 상태 GE 적용. 몽타주보다 먼저 적용해서 몽타주가 없거나 재생에 실패해도 사망 상태는 확정.
	Auth_ApplyDeadState(Handle, ActorInfo, ActivationInfo);

	// 베이스가 사망 몽타주를 재생 (GameplayCue로 전 클라 동기화)
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UCBDeathAbility::Auth_ApplyDeadState(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// GE는 서버에서만 적용. (GAS 복제 경로에 의해 전 클라로 복제됨)
	if (!HasAuthority(&ActivationInfo)) return;

	// BP 자식에서 GE를 지정하지 않았으면 사망 상태가 부여되지 않으므로 경고 (조용히 죽는 것을 막음)
	if (!DeadStateEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] DeadStateEffectClass 미지정 — Status.Dead가 부여되지 않음"), *GetName());
		return;
	}

	// 사망 GE 스펙 생성
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DeadStateEffectClass);
	if (!SpecHandle.IsValid()) return;

	// 사망 GE 적용
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}
