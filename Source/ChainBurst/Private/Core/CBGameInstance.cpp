// project
#include "Core/CBGameInstance.h"
#include "Core/CBOnlineSession.h"
#include "Types/CBEnumTypes.h"
#include "Core/CBAuthSubsystem.h"

// engine
#include "GenericTeamAgentInterface.h"

namespace
{
	/**
	 * ChainBurst 전용 진영 판정 규칙.
	 * 엔진 기본 solver 는 "다르면 적, 같으면 아군"뿐이라 중립 개념이 없기에 새로 규칙을 만들어서 적용함.
	 * 시각은 인터페이스를, 청각은 팀 ID 를 직접 비교하는 서로 다른 경로를 타므로 커스텀 해야함.
	 */
	ETeamAttitude::Type CBTeamAttitudeSolver(FGenericTeamId TeamA, FGenericTeamId TeamB)
	{
		// 한쪽이라도 중립이면 서로 무관심 (중립은 누구의 적도 아니다)
		if (GenericIdToCBTeam(TeamA) == ECBTeam::Neutral || GenericIdToCBTeam(TeamB) == ECBTeam::Neutral)
		{
			return ETeamAttitude::Neutral;
		}

		return TeamA == TeamB ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
	}
}

// 게임 인스턴스 초기화. 진영 판정 규칙을 엔진에 등록.
void UCBGameInstance::Init()
{
	Super::Init();

	// 새로 만든 규칙을 등록. 이후 모든 FGenericTeamId::GetAttitude 호출이 이 규칙을 따름.
	FGenericTeamId::SetAttitudeSolver(&CBTeamAttitudeSolver);
}

// 게임 인스턴스 종료. 등록한 규칙을 엔진 기본값으로 되돌림 (에디터 PIE 반복 대비).
void UCBGameInstance::Shutdown()
{
	FGenericTeamId::ResetAttitudeSolver();

	Super::Shutdown();
}

void UCBGameInstance::OnStart()
{
	Super::OnStart();
	
	// 로컬 플레이어가 만들어진 뒤라 이 시점에 로그인할 수 있음
	if (UCBAuthSubsystem* AuthSubsystem = GetSubsystem<UCBAuthSubsystem>())
	{
		AuthSubsystem->RequestLogin();
	}
}

// UCBGameInstance 에서 사용할 온라인 세션 클래스 지정
TSubclassOf<UOnlineSession> UCBGameInstance::GetOnlineSessionClass()
{
	// UCBOnlineSession 클래스 사용
	return UCBOnlineSession::StaticClass();
}
