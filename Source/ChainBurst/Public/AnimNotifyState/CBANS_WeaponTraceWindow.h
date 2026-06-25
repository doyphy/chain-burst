#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "CBANS_WeaponTraceWindow.generated.h"

struct FGameplayEventData;

UCLASS()
class CHAINBURST_API UCBANS_WeaponTraceWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

private:
	void SendEvent(APawn* Owner, FGameplayTag EventTag, FGameplayEventData Payload);

	/** 트레이스 시작 태그 */
	UPROPERTY(EditAnywhere, Category = "WeaponTrace")
	FGameplayTag TraceStartTag = FGameplayTag::RequestGameplayTag(FName("Event.Combat.TraceStart"));

	/** 트레이스 종료 태그 */
	UPROPERTY(EditAnywhere, Category = "WeaponTrace")
	FGameplayTag TraceEndTag = FGameplayTag::RequestGameplayTag(FName("Event.Combat.TraceEnd"));
};
