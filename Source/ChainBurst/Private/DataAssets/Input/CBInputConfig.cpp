#include "DataAssets/Input/CBInputConfig.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FCBInputActionConfig* UCBInputConfig::FindNativeInputConfigByTag(const FGameplayTag& InputTag) const
{
	// NativeInputActions 배열을 순회하며
	for(const FCBInputActionConfig& InputActionConfig : NativeInputActions)
	{
		// 입력 태그가 일치하고, 입력 액션이 유효한 경우
		if(InputActionConfig.InputTag == InputTag && InputActionConfig.InputAction)
		{
			// 해당 설정 항목(액션+트리거)을 반환
			return &InputActionConfig;
		}
	}
	// 일치하는 입력 태그가 없으면 nullptr 반환
	return nullptr;
}

#if WITH_EDITOR
// 입력 액션 배열 하나를 검증하는 내부 헬퍼 (유효성 + 카테고리 내 태그 중복 검사)
static void ValidateInputActionArray(const TArray<FCBInputActionConfig>& Actions, const TCHAR* ArrayName,
                                     FDataValidationContext& Context, EDataValidationResult& Result)
{
	TSet<FGameplayTag> SeenTags;

	for (int32 Index = 0; Index < Actions.Num(); ++Index)
	{
		const FCBInputActionConfig& Config = Actions[Index];

		// 입력 태그 유효성 / 중복 검사
		if (!Config.InputTag.IsValid())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s[%d] 의 InputTag 가 유효하지 않습니다."), ArrayName, Index)));
			Result = EDataValidationResult::Invalid;
		}
		else if (SeenTags.Contains(Config.InputTag))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s 에 InputTag '%s' 가 중복되었습니다."), ArrayName, *Config.InputTag.ToString())));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			SeenTags.Add(Config.InputTag);
		}

		// 입력 액션 누락 검사
		if (!Config.InputAction)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s[%d] (%s) 의 InputAction 이 비어 있습니다."),
				ArrayName, Index, *Config.InputTag.ToString())));
			Result = EDataValidationResult::Invalid;
		}
	}
}

EDataValidationResult UCBInputConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	// 매핑 컨텍스트 목록 검사 (비어 있거나 항목에 누락이 있으면 오류)
	if (MappingContexts.IsEmpty())
	{
		Context.AddError(FText::FromString(TEXT("MappingContexts 가 비어 있습니다. 최소 하나의 매핑 컨텍스트가 필요합니다.")));
		Result = EDataValidationResult::Invalid;
	}
	else
	{
		for (int32 Index = 0; Index < MappingContexts.Num(); ++Index)
		{
			if (!MappingContexts[Index].MappingContext)
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("MappingContexts[%d] 의 MappingContext 가 비어 있습니다."), Index)));
				Result = EDataValidationResult::Invalid;
			}
		}
	}

	// 일반/어빌리티 입력 배열 각각 검증
	ValidateInputActionArray(NativeInputActions, TEXT("NativeInputActions"), Context, Result);
	ValidateInputActionArray(AbilityInputActions, TEXT("AbilityInputActions"), Context, Result);

	return Result;
}
#endif
