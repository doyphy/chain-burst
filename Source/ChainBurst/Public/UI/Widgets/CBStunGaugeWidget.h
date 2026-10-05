#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CBStunGaugeWidget.generated.h"

class UCBAbilitySystemComponent;
struct FOnAttributeChangeData;

/**
 * 기절 게이지 표시 위젯의 공용 베이스 (보스 바의 기절 게이지).
 * 대상 ASC의 StunGauge 어트리뷰트 변경 델리게이트에 구독해 값 변화를 받아 BP 이벤트로 전달.
 * UI는 각 클라이언트 로컬이며, 값 동기화는 어트리뷰트 리플리케이션이 담당.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBStunGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 대상 ASC의 기절 게이지 어트리뷰트에 구독하고 초기값을 반영하는 함수.
	 * 재호출 시 기존 구독을 먼저 해제하므로 여러 번 불려도 안전.
	 * @param InASC 기절 게이지를 표시할 대상의 ASC
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|UI")
	void InitializeWithASC(UCBAbilitySystemComponent* InASC);

protected:
	//~ Begin UUserWidget Interface.
	/** 슬레이트가 화면에 붙을 때 호출되는 함수 */
	virtual void NativeConstruct() override;
	/** 슬레이트가 화면에서 제거될 때 호출되는 함수 */
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface.

	/**
	 * 기절 게이지 변경 시(및 초기화 시 1회) 호출되는 BP 이벤트. WBP가 비주얼(프로그레스 바 등)을 갱신.
	 * 기절이 시작되면 게이지가 비워지므로 0 이 들어옴.
	 * @param CurrentGauge 현재 게이지
	 * @param MaxGauge     최대 게이지 (UCBAttributeSet::MaxStunGauge)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnStunGaugeChanged(float CurrentGauge, float MaxGauge);

private:
	/** StunGauge 변경 델리게이트 내부 콜백 (현재 값을 읽어 BP 이벤트로 전달) */
	void HandleStunGaugeChanged(const FOnAttributeChangeData& Data);

	/** 현재 ASC 값으로 OnStunGaugeChanged 이벤트를 발화하는 함수 */
	void BroadcastStunGaugeChanged();

	/** 캐시된 대상 ASC에 구독하고 현재 값을 반영하는 함수 (초기화·재구성 공용, 중복 구독 방지 포함. 슬레이트가 없으면 둘 다 미룸) */
	void BindToASC();

	/** 어트리뷰트 변경 델리게이트 구독만 해제하는 함수 (대상 캐시는 유지) */
	void UnbindFromASC();

	/** 값 조회·재구독용 대상 ASC 캐시. 위젯이 화면에서 빠졌다 돌아와도 유지된다 */
	TWeakObjectPtr<UCBAbilitySystemComponent> CachedASC;

	/** StunGauge 변경 델리게이트 구독 해제용 핸들 */
	FDelegateHandle StunGaugeChangedHandle;
};
