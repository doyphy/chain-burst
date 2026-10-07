// project
#include "UI/Widgets/CBExperienceBarWidget.h"
#include "Core/CBGameInstance.h"
#include "DataAssets/LevelUp/CBLevelUpData.h"
#include "GameStates/CBGameplayGameState.h"

// engine
#include "Engine/World.h"

void UCBExperienceBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 구독 + 현재 값 반영. 위젯이 다시 만들어져도(리스폰) 여기서 맞춰짐
	BindToGameState();
}

void UCBExperienceBarWidget::NativeDestruct()
{
	// 슬레이트가 사라지는 동안은 구독을 끊음 (댕글링 방지)
	UnbindFromGameState();

	Super::NativeDestruct();
}

void UCBExperienceBarWidget::BindToGameState()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 재호출·재구성 대비: 기존 구독이 있으면 먼저 해제
	UnbindFromGameState();

	AGameStateBase* GameState = World->GetGameState();
	if (!GameState)
	{
		// 게임 스테이트 복제가 위젯 생성보다 늦은 경우. 도착 시점을 기다렸다가 다시 시도함
		GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &UCBExperienceBarWidget::HandleGameStateSet);
		return;
	}

	// 게임플레이 레벨이 아니면(로비 등) 경험치가 없음
	ACBGameplayGameState* GameplayGameState = Cast<ACBGameplayGameState>(GameState);
	if (!GameplayGameState) return;

	CachedGameState = GameplayGameState;

	// 변경 구독 (다이내믹이라 중복 구독은 AddUnique 로 방지)
	GameplayGameState->OnExperienceChanged.AddUniqueDynamic(this, &UCBExperienceBarWidget::HandleExperienceChanged);

	// 구독 전에 이미 확정된 값 반영 (델리게이트는 '바뀔 때'만 오므로)
	BroadcastExperience(GameplayGameState->GetCurrentLevel(), GameplayGameState->GetExperience());
}

void UCBExperienceBarWidget::UnbindFromGameState()
{
	if (ACBGameplayGameState* GameplayGameState = CachedGameState.Get())
	{
		GameplayGameState->OnExperienceChanged.RemoveDynamic(this, &UCBExperienceBarWidget::HandleExperienceChanged);
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
void UCBExperienceBarWidget::HandleGameStateSet(AGameStateBase* /*InGameState*/)
{
	BindToGameState();
}

void UCBExperienceBarWidget::HandleExperienceChanged(int32 InLevel, int32 InExperience)
{
	BroadcastExperience(InLevel, InExperience);
}

void UCBExperienceBarWidget::BroadcastExperience(int32 InLevel, int32 InExperience)
{
	// 필요량은 복제하지 않고 서버와 같은 데이터에서 계산함
	const UCBGameInstance* CBGameInstance = GetGameInstance<UCBGameInstance>();
	const UCBLevelUpData* LevelUpData = CBGameInstance ? CBGameInstance->GetLevelUpData() : nullptr;
	const int32 RequiredExperience = LevelUpData ? LevelUpData->GetRequiredExperience(InLevel) : 0;

	// 최대 레벨이면 더 쌓이지 않으므로 바를 가득 찬 상태로 보여줌
	const bool bIsMaxLevel = LevelUpData && LevelUpData->IsMaxLevel(InLevel);
	OnExperienceChanged(InLevel, bIsMaxLevel ? RequiredExperience : InExperience, RequiredExperience);
}
