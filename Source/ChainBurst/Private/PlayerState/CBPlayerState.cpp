// project
#include "PlayerState/CBPlayerState.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem//CBAttributeSet.h"
#include "Characters/CBChaserCharacter.h"
#include "Components/Mesh/CBModularMeshComponent.h"
#include "GameStates/CBLobbyGameState.h"

// engine
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ACBPlayerState::ACBPlayerState()
{
	// ASC 생성
	CBAbilitySystemComponent = CreateDefaultSubobject<UCBAbilitySystemComponent>(TEXT("CBAbilitySystemComponent"));
	// 복제 설정
	CBAbilitySystemComponent->SetIsReplicated(true);
	// 플레이어 캐릭터의 ASC 복제 모드는 Mixed로 설정
	CBAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// AttributeSet 생성
	CBAttributeSet = CreateDefaultSubobject<UCBAttributeSet>(TEXT("CBAttributeSet"));

	// 슬롯을 배열 인덱스로 쓰므로 크기를 미리 고정 (빈 태그 = 선택 해제)
	Cosmetics.Init(FGameplayTag(), static_cast<int32>(ECBCosmeticSlot::MAX));
}

UAbilitySystemComponent* ACBPlayerState::GetAbilitySystemComponent() const
{
	return Cast<UAbilitySystemComponent>(CBAbilitySystemComponent);
}

void ACBPlayerState::BeginPlay()
{
	Super::BeginPlay();

	// 복제가 폰보다 먼저 도착할 수 있으므로, 폰이 연결되는 시점에도 한 번 더 적용함
	OnPawnSet.AddDynamic(this, &ACBPlayerState::HandlePawnSet);

	// 폰이 이미 연결되어 있다면 바로 적용
	ApplyCosmeticsWhenReady();
}

void ACBPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 다른 플레이어의 외형도 보여야 하므로 전 클라이언트에 복제
	DOREPLIFETIME(ACBPlayerState, Cosmetics);

	// 로비 위젯이 다른 플레이어의 준비 여부도 볼 수 있어야 하므로 전 클라이언트에 복제
	DOREPLIFETIME(ACBPlayerState, bIsReady);

	// 선택 UI 가 자기 선택을 표시해야 하므로 복제 (다른 플레이어의 선택도 로비 위젯에 쓸 수 있음)
	DOREPLIFETIME(ACBPlayerState, SelectedCharacterId);
}

// seamless travel 로 맵을 넘어갈 때 엔진이 새 PlayerState 를 만든 뒤 호출
void ACBPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);

	// 로비에서 고른 의상이 새로운 레벨에서도 유지되도록 Cosmetics 배열을 새 PlayerState 에 복사
	if (ACBPlayerState* NewCBPlayerState = Cast<ACBPlayerState>(NewPlayerState))
	{
		NewCBPlayerState->Cosmetics = Cosmetics;

		// 고른 캐릭터도 함께 이관. 빠지면 게임플레이 레벨에서 기본 폰 클래스로 스폰됨
		NewCBPlayerState->SelectedCharacterId = SelectedCharacterId;
	}
}

// [서버] 컨트롤러가 요청을 검증한 뒤 호출 (서버에서 실행)
void ACBPlayerState::Auth_SetSelectedCharacterId(const FGameplayTag& InCharacterId)
{
	// 이미 같은 캐릭터라면 무시
	if (SelectedCharacterId == InCharacterId) return;

	// 고른 캐릭터 태그를 저장 (빈 태그 = 선택 안 함)
	SelectedCharacterId = InCharacterId;

	// 서버에서는 OnRep 이 불리지 않으므로 직접 호출해 호스트 위젯에도 반영.
	OnRep_SelectedCharacterId();
}

// 고른 캐릭터가 바뀌었을 때 호출되는 콜백. 실제 캐릭터 교체는 서버의 재스폰이 담당하고, 여기서는 표시만 갱신함
void ACBPlayerState::OnRep_SelectedCharacterId()
{
	// 선택 변경 신호를 방송. 위젯이 구독해 선택 표시를 갱신하도록 함
	OnCharacterSelectionChanged.Broadcast(SelectedCharacterId);
}

// [서버] 컨트롤러가 요청을 검증한 뒤 호출 (서버에서 실행)
void ACBPlayerState::Auth_SetCosmeticPart(ECBCosmeticSlot InSlot, const FGameplayTag& InPartId)
{
	// enum 을 int32 로 변환하여 배열 인덱스로 사용
	const int32 SlotIndex = static_cast<int32>(InSlot);

	// MAX 같은 유효하지 않은 슬롯 값이 들어오면 무시
	if (!Cosmetics.IsValidIndex(SlotIndex)) return;

	// 슬롯에 파츠 태그를 저장 (빈 태그 = 선택 해제)
	Cosmetics[SlotIndex] = InPartId;

	// 서버에서는 OnRep 이 불리지 않으므로 직접 호출해 서버 호스트 화면에도 반영.
	OnRep_Cosmetics();
}

// Cosmetics 배열이 바뀌었을 때 호출되는 콜백. 각 인스턴스가 로컬로 외형에 반영함.
void ACBPlayerState::OnRep_Cosmetics()
{
	// 코스메틱 적용
	ApplyCosmeticsWhenReady();
}

// 폰이 연결되는 시점에 호출되는 콜백. BeginPlay에서 구독함.
void ACBPlayerState::HandlePawnSet(APlayerState* InPlayerState, APawn* InNewPawn, APawn* InOldPawn)
{
	// 폰이 바뀌었으므로 로드 완료 여부를 다시 판정.
	Local_RefreshCharacterLoaded();

	// 코스메틱 적용
	ApplyCosmeticsWhenReady();
}

// [서버] 컨트롤러가 준비 요청을 받으면 호출 (서버에서 실행)
void ACBPlayerState::Auth_SetReady(bool bInReady)
{
	// 이미 같은 상태라면 무시
	if (bIsReady == bInReady) return;

	// 준비 상태 변경
	bIsReady = bInReady;

	// 서버에서는 OnRep 이 불리지 않으므로 직접 호출해 호스트 위젯에도 반영.
	OnRep_IsReady();
}

// 준비 상태가 바뀌었을 때 호출되는 콜백
void ACBPlayerState::OnRep_IsReady()
{
	// 이 플레이어의 준비 상태 변경을 방송. 준비 중 잠가야 하는 위젯이 각자 구독해 갱신함
	OnPlayerReadyChanged.Broadcast(bIsReady);

	const UWorld* World = GetWorld();
	if (!World) return;

	// 로비 게임 스테이트를 가져와 준비 상태 변경을 알림.
	if (ACBLobbyGameState* LobbyGameState = World->GetGameState<ACBLobbyGameState>())
	{
		LobbyGameState->NotifyReadyStateChanged();
	}
}

// 폰과 캐릭터 준비가 모두 갖춰졌을 때만 적용하는 진입점
void ACBPlayerState::ApplyCosmeticsWhenReady()
{
	// 폰이 아직 없음.
	ACBChaserCharacter* ChaserCharacter = Cast<ACBChaserCharacter>(GetPawn());
	if (!ChaserCharacter) return;

	// 준비가 끝났으면 바로 적용
	if (ChaserCharacter->IsCharacterSystemReady())
	{
		// 준비 완료 델리게이트를 거치지 않는 경로이므로 여기서도 로드 상태를 갱신해야 한다
		Local_RefreshCharacterLoaded();

		ApplyCosmeticsToPawn();
		return;
	}

	// 이미 같은 캐릭터의 준비 완료를 기다리는 중이면 중복 구독하지 않고 무시
	if (PendingReadyCharacter.Get() == ChaserCharacter && CharacterSystemReadyHandle.IsValid()) return;

	// 다른 캐릭터의 준비 완료를 기다리는 중이면 구독 해제
	if (ACBChaserCharacter* OldCharacter = PendingReadyCharacter.Get())
	{
		OldCharacter->OnCharacterSystemReadyDelegate.Remove(CharacterSystemReadyHandle);
	}

	// 새로운 캐릭터의 준비 완료를 기다림
	PendingReadyCharacter = ChaserCharacter;
	// 준비 완료 콜백 구독
	CharacterSystemReadyHandle = ChaserCharacter->OnCharacterSystemReadyDelegate.AddUObject(this, &ACBPlayerState::HandleCharacterSystemReady);
}

// 캐릭터 준비 완료 콜백
void ACBPlayerState::HandleCharacterSystemReady()
{
	// 구독 해제
	if (ACBChaserCharacter* ReadyCharacter = PendingReadyCharacter.Get())
	{
		ReadyCharacter->OnCharacterSystemReadyDelegate.Remove(CharacterSystemReadyHandle);
	}

	// 초기화
	PendingReadyCharacter.Reset();
	CharacterSystemReadyHandle.Reset();

	// 로드 완료를 위젯에 알림 (준비 버튼 잠금 해제)
	Local_RefreshCharacterLoaded();

	// 코스메틱 적용
	ApplyCosmeticsToPawn();
}

// 현재 폰의 준비 여부를 다시 판정하고, 값이 바뀌었을 때만 방송
void ACBPlayerState::Local_RefreshCharacterLoaded()
{
	// 폰이 없으면(재스폰 사이의 공백 등) 로드되지 않은 것으로 본다
	const ACBBaseCharacter* CBCharacter = Cast<ACBBaseCharacter>(GetPawn());
	const bool bNewLoaded = CBCharacter && CBCharacter->IsCharacterSystemReady();

	// 값이 그대로면 방송하지 않음 (위젯이 같은 상태로 반복 갱신되는 것을 막음)
	if (bCharacterLoaded == bNewLoaded) return;

	bCharacterLoaded = bNewLoaded;

	OnCharacterLoadedChanged.Broadcast(bCharacterLoaded);
}

void ACBPlayerState::ApplyCosmeticsToPawn()
{
	// 캐릭터 가져오기
	const ACBChaserCharacter* ChaserCharacter = Cast<ACBChaserCharacter>(GetPawn());
	if (!ChaserCharacter) return;

	// 모듈러 메시 컴포넌트 가져오기
	UCBModularMeshComponent* ModularMeshComponent = ChaserCharacter->GetModularMeshComponent();
	if (!ModularMeshComponent) return;

	// 슬롯별로 착용 파츠를 적용
	for (int32 SlotIndex = 0; SlotIndex < Cosmetics.Num(); ++SlotIndex)
	{
		// 빈 태그는 선택 해제라 건드리지 않음 (로드아웃의 기본 의상이 그대로 남음)
		if (!Cosmetics[SlotIndex].IsValid()) continue;

		// 태그에 맞는 파츠를 모듈러 메시 컴포넌트에 요청 (비동기 로드)
		ModularMeshComponent->RequestCosmeticPart(static_cast<ECBCosmeticSlot>(SlotIndex), Cosmetics[SlotIndex]);
	}
}