#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Types/CBDelegates.h"
#include "CBBurstGaugeWidget.generated.h"

class UCBAbilitySystemComponent;
struct FOnAttributeChangeData;

/**
 * HUD 버스트 게이지 위젯의 공용 베이스. 한 바가 두 모드로 동작.
 * - 충전 : BurstGauge 어트리뷰트 변경을 구독해 OnBurstGaugeChanged 로 전달
 * - 버스트 : Status.Combat.Burst 태그 카운트 변화로 시작·종료를 감지하고, 버스트 중에는 매 틱 버스트 GE의 남은 시간을 OnBurstProgress 로 전달
 * 발동 가능 여부(게이지 가득 참 && 버스트 중 아님)가 바뀌면 OnBurstReadyChanged 를 방송함. (HUD 등 외부 위젯이 구독)
 */
UCLASS(Abstract)
class CHAINBURST_API UCBBurstGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 대상 ASC의 버스트 게이지·버스트 상태에 구독하고 현재 상태를 반영하는 함수.
	 * 재호출 시 기존 구독을 먼저 해제하므로 여러 번 불려도 안전.
	 * @param InASC 버스트 게이지를 표시할 대상의 ASC
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|UI")
	void InitializeWithASC(UCBAbilitySystemComponent* InASC);

	/** 발동 가능 여부(게이지 가득 참 && 버스트 중 아님)가 바뀔 때 방송. */
	UPROPERTY(BlueprintAssignable, Category = "ChainBurst|UI")
	FCBOnBurstReadyChanged OnBurstReadyChanged;

protected:
	//~ Begin UUserWidget Interface.
		/** 슬레이트가 화면에 붙을 때 호출되는 함수 */
	virtual void NativeConstruct() override;
		/** 슬레이트가 화면에서 제거될 때 호출되는 함수 */
	virtual void NativeDestruct() override;
		/** 매 프레임 호출되는 함수 (버스트 중에만 실제 작업 수행) */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface.

	/**
	 * [충전] 게이지 변경 시(초기화·버스트 종료 직후 1회 포함) 호출되는 BP 이벤트. 버스트 중에는 호출되지 않음.
	 * @param CurrentGauge 현재 게이지
	 * @param MaxGauge     최대 게이지 (가득 참 판정은 CurrentGauge >= MaxGauge)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnBurstGaugeChanged(float CurrentGauge, float MaxGauge);

	/** [버스트] 버스트가 시작될 때 1회 호출되는 BP 이벤트 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnBurstStarted();

	/**
	 * [버스트] 버스트 중 매 틱(및 시작 시 1회) 호출되는 BP 이벤트.
	 * @param RemainingRatio 남은 비율. 1 = 방금 발동, 0 = 종료
	 * @param RemainingTime  남은 시간(초)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnBurstProgress(float RemainingRatio, float RemainingTime);

	/** [버스트] 버스트가 끝날 때 1회 호출되는 BP 이벤트. 직후 OnBurstGaugeChanged 가 현재 게이지로 한 번 호출됨 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnBurstEnded();

private:
	/** 캐시된 대상 ASC에 구독하고 현재 상태를 반영하는 함수 (초기화·재구성 공용, 중복 구독 방지 포함) */
	void BindToASC();

	/** 게이지·태그 델리게이트 구독만 해제하는 함수 (대상 캐시는 유지) */
	void UnbindFromASC();

	/** BurstGauge 변경 델리게이트 내부 콜백 (버스트 중이면 무시) */
	void HandleBurstGaugeChanged(const FOnAttributeChangeData& Data);

	/** Status.Combat.Burst 태그 카운트 변경 델리게이트 내부 콜백 (카운트 유무로 버스트 모드를 전환) */
	void HandleBurstTagChanged(const FGameplayTag InTag, int32 NewCount);

	/** 버스트 모드를 전환하는 함수. 시작이면 시작·진행률 이벤트, 종료면 종료·현재 게이지 이벤트를 발화 */
	void SetBurstActive(bool bNewBurstActive);

	/** 현재 게이지 값으로 OnBurstGaugeChanged 이벤트를 발화하고 발동 가능 여부를 갱신하는 함수 */
	void BroadcastBurstGaugeChanged();

	/** 발동 가능 여부를 다시 계산해 바뀌었을 때만 OnBurstReadyChanged 를 방송하는 함수 */
	void UpdateBurstReady();

	/** 버스트 GE의 남은 시간·전체 길이로 남은 비율을 계산해 OnBurstProgress 이벤트를 발화하는 함수 */
	void BroadcastBurstProgress();

	/**
	 * 활성 버스트 GE에서 남은 시간과 전체 지속시간을 조회하는 함수.
	 * @param InASC        조회할 대상 ASC
	 * @param OutRemaining 남은 시간(초)
	 * @param OutDuration  버스트 전체 지속시간(초). 무한 지속이면 0 이하
	 * @return 조회에 성공했으면 true. 해당하는 활성 버스트 GE가 없으면 false
	 */
	bool QueryBurstTime(const UCBAbilitySystemComponent* InASC, float& OutRemaining, float& OutDuration) const;

	/** 값 조회·재구독용 대상 ASC 캐시. 위젯이 화면에서 빠졌다 돌아와도 유지 */
	TWeakObjectPtr<UCBAbilitySystemComponent> CachedASC;

	/** BurstGauge 변경 델리게이트 구독 해제용 핸들 */
	FDelegateHandle BurstGaugeChangedHandle;

	/** Status.Combat.Burst 태그 카운트 변경 델리게이트 구독 해제용 핸들 */
	FDelegateHandle BurstTagChangedHandle;

	/** 현재 버스트 모드인지 여부. 매 프레임 틱의 조기 반환 게이트 겸 모드 전환 판정에 사용 */
	bool bIsBurstActive = false;

	/** 마지막으로 방송한 발동 가능 여부. 바뀔 때만 방송하기 위한 비교값 */
	bool bIsBurstReady = false;
};
