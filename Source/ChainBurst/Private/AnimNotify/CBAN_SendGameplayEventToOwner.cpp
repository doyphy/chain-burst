// project
#include "AnimNotify/CBAN_SendGameplayEventToOwner.h"

// engine
#include "AbilitySystemBlueprintLibrary.h"

void UCBAN_SendGameplayEventToOwner::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;
	
	APawn* Owner = Cast<APawn>(MeshComp->GetOwner());
	
	if (Owner)
	{
		// 이 애님 노티파이가 실행되는 액터가 로컬 플레이어의 소유라면 (로컬에서 실행되는 애님 노티파이인지 확인)
		if (Owner->IsLocallyControlled())
		{
			// 애님 노티파이 이벤트가 발생한 액터(소유자)에게 게임플레이 이벤트 전송
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, FGameplayEventData());
		}
	}
}
