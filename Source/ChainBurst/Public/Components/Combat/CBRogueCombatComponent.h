#pragma once

#include "CoreMinimal.h"
#include "Components/Combat/CBCombatComponent.h"
#include "CBRogueCombatComponent.generated.h"

/**
 * [AI] 로그(일반 잡몹) 전용 전투 컴포넌트.
 * 현재는 UCBCombatComponent의 공통 전투 기능을 그대로 사용하며, 로그 전용 로직이 필요해지면 여기에 추가한다.
 */
UCLASS()
class CHAINBURST_API UCBRogueCombatComponent : public UCBCombatComponent
{
	GENERATED_BODY()
};
