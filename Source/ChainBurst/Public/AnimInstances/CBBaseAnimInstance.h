#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CBBaseAnimInstance.generated.h"

class ACBBaseCharacter;

UCLASS()
class CHAINBURST_API UCBBaseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:
	//~ Begin UAnimInstance Interface
	/** 캐릭터 시스템 준비 완료 델리게이트를 구독한다 (이미 준비되었으면 즉시 처리). */
	virtual void NativeInitializeAnimation() override;
	//~ End UAnimInstance Interface

	/**
	 * 캐릭터 시스템 준비 완료 시 1회 호출되는 초기화 훅.
	 * 자식 애님 인스턴스가 오버라이드해 "준비 완료 후" 필요한 초기 작업을 넣는다. (기본 구현은 비어 있음)
	 */
	virtual void OnCharacterSystemReady();

	/**
	 * 캐릭터 시스템 준비 전까지의 잠금 여부 (기본 true, 준비 완료 시 false).
	 * 업데이트·분기에서 `if (IsSystemLocked()) return;` 형태의 게이트로 사용.
	 */
	FORCEINLINE bool IsSystemLocked() const { return bIsSystemLocked; }

private:
	/** 캐릭터 시스템 준비 완료 델리게이트에 바인딩되는 내부 콜백 (구독 해제 + 잠금 해제 + OnCharacterSystemReady 호출) */
	void NativeOnCharacterSystemReady();

	/** 캐릭터 시스템 준비 전 잠금 플래그 */
	bool bIsSystemLocked = true;
};
