#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "DataAssets/Input/CBInputConfig.h"
#include "CBInputComponent.generated.h"

UCLASS()
class CHAINBURST_API UCBInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()
	
public:
	/**
	 * 기본 입력 액션을 바인딩하는 템플릿 함수.
	 * 트리거 이벤트(ETriggerEvent)는 InputConfig 데이터에서 태그별로 지정된 값을 사용한다.
	 * @tparam UserObject
	 * @tparam CallbackFunc
	 * @param InInputConfig 검색할 입력 데이터 에셋
	 * @param InInputTag  검색할 입력 태그
	 * @param ContextObject 바인딩할 객체
	 * @param Func 바인딩할 콜백 함수
	 */
	template<class UserObject, typename CallbackFunc>
	void BindNativeInputAction(const UCBInputConfig* InInputConfig, const FGameplayTag& InInputTag, UserObject* ContextObject, CallbackFunc Func);

	/**
	 * 어빌리티 입력 액션을 바인딩하는 템플릿 함수
	 * 어빌리티 활성화는 태그를 찾아 해당 어빌리티를 활성화하는 방식으로 구현하기 때문에, 각각의 어빌리티마다 콜백함수를 만들필요가 없음.
	 * @tparam UserObject 
	 * @tparam CallbackFunc 
	 * @param InInputConfig 검색할 입력 데이터 에셋
	 * @param ContextObject 바인딩할 객체
	 * @param InputPressedFunc 눌렀을 때 실행할 함수
	 * @param InputReleasedFunc 뗐을 때 실행할 함수
	 */
	template<class UserObject, typename CallbackFunc>
	void BindAbilityInputAction(const UCBInputConfig* InInputConfig, UserObject* ContextObject, CallbackFunc InputPressedFunc, CallbackFunc InputReleasedFunc);
};

// BindNativeInputAction 구현부
template<class UserObject, typename CallbackFunc>
void UCBInputComponent::BindNativeInputAction(const UCBInputConfig* InInputConfig, const FGameplayTag& InInputTag, UserObject* ContextObject, CallbackFunc Func)
{
	// InInputConfig가 유효한지 확인
	checkf(InInputConfig, TEXT("InputConfig 데이터 에셋이 유효하지 않음."));

	// 데이터 에셋에서 InputTag에 맞는 일반 입력 설정(액션+트리거)을 검색
	if (const FCBInputActionConfig* Config = InInputConfig->FindNativeInputConfigByTag(InInputTag))
	{
		// 데이터에 지정된 트리거 이벤트로 바인딩 (FInputActionValue 매개 변수는 자동으로 추가됨)
		BindAction(Config->InputAction, Config->TriggerEvent, ContextObject, Func);
	}
	else
	{
		// 입력 태그에 해당하는 설정이 없을 경우 경고 로그 출력
		UE_LOG(LogTemp, Warning, TEXT("%s 태그에 해당하는 Native Input 설정이 %s 에 없음."), *InInputTag.ToString(), *InInputConfig->GetName());
	}
}

template <class UserObject, typename CallbackFunc>
void UCBInputComponent::BindAbilityInputAction(const UCBInputConfig* InInputConfig, UserObject* ContextObject,
	CallbackFunc InputPressedFunc, CallbackFunc InputReleasedFunc)
{
	checkf(InInputConfig, TEXT("InputConfig 데이터 에셋이 유효하지 않음."));
	// 입력 설정 데이터에서 능력 관련 입력 액션들을 가져옴
	for (const FCBInputActionConfig& AbilityInputActionConfig : InInputConfig->AbilityInputActions)
	{
		// Input Config 유효한지 확인
		if (!AbilityInputActionConfig.IsValid()) continue;
		// 입력 액션 바인딩
		// 매개 변수로 InputTag 태그를 넘김
		BindAction(AbilityInputActionConfig.InputAction, ETriggerEvent::Started, ContextObject, InputPressedFunc, AbilityInputActionConfig.InputTag);
		BindAction(AbilityInputActionConfig.InputAction, ETriggerEvent::Completed, ContextObject, InputReleasedFunc, AbilityInputActionConfig.InputTag);
	}
}
