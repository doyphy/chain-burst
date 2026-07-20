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