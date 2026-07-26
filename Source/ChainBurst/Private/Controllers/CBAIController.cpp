// project
#include "Controllers/CBAIController.h"
#include "Characters/CBAICharacter.h"
#include "Characters/CBChaserCharacter.h"

// engine
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "BehaviorTree/BlackboardComponent.h"

// 블랙보드 타겟 키 이름 (에디터 BB 키 이름과 반드시 일치)
const FName ACBAIController::TargetActorKey(TEXT("TargetActor"));

ACBAIController::ACBAIController()
{
	// 시야 퍼셉션 컴포넌트 생성 (AAIController 내장 멤버에 할당)
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));

	// 시야 감각 설정 (값은 하드코딩, 필요 시 추후 노출)
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1500.f; // 최초 발견 거리 (아직 못 본 타겟을 발견할 수 있는 최대 거리)
	SightConfig->LoseSightRadius = 2000.f; // 이미 발견한 타겟을 놓치는 거리 (SightRadius보다 크게 둬 발견↔상실 깜빡임 방지)
	SightConfig->PeripheralVisionAngleDegrees = 60.f; // 전방 기준 좌우 각각 60° (= 전체 시야 120°)
	SightConfig->SetMaxAge(5.f); // 감지 자극을 기억하는 시간(초). 0이면 무한, 지나면 만료(망각)
	
	// 팀이 없어 모두 Neutral이므로 일단 전부 감지 (실제 타겟 선별은 IsValidTarget)
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	// 청각 감각 설정 — 전방위이며 LOS와 무관. 시야 사각지대(등 뒤)를 소리로 보완.
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 1500.f; // 기준 청취 거리. 실제 유효 거리 = 이 값 x 소음 Loudness
	HearingConfig->SetMaxAge(3.f); // 들은 소리를 기억하는 시간(초). 지나면 만료되어 타겟 해제
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

	// 위에서 구성한 Sight/Hearing 설정을 퍼셉션 컴포넌트에 등록 (이 감각들로 감지를 수행)
	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->ConfigureSense(*HearingConfig);
	// 지배 감각을 Sight로 지정 (여러 감각이 한 타겟을 감지할 때 최종 위치/상태의 기준이 되는 감각)
	PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	// 타겟 감지 상태가 바뀔 때(감지↔상실) 호출될 콜백 바인딩
	PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ACBAIController::HandleTargetPerceptionUpdated);
}

// [서버] 컨트롤러가 폰을 빙의할 때 호출. AI 두뇌 시작을 준비 완료 시점까지 게이트.
void ACBAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 빙의한 폰을 CB AI 캐릭터로 캐싱 (아니면 이후 로직 스킵)
	CachedAICharacter = Cast<ACBAICharacter>(InPawn);
	if (!CachedAICharacter) return;

	// 이미 준비 완료면 즉시 두뇌 시작, 아직이면 준비 완료 델리게이트에 바인딩해 대기
	if (CachedAICharacter->IsCharacterSystemReady())
	{
		StartAILogic();
	}
	else
	{
		SystemReadyHandle = CachedAICharacter->OnCharacterSystemReadyDelegate.AddUObject(this, &ACBAIController::StartAILogic);
	}
}

// [서버] 빙의 해제 시 캐싱·델리게이트 정리
void ACBAIController::OnUnPossess()
{
	// 준비 대기 중이었다면 델리게이트 바인딩 해제
	if (CachedAICharacter && SystemReadyHandle.IsValid())
	{
		CachedAICharacter->OnCharacterSystemReadyDelegate.Remove(SystemReadyHandle);
	}
	SystemReadyHandle.Reset();
	CachedAICharacter = nullptr;

	Super::OnUnPossess();
}

// AI 두뇌 시작 진입점. 베이스는 비어 있으며 자식이 오버라이드해 두뇌를 구동한다.
void ACBAIController::StartAILogic()
{
}

#pragma region Perception
// 감지 결과 갱신 콜백 (감지/상실 상태 변화 시)
void ACBAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// 타겟 후보가 아니면 무시 (Phase 1: 플레이어 클래스 필터)
	if (!IsValidTarget(Actor)) return;

	if (Stimulus.WasSuccessfullySensed())
	{
		// 감지: 타겟 설정
		UpdateTargetInBlackboard(Actor);
	}
	else
	{
		// 상실: 현재 타겟이 이 액터일 때만 클리어 (다른 타겟 추적 중이면 유지)
		const UBlackboardComponent* BB = GetBlackboardComponent();
		if (BB && BB->GetValueAsObject(TargetActorKey) == Actor)
		{
			UpdateTargetInBlackboard(nullptr);
		}
	}
}

// 이 액터를 타겟으로 삼을지 판정 (적 판정 seam)
bool ACBAIController::IsValidTarget(AActor* InActor) const
{
	// Phase 1: 플레이어(Chaser) 클래스 필터. Phase 2: 팀(진영) 판정으로 교체 예정.
	return InActor && InActor->IsA<ACBChaserCharacter>();
}

// BT 시작 시점에 이미 시야에 있던 정지 타겟을 시드
void ACBAIController::SeedTargetFromCurrentPerception()
{
	UAIPerceptionComponent* Perception = GetAIPerceptionComponent();
	if (!Perception) return;

	// 현재 인지된 액터 중 유효 타겟을 찾아 시드 (nullptr = 감각 종류 무관, 시각·청각 모두 포함)
	TArray<AActor*> PerceivedActors;
	Perception->GetCurrentlyPerceivedActors(nullptr, PerceivedActors);
	for (AActor* Actor : PerceivedActors)
	{
		if (IsValidTarget(Actor))
		{
			UpdateTargetInBlackboard(Actor);
			return;
		}
	}
}

// 블랙보드 TargetActor 갱신 (감지=세팅 / 상실·무효=클리어). 블랙보드 미준비 시 무시.
void ACBAIController::UpdateTargetInBlackboard(AActor* InTarget)
{
	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB) return;

	if (InTarget)
	{
		BB->SetValueAsObject(TargetActorKey, InTarget);
	}
	else
	{
		BB->ClearValue(TargetActorKey);
	}
}
#pragma endregion
