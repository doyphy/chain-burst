#include "DataAssets/Animation/CBActionMontageData.h"

UAnimMontage* UCBActionMontageData::FindSingleMontage(const FGameplayTag& InTag, bool& bOutAffectedByAttackSpeed) const
{
	bOutAffectedByAttackSpeed = false;

	if (!InTag.IsValid()) return nullptr;

	// 태그에 맞는 단일 몽타주 찾기
	const FCBSingleMontageData* FoundMontageData = SingleMontageMap.Find(InTag);

	if (!FoundMontageData || !FoundMontageData->IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CBMontageDataAsset] 단일 몽타주 태그 [%s] 를 찾을 수 없음"), *InTag.ToString());
		return nullptr;
	}

	bOutAffectedByAttackSpeed = FoundMontageData->bAffectedByAttackSpeed;
	return FoundMontageData->Montage.Get();
}

UAnimMontage* UCBActionMontageData::FindComboMontage(const FGameplayTag& InTag, int32 InComboIndex, bool& bOutAffectedByAttackSpeed) const
{
	bOutAffectedByAttackSpeed = false;

	if (!InTag.IsValid()) return nullptr;

	// 태그에 맞는 콤보 몽타주 찾기
	const FCBComboMontageData* FoundCombo = ComboMontageMap.Find(InTag);
	if (!FoundCombo || !FoundCombo->IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CBMontageDataAsset] 콤보 몽타주 태그 [%s] 를 찾을 수 없음"), *InTag.ToString());
		return nullptr;
	}

	// 인덱스 범위 초과 검사
	if (!FoundCombo->Montages.IsValidIndex(InComboIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CBMontageDataAsset] 콤보 태그 [%s] 의 인덱스 [%d] 초과 (최대 %d)"), *InTag.ToString(), InComboIndex, FoundCombo->Montages.Num() - 1);
		return nullptr;
	}

	// 콤보에 맞는 몽타주 가져오기
	UAnimMontage* Montage = FoundCombo->Montages[InComboIndex].Get();
	if (!Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CBMontageDataAsset] 콤보 태그 [%s] 인덱스 [%d] 몽타주가 nullptr"), *InTag.ToString(), InComboIndex);
		return nullptr;
	}

	bOutAffectedByAttackSpeed = FoundCombo->bAffectedByAttackSpeed;
	return Montage;
}

int32 UCBActionMontageData::GetComboCount(const FGameplayTag& InTag) const
{
	if (!InTag.IsValid()) return 0;

	// 태그에 맞는 콤보 몽타주 찾기
	const FCBComboMontageData* FoundCombo = ComboMontageMap.Find(InTag);
	if (!FoundCombo || !FoundCombo->IsValid()) return 0;

	// 콤보 몽타주 개수 반환
	return FoundCombo->Montages.Num();
}
