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
struct FGameplayEventData;

/**
 * AI 컨트롤러 공통 베이스 (Outlaw·Rogue 등).
 * AI 두뇌(BT/StateTree 등)의 시작 타이밍을 캐릭터의 준비 완료(SystemReady) 신호에 맞춰 게이트.
 * 시야·청각 퍼셉션으로 적(진영이 다른 대상)을 감지하고, 후보들을 점수화해 블랙보드 타겟을 선정·갱신.
 * 경로 추종은 군중 회피(Detour Crowd)를 사용해 여러 AI 가 서로 겹치지 않게 이동.
 * 실제 실행할 두뇌는 자식이 StartAILogic()을 오버라이드해 지정. 직접 스폰하지 않는 추상 클래스.
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
	 * 베이스는 위협 판정용 피격 이벤트 구독만 수행하므로, 자식은 반드시 Super를 호출한 뒤 자기 두뇌(BT/StateTree 등)를 구동할 것.
	 */
	virtual void StartAILogic();

	/** 빙의한 CB AI 캐릭터 (타입 캐싱). OnPossess에서 세팅, OnUnPossess에서 해제. */
	UPROPERTY(Transient)
	TObjectPtr<ACBAICharacter> CachedAICharacter = nullptr;

	FORCEINLINE ACBAICharacter* GetCachedAICharacter() const { return CachedAICharacter.Get(); }

	/** [서버] 로드아웃에서 주입된 비헤이비어 트리 (하드 참조로 생존 보장). 자식이 StartAILogic에서 구동. */
	UPROPERTY(Transient)
	TObjectPtr<UBehaviorTree> BehaviorTree = nullptr;

#pragma region Perception
	/** 시야·청각 퍼셉션으로 적을 감지 → 타겟 재선정 요청. */
protected:
	/** 감지 결과가 갱신될 때 호출 (감지/상실 상태 변화 시). */
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** 이 액터를 타겟으로 삼을지 판정. (적 판정) */
	virtual bool IsValidTarget(AActor* InActor) const;

	/** 블랙보드 TargetActor 키에 값을 쓰거나(감지) 지움(상실/무효). 블랙보드 미준비 시 무시. */
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
	 * 지금 타겟을 교체해도 되는지 판정 (전환 금지 구간).
	 * @return 교체 가능하면 true
	 */
	virtual bool CanSwitchTarget() const;

	/**
	 * 현재 타겟을 계속 들고 있어도 되는지 판정 (적 판정 + 생존 + 인지 유지).
	 * @param InActor 검사할 현재 타겟
	 * @return 유지 가능하면 true
	 */
	bool IsTargetStillValid(AActor* InActor) const;

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

	/** 피격을 위협으로 기억하는 시간(초). 지나면 가산점 소멸. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|AI|Targeting", meta = (ClampMin = "0.0"))
	float RecentDamageMemoryTime = 5.f;

	/**
	 * 타겟 교체 문턱 (히스테리시스). 새 후보가 현재 타겟 점수의 이 배수를 넘어야 교체.
	 * 1.0이면 문턱이 없어져 점수가 엎치락뒤치락할 때마다 타겟이 튐.
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
	void BindHitReactEvent();
	void UnbindHitReactEvent();

	/** 피격 반응 이벤트 콜백. 가해자를 폰으로 정규화해 최근 피격 정보로 기록. */
	void HandleHitReactEvent(const FGameplayEventData* Payload);

	/**
	 * 가해자 액터를 가해자 소유 폰으로 변환.
	 * 이벤트가 싣고 오는 Instigator는 가해자의 ASC 소유 액터라, 플레이어는 폰이 아니라 PlayerState가 들어옴.
	 * @param InActor 이벤트가 실어 온 가해자
	 * @return 가해자의 액터. 변환할 수 없으면 입력 그대로.
	 */
	static const AActor* ResolveThreatPawn(const AActor* InActor);

	/** 피격 이벤트를 구독한 ASC (해제용). */
	TWeakObjectPtr<UAbilitySystemComponent> CachedThreatASC;

	/** 피격 이벤트 구독 핸들. */
	FDelegateHandle HitReactEventHandle;

	/** 마지막으로 자신을 때린 대상 (폰 기준). */
	TWeakObjectPtr<const AActor> LastDamageInstigator;

	/** 마지막 피격 시각(초). 음수면 피격 기록 없음. */
	float LastDamageTime = -1.f;
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
