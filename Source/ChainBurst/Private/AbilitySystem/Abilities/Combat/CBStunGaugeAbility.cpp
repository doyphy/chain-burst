// project
#include "AbilitySystem/Abilities/Combat/CBStunGaugeAbility.h"
#include "AbilitySystem/CBAttributeSet.h"
#include "CBGameplayTags.h"

// engine
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

UCBStunGaugeAbility::UCBStunGaugeAbility()
{
	// 폰이 살아 있는 동안 계속 켜져 있어야 피격·어빌리티 종료를 받을 수 있으므로 부여 즉시 활성화
	AbilityActivationPolicy = ECBAbilityActivationPolicy::OnGiven;

	// 피격 이벤트는 서버에서 발행되고, 게이지도 서버 권위이므로 서버 전용
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// 이 어빌리티를 식별하는 태그 (종류가 하나뿐이라 C++에서 지정)
	SetAssetTags(FGameplayTagContainer(CBGameplayTags::Ability_Combat_StunGauge));
}

void UCBStunGaugeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 적립 1 - 피격 이벤트 대기 (어빌리티가 끝날 때까지 계속 수신)
	UAbilityTask_WaitGameplayEvent* WaitHitReact = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, CBGameplayTags::Event_Combat_HitReact, nullptr, false);
	WaitHitReact->EventReceived.AddDynamic(this, &ThisClass::OnHitReact);
	WaitHitReact->ReadyForActivation();

	// 적립 2 - 자기 어빌리티 종료 알림 구독
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::OnAbilityEnded);
	}
}

void UCBStunGaugeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 어빌리티 종료 알림 구독 해제 (피격 이벤트 대기는 태스크라 어빌리티와 함께 정리됨)
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->OnAbilityEnded.Remove(AbilityEndedHandle);
	}
	AbilityEndedHandle.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCBStunGaugeAbility::OnHitReact(FGameplayEventData Payload)
{
	AddStunGauge(GaugeGainPerHit);
}

void UCBStunGaugeAbility::OnAbilityEnded(const FAbilityEndedData& InEndedData)
{
	// 캔슬된 어빌리티는 쌓지 않음 (피격·기절에 끊긴 스킬 등)
	if (InEndedData.bWasCancelled || !InEndedData.AbilityThatEnded) return;

	// 식별 태그로 적립량 조회 (표에 없으면 적립 없음)
	for (const FGameplayTag& AbilityTag : InEndedData.AbilityThatEnded->GetAssetTags())
	{
		if (const float* Gain = AbilityGaugeGains.Find(AbilityTag))
		{
			AddStunGauge(*Gain);
			return;
		}
	}
}

void UCBStunGaugeAbility::AddStunGauge(float InGain)
{
	if (InGain <= 0.f) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	// 기절 중에는 쌓지 않음 (기절할 때 비운 게이지가 기절 동안 0으로 유지됨)
	if (ASC->HasMatchingGameplayTag(CBGameplayTags::Status_Combat_Stunned)) return;

	const FGameplayAttribute GaugeAttribute = UCBAttributeSet::GetStunGaugeAttribute();
	const float NewGauge = FMath::Min(ASC->GetNumericAttributeBase(GaugeAttribute) + InGain, UCBAttributeSet::MaxStunGauge);

	// 아직 덜 참 - 서버 권위 기록 (기본값에 쓰므로 전 클라로 복제됨)
	if (NewGauge < UCBAttributeSet::MaxStunGauge)
	{
		ASC->SetNumericAttributeBase(GaugeAttribute, NewGauge);
		return;
	}

	// 가득 참 - 게이지를 비우고 기절 이벤트 발행 (기절 어빌리티가 이 태그를 트리거로 발동)
	ASC->SetNumericAttributeBase(GaugeAttribute, 0.f);

	AActor* Avatar = GetAvatarActorFromActorInfo();
	FGameplayEventData Payload;
	Payload.EventTag = CBGameplayTags::Event_Combat_Stunned;
	Payload.Instigator = Avatar;
	Payload.Target = Avatar;

	// 발동된 어빌리티가 없으면 게이지만 비워지고 끝나므로 조용히 넘어가지 않게 경고
	if (ASC->HandleGameplayEvent(CBGameplayTags::Event_Combat_Stunned, &Payload) == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 기절 게이지가 가득 찼지만 기절 어빌리티가 발동되지 않음 - 로드아웃 부여·트리거(Event.Combat.Stunned)·발동 조건 확인"),
			*GetNameSafe(Avatar));
	}
}