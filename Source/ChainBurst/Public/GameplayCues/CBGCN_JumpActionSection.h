#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "CBGCN_JumpActionSection.generated.h"

/**
 * 액션 몽타주 섹션 점프 GameplayCue (GameplayCue.JumpActionSection)
 * 파라미터: SourceObject = 점프할 몽타주, RawMagnitude = 섹션 인덱스
 */
UCLASS()
class CHAINBURST_API UCBGCN_JumpActionSection : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
};
