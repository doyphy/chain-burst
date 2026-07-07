#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/CBCharacterAnimInstance.h"
#include "CBAIAnimInstance.generated.h"

/**
 * AI가 조작하는 캐릭터의 애님 인스턴스 베이스.
 * 포커스/타겟 방향과 AI 상태(순찰·전투·경계)를 기준으로 구동되는 애니메이션 데이터를 담는다.
 * 이동 소스가 PathFollowing이라는 전제의 처리를 여기에 두고, 진영 무관 공통 로직은 UCBCharacterAnimInstance에 둔다.
 */
UCLASS()
class CHAINBURST_API UCBAIAnimInstance : public UCBCharacterAnimInstance
{
	GENERATED_BODY()

};
