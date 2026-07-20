#pragma once

#include "CoreMinimal.h"
#include "Items/Weapons/CBBaseWeapon.h"
#include "CBRogueWeapon.generated.h"

/**
 * [AI] 로그(일반 잡몹) 전용 무기.
 * 현재는 ACBBaseWeapon 의 공통 기능을 그대로 사용하며, 로그 전용 로직이 필요해지면 여기에 추가한다.
 */
UCLASS()
class CHAINBURST_API ACBRogueWeapon : public ACBBaseWeapon
{
	GENERATED_BODY()

};
