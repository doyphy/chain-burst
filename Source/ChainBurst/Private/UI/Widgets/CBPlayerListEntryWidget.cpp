// project
#include "UI/Widgets/CBPlayerListEntryWidget.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "PlayerState/CBPlayerState.h"
#include "UI/Widgets/CBHealthBarWidget.h"
#include "UI/Widgets/CBNamePlateWidget.h"

void UCBPlayerListEntryWidget::InitializeWithPlayerState(APlayerState* InPlayerState)
{
	// 닉네임·ASC 모두 ACBPlayerState 가 들고 있으므로 그 타입일 때만 대상으로 삼음
	ACBPlayerState* CBPlayerState = Cast<ACBPlayerState>(InPlayerState);
	if (!CBPlayerState) return;

	CachedPlayerState = CBPlayerState;

	// 닉네임 배선. 목록에는 남만 올라가므로 로컬 대상 플래그는 항상 false
	if (NamePlateWidget)
	{
		NamePlateWidget->InitializeWithPlayerState(CBPlayerState, false);
	}

	// 체력 배선. ASC 가 PlayerState 소유라 남의 폰을 찾지 않아도 됨
	if (HealthBarWidget)
	{
		HealthBarWidget->InitializeWithASC(CBPlayerState->GetCBAbilitySystemComponent());
	}

	// 그 밖의 표시는 WBP 에 위임
	OnPlayerStateSet(CBPlayerState);
}

ACBPlayerState* UCBPlayerListEntryWidget::GetTargetPlayerState() const
{
	return CachedPlayerState.Get();
}
