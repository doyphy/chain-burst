// project
#include "GameModes/CBGameModeBase.h"
#include "Controllers/CBChaserController.h"
#include "PlayerState/CBPlayerState.h"

ACBGameModeBase::ACBGameModeBase()
{
	// 맵을 넘어갈 때 PlayerState 이관 경로(CopyProperties)를 타려면 필수.
	// 꺼져 있으면 로비에서 고른 값이 에러 없이 조용히 사라짐.
	bUseSeamlessTravel = true;

	// BP 마다 지정하면 누락 위험이 있으므로 베이스에서 고정함
	PlayerControllerClass = ACBChaserController::StaticClass();
	PlayerStateClass = ACBPlayerState::StaticClass();
}
