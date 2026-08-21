#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "CBGameInstance.generated.h"

/**
 * 프로젝트 공용 게임 인스턴스 클래스.
 * 맵을 넘어 유지돼야 하는 게임 전역 로직을 여기에 둠.
 * 접속 실패 시 엔진의 자동 기본 맵 복귀를 통제하기 위해 온라인 세션 클래스를 갈아끼움.
 */
UCLASS()
class CHAINBURST_API UCBGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/** UCBGameInstance 에서 사용할 온라인 세션 클래스를 지정함. */
	virtual TSubclassOf<UOnlineSession> GetOnlineSessionClass() override;
};
