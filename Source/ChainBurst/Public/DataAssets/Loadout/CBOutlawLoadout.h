#pragma once

#include "CoreMinimal.h"
#include "DataAssets/Loadout/CBAILoadout.h"
#include "CBOutlawLoadout.generated.h"

class UCBBossBarWidget;
class USoundBase;

/**
 * Outlaw(보스·엘리트) 로드아웃.
 * AI 공통 데이터에 더해 화면 상단 보스 바 위젯 클래스와 보스 BGM 을 담음 (비우면 각각 없음 - 엘리트 등급 등).
 */
UCLASS()
class CHAINBURST_API UCBOutlawLoadout : public UCBAILoadout
{
	GENERATED_BODY()

public:
	/** [공용] 전 인스턴스 공용 데이터를 적용하는 함수. 보스 바 위젯 클래스를 UI 컴포넌트에, 보스 BGM 을 캐릭터에 주입함. */
	virtual void ApplyToCharacter(ACBBaseCharacter* InCharacter) override;

protected:
	/**
	 * 화면 상단 보스 바 위젯 클래스. 비우면 보스 바를 만들지 않음.
	 * UCBUIComponent 에 주입되어, 보스가 교전을 시작하면(Status.Combat.Engaged) 각 클라가 자기 HUD 스택에 올림.
	 * 비우면 보스 바를 만들지 않음.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|UI")
	TSubclassOf<UCBBossBarWidget> BossBarWidgetClass = nullptr;

	/**
	 * 보스 BGM. 비우면 BGM 을 바꾸지 않음.
	 * 캐릭터에 주입되어, 각 머신에서 보스가 준비되는 순간(소환 = 등장 연출) 레벨 BGM 대신 틀고 사망·파괴 때 되돌림.
	 * 사운드 에셋은 Sound Class 를 옵션 메뉴 Music 클래스로, Looping 을 켤 것.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|Audio")
	TObjectPtr<USoundBase> BossMusic = nullptr;
};
