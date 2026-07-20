#pragma once

#include "CoreMinimal.h"
#include "CBEnumTypes.generated.h"

/**
 * 무기 부착 소켓 타입
 * 무기가 캐릭터의 어느 소켓(주무기/부무기·전투/비전투)에 붙는지를 구분하는 값.
 * CBWeaponSocketData 의 소켓 config 조회 키로만 사용된다(무브셋/카테고리 아님).
 */
UENUM(BlueprintType)
enum class ECBWeaponSocketType : uint8
{
	// 한손검
	Sword		UMETA(DisplayName = "Sword"),

	// 대검
	GreatSword	UMETA(DisplayName = "GreatSword"),

	// 창
	Spear		UMETA(DisplayName = "Spear"),

	// 쌍수 단검 - 왼손(부무기)
	Dagger_L	UMETA(DisplayName = "Dagger (Left)"),

	// 쌍수 단검 - 오른손(주무기)
	Dagger_R	UMETA(DisplayName = "Dagger (Right)"),

	// 숨김 (소환형 무기 등)
	None		UMETA(DisplayName = "None")
};

UENUM(BlueprintType)
enum class ECBSuccessType : uint8
{
	Success,
	Failure,
};

