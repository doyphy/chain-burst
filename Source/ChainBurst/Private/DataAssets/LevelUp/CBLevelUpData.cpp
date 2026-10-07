// project
#include "DataAssets/LevelUp/CBLevelUpData.h"

// engine
#include "GameplayEffect.h"

namespace
{
	/**
	 * 남은 카드가 있는 등급 중에서 가중치 비율로 하나를 고르는 함수.
	 * @param InCandidatesByGrade 등급별로 아직 뽑히지 않은 카드 인덱스
	 * @param InGradeWeights 등급별 가중치 (없는 등급·음수는 0)
	 * @return 고른 등급의 남은 카드 목록. 남은 등급이 전부 가중치 0 이면 nullptr
	 */
	TArray<int32>* PickWeightedGrade(TMap<ECBLevelUpCardGrade, TArray<int32>>& InCandidatesByGrade, const TMap<ECBLevelUpCardGrade, float>& InGradeWeights)
	{
		auto GetWeight = [&InGradeWeights](ECBLevelUpCardGrade InGrade)
		{
			const float* Weight = InGradeWeights.Find(InGrade);
			return Weight ? FMath::Max(*Weight, 0.f) : 0.f;
		};

		// 이미 다 뽑힌 등급은 빼고 비율을 다시 나눔
		float TotalWeight = 0.f;
		for (const TPair<ECBLevelUpCardGrade, TArray<int32>>& Pair : InCandidatesByGrade)
		{
			if (!Pair.Value.IsEmpty())
			{
				TotalWeight += GetWeight(Pair.Key);
			}
		}
		if (TotalWeight <= 0.f) return nullptr;

		float Roll = FMath::FRandRange(0.f, TotalWeight);
		TArray<int32>* PickedCandidates = nullptr;
		for (TPair<ECBLevelUpCardGrade, TArray<int32>>& Pair : InCandidatesByGrade)
		{
			const float Weight = Pair.Value.IsEmpty() ? 0.f : GetWeight(Pair.Key);
			if (Weight <= 0.f) continue;

			// 부동소수 오차로 끝까지 가더라도 마지막 후보가 남도록 먼저 기록함
			PickedCandidates = &Pair.Value;
			if (Roll < Weight) break;
			Roll -= Weight;
		}
		return PickedCandidates;
	}
}

int32 UCBLevelUpData::GetRequiredExperience(int32 InLevel) const
{
	// 키가 없는 곡선은 0 을 반환하므로 아래 경고로 이어짐
	const FRichCurve* Curve = RequiredExperienceCurve.GetRichCurveConst();
	const float CurveValue = (Curve && Curve->GetNumKeys() > 0) ? Curve->Eval(static_cast<float>(InLevel)) : 0.f;
	const int32 RequiredExperience = FMath::RoundToInt(CurveValue);

	// 0 이하면 경험치가 0 이어도 레벨업 조건을 만족해 끝없이 반복되므로 1 로 올림
	if (RequiredExperience <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 레벨 %d 의 필요 경험치가 0 이하임. 곡선을 확인할 것 (1 로 처리함)"), *GetName(), InLevel);
		return 1;
	}

	return RequiredExperience;
}

const FCBLevelUpCard* UCBLevelUpData::FindCard(int32 InCardIndex) const
{
	return Cards.IsValidIndex(InCardIndex) ? &Cards[InCardIndex] : nullptr;
}

bool UCBLevelUpData::FindGradeStyle(ECBLevelUpCardGrade InGrade, FCBLevelUpCardGradeStyle& OutStyle) const
{
	if (const FCBLevelUpCardGradeStyle* Style = GradeStyles.Find(InGrade))
	{
		OutStyle = *Style;
		return true;
	}

	// 등록 누락은 카드가 기본 모습으로 조용히 뜨는 것으로만 드러나므로 로그로 알림
	UE_LOG(LogTemp, Warning, TEXT("[%s] 등급 %s 의 카드 겉모습이 등록되지 않음 (GradeStyles 확인)"), *GetName(), *UEnum::GetValueAsString(InGrade));
	return false;
}

void UCBLevelUpData::RollCards(TArray<int32>& OutCardIndices) const
{
	OutCardIndices.Reset();

	// 등급 가중치가 없으면 등급과 무관하게 전체 카드에서 균등하게 뽑음
	if (GradeWeights.IsEmpty())
	{
		// 효과가 지정된 카드만 후보로 둠
		for (int32 CardIndex = 0; CardIndex < Cards.Num(); ++CardIndex)
		{
			if (Cards[CardIndex].Effect)
			{
				OutCardIndices.Add(CardIndex);
			}
		}

		// 앞자리부터 남은 후보 중 하나와 자리를 바꿔 중복 없이 섞음 (필요한 장수만큼만)
		const int32 PickCount = FMath::Min(CardsPerLevelUp, OutCardIndices.Num());
		for (int32 Slot = 0; Slot < PickCount; ++Slot)
		{
			OutCardIndices.Swap(Slot, FMath::RandRange(Slot, OutCardIndices.Num() - 1));
		}

		OutCardIndices.SetNum(PickCount);
		return;
	}

	// 효과가 지정된 카드를 등급별로 모음
	TMap<ECBLevelUpCardGrade, TArray<int32>> CandidatesByGrade;
	int32 CandidateCount = 0;
	for (int32 CardIndex = 0; CardIndex < Cards.Num(); ++CardIndex)
	{
		if (Cards[CardIndex].Effect)
		{
			CandidatesByGrade.FindOrAdd(Cards[CardIndex].Grade).Add(CardIndex);
			++CandidateCount;
		}
	}

	// 카드는 있는데 가중치가 없는 등급은 조용히 안 나오므로 로그로 알림
	for (const TPair<ECBLevelUpCardGrade, TArray<int32>>& Pair : CandidatesByGrade)
	{
		if (!GradeWeights.Contains(Pair.Key))
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] 등급 %s 의 뽑기 가중치가 없어 이 등급 카드는 나오지 않음 (GradeWeights 확인)"), *GetName(), *UEnum::GetValueAsString(Pair.Key));
		}
	}

	// 칸마다 등급을 가중치로 먼저 고르고, 그 등급 안에서 균등하게 한 장 (뽑은 카드는 후보에서 빠져 중복 없음)
	const int32 PickCount = FMath::Min(CardsPerLevelUp, CandidateCount);
	for (int32 Slot = 0; Slot < PickCount; ++Slot)
	{
		TArray<int32>* GradeCandidates = PickWeightedGrade(CandidatesByGrade, GradeWeights);

		// 가중치가 있는 등급의 카드를 다 뽑았으면 그만큼만 제시함
		if (!GradeCandidates) break;

		const int32 PickIndex = FMath::RandRange(0, GradeCandidates->Num() - 1);
		OutCardIndices.Add((*GradeCandidates)[PickIndex]);
		GradeCandidates->RemoveAtSwap(PickIndex);
	}
}
