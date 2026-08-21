// project
#include "Core/CBOnlineSession.h"
#include "Core/CBSessionSubsystem.h"

// engine
#include "Engine/GameInstance.h"

// [로컬] 접속 끊김·접속 실패 시 엔진이 호출함
void UCBOnlineSession::HandleDisconnect(UWorld* InWorld, UNetDriver* InNetDriver)
{
	const UGameInstance* OwningGameInstance = GetTypedOuter<UGameInstance>();
	const UCBSessionSubsystem* SessionSubsystem = OwningGameInstance ? OwningGameInstance->GetSubsystem<UCBSessionSubsystem>() : nullptr;

	// 접속 시도 단계의 실패면 엔진의 기본 맵 복귀를 건너뜀.
	// 막지 않으면 사유를 띄울 위젯이 맵과 함께 파괴됨
	if (SessionSubsystem && SessionSubsystem->Local_ShouldKeepCurrentLevel(InNetDriver))
	{
		UE_LOG(LogTemp, Log, TEXT("[Session] 접속 시도 실패 — 현재 레벨을 유지함 (엔진의 기본 맵 복귀를 건너뜀)"));
		return;
	}

	// 그 외(인게임 접속 끊김·맵 이동 실패)는 엔진 기본 동작에 맡김
	Super::HandleDisconnect(InWorld, InNetDriver);
}
