#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/CBCharacterAnimInstance.h"
#include "CBAIAnimInstance.generated.h"

/**
 * AI가 조작하는 캐릭터의 애님 인스턴스 베이스.
 * 포커스/타겟 방향과 AI 상태(순찰·전투·경계)를 기준으로 구동되는 애니메이션 데이터를 관리.
 */
UCLASS()
class CHAINBURST_API UCBAIAnimInstance : public UCBCharacterAnimInstance
{
	GENERATED_BODY()

public:
	/**
	 * 타겟을 주시한 채 이동하는 중인지 여부 (Status.Movement.Strafe 태그 반영).
	 * ABP 상태 머신이 전방 이동 클립 대신 2D 블렌드스페이스로 분기하는 기준.
	 */
	UFUNCTION(BlueprintPure, Category = "AnimData|AI", meta = (BlueprintThreadSafe))
	bool IsStrafing() const { return bIsStrafing; }

protected:
	//~ Begin UAnimInstance Interface
	/** 애님 인스턴스 정리 시점 */
	virtual void NativeUninitializeAnimation() override;
	//~ End UAnimInstance Interface

	//~ Begin UCBBaseAnimInstance Interface
	/** 캐릭터 시스템 준비 완료 */
	virtual void OnCharacterSystemReady() override;
	//~ End UCBBaseAnimInstance Interface

	/** 스트레이프 태그 변경 시 호출되는 콜백 함수 */
	void OnStrafeTagChanged(const FGameplayTag InTag, int32 InCount);

	UPROPERTY(BlueprintReadOnly, Transient, Category = "AnimData|AI")
	bool bIsStrafing = false;
};
