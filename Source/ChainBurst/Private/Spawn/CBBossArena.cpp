// project
#include "Spawn/CBBossArena.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/CBAttributeSet.h"
#include "CBGameplayTags.h"
#include "Characters/CBAICharacter.h"
#include "Characters/CBBaseCharacter.h"
#include "Controllers/CBAIController.h"
#include "PlayerState/CBPlayerState.h"

// engine
#include "Abilities/GameplayAbilityTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

namespace
{
	/** 플레이어 컨트롤러의 ASC. 플레이어 ASC 는 PlayerState 소유라 폰이 바뀌어도 같음 */
	UAbilitySystemComponent* GetPlayerASC(const APlayerController* InPlayerController)
	{
		const ACBPlayerState* CBPlayerState = InPlayerController ? InPlayerController->GetPlayerState<ACBPlayerState>() : nullptr;
		return CBPlayerState ? CBPlayerState->GetCBAbilitySystemComponent() : nullptr;
	}
}

ACBBossArena::ACBBossArena()
{
	PrimaryActorTick.bCanEverTick = false;

	// 보스 구역 = 이 박스. 콜리전은 끄고 범위 값·에디터 시각화 용도로만 씀 (인원 판정은 오버랩이 아니라 스캔 폴링)
	ArenaBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ArenaBounds"));
	SetRootComponent(ArenaBounds);
	ArenaBounds->SetBoxExtent(FVector(1500.f, 1500.f, 500.f));
	ArenaBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArenaBounds->SetGenerateOverlapEvents(false);
	ArenaBounds->ShapeColor = FColor(200, 60, 60);

	// 지점 화살표들. 레벨에 배치한 뒤 각자 원하는 자리로 끌어다 둠 (게임 중에는 보이지 않음)
	BossSpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("BossSpawnPoint"));
	BossSpawnPoint->SetupAttachment(ArenaBounds);
	BossSpawnPoint->ArrowColor = FColor::Red;

	EntranceBlockerPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("EntranceBlockerPoint"));
	EntranceBlockerPoint->SetupAttachment(ArenaBounds);
	EntranceBlockerPoint->ArrowColor = FColor::Yellow;

	RespawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("RespawnPoint"));
	RespawnPoint->SetupAttachment(ArenaBounds);
	RespawnPoint->ArrowColor = FColor::Green;

	// 등장 연출 카메라. 이 액터가 뷰 타겟이 되면 엔진(AActor::CalcCamera)이 활성 카메라 컴포넌트를 찾아 씀
	IntroCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("IntroCamera"));
	IntroCamera->SetupAttachment(ArenaBounds);
}

FTransform ACBBossArena::GetRespawnTransform(TSubclassOf<APawn> InPawnClass) const
{
	return MakeGroundedSpawnTransform(RespawnPoint, InPawnClass);
}

void ACBBossArena::BeginPlay()
{
	Super::BeginPlay();

	// 구역 판정은 서버 권위. 클라는 복제된 보스·입구 차단 액터·대기 태그만 받으므로 아무것도 하지 않음
	if (!HasAuthority()) return;

	// 보스가 없으면 조용히 아무 일도 일어나지 않으므로 미리 경고하고 멈춤
	if (!BossClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossArena][%s] BossClass 가 비어 있어 동작하지 않음"), *GetName());
		return;
	}

	// 입구 차단은 없어도 보스전은 진행되지만, 구역을 나갈 수 있게 되므로 경고
	if (!EntranceBlockerClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossArena][%s] EntranceBlockerClass 가 비어 있어 입구를 막지 않음"), *GetName());
	}

	// 카운트다운 GE 는 기간형이어야 남은 시간을 읽을 수 있고, 태그를 부여해야 위젯이 시작·종료를 알 수 있음.
	if (!CountdownEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossArena][%s] CountdownEffectClass 가 비어 있어 카운트다운 표시 없음"), *GetName());
	}
	else
	{
		const UGameplayEffect* CountdownEffect = GetDefault<UGameplayEffect>(CountdownEffectClass);
		if (CountdownEffect->DurationPolicy != EGameplayEffectDurationType::HasDuration
			|| !CountdownEffect->GetGrantedTags().HasTagExact(CBGameplayTags::Status_Arena_Countdown))
		{
			UE_LOG(LogTemp, Warning, TEXT("[BossArena][%s] %s 가 기간형(Has Duration)이 아니거나 Status.Arena.Countdown 을 부여하지 않아 카운트다운 표시 없음"),
				*GetName(), *GetNameSafe(CountdownEffectClass));
			CountdownEffectClass = nullptr;
		}
	}

	// 행동 차단 GE 는 연출이 끝날 때 걷어내므로 Infinite 여야 하고, 차단 태그가 없으면 아무것도 막지 않음 (컴포넌트 누락).
	// 조건이 안 맞으면 행동이 조용히 풀려 있으므로 경고하고 쓰지 않음 (이동·시점은 시네마틱 모드가 그대로 막음)
	if (!IntroBlockEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossArena][%s] IntroBlockEffectClass 가 비어 있어 등장 연출 중 공격·대시 등을 막지 않음"), *GetName());
	}
	else
	{
		const UGameplayEffect* BlockEffect = GetDefault<UGameplayEffect>(IntroBlockEffectClass);
		if (BlockEffect->DurationPolicy != EGameplayEffectDurationType::Infinite || BlockEffect->GetBlockedAbilityTags().IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[BossArena][%s] %s 가 Infinite 가 아니거나 차단 태그(Block Abilities with Tags)가 없어 등장 연출 중 공격·대시 등을 막지 않음"),
				*GetName(), *GetNameSafe(IntroBlockEffectClass));
			IntroBlockEffectClass = nullptr;
		}
	}

	// 인원 집계·이탈·전멸 감지를 겸하는 단일 스캔 타이머
	GetWorldTimerManager().SetTimer(
		ScanTimerHandle, this, &ACBBossArena::Auth_ScanTick, ScanInterval, true);
}

void ACBBossArena::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ScanTimerHandle);
	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);

	Super::EndPlay(EndPlayReason);
}

// [서버] 스캔 주기마다 인원을 다시 세어 단계별 처리를 진행.
void ACBBossArena::Auth_ScanTick()
{
	TArray<APlayerController*> InsidePlayers;
	const int32 TotalPlayers = Auth_CollectPlayersInArena(InsidePlayers);

	// 접속한 플레이어 전원이 살아서 구역 안에 있는지 (로딩 중·리스폰 대기 중인 플레이어가 있으면 거짓)
	const bool bEveryoneInside = TotalPlayers > 0 && InsidePlayers.Num() == TotalPlayers;

	switch (Phase)
	{
	case ECBBossArenaPhase::Waiting: // 전원 진입 대기
	{
		if (bEveryoneInside)
		{
			Auth_StartCountdown(InsidePlayers);
			return;
		}

		// 들어와 있는 플레이어에게만 대기 표시
		Auth_UpdateWaitingTags(InsidePlayers);
		return;
	}

	case ECBBossArenaPhase::Countdown: // 보스 등장 대기 (등장 자체는 타이머가 처리)
	{
		// 한 명이라도 나가거나 죽으면 취소하고 다시 대기. 다시 전원이 들어오면 카운트다운을 처음부터 셈
		if (!bEveryoneInside)
		{
			Auth_CancelCountdown(InsidePlayers);
		}
		return;
	}

	case ECBBossArenaPhase::Fighting: // 보스전
	{
		// 보스 처치가 전멸보다 먼저 - 마지막 일격과 동시에 전원이 쓰러져도 승리로 침
		const ACBBaseCharacter* Boss = SpawnedBoss.Get();
		if (!Boss || Boss->IsDead())
		{
			Auth_ClearArena();
			return;
		}

		// 부활한 플레이어는 구역 밖에서 나타나므로, 안에 살아있는 플레이어가 없으면 다시 들어올 방법이 없음 - 리셋
		if (InsidePlayers.IsEmpty())
		{
			Auth_ResetBossFight();
			return;
		}

		// 등장 연출 동안은 플레이어가 행동 차단으로 묶여 있으므로 졸개는 연출이 끝난 뒤부터
		if (!bBossIntroActive)
		{
			Auth_UpdateMinions(InsidePlayers);
		}
		return;
	}

	case ECBBossArenaPhase::Cleared: // 완료 (스캔이 멈춰 있어 오지 않음)
		return;
	}
}

// [서버] 전원 진입. 대기 표시를 내리고 카운트다운 GE·보스 등장 타이머를 시작.
void ACBBossArena::Auth_StartCountdown(const TArray<APlayerController*>& InInsidePlayers)
{
	// 더 기다릴 플레이어가 없으므로 대기 표시를 모두 내림
	Auth_UpdateWaitingTags({});

	// 기다릴 시간이 없으면 바로 등장. (SetTimer 는 0 초를 받으면 타이머를 걸지 않음)
	if (BossAppearDelay <= 0.f)
	{
		Auth_StartBossFight();
		return;
	}

	Phase = ECBBossArenaPhase::Countdown;

	// 플레이어마다 카운트다운 GE. 기간을 BossAppearDelay 로 덮어써 값이 이 액터 한 곳에만 있게 함.
	// 플레이어 ASC 는 Mixed 라 소유 클라가 기간·시작 시각까지 받아 위젯이 남은 시간을 읽음
	if (CountdownEffectClass)
	{
		for (const APlayerController* PlayerController : InInsidePlayers)
		{
			UAbilitySystemComponent* ASC = GetPlayerASC(PlayerController);
			if (!ASC) continue;

			const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(CountdownEffectClass, 1.f, ASC->MakeEffectContext());
			if (!SpecHandle.IsValid()) continue;

			SpecHandle.Data->SetDuration(BossAppearDelay, true);
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			CountdownASCs.Add(ASC);
		}
	}

	// 보스 등장 타이머. 카운트다운 GE 기간과 함께 맞춤.
	GetWorldTimerManager().SetTimer(
		CountdownTimerHandle, this, &ACBBossArena::Auth_HandleCountdownFinished, BossAppearDelay, false);

	UE_LOG(LogTemp, Log, TEXT("[BossArena][%s] 전원 진입 - %.1f초 뒤 보스 등장"), *GetName(), BossAppearDelay);
}

// [서버] 카운트다운 취소. 표시를 걷고 남은 플레이어를 다시 대기 상태로.
void ACBBossArena::Auth_CancelCountdown(const TArray<APlayerController*>& InInsidePlayers)
{
	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	Auth_RemoveCountdownEffects();

	Phase = ECBBossArenaPhase::Waiting;
	Auth_UpdateWaitingTags(InInsidePlayers);

	UE_LOG(LogTemp, Log, TEXT("[BossArena][%s] 카운트다운 취소 - 구역 안 %d명"), *GetName(), InInsidePlayers.Num());
}

// [서버] 보스 등장 시각. 카운트 다운이 끝나면 호출
void ACBBossArena::Auth_HandleCountdownFinished()
{
	// 마지막 스캔 뒤에 누가 나갔을 수 있으므로 인원을 다시 확인. 전원이 그대로면 보스전을 시작하고 아니면 카운트다운을 취소
	TArray<APlayerController*> InsidePlayers;
	const int32 TotalPlayers = Auth_CollectPlayersInArena(InsidePlayers);

	if (TotalPlayers > 0 && InsidePlayers.Num() == TotalPlayers)
	{
		Auth_StartBossFight();
	}
	else
	{
		Auth_CancelCountdown(InsidePlayers);
	}
}

void ACBBossArena::Auth_RemoveCountdownEffects()
{
	// 보스 등장 때는 GE 도 같은 순간 만료되지만, 타이머와 GE 만료의 처리 순서에 기대지 않고 직접 걷어냄
	const FGameplayTagContainer CountdownTags(CBGameplayTags::Status_Arena_Countdown);
	for (const TWeakObjectPtr<UAbilitySystemComponent>& CountdownASC : CountdownASCs)
	{
		if (UAbilitySystemComponent* ASC = CountdownASC.Get())
		{
			ASC->RemoveActiveEffectsWithGrantedTags(CountdownTags);
		}
	}
	CountdownASCs.Reset();
}

// [서버] 보스 등장. 보스와 입구 차단 액터를 소환.
void ACBBossArena::Auth_StartBossFight()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 카운트다운 표시를 내림
	Auth_RemoveCountdownEffects();

	FActorSpawnParameters BossSpawnParams;
	BossSpawnParams.Owner = this;
	BossSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// 보스 런타임 생성
	ACBAICharacter* Boss = World->SpawnActor<ACBAICharacter>(
		BossClass, MakeGroundedSpawnTransform(BossSpawnPoint, BossClass), BossSpawnParams);

	// 보스 없이 입구만 막히면 나갈 수 없게 되므로, 실패하면 막지 않고 끝냄
	if (!Boss)
	{
		Phase = ECBBossArenaPhase::Cleared;
		GetWorldTimerManager().ClearTimer(ScanTimerHandle);

		UE_LOG(LogTemp, Warning, TEXT("[BossArena][%s] 보스 소환 실패 - 입구를 막지 않고 종료"), *GetName());
		return;
	}

	SpawnedBoss = Boss;
	Phase = ECBBossArenaPhase::Fighting;

	if (EntranceBlockerClass)
	{
		FActorSpawnParameters BlockerSpawnParams;
		BlockerSpawnParams.Owner = this;
		BlockerSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Blocker = World->SpawnActor<AActor>(EntranceBlockerClass,
			EntranceBlockerPoint->GetComponentLocation(), EntranceBlockerPoint->GetComponentRotation(), BlockerSpawnParams);

		if (Blocker)
		{
			// 클라 이동 예측도 이 벽에 막혀야 서버 보정으로 튕기지 않음. BP 설정 누락에 기대지 않고 코드에서 복제를 켬
			Blocker->SetReplicates(true);
			SpawnedEntranceBlocker = Blocker;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[BossArena][%s] 보스전 시작: %s"), *GetName(), *Boss->GetName());

	// 소환과 동시에 등장 연출
	Auth_StartBossIntro();
}

// [서버] 보스 내구도를 접속 인원만큼 곱함.
void ACBBossArena::Auth_ScaleBossByPlayerCount(UAbilitySystemComponent* InBossASC) const
{
	if (!InBossASC) return;

	// 보스전은 접속 인원 전원이 들어와야 시작하므로 접속 인원 = 보스전 인원. 1명이면 1인 기준 값 그대로
	const int32 PlayerCount = GetWorld()->GetNumPlayerControllers();
	if (PlayerCount <= 1) return;

	// 기본값에 바로 곱함. 보스 ASC 는 보스와 함께 사라져 되돌릴 일이 없음 (어트리뷰트라 전 클라로 복제됨)
	const FGameplayAttribute MaxHealthAttribute = UCBAttributeSet::GetMaxHealthAttribute();
	InBossASC->SetNumericAttributeBase(MaxHealthAttribute, InBossASC->GetNumericAttributeBase(MaxHealthAttribute) * PlayerCount);

	const FGameplayAttribute MaxStunGaugeAttribute = UCBAttributeSet::GetMaxStunGaugeAttribute();
	InBossASC->SetNumericAttributeBase(MaxStunGaugeAttribute, InBossASC->GetNumericAttributeBase(MaxStunGaugeAttribute) * PlayerCount);

	// 시작 GE 가 채운 체력은 1인 기준이라 늘어난 최대까지 채움
	const float NewMaxHealth = InBossASC->GetNumericAttribute(MaxHealthAttribute);
	InBossASC->SetNumericAttributeBase(UCBAttributeSet::GetCurrentHealthAttribute(), NewMaxHealth);

	UE_LOG(LogTemp, Log, TEXT("[BossArena][%s] 보스 내구도 x%d - 최대 체력 %.0f, 최대 기절 게이지 %.0f"),
		*GetName(), PlayerCount, NewMaxHealth, InBossASC->GetNumericAttribute(MaxStunGaugeAttribute));
}

// [서버] 보스 처치. 입구를 열고 더 이상 진행하지 않음.
void ACBBossArena::Auth_ClearArena()
{
	// 연출 도중 끝나면 종료 태그가 오지 않으므로 여기서 카메라를 되돌림 (연출 중이 아니면 아무 일 없음)
	Auth_EndBossIntro();

	// 보스와 함께 쓰러지는 그림 - 사망 몽타주·소멸 연출이 평소처럼 진행됨
	Auth_RemoveMinions(/*bInKill =*/ true);

	Phase = ECBBossArenaPhase::Cleared;
	Auth_OpenEntrance();
	GetWorldTimerManager().ClearTimer(ScanTimerHandle);

	UE_LOG(LogTemp, Log, TEXT("[BossArena][%s] 보스 처치 - 입구 개방"), *GetName());
}

// [서버] 전멸. 보스를 없애고 입구를 열어 처음부터 다시 시작하게 함.
void ACBBossArena::Auth_ResetBossFight()
{
	// 보스가 파괴되면 종료 태그가 오지 않으므로 파괴 전에 카메라를 되돌림 (연출 중이 아니면 아무 일 없음)
	Auth_EndBossIntro();

	// 죽은 게 아니라 사라지는 것이라 소멸 연출 없이 파괴됨. 보스 바는 보스 파괴(EndPlay)에서 함께 내려감
	if (ACBBaseCharacter* Boss = SpawnedBoss.Get())
	{
		Boss->Destroy();
	}
	SpawnedBoss.Reset();

	// 졸개도 보스처럼 연출 없이 사라짐. 다음 보스전은 묶음을 처음부터 다시 셈
	Auth_RemoveMinions(/*bInKill =*/ false);

	Auth_OpenEntrance();
	Phase = ECBBossArenaPhase::Waiting;

	UE_LOG(LogTemp, Log, TEXT("[BossArena][%s] 전멸 - 보스 제거, 입구 개방 후 대기로"), *GetName());
}

void ACBBossArena::Auth_OpenEntrance()
{
	if (AActor* Blocker = SpawnedEntranceBlocker.Get())
	{
		Blocker->Destroy();
	}
	SpawnedEntranceBlocker.Reset();
}

#pragma region BossIntro
// [서버] 등장 연출 시작. 카메라를 먼저 돌려 생성 연출(디졸브)부터 보이게 하고, 보스가 준비되면 등장 어빌리티를 발동.
void ACBBossArena::Auth_StartBossIntro()
{
	ACBBaseCharacter* Boss = SpawnedBoss.Get();
	if (!Boss) return;

	bBossIntroActive = true;
	Auth_SetPlayersIntroView(true);

	// 등장 어빌리티는 로드아웃이 부여하므로 준비 완료 뒤에만 발동할 수 있음.
	// 로드아웃이 이미 메모리에 있으면 소환(SpawnActor) 안에서 준비까지 끝나 있음
	if (Boss->IsCharacterSystemReady())
	{
		Auth_HandleBossReady();
		return;
	}

	BossReadyHandle = Boss->OnCharacterSystemReadyDelegate.AddUObject(this, &ACBBossArena::Auth_HandleBossReady);
}

// [서버] 보스 준비 완료. 등장 어빌리티를 발동하고 그 종료를 기다림.
void ACBBossArena::Auth_HandleBossReady()
{
	ACBBaseCharacter* Boss = SpawnedBoss.Get();
	if (!Boss) return;

	// 1회성 - 구독으로 들어왔으면 해제
	if (BossReadyHandle.IsValid())
	{
		Boss->OnCharacterSystemReadyDelegate.Remove(BossReadyHandle);
		BossReadyHandle.Reset();
	}

	UAbilitySystemComponent* BossASC = Boss->GetAbilitySystemComponent();
	if (!BossASC)
	{
		Auth_EndBossIntro();
		return;
	}

	// 로드아웃 시작 GE 가 1인 기준 값을 정한 뒤라 여기서 곱함 (소환 직후에는 아직 적용 전일 수 있음)
	Auth_ScaleBossByPlayerCount(BossASC);

	// 종료 구독을 발동보다 먼저 - 어빌리티가 발동한 호출 안에서 곧바로 끝나도 놓치지 않게
	BossIntroTagChangedHandle = BossASC->RegisterGameplayTagEvent(CBGameplayTags::Status_Combat_Intro, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ACBBossArena::HandleBossIntroTagChanged);

	// 몽타주 재생·BT 대기(Staggered)·슈퍼아머는 전부 등장 어빌리티(BP)가 맡음
	FGameplayEventData Payload;
	Payload.EventTag = CBGameplayTags::Event_Combat_Intro;
	Payload.Instigator = this;
	Payload.Target = Boss;
	const int32 ActivatedCount = BossASC->HandleGameplayEvent(Payload.EventTag, &Payload);

	// 연출 태그가 없으면 끝을 알릴 신호도 없어 카메라가 묶인 채 남음 - 바로 되돌림
	// (발동된 어빌리티가 없음: 로드아웃 부여·트리거 누락 / 있는데 태그 없음: ActivationOwnedTags 누락)
	if (bBossIntroActive && !BossASC->HasMatchingGameplayTag(CBGameplayTags::Status_Combat_Intro))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossArena][%s] %s 에 Status.Combat.Intro 가 없어 등장 연출을 바로 끝냄 (발동된 어빌리티 %d개 - 로드아웃·트리거·ActivationOwnedTags 확인)"),
			*GetName(), *Boss->GetName(), ActivatedCount);
		Auth_EndBossIntro();
	}
}

// 등장 연출 태그 변경 콜백
void ACBBossArena::HandleBossIntroTagChanged(const FGameplayTag /*InTag*/, int32 NewCount)
{
	// 태그가 빠짐 = 등장 어빌리티 종료 (몽타주 블렌드 아웃 시작)
	if (NewCount == 0)
	{
		Auth_EndBossIntro();
	}
}

// [서버] 등장 연출 종료. 구독을 풀고 카메라·입력을 되돌림.
void ACBBossArena::Auth_EndBossIntro()
{
	// 종료 경로가 여럿(태그 제거·보스 처치·전멸·설정 누락)이라 한 번만 처리
	if (!bBossIntroActive) return;
	bBossIntroActive = false;

	if (ACBBaseCharacter* Boss = SpawnedBoss.Get())
	{
		if (BossReadyHandle.IsValid())
		{
			Boss->OnCharacterSystemReadyDelegate.Remove(BossReadyHandle);
		}

		UAbilitySystemComponent* BossASC = Boss->GetAbilitySystemComponent();
		if (BossASC && BossIntroTagChangedHandle.IsValid())
		{
			BossASC->UnregisterGameplayTagEvent(BossIntroTagChangedHandle, CBGameplayTags::Status_Combat_Intro, EGameplayTagEventType::NewOrRemoved);
		}
	}
	BossReadyHandle.Reset();
	BossIntroTagChangedHandle.Reset();

	Auth_SetPlayersIntroView(false);

	UE_LOG(LogTemp, Log, TEXT("[BossArena][%s] 등장 연출 종료"), *GetName());
}

// [서버] 모든 플레이어 카메라·입력 전환. 원격 PC 는 엔진이 ClientSetViewTarget / ClientSetCinematicMode 로 클라에 보냄
void ACBBossArena::Auth_SetPlayersIntroView(bool bInIntro)
{
	const UWorld* World = GetWorld();
	if (!World) return;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!PlayerController) continue;

		if (bInIntro)
		{
			// 이 액터를 뷰 타겟으로 - 활성 카메라 컴포넌트(IntroCamera)로 봄. 클라에도 레벨 배치 액터로 존재해 참조가 그대로 풀림
			PlayerController->SetViewTargetWithBlend(this, IntroCameraBlendTime);
		}
		else if (APawn* Pawn = PlayerController->GetPawn())
		{
			// 폰이 없으면(리스폰 대기) 다음 빙의가 뷰 타겟을 폰으로 맞추므로 건드리지 않음
			PlayerController->SetViewTargetWithBlend(Pawn, IntroCameraBlendTime);
		}

		// 이동·시점 입력만 막음. 폰·HUD 표시는 그대로 둠
		PlayerController->SetCinematicMode(bInIntro, /*bHidePlayer =*/ false, /*bAffectsHUD =*/ false, /*bAffectsMovement =*/ true, /*bAffectsTurning =*/ true);

		// 어빌리티로 하는 행동(공격·대시·점프 등)은 차단 GE 로 막음. 플레이어 ASC 는 PlayerState 소유라
		// 연출 중 죽어 리스폰해도 GE 가 남아 있다가 여기서 함께 걷힘
		UAbilitySystemComponent* ASC = GetPlayerASC(PlayerController);
		if (!ASC || !IntroBlockEffectClass) continue;

		if (bInIntro)
		{
			const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(IntroBlockEffectClass, 1.f, ASC->MakeEffectContext());
			if (SpecHandle.IsValid())
			{
				ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			}
		}
		else
		{
			ASC->RemoveActiveGameplayEffectBySourceEffect(IntroBlockEffectClass, nullptr);
		}
	}
}
#pragma endregion

#pragma region Minion
// [서버] 보스전 스캔마다 졸개 진행. 플레이어마다 따로 "전부 죽으면 대기 → 다시 소환" 을 돌림.
void ACBBossArena::Auth_UpdateMinions(const TArray<APlayerController*>& InInsidePlayers)
{
	if (!MinionClass) return;

	const float Now = GetWorld()->GetTimeSeconds();

	// 죽었거나 사라진 졸개 정리. 사망 델리게이트 대신 스캔으로 확인 (스포너·보스와 같음).
	// 묶음이 방금 빈 순간부터 재소환 대기를 셈
	for (TPair<TWeakObjectPtr<APlayerController>, FCBArenaMinionSquad>& SquadPair : MinionSquads)
	{
		FCBArenaMinionSquad& Squad = SquadPair.Value;
		if (Squad.Minions.IsEmpty()) continue;

		Squad.Minions.RemoveAll([](const TWeakObjectPtr<ACBBaseCharacter>& InMinion)
		{
			const ACBBaseCharacter* Minion = InMinion.Get();
			return !Minion || Minion->IsDead();
		});

		if (Squad.Minions.IsEmpty())
		{
			Squad.NextSpawnTime = Now + MinionRespawnDelay;
		}
	}

	// 소환 지점이 어느 플레이어 코앞도 아니게 검사할 위치 (구역 안 생존 플레이어는 폰이 있음)
	TArray<FVector> PlayerLocations;
	for (const APlayerController* PlayerController : InInsidePlayers)
	{
		PlayerLocations.Add(PlayerController->GetPawn()->GetActorLocation());
	}

	for (APlayerController* PlayerController : InInsidePlayers)
	{
		FCBArenaMinionSquad* Squad = MinionSquads.Find(PlayerController);

		// 처음 보는 플레이어 - 첫 소환도 재소환과 같은 지연 뒤 (연출이 끝나자마자 몰려오지 않게)
		if (!Squad)
		{
			MinionSquads.Add(PlayerController).NextSpawnTime = Now + MinionRespawnDelay;
			continue;
		}

		// 아직 살아 있는 졸개가 있거나 대기 중이면 다음 스캔으로
		if (!Squad->Minions.IsEmpty() || Now < Squad->NextSpawnTime) continue;

		Auth_SpawnMinionSquad(PlayerController, *Squad, PlayerLocations);
	}
}

// [서버] 한 플레이어 주변에 졸개 소환 + 그 플레이어를 타겟으로 지정.
void ACBBossArena::Auth_SpawnMinionSquad(const APlayerController* InPlayer, FCBArenaMinionSquad& InOutSquad, const TArray<FVector>& InPlayerLocations)
{
	UWorld* World = GetWorld();
	APawn* PlayerPawn = InPlayer ? InPlayer->GetPawn() : nullptr;
	if (!World || !PlayerPawn) return;

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (int32 Index = 0; Index < MinionsPerPlayer; ++Index)
	{
		// 자리를 못 찾으면 이 마리만 건너뜀 (스포너와 같음)
		FVector SpawnLocation;
		if (!Auth_FindMinionSpawnLocation(PlayerLocation, InPlayerLocations, SpawnLocation)) continue;

		// 나타나는 순간부터 쫓을 플레이어를 바라봄
		const float Yaw = (PlayerLocation - SpawnLocation).Rotation().Yaw;

		ACBAICharacter* Minion = World->SpawnActor<ACBAICharacter>(
			MinionClass, MakeGroundedSpawnTransform(SpawnLocation, Yaw, MinionClass), SpawnParams);
		if (!Minion) continue;

		InOutSquad.Minions.Add(Minion);

		// 이 플레이어만 쫓게 지정. AI 컨트롤러는 소환(SpawnActor) 안에서 이미 빙의돼 있음 (AutoPossessAI)
		if (ACBAIController* AIController = Cast<ACBAIController>(Minion->GetController()))
		{
			AIController->Auth_AssignTarget(PlayerPawn);
		}
	}

	// 한 마리도 못 냈으면 대기부터 다시 셈 - 다음 스캔마다 재시도해 지점 탐색 실패 경고가 쌓이지 않게
	if (InOutSquad.Minions.IsEmpty())
	{
		InOutSquad.NextSpawnTime = World->GetTimeSeconds() + MinionRespawnDelay;
	}

	UE_LOG(LogTemp, Log, TEXT("[BossArena][%s] 졸개 %d마리 소환 → %s"), *GetName(), InOutSquad.Minions.Num(), *PlayerPawn->GetName());
}

// [서버] 플레이어 주변 내비메시에서 졸개 소환 지점 탐색.
bool ACBBossArena::Auth_FindMinionSpawnLocation(const FVector& InCenter, const TArray<FVector>& InPlayerLocations, FVector& OutLocation) const
{
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem) return false;

	// 반경의 절반 안쪽은 코앞이라 제외 - 플레이어를 둘러싼 고리 모양으로 나옴
	const float MinDistanceSquared = FMath::Square(MinionSpawnRadius * 0.5f);

	for (int32 Try = 0; Try < MaxMinionSpawnLocationTries; ++Try)
	{
		// 플레이어에게서 걸어갈 수 있는 지점만 뽑아, 벽 너머의 끊긴 내비메시 조각에 소환되는 것을 막음
		FNavLocation NavLocation;
		if (!NavSystem->GetRandomReachablePointInRadius(InCenter, MinionSpawnRadius, NavLocation)) continue;

		// 입구 통로는 내비메시로 이어져 있으므로, 구역 밖(입구 벽 너머)이면 다시 뽑음
		if (!IsInsideArena(NavLocation.Location)) continue;

		// 어느 플레이어든 코앞이면 다시 뽑음
		const bool bTooCloseToPlayer = InPlayerLocations.ContainsByPredicate([&](const FVector& InPlayerLocation)
		{
			return FVector::DistSquared(NavLocation.Location, InPlayerLocation) < MinDistanceSquared;
		});
		if (bTooCloseToPlayer) continue;

		OutLocation = NavLocation.Location;
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[BossArena][%s] 졸개 소환 지점을 찾지 못함 (내비메시·MinionSpawnRadius·구역 박스가 바닥을 덮는지 확인 필요)"), *GetName());
	return false;
}

// [서버] 살아 있는 졸개 전부 정리.
void ACBBossArena::Auth_RemoveMinions(bool bInKill)
{
	for (const TPair<TWeakObjectPtr<APlayerController>, FCBArenaMinionSquad>& SquadPair : MinionSquads)
	{
		for (const TWeakObjectPtr<ACBBaseCharacter>& MinionPtr : SquadPair.Value.Minions)
		{
			// 이미 죽은 졸개는 스스로 시체를 정리하므로 건드리지 않음
			ACBBaseCharacter* Minion = MinionPtr.Get();
			if (!Minion || Minion->IsDead()) continue;

			if (bInKill)
			{
				// 체력이 0 이 됐을 때와 같은 사망 이벤트 - 사망 어빌리티가 몽타주·사망 상태·소멸 연출을 처리
				UAbilitySystemComponent* MinionASC = Minion->GetAbilitySystemComponent();
				if (MinionASC)
				{
					FGameplayEventData Payload;
					Payload.EventTag = CBGameplayTags::Event_Combat_Death;
					Payload.Instigator = this;
					Payload.Target = Minion;

					// 사망 어빌리티가 발동하면 끝. 발동하지 못했으면(준비 전이라 미부여·로드아웃 누락) 아래에서 파괴 -
					// 보스전이 끝난 뒤에도 졸개가 살아서 쫓아오지 않게
					if (MinionASC->HandleGameplayEvent(Payload.EventTag, &Payload) > 0) continue;
				}
			}

			Minion->Destroy();
		}
	}

	MinionSquads.Reset();
}
#pragma endregion

// [서버] 대기 태그를 구역 안 플레이어에게만 붙인 상태로 맞춤.
void ACBBossArena::Auth_UpdateWaitingTags(const TArray<APlayerController*>& InInsidePlayers)
{
	TSet<TWeakObjectPtr<UAbilitySystemComponent>> NewTaggedASCs;

	// 새로 들어온 플레이어에게 붙임. 클라 HUD 가 구독하므로 TagOnly 로 복제
	for (const APlayerController* PlayerController : InInsidePlayers)
	{
		UAbilitySystemComponent* ASC = GetPlayerASC(PlayerController);
		if (!ASC) continue;

		NewTaggedASCs.Add(ASC);
		if (!WaitingTaggedASCs.Contains(ASC))
		{
			ASC->AddLooseGameplayTag(CBGameplayTags::Status_Arena_Waiting, 1, EGameplayTagReplicationState::TagOnly);
		}
	}

	// 빠진 플레이어(나감·사망)에게서 뗌. 접속을 끊어 PlayerState 가 사라진 경우는 뗄 대상이 없음
	for (const TWeakObjectPtr<UAbilitySystemComponent>& TaggedASC : WaitingTaggedASCs)
	{
		UAbilitySystemComponent* ASC = TaggedASC.Get();
		if (ASC && !NewTaggedASCs.Contains(TaggedASC))
		{
			ASC->RemoveLooseGameplayTag(CBGameplayTags::Status_Arena_Waiting, 1, EGameplayTagReplicationState::TagOnly);
		}
	}

	WaitingTaggedASCs = MoveTemp(NewTaggedASCs);
}

// [서버] 접속 인원과 살아서 구역 안에 있는 플레이어를 모음.
int32 ACBBossArena::Auth_CollectPlayersInArena(TArray<APlayerController*>& OutInsidePlayers) const
{
	const UWorld* World = GetWorld();
	if (!World) return 0;

	int32 TotalCount = 0;

	// 서버에는 접속한 모든 플레이어 컨트롤러가 있으므로 여기서 현재 폰을 그때그때 읽음 (리스폰으로 폰이 갈려도 무관)
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!PlayerController) continue;

		++TotalCount;

		// 폰이 없거나(리스폰 대기) 죽었으면 구역 밖으로 침. 시체가 구역 안에 남아도 인원에 들어가지 않음
		const ACBBaseCharacter* PlayerCharacter = Cast<ACBBaseCharacter>(PlayerController->GetPawn());
		if (!PlayerCharacter || PlayerCharacter->IsDead()) continue;

		if (IsInsideArena(PlayerCharacter->GetActorLocation()))
		{
			OutInsidePlayers.Add(PlayerController);
		}
	}
	return TotalCount;
}

bool ACBBossArena::IsInsideArena(const FVector& InLocation) const
{
	// 박스 로컬 공간으로 옮겨 비교 - 박스를 회전·스케일해도 그대로 맞음 (역변환이 스케일까지 되돌리므로 스케일 전 크기와 비교)
	const FVector LocalLocation = ArenaBounds->GetComponentTransform().InverseTransformPosition(InLocation);
	const FVector Extent = ArenaBounds->GetUnscaledBoxExtent();
	return FBox(-Extent, Extent).IsInsideOrOn(LocalLocation);
}

FTransform ACBBossArena::MakeGroundedSpawnTransform(const UArrowComponent* InPoint, TSubclassOf<APawn> InPawnClass)
{
	// 화살표를 기울여 놓아도 캐릭터는 똑바로 서야 하므로 Yaw 만 씀
	return MakeGroundedSpawnTransform(InPoint->GetComponentLocation(), InPoint->GetComponentRotation().Yaw, InPawnClass);
}

FTransform ACBBossArena::MakeGroundedSpawnTransform(const FVector& InGroundLocation, float InYaw, TSubclassOf<APawn> InPawnClass)
{
	FVector Location = InGroundLocation;

	// 바닥 위치라, 캡슐 절반 높이만큼 띄우지 않으면 지면에 박힌 채 스폰됨 (ACBEnemySpawner 와 같은 처리).
	// 템플릿 GetDefaultObject<ACharacter>() 는 캐릭터가 아니면 check 로 멈추므로 Cast 로 거름
	if (const ACharacter* CharacterCDO = InPawnClass ? Cast<ACharacter>(InPawnClass->GetDefaultObject()) : nullptr)
	{
		Location.Z += CharacterCDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}

	return FTransform(FRotator(0.f, InYaw, 0.f), Location);
}
