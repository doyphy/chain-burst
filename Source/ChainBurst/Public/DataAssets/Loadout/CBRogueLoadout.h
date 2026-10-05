#pragma once

#include "CoreMinimal.h"
#include "DataAssets/Loadout/CBAILoadout.h"
#include "CBRogueLoadout.generated.h"

/**
 * Rogue(일반 잡몹) 로드아웃.
 * 현재는 AI 공통 로드아웃(비헤이비어 트리 포함)을 그대로 사용하며, 잡몹 전용 데이터가 생기면 여기에 추가.
 */
UCLASS()
class CHAINBURST_API UCBRogueLoadout : public UCBAILoadout
{
	GENERATED_BODY()
};
