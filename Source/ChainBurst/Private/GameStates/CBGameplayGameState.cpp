// project
#include "GameStates/CBGameplayGameState.h"
#include "Core/CBGameInstance.h"
#include "DataAssets/LevelUp/CBLevelUpData.h"

// engine
#include "Net/UnrealNetwork.h"

void ACBGameplayGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ACBGameplayGameState, Level);
	DOREPLIFETIME(ACBGameplayGameState, Experience);
}

// [서버] 게임모드가 계산한 결과를 반영 (서버에서 실행)
void ACBGameplayGameState::Auth_SetExperience(int32 InLevel, int32 InExperience)
{
	Level = InLevel;
	Experience = InExperience;

	// 레벨업 직후 월드가 정지되면 서버 시간이 멈춰 평소 주기로는 복제가 나가지 않음. 다음 네트 틱에 강제 송출
	ForceNetUpdate();

	// 서버에서는 OnRep 이 불리지 않으므로 직접 호출해 호스트 위젯에도 반영
	OnRep_Experience();
}

// [서버/클라이언트] 다음 레벨까지 필요한 경험치. 서버 판정과 경험치 바가 같은 값을 쓰도록 계산을 여기 한 곳에 둠
int32 ACBGameplayGameState::GetRequiredExperience(int32 InLevel) const
{
	const UCBGameInstance* CBGameInstance = GetGameInstance<UCBGameInstance>();
	const UCBLevelUpData* LevelUpData = CBGameInstance ? CBGameInstance->GetLevelUpData() : nullptr;
	if (!LevelUpData) return 0;

	// 인원이 늘면 처치 속도도 그만큼 빨라지므로 필요량도 인원만큼 늘림.
	// PlayerState 가 아직 복제되지 않은 순간에도 필요량이 0 이 되지 않게 최소 1명
	const int32 PlayerCount = FMath::Max(PlayerArray.Num(), 1);
	return LevelUpData->GetRequiredExperience(InLevel) * PlayerCount;
}

// [서버/클라이언트] 레벨·경험치 변경 신호 방송
void ACBGameplayGameState::OnRep_Experience()
{
	OnExperienceChanged.Broadcast(Level, Experience);
}
