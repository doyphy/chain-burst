// project
#include "UI/Widgets/CBLevelUpWidget.h"
#include "Controllers/CBChaserController.h"
#include "Core/CBGameInstance.h"

// engine
#include "Engine/World.h"

// [로컬] 카드 제시. 인덱스를 카드로 풀어 BP 에 넘기고 카운트다운을 새로 시작함
void UCBLevelUpWidget::Local_ShowOfferedCards(const TArray<int32>& InCardIndices)
{
	const UCBGameInstance* CBGameInstance = GetGameInstance<UCBGameInstance>();
	const UCBLevelUpData* LevelUpData = CBGameInstance ? CBGameInstance->GetLevelUpData() : nullptr;
	if (!LevelUpData) return;

	// 서버와 같은 에셋이라 인덱스가 그대로 통함. 없는 인덱스도 빈 카드로 자리를 채워 슬롯 번호가 어긋나지 않게 함
	TArray<FCBLevelUpCard> OfferedCards;
	OfferedCards.Reserve(InCardIndices.Num());
	for (const int32 CardIndex : InCardIndices)
	{
		const FCBLevelUpCard* Card = LevelUpData->FindCard(CardIndex);
		if (!Card)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] 레벨업 데이터에 없는 카드 인덱스: %d"), *GetName(), CardIndex);
		}
		OfferedCards.Add(Card ? *Card : FCBLevelUpCard());
	}

	// 정지 중에는 월드 시간이 멈추므로 실제 시간 기준으로 셈
	const UWorld* World = GetWorld();
	OfferedRealTime = World ? World->GetRealTimeSeconds() : 0.0;
	SelectionTimeout = LevelUpData->GetSelectionTimeout();
	OfferedCardCount = OfferedCards.Num();
	bSelectionSent = false;

	OnCardsOffered(OfferedCards);
}

// [로컬] 카드 선택. 서버에 보내고 대기 상태로 바뀜
void UCBLevelUpWidget::Local_SelectCard(int32 InSlot)
{
	// 한 번 고른 뒤 다시 누른 경우 (서버도 거부하지만 RPC 를 낼 이유가 없음)
	if (bSelectionSent) return;

	if (InSlot < 0 || InSlot >= OfferedCardCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 범위를 벗어난 카드 슬롯: %d (제시된 카드 %d 장)"), *GetName(), InSlot, OfferedCardCount);
		return;
	}

	ACBChaserController* ChaserController = Cast<ACBChaserController>(GetOwningPlayer());
	if (!ChaserController) return;

	bSelectionSent = true;

	// 대기 표시를 먼저 함. 호스트는 서버 RPC 가 즉시 실행되어, 이어지는 레벨업의 카드 제시가
	// 이 호출 안에서 도착할 수 있음. 순서가 반대면 새 카드가 대기 상태로 덮임
	OnCardSelected(InSlot);
	
	ChaserController->Server_SelectLevelUpCard(InSlot);
}

float UCBLevelUpWidget::GetRemainingSelectionTime() const
{
	const UWorld* World = GetWorld();
	if (!World) return 0.f;

	const float ElapsedTime = static_cast<float>(World->GetRealTimeSeconds() - OfferedRealTime);
	return FMath::Max(0.f, SelectionTimeout - ElapsedTime);
}
