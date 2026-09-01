#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "CBEnvQueryContext_Target.generated.h"

/**
 * [EQS] 블랙보드의 현재 타겟(TargetActor)을 쿼리의 기준 액터로 제공하는 컨텍스트.
 */
UCLASS()
class CHAINBURST_API UCBEnvQueryContext_Target : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	//~ Begin UEnvQueryContext Interface
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
	//~ End UEnvQueryContext Interface
};
