#include "DataAssets/Movement/CBCharacterMovementData.h"

float UCBCharacterMovementData::GetSpeedForTag(FGameplayTag InTag) const
{
	// 맵에 해당 태그가 있는지 확인
	if (const float* FoundSpeed = MovementSpeedMap.Find(InTag))
	{
		return *FoundSpeed;
	}

	// 태그가 없으면 0.0f 반환
	return 0.0f;
}
