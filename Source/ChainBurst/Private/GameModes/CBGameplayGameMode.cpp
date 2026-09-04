// project
#include "GameModes/CBGameplayGameMode.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "CBGameplayTags.h"
#include "Core/CBSessionSubsystem.h"
#include "PlayerState/CBPlayerState.h"

// engine
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

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
