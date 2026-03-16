#pragma once

#include "CoreMinimal.h"
#include "CharacterTrajectoryComponent.h"
#include "CBCharacterTrajectoryComponent.generated.h"

UCLASS()
class CHAINBURST_API UCBCharacterTrajectoryComponent : public UCharacterTrajectoryComponent
{
	GENERATED_BODY()

public:
	FORCEINLINE const FPoseSearchQueryTrajectory& GetTrajectory() const { return Trajectory; }
};
