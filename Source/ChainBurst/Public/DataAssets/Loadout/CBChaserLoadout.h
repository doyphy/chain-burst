#pragma once

#include "CoreMinimal.h"
#include "DataAssets/Loadout/CBCharacterLoadout.h"
#include "GameplayTagContainer.h"
#include "CBChaserLoadout.generated.h"

class UCBGameplayAbility;
class UCBAbilitySystemComponent;
class UCBInputConfig;
class ACBChaserCharacter;
class UCBHealthBarWidget;
class USkeletalMesh;

USTRUCT(BlueprintType)
struct FCBInputAbilitySet
{
	GENERATED_BODY()

	/** 어빌리티 입력 태그 (어떤 입력 태그와 연결된 어빌리티인지 구분) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Input"))
	FGameplayTag InputTag;
	
	/** 어빌리티 클래스 (입력 태그로 구분) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UCBGameplayAbility> AbilityToGrant;

	/** InputTag와 AbilityToGrant가 유효한지 확인하는 함수 */
	bool IsValid() const;
};

UCLASS()
class CHAINBURST_API UCBChaserLoadout : public UCBCharacterLoadout
{
	GENERATED_BODY()
	
private:
	/** 추격자의 어빌리티 세트 배열. 각 세트는 입력 태그와 어빌리티 클래스를 포함 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|Abilities", meta = (TitleProperty = "InputTag"))
	TArray<FCBInputAbilitySet> InputAbilitySets;

	/** 입력 설정 데이터 에셋 (매핑 컨텍스트 + 입력 태그↔액션 바인딩). 소유 클라이언트에서만 사용 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|Input")
	TObjectPtr<UCBInputConfig> InputConfig = nullptr;

	/** HUD 체력 위젯 클래스 (소유 클라이언트 전용. UCBUIComponent에 주입되어 준비 완료 후 뷰포트에 추가됨) */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|UI")
	TSubclassOf<UCBHealthBarWidget> HUDHealthWidgetClass = nullptr;

	/**
	 * 본체 메시(리더)를 따라갈 팔로워 스켈레탈 메시 목록 (MetaHuman 바디·의상 파츠 등).
	 * UCBModularMeshComponent에 주입되어 준비 완료 후 컴포넌트가 생성·부착.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|Visuals")
	TArray<TObjectPtr<USkeletalMesh>> FollowerMeshes;

	/**
	 * 리더 메시를 숨길지 여부 (팔로워가 몸 전체를 덮는 구성에서 true).
	 * 팔로워 목록에 종속되는 값이라 FollowerMeshes와 한 세트로 관리.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout|Visuals")
	bool bHideLeaderMesh = false;

public:
	/** 어빌리티 시스템 컴포넌트에 어빌리티를 부여하는 함수 (부모 함수 재정의, 추격자 전용) */
	virtual void Auth_GrantAbilitiesToASC(UCBAbilitySystemComponent* InASCToGive, int32 ApplyLevel = 1) override;

	/**
	 * [공용] 전 인스턴스 공용 데이터를 적용한다 (부모 처리 + 추격자 전용 팔로워 메시 주입).
	 */
	virtual void ApplyToCharacter(ACBBaseCharacter* InCharacter) override;

	/**
	 * [로컬 전용] 소유 클라이언트에서 필요한 데이터(입력 설정 등)를 캐릭터에 적용한다.
	 * 입력 설정을 주입하면 캐릭터가 입력 컴포넌트 준비 여부를 확인해 지연 바인딩을 수행한다.
	 */
	void Local_ApplyToCharacter(ACBChaserCharacter* InCharacter);
};
