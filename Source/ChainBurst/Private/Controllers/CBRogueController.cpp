// project
#include "Controllers/CBRogueController.h"

// engine
#include "BehaviorTree/BehaviorTree.h"

// AI 두뇌 시작: 로드아웃이 주입한 BT를 실행. BT가 없으면 안전하게 스킵.
void ACBRogueController::StartAILogic()
{
	// 베이스가 위협 판정용 피격 이벤트를 구독
	Super::StartAILogic();

	// RunBehaviorTree가 BT에 지정된 Blackboard를 자동 세팅한다 (별도 BB 참조 불필요).
	if (BehaviorTree)
	{
		RunBehaviorTree(BehaviorTree);

		// BT 시작으로 블랙보드가 준비된 뒤 1회 선정 (이미 시야에 있던 정지 타겟 놓침 방지)
		UpdateTarget();
	}
}
