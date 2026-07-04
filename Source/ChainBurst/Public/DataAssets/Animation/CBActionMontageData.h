#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CBActionMontageData.generated.h"


/**
 * 액션(몽타주) 데이터 엔트리
 * 액션 태그 하나에 몽타주 배열과 공속 영향 여부를 묶어 관리.
 * 몽타주 배열은 기본 인덱스 0을 재생하며, 콤보(단계) / 랜덤(변형 풀) 여부는 저장 구조가 아니라
 * 인덱스를 넘기는 호출자가 결정한다. (데이터 에셋은 순수 조회 테이블)
 */
USTRUCT(BlueprintType)
struct FCBActionMontageEntry
{
	GENERATED_BODY()

	/** 액션 식별 태그 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta = (Categories = "Action"))
	FGameplayTag ActionTag;

	/**
	 * 이 액션의 몽타주 배열.
	 * 인덱스 0이 기본 재생. 콤보면 순서(단계), 랜덤이면 변형 풀로 해석되며, 해석은 호출자가 결정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<TObjectPtr<UAnimMontage>> Montages;

	/** 공격 속도(AttackSpeed) 어트리뷰트의 영향을 받아 재생 속도가 조절되는지 여부 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bAffectedByAttackSpeed = false;

	bool IsValid() const { return !Montages.IsEmpty(); }
};


UCLASS()
class CHAINBURST_API UCBActionMontageData : public UDataAsset
{
	GENERATED_BODY()

protected:
	/** [에디터용 배열] 태그 → 몽타주 엔트리 */
	UPROPERTY(EditDefaultsOnly, Category = "Montages", meta = (TitleProperty = "ActionTag"))
	TArray<FCBActionMontageEntry> MontageEntries;

	/** [런타임용 맵] 태그 → 몽타주 엔트리 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Montages")
	TMap<FGameplayTag, FCBActionMontageEntry> MontageMap;

#if WITH_EDITOR
	/** 에디터에서 배열 데이터를 수정할 때마다 TMap을 동기화 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);
		UpdateRuntimeMap();
	}
#endif

public:
	/**
	 * 몽타주 조회
	 * @param InTag 조회할 액션 태그
	 * @param InIndex 재생할 몽타주 인덱스. 콤보 단계 또는 랜덤 변형 인덱스 — 의미는 호출자가 결정. (단일 액션은 0)
	 * @param bOutAffectedByAttackSpeed 해당 엔트리가 공격 속도 영향을 받는지 여부를 반환 (재생 시 PlayRate 결정에 사용)
	 * @return 없거나 인덱스 초과 시 nullptr
	 */
	UAnimMontage* FindMontage(const FGameplayTag& InTag, int32 InIndex, bool& bOutAffectedByAttackSpeed) const;

	/** 해당 태그의 몽타주 개수 반환 (콤보 단계 수 또는 랜덤 변형 수). 없으면 0. */
	int32 GetMontageCount(const FGameplayTag& InTag) const;

	/** 게임 시작 시, 에디터 켤 때 호출 */
	virtual void PostLoad() override
	{
		Super::PostLoad();
		UpdateRuntimeMap();
	}

private:
	/** 배열을 순회하며 TMap을 채워주는 함수 */
	void UpdateRuntimeMap()
	{
		MontageMap.Empty();
		for (const FCBActionMontageEntry& Entry : MontageEntries)
		{
			MontageMap.Add(Entry.ActionTag, Entry);
		}
	}
};
