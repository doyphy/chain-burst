// project
#include "DataAssets/Cosmetic/CBCosmeticCatalog.h"

bool FCBCosmeticPart::IsValid() const
{
	// 태그가 없으면 조회할 키가 없고, 메시가 없으면 입힐 것이 없음
	return PartId.IsValid() && !Mesh.IsNull();
}

const FCBCosmeticPart* UCBCosmeticCatalog::FindPart(ECBCosmeticSlot InSlot, const FGameplayTag& InPartId) const
{
	if (!InPartId.IsValid()) return nullptr;

	// 슬롯에 맞는 파츠 목록 가져오기
	const FCBCosmeticPartList* FoundList = PartsBySlot.Find(InSlot);
	if (!FoundList) return nullptr;

	// 태그가 일치하는 파츠를 찾아 반환. 없으면 nullptr
	return FoundList->Parts.FindByPredicate(
		[&InPartId](const FCBCosmeticPart& InPart) { return InPart.PartId == InPartId; });
}

void UCBCosmeticCatalog::GetPartIdsForSlot(ECBCosmeticSlot InSlot, TArray<FGameplayTag>& OutPartIds) const
{
	OutPartIds.Reset();

	const FCBCosmeticPartList* FoundList = PartsBySlot.Find(InSlot);
	if (!FoundList) return;

	for (const FCBCosmeticPart& Part : FoundList->Parts)
	{
		if (!Part.IsValid()) continue;

		OutPartIds.Add(Part.PartId);
	}
}

bool UCBCosmeticCatalog::IsValidPartForSlot(ECBCosmeticSlot InSlot, const FGameplayTag& InPartId) const
{
	const FCBCosmeticPart* FoundPart = FindPart(InSlot, InPartId);

	// 그 부위에 등록돼 있고 설정이 온전해야 통과
	return FoundPart && FoundPart->IsValid();
}
