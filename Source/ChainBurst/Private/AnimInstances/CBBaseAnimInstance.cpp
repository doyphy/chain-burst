#include "AnimInstances/CBBaseAnimInstance.h"

#include "Characters/CBBaseCharacter.h"

void UCBBaseAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 소유 캐릭터가 있어야 준비 완료 시스템을 구독할 수 있음
	if (ACBBaseCharacter* OwnerCharacter = Cast<ACBBaseCharacter>(TryGetPawnOwner()))
	{
		if (OwnerCharacter->IsCharacterSystemReady())
		{
			// 이미 시스템이 준비된 상태라면 즉시 처리
			NativeOnCharacterSystemReady();
		}
		else
		{
			// 캐릭터 시스템 준비 완료 델리게이트에 바인딩
			OwnerCharacter->OnCharacterSystemReadyDelegate.AddUObject(this, &ThisClass::NativeOnCharacterSystemReady);
		}
	}
}

void UCBBaseAnimInstance::NativeOnCharacterSystemReady()
{
	// 중복 실행 방지: 이미 준비 처리되어 잠금이 해제되었으면 무시 (OnCharacterSystemReady는 1회만 실행)
	if (!bIsSystemLocked)
	{
		return;
	}

	// 델리게이트 구독 해제 (1회성)
	if (ACBBaseCharacter* OwnerCharacter = Cast<ACBBaseCharacter>(TryGetPawnOwner()))
	{
		OwnerCharacter->OnCharacterSystemReadyDelegate.RemoveAll(this);
	}

	// 잠금 해제
	bIsSystemLocked = false;

	// 자식 애님 인스턴스의 초기화 훅 호출
	OnCharacterSystemReady();
}

void UCBBaseAnimInstance::OnCharacterSystemReady()
{
	// 기본 구현은 비어 있음. 자식 애님 인스턴스가 필요 시 오버라이드.
}
