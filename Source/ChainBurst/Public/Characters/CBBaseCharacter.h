#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Interfaces/CBCombatInterface.h"
#include "CBBaseCharacter.generated.h"

class UCBAbilitySystemComponent;
class UCBAttributeSet;
class UCBCharacterLoadout;
class UCBCharacterMovementData;
class UCBCombatComponent;
struct FOnAttributeChangeData;
class UCBLocomotionProcessor;
class UCBCharacterTrajectoryComponent;
class UCBActionComponent;

DECLARE_MULTICAST_DELEGATE(FOnCharacterSystemReady)

UCLASS()
class CHAINBURST_API ACBBaseCharacter : public ACharacter, public IAbilitySystemInterface, public ICBCombatInterface
{
	GENERATED_BODY()
	
public:
	ACBBaseCharacter();
	
	virtual void Tick(float DeltaTime) override;
	
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
	
#pragma region Components
	/** 궤적 컴포넌트 (무브먼트) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Movement")
	TObjectPtr<UCBCharacterTrajectoryComponent> CBTrajectoryComponent = nullptr;

	/** 이동 데이터 계산 컴포넌트 (무브먼트) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Movement")
	TObjectPtr<UCBLocomotionProcessor> CBLocomotionProcessor = nullptr;

	/** 액션(몽타주) 관련 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Animation")
	TObjectPtr<UCBActionComponent> CBActionComponent = nullptr;
#pragma endregion

	/** 이동 데이터. 로드아웃(UCBCharacterLoadout)에서 주입되는 런타임 캐시 */
	UPROPERTY()
	TObjectPtr<UCBCharacterMovementData> MovementDataAsset = nullptr;
	
public:
	/** 캐릭터 시스템 준비 완료 델리게이트 */
	FOnCharacterSystemReady OnCharacterSystemReadyDelegate;
	
	/** 캐릭터 시스템 준비 완료 여부 (중복 방지 플래그)*/
	bool bIsCharacterSystemReady = false;

	/** 캐릭터 이동 속도 변경 시 호출하는 함수 */
	void OnMovementSpeedChanged(float NewSpeed);
	
	/**
	 * 외부(어빌리티, 게임플레이 큐 등)에서 몽타주 재생을 요청하는 함수
	 * @param InActionTag  재생할 몽타주 식별 태그
	 * @param bIsCombo     콤보 몽타주 여부
	 * @return             재생 성공 여부
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Action")
	bool RequestPlayMontage(const FGameplayTag InActionTag, bool bIsCombo = false);

	UFUNCTION(Server, Reliable)
	void Server_SendGameplayEvent(AActor* Actor, FGameplayTag EventTag, FGameplayEventData Payload);

	//~ Begin IAbilitySystemInterface Interface.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface Interface.
	
	//~ Begin ICBCombatInterface Interface.
	virtual UCBCombatComponent* GetCBCombatComponent() const override;
	//~ End ICBCombatInterface Interface.
	
	FORCEINLINE UCBAbilitySystemComponent* GetCBAbilitySystemComponent() const { return CBASC.Get(); }
	FORCEINLINE UCBAttributeSet* GetCBAttributeSet() const { return CBAttributeSet.Get(); }
	FORCEINLINE UCBCharacterMovementData* GetMovementDataAsset() const { return MovementDataAsset.Get(); }
	FORCEINLINE UCBCharacterTrajectoryComponent* GetCBTrajectoryComponent() const { return CBTrajectoryComponent.Get(); }
	FORCEINLINE UCBActionComponent* GetCBActionComponent() const { return CBActionComponent.Get(); }
	
	/** 로드아웃에서 이동 데이터를 주입하는 세터 */
	FORCEINLINE void SetMovementDataAsset(UCBCharacterMovementData* InMovementData) { MovementDataAsset = InMovementData; }
	
protected:
	/** 통합 초기화 함수 (캐릭터 시스템이 완료되면 델리게이트를 방송) */
	virtual void HandleCharacterSystemReady();

	/** 어트리뷰트 초기화 함수 */
	virtual void InitializeAttributes();
};
