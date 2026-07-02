#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "GameplayTagContainer.h"
#include "CBActionComponent.generated.h"

class UCBActionMontageData;
class UCBCharacterAnimInstance;

/**
 * 액션(몽타주) 관련 처리 컴포넌트
 * 단일 몽타주, 콤보 몽타주 재생 및 콤보 단계 관리
 * 몽타주 데이터는 UCBActionMontageData 데이터 에셋에서 관리
 * 캐릭터의 애님 인스턴스에 몽타주 재생 요청
 */
UCLASS()
class CHAINBURST_API UCBActionComponent : public UCBExtensionComponent
{
	GENERATED_BODY()
	
public:
	UCBActionComponent();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|MontageData")
	TObjectPtr<UCBActionMontageData> MontageData = nullptr;

	TWeakObjectPtr<UCBCharacterAnimInstance> CachedAnimInstance;

	/** 현재 콤보 단계 (0부터 시작) */
	UPROPERTY(Replicated)
	int32 CurrentComboIndex = 0;

	/** 현재 재생 중인 액션 태그 (다른 액션으로 전환 시 콤보 인덱스 초기화 판단용) */
	UPROPERTY(Replicated)
	FGameplayTag CurrentActionTag = FGameplayTag::EmptyTag;

	/** 현재 재생 중인 액션의 지속 시간 */
	UPROPERTY(Replicated)
	float CurrentActionDuration = 0.f;
	
public:
	/**
	 * 단일 몽타주 재생 요청
	 * @param InActionTag 재생할 몽타주 식별 태그
	 * @return 재생 성공 여부
	 */
	bool RequestPlaySingleMontage(const FGameplayTag& InActionTag);

	/**
	 * 콤보 몽타주 재생 요청
	 * 내부적으로 현재 콤보 인덱스를 기반으로 몽타주 선택
	 * @param InActionTag 재생할 콤보 몽타주 식별 태그
	 * @return 재생 성공 여부
	 */
	bool RequestPlayComboMontage(const FGameplayTag& InActionTag);

	/** 현재 재생 중인 몽타주 강제 중단 */
	void StopMontage(float BlendOutTime = 0.25f, bool IsResetCombo = false);
	
	/** 현재 콤보 인덱스 반환 */
	int32 GetCurrentComboIndex() const { return CurrentComboIndex; }

	/** 현재 액션(몽타주) 재생 시간 반환 (기본 값 5초) */
	float GetCurrentActionDuration() const { return CurrentActionDuration > 0.f ? CurrentActionDuration : 5.f; }

	/** 액션 몽타주 데이터 에셋 반환 */
	FORCEINLINE UCBActionMontageData* GetActionMontageDataAsset() const { return MontageData.Get(); }
	
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	/**
	 * 태그 컨테이너에서 액션 태그 카테고리에 속하는 태그들 중 가장 우선순위가 높은 태그를 선택하여 반환하는 함수.
	 * @param InTags 캐릭터가 현재 보유한 태그 컨테이너. 이 중에서 액션 태그 카테고리에 속하는 태그들만 고려하여 가장 우선순위가 높은 태그를 선택.
	 * @return 가장 우선순위가 높은 액션 태그.	
	 */
	FGameplayTag SelectBestActionTag(const FGameplayTagContainer& InTags);

	/**
	 * 태그의 우선순위를 반환하는 함수.
	 * @param InTag 우선순위를 확인하고 싶은 태그.
	 * @return 태그의 우선순위 값. 값이 높을수록 우선순위가 높음. 액션 태그 카테고리에 속하지 않는 태그는 기본적으로 0을 반환.
	 */
	int32 GetActionPriority(FGameplayTag InTag);

	/**
	 * 애님 인스턴스에 몽타주 재생 전달
	 * @param bAffectedByAttackSpeed true면 AttackSpeed 어트리뷰트를 PlayRate에 반영, false면 기본 속도(1.0)로 재생
	 */
	bool PlayMontage(UAnimMontage* InMontage, bool bAffectedByAttackSpeed);

	/** 애님 인스턴스 지연 캐싱 */
	bool GetCachedAnimInstance(TWeakObjectPtr<UCBCharacterAnimInstance>& OutAnimInstance);

	/** 타이머 시작 함수 (몽타주 재생 시 호출) */
	void StartComboResetTimer(float MontageDuration);

	/** 타이머 취소 함수 (콤보가 이어지거나 강제 취소될 때 호출) */
	void CancelComboResetTimer();

	/** 콤보 초기화 함수 */
	void ResetComboIndex();

private:
	/** 콤보 초기화 타이머에서 호출될 콜백 함수 */
	void OnComboTimeout();

	/** 콤보 초기화 타이머 핸들 */
	FTimerHandle ComboResetTimerHandle;
};
