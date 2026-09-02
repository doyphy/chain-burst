// project
#include "Controllers/CBAIController.h"
#include "Characters/CBAICharacter.h"
#include "AbilitySystem/CBAttributeSet.h"
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"

// engine
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/PlayerState.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"

// 블랙보드 타겟 키 이름 (에디터 BB 키 이름과 반드시 일치)
const FName ACBAIController::TargetActorKey(TEXT("TargetActor"));

// 경로 추종 컴포넌트를 군중 회피(Detour Crowd) 버전으로 교체.
// 엔진 ADetourCrowdAIController 가 하는 일과 같으며, 베이스에서 하므로 Rogue·Outlaw 가 모두 물려받음.
ACBAIController::ACBAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
	// 시야 퍼셉션 컴포넌트 생성 (AAIController 내장 멤버에 할당)
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));

	// 시야 감각 설정 (값은 하드코딩, 필요 시 추후 노출)
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1500.f; // 최초 발견 거리 (아직 못 본 타겟을 발견할 수 있는 최대 거리)
	SightConfig->LoseSightRadius = 2000.f; // 이미 발견한 타겟을 놓치는 거리 (SightRadius보다 크게 둬 발견↔상실 깜빡임 방지)
	SightConfig->PeripheralVisionAngleDegrees = 60.f; // 전방 기준 좌우 각각 60° (= 전체 시야 120°)
	SightConfig->SetMaxAge(5.f); // 감지 자극을 기억하는 시간(초). 0이면 무한, 지나면 만료(망각)
	
	// 적(진영이 다른 대상)만 감지. 아군·중립은 퍼셉션 단계에서 잘려 자극조차 오지 않는다.
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	// 청각 감각 설정 — 전방위이며 LOS와 무관. 시야 사각지대(등 뒤)를 소리로 보완.
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 1500.f; // 기준 청취 거리. 실제 유효 거리 = 이 값 x 소음 Loudness
	HearingConfig->SetMaxAge(3.f); // 들은 소리를 기억하는 시간(초). 지나면 만료되어 타겟 해제
	
	// 청각도 적만 감지. 단 청각은 인터페이스가 아니라 팀 ID 로 직접 비교하므로(엔진 UAISense_Hearing::Update)
	// 판정은 UCBGameInstance에서 재정의한 attitude solver 를 따름.
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = false;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;

	// 위에서 구성한 Sight/Hearing 설정을 퍼셉션 컴포넌트에 등록 (이 감각들로 감지를 수행)
	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->ConfigureSense(*HearingConfig);
	// 지배 감각을 Sight로 지정 (여러 감각이 한 타겟을 감지할 때 최종 위치/상태의 기준이 되는 감각)
	PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	// 타겟 감지 상태가 바뀔 때(감지↔상실) 호출될 콜백 바인딩
	PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ACBAIController::HandleTargetPerceptionUpdated);

	// 공격 어빌리티가 도는 동안에는 타겟을 교체하지 않는다 (부모 태그라 기본 공격·스킬 전부 매칭)
	TargetLockAbilityTags.AddTag(CBGameplayTags::Ability_Combat_Attack);
}

// [서버] 컨트롤러가 폰을 빙의할 때 호출. AI 두뇌 시작을 준비 완료 시점까지 게이트.
void ACBAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 군중 회피 파라미터 적용 (이동 컴포넌트가 연결된 뒤여야 의미가 있음)
	ApplyCrowdAvoidanceSettings();

	// 빙의한 폰을 CB AI 캐릭터로 캐싱 (아니면 이후 로직 스킵)
	CachedAICharacter = Cast<ACBAICharacter>(InPawn);
	if (!CachedAICharacter) return;

	// 빙의로 이 컨트롤러의 진영이 확정됐으므로 퍼셉션에 재평가를 요청한다.
	// (퍼셉션 등록이 빙의보다 먼저면 팀 미확정 상태로 소속 필터가 계산돼 적을 놓친다)
	if (PerceptionComponent)
	{
		PerceptionComponent->RequestStimuliListenerUpdate();
	}

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

	// 폰의 ASC 에 걸어둔 피격 이벤트 구독 정리
	UnbindHitReactEvent();

	Super::OnUnPossess();
}

// AI 두뇌 시작 진입점. 베이스는 위협 판정용 구독만 하고, 두뇌 구동은 자식이 담당한다.
void ACBAIController::StartAILogic()
{
	// 피격 이벤트 구독 (위협 판정용)
	// 준비 완료 이후라 폰의 ASC 가 확정돼 있음
	BindHitReactEvent();
}

// 빙의한 폰의 팀 ID를 반환
FGenericTeamId ACBAIController::GetGenericTeamId() const
{
	// 자신의 소유 액터 가져오기
	const AActor* TeamOwner = CachedAICharacter ? static_cast<const AActor*>(CachedAICharacter) : GetPawn();
	
	// 인터페이스 상속했는지 확인
	if (const IGenericTeamAgentInterface* TeamAgent = Cast<const IGenericTeamAgentInterface>(TeamOwner))
	{
		// 자신의 팀 ID 반환
		return TeamAgent->GetGenericTeamId();
	}

	return FGenericTeamId::NoTeam;
}

#pragma region CrowdAvoidance
// [서버] 군중 회피 파라미터 적용
void ACBAIController::ApplyCrowdAvoidanceSettings()
{
	UCrowdFollowingComponent* CrowdComp = Cast<UCrowdFollowingComponent>(GetPathFollowingComponent());
	if (!CrowdComp) return;

	// 장애물 회피는 엔진 기본으로 켜져 있고, 서로 간격을 벌리는 분리는 기본 꺼짐이라 여기서 결정
	CrowdComp->SetCrowdSeparation(bUseCrowdSeparation);
	CrowdComp->SetCrowdSeparationWeight(CrowdSeparationWeight);
	CrowdComp->SetCrowdCollisionQueryRange(CrowdCollisionQueryRange);

	// 회피 품질. 엔진 기본값 Low 는 여럿이 몰릴 때 겹침이 남아 Medium 으로 올림.
	// ECrowdAvoidanceQuality는 UENUM이 아니라 UPROPERTY로 노출할 수 없어 코드로 설정.
	CrowdComp->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Medium);
}
#pragma endregion

#pragma region Perception
// 감지 결과 갱신 콜백 (감지/상실 상태 변화 시)
void ACBAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// 아군·중립 자극은 무시 (퍼셉션 소속 필터와 같은 기준)
	if (!IsValidTarget(Actor)) return;

	// 감지 상태 변화 시 타겟 후보를 재평가해 블랙보드 갱신
	UpdateTarget();
}

// 이 액터를 타겟으로 삼을지 판정 (적 판정)
bool ACBAIController::IsValidTarget(AActor* InActor) const
{
	if (!IsValid(InActor)) return false;

	// 진영이 적대인 대상만 타겟. (A:자신, B:상대), 규칙은 CBGameInstance 에서 커스텀한 규칙을 따름.
	return FGenericTeamId::GetAttitude(this, InActor) == ETeamAttitude::Hostile;
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

#pragma region Targeting
// [서버] 후보를 재평가해 블랙보드 타겟을 갱신
void ACBAIController::UpdateTarget()
{
	// 블랙보드가 없으면(BT 미시작) 결과를 쓸 곳이 없으므로 판단 자체를 생략
	const UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB) return;

	// 현재 타겟팅 액터 가져오기
	AActor* CurrentTarget = Cast<AActor>(BB->GetValueAsObject(TargetActorKey));

	// 현재 타겟팅 액터가 유지 불가(사망·인지 상실·파괴)인지 판정. 유지 불가면 즉시 클리어.
	if (CurrentTarget && !IsTargetStillValid(CurrentTarget))
	{
		CurrentTarget = nullptr;
	}

	// 타겟 후보 수집 (nullptr = 감각 종류 무관, 시각·청각 모두 포함)
	TArray<AActor*> PerceivedActors;
	if (const UAIPerceptionComponent* Perception = GetAIPerceptionComponent())
	{
		Perception->GetCurrentlyPerceivedActors(nullptr, PerceivedActors);
	}

	// 최고 점수 후보 계산
	AActor* BestTarget = nullptr;
	float BestScore = 0.f;
	
	// 타겟 후보 순회
	for (AActor* Candidate : PerceivedActors)
	{
		// 적이 아니거나 이미 죽은 후보는 제외
		if (!IsValidTarget(Candidate) || !IsTargetAlive(Candidate)) continue;

		// *후보 점수 계산
		const float Score = ScoreTarget(Candidate);
		
		// 최고 점수 후보 갱신
		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	// 들고 있던 타겟이 없으면 즉시 반영 (후보도 없으면 BestTarget = nullptr 로 클리어)
	if (!CurrentTarget)
	{
		// 블랙보드 타겟 갱신 (nullptr = 클리어)
		UpdateTargetInBlackboard(BestTarget);
		return;
	}

	// 전환 금지 구간(공격 몽타주 등)에서는 현재 타겟 유지
	if (!CanSwitchTarget()) return;

	// 현재 타겟보다 SwitchScoreRatio 배 이상 높을 때만 교체 (1.0으로 두면 튐 (히스테리시스))
	if (BestTarget && BestTarget != CurrentTarget && BestScore > ScoreTarget(CurrentTarget) * SwitchScoreRatio)
	{
		// 블랙보드 타겟 갱신
		UpdateTargetInBlackboard(BestTarget);
	}
}

// 후보의 우선순위 점수 계산 (거리 + 시야 확보 + 최근 피격)
float ACBAIController::ScoreTarget(AActor* InActor) const
{
	// 자신(AI)의 폰과 후보(Target)의 폰이 유효하지 않으면 점수 계산 불가 (0 반환)
	const APawn* SelfPawn = GetPawn();
	if (!IsValid(InActor) || !SelfPawn) return 0.f;

	// 기본 점수 설정 (배수 비교(SwitchScoreRatio)가 성립하려면 점수가 0이 되면 안 됨)
	float Score = BaseTargetScore;

	// 거리: 가까울수록 높게 (MaxScoreDistance 기준 0~1 정규화)
	// 자신(AI)과 후보(Target)의 거리 계산
	const float Distance = FVector::Dist(SelfPawn->GetActorLocation(), InActor->GetActorLocation());
	// MaxScoreDistance 이상이면 1, 이하이면 0~1 비율로 정규화
	const float DistanceRatio = FMath::Clamp(Distance / FMath::Max(MaxScoreDistance, KINDA_SMALL_NUMBER), 0.f, 1.f);
	// 1 - DistanceRatio = 가까울수록 점수 높음. DistanceWeight 배수로 가산.
	Score += DistanceWeight * (1.f - DistanceRatio);

	// 시야 확보: 소리만 들리는 후보보다 눈에 보이는 후보를 우선
	if (const UAIPerceptionComponent* Perception = GetAIPerceptionComponent())
	{
		// 시야 감각으로 인지 중이면 SightBonus 가산
		if (Perception->HasActiveStimulus(*InActor, UAISense::GetSenseID<UAISense_Sight>()))
		{
			Score += SightBonus;
		}
	}

	// 최근 피격: 최근에 자신을 공격한 대상이면 (Event_Combat_HitReact 태그 이벤트 콜백 함수에서 갱신)
	if (LastDamageTime >= 0.f && LastDamageInstigator.Get() == InActor)
	{
		// 현재 시간 가져오기
		const UWorld* World = GetWorld();
		const float Now = World ? World->GetTimeSeconds() : 0.f;
		
		// 최근 피격 기억 시간 이내라면 RecentDamageBonus 가산
		if (Now - LastDamageTime <= RecentDamageMemoryTime)
		{
			Score += RecentDamageBonus;
		}
	}

	return Score;
}

// 지금 타겟을 교체해도 되는지 판정 (전환 금지 구간)
bool ACBAIController::CanSwitchTarget() const
{
	// 등록된 잠금 태그가 없으면 교체 허용
	if (TargetLockAbilityTags.IsEmpty()) return true;

	const UAbilitySystemComponent* ASC = UCBAbilitySystemLibrary::GetASC(GetPawn());
	if (!ASC) return true;

	// 잠금 태그에 해당하는 어빌리티가 활성 중이면 교체 금지 (공격중에는 타겟을 바꾸지 않음 등)
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.IsActive() && Spec.Ability->GetAssetTags().HasAny(TargetLockAbilityTags))
		{
			return false;
		}
	}

	return true;
}

// 현재 타겟을 계속 들고 있어도 되는지 판정
bool ACBAIController::IsTargetStillValid(AActor* InActor) const
{
	// 파괴됐거나 적이 아니면 유지 불가
	if (!IsValidTarget(InActor)) return false;

	// 사망한 대상은 즉시 버린다 (전환 금지 구간의 예외)
	if (!IsTargetAlive(InActor)) return false;

	// 어느 감각으로든 아직 인지 중이어야 유지
	const UAIPerceptionComponent* Perception = GetAIPerceptionComponent();
	return Perception && Perception->HasAnyCurrentStimulus(*InActor);
}

// 대상이 살아 있는지 판정 (CurrentHealth 어트리뷰트)
bool ACBAIController::IsTargetAlive(const AActor* InActor) const
{
	const UAbilitySystemComponent* ASC = UCBAbilitySystemLibrary::GetASC(InActor);
	if (!ASC) return true;

	// 체력 어트리뷰트가 없는 대상은 생존으로 간주 (사망 개념이 없는 액터)
	bool bFound = false;
	const float Health = ASC->GetGameplayAttributeValue(UCBAttributeSet::GetCurrentHealthAttribute(), bFound);
	return !bFound || Health > 0.f;
}

// [서버] 피격 반응 이벤트 구독 (위협 판정 입력)
void ACBAIController::BindHitReactEvent()
{
	// 이미 구독 중이면 스킵
	// 자식이 Super 를 중복 호출해도 넘어가도록 방어
	if (HitReactEventHandle.IsValid()) return;

	// 폰의 ASC 조회 (AI는 캐릭터가 ASC를 소유)
	UAbilitySystemComponent* ASC = UCBAbilitySystemLibrary::GetASC(GetPawn());
	if (!ASC) return;

	// 피격 이벤트 구독 (Event_Combat_HitReact 태그 이벤트)
	CachedThreatASC = ASC;
	HitReactEventHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(CBGameplayTags::Event_Combat_HitReact)
		.AddUObject(this, &ACBAIController::HandleHitReactEvent);
}

// [서버] 피격 반응 이벤트 구독 해제
void ACBAIController::UnbindHitReactEvent()
{
	if (UAbilitySystemComponent* ASC = CachedThreatASC.Get())
	{
		// 피격 이벤트 구독중이라면
		if (HitReactEventHandle.IsValid())
		{
			// 피격 이벤트 구독 해제
			ASC->GenericGameplayEventCallbacks.FindOrAdd(CBGameplayTags::Event_Combat_HitReact).Remove(HitReactEventHandle);
		}
	}

	// 캐싱·핸들 초기화
	HitReactEventHandle.Reset();
	CachedThreatASC.Reset();
	LastDamageInstigator.Reset();
	LastDamageTime = -1.f;
}

// 피격 반응 이벤트 콜백 — 누가 때렸는지만 기록하고, 전환 여부는 UpdateTarget 이 판단
void ACBAIController::HandleHitReactEvent(const FGameplayEventData* Payload)
{
	if (!Payload) return;

	// 가해자 소유 폰 가져오기
	// 플레이어는 ASC 소유자가 PlayerState라 폰이 아니라, 폰으로 변환해야 함
	const AActor* ThreatPawn = ResolveThreatPawn(Payload->Instigator.Get());
	if (!ThreatPawn) return;

	const UWorld* World = GetWorld();
	LastDamageInstigator = ThreatPawn;
	LastDamageTime = World ? World->GetTimeSeconds() : 0.f;
}

// 가해자 액터를 가해자 소유 폰으로 변환. (플레이어는 ASC 소유자가 PlayerState라 폰이 아님)
const AActor* ACBAIController::ResolveThreatPawn(const AActor* InActor)
{
	if (!IsValid(InActor)) return nullptr;

	// 폰이면 그대로 (AI 는 캐릭터가 ASC 를 소유하므로 여기서 끝남)
	if (const APawn* Pawn = Cast<APawn>(InActor)) return Pawn;

	// 컨트롤러·PlayerState 는 조종 중인 폰으로 환원 (플레이어는 PlayerState 가 ASC 소유자)
	if (const AController* Controller = Cast<AController>(InActor)) return Controller->GetPawn();
	if (const APlayerState* PlayerState = Cast<APlayerState>(InActor)) return PlayerState->GetPawn();

	return InActor;
}
#pragma endregion
