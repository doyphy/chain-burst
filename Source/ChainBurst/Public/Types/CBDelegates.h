#pragma once

#include "CoreMinimal.h"
#include "Delegates/DelegateCombinations.h"
#include "CBDelegates.generated.h" // BP에도 사용할거라 Include

/**
 * 프로젝트 공용 델리게이트 타입 선언.
 * 선언은 타입일 뿐이며, 실제 신호는 이 타입의 인스턴스를 소유한 객체가 가짐.
 */

/** [로컬] 로컬 플레이어의 캐릭터 준비가 끝났음. */
UDELEGATE() // BP에서도 사용 가능하도록 UDELEGATE() 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCBOnLocalPlayerReady);
