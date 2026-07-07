#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/CBCharacterAnimInstance.h"
#include "CBPlayerAnimInstance.generated.h"

/**
 * 플레이어가 조작하는 캐릭터의 애님 인스턴스 베이스.
 * 컨트롤 회전/카메라·로컬 입력에만 의미 있는 로직(향후 에임오프셋/스트레이핑 lean 등)을 담는다.
 * 진영 무관 공통 로직은 UCBCharacterAnimInstance에 둔다.
 */
UCLASS()
class CHAINBURST_API UCBPlayerAnimInstance : public UCBCharacterAnimInstance
{
	GENERATED_BODY()

};
