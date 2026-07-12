// project
#include "AbilitySystem/Abilities/Movement/CBGAJump.h"
#include "CBGameplayTags.h"

// engine
#include "GameFramework/Character.h"

UCBGAJump::UCBGAJump()
{
	// 어빌리티 인스턴싱 정책 설정
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 로컬 입력으로 발동되어 즉시 반응해야 하므로 예측 실행
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 어빌리티 식별 태그 (C++ 최종 엣지 — 생성자에서 지정)
	SetAssetTags(FGameplayTagContainer(CBGameplayTags::Ability_Movement_Jump));

	// 대시 중 점프 차단 (루트모션이 수직 속도를 덮어써 점프가 씹힘)
	ActivationBlockedTags.AddTag(CBGameplayTags::Status_Movement_Dashing);
}

bool UCBGAJump::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags /* = nullptr */,
	const FGameplayTagContainer* TargetTags /* = nullptr */, FGameplayTagContainer* OptionalRelevantTags /* = nullptr */) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// CMC 점프 가능 여부 검사 (공중 재점프 등 불가 상황이면 활성화 자체를 차단)
	const ACharacter* Character = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	return Character && Character->CanJump();
}

void UCBGAJump::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// CMC 점프 트리거 — bPressedJump는 CMC 압축 플래그로 서버에 자동 복제됨
	Character->Jump();
}

void UCBGAJump::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	if (IsActive())
	{
		// 점프 입력 해제 (JumpMaxHoldTime 설정 시 가변 점프 높이 지원) 후 어빌리티 종료
		if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
		{
			Character->StopJumping();
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UCBGAJump::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 안전망 — 릴리즈 외 경로(취소 등)로 종료돼도 점프 입력 해제 보장 (중복 호출 무해)
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		Character->StopJumping();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
