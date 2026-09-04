#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CBPlayerListEntryWidget.generated.h"

class ACBPlayerState;
class APlayerState;
class UCBHealthBarWidget;
class UCBNamePlateWidget;

/**
 * HUD 플레이어 목록의 행 하나에 대한 공용 베이스.
 * PlayerState 하나만 받아 이름표·체력바 자식 위젯을 배선함 — Chaser 는 ASC 를 PlayerState 가 소유하므로
 * 남의 폰을 찾지 않아도 PlayerState 한 곳에서 닉네임과 체력이 모두 나옴.
 * 폰이 죽거나 다시 스폰돼도 ASC 는 PlayerState 와 함께 살아남아 체력 구독이 끊기지 않음.
 * 자식 위젯은 WBP 에서 같은 이름으로 배치하면 자동 연결되고(BindWidgetOptional), 없으면 그 부분만 건너뜀.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBPlayerListEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 이 행이 표시할 대상을 지정하고 자식 위젯을 배선하는 함수.
	 * 재호출 시 자식 위젯이 각자 기존 구독을 해제하므로 여러 번 불려도 안전.
	 * @param InPlayerState 표시할 대상 (ACBPlayerState 가 아니면 무시)
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|UI")
	void InitializeWithPlayerState(APlayerState* InPlayerState);

	/** [Getter] 이 행이 표시 중인 대상 (없으면 nullptr) */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|UI")
	ACBPlayerState* GetTargetPlayerState() const;

protected:
	/**
	 * 대상이 지정된 뒤 호출되는 BP 이벤트. 이름·체력 외의 표시(아이콘·팀 색 등)를 WBP 가 갱신.
	 * @param InPlayerState 표시할 대상
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnPlayerStateSet(ACBPlayerState* InPlayerState);

	/** 닉네임 표시 자식 위젯. WBP 에 같은 이름으로 두면 자동 연결됨 */
	UPROPERTY(BlueprintReadOnly, Category = "ChainBurst|UI", meta = (BindWidgetOptional))
	TObjectPtr<UCBNamePlateWidget> NamePlateWidget = nullptr;

	/** 체력 표시 자식 위젯. WBP 에 같은 이름으로 두면 자동 연결됨 */
	UPROPERTY(BlueprintReadOnly, Category = "ChainBurst|UI", meta = (BindWidgetOptional))
	TObjectPtr<UCBHealthBarWidget> HealthBarWidget = nullptr;

private:
	/** 표시 중인 대상 캐시 */
	TWeakObjectPtr<ACBPlayerState> CachedPlayerState;
};
