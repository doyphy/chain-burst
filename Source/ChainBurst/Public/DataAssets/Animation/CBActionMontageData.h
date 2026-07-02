#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CBActionMontageData.generated.h"


/** 콤보 몽타주 데이터 */
USTRUCT(BlueprintType)
struct FCBSingleMontageData
{
	GENERATED_BODY()

	/** 싱글 몽타주 액션 태그 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite);
	FGameplayTag SingleActionTag;

	/** 싱글 몽타주 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UAnimMontage> Montage;

	/** 공격 속도(AttackSpeed) 어트리뷰트의 영향을 받아 재생 속도가 조절되는지 여부 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bAffectedByAttackSpeed = false;

	bool IsValid() const { return Montage != nullptr; }
};

/** 콤보 몽타주 데이터 */
USTRUCT(BlueprintType)
struct FCBComboMontageData
{
	GENERATED_BODY()

	/** 콤보 몽타주 액션 태그 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FGameplayTag ComboActionTag;
	
	/** 콤보 순서대로 나열된 몽타주 배열 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<TObjectPtr<UAnimMontage>> Montages;

	/** 공격 속도(AttackSpeed) 어트리뷰트의 영향을 받아 재생 속도가 조절되는지 여부 (콤보 전체에 공통 적용) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bAffectedByAttackSpeed = false;

	bool IsValid() const { return !Montages.IsEmpty(); }
};


UCLASS()
class CHAINBURST_API UCBActionMontageData : public UDataAsset
{
	GENERATED_BODY()

protected:
	/** [에디터용 배열] 태그 -> 단일 몽타주 */
	UPROPERTY(EditDefaultsOnly, Category = "Montages|Single", meta = (TitleProperty = "SingleActionTag"))
	TArray<FCBSingleMontageData> SingleMontageDataArray;

	/** [에디터용 배열] 태그 -> 콤보 몽타주 배열 */
	UPROPERTY(EditDefaultsOnly, Category = "Montages|Combo", meta = (TitleProperty = "ComboActionTag"))
	TArray<FCBComboMontageData> ComboMontageDataArray;
	
	/** [런타임용 맵] 태그 → 단일 몽타주 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Montages|Single")
	TMap<FGameplayTag, FCBSingleMontageData> SingleMontageMap;

	/** [런타임용 맵] 태그 → 콤보 몽타주 배열 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Montages|Combo") 
	TMap<FGameplayTag, FCBComboMontageData> ComboMontageMap;

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
	 * 단일 몽타주 조회
	 * @param bOutAffectedByAttackSpeed 해당 몽타주가 공격 속도 영향을 받는지 여부를 반환
	 * @return 없으면 nullptr
	 */
	UAnimMontage* FindSingleMontage(const FGameplayTag& InTag, bool& bOutAffectedByAttackSpeed) const;

	/**
	 * 콤보 몽타주 조회
	 * @param InComboIndex 현재 콤보 단계 (0부터 시작)
	 * @param bOutAffectedByAttackSpeed 해당 콤보가 공격 속도 영향을 받는지 여부를 반환
	 * @return 없거나 인덱스 초과 시 nullptr
	 */
	UAnimMontage* FindComboMontage(const FGameplayTag& InTag, int32 InComboIndex, bool& bOutAffectedByAttackSpeed) const;

	/** 해당 태그의 콤보 최대 단계 수 반환 */
	int32 GetComboCount(const FGameplayTag& InTag) const;

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
		SingleMontageMap.Empty();
		for (const FCBSingleMontageData& Data : SingleMontageDataArray)
		{
			SingleMontageMap.Add(Data.SingleActionTag, Data);
		}

		ComboMontageMap.Empty();
		for (const FCBComboMontageData& Data : ComboMontageDataArray)
		{
			ComboMontageMap.Add(Data.ComboActionTag, Data);
		}
	}
};
