// project
#include "Core/CBLocalReadySubsystem.h"

// [로컬] 준비 완료를 알림. 이미 알렸으면 무시 (중복 삽입 등 부작용 방지)
void UCBLocalReadySubsystem::NotifyLocalPlayerReady()
{
	if (bLocalPlayerReady) return;

	bLocalPlayerReady = true;

	OnLocalPlayerReady.Broadcast();
}
