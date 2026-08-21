// project
#include "Core/CBGameInstance.h"
#include "Core/CBOnlineSession.h"

// UCBGameInstance 에서 사용할 온라인 세션 클래스 지정
TSubclassOf<UOnlineSession> UCBGameInstance::GetOnlineSessionClass()
{
	// UCBOnlineSession 클래스 사용
	return UCBOnlineSession::StaticClass();
}
