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
 * UI는 각 클라이언트 로컬이며, 값 동기화는 어트리뷰트 리플리케이션이 담당.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 대상 ASC의 체력 어트리뷰트에 구독하고 초기값을 반영하는 함수 (UI 컴포넌트가 준비 완료 후 호출)
	 * @param InASC 체력을 표시할 대상의 ASC
	 */
	void InitializeWithASC(UCBAbilitySystemComponent* InASC);

protected:
	//~ Begin UUserWidget Interface.
	/** 어트리뷰트 변경 델리게이트 구독을 해제. */
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

	/** 어트리뷰트 변경 델리게이트 구독을 해제하고 캐시를 비우는 함수 (재초기화·파괴 공용) */
	void UnbindFromASC();

	/** 값 조회·구독 해제용 ASC 캐시 */
	TWeakObjectPtr<UCBAbilitySystemComponent> CachedASC;

	/** CurrentHealth 변경 델리게이트 구독 해제용 핸들 */
	FDelegateHandle CurrentHealthChangedHandle;

	/** MaxHealth 변경 델리게이트 구독 해제용 핸들 */
	FDelegateHandle MaxHealthChangedHandle;
};
