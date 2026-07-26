#pragma once

#include "CoreMinimal.h"
#include "Controllers/CBAIController.h"
#include "CBRogueController.generated.h"

/**
 * [AI] Rogue(일반 잡몹) 전용 컨트롤러.
 * 단순한 전투 두뇌를 비헤이비어 트리로 구동한다.
 * 실행할 BT는 캐릭터가 로드아웃에서 캐싱한 것을 조회해 사용한다.
 */
UCLASS()
class CHAINBURST_API ACBRogueController : public ACBAIController
{
	GENERATED_BODY()

protected:
	//~ Begin ACBAIController Interface
	virtual void StartAILogic() override;
	//~ End ACBAIController Interface
};
