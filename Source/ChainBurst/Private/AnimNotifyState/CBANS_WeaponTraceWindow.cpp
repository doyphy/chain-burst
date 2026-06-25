// project
#include "AnimNotifyState/CBANS_WeaponTraceWindow.h"

// engine
#include "AbilitySystemBlueprintLibrary.h"


void UCBANS_WeaponTraceWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                           float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	// 오너 가져오기
	APawn* Owner = Cast<APawn>(MeshComp->GetOwner());

	// 직접 조종하고 있는 캐릭터인지 확인 (트레이스 처리는 직접 조종하는 캐릭터에서만 하도록)
	if (!Owner || !Owner->IsLocallyControlled()) return;

	FGameplayEventData EventData;
	
	// 이벤트 전송
	SendEvent(Owner, TraceStartTag, EventData);
}

void UCBANS_WeaponTraceWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	// 오너 가져오기
	APawn* Owner = Cast<APawn>(MeshComp->GetOwner());

	// 직접 조종하고 있는 캐릭터인지 확인 (트레이스 처리는 직접 조종하는 캐릭터에서만 하도록)
	if (!Owner || !Owner->IsLocallyControlled()) return;

	FGameplayEventData EventData;
	
	// 이벤트 전송
	SendEvent(Owner, TraceEndTag, EventData);
}

void UCBANS_WeaponTraceWindow::SendEvent(APawn* Owner, FGameplayTag EventTag, FGameplayEventData Payload)
{
	// 로컬 이벤트 전송
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, Payload);
}
