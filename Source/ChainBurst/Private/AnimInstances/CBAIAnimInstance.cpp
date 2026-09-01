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
