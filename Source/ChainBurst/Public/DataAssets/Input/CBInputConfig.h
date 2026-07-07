#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTags.h"
#include "InputTriggers.h"
#include "CBInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;
enum class EDataValidationResult : uint8;
class FDataValidationContext;

/**
 * 입력 태그 ↔ InputAction 매핑 항목 (+ 일반 입력의 트리거 단계).
 * 일반 입력(NativeInputActions)·어빌리티 입력(AbilityInputActions) 목록의 공통 항목 타입.
 */
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
	TObjectPtr<UInputAction> InputAction = nullptr;

	/**
	 * [일반 입력 전용] 이 입력이 반응할 트리거 단계.
	 * 어빌리티 입력은 Started/Completed(누름/뗌)로 고정 처리되어 이 값을 무시.
	 * - GAS 는 Pressed / Released 로 두 신호로 설계되어 있어, 어빌리티 입력은 Started/Completed 로 처리하는 것이 적합.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	ETriggerEvent TriggerEvent = ETriggerEvent::Triggered;

	/** 유효성 검사 함수 (태그와 액션 모두 유효한지) */
	bool IsValid() const { return InputTag.IsValid() && InputAction; }
};


/**
 * 캐릭터가 사용하는 매핑 컨텍스트 항목 (컨텍스트 + 우선순위).
 * 캐릭터 조작에 본질적인 IMC만 여기서 관리한다.
 * (차량·UI·일시정지 등 상황/전역 컨텍스트는 그 상황과 가장 밀접한 클래스가 소유)
 */
USTRUCT(BlueprintType)
struct FCBMappingContextEntry
{
	GENERATED_BODY()
public:
	/** 매핑 컨텍스트 객체. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext = nullptr;

	/** 우선순위 (높을수록 상위 레이어로 먼저 처리). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	int32 Priority = 0;

	/** 유효성 검사 함수 (매핑 컨텍스트가 유효한지) */
	bool IsValid() const { return MappingContext != nullptr; }
};


/**
 * 캐릭터 입력 선언 데이터 에셋 (입력 매니페스트).
 * "무엇"만 담는다 — 매핑 컨텍스트 목록 + 일반/어빌리티 입력의 태그↔액션 매핑.
 * 바인딩·매핑 컨텍스트 등록·잠금 등 "어떻게/언제"는 입력 컴포넌트·매니저가 담당한다.
 */
UCLASS()
class CHAINBURST_API UCBInputConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:
	/**
	 * InputTag에 맞는 일반 입력 설정(액션+트리거)을 찾아 반환합니다.
	 * @param InputTag 검색 태그
	 * @return 찾은 설정 항목 포인터, 없으면 nullptr
	 */
	const FCBInputActionConfig* FindNativeInputConfigByTag(const FGameplayTag& InputTag) const;

#if WITH_EDITOR
	//~ Begin UObject Interface.
	/** 에디터 저장/쿡 시점 데이터 검증 (태그 유효성·중복, 액션·컨텍스트 누락 검사) */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	//~ End UObject Interface.
#endif

public:
	/** 캐릭터가 사용하는 매핑 컨텍스트 목록 (컨텍스트 + 우선순위). 매니저가 순회하며 서브시스템에 등록. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TArray<FCBMappingContextEntry> MappingContexts;

	/** 기본 InputAction 목록. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputTag"))
	TArray<FCBInputActionConfig> NativeInputActions;

	/** 어빌리티 InputAction 목록. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputTag"))
	TArray<FCBInputActionConfig> AbilityInputActions;
};
