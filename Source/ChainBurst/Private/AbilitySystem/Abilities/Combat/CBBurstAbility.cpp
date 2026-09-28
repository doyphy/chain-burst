// project
#include "AbilitySystem/Abilities/Combat/CBBurstAbility.h"
#include "AbilitySystem/CBAttributeSet.h"
#include "CBGameplayTags.h"

// engine
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

UCBBurstAbility::UCBBurstAbility()
{
	// 로컬 입력으로 발동되어 즉시 반응해야 하므로 예측 실행
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 이 어빌리티를 식별하는 태그 (종류가 하나뿐이라 C++에서 지정).
	// 공격 어빌리티 BP의 BlockAbilitiesWithTag(Ability.Combat)에 걸려 공격 중에는 발동되지 않음.
	SetAssetTags(FGameplayTagContainer(CBGameplayTags::Ability_Combat_Burst));

	// 재생할 발동 몽타주 태그
	BoundActionTag = CBGameplayTags::Action_Combat_Burst;
}

bool UCBBurstAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 게이지가 가득 찼을 때만 발동. 복제되는 어트리뷰트라 예측 판정에 써도 서버와 어긋나지 않음.
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	return ASC && ASC->GetNumericAttribute(UCBAttributeSet::GetBurstGaugeAttribute()) >= UCBAttributeSet::MaxBurstGauge;
}

void UCBBurstAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 몽타주 재생 가능한지 확인 (베이스가 액션 컴포넌트/액션 태그 검사 후 세팅)
	if (!bCanPlayMontage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 버스트 확정. 몽타주보다 먼저 해서 몽타주가 끊기거나 재생에 실패해도 버스트는 유지됨.
	ApplyBurstState(Handle, ActorInfo, ActivationInfo);

	// 발동 몽타주 재생 (GameplayCue로 전 클라 동기화)
	PlayActionMontage();
}

void UCBBurstAbility::ApplyBurstState(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// GE 유효성 검사
	if (!BurstEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] BurstEffectClass 미지정"), *GetName());
		return;
	}

	// 버스트 GE 적용. (활성화 예측 키로 적용되므로 소유 클라에 즉시 반영되고, 서버 확정 후 전 클라로 복제됨)
	// 서버에서 거부되면 몽타주만 재생되고 GE 값 모두 되돌림.
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(BurstEffectClass);
	if (!SpecHandle.IsValid()) return;
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);

	// [서버] 게이지 소모. 게이지는 서버만 기록하므로 예측하지 않음 (소유 클라에는 복제로 도착)
	if (HasAuthority(&ActivationInfo))
	{
		ActorInfo->AbilitySystemComponent->SetNumericAttributeBase(UCBAttributeSet::GetBurstGaugeAttribute(), 0.f);
	}
}
