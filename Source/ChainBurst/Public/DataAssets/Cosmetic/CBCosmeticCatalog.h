#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Types/CBEnumTypes.h"
#include "CBCosmeticCatalog.generated.h"

class USkeletalMesh;

/**
 * 교체 가능한 의상 파츠 데이터 구조체.
 */
USTRUCT(BlueprintType)
struct FCBCosmeticPart
{
	GENERATED_BODY()

	/** 파츠를 구분하는 고유 태그. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Item.Cosmetic"))
	FGameplayTag PartId;

	/** 입힐 스켈레탈 메시. 선택된 파츠만 로드하면 되므로 소프트 참조 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftObjectPtr<USkeletalMesh> Mesh = nullptr;

	/** UI에 표시할 이름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText DisplayName;

	/** 조회 키와 입힐 메시가 모두 지정됐는지 검사하는 함수 */
	bool IsValid() const;
};

/**
 * 한 부위에 들어갈 수 있는 파츠 목록.
 */
USTRUCT(BlueprintType)
struct FCBCosmeticPartList
{
	GENERATED_BODY()

	/** 해당 부위의 파츠들. 배열 순서가 순회 순서 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic", meta = (TitleProperty = "PartId"))
	TArray<FCBCosmeticPart> Parts;
};

/**
 * 교체 가능한 의상 파츠 목록을 담는 카탈로그.
 * 로드아웃이 하드 참조로 보유해 로드아웃과 함께 로드되며, 무거운 파츠 메시만 소프트 참조로 남겨 필요할 때 로드함.
 * 파츠와 파츠 태그의 원본은 이 에셋 하나뿐이므로, 파츠를 추가/삭제/수정하려면 이 에셋을 수정해야 함.
 */
UCLASS()
class CHAINBURST_API UCBCosmeticCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 부위와 태그로 파츠를 찾는 함수.
	 * @param InSlot 조회할 부위 슬롯
	 * @param InPartId 찾을 파츠 태그
	 * @return 찾은 파츠. 등록되지 않았으면 nullptr
	 */
	const FCBCosmeticPart* FindPart(ECBCosmeticSlot InSlot, const FGameplayTag& InPartId) const;

	/**
	 * 특정 부위의 등록된 파츠 태그 모두 가져오는 함수.
	 * 설정이 덜 된 항목(태그나 메시 누락)은 제외됨.
	 * @param InSlot 조회할 부위 슬롯
	 * @param OutPartIds 결과를 담을 배열 (기존 내용은 비워짐)
	 */
	void GetPartIdsForSlot(ECBCosmeticSlot InSlot, TArray<FGameplayTag>& OutPartIds) const;

	/**
	 * 해당 부위에 그 파츠를 입힐 수 있는지 검사하는 함수.
	 * @param InSlot 입히려는 부위 슬롯
	 * @param InPartId 입히려는 파츠 태그
	 */
	bool IsValidPartForSlot(ECBCosmeticSlot InSlot, const FGameplayTag& InPartId) const;

protected:
	/** 부위별 등록 파츠. */
	UPROPERTY(EditDefaultsOnly, Category = "Cosmetic")
	TMap<ECBCosmeticSlot, FCBCosmeticPartList> PartsBySlot;
};
