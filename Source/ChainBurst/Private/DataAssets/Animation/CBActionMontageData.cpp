#include "DataAssets/Animation/CBActionMontageData.h"

UAnimMontage* UCBActionMontageData::FindMontage(const FGameplayTag& InTag, int32 InIndex, bool& bOutAffectedByAttackSpeed) const
{
	bOutAffectedByAttackSpeed = false;

	if (!InTag.IsValid()) return nullptr;

	// 태그에 맞는 몽타주 엔트리 찾기
	const FCBActionMontageEntry* FoundEntry = MontageMap.Find(InTag);
	if (!FoundEntry || !FoundEntry->IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CBMontageDataAsset] 몽타주 태그 [%s] 를 찾을 수 없음"), *InTag.ToString());
		return nullptr;
	}

	// 인덱스 범위 검사 (콤보/랜덤 인덱스 모두 이 검사를 거침)
	if (!FoundEntry->Montages.IsValidIndex(InIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CBMontageDataAsset] 태그 [%s] 의 인덱스 [%d] 초과 (최대 %d)"), *InTag.ToString(), InIndex, FoundEntry->Montages.Num() - 1);
		return nullptr;
	}

	// 인덱스에 맞는 몽타주 가져오기
	UAnimMontage* Montage = FoundEntry->Montages[InIndex].Get();
	if (!Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CBMontageDataAsset] 태그 [%s] 인덱스 [%d] 몽타주가 nullptr"), *InTag.ToString(), InIndex);
		return nullptr;
	}

	bOutAffectedByAttackSpeed = FoundEntry->bAffectedByAttackSpeed;
	return Montage;
}

int32 UCBActionMontageData::GetMontageCount(const FGameplayTag& InTag) const
{
	if (!InTag.IsValid()) return 0;

	// 태그에 맞는 몽타주 엔트리 찾기
	const FCBActionMontageEntry* FoundEntry = MontageMap.Find(InTag);
	if (!FoundEntry || !FoundEntry->IsValid()) return 0;

	// 몽타주 개수 반환
	return FoundEntry->Montages.Num();
}
