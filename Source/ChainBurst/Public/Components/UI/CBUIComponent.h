#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "CBUIComponent.generated.h"

class UCBHealthBarWidget;
class UCBAbilitySystemComponent;
class UWidgetComponent;
class ACBHUD;

/**
 * 캐릭터 부착형 UI의 공용 관리자 컴포넌트 (전 캐릭터 공통, ACBBaseCharacter가 소유).
 * 캐릭터 시스템 준비 완료 후 오너 유형에 맞는 체력 UI를 생성·소유:
 * - 로컬 조작 플레이어: HUD 위젯을 생성해 HUD 스택(Game 레이어)에 위임
 * - 그 외(AI·원격 캐릭터): 머리 위 UWidgetComponent(Screen 모드)를 런타임 생성·부착
 * 위젯 클래스는 로드아웃에서 주입되는 런타임 캐시, UI 생성·표시는 전부 클라이언트 로컬.
 * (값 동기화는 어트리뷰트 리플리케이션이 담당). 외부 접근은 ICBUIInterface::GetCBUIComponent() 경유.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CHAINBURST_API UCBUIComponent : public UCBExtensionComponent
{
	GENERATED_BODY()

public:
	/** 로드아웃에서 머리 위 체력바 위젯 클래스를 주입하는 세터 (전 인스턴스 공용 경로) */
	FORCEINLINE void SetOverheadWidgetClass(TSubclassOf<UCBHealthBarWidget> InWidgetClass) { OverheadWidgetClass = InWidgetClass; }

	/** 로드아웃에서 HUD 체력 위젯 클래스를 주입하는 세터 (소유 클라이언트 전용 경로) */
	FORCEINLINE void SetHUDWidgetClass(TSubclassOf<UCBHealthBarWidget> InWidgetClass) { HUDWidgetClass = InWidgetClass; }

	/**
	 * [로컬 UI 연출용] 머리 위 체력바 표시 여부를 변경하는 함수.
	 * 호출한 클라이언트 화면에만 반영된다 (전 클라 반영이 필요하면 복제 신호 경유로 설계할 것).
	 * @param bVisible 표시 여부
	 */
	void SetOverheadBarVisible(bool bVisible);

	FORCEINLINE UCBHealthBarWidget* GetHUDWidget() const { return HUDWidget.Get(); }
	FORCEINLINE UCBHealthBarWidget* GetOverheadWidget() const { return OverheadWidget.Get(); }

protected:
	//~ Begin UActorComponent Interface.
	/** 생성한 HUD 위젯·머리 위 위젯 컴포넌트를 정리. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface.

	//~ Begin UCBExtensionComponent Interface.
	/** 준비 완료 시 오너 유형에 맞는 체력 UI를 생성 (로컬 플레이어=HUD, 그 외=머리 위 바). */
	virtual void OnCharacterSystemReady() override;
	//~ End UCBExtensionComponent Interface.

	/** 머리 위 체력바 사용 여부 (끄면 이 캐릭터는 머리 위 바를 만들지 않음) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|UI")
	bool bShowOverheadBar = true;

	/** 머리 위 체력바의 부착 높이 여유값 (캡슐 상단 기준 추가 높이) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|UI")
	float OverheadZMargin = 30.f;

private:
	/** [로컬 전용] HUD 위젯을 생성해 HUD 스택에 넘기는 함수 (로컬 조작 플레이어) */
	void Local_CreateHUDWidget(UCBAbilitySystemComponent* InASC);

	/** [로컬 전용] 생성한 HUD 위젯을 HUD 스택(Game 레이어)에 삽입하는 함수 */
	void Local_PushHUDWidgetToStack();

	/** [로컬 전용] HUD 위젯을 HUD 스택에서 제거하는 함수 */
	void Local_RemoveHUDWidgetFromStack();

	/** 머리 위 위젯 컴포넌트를 런타임 생성·부착하는 함수 (AI·원격 캐릭터) */
	void CreateOverheadWidget(UCBAbilitySystemComponent* InASC);

	/** 머리 위 체력바 위젯 클래스. 로드아웃에서 주입되는 런타임 캐시 */
	UPROPERTY()
	TSubclassOf<UCBHealthBarWidget> OverheadWidgetClass = nullptr;

	/** HUD 체력 위젯 클래스. 로드아웃(Chaser)에서 주입되는 런타임 캐시 */
	UPROPERTY()
	TSubclassOf<UCBHealthBarWidget> HUDWidgetClass = nullptr;

	/** 생성한 HUD 위젯 인스턴스 (로컬 조작 플레이어 전용) */
	UPROPERTY()
	TObjectPtr<UCBHealthBarWidget> HUDWidget = nullptr;

	/** 생성한 머리 위 위젯 인스턴스 */
	UPROPERTY()
	TObjectPtr<UCBHealthBarWidget> OverheadWidget = nullptr;

	/** 머리 위 위젯을 그리는 위젯 컴포넌트 (런타임 생성) */
	UPROPERTY()
	TObjectPtr<UWidgetComponent> OverheadWidgetComponent = nullptr;

	/** HUD 스택 제거용 HUD 캐시 (제거 시점엔 컨트롤러 연결이 끊겼을 수 있어 삽입 시점에 캐싱) */
	TWeakObjectPtr<ACBHUD> CachedHUD;
};
