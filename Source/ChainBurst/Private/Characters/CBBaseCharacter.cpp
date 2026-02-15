// project
#include "Characters/CBBaseCharacter.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/CBAttributeSet.h"

ACBBaseCharacter::ACBBaseCharacter()
{
	// 이 캐릭터는 매 프레임마다 Tick() 함수를 호출하지 않도록 설정 (Tick 비활성화)
	PrimaryActorTick.bCanEverTick = false;
	// 액터가 생성될 때 Tick이 비활성화된 상태로 시작하도록 설정
	PrimaryActorTick.bStartWithTickEnabled = false;
	// 캐릭터의 메시가 데칼(총알 자국, 피 등)을 받지 않도록 설정
	GetMesh()->bReceivesDecals = false;

	CBAbilitySystemComponent = CreateDefaultSubobject<UCBAbilitySystemComponent>(TEXT("CBAbilitySystemComponent"));
	CBAttributeSet = CreateDefaultSubobject<UCBAttributeSet>(TEXT("CBAttributeSet"));
}

UAbilitySystemComponent* ACBBaseCharacter::GetAbilitySystemComponent() const
{
	return GetCBAbilitySystemComponent();
}

void ACBBaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	if (CBAbilitySystemComponent)
	{
		// AbilitySystemComponent에 이 캐릭터(Actor)와 소유자 정보를 초기화하여 어빌리티 시스템이 올바르게 동작하도록 설정
		// 첫 번째 인자: Ability를 적용할 Actor (보통 자신)
		// 두 번째 인자: Ability의 Owner (일반적으로 자신 또는 컨트롤러)
		CBAbilitySystemComponent->InitAbilityActorInfo(this, this);

		ensureMsgf(!CharacterLoadout.IsNull(), TEXT("%s 의 CharacterLoadout 유효하지 않음."), *GetName());
	}
}

