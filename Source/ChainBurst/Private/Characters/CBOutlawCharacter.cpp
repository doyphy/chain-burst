// project
#include "Characters/CBOutlawCharacter.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/CBAttributeSet.h"


ACBOutlawCharacter::ACBOutlawCharacter()
{
	// ASC 생성
	CBASC = CreateDefaultSubobject<UCBAbilitySystemComponent>(TEXT("CBAbilitySystemComponent"));
	// 복제 설정
	CBASC->SetIsReplicated(true);
	// AI 의 복제 설정은 최소한으로 설정
	CBASC->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// AttributeSet 생성
	CBAttributeSet = CreateDefaultSubobject<UCBAttributeSet>(TEXT("CBAttributeSet"));

	// AI 컨트롤러 자동 빙의 설정
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
}

void ACBOutlawCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
}

void ACBOutlawCharacter::PostNetInit()
{
	Super::PostNetInit();
}

void ACBOutlawCharacter::InitializeAISystem()
{
}

void ACBOutlawCharacter::Auth_InitServerData()
{
}

void ACBOutlawCharacter::Local_InitClientData()
{
}
