// project
#include "AnimNotifyState/CBANS_GameplayEventWindow.h"

// engine
#include "AbilitySystemBlueprintLibrary.h"


void UCBANS_GameplayEventWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                             float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	// 오너 가져오기
	APawn* Owner = Cast<APawn>(MeshComp->GetOwner());

	// 직접 조종하고 있는 캐릭터인지 확인 (구간 동작은 직접 조종하는 캐릭터에서만 하도록)
	if (!Owner || !Owner->IsLocallyControlled()) return;

	FGameplayEventData EventData;

	// 이벤트 전송
	SendEvent(Owner, StartEventTag, EventData);
}

void UCBANS_GameplayEventWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	// 오너 가져오기
	APawn* Owner = Cast<APawn>(MeshComp->GetOwner());

	// 직접 조종하고 있는 캐릭터인지 확인 (구간 동작은 직접 조종하는 캐릭터에서만 하도록)
	if (!Owner || !Owner->IsLocallyControlled()) return;

	FGameplayEventData EventData;

	// 이벤트 전송
	SendEvent(Owner, EndEventTag, EventData);
}

void UCBANS_GameplayEventWindow::SendEvent(APawn* Owner, FGameplayTag EventTag, FGameplayEventData Payload)
{
	// 로컬 이벤트 전송
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, Payload);
}
