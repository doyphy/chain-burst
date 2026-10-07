#pragma once

#include "CoreMinimal.h"
#include "DataAssets/Loadout/CBCharacterLoadout.h"
#include "CBAILoadout.generated.h"

class UBehaviorTree;
class AController;

/**
 * AI 캐릭터 공통 로드아웃 베이스 (Outlaw·Rogue 등).
 * AI 전용 데이터(두뇌 비헤이비어 트리, 처치 경험치 보상 등)를 담음. 플레이어(Chaser)는 이 계층을 쓰지 않음.
 * 직접 인스턴스화하지 않는 추상 클래스.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBAILoadout : public UCBCharacterLoadout
{
	GENERATED_BODY()

public:
	/**
	 * [서버 전용] 이 로드아웃의 비헤이비어 트리를 AI 컨트롤러에 주입.
	 * AController를 받아 내부에서 ACBAIController로 캐스팅하므로, 캐릭터는 자기 컨트롤러를 넘겨 호출만 하면 됨.
	 * @param InController 주입 대상 컨트롤러 (AI 컨트롤러가 아니면 무시)
	 */
	void Auth_ApplyBehaviorTreeToController(AController* InController) const;

	/** [Getter] 이 AI를 처치했을 때 전 플레이어가 함께 얻는 경험치 */
	FORCEINLINE int32 GetExperienceReward() const { return ExperienceReward; }

protected:
	/** 이 AI가 실행할 비헤이비어 트리 (컨트롤러가 준비 완료 후 RunBehaviorTree로 구동). 비우면 두뇌 없이 제자리에 섬. */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|AI")
	TObjectPtr<UBehaviorTree> BehaviorTree = nullptr;

	/** 이 AI를 처치했을 때 전 플레이어가 함께 얻는 경험치. 사망 시 게임모드에 전달됨 (0 = 보상 없음) */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|AI", meta = (ClampMin = "0"))
	int32 ExperienceReward = 0;
};
