// project
#include "Components/UI/CBUIComponent.h"
#include "Characters/CBBaseCharacter.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/CBAttributeSet.h"
#include "UI/Widgets/CBHealthBarWidget.h"
#include "UI/Widgets/CBNamePlateWidget.h"
#include "UI/CBHUD.h"
#include "GameStates/CBLobbyGameState.h"

// engine
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

void UCBUIComponent::OnCharacterSystemReady()
{
	// 데디케이트 서버는 화면이 없으므로 UI 생성 안 함 (프로젝트는 리슨 서버 전제지만 안전 가드)
	if (GetNetMode() == NM_DedicatedServer) return;

	// 이름표는 로비·게임플레이 공통. 게임플레이에서는 자기 자신을 빼고 거리 컬링 처리 (CreateNamePlateWidget 참고)
	CreateNamePlateWidget();

	// 로비에서는 체력 UI(HUD·머리 위 바)를 만들지 않음.
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

	// 사망 알림 구독 (시체 위에 체력바가 남지 않도록). 캐릭터가 Status.Dead 받아 전 인스턴스에서 방송함.
	OwnerCharacter->OnCharacterDiedDelegate.AddUObject(this, &UCBUIComponent::OnOwnerDied);

	// 위젯 인스턴스 생성
	OverheadWidget = CreateWidget<UCBHealthBarWidget>(OwnerCharacter->GetWorld(), OverheadWidgetClass);
	if (!OverheadWidget) return;

	// 위젯 컴포넌트 런타임 생성 (Screen 모드: 항상 카메라를 향하고 위젯 원본 크기로 렌더)
	OverheadWidgetComponent = NewObject<UWidgetComponent>(OwnerCharacter, TEXT("CBOverheadHealthBar"));
	OverheadWidgetComponent->SetupAttachment(OwnerCharacter->GetRootComponent()); // 캐릭터의 루트(캡슐)에 부착
	OverheadWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen); // 2D로 그리고, 항상 카메라를 향함
	OverheadWidgetComponent->SetDrawAtDesiredSize(true); // 위젯 원본 크기로 렌더

	// 부착 높이 = 캡슐 상단 + 여유값. 준비 완료 시점엔 로드아웃 바디 셋업이 적용된 뒤라 캡슐 크기가 확정 상태.
	// 위젯을 붙이기 전에 잡아야 첫 프레임이 캐릭터 원점에 뜨지 않음
	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f;
	OverheadWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, CapsuleHalfHeight + OverheadZMargin));

	// 평소 숨김 모드면 위젯을 붙이기 전에 숨겨 둠.
	if (bHideOverheadBarUntilDamaged)
	{
		// 위젯을 붙이기 전에 숨기기. (첫 프레임에 멀리 있는 이름표가 화면에 잠깐 올라오는 것을 방지)
		SetOverheadBarVisible(false); // 숨김 모드로 시작
	}

	// 위젯 부착
	OverheadWidgetComponent->SetWidget(OverheadWidget); // 컴포넌트에 위젯 설정
	OverheadWidgetComponent->RegisterComponent();

	// ASC 바인딩 (초기값 반영 + 변경 구독)
	OverheadWidget->InitializeWithASC(InASC);

	// 평소 숨김 모드면 체력 감소를 구독해 피격 시에만 표시함.
	// 캐릭터 준비 완료 후 작업이므로 ASC 준비되어 있음.
	if (bHideOverheadBarUntilDamaged)
	{
		BindOverheadDamageTrigger(InASC); // CurrentHealth 변경 구독
	}
}

// [로비 전용] 이름표 위젯 생성 및 부착
void UCBUIComponent::CreateNamePlateWidget()
{
	// 꺼두었거나 위젯 클래스 미지정이면 생성하지 않음
	if (!bShowNamePlate || !NamePlateWidgetClass) return;

	ACBBaseCharacter* OwnerCharacter = GetOwningPawn<ACBBaseCharacter>();

	// 이름은 PlayerState 에서 옴. Chaser 는 초기화 진입점이 PossessedBy(서버)/OnRep_PlayerState(클라)라
	// 준비 완료 시점엔 이미 유효하지만, PlayerState 가 없는 대상(AI 등)은 여기서 걸러짐.
	APlayerState* OwnerPlayerState = OwnerCharacter->GetPlayerState();
	if (!OwnerPlayerState) return;

	// 위젯 인스턴스 생성
	NamePlateWidget = CreateWidget<UCBNamePlateWidget>(OwnerCharacter->GetWorld(), NamePlateWidgetClass);
	if (!NamePlateWidget) return;

	// 로비인지 판단. 로비는 라인업을 함께 보는 화면이라 자기 이름표도 띄우고 거리 컬링도 하지 않음
	const UWorld* World = GetWorld();
	const bool bIsLobby = World && World->GetGameState<ACBLobbyGameState>() != nullptr;

	// 이 이름표 위젯의 소유자가 로컬 플레이어인지 판정.
	// 클라이언트 입장에서 Controller 복제가 아직 채워지지 않은 상태라면 GetLocalRole() == ROLE_AutonomousProxy 로 판단.
	// 클라의 캐릭터 초기화의 진입점이 OnRep_PlayerState 이기 때문에, 컨트롤러는 아직 복제되지 않은 상태일 수 있음. (즉, IsLocallyControlled()가 false)
	const bool bIsLocalTarget = OwnerCharacter->IsLocallyControlled()
		|| OwnerCharacter->GetLocalRole() == ROLE_AutonomousProxy;

	// 게임플레이에서는 자기 발밑 이름표가 화면 한가운데를 계속 차지하므로 만들지 않음.
	// 로비는 라인업에서 자기 자리를 확인해야 하므로 그대로 띄움.
	if (bIsLocalTarget && !bIsLobby)
	{
		NamePlateWidget = nullptr;
		return;
	}

	// 대상 바인딩 (현재 이름 반영 + 변경 구독). 위젯의 Construct 보다 먼저 끝나도록 컴포넌트 등록 전에 수행
	NamePlateWidget->InitializeWithPlayerState(OwnerPlayerState, bIsLocalTarget);

	// 위젯 컴포넌트 런타임 생성 (Screen 모드: 항상 카메라를 향하고 위젯 원본 크기로 렌더)
	NamePlateWidgetComponent = NewObject<UWidgetComponent>(OwnerCharacter, TEXT("CBNamePlate"));
	NamePlateWidgetComponent->SetupAttachment(OwnerCharacter->GetRootComponent()); // 캐릭터의 루트(캡슐)에 부착
	NamePlateWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen); // 2D로 그리고, 항상 카메라를 향함
	NamePlateWidgetComponent->SetDrawAtDesiredSize(true); // 위젯 원본 크기로 렌더

	// 부착 높이 = 캡슐 하단(발밑) - 여유값. 준비 완료 시점이라 로드아웃 바디 셋업이 끝나 캡슐 크기가 확정 상태.
	// 위젯을 붙이기 전에 잡아야 첫 프레임이 캐릭터 원점에 뜨지 않음
	const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
	const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f;
	NamePlateWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, -(CapsuleHalfHeight + NamePlateZMargin)));

	// 게임플레이에서는 멀리 있는 이름표를 숨김.
	if (!bIsLobby)
	{
		// 첫 판정으로 거리 밖이면 미리 숨기고, 거리 검사 타이머를 시작
		// 위젯을 붙이기 전에 숨기기. (첫 프레임에 멀리 있는 이름표가 화면에 잠깐 올라오는 것을 방지)
		StartNamePlateDistanceCheck();
	}

	// 위젯 부착
	NamePlateWidgetComponent->SetWidget(NamePlateWidget);
	NamePlateWidgetComponent->RegisterComponent();
}

// [게임플레이 전용] 거리 검사 타이머 시작
void UCBUIComponent::StartNamePlateDistanceCheck()
{
	// 거리 제한이 없으면 항상 표시 (타이머도 돌리지 않음)
	if (NamePlateVisibleDistance <= 0.f) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// 첫 판정 (시작할 때 멀리 있는 이름표가 보이는 것을 막음).
	// 호출자가 SetWidget 전에 불러 주므로 여기서 숨기면 화면에 아예 올라가지 않음
	CheckNamePlateDistance();

	// 거리 검사 주기마다 CheckNamePlateDistance를 호출하도록 타이머를 설정 (반복)
	World->GetTimerManager().SetTimer(NamePlateDistanceTimerHandle, this,
		&UCBUIComponent::CheckNamePlateDistance, NamePlateDistanceCheckInterval, true);
}

// 닉네임 거리 검사 타이머 콜백.
void UCBUIComponent::CheckNamePlateDistance()
{
	if (!NamePlateWidgetComponent) return;

	// NamePlateVisibleDistance 거리 안에 있는지 검사.
	const bool bShouldBeVisible = IsWithinDistanceFromLocalViewer(NamePlateVisibleDistance);

	// 같은 상태면 건드리지 않음 (Screen 모드는 숨길 때마다 위젯이 파괴·재생성됨)
	if (NamePlateWidgetComponent->IsVisible() == bShouldBeVisible) return;

	// 안에 있으면 보이게, 밖이면 숨김. (Screen 모드는 숨길 때마다 위젯이 파괴·재생성됨)
	NamePlateWidgetComponent->SetVisibility(bShouldBeVisible);
}

// 피격 시에만 머리 위 바를 표시하기 위해 CurrentHealth 변경을 구독하는 함수. (CreateOverheadWidget에서 호출)
void UCBUIComponent::BindOverheadDamageTrigger(UCBAbilitySystemComponent* InASC)
{
	if (!InASC) return;

	// 구독 해제용 캐시
	OverheadTriggerASC = InASC;

	// CurrentHealth 변경 시 HandleOwnerHealthChanged를 호출하도록 델리게이트 구독
	OverheadHealthChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UCBAttributeSet::GetCurrentHealthAttribute())
		.AddUObject(this, &UCBUIComponent::HandleOwnerHealthChanged);
}

// CurrentHealth 변경 구독을 해제하고 캐시를 비우는 함수. (EndPlay에서 호출)
void UCBUIComponent::UnbindOverheadDamageTrigger()
{
	// 구독 중이던 델리게이트 해제 (댕글링 방지)
	if (OverheadTriggerASC.IsValid())
	{
		OverheadTriggerASC->GetGameplayAttributeValueChangeDelegate(UCBAttributeSet::GetCurrentHealthAttribute()).Remove(OverheadHealthChangedHandle);
	}

	// 캐시·핸들 초기화
	OverheadTriggerASC = nullptr;
	OverheadHealthChangedHandle.Reset();
}

// CurrentHealth 변경 델리게이트 내부 콜백. 감소(=피격)일 때만 머리 위 바를 표시함.
void UCBUIComponent::HandleOwnerHealthChanged(const FOnAttributeChangeData& Data)
{
	// 감소(=피격)일 때만 표시. 회복이나 초기화로 늘어난 경우는 무시함
	if (Data.NewValue >= Data.OldValue) return;

	// 거리 밖이면 아예 켜지 않음.
	// 켜고 나서 거리 타이머가 끄게 두면 먼 곳에서 한 틱 동안 깜빡임.
	if (!IsWithinOverheadBarDistance()) return;

	// 머리 위 바를 표시하고 유지·거리 검사 타이머를 (재)시작
	ShowOverheadBarTemporarily();
}

// 머리 위 바를 표시하고 유지·거리 검사 타이머를 (재)시작하는 함수
void UCBUIComponent::ShowOverheadBarTemporarily()
{
	// 머리 위 위젯이 없는 캐릭터(로컬 플레이어 등)면 할 일 없음
	if (!OverheadWidgetComponent) return;

	// 머리 위 바를 표시
	SetOverheadBarVisible(true);

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();

	// 유지 타이머 (표시 중에 다시 맞으면 SetTimer가 덮어써 시간이 리셋됨. 0 이하면 시간 만료로는 숨기지 않음)
	if (OverheadBarShowDuration > 0.f)
	{
		// 유지 시간 만료 시 HideOverheadBar를 호출하도록 타이머를 설정 (한 번만)
		TimerManager.SetTimer(OverheadHideTimerHandle, this, &UCBUIComponent::HideOverheadBar, OverheadBarShowDuration, false);
	}

	// 거리 검사 타이머 (표시 중일 때만 돌려 평상시 비용을 없앰. 이미 돌고 있으면 주기를 흐트러뜨리지 않도록 그대로 둠)
	if (!TimerManager.IsTimerActive(OverheadDistanceTimerHandle))
	{
		// 일정 주기마다 CheckOverheadBarDistance를 호출하도록 타이머를 설정 (반복)
		TimerManager.SetTimer(OverheadDistanceTimerHandle, this, &UCBUIComponent::CheckOverheadBarDistance, OverheadBarDistanceCheckInterval, true);
	}
}

// 머리 위 바를 숨기고 유지·거리 검사 타이머를 초기화하는 함수 (유지 시간 만료·거리 초과 공용)
void UCBUIComponent::HideOverheadBar()
{
	SetOverheadBarVisible(false);

	// 숨은 동안은 타이머를 돌리지 않음 (다음 피격 때 다시 시작)
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(OverheadHideTimerHandle);
		TimerManager.ClearTimer(OverheadDistanceTimerHandle);
	}
}

// 로컬 시점이 머리 위 바가 보이는 거리 안에 있는지 반환하는 함수. (true: 거리안에 있거나 판단 불가, false: 거리 밖)
bool UCBUIComponent::IsWithinOverheadBarDistance() const
{
	// 머리 위 바 기준 거리로 공용 판정을 수행
	return IsWithinDistanceFromLocalViewer(OverheadBarVisibleDistance);
}

// 로컬 시점(폰 → 없으면 카메라)과 이 위젯의 소유 액터 사이의 거리 판정. (InDistance 이하이면 true, 판단 불가면 true)
bool UCBUIComponent::IsWithinDistanceFromLocalViewer(float InDistance) const
{
	// 로컬 시점 기준의 거리이므로
	const APlayerController* LocalPC = GEngine ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	if (!LocalPC) return true;

	// 로컬 플레이어의 폰 위치를 얻음.
	FVector ViewLocation;
	if (const APawn* LocalPawn = LocalPC->GetPawn())
	{
		ViewLocation = LocalPawn->GetActorLocation();
	}
	// 로컬 플레이어가 Pawn을 갖고 있지 않으면 카메라의 위치를 얻음.
	else if (const APlayerCameraManager* CameraManager = LocalPC->PlayerCameraManager)
	{
		ViewLocation = CameraManager->GetCameraLocation();
	}
	// 둘 다 없으면 판단 불가이므로 true를 반환.
	else
	{
		return true;
	}

	// 로컬 시점과 오너 액터 사이의 거리를 계산
	// 제곱 거리로 비교 (sqrt 회피)
	const float DistanceSqr = FVector::DistSquared(ViewLocation, GetOwner()->GetActorLocation());
	
	// 기준 거리 안이면 true, 밖이면 false
	return DistanceSqr <= FMath::Square(InDistance);
}

// 체력바 거리 검사 타이머 콜백. 보이는 거리를 벗어나면 숨김
void UCBUIComponent::CheckOverheadBarDistance()
{
	// 보이는 거리를 벗어났으면 숨김
	if (!IsWithinOverheadBarDistance())
	{
		HideOverheadBar();
	}
}

// 오너 사망 콜백. 머리 위 바를 숨기고 다시 뜨지 않게 함.
void UCBUIComponent::OnOwnerDied()
{
	// 피격 구독을 먼저 끊기. (이후의 값 변동으로 바가 되살아나지 않음)
	UnbindOverheadDamageTrigger();

	HideOverheadBar();
}

// 머리 위 바를 표시하거나 숨기는 함수. (Show/Hide 공용)
void UCBUIComponent::SetOverheadBarVisible(bool bVisible)
{
	// 머리 위 위젯이 없는 캐릭터(로컬 플레이어 등)면 무시
	if (!OverheadWidgetComponent) return;

	// 숨기면 NativeDestruct 호출되어 구독을 정리.
	// 표시하면 NativeConstruct 호출되어 구독을 재설정.
	OverheadWidgetComponent->SetVisibility(bVisible);
}

void UCBUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// HUD 위젯 정리. 컴포넌트의 오너 액터(=캐릭터)가 파괴된 경우(사망·재스폰)만 스택에서 뺌.
	// 월드가 통째로 끝나는 경우(맵 전환·PIE 종료)엔 HUD도 함께 파괴되므로 정리가 무의미함.
	if (HUDWidget && EndPlayReason == EEndPlayReason::Destroyed)
	{
		Local_RemoveHUDWidgetFromStack();
		HUDWidget = nullptr;
	}

	// 머리 위 바 표시 제어 정리 (구독·타이머). 위젯 컴포넌트를 없애기 전에 먼저 끊음
	UnbindOverheadDamageTrigger();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OverheadHideTimerHandle);
		World->GetTimerManager().ClearTimer(OverheadDistanceTimerHandle);
		World->GetTimerManager().ClearTimer(NamePlateDistanceTimerHandle);
	}

	// 머리 위 위젯 컴포넌트 정리
	if (OverheadWidgetComponent)
	{
		OverheadWidgetComponent->DestroyComponent();
		OverheadWidgetComponent = nullptr;
	}
	OverheadWidget = nullptr;

	// 발밑 이름표 위젯 컴포넌트 정리
	if (NamePlateWidgetComponent)
	{
		NamePlateWidgetComponent->DestroyComponent();
		NamePlateWidgetComponent = nullptr;
	}
	NamePlateWidget = nullptr;

	Super::EndPlay(EndPlayReason);
}
