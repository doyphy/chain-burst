// project
#include "AbilitySystem/Abilities/CBEventActionAbility.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"

UCBEventActionAbility::UCBEventActionAbility()
{
	// 이벤트는 서버에서 발행되므로 서버 주도 실행
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

void UCBEventActionAbility::RegisterEventTrigger(const FGameplayTag& InEventTag)
{
	if (!InEventTag.IsValid())
	{
		return;
	}

	// 지정한 이벤트 태그를 GameplayEvent 트리거로 등록 (수신 시 자동 발동)
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = InEventTag;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
}

void UCBEventActionAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// [디버그] 누구의 / 어떤 어빌리티가 / 어떤 이벤트 태그로 발동됐는지 로그
	{
		const AActor* AvatarActor = GetAvatarActorFromActorInfo();
		const FString ActorName = AvatarActor ? AvatarActor->GetName() : TEXT("Unknown");
		const FString TriggerTag = (TriggerEventData && TriggerEventData->EventTag.IsValid())
			? TriggerEventData->EventTag.ToString()
			: TEXT("(이벤트 태그 없음)");
		const TCHAR* NetContext = (ActorInfo && ActorInfo->IsNetAuthority()) ? TEXT("Server") : TEXT("Client");

		UE_LOG(LogTemp, Log, TEXT("[EventAction][%s] %s 의 %s 어빌리티가 '%s' 태그로 발동"),
			NetContext, *ActorName, *GetClass()->GetName(), *TriggerTag);
	}

	// 발동 시 진행 중인 지정 액션을 캔슬 (예: 공격 중 피격 → 휘두르던 공격 중단)
	if (CancelActionTag.IsValid())
	{
		if (UCBAbilitySystemComponent* CBASC = GetCBAbilitySystemComponentFromActorInfo())
		{
			FGameplayTagContainer CancelTags;
			CancelTags.AddTag(CancelActionTag);
			CBASC->CancelAbilities(&CancelTags, nullptr, this);
		}
	}

	// 몽타주 재생 (베이스의 공용 재생 로직 사용)
	PlayActionMontage();
}
