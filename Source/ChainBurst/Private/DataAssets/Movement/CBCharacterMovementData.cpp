#include "DataAssets/Movement/CBCharacterMovementData.h"

float UCBCharacterMovementData::GetSpeedForTag(FGameplayTag InTag) const
{
	// 맵에 해당 태그가 있는지 확인
	if (const FCBGaitMovementData* FoundData = FindGaitData(InTag))
	{
		return FoundData->MaxSpeed;
	}

	// 태그가 없으면 0.0f 반환
	return 0.0f;
}

float UCBCharacterMovementData::GetRotationInterpSpeedForTag(FGameplayTag InTag) const
{
	// 맵에 해당 태그가 있는지 확인
	if (const FCBGaitMovementData* FoundData = FindGaitData(InTag))
	{
		return FoundData->RotationInterpSpeed;
	}

	// 태그가 없으면 0.0f 반환 (호출부에서 폴백 처리 (예외 처리))
	return 0.0f;
}

const FCBGaitMovementData* UCBCharacterMovementData::FindGaitData(FGameplayTag InTag) const
{
	return MovementDataMap.Find(InTag);
}
