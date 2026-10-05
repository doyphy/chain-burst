// project
#include "DataAssets/Loadout/CBOutlawLoadout.h"
#include "Characters/CBBaseCharacter.h"
#include "Components/UI/CBUIComponent.h"
#include "UI/Widgets/CBBossBarWidget.h"

void UCBOutlawLoadout::ApplyToCharacter(ACBBaseCharacter* InCharacter)
{
	Super::ApplyToCharacter(InCharacter);

	// 캐릭터 유효성 검사
	if (!InCharacter) return;

	// 보스 바 위젯 클래스를 UI 컴포넌트에 주입 (위젯 생성은 교전 개시 후 UI 컴포넌트가 수행)
	if (UCBUIComponent* UIComponent = InCharacter->GetCBUIComponent())
	{
		UIComponent->SetBossBarWidgetClass(BossBarWidgetClass);
	}
}
