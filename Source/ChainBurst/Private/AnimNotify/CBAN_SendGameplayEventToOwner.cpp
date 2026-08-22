// project
#include "AnimNotify/CBAN_SendGameplayEventToOwner.h"
#include "Characters/CBBaseCharacter.h"

// engine
#include "AbilitySystemBlueprintLibrary.h"

void UCBAN_SendGameplayEventToOwner::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	
	if (!MeshComp) return;

	// 애님 인스턴스 가져오기
	UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
	if (!AnimInstance) return;

	// 현재 메인으로 재생중인 몽타주 가져오기
	UAnimMontage* CurrentActiveMontage = AnimInstance->GetCurrentActiveMontage();
	
	// 이 노티파이를 부른 몽타주가 메인 몽타주가 아니라면
	// 블렌드 타임에 호출할 경우 CurrentActiveMontage가 바뀌기 때문에 블렌드 타임에 노티파이 호출안하도록 주의.
	if (Animation != CurrentActiveMontage)
	{
		return; 
	}
	
	APawn* Owner = Cast<APawn>(MeshComp->GetOwner());
	
	// 이 애님 노티파이가 실행되는 액터가 로컬 플레이어의 소유라면 (로컬에서 실행되는 애님 노티파이인지 확인)
	if (Owner && Owner->IsLocallyControlled())
	{
		FGameplayEventData Payload = FGameplayEventData();
		
		// 애님 노티파이 이벤트가 발생한 액터(소유자)에게 게임플레이 이벤트 전송
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, Payload);

		// 서버가 아니면 서버에도 이벤트 전송 (RPC)
		if (!Owner->HasAuthority())
		{
			if (ACBBaseCharacter* Character = Cast<ACBBaseCharacter>(Owner))
			{
				Character->Server_SendGameplayEvent(Owner, EventTag, Payload);
			}
		}
	} 
}
