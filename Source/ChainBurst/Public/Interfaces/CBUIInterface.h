#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CBUIInterface.generated.h"

class UCBUIComponent;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UCBUIInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * UI를 가진 액터의 공용 접근 인터페이스.
 * 외부(타게팅·연출 등 로컬 UI 소비자)가 액터의 구체 타입을 몰라도 UI 컴포넌트에 접근할 수 있게 함.
 * 주의: UI는 각 클라이언트 로컬이므로 이 인터페이스 직접 호출은 호출한 클라이언트 화면에만 반영.
 * 게임플레이가 원인인 UI 변화는 복제되는 신호(어트리뷰트·태그·GameplayCue)를 UI 쪽에서 구독해 처리.
 */
class CHAINBURST_API ICBUIInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual UCBUIComponent* GetCBUIComponent() const = 0;
};
