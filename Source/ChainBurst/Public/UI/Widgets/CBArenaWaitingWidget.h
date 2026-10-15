#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "CBArenaWaitingWidget.generated.h"

class UCBAbilitySystemComponent;

/**
 * 보스 구역의 "다른 플레이어 대기 중" + 보스 등장 카운트다운 표시의 공용 베이스. HUD 컨테이너 WBP 안에 자식으로 둠.
 * - 대기: 보스 구역(ACBBossArena, 서버)에서 플레이어 ASC에 붙이는 Status.Arena.Waiting 태그를 구독해
 *   OnWaitingChanged BP 이벤트로 표현 처리 (보이기·숨기기·연출은 WBP).
 * - 카운트다운: 보스 구역이 거는 카운트다운 기간 GE 의 Status.Arena.Countdown 태그로 시작·종료를 감지하고,
 *   카운트다운 중에는 매 틱 그 GE 의 남은 시간을 OnCountdownProgress 로 전달 (UCBBurstGaugeWidget 의 버스트 모드와 같은 방식).
 * 구독은 슬레이트가 있는 동안만, 대상 캐시는 '대상' 수명을 따른다 (UCBHealthBarWidget 과 같은 계약).
 * UI는 각 클라이언트 로컬이며, 상태 동기화는 태그·GE 리플리케이션이 담당.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBArenaWaitingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 대상 ASC 에 구독하고 현재 대기 상태를 반영하는 함수.
	 * 재호출 시 기존 구독을 먼저 해제하므로 여러 번 불려도 안전.
	 * @param InASC 대기 상태를 표시할 대상(자기 플레이어)의 ASC
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|UI")
	void InitializeWithASC(UCBAbilitySystemComponent* InASC);

protected:
	//~ Begin UUserWidget Interface.
	/** 슬레이트가 화면에 붙을 때 호출되는 함수 */
	virtual void NativeConstruct() override;
	/** 슬레이트가 화면에서 제거될 때 호출되는 함수 */
	virtual void NativeDestruct() override;
	/** 카운트다운 중일 때만 남은 시간을 갱신함 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface.

	/**
	 * 대기 상태를 반영하라는 BP 이벤트. WBP 가 표시를 켜고 끔.
	 * 구독할 때 현재 상태로 한 번, 이후 바뀔 때마다 호출됨 - 디자이너 기본 모습에 기대지 않아도 됨.
	 * @param bIsWaiting true = 구역 안에서 다른 플레이어를 기다리는 중
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnWaitingChanged(bool bIsWaiting);

	/** 보스 등장 카운트다운이 시작될 때 호출되는 BP 이벤트. (직후 같은 프레임에 OnCountdownProgress 가 한 번 옴) */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnCountdownStarted();

	/**
	 * 카운트다운 중 매 틱 호출되는 BP 이벤트. 남은 시간은 서버가 건 GE 에서 읽음.
	 * @param RemainingTime 보스 등장까지 남은 시간(초). 정수로 보이려면 WBP 에서 올림(Ceil)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnCountdownProgress(float RemainingTime);

	/** 카운트다운이 끝날 때 호출되는 BP 이벤트. (보스 등장 또는 누가 나가서 취소 - 구분은 OnWaitingChanged 로) */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnCountdownEnded();

private:
	/** 캐시된 대상 ASC 에 구독하고 현재 상태를 반영하는 함수 (초기화·재구성 공용, 중복 구독 방지 포함. 슬레이트가 없으면 둘 다 미룸) */
	void BindToASC();

	/** 태그 델리게이트 구독만 해제하는 함수 (대상 캐시는 유지) */
	void UnbindFromASC();

	/** Status.Arena.Waiting 태그 카운트 변경 델리게이트 내부 콜백 */
	void HandleWaitingTagChanged(const FGameplayTag InTag, int32 NewCount);

	/** Status.Arena.Countdown 태그 카운트 변경 델리게이트 내부 콜백 (카운트 유무로 카운트다운 상태를 전환) */
	void HandleCountdownTagChanged(const FGameplayTag InTag, int32 NewCount);

	/** 카운트다운 상태를 전환하는 함수. 바뀌었을 때만 시작·종료 BP 이벤트를 발화 */
	void SetCountingDown(bool bNewCountingDown);

	/** 카운트다운 GE 의 남은 시간을 읽어 OnCountdownProgress 로 전달하는 함수 (GE 가 아직 없으면 보내지 않음) */
	void BroadcastCountdownProgress();

	/** 재구독용 대상 ASC 캐시. 위젯이 화면에서 빠졌다 돌아와도 유지 */
	TWeakObjectPtr<UCBAbilitySystemComponent> CachedASC;

	/** Status.Arena.Waiting 태그 카운트 변경 델리게이트 구독 해제용 핸들 */
	FDelegateHandle WaitingTagChangedHandle;

	/** Status.Arena.Countdown 태그 카운트 변경 델리게이트 구독 해제용 핸들 */
	FDelegateHandle CountdownTagChangedHandle;

	/** 현재 카운트다운 중인지. 바뀔 때만 BP 이벤트를 발화하고, 이 동안만 틱에서 남은 시간을 읽음 */
	bool bIsCountingDown = false;
};
