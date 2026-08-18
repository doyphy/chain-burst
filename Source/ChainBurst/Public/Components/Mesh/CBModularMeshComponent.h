#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "GameplayTagContainer.h"
#include "Types/CBEnumTypes.h"
#include "CBModularMeshComponent.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UCBCosmeticCatalog;

/**
 * 모듈러 캐릭터 외형을 조립하는 공용 컴포넌트.
 * 캐릭터 본체 메시(ACharacter::Mesh)를 '리더'로 삼아, 팔로워 스켈레탈 메시 컴포넌트를
 * 런타임 생성·부착하고 SetLeaderPoseComponent로 리더의 포즈를 그대로 따라가게 함.
 * 의상은 부위 슬롯별로 컴포넌트를 항상 보유하고 메시만 갈아끼우므로 런타임 교체가 가능함.
 * 생성·부착은 전 인스턴스(서버·클라)가 각자 로컬로 수행하므로 복제 코드가 없음.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CHAINBURST_API UCBModularMeshComponent : public UCBExtensionComponent
{
	GENERATED_BODY()

public:
	/** 의상 슬롯 배열을 슬롯 개수만큼 확보함 (슬롯을 인덱스로 쓰므로 크기가 항상 고정) */
	UCBModularMeshComponent();

	/**
	 * 리더 메시 숨김 여부를 주입하는 세터 (전 인스턴스 공용 경로).
	 */
	FORCEINLINE void SetHideLeaderMesh(bool bInHideLeaderMesh) { bHideLeaderMesh = bInHideLeaderMesh; }

protected:
	//~ Begin UActorComponent Interface.
	/** 런타임 생성된 팔로워 컴포넌트를 정리 (피부·의상 모두). */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface.

	//~ Begin UCBExtensionComponent Interface.
	/** 준비 완료 시 피부·의상 팔로워를 생성해 리더 메시에 부착 (리더 메시 에셋이 적용된 뒤 시점을 보장받기 위함). */
	virtual void OnCharacterSystemReady() override;
	//~ End UCBExtensionComponent Interface.

	/**
	 * 리더 메시의 렌더링을 숨길지 여부. 로드아웃에서 주입되는 런타임 캐시.
	 * 켜면 리더 메시를 숨긴 뒤에도 리더 메시의 포즈가 갱신되도록 VisibilityBasedAnimTickOption을 함께 조정.
	 * 리더는 BP 에서 꺼진 채로 스폰되므로, 이 값이 false 면 준비 완료 시점에 켜 주는 것도 이 컴포넌트가 담당함.
	 */
	UPROPERTY()
	bool bHideLeaderMesh = false;

#pragma region Skin
	/** 피부(MetaHuman 바디) 파츠. 로드아웃이 목록 그대로 주입. */
public:
	/**
	 * 피부 팔로워 메시 목록을 주입하는 세터 (전 인스턴스 공용 경로).
	 * 준비 완료(OnCharacterSystemReady) 이전에 호출해야 반영. 로드아웃 적용 경로에서 사용.
	 */
	void SetSkinMeshes(const TArray<TObjectPtr<USkeletalMesh>>& InSkinMeshes);

	/** 런타임 생성된 피부 팔로워 컴포넌트 목록 (인덱스는 SkinMeshes와 대응, 생성 실패분은 nullptr) */
	FORCEINLINE const TArray<TObjectPtr<USkeletalMeshComponent>>& GetSkinComponents() const { return SkinComponents; }

protected:
	/** 
	 * 리더 메시를 따라갈 피부 파츠 목록. 로드아웃에서 주입되는 런타임 캐시 
	 * 컴포넌트 생성 전 임시로 메시를 보관하는 곳으로 사용 (로드아웃 전용)
	 * 캐릭터 시스템이 완료되고 SkinComponent를 만들면서 메시 주입.
	 */
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMesh>> SkinMeshes;

private:
	/** 런타임 생성한 피부 팔로워 컴포넌트 목록 */
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> SkinComponents;
#pragma endregion

#pragma region Cosmetic
	/** 부위 슬롯(ECBCosmeticSlot)별 의상 파츠. 배열 인덱스가 곧 슬롯. (빈 슬롯은 입히지 않음) */
public:
	/** 부위별 기본 의상 파츠 태그를 주입하는 세터 (로드아웃에서 적용). */
	void SetDefaultCosmeticIds(const TMap<ECBCosmeticSlot, FGameplayTag>& InDefaultCosmeticIds);

	/**
	 * 기본 의상 파츠의 메시를 미리 로드한 뒤 완료를 알리는 함수.
	 * 캐릭터 준비 완료가 이 로드를 기다림.
	 * @param OnComplete 로드가 끝난 뒤 호출될 콜백
	 */
	void PreloadDefaultCosmetics(TFunction<void()> OnComplete);

	/**
	 * 슬롯 하나의 의상 메시를 교체함. nullptr을 넘기면 그 부위를 벗김.
	 * @param InSlot 교체할 부위 슬롯
	 * @param InMesh 입힐 메시 (nullptr = 벗김)
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Cosmetic")
	void SetCosmeticForSlot(ECBCosmeticSlot InSlot, USkeletalMesh* InMesh);

	/**
	 * 파츠 카탈로그를 주입하는 세터.
	 * 파츠 태그로 교체할 때 메시를 조회할 목록임. 로드아웃 적용 경로에서 사용.
	 */
	void SetCosmeticCatalog(UCBCosmeticCatalog* InCosmeticCatalog);

	/**
	 * 파츠 태그로 슬롯 의상을 교체함.
	 * 메시를 비동기 로드한 뒤 적용되므로 즉시 반영되지 않음 (벗는 경우만 즉시).
	 * 카탈로그에 없거나 부위가 다른 파츠는 경고만 남기고 무시함.
	 * @param InSlot 교체할 부위 슬롯
	 * @param InPartId 입힐 파츠 태그. Item.Cosmetic.None = 그 부위를 벗김 / 빈 태그 = 선택 안 함(기본 의상 유지)
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Cosmetic")
	void RequestCosmeticPart(ECBCosmeticSlot InSlot, const FGameplayTag& InPartId);

	/**
	 * 해당 부위에 그 파츠를 입힐 수 있는지 카탈로그로 검사하는 함수 (서버 검증용).
	 * 카탈로그가 없거나 등록되지 않은 파츠, 부위가 다른 파츠는 모두 거부됨.
	 * @param InSlot 입히려는 부위 슬롯
	 * @param InPartId 입히려는 파츠 태그
	 */
	bool IsValidCosmeticPart(ECBCosmeticSlot InSlot, const FGameplayTag& InPartId) const;

	/** 그 부위가 지금 입고 있는 파츠 태그 반환. */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Cosmetic")
	FGameplayTag GetCurrentCosmeticPartId(ECBCosmeticSlot InSlot) const;

	/**
	 * 현재 착용 파츠 기준으로 다음 파츠 태그를 반환함. (순회용)
	 * @param InSlot 조회할 부위 슬롯
	 * @return 다음 파츠 태그. 등록된 파츠가 없으면 빈 태그
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Cosmetic")
	FGameplayTag GetNextCosmeticPartId(ECBCosmeticSlot InSlot) const;

	/**
	 * 현재 착용 파츠 기준으로 이전 파츠 태그를 반환함. (순회용)
	 * @param InSlot 조회할 부위 슬롯
	 * @return 이전 파츠 태그. 등록된 파츠가 없으면 빈 태그
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Cosmetic")
	FGameplayTag GetPreviousCosmeticPartId(ECBCosmeticSlot InSlot) const;

	/**
	 * 런타임 생성된 의상 팔로워 컴포넌트 목록 (인덱스 = ECBCosmeticSlot).
	 * 벗은 슬롯도 컴포넌트를 유지하므로 항상 슬롯 개수만큼 존재하며, 그 경우 메시만 비어 있음.
	 */
	FORCEINLINE const TArray<TObjectPtr<USkeletalMeshComponent>>& GetCosmeticComponents() const { return CosmeticComponents; }

protected:
	/** 
	 * 슬롯별 의상 메시. 인덱스가 ECBCosmeticSlot이며 항상 슬롯 개수만큼 유지. 로드아웃에서 주입되는 런타임 캐시
	 * 컴포넌트 생성 전 임시로 메시를 보관하는 곳으로 사용 (로드아웃 전용)
	 * 캐릭터 시스템이 완료되고 CosmeticComponent를 만들면서 메시 주입.
	 */
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMesh>> CosmeticMeshes;

	/** 파츠 태그로 메시를 조회할 카탈로그. 로드아웃에서 주입되는 런타임 캐시 */
	UPROPERTY()
	TObjectPtr<UCBCosmeticCatalog> CosmeticCatalog = nullptr;

	/** 부위별 기본 의상 파츠 태그. 로드아웃에서 주입되며 PreloadDefaultCosmetics 가 이 값으로 메시를 로드함 */
	UPROPERTY()
	TMap<ECBCosmeticSlot, FGameplayTag> DefaultCosmeticIds;

private:
	/** 런타임 생성한 의상 팔로워 컴포넌트 목록 (인덱스는 CosmeticMeshes와 대응) */
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> CosmeticComponents;

	/** 슬롯(인덱스)별로 마지막에 요청한 파츠 태그 (인덱스 = ECBCosmeticSlot). 요청 추적용이지 착용 상태가 아님 */
	UPROPERTY()
	TArray<FGameplayTag> RequestedParts;

	/** 슬롯(인덱스)별로 실제로 적용된 파츠 태그 (인덱스 = ECBCosmeticSlot). 순회 계산의 기준 */
	UPROPERTY()
	TArray<FGameplayTag> CurrentPartIds;

	/** 비동기 로드가 끝난 파츠를 적용하는 내부 함수 (그 사이 다른 요청이 왔으면 버림) */
	void HandleCosmeticPartLoaded(ECBCosmeticSlot InSlot, const FGameplayTag& InPartId, USkeletalMesh* InLoadedMesh);

	/** 로드가 끝난 기본 의상을 슬롯에 반영하는 내부 함수 (PreloadDefaultCosmetics 콜백에서 호출) */
	void ApplyDefaultCosmetics();

	/** 슬롯의 현재 착용 태그를 기록하는 내부 함수 (인덱스 범위를 여기서 처리) */
	void SetCurrentPartId(ECBCosmeticSlot InSlot, const FGameplayTag& InPartId);

	/**
	 * 현재 착용 파츠에서 한 칸 이동한 태그를 구하는 내부 함수 (다음/이전 공용).
	 * @param InStep +1 = 다음, -1 = 이전
	 */
	FGameplayTag GetCosmeticPartIdByStep(ECBCosmeticSlot InSlot, int32 InStep) const;
#pragma endregion

private:
	/** 캐싱한 리더 메시. 런타임 교체에서 리더 포즈를 다시 연결할 때 사용 (캐릭터와 생명 주기가 같아 하드 참조) */
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> CachedLeader = nullptr;

	/**
	 * 메시 목록으로 팔로워 컴포넌트를 생성해 전달받은 배열을 채우는 내부 함수.
	 * @param bInKeepEmptySlots 메시가 없는 자리에도 빈 컴포넌트를 만들지 여부 (의상=true 런타임 교체용 / 피부=false)
	 */
	void BuildFollowers(const TArray<TObjectPtr<USkeletalMesh>>& InMeshes, USkeletalMeshComponent* InLeader, const FString& InNamePrefix, bool bInKeepEmptySlots, TArray<TObjectPtr<USkeletalMeshComponent>>& OutComponents);

	/** 팔로워 컴포넌트 하나를 생성·부착하고 리더 포즈에 연결하는 내부 함수 */
	USkeletalMeshComponent* CreateFollowerComponent(USkeletalMesh* InMesh, USkeletalMeshComponent* InLeader, const FName& InComponentName);

	/** 런타임 생성한 팔로워 컴포넌트를 파괴하고 배열을 비우는 내부 함수 */
	static void DestroyFollowers(TArray<TObjectPtr<USkeletalMeshComponent>>& InOutComponents);
};
