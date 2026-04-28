#pragma once

#include "CoreMinimal.h"
#include "CBEnumTypes.generated.h"

/** 무기 전투 타입 */
UENUM(BlueprintType)
enum class ECBWeaponCombatType : uint8
{
	// 한손검
	Sword		UMETA(DisplayName = "Sword"),

	// 대검
	GreatSword	UMETA(DisplayName = "GreatSword"),

	// 창
	Spear		UMETA(DisplayName = "Spear"),

	// 단검
	Dagger		UMETA(DisplayName = "Dagger"),
	
	// 숨김 (소환형 무기 등)
	None		UMETA(DisplayName = "None")
};

UENUM(BlueprintType)
enum class ECBSuccessType : uint8
{
	Success,
	Failure,
};

