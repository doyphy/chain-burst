#include "DataAssets/Input/CBInputConfig.h"

UInputAction* UCBInputConfig::FindNativeInputActionByTag(const FGameplayTag& InputTag) const
{
	// NativeInputActions 배열을 순회하며
	for(const FCBInputActionConfig& InputActionConfig : NativeInputActions)
	{
		// 입력 태그가 일치하고, 입력 액션이 유효한 경우
		if(InputActionConfig.InputTag == InputTag && InputActionConfig.InputAction)
		{
			// 해당 입력 액션을 반환
			return InputActionConfig.InputAction;
		}
	}
	// 일치하는 입력 태그가 없으면 nullptr 반환
	return nullptr;
}
