#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "GameplayTagContainer.h"
#include "CBActionComponent.generated.h"

class UCBActionMontageData;
class UCBCharacterAnimInstance;

/**
 * 액션(몽타주) 관련 처리 컴포넌트
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
	/** 액션 몽타주 데이터. 로드아웃(UCBCharacterLoadout)에서 주입되는 런타임 캐시 */
	UPROPERTY()
	TObjectPtr<UCBActionMontageData> MontageData = nullptr;

	TWeakObjectPtr<UCBCharacterAnimInstance> CachedAnimInstance;

	/** 현재 재생 중인 액션의 지속 시간 */
	UPROPERTY(Replicated)
	float CurrentActionDuration = 0.f;
	
public:
	/**
	 * 몽타주 재생 요청
	 * 콤보/랜덤 여부와 무관하게 인덱스로 몽타주를 선택해 재생한다. (인덱스 의미는 호출자가 결정)
	 * @param InActionTag 재생할 몽타주 식별 태그
	 * @param InIndex     재생할 몽타주 인덱스 (단일 액션은 0)
	 * @return 재생 성공 여부
	 */
	bool RequestPlayMontage(const FGameplayTag& InActionTag, int32 InIndex = 0);

	/** 현재 재생 중인 몽타주 강제 중단 */
	void StopMontage(float BlendOutTime = 0.25f);

	/** 현재 액션(몽타주) 재생 시간 반환 (기본 값 5초) */
	float GetCurrentActionDuration() const { return CurrentActionDuration > 0.f ? CurrentActionDuration : 5.f; }

	/**
	 * 액션 태그에 등록된 몽타주 개수 반환 (콤보 단계 수).
	 * 몽타주 데이터 에셋 접근을 컴포넌트 뒤로 캡슐화한다 — 외부(어빌리티 등)는 데이터 에셋 대신 이 함수를 사용.
	 * @return 몽타주 개수. 데이터 에셋이 없거나 태그가 없으면 0.
	 */
	int32 GetMontageCount(const FGameplayTag& InActionTag) const;

	/** 로드아웃에서 액션 몽타주 데이터를 주입하는 세터 */
	FORCEINLINE void SetMontageData(UCBActionMontageData* InMontageData) { MontageData = InMontageData; }
	
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
};
