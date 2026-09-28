// project
#include "AbilitySystem/Abilities/Combat/CBBurstGaugeAbility.h"
#include "AbilitySystem/CBAttributeSet.h"
#include "CBGameplayTags.h"

// engine
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

UCBBurstGaugeAbility::UCBBurstGaugeAbility()
{
	// 폰이 살아 있는 동안 계속 켜져 있어야 적중 이벤트를 받을 수 있으므로 부여 즉시 활성화
	AbilityActivationPolicy = ECBAbilityActivationPolicy::OnGiven;

	// 적중 이벤트는 서버 검증을 통과한 뒤 서버에서만 발행되고, 게이지도 서버 권위이므로 서버 전용
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// 이 어빌리티를 식별하는 태그 (종류가 하나뿐이라 C++에서 지정)
	SetAssetTags(FGameplayTagContainer(CBGameplayTags::Ability_Combat_BurstGauge));
}

void UCBBurstGaugeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
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

void UCBBurstGaugeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 폰의 수명이 끝남 (사망 → 전체 캔슬 / 캐릭터 변경 → 로드아웃 회수). 서버 전용 어빌리티라 여기는 항상 서버.
	// 플레이어 ASC는 PlayerState 소유라 폰보다 오래 살아서, 비우지 않으면 새 폰이 게이지와 버스트를 그대로 이어받음.
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->SetNumericAttributeBase(UCBAttributeSet::GetBurstGaugeAttribute(), 0.f);
		ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(CBGameplayTags::Status_Combat_Burst));
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCBBurstGaugeAbility::OnAttackHit(FGameplayEventData Payload)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	// 버스트 중에는 쌓지 않음 (발동 때 비운 게이지가 버스트 동안 0으로 유지됨)
	if (ASC->HasMatchingGameplayTag(CBGameplayTags::Status_Combat_Burst)) return;

	// 히트는 배칭되어 한 이벤트에 여러 피격자가 실려 옴. 같은 대상은 한 스윙에 한 번만 실리므로 곧 맞은 적 수.
	const float Gain = GaugeGainPerHit * Payload.TargetData.Num();

	const FGameplayAttribute GaugeAttribute = UCBAttributeSet::GetBurstGaugeAttribute();
	const float NewGauge = FMath::Min(ASC->GetNumericAttributeBase(GaugeAttribute) + Gain, UCBAttributeSet::MaxBurstGauge);

	// 서버 권위 기록 (기본값에 쓰므로 전 클라로 복제됨)
	ASC->SetNumericAttributeBase(GaugeAttribute, NewGauge);
}
