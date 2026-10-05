// project
#include "Controllers/CBAIController.h"
#include "Characters/CBAICharacter.h"
#include "AbilitySystem/CBAttributeSet.h"
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"

// engine
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISense_Damage.h"

// 블랙보드 타겟 키 이름 (에디터 BB 키 이름과 반드시 일치)
const FName ACBAIController::TargetActorKey(TEXT("TargetActor"));

// 블랙보드 경직 키 이름 (에디터 BB 키 이름과 반드시 일치)
const FName ACBAIController::StaggeredKey(TEXT("bIsStaggered"));

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

	// 피해 감각 설정 - 맞으면 가해자를 인지. (시야(전방)·청각(이동 소음)으로 못 잡는, 등 뒤에서 가만히 때리는 대상을 보완)
	// 자극 유지 시간은 RecentDamageMemoryTime 하나로 관리 (BP 값은 생성자에서 읽을 수 없어 빙의 시 다시 적용).
	// 소속 필터는 없지만 피격 자체가 적대 대상에게서만 오므로(무기 히트 필터) 문제없음.
	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));
	DamageConfig->SetMaxAge(RecentDamageMemoryTime);

	// 위에서 구성한 Sight/Hearing/Damage 설정을 퍼셉션 컴포넌트에 등록 (이 감각들로 감지를 수행)
	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->ConfigureSense(*HearingConfig);
	PerceptionComponent->ConfigureSense(*DamageConfig);
	// 지배 감각을 Sight로 지정 (여러 감각이 한 타겟을 감지할 때 최종 위치/상태의 기준이 되는 감각)
	PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	// 타겟 감지 상태가 바뀔 때(감지↔상실) 호출될 콜백 바인딩
	PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ACBAIController::HandleTargetPerceptionUpdated);

	// 공격 어빌리티가 도는 동안에는 타겟을 교체하지 않음 (부모 태그라 기본 공격·스킬 전부 매칭)
	TargetLockAbilityTags.AddTag(CBGameplayTags::Ability_Combat_Attack);
}

// [서버] 컨트롤러가 폰을 빙의할 때 호출. AI BT 시작을 준비 완료 시점까지 게이트.
void ACBAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 군중 회피 파라미터 적용 (이동 컴포넌트가 연결된 뒤여야 의미가 있음)
	ApplyCrowdAvoidanceSettings();

	// 빙의한 폰을 CB AI 캐릭터로 캐싱 (아니면 이후 로직 스킵)
	CachedAICharacter = Cast<ACBAICharacter>(InPawn);
	if (!CachedAICharacter) return;

	// 빙의로 이 컨트롤러의 진영이 확정됐으므로 퍼셉션에 재평가를 요청.
	// (퍼셉션 등록이 빙의보다 먼저면 팀 미확정 상태로 소속 필터가 계산돼 적을 놓침)
	if (PerceptionComponent)
	{
		PerceptionComponent->RequestStimuliListenerUpdate();

		// 피격 기억 시간을 피해 감각의 자극 유지 시간으로 적용 (컨트롤러 BP 에서 조정한 값 반영).
		// 이미 등록된 감각을 다시 구성하면 엔진이 유지 시간만 갱신함.
		if (DamageConfig)
		{
			DamageConfig->SetMaxAge(RecentDamageMemoryTime);
			PerceptionComponent->ConfigureSense(*DamageConfig);
		}
	}

	// 이미 준비 완료면 즉시 BT 시작, 아직이면 준비 완료 델리게이트에 바인딩해 대기
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

	// 폰의 ASC 에 걸어둔 구독 정리
	UnbindPawnASCEvents();

	Super::OnUnPossess();
}

// AI 두뇌 시작 진입점. 폰 ASC 구독 후 로드아웃이 주입한 BT를 구동. BT가 없으면 안전하게 스킵.
void ACBAIController::StartAILogic()
{
	// 폰 ASC 구독 일괄 (위협 판정 + 경직)
	// 준비 완료 이후라 폰의 ASC 가 확정돼 있음
	BindPawnASCEvents();

	// RunBehaviorTree가 BT에 지정된 Blackboard를 자동 세팅함 (별도 BB 참조 불필요).
	if (BehaviorTree)
	{
		RunBehaviorTree(BehaviorTree);

		// BT 시작으로 블랙보드가 준비된 뒤 1회 선정 (이미 시야에 있던 정지 타겟 놓침 방지)
		UpdateTarget();
	}
}

// [서버] 폰 ASC 에 거는 구독의 단일 진입점.
void ACBAIController::BindPawnASCEvents()
{
	// 이미 구독 중이면 스킵 (자식이 Super 를 중복 호출해도 넘어가도록 방어)
	if (CachedPawnASC.IsValid()) return;

	// 폰의 ASC 조회 (AI는 캐릭터가 ASC를 소유)
	UAbilitySystemComponent* ASC = UCBAbilitySystemLibrary::GetASC(GetPawn());
	if (!ASC) return;

	CachedPawnASC = ASC;

	BindHitReactEvent(*ASC);      // 위협 판정
	BindStaggerStateEvent(*ASC);  // BT 경직 분기
}

// [서버] 폰 ASC 구독 일괄 해제.
void ACBAIController::UnbindPawnASCEvents()
{
	if (UAbilitySystemComponent* ASC = CachedPawnASC.Get())
	{
		UnbindHitReactEvent(*ASC);
		UnbindStaggerStateEvent(*ASC);
	}

	CachedPawnASC.Reset();
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

		// 지정 시각 기록. 같은 대상을 다시 써도 갱신 (Outlaw 는 이걸로 집중을 다시 시작함)
		const UWorld* World = GetWorld();
		TargetAssignedTime = World ? World->GetTimeSeconds() : 0.f;

		// 첫 타겟 = 교전 개시. 사망까지 유지하는 복제 태그로 알림 (보스 바 등 클라 UI 가 구독).
		// 타겟을 잠깐 잃어도 떼지 않음 - 남은 플레이어가 인지 밖이라 타겟이 비는 순간마다 UI 가 꺼졌다 켜지지 않게.
		UAbilitySystemComponent* ASC = CachedPawnASC.Get();
		if (ASC && !ASC->HasMatchingGameplayTag(CBGameplayTags::Status_Combat_Engaged))
		{
			ASC->AddLooseGameplayTag(CBGameplayTags::Status_Combat_Engaged, 1, EGameplayTagReplicationState::TagOnly);
		}
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

	// 들고 있던 타겟이 없으면 최고 점수 후보를 즉시 반영 (후보도 없으면 nullptr 로 클리어)
	if (!CurrentTarget)
	{
		UpdateTargetInBlackboard(FindBestTarget());
		return;
	}

	// 전환 금지 구간(공격 몽타주 등)에서는 현재 타겟 유지
	if (!CanSwitchTarget()) return;

	// 교체 여부는 등급별 규칙이 판단 (베이스 = 점수 배수 비교)
	ReevaluateTarget(CurrentTarget);
}

// 현재 타겟을 들고 있을 때 교체 판단 - 베이스는 점수 배수 비교 (히스테리시스)
void ACBAIController::ReevaluateTarget(AActor* InCurrentTarget)
{
	AActor* BestTarget = FindBestTarget();

	// 현재 타겟보다 SwitchScoreRatio 배 이상 높을 때만 교체 (1.0으로 두면 튐 (히스테리시스))
	if (BestTarget && BestTarget != InCurrentTarget && ScoreTarget(BestTarget) > ScoreTarget(InCurrentTarget) * SwitchScoreRatio)
	{
		UpdateTargetInBlackboard(BestTarget);
	}
}

// 인지 중인 후보 중 최고 점수 대상 (InExclude 는 후보에서 뺌)
AActor* ACBAIController::FindBestTarget(const AActor* InExclude /* = nullptr */) const
{
	// 타겟 후보 수집 (nullptr = 감각 종류 무관, 시각·청각·피해 모두 포함)
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
		// 제외 대상이거나, 적이 아니거나 이미 죽은 후보는 제외
		if (Candidate == InExclude || !IsValidTarget(Candidate) || !IsTargetAlive(Candidate)) continue;

		// *후보 점수 계산
		const float Score = ScoreTarget(Candidate);

		// 최고 점수 후보 갱신
		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

// 대상이 최근에 자신을 때렸는지 (피해 감각 자극이 유지 시간 안에 살아 있는지)
bool ACBAIController::HasRecentlyDamagedMe(const AActor* InActor) const
{
	const UAIPerceptionComponent* Perception = GetAIPerceptionComponent();
	return InActor && Perception && Perception->HasActiveStimulus(*InActor, UAISense::GetSenseID<UAISense_Damage>());
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

	// 최근 피격: RecentDamageMemoryTime 안에 자신을 공격한 대상이면 RecentDamageBonus 가산.
	// 피해 감각이 가해자별로 기억하므로 여럿이 번갈아 때려도 각자 가산점을 받음.
	if (HasRecentlyDamagedMe(InActor))
	{
		Score += RecentDamageBonus;
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
void ACBAIController::BindHitReactEvent(UAbilitySystemComponent& InASC)
{
	// 이미 구독 중이면 스킵
	if (HitReactEventHandle.IsValid()) return;

	// 피격 이벤트 구독 (Event_Combat_HitReact 태그 이벤트)
	HitReactEventHandle = InASC.GenericGameplayEventCallbacks.FindOrAdd(CBGameplayTags::Event_Combat_HitReact)
		.AddUObject(this, &ACBAIController::HandleHitReactEvent);
}

// [서버] 피격 반응 이벤트 구독 해제
void ACBAIController::UnbindHitReactEvent(UAbilitySystemComponent& InASC)
{
	// 피격 이벤트 구독중이라면 해제
	if (HitReactEventHandle.IsValid())
	{
		InASC.GenericGameplayEventCallbacks.FindOrAdd(CBGameplayTags::Event_Combat_HitReact).Remove(HitReactEventHandle);
	}

	HitReactEventHandle.Reset();
}

// 피격 반응 이벤트 콜백 - 가해자를 피해 감각에 보고만 하고, 전환 여부는 UpdateTarget 이 판단
void ACBAIController::HandleHitReactEvent(const FGameplayEventData* Payload)
{
	APawn* SelfPawn = GetPawn();
	if (!Payload || !SelfPawn) return;

	// 가해자 소유 폰 가져오기
	// 플레이어는 ASC 소유자가 PlayerState라 폰이 아니라, 폰으로 변환해야 함
	const AActor* ThreatPawn = UCBAbilitySystemLibrary::ResolveOwningPawn(Payload->Instigator.Get());
	if (!ThreatPawn) return;

	// 피해 감각에 보고 → 다음 퍼셉션 갱신에서 가해자가 인지됨 (OnTargetPerceptionUpdated → UpdateTarget).
	// 가해자별로 기억되어 HasRecentlyDamagedMe·점수 가산의 근거가 됨. 엔진 API 가 비const 라 const_cast (읽기만 함)
	UAISense_Damage::ReportDamageEvent(this, SelfPawn, const_cast<AActor*>(ThreatPawn), Payload->EventMagnitude,
		ThreatPawn->GetActorLocation(), SelfPawn->GetActorLocation());
}
#pragma endregion

#pragma region Stagger
// [서버] 경직 상태 태그 구독 (피격 어빌리티가 ActivationOwnedTags 로 부여하는 태그)
void ACBAIController::BindStaggerStateEvent(UAbilitySystemComponent& InASC)
{
	// 이미 구독 중이면 스킵
	if (StaggerTagHandle.IsValid()) return;

	// 태그 추가/제거 이벤트 구독
	StaggerTagHandle = InASC.RegisterGameplayTagEvent(
		CBGameplayTags::Status_Combat_Staggered, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ACBAIController::OnStaggerTagChanged);

	// 현재 값을 1회 반영. (구독 전 상태 갱신)
	OnStaggerTagChanged(CBGameplayTags::Status_Combat_Staggered,
		InASC.GetTagCount(CBGameplayTags::Status_Combat_Staggered));
}

// [서버] 경직 상태 태그 구독 해제
void ACBAIController::UnbindStaggerStateEvent(UAbilitySystemComponent& InASC)
{
	if (StaggerTagHandle.IsValid())
	{
		InASC.RegisterGameplayTagEvent(
			CBGameplayTags::Status_Combat_Staggered, EGameplayTagEventType::NewOrRemoved)
			.Remove(StaggerTagHandle);
	}

	StaggerTagHandle.Reset();
}

// 경직 태그 변화 콜백 - 블랙보드에 그대로 반영. (경직 중 무엇을 할지는 BT 가 판단)
void ACBAIController::OnStaggerTagChanged(const FGameplayTag /*CallbackTag*/, int32 NewCount)
{
	// 블랙보드 미준비(BT 미시작) 시 무시
	UBlackboardComponent* BB = GetBlackboardComponent();
	if (!BB) return;

	BB->SetValueAsBool(StaggeredKey, NewCount > 0);
}
#pragma endregion
