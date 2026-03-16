#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CBCharacterMovementData.generated.h"

UCLASS()
class CHAINBURST_API UCBCharacterMovementData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	/** 
	 * GameplayTag에 따른 이동 속도 매핑
	 * Key: Shared.Movement.Run -> Value: 550.0f
	 * Key: Shared.Movement.Sprint -> Value: 700.0f
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Speeds")
	TMap<FGameplayTag, float> MovementSpeedMap;
	
public:
	/**
	 * 태그에 해당하는 속도를 반환합니다.
	 * @param InTag 조회할 이동 상태 태그
	 * @return 태그에 해당하는 이동 속도 반환, 없으면 0.0f를 반환합니다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	float GetSpeedForTag(FGameplayTag InTag) const;
};
