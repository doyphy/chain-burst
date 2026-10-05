#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "CBANS_GameplayEventWindow.generated.h"

struct FGameplayEventData;

/**
 * 몽타주 구간의 시작·끝에 게임플레이 이벤트를 보내는 노티파이 스테이트.
 * 구간 시작에 StartEventTag, 끝에 EndEventTag 를 오너 폰에게 보냄. 어빌리티·기능 조각이 이 이벤트로 구간 동작을 켜고 끔.
 * 직접 조종 중인 폰(IsLocallyControlled)에서만 보냄 — 플레이어는 소유 클라, AI 는 서버.
 * 기본 태그는 무기 트레이스 구간(Event.Combat.TraceStart/End). 다른 용도는 몽타주에서 태그만 바꿔 씀.
 * (예전 이름 CBANS_WeaponTraceWindow. 기존 몽타주의 노티파이 인스턴스 이름에는 옛 이름이 남아 있으나 동작과 무관)
 */
UCLASS(meta = (DisplayName = "Gameplay Event Window"))
class CHAINBURST_API UCBANS_GameplayEventWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

private:
	void SendEvent(APawn* Owner, FGameplayTag EventTag, FGameplayEventData Payload);

	/** 구간 시작 이벤트 태그 (기본: 무기 트레이스 시작) */
	UPROPERTY(EditAnywhere, Category = "Event")
	FGameplayTag StartEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Combat.TraceStart"));

	/** 구간 종료 이벤트 태그 (기본: 무기 트레이스 종료) */
	UPROPERTY(EditAnywhere, Category = "Event")
	FGameplayTag EndEventTag = FGameplayTag::RequestGameplayTag(FName("Event.Combat.TraceEnd"));
};
