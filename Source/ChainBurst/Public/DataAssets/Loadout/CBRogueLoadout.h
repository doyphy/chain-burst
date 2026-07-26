#pragma once

#include "CoreMinimal.h"
#include "DataAssets/Loadout/CBAILoadout.h"
#include "CBRogueLoadout.generated.h"

class UBehaviorTree;

/**
 * Rogue(일반 잡몹) 로드아웃.
 * 단순 전투 두뇌로 비헤이비어 트리를 사용.
 */
UCLASS()
class CHAINBURST_API UCBRogueLoadout : public UCBAILoadout
{
	GENERATED_BODY()

public:
	//~ Begin UCBAILoadout Interface
	virtual UBehaviorTree* GetBehaviorTree() const override { return BehaviorTree; }
	//~ End UCBAILoadout Interface

protected:
	/** 이 AI가 실행할 비헤이비어 트리 (컨트롤러가 준비 완료 후 RunBehaviorTree로 구동) */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|AI")
	TObjectPtr<UBehaviorTree> BehaviorTree = nullptr;
};
