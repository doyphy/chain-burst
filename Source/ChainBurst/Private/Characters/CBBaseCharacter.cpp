// project
#include "Characters/CBBaseCharacter.h"
#include "Components/Movement/CBLocomotionProcessor.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Components/Animation/CBActionComponent.h"
#include "Components/UI/CBUIComponent.h"
#include "Components/Perception/CBNoiseEmitterComponent.h"
#include "AbilitySystem/CBAttributeSet.h"

// engine
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

ACBBaseCharacter::ACBBaseCharacter()
{
	// 이 캐릭터는 매 프레임마다 Tick() 함수를 호출하지 않도록 설정 (Tick 비활성화)
	PrimaryActorTick.bCanEverTick = false;
	// 액터가 생성될 때 Tick이 비활성화된 상태로 시작하도록 설정
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 네트워크 복제 활성화
	bReplicates = true;
	SetReplicateMovement(true);
	
	// 캐릭터의 메시가 데칼(총알 자국, 피 등)을 받지 않도록 설정
	GetMesh()->bReceivesDecals = false;
	
	CBLocomotionProcessor = CreateDefaultSubobject<UCBLocomotionProcessor>(TEXT("CBLocomotionProcessor"));
	CBActionComponent = CreateDefaultSubobject<UCBActionComponent>(TEXT("CBActionComponent"));
	CBUIComponent = CreateDefaultSubobject<UCBUIComponent>(TEXT("CBUIComponent"));
	CBNoiseEmitterComponent = CreateDefaultSubobject<UCBNoiseEmitterComponent>(TEXT("CBNoiseEmitterComponent"));
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

void ACBBaseCharacter::OnMovementSpeedChanged(float NewSpeed)
{
	if (GetCharacterMovement())
	{
		const float ClampedSpeed = FMath::Max(NewSpeed, 0.0f);
		GetCharacterMovement()->MaxWalkSpeed = ClampedSpeed;
	}
}

bool ACBBaseCharacter::RequestPlayMontage(const FGameplayTag InActionTag, int32 ComboIndex /* = 0 */)
{
	if (!CBActionComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] ActionComponent 가 없음"), *GetName());
		return false;
	}

	return CBActionComponent->RequestPlayMontage(InActionTag, ComboIndex);
}

UCBCombatComponent* ACBBaseCharacter::GetCBCombatComponent() const
{
	return nullptr;
}

UCBUIComponent* ACBBaseCharacter::GetCBUIComponent() const
{
	return CBUIComponent.Get();
}

void ACBBaseCharacter::Server_SendGameplayEvent_Implementation(AActor* Actor, FGameplayTag EventTag, FGameplayEventData Payload)
{
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Actor, EventTag, Payload);
}

bool ACBBaseCharacter::StartSystemInitialization()
{
	// 이미 초기화가 시작/완료되었으면 재진입 방지 (PossessedBy/OnRep_PlayerState 중복 호출 대비)
	if (SystemState != ECBSystemState::Uninitialized)
	{
		return false;
	}

	// 초기화 진행 상태로 전환. 완료는 공용 데이터 로드 콜백에서 HandleCharacterSystemReady로 통지된다.
	SystemState = ECBSystemState::Initializing;
	return true;
}

void ACBBaseCharacter::HandleCharacterSystemReady()
{
	// 이미 시스템이 준비되었으면 중복 실행 방지
	if (SystemState == ECBSystemState::Ready) return;

	// 시스템 준비 완료 상태로 전환
	SystemState = ECBSystemState::Ready;

	// 델리게이트 방송 (구독 중인 컴포넌트 및 애님 인스턴스에 알림)
	OnCharacterSystemReadyDelegate.Broadcast();

	// 어트리뷰트 초기화 (모든 비동기 로드 완료 후 실행되므로 MovementData 등 준비 완료 상태)
	InitializeAttributes();

	// Tick 활성화 (필요할 때)
	// SetActorTickEnabled(true);
}

void ACBBaseCharacter::InitializeAttributes()
{
	if (CBAttributeSet)
	{
		CBAttributeSet->OnCharacterSystemReady();
	}
}
