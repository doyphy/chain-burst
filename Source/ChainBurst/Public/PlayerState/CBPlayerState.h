#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "Types/CBEnumTypes.h"
#include "CBPlayerState.generated.h"

class ACBChaserCharacter;
class UCBAbilitySystemComponent;
class UCBAttributeSet;

/**
 * 플레이어의 상태를 관리하는 클래스. 주로 어빌리티 시스템과 관련된 데이터를 저장하고 관리하는 역할을 함.
 * 플레이어가 소유한 [ASC]와 [AttributeSet]을 보유하며, IAbilitySystemInterface를 구현하여 다른 클래스에서 ASC에 접근할 수 있도록 함.
 */
UCLASS()
class CHAINBURST_API ACBPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACBPlayerState();
	
	//~ Begin IAbilitySystemInterface Interface.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface Interface.

	//~ Begin APlayerState Interface.
	/** seamless travel 시 새로 만들어지는 PlayerState 로 값을 옮김 (오버라이드하지 않으면 커스텀 값이 사라짐). */
	virtual void CopyProperties(APlayerState* NewPlayerState) override;
	//~ End APlayerState Interface.

protected:
	//~ Begin AActor Interface.
	/** 폰 연결 델리게이트를 구독함 (복제가 폰보다 먼저 도착하는 순서를 처리하기 위함). */
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~ End AActor Interface.

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|AbilitySystem")
	TObjectPtr<UCBAbilitySystemComponent> CBAbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|AbilitySystem")
	TObjectPtr<UCBAttributeSet> CBAttributeSet;

public:
	FORCEINLINE UCBAbilitySystemComponent* GetCBAbilitySystemComponent() const { return CBAbilitySystemComponent.Get(); }
	FORCEINLINE UCBAttributeSet* GetCBAttributeSet() const { return CBAttributeSet.Get(); }

#pragma region Cosmetic
	/** 로비에서 고른 의상 조합. 확정 상태는 서버가 정하고, 복제된 값을 각 인스턴스가 로컬로 외형에 반영함. */
public:
	/**
	 * [서버] 검증을 통과한 파츠를 적용하는 함수.
	 * 서버에서는 OnRep 이 불리지 않으므로 직접 호출해 서버 호스트 화면에도 적용.
	 * @param InSlot 교체할 부위 슬롯
	 * @param InPartId 입힐 파츠 태그 (빈 태그 = 선택 해제)
	 */
	void Auth_SetCosmeticPart(ECBCosmeticSlot InSlot, const FGameplayTag& InPartId);

	/** [Getter] 슬롯별 착용 파츠 태그 (인덱스 = ECBCosmeticSlot) */
	FORCEINLINE const TArray<FGameplayTag>& GetCosmetics() const { return Cosmetics; }

protected:
	/** Cosmetics 배열이 바뀌었을 때 호출되는 콜백. 각 인스턴스가 로컬로 외형에 반영함. */
	UFUNCTION()
	void OnRep_Cosmetics();

	/**
	 * 폰이 붙고 캐릭터 준비가 끝난 뒤에 의상을 적용하는 진입점 (전 인스턴스 각자 로컬 실행).
	 * 폰이 없으면 아무것도 하지 않고, 준비 전이면 준비 완료 신호를 구독해 기다림.
	 * 로드아웃의 기본 의상이 준비 완료 직전에 적용되므로, 그보다 늦게 덮어써야 로비에서 고른 조합이 적용.
	 */
	void ApplyCosmeticsWhenReady();

	/** 캐릭터 시스템 준비 완료 콜백. 구독을 해제하고 의상을 적용함. */
	void HandleCharacterSystemReady();

	/** 현재 조합을 소유 폰의 모듈러 메시 컴포넌트에 적용하는 함수 (전 인스턴스 각자 로컬 실행). */
	void ApplyCosmeticsToPawn();

	/** 폰이 연결될 때 호출되는 콜백. 복제가 폰보다 먼저 도착한 경우를 위해 다시 적용함 */
	UFUNCTION()
	void HandlePawnSet(APlayerState* InPlayerState, APawn* InNewPawn, APawn* InOldPawn);

	/**
	 * 슬롯별 착용 파츠 태그 (인덱스 = ECBCosmeticSlot).
	 * 로비 선택 결과이며 CopyProperties 를 통해 게임플레이 레벨까지 이관됨.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_Cosmetics)
	TArray<FGameplayTag> Cosmetics;

	/** 준비 완료를 기다리는 중인 캐릭터. 구독 해제 대상 추적용. */
	TWeakObjectPtr<ACBChaserCharacter> PendingReadyCharacter;

	/** PendingReadyCharacter 의 준비 완료 델리게이트 구독 핸들. 유효하면 이미 대기 중이라는 뜻. */
	FDelegateHandle CharacterSystemReadyHandle;
#pragma endregion

#pragma region Lobby
	/** 로비 준비 상태 처리 */
public:
	/**
	 * [서버] 준비 상태를 설정하는 함수. (서버에서만 실행)
	 * @param bInReady 준비 여부
	 */
	void Auth_SetReady(bool bInReady);

	/** [Getter] 준비 상태 (위젯이 버튼 표시 분기에 사용) */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Lobby")
	FORCEINLINE bool IsReady() const { return bIsReady; }

protected:
	/** 준비 상태가 바뀌었을 때 호출되는 콜백. */
	UFUNCTION()
	void OnRep_IsReady();

	/** 로비에서의 준비 여부. */
	UPROPERTY(ReplicatedUsing = OnRep_IsReady)
	bool bIsReady = false;
#pragma endregion
};
