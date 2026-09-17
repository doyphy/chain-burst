#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

/**
 * 프로젝트 커스텀 콜리전 채널 별칭.
 *
 * 채널의 실제 이름과 기본 응답은 Config/DefaultEngine.ini 의 [/Script/Engine.CollisionProfile] 이 소유하고,
 * 여기서는 ECC_GameTraceChannelN 인덱스가 코드 곳곳에 흩어지지 않게 이름만 붙임.
 *
 * 주의: 채널은 이름이 아니라 인덱스로 에셋에 저장됨. ini 에서 순서를 바꾸거나 지우면
 * 저장된 콜리전 설정이 다른 채널로 밀리므로, 추가만 하고 제거·재배치는 하지 말 것.
 */
namespace CBCollisionChannels
{
	/**
	 * 무기 히트 판정 전용 트레이스 채널.
	 * 캐릭터 기준 - 캡슐=Ignore(판정 제외), 메시=Overlap(피직스 애셋 실루엣으로 판정).
	 */
	inline constexpr ECollisionChannel Weapon = ECC_GameTraceChannel1;
}
