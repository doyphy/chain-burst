// project
#include "GameModes/CBGameplayGameMode.h"
#include "Core/CBSessionSubsystem.h"

// [서버] 접속 승인. 진행 중인 매치로의 난입을 거부함
void ACBGameplayGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	// 정원 판정 등 엔진의 기본 승인 절차를 먼저 태움
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	// 이미 거부됐으면 그 사유를 덮어쓰지 않음
	if (!ErrorMessage.IsEmpty()) return;

	// 난입을 허용하도록 설정했으면 그대로 받음
	if (bAllowJoinInProgress) return;

	// 진행 중인 매치로의 난입을 거부함.
	// ErrorMessage 에 문자열을 넣으면 다음 단계에서 엔진이 에러를 표시하고 접속을 끊음
	ErrorMessage = UCBSessionSubsystem::MatchInProgressError;

	UE_LOG(LogTemp, Log, TEXT("[Gameplay] 진행 중인 매치로의 접속을 거부함: %s"), *Address);
}
