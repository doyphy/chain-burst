// project
#include "DataAssets/Loadout/CBAILoadout.h"
#include "Controllers/CBAIController.h"

// [서버 전용] 이 로드아웃의 BT를 AI 컨트롤러에 주입.
void UCBAILoadout::Auth_ApplyBehaviorTreeToController(AController* InController) const
{
	// AI 컨트롤러가 아니면 스킵 (플레이어 컨트롤러 등)
	ACBAIController* AIController = Cast<ACBAIController>(InController);
	if (!AIController) return;

	// 이 로드아웃의 BT를 컨트롤러에 주입 (null이면 BT 두뇌 없음)
	AIController->SetBehaviorTree(GetBehaviorTree());
}
