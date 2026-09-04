// project
#include "AnimInstances/CBAIAnimInstance.h"
#include "CBGameplayTags.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Characters/CBBaseCharacter.h"

// 캐릭터 시스템 준비 완료 후 스트레이프 태그를 구독 (베이스가 캐릭터 캐싱까지 끝낸 뒤)
void UCBAIAnimInstance::OnCharacterSystemReady()
{
	Super::OnCharacterSystemReady();

	if (!CachedCharacter.IsValid()) return;

	if (UCBAbilitySystemComponent* ASC = CachedCharacter->GetCBAbilitySystemComponent())
	{
		// 스트레이프 태그 변경 시 OnStrafeTagChanged 함수 호출
		ASC->RegisterGameplayTagEvent(
			CBGameplayTags::Status_Movement_Strafe,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UCBAIAnimInstance::OnStrafeTagChanged);

		// 구독은 과거 변화를 소급 발화하지 않으므로 현재 상태를 1회 반영.
		// 없으면 이미 스트레이프 중인 AI 에 뒤늦게 관련성을 얻은 클라가 주시하지 않는 모션으로 시작한다
		bIsStrafing = ASC->HasMatchingGameplayTag(CBGameplayTags::Status_Movement_Strafe);
	}
}

// 애님 인스턴스 정리 시점. ASC에 걸어둔 태그 이벤트 구독을 해제한다.
void UCBAIAnimInstance::NativeUninitializeAnimation()
{
	if (CachedCharacter.IsValid())
	{
		if (UCBAbilitySystemComponent* ASC = CachedCharacter->GetCBAbilitySystemComponent())
		{
			// 구독 해제
			ASC->RegisterGameplayTagEvent(
				CBGameplayTags::Status_Movement_Strafe,
				EGameplayTagEventType::NewOrRemoved)
			.RemoveAll(this);
		}
	}

	Super::NativeUninitializeAnimation();
}

void UCBAIAnimInstance::OnStrafeTagChanged(const FGameplayTag InTag, int32 InCount)
{
	// 태그가 1개 이상 추가되면 true, 모두 제거되면 false
	bIsStrafing = (InCount > 0);
}
