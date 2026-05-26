#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTags.h"
#include "CBInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

USTRUCT(BlueprintType)
struct FCBInputActionConfig
{
	GENERATED_BODY()
public:
	/** 입력 태그 (예: 이동, 시점 등). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input"))
	FGameplayTag InputTag;

	/** 입력 액션 객체. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* InputAction = nullptr;

	/** 유효성 검사 함수 (태그와 액션 모두 유효한지) */
	bool IsValid() const { return InputTag.IsValid() && InputAction; }
};


UCLASS()
class CHAINBURST_API UCBInputConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:
	/**
	 * InputTag에 맞는 InputAction을 찾아 반환합니다.
	 * @param InputTag 검색 태그
	 * @return 찾은 InputAction 객체, 없으면 nullptr
	 */
	UInputAction* FindNativeInputActionByTag(const FGameplayTag& InputTag) const;

public:
	/** 기본 매핑 컨텍스트 객체. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	/** 기본 InputAction 목록. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputTag"))
	TArray<FCBInputActionConfig> NativeInputActions;

	/** 어빌리티 InputAction 목록. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputTag"))
	TArray<FCBInputActionConfig> AbilityInputActions;
};
