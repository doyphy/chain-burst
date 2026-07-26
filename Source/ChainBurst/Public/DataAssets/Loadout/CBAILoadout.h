#pragma once

#include "CoreMinimal.h"
#include "DataAssets/Loadout/CBCharacterLoadout.h"
#include "CBAILoadout.generated.h"

class UBehaviorTree;
class AController;

/**
 * AI 캐릭터 공통 로드아웃 베이스 (Outlaw·Rogue 등).
 * AI 전용 데이터(두뇌 에셋 등)를 담는다. 플레이어(Chaser)는 이 계층을 쓰지 않음.
 * 직접 인스턴스화하지 않는 추상 클래스.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBAILoadout : public UCBCharacterLoadout
{
	GENERATED_BODY()

public:
	/**
	 * 이 AI가 실행할 비헤이비어 트리를 반환.
	 * 베이스는 nullptr(= BT 두뇌 없음)을 반환하며, BT를 쓰는 자식이 오버라이드.
	 * (StateTree 등 다른 두뇌를 쓰는 AI는 이를 오버라이드하지 않고 자체 접근자를 둔다)
	 */
	virtual UBehaviorTree* GetBehaviorTree() const { return nullptr; }

	/**
	 * [서버 전용] 이 로드아웃의 비헤이비어 트리를 AI 컨트롤러에 주입.
	 * AController를 받아 내부에서 ACBAIController로 캐스팅하므로, 캐릭터는 자기 컨트롤러를 넘겨 호출만 하면 됨.
	 * @param InController 주입 대상 컨트롤러 (AI 컨트롤러가 아니면 무시)
	 */
	void Auth_ApplyBehaviorTreeToController(AController* InController) const;
};
