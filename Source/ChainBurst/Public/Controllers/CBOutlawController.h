#pragma once

#include "CoreMinimal.h"
#include "Controllers/CBAIController.h"
#include "CBOutlawController.generated.h"

/**
 * [AI] Outlaw(보스/엘리트) 전용 컨트롤러.
 * 복잡한 전투 두뇌를 담당. 현재는 베이스의 SystemReady 게이트만 사용하며,
 * 두뇌 방식(BT/StateTree) 결정 후 StartAILogic()을 오버라이드해 구동한다.
 */
UCLASS()
class CHAINBURST_API ACBOutlawController : public ACBAIController
{
	GENERATED_BODY()
};
