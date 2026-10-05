#pragma once

#include "CoreMinimal.h"
#include "Controllers/CBAIController.h"
#include "CBRogueController.generated.h"

/**
 * [AI] Rogue(일반 잡몹) 전용 컨트롤러.
 * 단순한 전투 두뇌를 비헤이비어 트리로 구동함. BT 구동은 베이스가 담당하며,
 * 잡몹 전용 판정(타겟 점수·전환 조건 등)이 필요해지면 여기서 오버라이드.
 */
UCLASS()
class CHAINBURST_API ACBRogueController : public ACBAIController
{
	GENERATED_BODY()
};
