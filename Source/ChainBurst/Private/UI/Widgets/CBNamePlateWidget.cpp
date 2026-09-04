// project
#include "UI/Widgets/CBNamePlateWidget.h"
#include "PlayerState/CBPlayerState.h"

void UCBNamePlateWidget::InitializeWithPlayerState(APlayerState* InPlayerState, bool bInIsLocalTarget)
{
	// 닉네임 신호는 ACBPlayerState 가 가지므로 그 타입일 때만 대상으로 삼음
	ACBPlayerState* CBPlayerState = Cast<ACBPlayerState>(InPlayerState);
	if (!CBPlayerState) return;

	// 이 위젯이 로컬 플레이어의 소유인지는 UI 컴포넌트가 폰을 보고 판정해 넘겨줌.
	bIsLocalTarget = bInIsLocalTarget;

	// 이 위젯을 소유한 캐릭터의 PlayerState
	// 값 조회·재구독용 대상 캐시 (위젯이 화면에서 빠졌다 돌아와도 유지)
	CachedPlayerState = CBPlayerState;

	// 구독 + 현재 이름 반영
	BindToPlayerState();
}

FString UCBNamePlateWidget::GetNickname() const
{
	// 대상이 없으면 표시할 이름도 없음
	const ACBPlayerState* CBPlayerState = CachedPlayerState.Get();
	return CBPlayerState ? CBPlayerState->GetPlayerName() : FString();
}

ACBPlayerState* UCBNamePlateWidget::GetTargetPlayerState() const
{
	return CachedPlayerState.Get();
}

void UCBNamePlateWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 위젯 컴포넌트를 숨기면 위젯이 스크린 레이어에서 제거되며 NativeDestruct를 호출해 구독을 끊음.
	// 다시 붙을 때 여기서 재구독하고, 숨어 있는 동안 바뀐 이름을 한 번에 반영함.
	BindToPlayerState();
}

void UCBNamePlateWidget::NativeDestruct()
{
	// 슬레이트가 사라지는 동안은 구독을 끊음 (댕글링 방지). 대상 캐시는 남겨 재구성 때 다시 구독함
	UnbindFromPlayerState();

	Super::NativeDestruct();
}

void UCBNamePlateWidget::BindToPlayerState()
{
	// 대상이 아직 없거나 이미 사라졌으면 할 일 없음
	ACBPlayerState* CBPlayerState = CachedPlayerState.Get();
	if (!CBPlayerState) return;

	// 닉네임 변경 델리게이트 구독 (다이내믹이라 중복 구독은 AddUnique 로 방지)
	CBPlayerState->OnPlayerNicknameChanged.AddUniqueDynamic(this, &UCBNamePlateWidget::HandleNicknameChanged);

	// 구독 전에 이미 확정된 이름 반영 (델리게이트는 '바뀔 때'만 오므로 현재 값은 직접 읽어야 함)
	OnNicknameChanged(CBPlayerState->GetPlayerName());
}

void UCBNamePlateWidget::UnbindFromPlayerState()
{
	// 구독 중이던 델리게이트 해제 (대상 캐시는 재구독 대상이므로 유지)
	if (ACBPlayerState* CBPlayerState = CachedPlayerState.Get())
	{
		CBPlayerState->OnPlayerNicknameChanged.RemoveDynamic(this, &UCBNamePlateWidget::HandleNicknameChanged);
	}
}

void UCBNamePlateWidget::HandleNicknameChanged(const FString& Nickname)
{
	// 바뀐 이름을 그대로 BP 이벤트로 전달
	OnNicknameChanged(Nickname);
}
