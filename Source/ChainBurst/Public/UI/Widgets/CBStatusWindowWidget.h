#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "CBStatusWindowWidget.generated.h"

class UCBAbilitySystemComponent;
class UCBAttributeTextBlock;
struct FOnAttributeChangeData;

/**
 * 상태창(자기 능력치를 보여 주는 표시 전용 패널)의 공용 베이스. HUD 컨테이너 WBP 안에 자식으로 둠.
 * - 값: WBP 에 배치된 UCBAttributeTextBlock 을 모두 찾아, 각 텍스트가 고른 어트리뷰트에 구독하고 값이 바뀌면 그 텍스트를 갱신함.
 *   어떤 어트리뷰트를 띄울지는 WBP 에서 텍스트를 배치하고 어트리뷰트를 고르는 것으로 정함 (C++ 수정 없음).
 * - 열림: 상태창 어빌리티가 활성 동안 붙이는 Status.UI.StatusWindow 태그를 구독해
 *   OnStatusWindowOpened / OnStatusWindowClosed BP 이벤트로 위임 (슬라이드 연출은 WBP).
 * 구독은 슬레이트가 있는 동안만, 대상 캐시는 '대상' 수명을 따른다 (UCBHealthBarWidget 과 같은 계약).
 * UI는 각 클라이언트 로컬이며, 값 동기화는 어트리뷰트 리플리케이션이 담당.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBStatusWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 대상 ASC 에 구독하고 현재 값·열림 상태를 반영하는 함수.
	 * 재호출 시 기존 구독을 먼저 해제하므로 여러 번 불려도 안전.
	 * @param InASC 능력치를 표시할 대상의 ASC
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|UI")
	void InitializeWithASC(UCBAbilitySystemComponent* InASC);

protected:
	//~ Begin UUserWidget Interface.
	/** 위젯 트리가 만들어진 직후 1회 호출되는 함수 (배치된 어트리뷰트 텍스트 수집) */
	virtual void NativeOnInitialized() override;
	/** 슬레이트가 화면에 붙을 때 호출되는 함수 */
	virtual void NativeConstruct() override;
	/** 슬레이트가 화면에서 제거될 때 호출되는 함수 */
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface.

	/**
	 * 상태창이 열릴 때 호출되는 BP 이벤트. WBP 가 슬라이드인 연출 (Play Animation Forward).
	 * 열림 상태가 바뀔 때만 호출됨 - 디자이너의 기본 모습은 닫힌 상태로 둘 것.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnStatusWindowOpened();

	/**
	 * 상태창이 닫힐 때 호출되는 BP 이벤트. WBP 가 슬라이드아웃 연출 (Play Animation Reverse).
	 * 열림 상태가 바뀔 때만 호출됨.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnStatusWindowClosed();

private:
	/** 캐시된 대상 ASC 에 구독하고 현재 상태를 반영하는 함수 (초기화·재구성 공용, 중복 구독 방지 포함. 슬레이트가 없으면 둘 다 미룸) */
	void BindToASC();

	/** 어트리뷰트·태그 델리게이트 구독만 해제하는 함수 (대상 캐시는 유지) */
	void UnbindFromASC();

	/** 어트리뷰트 변경 델리게이트 내부 콜백 (그 어트리뷰트를 고른 텍스트만 갱신) */
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);

	/** Status.UI.StatusWindow 태그 카운트 변경 델리게이트 내부 콜백 (카운트 유무로 열림 상태를 전환) */
	void HandleOpenTagChanged(const FGameplayTag InTag, int32 NewCount);

	/** 열림 상태를 전환하는 함수. 바뀌었을 때만 열림·닫힘 BP 이벤트를 발화 */
	void SetOpen(bool bNewOpen);

	/** 값 조회·재구독용 대상 ASC 캐시. 위젯이 화면에서 빠졌다 돌아와도 유지 */
	TWeakObjectPtr<UCBAbilitySystemComponent> CachedASC;

	/** WBP 에 배치된 어트리뷰트 텍스트 (어트리뷰트 미지정 텍스트는 제외) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCBAttributeTextBlock>> AttributeTexts;

	/** 어트리뷰트별 변경 델리게이트 구독 해제용 핸들 (같은 어트리뷰트를 여러 텍스트가 골라도 구독은 하나) */
	TMap<FGameplayAttribute, FDelegateHandle> AttributeChangedHandles;

	/** Status.UI.StatusWindow 태그 카운트 변경 델리게이트 구독 해제용 핸들 */
	FDelegateHandle OpenTagChangedHandle;

	/** 현재 열림 상태. 바뀔 때만 BP 이벤트를 발화하기 위한 비교값 (디자이너 기본 모습 = 닫힘) */
	bool bIsOpen = false;
};
