#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GenericTeamAgentInterface.h"
#include "Interfaces/CBCombatInterface.h"
#include "Interfaces/CBUIInterface.h"
#include "Types/CBEnumTypes.h"
#include "CBBaseCharacter.generated.h"

class UCBAbilitySystemComponent;
class UCBAttributeSet;
class UCBCharacterLoadout;
class UCBCharacterMovementData;
class UCBCombatComponent;
struct FOnAttributeChangeData;
class UCBLocomotionProcessor;
class UCBActionComponent;
class UCBNoiseEmitterComponent;
class UCBUIComponent;
class UMotionWarpingComponent;

DECLARE_MULTICAST_DELEGATE(FOnCharacterSystemReady);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCBTeamChanged, ECBTeam /* NewTeam */);
DECLARE_MULTICAST_DELEGATE(FOnCBCharacterDied);


/** 캐릭터 초기화 상태 */
enum class ECBSystemState : uint8
{
	Uninitialized,	// 초기화 시작 전
	Initializing,	// 비동기 초기화 진행 중
	Ready			// 모든 초기화 완료
};

UCLASS()
class CHAINBURST_API ACBBaseCharacter : public ACharacter, public IAbilitySystemInterface, public ICBCombatInterface, public ICBUIInterface, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	ACBBaseCharacter();

	virtual void Tick(float DeltaTime) override;

	//~ Begin AActor Interface.
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** 사망 태그 구독과 디스폰 타이머를 정리. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface.

#pragma region Ability System
	/**
	 * 어빌리티 시스템 — ASC·AttributeSet 캐싱과 접근.
	 * 실제 소유는 [플레이어]는 PlayerState, [AI]는 Character이며, 여기서는 포인터를 캐싱한다.
	 */
protected:
	/**
	 * CBASC는 자식에서 캐싱
	 * [플레이어]는 PlayerState에서 [AI]는 Character에서 ASC와 AttributeSet을 가져오는 방식으로 구현
	 */
	UPROPERTY()
	TObjectPtr<UCBAbilitySystemComponent> CBASC = nullptr;

	/**
	 * CBAttributeSet는 자식에서 캐싱
	 * [플레이어]는 PlayerState에서 [AI]는 Character에서 ASC와 AttributeSet을 가져오는 방식으로 구현
	 */
	UPROPERTY()
	TObjectPtr<UCBAttributeSet> CBAttributeSet = nullptr;

	/** 어트리뷰트 초기화 함수 */
	virtual void InitializeAttributes();

public:
	//~ Begin IAbilitySystemInterface Interface.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface Interface.

	FORCEINLINE UCBAbilitySystemComponent* GetCBAbilitySystemComponent() const { return CBASC.Get(); }
	FORCEINLINE UCBAttributeSet* GetCBAttributeSet() const { return CBAttributeSet.Get(); }

	UFUNCTION(Server, Reliable)
	void Server_SendGameplayEvent(AActor* Actor, FGameplayTag EventTag, FGameplayEventData Payload);
#pragma endregion

#pragma region Components
	/** 캐릭터가 소유하는 공통 컴포넌트와 액션(몽타주) 요청. */
protected:
	/** 이동 데이터 계산 컴포넌트 (무브먼트) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Movement")
	TObjectPtr<UCBLocomotionProcessor> CBLocomotionProcessor = nullptr;

	/** 액션(몽타주) 관련 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Animation")
	TObjectPtr<UCBActionComponent> CBActionComponent = nullptr;

	/** 캐릭터 부착형 UI 관리 컴포넌트 (로컬 플레이어=HUD, 그 외=머리 위 체력바) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|UI")
	TObjectPtr<UCBUIComponent> CBUIComponent = nullptr;

	/** [서버 전용 동작] 이동 시 AI 청각용 소음을 발생시키는 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Perception")
	TObjectPtr<UCBNoiseEmitterComponent> CBNoiseEmitterComponent = nullptr;

	/** 모션 워핑 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Movement")
	TObjectPtr<UMotionWarpingComponent> CBMotionWarpingComponent = nullptr;

public:
	/**
	 * 외부(어빌리티, 게임플레이 큐 등)에서 몽타주 재생을 요청하는 함수
	 * @param InActionTag  재생할 몽타주 식별 태그
	 * @param ComboIndex   재생할 몽타주 인덱스 (콤보 단계 또는 랜덤 변형 인덱스. 단일 액션은 0)
	 * @return             재생 성공 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Action")
	bool RequestPlayMontage(const FGameplayTag InActionTag, int32 ComboIndex = 0);

	/**
	 * 외부(게임플레이 큐)에서 몽타주 정지를 요청하는 함수
	 * @param BlendOutTime 블렌드 아웃 시간. (기본 0.f)
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Action")
	void RequestStopMontage(float BlendOutTime = 0.f);

	//~ Begin ICBCombatInterface Interface.
	virtual UCBCombatComponent* GetCBCombatComponent() const override;
	//~ End ICBCombatInterface Interface.

	//~ Begin ICBUIInterface Interface.
	virtual UCBUIComponent* GetCBUIComponent() const override;
	//~ End ICBUIInterface Interface.

	FORCEINLINE UCBActionComponent* GetCBActionComponent() const { return CBActionComponent.Get(); }
	FORCEINLINE UMotionWarpingComponent* GetMotionWarpingComponent() const { return CBMotionWarpingComponent.Get(); }
#pragma endregion

#pragma region Movement
	/** 이동 데이터 캐시와 이동 속도 반영. */
protected:
	/** 이동 데이터. 로드아웃(UCBCharacterLoadout)에서 주입되는 런타임 캐시 */
	UPROPERTY()
	TObjectPtr<UCBCharacterMovementData> MovementDataAsset = nullptr;

public:
	/** 캐릭터 이동 속도 변경 시 호출하는 함수 */
	void OnMovementSpeedChanged(float NewSpeed);

	FORCEINLINE UCBCharacterMovementData* GetMovementDataAsset() const { return MovementDataAsset.Get(); }

	/** 로드아웃에서 이동 데이터를 주입하는 세터 */
	FORCEINLINE void SetMovementDataAsset(UCBCharacterMovementData* InMovementData) { MovementDataAsset = InMovementData; }
#pragma endregion

#pragma region Team
	/** 진영(팀) — AI 의 적/아군 판정 기준. */
public:
	/** 팀이 바뀌었을 때 방송 (UI 색 구분 등 로컬 표현용). */
	FOnCBTeamChanged OnTeamChangedDelegate;

	//~ Begin IGenericTeamAgentInterface Interface.
	virtual FGenericTeamId GetGenericTeamId() const override { return CBTeamToGenericId(Team); }
	virtual void SetGenericTeamId(const FGenericTeamId& InTeamId) override;
	//~ End IGenericTeamAgentInterface Interface.

	FORCEINLINE ECBTeam GetTeam() const { return Team; }

	/** [서버] 진영을 변경한다. 복제되어 전 클라이언트에 반영. */
	void Auth_SetTeam(ECBTeam InTeam);

protected:
	/** 이 캐릭터의 진영. 캐릭터 BP 기본값 설정 가능. (Chaser=Chaser, Outlaw·Rogue=Outlaw). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Team, Category = "ChainBurst|Team")
	ECBTeam Team = ECBTeam::Neutral;

	UFUNCTION()
	void OnRep_Team();
#pragma endregion

#pragma region System Ready
	/** 캐릭터 시스템 준비(Ready) 라이프사이클 — 상태·델리게이트·초기화 진입/완료. */
public:
	/** 캐릭터 시스템 준비 완료 델리게이트 (로드아웃 비동기 로드 끝나면 방송) */
	FOnCharacterSystemReady OnCharacterSystemReadyDelegate;

	/** 캐릭터 시스템이 준비 완료(Ready) 상태인지 여부 */
	FORCEINLINE bool IsCharacterSystemReady() const { return SystemState == ECBSystemState::Ready; }

protected:
	/** 캐릭터 초기화 상태 */
	ECBSystemState SystemState = ECBSystemState::Uninitialized;

	/**
	 * 초기화를 시작한다. 상태를 Initializing으로 전환한다 (재진입 방지 가드).
	 * 초기화는 캐릭터당 공용 데이터 로드(async) 1개로 수렴하며, 그 완료 시점에 HandleCharacterSystemReady를 호출.
	 * @return 이미 초기화가 시작/완료되었으면 false (재진입 방지), 새로 시작하면 true
	 */
	bool StartSystemInitialization();

	/** 통합 초기화 완료 함수 (공용 데이터 적용 완료 시 1회. 델리게이트 방송 + 어트리뷰트 초기화) */
	virtual void HandleCharacterSystemReady();
#pragma endregion

#pragma region Death
	/**
	 * 사망 라이프사이클. (Status.Dead 태그가 진입점)
	 * 태그는 전 클라이언트에 복제되므로 서버·오너·시뮬 프록시가 같은 콜백에서 각자 자기 몫만 수행:
	 * 서버는 권위 정리(이동·충돌·두뇌 정지·디스폰), 전 인스턴스는 표현 정리(머리 위 바 숨김 등).
	 */
public:
	/** 사망 상태가 되었을 때 방송 (표현 정리용). 전 인스턴스에서 처리 */
	FOnCBCharacterDied OnCharacterDiedDelegate;

	/** 사망 상태인지 여부 (Status.Dead 보유 여부의 로컬 캐시) */
	FORCEINLINE bool IsDead() const { return bIsDead; }

protected:
	//~ Begin ACharacter Interface.
	/**
	 * 착지 콜백. 공중에서 죽어 낙하 중이던 시체가 바닥에 닿는 시점에 이동을 정지.
	 * 사망 시점에 바로 멈추면 MOVE_None 이라 중력이 안 먹어 공중에 떠 버리기 때문.
	 */
	virtual void Landed(const FHitResult& Hit) override;
	//~ End ACharacter Interface.

	/** [서버] 사망 시 권위 정리 (이동 정지·충돌 해제·자식 훅·디스폰 예약) */
	virtual void Auth_HandleDeath();

	/** [서버] 이동을 완전히 정지 (사망 확정 시점 또는 공중 사망 후 착지 시점) */
	void Auth_StopMovementForDeath();

	/** [서버] 자식이 확장하는 사망 정리 훅 (AI = 두뇌 정지 등). 기본 구현은 비어 있음 */
	virtual void Auth_OnDeath() {}

	/** [서버] 디스폰 타이머 만료 시 액터를 파괴 */
	virtual void Auth_Despawn();

	/** 사망 로컬 정리 (전 인스턴스 각자 실행. 기본 구현은 델리게이트 방송) */
	virtual void Local_ApplyDeathVisuals();

	/** 사망 후 액터를 파괴하기까지의 지연(초). 0 이하면 자동 파괴하지 않음 (리스폰이 있는 플레이어 등) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Death")
	float DespawnDelay = 5.f;

	/** 사망 상태 캐시 (Status.Dead 보유 여부) */
	bool bIsDead = false;

private:
	/** Status.Dead 태그를 구독하고 현재 상태를 1회 반영하는 함수 (준비 완료 시점에 호출) */
	void BindDeathStateEvent();

	/** Status.Dead 태그 이벤트 콜백. 서버·오너·시뮬 프록시 전부에서 발화한다 */
	void OnDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	/** 사망 태그 구독 해제용 핸들 (Chaser는 ASC가 PlayerState 소유라 폰보다 오래 살아남아 해제가 필수) */
	FDelegateHandle DeadTagChangedHandle;

	/** 디스폰 타이머 핸들 */
	FTimerHandle DespawnTimerHandle;
#pragma endregion
};
