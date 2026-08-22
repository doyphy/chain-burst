#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "CBGCN_StopAction.generated.h"

/** 액션 몽타주 정지 GameplayCue (GameplayCue.StopAction) */
UCLASS()
class CHAINBURST_API UCBGCN_StopAction : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
