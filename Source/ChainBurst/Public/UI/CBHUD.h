#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CBHUD.generated.h"

class UUserWidget;

/**
 * 프로젝트 공용 HUD 베이스 클래스.
 * 팩의 위젯 스택 조작은 블루프린트 인터페이스(BPI_EGUI_HUDInterface)로만 가능하므로,
 * C++에서 호출할 수 있도록 BP가 구현할 이벤트를 선언해 다리 역할만 함.
 * 화면에 띄우는 위젯은 예외 없이 이 스택을 거쳐야 함.
 */
UCLASS()
class CHAINBURST_API ACBHUD : public AHUD
{
	GENERATED_BODY()

public:
	/**
	 * 위젯을 HUD 스택의 Game 레이어에 삽입하도록 요청하는 이벤트.
	 * (BP가 Insert Widget Instance in Stack으로 구현)
	 * @param Widget 삽입할 위젯 인스턴스
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void PushGameLayerWidget(UUserWidget* Widget);

	/**
	 * 위젯을 HUD 스택에서 제거하도록 요청하는 이벤트.
	 * (BP가 Remove Widget Instance from Stack으로 구현)
	 * @param Widget 제거할 위젯 인스턴스
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void PopWidgetFromStack(UUserWidget* Widget);
};
