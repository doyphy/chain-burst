// project
#include "Controllers/CBRogueController.h"

// engine
#include "BehaviorTree/BehaviorTree.h"

// AI 두뇌 시작: 로드아웃이 주입한 BT를 실행. BT가 없으면 안전하게 스킵.
void ACBRogueController::StartAILogic()
{
	// RunBehaviorTree가 BT에 지정된 Blackboard를 자동 세팅한다 (별도 BB 참조 불필요).
	if (BehaviorTree)
	{
		RunBehaviorTree(BehaviorTree);

		// BT 시작으로 블랙보드가 준비된 뒤, 이미 시야에 있던 타겟을 시드 (정지 플레이어 놓침 방지)
		SeedTargetFromCurrentPerception();
	}
}
