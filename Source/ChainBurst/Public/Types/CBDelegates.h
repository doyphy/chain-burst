#pragma once

#include "CoreMinimal.h"
#include "Delegates/DelegateCombinations.h"
#include "GameplayTagContainer.h"
#include "CBDelegates.generated.h" // BP에도 사용할거라 Include

/**
 * 프로젝트 공용 델리게이트 타입 선언.
 * 선언은 타입일 뿐이며, 실제 신호는 이 타입의 인스턴스를 소유한 객체가 가짐.
 */

/** [로컬] 로컬 플레이어의 캐릭터 준비가 끝났음. */
UDELEGATE() // BP에서도 사용 가능하도록 UDELEGATE() 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCBOnLocalPlayerReady);

/** [로컬] 서버 접속 또는 맵 이동에 실패했음. (표시할 사유 텍스트를 전달) */
UDELEGATE() // BP에서도 사용 가능하도록 UDELEGATE() 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCBOnConnectionFailed, const FText&, FailureReason);

/** 로비의 준비 인원이 바뀌었음. (준비 인원 / 전체 인원) */
UDELEGATE() // BP에서도 사용 가능하도록 UDELEGATE() 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCBOnLobbyReadyStateChanged, int32, ReadyCount, int32, TotalCount);

/** 게임이 시작되어 곧 게임플레이 레벨로 이동함. (로비 위젯이 스스로를 정리할 시점) */
UDELEGATE() // BP에서도 사용 가능하도록 UDELEGATE() 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCBOnMatchStarting);

/** 플레이어가 고른 캐릭터(무기)가 바뀌었음. (새로 고른 캐릭터 태그) */
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCBOnCharacterSelectionChanged, FGameplayTag, CharacterId);

/** [로컬] 세션 검색이 끝났음. (성공 여부 / 찾은 개수) */
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCBOnSessionSearchCompleted, bool, bSuccess, int32, ResultCount);

/** [로컬] 세션 작업(생성·검색·참가)이 실패했음. (표시할 사유 텍스트를 전달) */
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCBOnSessionOperationFailed, const FText&, FailureReason);