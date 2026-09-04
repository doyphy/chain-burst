// project
#include "UI/Widgets/CBPlayerListWidget.h"
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

	// 구독 + 현재 목록 반영. 위젯이 화면에서 빠졌다 돌아와도 여기서 다시 맞춰짐
	BindToGameState();
}

void UCBPlayerListWidget::NativeDestruct()
{
	// 슬레이트가 사라지는 동안은 구독을 끊음 (댕글링 방지)
	UnbindFromGameState();

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

	// 자기 자신을 목록에서 빼기 위한 기준. 아직 확정되지 않았어도 목록 변경 때마다 다시 평가되므로 스스로 복구됨
	const APlayerController* OwningPlayerController = GetOwningPlayer();
	const APlayerState* LocalPlayerState = OwningPlayerController ? OwningPlayerController->PlayerState : nullptr;

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
