#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "CBAIController.generated.h"

class ACBAICharacter;
class UBehaviorTree;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;

/**
 * AI 컨트롤러 공통 베이스 (Outlaw·Rogue 등).
 * AI 두뇌(BT/StateTree 등)의 시작 타이밍을 캐릭터의 준비 완료(SystemReady) 신호에 맞춰 게이트.
 * 시야·청각 퍼셉션으로 적(진영이 다른 대상)을 감지해 블랙보드 타겟을 갱신.
 * 실제 실행할 두뇌는 자식이 StartAILogic()을 오버라이드해 지정. 직접 스폰하지 않는 추상 클래스.
 */
UCLASS(Abstract)
class CHAINBURST_API ACBAIController : public AAIController
{
	GENERATED_BODY()

public:
	ACBAIController();

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
	 * 베이스는 아무것도 하지 않으며, 자식이 실행할 두뇌(BT/StateTree 등)를 여기서 구동.
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
	/** 시야 퍼셉션으로 적을 감지 → 블랙보드 TargetActor 갱신. */
protected:
	/** 감지 결과가 갱신될 때 호출 (감지/상실 상태 변화 시). */
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** 이 액터를 타겟으로 삼을지 판정. (적 판정) */
	virtual bool IsValidTarget(AActor* InActor) const;

	/**
	 * BT 시작 시점에 이미 시야에 있던(정지한) 타겟을 놓치지 않도록 현재 감지 목록에서 한 번 시드.
	 * OnTargetPerceptionUpdated가 상태 변화 시에만 발화하는 특성을 보완.
	 */
	void SeedTargetFromCurrentPerception();

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

	/** 블랙보드 타겟 키 이름 (에디터 BB 키 이름과 반드시 일치). */
	static const FName TargetActorKey;
#pragma endregion

private:
	/** 준비 완료 델리게이트 핸들 (아직 준비 전에 빙의한 경우 대기용. OnUnPossess에서 해제) */
	FDelegateHandle SystemReadyHandle;
};
