// project
#include "UI/Widgets/CBPlayerListWidget.h"
#include "Controllers/CBChaserController.h"
#include "GameStates/CBGameStateBase.h"
#include "PlayerState/CBPlayerState.h"
#include "UI/Widgets/CBPlayerListEntryWidget.h"

// engine
#include "Components/PanelWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

void UCBPlayerListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 로컬 PlayerState 확정 신호 구독. 목록 변경 방송보다 늦게 오는 경우를 여기서 받음
	BindToOwningController();

	// 구독 + 현재 목록 반영. 위젯이 화면에서 빠졌다 돌아와도 여기서 다시 맞춰짐
	BindToGameState();
}

void UCBPlayerListWidget::NativeDestruct()
{
	// 슬레이트가 사라지는 동안은 구독을 끊음 (댕글링 방지)
	UnbindFromGameState();
	UnbindFromOwningController();

	Super::NativeDestruct();
}

void UCBPlayerListWidget::BindToGameState()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 재호출·재구성 대비: 기존 구독이 있으면 먼저 해제
	UnbindFromGameState();

	ACBGameStateBase* CBGameState = World->GetGameState<ACBGameStateBase>();
	if (!CBGameState)
	{
		// 게임 스테이트 복제가 위젯 생성보다 늦은 경우. 도착 시점을 기다렸다가 다시 시도함
		GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &UCBPlayerListWidget::HandleGameStateSet);
		return;
	}

	CachedGameState = CBGameState;

	// 목록 변경 구독 (다이내믹이라 중복 구독은 AddUnique 로 방지)
	CBGameState->OnPlayerListChanged.AddUniqueDynamic(this, &UCBPlayerListWidget::HandlePlayerListChanged);

	// 구독 전에 이미 들어와 있는 플레이어를 반영 (델리게이트는 '바뀔 때'만 오므로)
	Local_RefreshEntries();
}

void UCBPlayerListWidget::UnbindFromGameState()
{
	if (ACBGameStateBase* CBGameState = CachedGameState.Get())
	{
		CBGameState->OnPlayerListChanged.RemoveDynamic(this, &UCBPlayerListWidget::HandlePlayerListChanged);
	}
	CachedGameState.Reset();

	// 게임 스테이트 도착을 기다리는 중이었으면 그 구독도 해제
	if (UWorld* World = GetWorld(); World && GameStateSetHandle.IsValid())
	{
		World->GameStateSetEvent.Remove(GameStateSetHandle);
	}
	GameStateSetHandle.Reset();
}

// 소유 컨트롤러의 PlayerState 확정 신호를 구독.
void UCBPlayerListWidget::BindToOwningController()
{
	// 재호출·재구성 대비: 기존 구독이 있으면 먼저 해제
	UnbindFromOwningController();

	ACBChaserController* OwningController = Cast<ACBChaserController>(GetOwningPlayer());
	if (!OwningController) return;

	CachedOwningController = OwningController;

	// 다이내믹이라 중복 구독은 AddUnique 로 방지
	OwningController->OnLocalPlayerStateSet.AddUniqueDynamic(this, &UCBPlayerListWidget::HandleLocalPlayerStateSet);
}

void UCBPlayerListWidget::UnbindFromOwningController()
{
	if (ACBChaserController* OwningController = CachedOwningController.Get())
	{
		OwningController->OnLocalPlayerStateSet.RemoveDynamic(this, &UCBPlayerListWidget::HandleLocalPlayerStateSet);
	}
	CachedOwningController.Reset();
}

// 로컬 PlayerState 가 확정됐으면 자기 자신을 가려낼 수 있으므로 목록을 다시 만듦.
void UCBPlayerListWidget::HandleLocalPlayerStateSet()
{
	Local_RefreshEntries();
}

// 게임 스테이트가 늦게 도착한 경우. 이제 구독할 수 있음
void UCBPlayerListWidget::HandleGameStateSet(AGameStateBase* /*InGameState*/)
{
	BindToGameState();
}

// 게임 스테이트의 목록 변경 신호가 오면 행을 다시 만듦.
void UCBPlayerListWidget::HandlePlayerListChanged()
{
	Local_RefreshEntries();
}

// 현재 PlayerArray 로 행을 다시 만듦.
void UCBPlayerListWidget::Local_RefreshEntries()
{
	if (!EntryContainer || !EntryWidgetClass) return;

	const ACBGameStateBase* CBGameState = CachedGameState.Get();
	if (!CBGameState) return;

	// 이전 행을 모두 비움 (행 위젯의 구독은 각자 NativeDestruct 에서 정리됨)
	EntryContainer->ClearChildren();

	// 로컬 플레이어 스테이트 가져오기 (로컬 판정용)
	const APlayerController* OwningPlayerController = GetOwningPlayer();
	const APlayerState* LocalPlayerState = OwningPlayerController ? OwningPlayerController->PlayerState : nullptr;
	// PC 의 PlayerState 가 복제되기 전에 위젯이 먼저 생성될 수 있으므로, 아직 없으면 목록을 만들지 않음
	// PC 의 PlayerState 가 복제되면 OnLocalPlayerStateSet 신호가 오므로, 그때 목록을 다시 만듦
	if (!LocalPlayerState) return;

	for (APlayerState* PlayerState : CBGameState->PlayerArray)
	{
		if (!PlayerState) continue;

		// 자기 자신은 HUD 의 자기 체력·이름이 따로 있으므로 목록에서 제외
		if (PlayerState == LocalPlayerState) continue;

		// 봇은 사람 플레이어 목록에 올리지 않음 (지금은 AI 에 PlayerState 가 없어 걸리지 않음)
		if (PlayerState->IsABot()) continue;

		// 닉네임·ASC 를 모두 가진 대상만 행으로 만듦
		ACBPlayerState* CBPlayerState = Cast<ACBPlayerState>(PlayerState);
		if (!CBPlayerState) continue;

		// 행 위젯 생성
		UCBPlayerListEntryWidget* EntryWidget = CreateWidget<UCBPlayerListEntryWidget>(GetOwningPlayer(), EntryWidgetClass);
		if (!EntryWidget) continue;

		// 행 위젯 초기화
		EntryWidget->InitializeWithPlayerState(CBPlayerState);
		
		// 패널에 추가.
		EntryContainer->AddChild(EntryWidget);
	}
}
