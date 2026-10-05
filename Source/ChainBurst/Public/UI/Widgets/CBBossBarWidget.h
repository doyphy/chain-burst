#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CBBossBarWidget.generated.h"

class UCBAbilitySystemComponent;
class UCBHealthBarWidget;
class UCBStunGaugeWidget;

/**
 * 화면 상단 보스 바(체력 + 기절 게이지)의 공용 베이스 (컨테이너).
 * 보스 ASC 하나만 받아 체력바·기절 게이지 자식 위젯을 배선함. 값 구독은 자식 위젯이 각자 담당.
 * 보스의 UCBUIComponent 가 교전 개시(Status.Combat.Engaged) 때 생성해 로컬 플레이어 HUD 스택에 올림.
 * 자식 위젯은 WBP 에서 같은 이름으로 배치하면 자동 연결되고(BindWidgetOptional), 없으면 그 부분만 건너뜀.
 * 보스 이름 등 그 밖의 표시는 WBP 에 직접 둠 (위젯 클래스가 보스별로 로드아웃에 등록되므로).
 */
UCLASS(Abstract)
class CHAINBURST_API UCBBossBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 표시할 보스를 지정하고 자식 위젯을 배선하는 함수.
	 * 재호출 시 자식 위젯이 각자 기존 구독을 해제하므로 여러 번 불려도 안전.
	 * @param InASC 표시할 보스의 ASC
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|UI")
	void InitializeWithASC(UCBAbilitySystemComponent* InASC);

protected:
	/** 체력 표시 자식 위젯. WBP 에 같은 이름으로 두면 자동 연결됨 */
	UPROPERTY(BlueprintReadOnly, Category = "ChainBurst|UI", meta = (BindWidgetOptional))
	TObjectPtr<UCBHealthBarWidget> HealthBarWidget = nullptr;

	/** 기절 게이지 표시 자식 위젯. WBP 에 같은 이름으로 두면 자동 연결됨 */
	UPROPERTY(BlueprintReadOnly, Category = "ChainBurst|UI", meta = (BindWidgetOptional))
	TObjectPtr<UCBStunGaugeWidget> StunGaugeWidget = nullptr;
};
