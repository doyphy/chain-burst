#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CBCharacterCatalog.generated.h"

class ACBChaserCharacter;
class UTexture2D;

/**
 * 고를 수 있는 캐릭터 한 항목.
 * 무기 종류가 곧 캐릭터 종류이므로(무기마다 캐릭터 BP·로드아웃이 따로 있음) 무기 태그를 그대로 식별자로 씀.
 */
USTRUCT(BlueprintType)
struct FCBCharacterEntry
{
	GENERATED_BODY()

	/** 항목을 구분하는 고유 태그. PlayerState 에 저장되어 스폰할 클래스를 결정하는 키가 됨. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Item.Weapon"))
	FGameplayTag CharacterId;

	/** 스폰할 캐릭터 블루프린트 클래스. 고른 것만 있으면 되고 로비가 미리 로드하므로 소프트 참조 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftClassPtr<ACBChaserCharacter> CharacterClass;

	/** UI 에 표시할 이름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText DisplayName;

	/** UI 에 표시할 아이콘. 위젯이 필요할 때 로드하므로 소프트 참조 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon = nullptr;

	/** 조회 키와 스폰할 클래스가 모두 지정됐는지 검사하는 함수 */
	bool IsValid() const;
};

/**
 * 고를 수 있는 캐릭터(무기) 목록을 담는 카탈로그.
 * 게임 인스턴스가 보유해 서버(스폰 클래스 결정)와 전 클라이언트(선택 UI)가 같은 목록을 봄.
 */
UCLASS()
class CHAINBURST_API UCBCharacterCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 태그로 항목을 찾는 함수.
	 * @param InCharacterId 찾을 캐릭터 태그
	 * @return 찾은 항목. 등록되지 않았으면 nullptr
	 */
	const FCBCharacterEntry* FindEntry(const FGameplayTag& InCharacterId) const;

	/**
	 * 그 태그로 스폰할 수 있는지 검사하는 함수. (서버가 클라이언트 요청을 검증할 때 사용)
	 * @param InCharacterId 검사할 캐릭터 태그
	 */
	bool IsValidCharacterId(const FGameplayTag& InCharacterId) const;

	/**
	 * 태그에 해당하는 캐릭터 클래스를 반환하는 함수.
	 * 아직 로드되지 않았으면 동기 로드하므로, 로비에서 PreloadCharacterClasses 로 미리 로드해 둘 것.
	 * @param InCharacterId 스폰할 캐릭터 태그
	 * @return 스폰할 클래스. 등록되지 않았거나 로드에 실패하면 nullptr
	 */
	UClass* LoadCharacterClass(const FGameplayTag& InCharacterId) const;

	/**
	 * 등록된 캐릭터 클래스 경로를 모두 가져오는 함수. (미리 로드용)
	 * 설정이 덜 된 항목(태그나 클래스 누락)은 제외됨.
	 * @param OutClassPaths 결과를 담을 배열 (기존 내용은 비워짐)
	 */
	void GetCharacterClassPaths(TArray<FSoftObjectPath>& OutClassPaths) const;

	/**
	 * 등록된 항목을 모두 가져오는 함수. (선택 UI 가 버튼을 만들 때 사용)
	 * 설정이 덜 된 항목(태그나 클래스 누락)은 제외됨.
	 * @param OutEntries 결과를 담을 배열 (기존 내용은 비워짐)
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Character")
	void GetAllEntries(TArray<FCBCharacterEntry>& OutEntries) const;

protected:
	/** 등록된 캐릭터 항목. 배열 순서가 UI 표시 순서 */
	UPROPERTY(EditDefaultsOnly, Category = "Character", meta = (TitleProperty = "CharacterId"))
	TArray<FCBCharacterEntry> Entries;
};
