// project
#include "Characters/CBBaseCharacter.h"
#include "Components/Movement/CBLocomotionProcessor.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Components/Animation/CBActionComponent.h"
#include "Components/UI/CBUIComponent.h"
#include "Components/Perception/CBNoiseEmitterComponent.h"
#include "AbilitySystem/CBAttributeSet.h"
#include "CBGameplayTags.h"

// engine
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "MotionWarpingComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"

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

#pragma region 캐릭터 간 충돌
	// 캐릭터 위에는 설 수 없게 한다.
	// 캡슐 윗면에 착지하면 그 캐릭터가 무빙 베이스가 되어서 해당 캐릭터의 속도에 영향을 받음.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		// 걸을 수 없는 면으로 선언해야 베이스 자체가 성립하지 않음.
		// (WalkableSlope_Unwalkable, 0.f)로 설정하면 캡슐 위에 올라가도 걸을 수 없고, 캐릭터는 그 위에서 미끄러짐.
		Capsule->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.f));

		// 옆에서 부딪혔을 때 기어오르는 경로도 막음 (위 설정과 짝)
		Capsule->CanCharacterStepUpOn = ECB_No;
	}

	// 캡슐이 겹쳤을 때 프레임당 밀려나는 최대 거리 (엔진 기본 100cm).
	// 루트모션으로 계속 밀고 들어오면 매 프레임 이만큼 텔레포트되므로, 기본값이면 튕겨나가는 것처럼 보임.
	GetCharacterMovement()->MaxDepenetrationWithPawn = 20.f;
#pragma endregion

	CBLocomotionProcessor = CreateDefaultSubobject<UCBLocomotionProcessor>(TEXT("CBLocomotionProcessor"));
	CBActionComponent = CreateDefaultSubobject<UCBActionComponent>(TEXT("CBActionComponent"));
	CBUIComponent = CreateDefaultSubobject<UCBUIComponent>(TEXT("CBUIComponent"));
	CBNoiseEmitterComponent = CreateDefaultSubobject<UCBNoiseEmitterComponent>(TEXT("CBNoiseEmitterComponent"));
	CBMotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("CBMotionWarpingComponent"));
}

void ACBBaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 진영은 AI 판정(서버)뿐 아니라 아군/적 표현(클라)에도 필요하므로 전 클라이언트에 복제.
	DOREPLIFETIME(ACBBaseCharacter, Team);
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

void ACBBaseCharacter::RequestStopMontage(float BlendOutTime /* = 0.f */)
{
	if (!CBActionComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] ActionComponent 가 없음"), *GetName());
		return;
	}

	CBActionComponent->StopMontage(BlendOutTime);
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

#pragma region Team
// [서버] 진영 변경. 복제(OnRep_Team)로 전 클라이언트에 반영.
void ACBBaseCharacter::Auth_SetTeam(ECBTeam InTeam)
{
	if (!HasAuthority() || Team == InTeam) return;

	Team = InTeam;

	// 서버에는 OnRep 이 오지 않으므로 직접 방송.
	OnTeamChangedDelegate.Broadcast(Team);
}

// IGenericTeamAgentInterface 경유 팀 지정 (엔진 코드가 호출하는 경로). 권위 검사는 Auth_SetTeam 이 수행.
void ACBBaseCharacter::SetGenericTeamId(const FGenericTeamId& InTeamId)
{
	Auth_SetTeam(GenericIdToCBTeam(InTeamId));
}

void ACBBaseCharacter::OnRep_Team()
{
	OnTeamChangedDelegate.Broadcast(Team);
}
#pragma endregion

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

// 자식 클래스에서 로드아웃 데이터 로드 완료 시점에 호출. 델리게이트 방송 + 어트리뷰트 초기화 + 사망 상태 구독.
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

	// 사망 상태 구독.
	BindDeathStateEvent();

	// Tick 활성화 (필요할 때)
	// SetActorTickEnabled(true);
}

#pragma region Death
// Status.Dead 태그를 구독하고, 이미 사망한 상태라면 현재 값을 OnDeadTagChanged로 전달하여 사망 처리 수행.
void ACBBaseCharacter::BindDeathStateEvent()
{
	if (!CBASC) return;

	// 태그 추가/제거 델리게이트 구독 (태그는 전 클라이언트에 복제되므로 서버·오너·프록시가 모두 이 콜백을 받음)
	DeadTagChangedHandle = CBASC->RegisterGameplayTagEvent(
		CBGameplayTags::Status_Dead, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ACBBaseCharacter::OnDeadTagChanged);

	// 구독 이전에 태그가 이미 존재하면 NewCount > 0이므로 OnDeadTagChanged가 호출되어 사망 처리 수행
	OnDeadTagChanged(CBGameplayTags::Status_Dead, CBASC->GetTagCount(CBGameplayTags::Status_Dead));
}

// Status.Dead 태그 델리게이트 콜백. (권위 정리는 서버에서만, 표현 정리는 전 인스턴스에서 수행)
void ACBBaseCharacter::OnDeadTagChanged(const FGameplayTag /*CallbackTag*/, int32 NewCount)
{
	const bool bNowDead = NewCount > 0;

	// 이미 사망 상태와 동일하면 처리하지 않음 (태그 추가/제거가 반복될 수 있음)
	if (bIsDead == bNowDead) return;
	bIsDead = bNowDead;

	// 태그 제거(부활)은 따로 처리.
	if (!bIsDead) return;

	// 서버 정리 (파괴·두뇌 정지 등은 서버에서만)
	if (HasAuthority())
	{
		Auth_HandleDeath();
	}

	// 로컬 정리 (서버·오너·시뮬 프록시 각자 수행)
	Local_ApplyDeathVisuals();
}

// [서버] 사망 시 권위 정리.
void ACBBaseCharacter::Auth_HandleDeath()
{
	// 이동 정지.
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		// 공중에서 죽었다면 여기서 멈추지 않고 Landed() 에서 정지
		if (CMC->IsFalling())
		{
			// 수평 이동만 죽여 수직으로 떨어지게 함. 사망 후에는 입력·두뇌가 멈춰 가속이 들어오지 않음.
			CMC->Velocity.X = 0.f;
			CMC->Velocity.Y = 0.f;
		}
		else
		{
			// 착지 상태라면 즉시 이동 정지
			Auth_StopMovementForDeath();
		}
	}

	// 시체를 통과할 수 있게 하고, 무기 트레이스(ECC_Pawn 채널)에도 더 이상 걸리지 않게 함
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	// 자식 확장 훅 (AI = 두뇌 정지 등)
	Auth_OnDeath();

	// 디스폰 예약. 0 이하면 자동 파괴하지 않음 (폰이 사라지면 안 되는 플레이어 등)
	if (DespawnDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			DespawnTimerHandle, this, &ACBBaseCharacter::Auth_Despawn, DespawnDelay, false);
	}
}

// 착지 콜백. 공중 사망 후 낙하하던 시체가 바닥에 닿으면 그때 이동을 정지시킨다.
void ACBBaseCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// 살아 있으면 평소 착지. 정지는 권위에서만 결정하고 클라는 이동 복제로 처리.
	if (!bIsDead || !HasAuthority()) return;

	// 착지할 때 사망 상태인 경우 이동 정지
	Auth_StopMovementForDeath();
}

// [서버] 이동을 완전히 정지 (사망 확정 시점 또는 공중 사망 후 착지 시점)
void ACBBaseCharacter::Auth_StopMovementForDeath()
{
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->StopMovementImmediately();
		CMC->DisableMovement();
	}
}

// [서버] 디스폰 타이머 만료 시 액터 파괴.
void ACBBaseCharacter::Auth_Despawn()
{
	Destroy();
}

// 사망 로컬 정리. 구독 중인 컴포넌트에 알림 (머리 위 체력바 숨김 등).
void ACBBaseCharacter::Local_ApplyDeathVisuals()
{
	OnCharacterDiedDelegate.Broadcast();
}

void ACBBaseCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 사망 태그 구독 해제. Chaser는 ASC가 PlayerState 소유라 폰보다 오래 살아남으므로 반드시 해제
	if (CBASC && DeadTagChangedHandle.IsValid())
	{
		CBASC->RegisterGameplayTagEvent(CBGameplayTags::Status_Dead, EGameplayTagEventType::NewOrRemoved)
			.Remove(DeadTagChangedHandle);
	}
	DeadTagChangedHandle.Reset();

	// 디스폰 타이머 정리
	GetWorldTimerManager().ClearTimer(DespawnTimerHandle);

	Super::EndPlay(EndPlayReason);
}
#pragma endregion

void ACBBaseCharacter::InitializeAttributes()
{
	if (CBAttributeSet)
	{
		CBAttributeSet->OnCharacterSystemReady();
	}
}
