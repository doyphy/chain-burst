#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CBGameModeBase.generated.h"

/**
 * 프로젝트 공용 게임모드 베이스.
 * 파생 게임모드가 공통으로 쓸 컨트롤러·플레이어 상태 클래스와 맵 전환 방식을 고정함.
 * 레벨별 규칙은 파생 클래스(로비·게임플레이)가 담당함.
 */
UCLASS()
class CHAINBURST_API ACBGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** 파생 게임모드가 공통으로 물려받을 클래스·맵 전환 기본값을 설정함 */
	ACBGameModeBase();
};
