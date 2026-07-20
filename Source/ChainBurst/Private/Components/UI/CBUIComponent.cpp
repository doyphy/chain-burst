// project
#include "Components/UI/CBUIComponent.h"
#include "Characters/CBBaseCharacter.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "UI/Widgets/CBHealthBarWidget.h"

// engine
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"

void UCBUIComponent::OnCharacterSystemReady()
{
	// 데디케이트 서버는 화면이 없으므로 UI 생성 안 함 (프로젝트는 리슨 서버 전제지만 안전 가드)
	if (GetNetMode() == NM_DedicatedServer) return;

	ACBBaseCharacter* OwnerCharacter = GetOwningPawn<ACBBaseCharacter>();

	// 준비 완료 시점이므로 ASC는 캐싱 완료 상태여야 함
	UCBAbilitySystemComponent* ASC = OwnerCharacter->GetCBAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 준비 완료 시점에 ASC가 유효하지 않아 체력 UI를 생성하지 않음"), *GetOwner()->GetName());
		return;
	}

	// 로컬 조작 '플레이어'만 HUD.
	// AI는 서버에서 IsLocallyControlled가 참이므로 IsPlayerControlled를 병행 검사
	if (OwnerCharacter->IsPlayerControlled() && OwnerCharacter->IsLocallyControlled())
	{
		Local_CreateHUDWidget(ASC);
	}
	// 그 외(AI 전부 + 원격 캐릭터)는 머리 위 바
	else
	{
		CreateOverheadWidget(ASC);
	}
}

void UCBUIComponent::Local_CreateHUDWidget(UCBAbilitySystemComponent* InASC)
{
	// 위젯 클래스 미주입이면 생성하지 않음 (로드아웃에 HUD 위젯 클래스 미등록)
	if (!HUDWidgetClass) return;

	// HUD 위젯은 소유 플레이어 컨트롤러 기준으로 생성
	APlayerController* PC = GetOwningController<APlayerController>();
	if (!PC) return;

	// 위젯 생성 후 ASC 바인딩 → 뷰포트 추가
	HUDWidget = CreateWidget<UCBHealthBarWidget>(PC, HUDWidgetClass);
	if (!HUDWidget) return;

	HUDWidget->InitializeWithASC(InASC);
	HUDWidget->AddToViewport();
}

void UCBUIComponent::CreateOverheadWidget(UCBAbilitySystemComponent* InASC)
{
	// 클래스별로 꺼두었거나 위젯 클래스 미주입이면 생성하지 않음
	if (!bShowOverheadBar || !OverheadWidgetClass) return;

	ACBBaseCharacter* OwnerCharacter = GetOwningPawn<ACBBaseCharacter>();

	// 위젯 인스턴스를 직접 생성해 SetWidget으로 연결 (ASC 바인딩 호출 지점을 확보하기 위함)
	OverheadWidget = CreateWidget<UCBHealthBarWidget>(OwnerCharacter->GetWorld(), OverheadWidgetClass);
	if (!OverheadWidget) return;

	// 위젯 컴포넌트 런타임 생성 (Screen 모드: 항상 카메라를 향하고 위젯 원본 크기로 렌더)
	OverheadWidgetComponent = NewObject<UWidgetComponent>(OwnerCharacter, TEXT("CBOverheadHealthBar"));
	OverheadWidgetComponent->SetupAttachment(OwnerCharacter->GetRootComponent()); // 캐릭터의 루트(캡슐)에 부착
	OverheadWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen); // 2D로 그리고, 항상 카메라를 향함
	OverheadWidgetComponent->SetDrawAtDesiredSize(true); // 위젯 원본 크기로 렌더
	OverheadWidgetComponent->SetWidget(OverheadWidget); // 컴포넌트에 위젯 설정

	// 부착 높이 = 캡슐 상단 + 여유값. 준비 완료 시점엔 로드아웃 바디 셋업이 적용된 뒤라 캡슐 크기가 확정 상태
	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f;
	OverheadWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, CapsuleHalfHeight + OverheadZMargin));

	OverheadWidgetComponent->RegisterComponent();

	// ASC 바인딩 (초기값 반영 + 변경 구독)
	OverheadWidget->InitializeWithASC(InASC);
}

void UCBUIComponent::SetOverheadBarVisible(bool bVisible)
{
	// 머리 위 위젯이 없는 캐릭터(로컬 플레이어 등)면 무시
	if (OverheadWidgetComponent)
	{
		OverheadWidgetComponent->SetVisibility(bVisible);
	}
}

void UCBUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// HUD 위젯 정리 (어트리뷰트 구독 해제는 위젯의 NativeDestruct가 수행)
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}

	// 머리 위 위젯 컴포넌트 정리
	if (OverheadWidgetComponent)
	{
		OverheadWidgetComponent->DestroyComponent();
		OverheadWidgetComponent = nullptr;
	}
	OverheadWidget = nullptr;

	Super::EndPlay(EndPlayReason);
}
