#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CBExperienceBarWidget.generated.h"

class ACBGameplayGameState;
class AGameStateBase;

/**
 * HUD 경험치 바의 공용 베이스.
 * 게임플레이 게임 스테이트의 레벨·경험치 변경 신호를 구독해 BP 이벤트로 전달함. 다음 레벨까지 필요한 양은 레벨업 데이터에서 각자 계산.
 * 값은 게임 스테이트 복제로 오므로 UI 를 위한 네트워크 코드가 없음. 게임플레이 레벨이 아니면(로비 등) 아무것도 하지 않음.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBExperienceBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface.
	/** 레벨·경험치 변경 신호를 구독하고 현재 값을 한 번 반영함. */
	virtual void NativeConstruct() override;
	/** 구독을 해제함. */
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface.

	/**
	 * 레벨·경험치가 바뀌었을 때(및 구독 시 1회) 호출되는 BP 이벤트. WBP 가 비주얼(프로그레스 바·레벨 텍스트)을 갱신.
	 * @param Level 현재 레벨
	 * @param CurrentExperience 현재 레벨에서 쌓인 경험치 (최대 레벨이면 필요량과 같아 바가 가득 참)
	 * @param RequiredExperience 다음 레벨까지 필요한 경험치 (레벨업 데이터가 없으면 0)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnExperienceChanged(int32 Level, int32 CurrentExperience, int32 RequiredExperience);

private:
	/** 게임 스테이트의 변경 델리게이트 내부 콜백 (다이내믹 델리게이트라 UFUNCTION 필수) */
	UFUNCTION()
	void HandleExperienceChanged(int32 InLevel, int32 InExperience);

	/** 필요 경험치를 계산해 OnExperienceChanged 이벤트를 발화하는 함수 */
	void BroadcastExperience(int32 InLevel, int32 InExperience);

	/**
	 * 게임 스테이트의 변경 신호를 구독하는 함수 (중복 구독 방지 포함).
	 * 게임 스테이트가 아직 복제되지 않았으면 도착 시점을 기다렸다가 다시 시도함.
	 */
	void BindToGameState();

	/** 변경 신호 구독을 해제하는 함수 */
	void UnbindFromGameState();

	/** 게임 스테이트 도착 콜백 (복제가 위젯 생성보다 늦은 경우) */
	void HandleGameStateSet(AGameStateBase* InGameState);

	/** 구독 해제용 게임 스테이트 캐시 */
	TWeakObjectPtr<ACBGameplayGameState> CachedGameState;

	/** 게임 스테이트 도착 이벤트 구독 핸들. 유효하면 도착을 기다리는 중이라는 뜻 */
	FDelegateHandle GameStateSetHandle;
};
