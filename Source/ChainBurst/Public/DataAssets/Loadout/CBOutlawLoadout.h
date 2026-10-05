#pragma once

#include "CoreMinimal.h"
#include "DataAssets/Loadout/CBAILoadout.h"
#include "CBOutlawLoadout.generated.h"

class UCBBossBarWidget;

/**
 * Outlaw(보스·엘리트) 로드아웃.
 * AI 공통 데이터에 더해 화면 상단 보스 바 위젯 클래스를 담음 (비우면 보스 바 없음 - 엘리트 등급 등).
 */
UCLASS()
class CHAINBURST_API UCBOutlawLoadout : public UCBAILoadout
{
	GENERATED_BODY()

public:
	/** [공용] 전 인스턴스 공용 데이터를 적용하는 함수. 보스 바 위젯 클래스를 UI 컴포넌트에 주입함. */
	virtual void ApplyToCharacter(ACBBaseCharacter* InCharacter) override;

protected:
	/**
	 * 화면 상단 보스 바 위젯 클래스. 비우면 보스 바를 만들지 않음.
	 * UCBUIComponent 에 주입되어, 보스가 교전을 시작하면(Status.Combat.Engaged) 각 클라가 자기 HUD 스택에 올림.
	 * 비우면 보스 바를 만들지 않음.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|UI")
	TSubclassOf<UCBBossBarWidget> BossBarWidgetClass = nullptr;
};
