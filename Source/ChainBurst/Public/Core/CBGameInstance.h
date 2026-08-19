#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "CBGameInstance.generated.h"

/**
 * 프로젝트 공용 게임 인스턴스 클래스.
 * 맵을 넘어 유지돼야 하는 게임 전역 로직을 여기에 둠.
 */
UCLASS()
class CHAINBURST_API UCBGameInstance : public UGameInstance
{
	GENERATED_BODY()
};
