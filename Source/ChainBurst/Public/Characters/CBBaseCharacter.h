#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Interfaces/CBCombatInterface.h"
#include "Interfaces/CBUIInterface.h"
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

DECLARE_MULTICAST_DELEGATE(FOnCharacterSystemReady);

/** 캐릭터 초기화 상태 */
enum class ECBSystemState : uint8
{
	Uninitialized,	// 초기화 시작 전
	Initializing,	// 비동기 초기화 진행 중
	Ready			// 모든 초기화 완료
};

UCLASS()
class CHAINBURST_API ACBBaseCharacter : public ACharacter, public IAbilitySystemInterface, public ICBCombatInterface, public ICBUIInterface
{
	GENERATED_BODY()

public:
	ACBBaseCharacter();

	virtual void Tick(float DeltaTime) override;

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
};
