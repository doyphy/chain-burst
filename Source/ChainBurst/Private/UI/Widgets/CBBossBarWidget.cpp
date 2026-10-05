// project
#include "UI/Widgets/CBBossBarWidget.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "UI/Widgets/CBHealthBarWidget.h"
#include "UI/Widgets/CBStunGaugeWidget.h"

void UCBBossBarWidget::InitializeWithASC(UCBAbilitySystemComponent* InASC)
{
	// ASC 유효성 검사
	if (!InASC) return;

	// 체력 배선 (CurrentHealth/MaxHealth 복제)
	if (HealthBarWidget)
	{
		HealthBarWidget->InitializeWithASC(InASC);
	}

	// 기절 게이지 배선 (StunGauge 복제)
	if (StunGaugeWidget)
	{
		StunGaugeWidget->InitializeWithASC(InASC);
	}
}
