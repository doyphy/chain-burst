#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
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

/**
 * 캐릭터 치장(의상) 부위 슬롯
 * 모듈러 메시의 의상 파츠를 부위별로 식별하는 키.
 */
UENUM(BlueprintType)
enum class ECBCosmeticSlot : uint8
{
	// 머리 (헬멧·모자 등)
	Helmet	= 0		UMETA(DisplayName = "Helmet"),

	// 상의
	Torso	= 1		UMETA(DisplayName = "Torso"),

	// 하의
	Legs	= 2		UMETA(DisplayName = "Legs"),

	// 신발
	Feet	= 3		UMETA(DisplayName = "Feet"),

	// 슬롯 개수 (배열 크기 산정용, 선택 값 아님). 값을 적지 않아 마지막 슬롯 다음 값이 자동으로 들어감.
	MAX				UMETA(Hidden)
};

/** 캐릭터 진영(팀) */
UENUM(BlueprintType)
enum class ECBTeam : uint8
{
	// 추격자 (플레이어)
	Chaser	= 0		UMETA(DisplayName = "Chaser"),

	// 무법자 (AI 적. Outlaw·Rogue 모두 같은 팀)
	Outlaw	= 1		UMETA(DisplayName = "Outlaw"),

	// 중립 (누구와도 싸우지 않음. 팀 미지정의 기본값)
	Neutral	= 255	UMETA(DisplayName = "Neutral")
};

/**
 * ECBTeam 을 엔진 팀 ID 로 변환하는 함수.
 * Neutral 은 엔진의 FGenericTeamId::NoTeam(255) 과 값이 같아 그대로 대응됨.
 * NoTeam 을 중립으로 해석하는 것은 엔진 기본 solver 가 아니라 UCBGameInstance 가 등록하는 커스텀 solver 의 역할.
 */
FORCEINLINE FGenericTeamId CBTeamToGenericId(ECBTeam InTeam)
{
	return FGenericTeamId(static_cast<uint8>(InTeam));
}

/** 엔진 팀 ID 를 ECBTeam 으로 변환하는 함수. 정의된 값이 아니면 Neutral로 반환. */
FORCEINLINE ECBTeam GenericIdToCBTeam(const FGenericTeamId& InTeamId)
{
	const uint8 RawId = InTeamId.GetId();
	switch (RawId)
	{
	case static_cast<uint8>(ECBTeam::Chaser):	return ECBTeam::Chaser;
	case static_cast<uint8>(ECBTeam::Outlaw):	return ECBTeam::Outlaw;
	default:									return ECBTeam::Neutral;
	}
}