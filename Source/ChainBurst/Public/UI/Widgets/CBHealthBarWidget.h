#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CBHealthBarWidget.generated.h"

class UCBAbilitySystemComponent;
struct FOnAttributeChangeData;

/**
 * 체력 표시 위젯의 공용 베이스.
 * 대상 ASC의 CurrentHealth/MaxHealth 어트리뷰트 변경 델리게이트에 구독해 값 변화를 받고,
 * 비주얼 갱신은 OnHealthChanged BP 이벤트로 위임한다. (HUD용/머리 위 바용 WBP가 공유 상속)
 * 구독 수명은 슬레이트 수명이 아니라 '대상' 수명을 따른다 — 위젯이 화면에서 빠지면 구독만 끊고
 * 대상 캐시는 남겨, 다시 화면에 붙을 때(NativeConstruct) 스스로 재구독하고 그사이 바뀐 값을 반영한다.
 * UI는 각 클라이언트 로컬이며, 값 동기화는 어트리뷰트 리플리케이션이 담당.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 대상 ASC의 체력 어트리뷰트에 구독하고 초기값을 반영하는 함수.
	 * 재호출 시 기존 구독을 먼저 해제하므로 여러 번 불려도 안전.
	 * @param InASC 체력을 표시할 대상의 ASC
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
	 * 체력 값 변경 시(및 초기화 시 1회) 호출되는 BP 이벤트. WBP가 비주얼(프로그레스 바 등)을 갱신.
	 * @param CurrentHealth 현재 체력
	 * @param MaxHealth     최대 체력
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnHealthChanged(float CurrentHealth, float MaxHealth);

private:
	/** CurrentHealth/MaxHealth 변경 델리게이트 내부 콜백 (현재 값을 읽어 BP 이벤트로 전달) */
	void HandleHealthAttributeChanged(const FOnAttributeChangeData& Data);

	/** 현재 ASC 값으로 OnHealthChanged 이벤트를 발화하는 함수 */
	void BroadcastHealthChanged();

	/** 캐시된 대상 ASC에 구독하고 현재 값을 반영하는 함수 (초기화·재구성 공용, 중복 구독 방지 포함) */
	void BindToASC();

	/** 어트리뷰트 변경 델리게이트 구독만 해제하는 함수 (대상 캐시는 유지) */
	void UnbindFromASC();

	/** 값 조회·재구독용 대상 ASC 캐시. 위젯이 화면에서 빠졌다 돌아와도 유지된다 */
	TWeakObjectPtr<UCBAbilitySystemComponent> CachedASC;

	/** CurrentHealth 변경 델리게이트 구독 해제용 핸들 */
	FDelegateHandle CurrentHealthChangedHandle;

	/** MaxHealth 변경 델리게이트 구독 해제용 핸들 */
	FDelegateHandle MaxHealthChangedHandle;
};
