#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CBNamePlateWidget.generated.h"

class ACBPlayerState;
class APlayerState;

/**
 * 캐릭터 이름표 위젯의 공용 베이스.
 * 대상 PlayerState 의 닉네임 변경 신호에 구독해 값 변화를 받고, 비주얼 갱신은 OnNicknameChanged BP 이벤트로 처리.
 * 닉네임 자체는 엔진의 PlayerName 이라 복제·맵 이관이 이미 보장되므로 이 위젯은 표시만 담당.
 * UI 는 각 클라이언트 로컬이며, 값 동기화는 PlayerState 리플리케이션이 담당.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBNamePlateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 대상의 닉네임 변경에 구독하고 현재 이름을 반영하는 함수.
	 * 재호출 시 기존 구독을 먼저 해제하므로 여러 번 불려도 안전.
	 * @param InPlayerState 이름을 표시할 대상 (ACBPlayerState 가 아니면 무시)
	 * @param bInIsLocalTarget 이 이름표의 주인이 로컬 플레이어인지 (판정은 UI 컴포넌트가 폰을 보고 함)
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|UI")
	void InitializeWithPlayerState(APlayerState* InPlayerState, bool bInIsLocalTarget);

	/** [Getter] 현재 대상의 닉네임 (대상이 없으면 빈 문자열) */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|UI")
	FString GetNickname() const;

	/** [Getter] 이름을 표시 중인 대상 (없으면 nullptr) */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|UI")
	ACBPlayerState* GetTargetPlayerState() const;

	/**
	 * 이 위젯의 소유자가 이 화면의 로컬 플레이어인지 여부 (자기 이름표를 다르게 꾸밀 때 사용).
	 * 값은 초기화 때 주입되므로 Event Construct 등 어느 시점에 읽어도 확정된 값.
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|UI")
	FORCEINLINE bool IsLocalPlayerTarget() const { return bIsLocalTarget; }

protected:
	//~ Begin UUserWidget Interface.
	/** 슬레이트가 화면에 붙을 때 호출되는 함수 */
	virtual void NativeConstruct() override;
	/** 슬레이트가 화면에서 제거될 때 호출되는 함수 */
	virtual void NativeDestruct() override;
	//~ End UUserWidget Interface.

	/**
	 * 닉네임이 확정되거나 바뀔 때 호출되는 BP 이벤트.
	 * @param Nickname 표시할 닉네임
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnNicknameChanged(const FString& Nickname);

private:
	/** 닉네임 변경 델리게이트 내부 콜백 (다이내믹 델리게이트라 UFUNCTION 필수) */
	UFUNCTION()
	void HandleNicknameChanged(const FString& Nickname);

	/** 캐시된 대상에 구독하고 현재 이름을 반영하는 함수 (초기화·재구성 공용, 중복 구독 방지 포함) */
	void BindToPlayerState();

	/** 닉네임 변경 델리게이트 구독을 해제하는 함수 (대상 캐시는 유지) */
	void UnbindFromPlayerState();

	/**
	 * 이 위젯을 소유한 캐릭터의 PlayerState
	 * 값 조회·재구독용 대상 캐시. 위젯이 화면에서 빠졌다 돌아와도 유지됨.
	 */
	TWeakObjectPtr<ACBPlayerState> CachedPlayerState;

	/** 이 위젯의 소유자가 로컬 플레이어인지. 초기화 때 UI 컴포넌트가 주입함. */
	bool bIsLocalTarget = false;
};
