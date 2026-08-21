#pragma once

#include "CoreMinimal.h"
#include "Types/CBEnumTypes.h"
#include "CBStructTypes.generated.h"

/**
 * 무기 타입별 소켓 부착 정보 구조체
 * CBWeaponSocketData 에서 소켓 타입별로 소켓 이름을 직접 입력
 */
USTRUCT(BlueprintType)
struct FCBWeaponSocketConfig
{
	GENERATED_BODY()

	/** 이 설정이 적용될 무기 소켓 타입 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	ECBWeaponSocketType WeaponSocketType = ECBWeaponSocketType::None;

	/** 전투 시 부착 소켓 이름 (예: Socket_Combat_Hand_R) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName CombatSocket = NAME_None;

	/** 비전투 시 부착 소켓 이름 (예: Socket_Sheath_Hip_L) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName SheathSocket = NAME_None;

	bool IsValid() const
	{
		return WeaponSocketType != ECBWeaponSocketType::None
			&& CombatSocket != NAME_None;
	}
};

/**
 * 세션 검색 결과 한 줄 (목록 위젯 표시용).
 * 온라인 서비스의 세션 객체는 BP 로 넘길 수 없으므로, 표시에 필요한 값만 복사해 담는다.
 * 참가는 이 배열의 인덱스로 요청한다. (UCBSessionSubsystem::Local_JoinFoundSession)
 */
USTRUCT(BlueprintType)
struct FCBSessionSearchEntry
{
	GENERATED_BODY()

	/** 방 이름 (호스트가 세션 속성으로 실어 보낸 값. 없으면 대체 문구) */
	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	/** 현재 인원 */
	UPROPERTY(BlueprintReadOnly)
	int32 CurrentPlayers = 0;

	/** 최대 인원 */
	UPROPERTY(BlueprintReadOnly)
	int32 MaxPlayers = 0;

	/** 참가 가능한지 (정원 초과·비공개 등이면 false) */
	UPROPERTY(BlueprintReadOnly)
	bool bIsJoinable = false;
};
