#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DataAssets/LevelUp/CBLevelUpData.h"
#include "CBLevelUpWidget.generated.h"

/**
 * 레벨업 카드 선택 위젯의 공용 베이스.
 * 서버가 이 플레이어에게 뽑아 준 카드를 표시하고, 고른 카드의 순서(슬롯)를 소유 컨트롤러를 거쳐 서버로 보냄.
 * 생성·스택 삽입·제거는 소유 컨트롤러(ACBChaserController)가 하고, 이 위젯은 표시와 선택 전송만 함.
 * 월드가 정지된 동안 떠 있으므로 남은 시간은 실제 시간으로 셈.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBLevelUpWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * [로컬] 제시받은 카드를 표시하는 함수. (소유 컨트롤러가 카드 제시 RPC 를 받으면 호출)
	 * 대기 상태를 풀고 남은 시간을 처음부터 다시 셈. 레벨업이 이어지면 같은 위젯에 다시 불림.
	 * @param InCardIndices 서버가 뽑아 준 카드 인덱스 (레벨업 데이터의 카드 배열 기준, 배열 순서 = 슬롯)
	 */
	void Local_ShowOfferedCards(const TArray<int32>& InCardIndices);

	/**
	 * [로컬] 카드를 고르는 함수. 서버에 선택을 보내고 다른 플레이어를 기다리는 상태로 바뀜.
	 * 이미 골랐으면 무시함. 제시받은 카드인지 검증은 서버가 함.
	 * @param InSlot 제시받은 카드 중 몇 번째인지 (0부터)
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|LevelUp")
	void Local_SelectCard(int32 InSlot);

	/** 선택 제한까지 남은 시간(초, 실제 시간). 0 아래로 내려가지 않음 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|LevelUp")
	float GetRemainingSelectionTime() const;

	/** 카드를 이미 골라 다른 플레이어를 기다리는 중인지 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|LevelUp")
	FORCEINLINE bool IsWaitingForOthers() const { return bSelectionSent; }

protected:
	/**
	 * 카드가 제시됐을 때 호출되는 BP 이벤트. WBP 가 카드 칸을 채우고 선택 가능한 상태로 되돌림.
	 * @param Cards 제시된 카드 (배열 순서 = Local_SelectCard 에 넘길 슬롯 번호)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|LevelUp")
	void OnCardsOffered(const TArray<FCBLevelUpCard>& Cards);

	/**
	 * 카드를 고른 직후 호출되는 BP 이벤트. WBP 가 대기 표시로 바꿈.
	 * @param SelectedSlot 고른 카드 슬롯 (UWidget::Slot 과 이름이 겹쳐 Selected 를 붙임)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|LevelUp")
	void OnCardSelected(int32 SelectedSlot);

private:
	/** 카드가 제시된 시각 (월드 실제 시간, 정지 중에도 흐름) */
	double OfferedRealTime = 0.0;

	/** 이번 제시의 선택 제한 시간(초) */
	float SelectionTimeout = 0.f;

	/** 제시된 카드 수. 슬롯 범위 확인용 */
	int32 OfferedCardCount = 0;

	/** 이번 제시에서 이미 골랐는지 */
	bool bSelectionSent = false;
};
