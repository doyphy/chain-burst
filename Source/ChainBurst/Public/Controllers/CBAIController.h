#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "Perception/AIPerceptionTypes.h"
#include "CBAIController.generated.h"

class ACBAICharacter;
class UBehaviorTree;
class UAbilitySystemComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UAISenseConfig_Damage;
struct FGameplayEventData;

/**
 * AI 컨트롤러 공통 베이스 (Outlaw·Rogue 등).
 * 로드아웃이 주입한 비헤이비어 트리를 캐릭터의 준비 완료(SystemReady) 신호 이후에 구동.
 * 시야·청각·피해 퍼셉션으로 적(진영이 다른 대상)을 감지하고, 후보들을 점수화해 블랙보드 타겟을 선정·갱신.
 * 현재 타겟을 들고 있을 때 교체할지는 ReevaluateTarget()이 정함 (베이스 = 점수 배수 비교, 등급별 오버라이드).
 * 경로 추종은 군중 회피(Detour Crowd)를 사용해 여러 AI 가 서로 겹치지 않게 이동.
 * BT가 아닌 다른 두뇌가 필요한 자식은 StartAILogic()을 오버라이드. 직접 스폰하지 않는 추상 클래스.
 */
UCLASS(Abstract)
class CHAINBURST_API ACBAIController : public AAIController
{
	GENERATED_BODY()

public:
	ACBAIController(const FObjectInitializer& ObjectInitializer);

	/** [서버] 로드아웃이 이 컨트롤러에 실행할 비헤이비어 트리를 주입하는 세터. */
	FORCEINLINE void SetBehaviorTree(UBehaviorTree* InBehaviorTree) { BehaviorTree = InBehaviorTree; }

	//~ Begin IGenericTeamAgentInterface Interface.
	/** AI 퍼셉션이 진영을 물을 때 쓰는 값. */
	virtual FGenericTeamId GetGenericTeamId() const override;
	//~ End IGenericTeamAgentInterface Interface.

protected:
	//~ Begin AController Interface
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	//~ End AController Interface

	/**
	 * AI 두뇌 시작 진입점. 캐릭터가 준비 완료(SystemReady)된 뒤 1회 호출.
	 * 베이스는 폰 ASC 구독(위협 판정·경직) 후 주입된 BT를 구동함 (BT가 없으면 두뇌 없이 대기).
	 * 다른 두뇌로 오버라이드할 때도 ASC 구독을 위해 반드시 Super를 호출할 것 (로드아웃 BT를 비우면 BT 구동은 건너뜀).
	 */
	virtual void StartAILogic();

	/** 빙의한 CB AI 캐릭터 (타입 캐싱). OnPossess에서 세팅, OnUnPossess에서 해제. */
	UPROPERTY(Transient)
	TObjectPtr<ACBAICharacter> CachedAICharacter = nullptr;

	FORCEINLINE ACBAICharacter* GetCachedAICharacter() const { return CachedAICharacter.Get(); }

	/** [서버] 로드아웃에서 주입된 비헤이비어 트리 (하드 참조로 생존 보장). StartAILogic에서 구동. */
	UPROPERTY(Transient)
	TObjectPtr<UBehaviorTree> BehaviorTree = nullptr;

private:
	/** [서버] 폰 ASC 이벤트 구독.  */
	void BindPawnASCEvents();
	/** [서버] 폰 ASC 구독 정리. */
	void UnbindPawnASCEvents();

	/** 구독을 건 폰의 ASC (해제용). */
	TWeakObjectPtr<UAbilitySystemComponent> CachedPawnASC;

#pragma region Perception
	/** 시야·청각·피해 퍼셉션으로 적을 감지 → 타겟 재선정 요청. */
protected:
	/** 감지 결과가 갱신될 때 호출 (감지/상실 상태 변화 시). */
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** 이 액터를 타겟으로 삼을지 판정. (적 판정) */
	virtual bool IsValidTarget(AActor* InActor) const;

	/**
	 * 블랙보드 TargetActor 키에 값을 쓰거나(감지) 지움(상실/무효). 블랙보드 미준비 시 무시.
	 * 타겟을 쓸 때마다 지정 시각(TargetAssignedTime)을 기록함 - 같은 대상을 다시 써도 갱신 (집중 재시작).
	 * 첫 타겟을 쓰는 순간 폰 ASC 에 교전 개시 태그(Status.Combat.Engaged, 복제)를 붙여 사망까지 유지.
	 */
	void UpdateTargetInBlackboard(AActor* InTarget);

	/** 시야 감각(Sight) 설정. 전방 부채꼴 + 시야 차단(LOS) 적용. */
	UPROPERTY(VisibleAnywhere, Category = "ChainBurst|AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig = nullptr;

	/**
	 * 청각(Hearing) 설정. 전방위이며 시야각·LOS와 무관하게 소음을 감지.
	 * 소음은 캐릭터의 UCBNoiseEmitterComponent가 이동 중일 때만 발생시키므로, 정지한 대상은 들리지 않음.
	 */
	UPROPERTY(VisibleAnywhere, Category = "ChainBurst|AI|Perception")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig = nullptr;

	/**
	 * 피해(Damage) 설정. 맞으면 가해자를 인지 - 등 뒤에서 가만히 서서 때리는 대상도 후보가 됨.
	 * 피격 이벤트에서 ReportDamageEvent 로 보고하며, 자극 유지 시간은 RecentDamageMemoryTime (빙의 시 적용).
	 */
	UPROPERTY(VisibleAnywhere, Category = "ChainBurst|AI|Perception")
	TObjectPtr<UAISenseConfig_Damage> DamageConfig = nullptr;

public:
	/**
	 * 블랙보드 타겟 키 이름 (에디터 BB 키 이름과 반드시 일치).
	 * EQS 컨텍스트 등 블랙보드에서 타겟을 읽는 쪽이 이 상수를 공유해 키 이름이 여러 곳에 중복 선언되는 것을 막음.
	 */
	static const FName TargetActorKey;
#pragma endregion

#pragma region Targeting
	/**
	 * 타겟 선정. 인지 중인 적 후보를 점수화해 블랙보드 TargetActor를 갱신.
	 */
public:
	/** [서버] 후보를 재평가해 블랙보드 타겟을 갱신. UCBBTService_UpdateTarget·퍼셉션 콜백이 호출. */
	virtual void UpdateTarget();

protected:
	/**
	 * 후보의 우선순위 점수를 계산 (높을수록 우선).
	 * 클래스별, 등급별, 상황별 등 점수를 커스텀하려면 이 함수만 오버라이드.
	 * @param InActor 점수를 매길 후보
	 * @return 후보의 점수. 무효한 후보는 0.
	 */
	virtual float ScoreTarget(AActor* InActor) const;

	/**
	 * 현재 타겟을 들고 있을 때 교체할지 판단하고, 교체하면 블랙보드에 씀 (전환 금지 구간을 통과한 뒤 호출).
	 * 베이스: 최고 점수 후보가 현재 타겟 점수의 SwitchScoreRatio 배를 넘으면 교체 (히스테리시스).
	 * 등급별 교체 규칙은 이 함수를 오버라이드 (예: Outlaw 는 집중 시간 규칙).
	 * @param InCurrentTarget 유효성 검사를 통과한 현재 타겟
	 */
	virtual void ReevaluateTarget(AActor* InCurrentTarget);

	/**
	 * 인지 중인 후보 중 최고 점수 대상을 찾음 (적 판정 + 생존).
	 * @param InExclude 후보에서 뺄 대상 (없으면 nullptr)
	 * @return 최고 점수 후보. 없으면 nullptr
	 */
	AActor* FindBestTarget(const AActor* InExclude = nullptr) const;

	/**
	 * 대상이 최근(RecentDamageMemoryTime 안)에 자신을 때렸는지 판정 (피해 감각 자극이 살아 있는지).
	 * @param InActor 검사할 대상
	 */
	bool HasRecentlyDamagedMe(const AActor* InActor) const;

	/** 현재 타겟을 블랙보드에 마지막으로 쓴 시각(초). 같은 대상을 다시 써도 갱신됨. 음수면 기록 없음 */
	FORCEINLINE float GetTargetAssignedTime() const { return TargetAssignedTime; }

	/**
	 * 지금 타겟을 교체해도 되는지 판정 (전환 금지 구간).
	 * @return 교체 가능하면 true
	 */
	virtual bool CanSwitchTarget() const;

	/**
	 * 현재 타겟을 계속 들고 있어도 되는지 판정 (적 판정 + 생존 + 인지 유지).
	 * 등급별로 유지 조건을 바꾸려면 오버라이드 (예: Outlaw는 인지 유지 조건을 뺌).
	 * @param InActor 검사할 현재 타겟
	 * @return 유지 가능하면 true
	 */
	virtual bool IsTargetStillValid(AActor* InActor) const;

	/**
	 * 대상이 살아 있는지 판정 (CurrentHealth 어트리뷰트).
	 * 체력 개념이 없는 대상(ASC·어트리뷰트 없음)은 생존으로 간주.
	 * @param InActor 검사할 액터
	 * @return 살아 있으면 true
	 */
	bool IsTargetAlive(const AActor* InActor) const;

	/** 거리 점수의 정규화 기준 (cm). 이 거리 이상은 거리 점수 0. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|AI|Targeting", meta = (ClampMin = "1.0"))
	float MaxScoreDistance = 2000.f;

	/** 모든 후보가 갖는 기본 점수. 배수 비교(SwitchScoreRatio)가 성립하려면 0이 되면 안됨. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|AI|Targeting", meta = (ClampMin = "0.01"))
	float BaseTargetScore = 1.f;

	/** 거리 가중치. 가까운 후보에 최대 이 값만큼 가산. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|AI|Targeting", meta = (ClampMin = "0.0"))
	float DistanceWeight = 1.f;

	/** 시야로 인지 중인 후보에 주는 가산점 (소리만 들리는 후보보다 우선). */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|AI|Targeting", meta = (ClampMin = "0.0"))
	float SightBonus = 0.5f;

	/** 최근에 자신을 때린 후보에 주는 가산점. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|AI|Targeting", meta = (ClampMin = "0.0"))
	float RecentDamageBonus = 1.f;

	/**
	 * 피격을 기억하는 시간(초). 피해 감각의 자극 유지 시간으로 적용됨 (빙의 시).
	 * 지나면 가산점이 사라지고, 다른 감각으로 인지 중이 아니면 인지에서도 빠짐.
	 * 0 은 엔진에서 "만료 없음"이라 막아 둠.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|AI|Targeting", meta = (ClampMin = "0.1"))
	float RecentDamageMemoryTime = 5.f;

	/**
	 * 타겟 교체 문턱 (히스테리시스). 새 후보가 현재 타겟 점수의 이 배수를 넘어야 교체.
	 * 1.0이면 문턱이 없어져 점수가 엎치락뒤치락할 때마다 타겟이 튐.
	 * 베이스 ReevaluateTarget 에서만 씀 (Outlaw 는 집중 시간 규칙이라 쓰지 않음).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|AI|Targeting", meta = (ClampMin = "1.0"))
	float SwitchScoreRatio = 1.25f;

	/**
	 * 이 태그에 해당하는 어빌리티가 활성 중이면 타겟 교체를 금지 (부모 태그 매칭).
	 * 공격 몽타주가 도는 도중 결정이 바뀌어, 끝나자마자 반대쪽으로 튀는 것을 막음.
	 * 비워두면 잠금 없음.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|AI|Targeting", meta = (Categories = "Ability"))
	FGameplayTagContainer TargetLockAbilityTags;

private:
	/** [서버] 피격 반응 이벤트(Event.Combat.HitReact) 구독/해제. */
	void BindHitReactEvent(UAbilitySystemComponent& InASC);
	void UnbindHitReactEvent(UAbilitySystemComponent& InASC);

	/** 피격 반응 이벤트 콜백. 가해자를 폰으로 정규화해 피해 감각에 보고 (가해자별로 인지·기억됨). */
	void HandleHitReactEvent(const FGameplayEventData* Payload);

	/** 피격 이벤트 구독 핸들. */
	FDelegateHandle HitReactEventHandle;

	/** 현재 타겟을 블랙보드에 마지막으로 쓴 시각(초). 음수면 기록 없음. */
	float TargetAssignedTime = -1.f;
#pragma endregion

#pragma region Stagger
	/** 피격 경직 상태(Status.Combat.Staggered)를 블랙보드로 미러링. */
public:
	/** 블랙보드 경직 키 이름 (에디터 BB 키 이름과 반드시 일치). */
	static const FName StaggeredKey;

private:
	/** [서버] 경직 상태 태그 구독/해제. */
	void BindStaggerStateEvent(UAbilitySystemComponent& InASC);
	void UnbindStaggerStateEvent(UAbilitySystemComponent& InASC);

	/** 경직 태그 변화 콜백. 블랙보드 bool 키에 그대로 반영. */
	void OnStaggerTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	/** 경직 태그 구독 핸들. */
	FDelegateHandle StaggerTagHandle;
#pragma endregion

#pragma region CrowdAvoidance
	/**
	 * 군중 회피 (Detour Crowd). 여러 AI 가 같은 목표로 이동할 때 서로 밀거나 겹치지 않게 함.
	 * 경로 추종 컴포넌트를 UCrowdFollowingComponent 로 교체해 사용하며, 교체는 생성자가 담당.
	 */
protected:
	/** [서버] 군중 회피 파라미터를 경로 추종 컴포넌트에 적용. 빙의 시 1회. */
	void ApplyCrowdAvoidanceSettings();

	/** 서로 간격을 벌리는 분리(Separation) 사용 여부. 끄면 부딪히지 않게 피하기만 하고 간격은 유지하지 않음. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|AI|Crowd")
	bool bUseCrowdSeparation = true;

	/** 분리 강도. 클수록 서로 멀찍이 떨어지려 하며, 과하면 경로를 크게 벗어나 목표에 못 붙음. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|AI|Crowd", meta = (EditCondition = "bUseCrowdSeparation", ClampMin = "0.0"))
	float CrowdSeparationWeight = 2.f;

	/** 회피 계산 시 이웃을 탐색하는 반경(cm). 캡슐 반지름의 10배 안팎이 기준값. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|AI|Crowd", meta = (ClampMin = "0.0"))
	float CrowdCollisionQueryRange = 400.f;
#pragma endregion

private:
	/** 준비 완료 델리게이트 핸들 (아직 준비 전에 빙의한 경우 대기용. OnUnPossess에서 해제) */
	FDelegateHandle SystemReadyHandle;
};
