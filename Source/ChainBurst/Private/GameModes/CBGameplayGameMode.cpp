// project
#include "GameModes/CBGameplayGameMode.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"
#include "Controllers/CBChaserController.h"
#include "Core/CBGameInstance.h"
#include "Core/CBSessionSubsystem.h"
#include "DataAssets/LevelUp/CBLevelUpData.h"
#include "GameStates/CBGameplayGameState.h"
#include "PlayerState/CBPlayerState.h"

// engine
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

ACBGameplayGameMode::ACBGameplayGameMode()
{
	// 공유 레벨·경험치를 복제할 게임 스테이트. 누락 시 경험치가 쌓이지 않으므로 C++ 에서 지정.
	GameStateClass = ACBGameplayGameState::StaticClass();
}

// [서버] 월드 밖에서 도는 티커라 게임모드와 함께 사라지지 않으므로 직접 해제
void ACBGameplayGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LevelUpTimeoutHandle.IsValid())
	{
		FTSTicker::RemoveTicker(LevelUpTimeoutHandle);
		LevelUpTimeoutHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

// [서버] 접속 승인. 진행 중인 매치로의 난입을 거부함
void ACBGameplayGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	// 정원 판정 등 엔진의 기본 승인 절차를 먼저 태움
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	// 이미 거부됐으면 그 사유를 덮어쓰지 않음
	if (!ErrorMessage.IsEmpty()) return;

	// 난입을 허용하도록 설정했으면 그대로 받음
	if (bAllowJoinInProgress) return;

	// 진행 중인 매치로의 난입을 거부함.
	// ErrorMessage 에 문자열을 넣으면 다음 단계에서 엔진이 에러를 표시하고 접속을 끊음
	ErrorMessage = UCBSessionSubsystem::MatchInProgressError;

	UE_LOG(LogTemp, Log, TEXT("[Gameplay] 진행 중인 매치로의 접속을 거부함: %s"), *Address);
}

// [서버] 나간 플레이어의 리스폰 예약을 취소. 사라진 컨트롤러를 뒤늦게 다시 스폰하지 않도록 함.
void ACBGameplayGameMode::Logout(AController* Exiting)
{
	if (FTimerHandle* RespawnTimer = RespawnTimers.Find(Exiting))
	{
		GetWorldTimerManager().ClearTimer(*RespawnTimer);
		RespawnTimers.Remove(Exiting);
	}

	// 카드를 기다리던 플레이어가 나가면 명단에서 뺌. 마지막 대기자였으면 레벨업을 끝냄 (안 그러면 정지가 풀리지 않음)
	if ( PendingCardOffers.Remove(Cast<APlayerController>(Exiting)) > 0 && PendingCardOffers.IsEmpty() )
	{
		Auth_FinishLevelUpRound();
	}

	Super::Logout(Exiting);
}

// [서버] 플레이어 사망 통지(ACBChaserCharacter::Auth_OnDeath). 지연 후 다시 스폰하도록 예약.
void ACBGameplayGameMode::Auth_HandlePlayerDeath(AController* InController)
{
	if (!InController) return;

	// 사망 처리가 두 번 들어와도 예약은 하나만 유지
	if (RespawnTimers.Contains(InController)) return;

	// 지연이 없으면 곧바로 처리
	if (RespawnDelay <= 0.f)
	{
		Auth_RespawnPlayer(InController);
		return;
	}

	// 대기 중 컨트롤러가 사라질 수 있으므로 약참조로 넘김
	const TWeakObjectPtr<AController> WeakController(InController);
	FTimerHandle& RespawnTimer = RespawnTimers.Add(InController);

	// 리스폰 타이머 예약. 만료 시 Auth_RespawnPlayer 호출
	GetWorldTimerManager().SetTimer(RespawnTimer, FTimerDelegate::CreateWeakLambda(this, [this, WeakController]()
	{
		Auth_RespawnPlayer(WeakController.Get());
	}), RespawnDelay, false);
}

// [서버] 예약 만료 시 실제 리스폰. 사망 상태를 걷어내고 폰을 새로 스폰.
void ACBGameplayGameMode::Auth_RespawnPlayer(AController* InController)
{
	// 컨트롤러가 사라졌더라도 예약 항목은 남지 않게 먼저 지움
	RespawnTimers.Remove(InController);

	APlayerController* PlayerController = Cast<APlayerController>(InController);
	if (!PlayerController) return;

	// ASC 는 PlayerState 소유라 폰을 갈아도 살아남음. 사망 상태를 먼저 걷어내야 새 폰이 죽은 채로 태어나지 않음
	Auth_ClearDeathState(PlayerController);

	// 시체를 파괴. 폰이 남아 있으면 RestartPlayer 가 그 폰을 그대로 재사용함.
	// Destroy() 로 Pawn::EndPlay 가 돌면서 무기 액터와 HUD 위젯도 함께 정리됨
	if (APawn* OldPawn = PlayerController->GetPawn())
	{
		PlayerController->UnPossess(); // 빙의 해제. Destroy() 전에 호출해야 함.
		OldPawn->Destroy();
	}

	// 스폰 지점은 엔진 기본 규칙(ChoosePlayerStart)이 PlayerStart 중에서 고름.
	// 스폰할 클래스는 GetDefaultPawnClassForController 가 PlayerState 의 선택 캐릭터를 보고 정함
	RestartPlayer(PlayerController);
}

// [서버] 사망 상태 정리. 사망 GE 제거 + 이전 폰의 로드아웃 부여분 회수.
void ACBGameplayGameMode::Auth_ClearDeathState(const AController* InController) const
{
	const ACBPlayerState* CBPlayerState = InController ? InController->GetPlayerState<ACBPlayerState>() : nullptr;
	if (!CBPlayerState) return;

	UCBAbilitySystemComponent* CBASC = CBPlayerState->GetCBAbilitySystemComponent();
	if (!CBASC) return;

	// 사망 GE 는 사망 어빌리티 BP 가 지정하므로 게임모드가 클래스를 알 수 없음. 부여 태그로 지움
	CBASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(CBGameplayTags::Status_Dead));

	// 이전 폰의 로드아웃 부여분 회수. 남겨두면 새 폰이 다시 부여하는 분량과 겹침 (로비의 캐릭터 변경과 같은 처리)
	CBASC->Auth_ClearLoadoutGrants();
}

// [서버] 공유 경험치 누적. 넘친 만큼 레벨을 올리고 레벨업을 시작함 (AI가 죽으면 ACBAICharacter::Auth_OnDeath 에서 호출)
void ACBGameplayGameMode::Auth_AddExperience(int32 InAmount)
{
	if (InAmount <= 0) return;

	// 레벨업 데이터 가져오기.
	ACBGameplayGameState* GameplayGameState = GetGameState<ACBGameplayGameState>();
	const UCBLevelUpData* LevelUpData = GetLevelUpData();
	if (!GameplayGameState || !LevelUpData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Gameplay] 게임 스테이트 또는 레벨업 데이터가 없어 경험치를 쌓지 못함 (게임 인스턴스의 LevelUpData 확인)"));
		return;
	}

	// 현재 레벨과 경험치 가져오기.
	int32 Level = GameplayGameState->GetCurrentLevel();

	// 최대 레벨이면 더 쌓지 않음
	if (LevelUpData->IsMaxLevel(Level)) return;

	int32 Experience = GameplayGameState->GetExperience() + InAmount;

	// 한 번에 여러 레벨을 넘을 수 있으므로 넘은 만큼 반복 (필요량은 최소 1 이라 반드시 끝남). 최대 레벨에서 멈춤
	int32 GainedLevels = 0;
	for (int32 RequiredExperience = LevelUpData->GetRequiredExperience(Level);
		!LevelUpData->IsMaxLevel(Level) && Experience >= RequiredExperience;
		RequiredExperience = LevelUpData->GetRequiredExperience(Level))
	{
		Experience -= RequiredExperience;
		++Level;
		++GainedLevels;
	}

	// 최대 레벨에 닿았으면 남은 경험치는 버림 (더 오를 레벨이 없음)
	if (LevelUpData->IsMaxLevel(Level))
	{
		Experience = 0;
	}

	// 게임 스테이트에 새 레벨과 경험치 기록. 클라이언트로 복제됨
	GameplayGameState->Auth_SetExperience(Level, Experience);

	// 레벨업이 없으면 끝
	if (GainedLevels == 0) return;
	
	// 레벨업이 있으면 남은 레벨업 수를 늘리고, 진행 중인 레벨업이 없으면 바로 시작
	PendingLevelUps += GainedLevels;

	// 진행 중인 레벨업이 있으면 그게 끝날 때 이어서 시작됨
	if (!bLevelUpRoundActive)
	{
		Auth_StartLevelUpRound();
	}
}

// [서버] 레벨업 하나 시작. 플레이어마다 카드를 따로 뽑아 본인에게만 보내고 월드를 정지함
void ACBGameplayGameMode::Auth_StartLevelUpRound()
{
	// 레벨업 데이터가 없거나 남은 레벨업이 없으면 무시
	const UCBLevelUpData* LevelUpData = GetLevelUpData();
	if (!LevelUpData || PendingLevelUps <= 0) return;
	
	--PendingLevelUps;
	bLevelUpRoundActive = true;
	PendingCardOffers.Reset();

	// 플레이어마다 카드를 뽑아 본인에게만 보냄. 정지 중에는 속성 복제가 멈추므로 RPC 로 보냄
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		ACBChaserController* ChaserController = Cast<ACBChaserController>(Iterator->Get());
		if (!ChaserController) continue;

		// 카드 인덱스 뽑기.
		TArray<int32> CardIndices;
		LevelUpData->RollCards(CardIndices);
		if (CardIndices.IsEmpty()) continue;

		// 서버에서 검증용으로 기억해 둠 (클라이언트가 잘못된 카드 인덱스를 보내는지 확인)
		PendingCardOffers.Add(ChaserController, CardIndices);
		// 정지 중에는 속성 복제가 멈추므로 RPC 로 보냄
		ChaserController->Client_OfferLevelUpCards(CardIndices);
	}

	// 고를 카드가 없으면(카드 목록이 비었거나 효과가 전부 비었음) 정지하지 않고 남은 레벨업까지 버림
	if (PendingCardOffers.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Gameplay] 뽑을 수 있는 레벨업 카드가 없어 레벨업을 건너뜀 (레벨업 데이터의 Cards 확인)"));
		PendingLevelUps = 0;
		bLevelUpRoundActive = false;
		return;
	}

	// 레벨업 정지.
	// 이미 정지 중이면(레벨업이 이어지는 경우) 그대로 유지됨
	Auth_SetLevelUpPause(true);

	// 정지 중에는 월드 타이머가 멈추므로 실제 시간으로 도는 코어 티커로 타임아웃을 검
	LevelUpTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ACBGameplayGameMode::HandleLevelUpTimeout), LevelUpData->GetSelectionTimeout());
}

// [서버 검증] 플레이어의 카드 선택. 제시한 카드인지 확인 후 확정 (CBChaserController::Client_SelectLevelUpCard RPC 로 호출됨)
void ACBGameplayGameMode::Auth_SelectLevelUpCard(APlayerController* InPlayerController, int32 InSlot)
{
	// 해당 플레이어의 제시 카드 인덱스 배열을 가져옴. 없으면 이미 고른 것임
	const TArray<int32>* OfferedCardIndices = PendingCardOffers.Find(InPlayerController);
	if (!OfferedCardIndices) return;

	// 제시한 카드 슬롯이 유효하지 않으면 무시. 클라이언트가 잘못된 인덱스를 보내는지 검증
	if (!OfferedCardIndices->IsValidIndex(InSlot))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Gameplay] %s 의 잘못된 카드 슬롯 요청: %d"), *GetNameSafe(InPlayerController), InSlot);
		return;
	}

	// 확정하면서 명단에서 지우므로 인덱스를 먼저 꺼내 둠
	const int32 CardIndex = (*OfferedCardIndices)[InSlot];
	Auth_CompleteCardSelection(InPlayerController, CardIndex);
}

// [서버] 선택 확정. 효과를 적용하고, 마지막 대기자였으면 레벨업을 끝냄
void ACBGameplayGameMode::Auth_CompleteCardSelection(APlayerController* InPlayerController, int32 InCardIndex)
{
	// 선택한 플레이어를 명단에서 지움. 비면 레벨업을 끝냄
	PendingCardOffers.Remove(InPlayerController);
	
	// 카드 효과 적용. ASC 는 PlayerState 소유라 폰이 없는 순간(리스폰 대기)에도 적용됨
	Auth_ApplyLevelUpCard(InPlayerController, InCardIndex);

	// 마지막 대기자였으면 레벨업을 끝냄. 남은 레벨업이 있으면 이어서 시작됨
	if (PendingCardOffers.IsEmpty())
	{
		Auth_FinishLevelUpRound();
	}
}

// [서버] 카드 효과 적용. 수치는 SetByCaller(Data.LevelUp)로 넘김
void ACBGameplayGameMode::Auth_ApplyLevelUpCard(const APlayerController* InPlayerController, int32 InCardIndex) const
{
	// 레벨업 데이터와 카드, 플레이어 ASC 가져오기
	const UCBLevelUpData* LevelUpData = GetLevelUpData();
	const FCBLevelUpCard* Card = LevelUpData ? LevelUpData->FindCard(InCardIndex) : nullptr;
	ACBPlayerState* CBPlayerState = InPlayerController ? InPlayerController->GetPlayerState<ACBPlayerState>() : nullptr;
	if (!Card || !CBPlayerState) return;

	// ASC 는 PlayerState 소유라 폰이 없는 순간(리스폰 대기)에도 적용됨
	const FGameplayEffectSpecHandle SpecHandle = UCBAbilitySystemLibrary::NativeMakeEffectSpecHandle(Card->Effect, CBPlayerState);
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Gameplay] 레벨업 카드 %d 의 효과 스펙을 만들지 못함"), InCardIndex);
		return;
	}

	// 카드 효과에 수치를 SetByCaller(Data.LevelUp)로 넘김. Magnitude 는 레벨업 카드에서 지정한 값
	SpecHandle.Data->SetSetByCallerMagnitude(CBGameplayTags::Data_LevelUp, Card->Magnitude);

	// 로드아웃 부여 기록(Auth_ApplyLoadoutEffect)을 거치지 않으므로 무기 변경·사망 정리에 회수되지 않음
	UCBAbilitySystemLibrary::NativeApplyEffectSpecHandleToTarget(CBPlayerState, SpecHandle);
}

// [서버] 레벨업 하나 종료. 남은 레벨업이 있으면 정지를 유지한 채 이어감
void ACBGameplayGameMode::Auth_FinishLevelUpRound()
{
	// 전원이 제한 시간 전에 골랐으면 타임아웃 해제
	if (LevelUpTimeoutHandle.IsValid())
	{
		FTSTicker::RemoveTicker(LevelUpTimeoutHandle);
		LevelUpTimeoutHandle.Reset();
	}

	PendingCardOffers.Reset();
	bLevelUpRoundActive = false;

	// 한 번에 여러 레벨이 올랐으면 재개하지 않고 다음 레벨업으로 (카드 창은 새 카드로 바뀜)
	if (PendingLevelUps > 0)
	{
		Auth_StartLevelUpRound();
		return;
	}

	// 전원의 카드 창을 내리고 재개
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (ACBChaserController* ChaserController = Cast<ACBChaserController>(Iterator->Get()))
		{
			ChaserController->Client_EndLevelUp();
		}
	}

	Auth_SetLevelUpPause(false);
}

// [서버] 선택 타임아웃. 아직 고르지 않은 플레이어는 받은 카드 중 하나를 무작위로 적용
bool ACBGameplayGameMode::HandleLevelUpTimeout(float /*DeltaTime*/)
{
	// 이미 실행된 티커라 종료 처리에서 다시 해제하지 않게 핸들을 먼저 비움
	LevelUpTimeoutHandle.Reset();

	for (const TPair<TWeakObjectPtr<APlayerController>, TArray<int32>>& PendingOffer : PendingCardOffers)
	{
		const APlayerController* PlayerController = PendingOffer.Key.Get();
		const TArray<int32>& OfferedCardIndices = PendingOffer.Value;
		if (!PlayerController || OfferedCardIndices.IsEmpty()) continue;

		Auth_ApplyLevelUpCard(PlayerController, OfferedCardIndices[FMath::RandRange(0, OfferedCardIndices.Num() - 1)]);
	}

	Auth_FinishLevelUpRound();

	// 1회만 실행
	return false;
}

// [서버] 정지는 호스트 컨트롤러로 검. 호스트는 세션이 끝날 때까지 나가지 않으므로 정지 주체(PauserPlayerState)가 사라지지 않음
void ACBGameplayGameMode::Auth_SetLevelUpPause(bool bInPause)
{
	APlayerController* HostController = GEngine ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	if (!HostController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Gameplay] 호스트 컨트롤러가 없어 레벨업 정지를 %s 못함"), bInPause ? TEXT("걸지") : TEXT("풀지"));
		return;
	}

	// PlayerController::SetPause 는 WorldSettings 의 즉시 송출(ForceNetUpdate)까지 해 줌.
	// 게임모드의 SetPause 만 부르면 정지 직후 서버 시간이 멈춰 정지 신호가 클라이언트로 나가지 않을 수 있음
	if (bInPause)
	{
		HostController->SetPause(true, FCanUnpause::CreateUObject(this, &ACBGameplayGameMode::CanUnpauseForLevelUp));
	}
	else
	{
		HostController->SetPause(false);
	}
}

// [서버] 정지 해제 조건. 레벨업이 남아 있는 동안은 다른 해제 요청(콘솔 pause 등)이 와도 풀리지 않음
bool ACBGameplayGameMode::CanUnpauseForLevelUp() const
{
	return !bLevelUpRoundActive && PendingLevelUps == 0;
}

const UCBLevelUpData* ACBGameplayGameMode::GetLevelUpData() const
{
	const UCBGameInstance* CBGameInstance = GetGameInstance<UCBGameInstance>();
	return CBGameInstance ? CBGameInstance->GetLevelUpData() : nullptr;
}
