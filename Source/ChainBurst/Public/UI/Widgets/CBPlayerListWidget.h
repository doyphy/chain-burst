#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CBPlayerListWidget.generated.h"

class ACBChaserController;
class ACBGameStateBase;
class UCBPlayerListEntryWidget;
class UPanelWidget;

/**
 * HUD 의 플레이어 목록 위젯 공용 베이스 (자기 자신을 제외한 플레이어의 닉네임·체력).
 * 게임 스테이트의 목록 변경 신호를 구독해 행 위젯을 다시 만들고, 값 갱신은 각 행의 자식 위젯이 스스로 구독해 처리.
 * 목록도 값도 전부 복제된 데이터를 각 클라이언트가 로컬로 읽는 구조라 UI 를 위한 네트워크 코드가 없음.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBPlayerListWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget Interface.
		/** 목록 변경 신호를 구독하고 현재 목록으로 한 번 채움. */
	virtual void NativeConstruct() override;
		/** 구독을 해제함 (대상 캐시는 유지). */
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface.

	/** 행을 담을 패널. WBP 에 같은 이름으로 배치해야 함 */
	UPROPERTY(BlueprintReadOnly, Category = "ChainBurst|UI", meta = (BindWidget))
	TObjectPtr<UPanelWidget> EntryContainer = nullptr;

	/** 행 위젯 클래스 (플레이어 한 명) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|UI")
	TSubclassOf<UCBPlayerListEntryWidget> EntryWidgetClass = nullptr;

private:
	/** 목록 변경 델리게이트 내부 콜백 (다이내믹 델리게이트라 UFUNCTION 필수) */
	UFUNCTION()
	void HandlePlayerListChanged();

	/** 로컬 PlayerState 확정 콜백 (다이내믹 델리게이트라 UFUNCTION 필수) */
	UFUNCTION()
	void HandleLocalPlayerStateSet();

	/**
	 * 소유 컨트롤러의 PlayerState 확정 신호를 구독하는 함수 (중복 구독 방지 포함).
	 * 이미 확정돼 있으면 신호가 오지 않으므로, 구독만 걸고 목록은 호출자가 채운다.
	 */
	void BindToOwningController();

	/** 컨트롤러 신호 구독을 해제하는 함수 */
	void UnbindFromOwningController();

	/**
	 * 게임 스테이트의 목록 변경 신호를 구독하는 함수 (중복 구독 방지 포함).
	 * 게임 스테이트가 아직 복제되지 않았으면 도착 시점을 기다렸다가 다시 시도함.
	 */
	void BindToGameState();

	/** 목록 변경 신호 구독을 해제하는 함수 */
	void UnbindFromGameState();

	/** 게임 스테이트 도착 콜백 (복제가 위젯 생성보다 늦은 경우) */
	void HandleGameStateSet(AGameStateBase* InGameState);

	/** 현재 PlayerArray 로 행을 다시 만드는 함수 (전 인스턴스 각자 로컬 실행). */
	void Local_RefreshEntries();

	/** 구독 해제용 게임 스테이트 캐시 */
	TWeakObjectPtr<ACBGameStateBase> CachedGameState;

	/** 게임 스테이트 도착 이벤트 구독 핸들. 유효하면 도착을 기다리는 중이라는 뜻 */
	FDelegateHandle GameStateSetHandle;

	/** 구독 해제용 소유 컨트롤러 캐시 */
	TWeakObjectPtr<ACBChaserController> CachedOwningController;
};
