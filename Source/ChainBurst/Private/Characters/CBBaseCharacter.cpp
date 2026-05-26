// project
#include "Characters/CBBaseCharacter.h"
#include "Components/Movement/CBLocomotionProcessor.h"
#include "Components/Movement/CBCharacterTrajectoryComponent.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Components/Animation/CBActionComponent.h"

ACBBaseCharacter::ACBBaseCharacter()
{
	// 이 캐릭터는 매 프레임마다 Tick() 함수를 호출하지 않도록 설정 (Tick 비활성화)
	PrimaryActorTick.bCanEverTick = false;
	// 액터가 생성될 때 Tick이 비활성화된 상태로 시작하도록 설정
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 네트워크 복제 활성화
	bReplicates = true;
	
	// 캐릭터의 메시가 데칼(총알 자국, 피 등)을 받지 않도록 설정
	GetMesh()->bReceivesDecals = false;
	
	CBTrajectoryComponent = CreateDefaultSubobject<UCBCharacterTrajectoryComponent>(TEXT("CBTrajectoryComponent"));
	CBLocomotionProcessor = CreateDefaultSubobject<UCBLocomotionProcessor>(TEXT("CBLocomotionProcessor"));
	CBActionComponent = CreateDefaultSubobject<UCBActionComponent>(TEXT("CBActionComponent"));
}

UAbilitySystemComponent* ACBBaseCharacter::GetAbilitySystemComponent() const
{
	return Cast<UAbilitySystemComponent>(CBASC);
}

// 필요할 때 Tick 설정
void ACBBaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool ACBBaseCharacter::RequestPlayMontage(const FGameplayTag InActionTag, bool bIsCombo)
{
	if (!CBActionComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] ActionComponent 가 없음"), *GetName());
		return false;
	}

	if (bIsCombo)
	{
		return CBActionComponent->RequestPlayComboMontage(InActionTag);
	}

	return CBActionComponent->RequestPlaySingleMontage(InActionTag);
}

void ACBBaseCharacter::HandleCharacterSystemReady()
{
	// 이미 시스템이 준비되었으면 중복 실행 방지
	if (bIsCharacterSystemReady) return;

	// 시스템 준비 완료 플래그 설정
	bIsCharacterSystemReady = true;

	// 델리게이트 방송 (구독 중인 컴포넌트 및 애님 인스턴스에 알림)
	OnCharacterSystemReadyDelegate.Broadcast();

	// Tick 활성화
	// SetActorTickEnabled(true);
}
