// project
#include "Components/UI/CBUIComponent.h"
#include "Characters/CBBaseCharacter.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "UI/Widgets/CBHealthBarWidget.h"
#include "UI/CBHUD.h"
#include "GameStates/CBLobbyGameState.h"

// engine
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"

void UCBUIComponent::OnCharacterSystemReady()
{
	// 데디케이트 서버는 화면이 없으므로 UI 생성 안 함 (프로젝트는 리슨 서버 전제지만 안전 가드)
	if (GetNetMode() == NM_DedicatedServer) return;

	// 로비에서는 캐릭터 부착 UI(HUD·머리 위 바)를 만들지 않음.
	if (GetWorld()->GetGameState<ACBLobbyGameState>()) return;

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
		Local_CreateHUDWidget();
	}
	// 그 외(AI 전부 + 원격 캐릭터)는 머리 위 바
	else
	{
		CreateOverheadWidget(ASC);
	}
}

void UCBUIComponent::Local_CreateHUDWidget()
{
	// 위젯 클래스 미주입이면 생성하지 않음 (로드아웃에 HUD 위젯 클래스 미등록)
	if (!HUDWidgetClass) return;

	// HUD 위젯은 소유 플레이어 컨트롤러 기준으로 생성
	APlayerController* PC = GetOwningController<APlayerController>();
	if (!PC) return;

	// 위젯 생성.
	HUDWidget = CreateWidget<UUserWidget>(PC, HUDWidgetClass);
	
	if (!HUDWidget) return;

	// HUD 스택 삽입.
	Local_PushHUDWidgetToStack();
}

void UCBUIComponent::Local_PushHUDWidgetToStack()
{
	// 화면 배치는 HUD의 내비게이션 스택이 전담한다 (AddToViewport 사용 금지)
	const APlayerController* PC = GetOwningController<APlayerController>();
	ACBHUD* HUD = PC ? Cast<ACBHUD>(PC->GetHUD()) : nullptr;
	if (!HUD)
	{
		// 리페어런팅 누락·HUD Class 오지정이면 여기서 걸린다 (그대로 두면 체력바가 조용히 안 뜸)
		UE_LOG(LogTemp, Warning, TEXT("[%s] HUD가 ACBHUD가 아니어서 HUD 위젯을 스택에 넣지 못함"), *GetOwner()->GetName());
		return;
	}

	// 제거 시점엔 컨트롤러 연결이 이미 끊겼을 수 있으므로 삽입 시점의 HUD를 캐싱
	CachedHUD = HUD;
	HUD->PushGameLayerWidget(HUDWidget);
}

void UCBUIComponent::Local_RemoveHUDWidgetFromStack()
{
	// RemoveFromParent를 쓰면 스택 배열에 항목이 남아 위젯이 화면에 그대로 남는다
	if (ACBHUD* HUD = CachedHUD.Get())
	{
		HUD->PopWidgetFromStack(HUDWidget);
	}
	CachedHUD.Reset();
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
	// HUD 위젯 정리 — 컴포넌트의 오너 액터(=캐릭터)가 파괴된 경우(사망·재스폰)만 스택에서 뺌.
	// 월드가 통째로 끝나는 경우(맵 전환·PIE 종료)엔 HUD도 함께 파괴되므로 정리가 무의미함.
	if (HUDWidget && EndPlayReason == EEndPlayReason::Destroyed)
	{
		Local_RemoveHUDWidgetFromStack();
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
