// project
#include "Characters/CBBaseCharacter.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/CBAttributeSet.h"
#include "CBGameplayTags.h"
#include "DataAssets/Movement/CBCharacterMovementData.h"
#include "Components/Movement/CBLocomotionProcessor.h"
#include "Components/Movement/CBCharacterTrajectoryComponent.h"

// engine
#include "GameFramework/CharacterMovementComponent.h" 

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
	
	CBAbilitySystemComponent = CreateDefaultSubobject<UCBAbilitySystemComponent>(TEXT("CBAbilitySystemComponent"));
	CBAttributeSet = CreateDefaultSubobject<UCBAttributeSet>(TEXT("CBAttributeSet"));
	CBTrajectoryComponent = CreateDefaultSubobject<UCBCharacterTrajectoryComponent>(TEXT("CBTrajectoryComponent"));
	CBLocomotionProcessor = CreateDefaultSubobject<UCBLocomotionProcessor>(TEXT("CBLocomotionProcessor"));
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

		// 어트리뷰트 초기화
		InitializeAttributes();
		
		ensureMsgf(!CharacterLoadout.IsNull(), TEXT("%s 의 CharacterLoadout 유효하지 않음."), *GetName());
	}
}

void ACBBaseCharacter::InitializeAttributes()
{
	if (!GetAbilitySystemComponent() || !CBAttributeSet) return;

	if (MovementDataAsset)
	{
		// 기본 상태(Run) 속도 가져오기
		float InitialSpeed = MovementDataAsset->GetSpeedForTag(CBGameplayTags::Shared_Movement_Run);

		// 기본 값 설정
		if (HasAuthority())
		{
			CBAttributeSet->InitMovementSpeed(InitialSpeed);
		}

		// 수동으로 값 초기화
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->MaxWalkSpeed = InitialSpeed;
		}

		// 중복 방지를 위해 기존 바인딩 제거 (네트워크 재접속, 초기화 로직 재실행 등)
		GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(CBAttributeSet->GetMovementSpeedAttribute())
			.RemoveAll(this);

		// 이동 속도 변경 시 실행되는 델리게이트에 함수 바인딩
		GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(CBAttributeSet->GetMovementSpeedAttribute())
		.AddUObject(this, &ACBBaseCharacter::OnMovementSpeedChanged);
	}

	// [TODO] 기타 속성 초기화 로직 (데이터 에셋에 Health/Mana 등 추가 후 구현)
}

void ACBBaseCharacter::OnMovementSpeedChanged(const FOnAttributeChangeData& Data)
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		// 0보다 작은 값이 들어오지 않도록 안전장치 추가
		const float NewSpeed = FMath::Max(Data.NewValue, 0.0f);
		MoveComp->MaxWalkSpeed = NewSpeed;
	}
}

void ACBBaseCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (CBAbilitySystemComponent)
	{
		// ASC 초기화
		CBAbilitySystemComponent->InitAbilityActorInfo(this, this);

		// 어트리뷰트 초기화
		InitializeAttributes();
	}
}

