#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "CBUIComponent.generated.h"

class UUserWidget;
class UCBHealthBarWidget;
class UCBNamePlateWidget;
class UCBAbilitySystemComponent;
class UWidgetComponent;
class ACBHUD;
struct FOnAttributeChangeData;

/**
 * 캐릭터 부착형 UI의 공용 관리자 컴포넌트 (전 캐릭터 공통, ACBBaseCharacter가 소유).
 * 캐릭터 시스템 준비 완료 후 오너 유형에 맞는 체력 UI를 생성·소유:
 * - 로컬 조작 플레이어: HUD 위젯을 생성해 HUD 스택(Game 레이어)에 위임
 * - 그 외(AI·원격 캐릭터): 머리 위 UWidgetComponent(Screen 모드)를 런타임 생성·부착
 * 
 * 로비에서는 체력 UI 대신 발밑 이름표(UWidgetComponent)만 만듦 — PlayerState 의 닉네임.
 *
 * 머리 위 바는 평소 숨김 상태로, 오너의 체력 감소(=피격)를 구독해 일정 시간 표시.
 * 체력은 복제 어트리뷰트라 거리 안의 모든 클라이언트에서 각자 표시.
 * 유지 시간 만료 또는 거리 이탈로 숨기는 것도 각자 로컬에서 판단.
 * 
 * 위젯 클래스는 로드아웃에서 주입되는 런타임 캐시, UI 생성·표시는 전부 클라이언트 로컬.
 * 외부 접근은 ICBUIInterface::GetCBUIComponent() 경유.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CHAINBURST_API UCBUIComponent : public UCBExtensionComponent
{
	GENERATED_BODY()

public:
	/** 로드아웃에서 머리 위 체력바 위젯 클래스를 주입하는 세터 (전 인스턴스 공용 경로) */
	FORCEINLINE void SetOverheadWidgetClass(TSubclassOf<UCBHealthBarWidget> InWidgetClass) { OverheadWidgetClass = InWidgetClass; }

	/** 로드아웃에서 HUD 위젯 클래스를 주입하는 세터 (소유 클라이언트 전용 경로) */
	FORCEINLINE void SetHUDWidgetClass(TSubclassOf<UUserWidget> InWidgetClass) { HUDWidgetClass = InWidgetClass; }

	/**
	 * 로드아웃에서 발밑 이름표 설정을 주입하는 세터 (전 인스턴스 공용 경로).
	 * 세 값이 항상 같은 자리에서 함께 오므로 하나로 묶음.
	 * @param bInShow      이름표 사용 여부
	 * @param InWidgetClass 이름표 위젯 클래스
	 * @param InZMargin    부착 높이 여유값 (캡슐 하단 기준)
	 * @param InVisibleDistance 이름표가 보이는 거리(cm). 0 이하면 거리 컬링 없음
	 */
	FORCEINLINE void SetNamePlateSettings(bool bInShow, TSubclassOf<UCBNamePlateWidget> InWidgetClass, float InZMargin, float InVisibleDistance)
	{
		bShowNamePlate = bInShow;
		NamePlateWidgetClass = InWidgetClass;
		NamePlateZMargin = InZMargin;
		NamePlateVisibleDistance = InVisibleDistance;
	}

	/**
	 * [로컬 UI 연출용] 머리 위 체력바 표시 여부를 변경하는 함수.
	 * 호출한 클라이언트 화면에만 반영된다 (전 클라 반영이 필요하면 복제 신호 경유로 설계할 것).
	 * 단, bHideOverheadBarUntilDamaged가 켜져 있으면 피격·유지 시간·거리 판단이 이후에 이 값을 덮어쓴다.
	 * @param bVisible 표시 여부
	 */
	void SetOverheadBarVisible(bool bVisible);

	FORCEINLINE UUserWidget* GetHUDWidget() const { return HUDWidget.Get(); }
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

	/** 머리 위 체력바를 피격 전까지 숨길지 여부 (끄면 생성 직후부터 항상 표시) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|UI")
	bool bHideOverheadBarUntilDamaged = true;

	/** 피격 후 머리 위 체력바를 유지할 시간(초). 0 이하면 시간 만료로는 숨기지 않음 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|UI", meta = (EditCondition = "bHideOverheadBarUntilDamaged"))
	float OverheadBarShowDuration = 5.f;

	/** 머리 위 체력바가 보이는 거리(cm). 로컬 시점이 이 안에 있어야 피격 시 표시되고, 넘어서면 표시 중이더라도 숨김 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|UI", meta = (EditCondition = "bHideOverheadBarUntilDamaged"))
	float OverheadBarVisibleDistance = 2000.f;

	/** 거리 검사 주기(초). 표시 중일 때만 돌아감 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|UI", meta = (EditCondition = "bHideOverheadBarUntilDamaged"))
	float OverheadBarDistanceCheckInterval = 0.2f;

	/** 발밑 이름표의 거리 검사 주기(초). 게임플레이에서만 돌아감 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|UI|NamePlate")
	float NamePlateDistanceCheckInterval = 0.25f;

private:
	/** [로컬 전용] HUD 위젯을 생성해 HUD 스택에 넘기는 함수 (로컬 조작 플레이어). ASC 바인딩은 HUD 위젯(WBP) 내부에서 처리 */
	void Local_CreateHUDWidget();

	/** [로컬 전용] 생성한 HUD 위젯을 HUD 스택(Game 레이어)에 삽입하는 함수 */
	void Local_PushHUDWidgetToStack();

	/** [로컬 전용] HUD 위젯을 HUD 스택에서 제거하는 함수 */
	void Local_RemoveHUDWidgetFromStack();

	/** 머리 위 위젯 컴포넌트를 런타임 생성·부착하는 함수 (AI·원격 캐릭터) */
	void CreateOverheadWidget(UCBAbilitySystemComponent* InASC);

	/**
	 * 발밑 이름표의 거리 검사를 시작하는 함수 (게임플레이 전용).
	 * 로비는 전원이 가까이 모여 있어 컬링할 것이 없으므로 돌리지 않는다.
	 */
	void StartNamePlateDistanceCheck();

	/** 거리 검사 타이머 콜백. 보이는 거리 안에 있을 때만 이름표를 표시함 */
	void CheckNamePlateDistance();

	/**
	 * 로컬 시점이 오너로부터 지정한 거리 안에 있는지 반환하는 함수 (판단 불가면 true).
	 * @param InDistance 기준 거리(cm)
	 */
	bool IsWithinDistanceFromLocalViewer(float InDistance) const;

	/**
	 * 발밑 이름표 위젯 컴포넌트를 런타임 생성·부착하는 함수.
	 * 이름의 출처인 PlayerState 가 없으면(AI 등) 만들지 않는다.
	 */
	void CreateNamePlateWidget();

	/** [로컬 전용] 오너의 CurrentHealth 변경을 구독해 머리 위 바 표시를 트리거하는 함수 */
	void BindOverheadDamageTrigger(UCBAbilitySystemComponent* InASC);

	/** CurrentHealth 변경 구독을 해제하고 캐시를 비우는 함수 */
	void UnbindOverheadDamageTrigger();

	/** 로컬 시점이 머리 위 바가 보이는 거리 안에 있는지 반환하는 함수. (true: 거리안에 있거나 판단 불가, false: 거리 밖) */
	bool IsWithinOverheadBarDistance() const;

	/** CurrentHealth 변경 델리게이트 내부 콜백 (감소 + 거리 안일 때만 표시) */
	void HandleOwnerHealthChanged(const FOnAttributeChangeData& Data);

	/** 머리 위 바를 표시하고 유지·거리 검사 타이머를 (재)시작하는 함수 */
	void ShowOverheadBarTemporarily();

	/** 머리 위 바를 숨기고 유지·거리 검사 타이머를 정지하는 함수 (유지 시간 만료·거리 초과 공용) */
	void HideOverheadBar();

	/** 거리 검사 타이머 콜백 (보이는 거리를 벗어나면 숨김) */
	void CheckOverheadBarDistance();

	/** 오너 사망 콜백 (머리 위 바를 숨기고 이후 피격 표시도 막음) */
	void OnOwnerDied();

	/** 머리 위 체력바 위젯 클래스. 로드아웃에서 주입되는 런타임 캐시 */
	UPROPERTY()
	TSubclassOf<UCBHealthBarWidget> OverheadWidgetClass = nullptr;

	/** HUD 위젯 클래스. 로드아웃(Chaser)에서 주입되는 런타임 캐시 */
	UPROPERTY()
	TSubclassOf<UUserWidget> HUDWidgetClass = nullptr;

	/** 발밑 이름표 사용 여부. 로드아웃에서 주입되는 런타임 캐시 */
	bool bShowNamePlate = true;

	/** 발밑 이름표 위젯 클래스. 로드아웃에서 주입되는 런타임 캐시 */
	UPROPERTY()
	TSubclassOf<UCBNamePlateWidget> NamePlateWidgetClass = nullptr;

	/** 발밑 이름표의 부착 높이 여유값. 로드아웃에서 주입되는 런타임 캐시 */
	float NamePlateZMargin = 30.f;

	/** 발밑 이름표가 보이는 거리(cm). 로드아웃에서 주입되는 런타임 캐시. 0 이하면 거리 컬링 없음 */
	float NamePlateVisibleDistance = 1500.f;

	/** 생성한 HUD 위젯 인스턴스 (로컬 조작 플레이어 전용) */
	UPROPERTY()
	TObjectPtr<UUserWidget> HUDWidget = nullptr;

	/** 생성한 머리 위 위젯 인스턴스 */
	UPROPERTY()
	TObjectPtr<UCBHealthBarWidget> OverheadWidget = nullptr;

	/** 머리 위 위젯을 그리는 위젯 컴포넌트 (런타임 생성) */
	UPROPERTY()
	TObjectPtr<UWidgetComponent> OverheadWidgetComponent = nullptr;

	/** 생성한 발밑 이름표 위젯 인스턴스 (로비 전용) */
	UPROPERTY()
	TObjectPtr<UCBNamePlateWidget> NamePlateWidget = nullptr;

	/** 발밑 이름표를 그리는 위젯 컴포넌트 (런타임 생성) */
	UPROPERTY()
	TObjectPtr<UWidgetComponent> NamePlateWidgetComponent = nullptr;

	/** HUD 스택 제거용 HUD 캐시 (제거 시점엔 컨트롤러 연결이 끊겼을 수 있어 삽입 시점에 캐싱) */
	TWeakObjectPtr<ACBHUD> CachedHUD;

	/** 체력 감소 구독 해제용 ASC 캐시 */
	TWeakObjectPtr<UCBAbilitySystemComponent> OverheadTriggerASC;

	/** CurrentHealth 변경 델리게이트 구독 해제용 핸들 */
	FDelegateHandle OverheadHealthChangedHandle;

	/** 머리 위 바 유지 시간 타이머 핸들 */
	FTimerHandle OverheadHideTimerHandle;

	/** 머리 위 바 거리 검사 타이머 핸들 */
	FTimerHandle OverheadDistanceTimerHandle;

	/** 이름표 거리 검사 타이머 핸들 */
	FTimerHandle NamePlateDistanceTimerHandle;
};
