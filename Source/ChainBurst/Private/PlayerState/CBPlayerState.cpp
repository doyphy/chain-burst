// project
#include "PlayerState/CBPlayerState.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem//CBAttributeSet.h"
#include "Characters/CBChaserCharacter.h"
#include "Components/Mesh/CBModularMeshComponent.h"

// engine
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
}

void ACBPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 다른 플레이어의 외형도 보여야 하므로 전 클라이언트에 복제
	DOREPLIFETIME(ACBPlayerState, Cosmetics);
}

// seamless travel 로 맵을 넘어갈 때 엔진이 새 PlayerState 를 만든 뒤 호출
void ACBPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);

	// 로비에서 고른 의상이 새로운 레벨에서도 유지되도록 Cosmetics 배열을 새 PlayerState 에 복사
	if (ACBPlayerState* NewCBPlayerState = Cast<ACBPlayerState>(NewPlayerState))
	{
		NewCBPlayerState->Cosmetics = Cosmetics;
	}
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
	ApplyCosmeticsToPawn();
}

// 폰이 늦게 생기는 순서를 처리하기 위해 폰 연결 시에도 호출
void ACBPlayerState::HandlePawnSet(APlayerState* InPlayerState, APawn* InNewPawn, APawn* InOldPawn)
{
	// 코스메틱 적용
	ApplyCosmeticsToPawn();
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
