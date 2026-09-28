// project
#include "AbilitySystem/Abilities/Combat/CBLifeOnHitAbility.h"
#include "AbilitySystem/CBAttributeSet.h"
#include "CBGameplayTags.h"

// engine
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameplayEffect.h"

UCBLifeOnHitAbility::UCBLifeOnHitAbility()
{
	// 폰이 살아 있는 동안 계속 켜져 있어야 적중 이벤트를 받을 수 있으므로 부여 즉시 활성화
	AbilityActivationPolicy = ECBAbilityActivationPolicy::OnGiven;

	// 적중 이벤트는 서버 검증을 통과한 뒤 서버에서만 발행되고, 체력도 서버 권위이므로 서버 전용
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// 이 어빌리티를 식별하는 태그
	SetAssetTags(FGameplayTagContainer(CBGameplayTags::Ability_Combat_LifeOnHit));
}

void UCBLifeOnHitAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 적중 이벤트 대기 (어빌리티가 끝날 때까지 계속 수신)
	UAbilityTask_WaitGameplayEvent* WaitAttackHit = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, CBGameplayTags::Event_Combat_Attack_Hit, nullptr, false);
	WaitAttackHit->EventReceived.AddDynamic(this, &ThisClass::OnAttackHit);
	WaitAttackHit->ReadyForActivation();
}

void UCBLifeOnHitAbility::OnAttackHit(FGameplayEventData Payload)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	// 적중 회복량 어트리뷰트 가져오기.
	const float LifePerHit = ASC->GetNumericAttribute(UCBAttributeSet::GetLifeOnHitAttribute());
	
	// 적중 회복량이 없으면 종료.
	if (LifePerHit <= 0.f) return;

	// 회복 GE 유효성 검사
	if (!HealEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] HealEffectClass 미지정 - 적중 회복이 적용되지 않음"), *GetName());
		return;
	}

	// 히트는 배칭되어 한 이벤트에 여러 피격자가 실려 옴. 같은 대상은 한 스윙에 한 번만 실리므로 곧 맞은 적 수.
	const float HealAmount = LifePerHit * Payload.TargetData.Num();

	// GE 스펙 만들기.
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(HealEffectClass);
	if (!SpecHandle.IsValid()) return;
	
	// Data_Heal 태그로 HealAmount 값 넘기기
	SpecHandle.Data->SetSetByCallerMagnitude(CBGameplayTags::Data_Heal, HealAmount);

	// 자신에게 회복 적용. GE 경로라 어트리뷰트셋이 MaxHealth 로 클램프하고, 양수 변화라 피격 반응은 일어나지 않음.
	ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle);
}
