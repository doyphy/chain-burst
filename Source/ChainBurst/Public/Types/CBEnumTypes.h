#pragma once

/** 무기 전투 타입 */
UENUM(BlueprintType)
enum class ECBWeaponCombatType : uint8
{
	None        UMETA(DisplayName = "None"),
    
	// 한손검, 단검 등 (반대 손이 빎)
	OneHanded   UMETA(DisplayName = "One Handed"),
    
	// 대검, 도끼, 창 등 (묵직함, 느림)
	TwoHanded   UMETA(DisplayName = "Two Handed"),
    
	// 쌍검, 쌍권총 등 (빠름, 연타)
	DualWield   UMETA(DisplayName = "Dual Wield") 
};


/** 비전투 상태일 때 무기를 어디에 부착할 지 결정하는 열거형 */
UENUM(BlueprintType)
enum class ECBWeaponSheathSocket : uint8
{
	// 허리 (한손 무기, 쌍수 등)
	Hip		UMETA(DisplayName = "Hip"),

	// 등 (양손 무기, 대형 무기 등)
	Back	UMETA(DisplayName = "Back"),

	// 숨김 (소환형 무기 등)
	None	UMETA(DisplayName = "None")
};

