#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "AttributeSet.h"
#include "CBAttributeTextBlock.generated.h"

/**
 * 어트리뷰트 하나의 현재 값을 숫자로 표시하는 텍스트.
 * 상태창(UCBStatusWindowWidget) WBP 에 배치하고 디테일 패널에서 어트리뷰트를 고르면, 상태창이 구독해 값을 넣어 줌. (스스로 구독하지 않음)
 */
UCLASS(meta = (DisplayName = "Attribute Text"))
class CHAINBURST_API UCBAttributeTextBlock : public UTextBlock
{
	GENERATED_BODY()

public:
	/** 표시할 어트리뷰트 (미지정이면 상태창이 건너뜀) */
	FORCEINLINE const FGameplayAttribute& GetAttribute() const { return Attribute; }

	/**
	 * 어트리뷰트 값을 표시 형식에 맞춰 텍스트로 반영하는 함수.
	 * @param InValue 표시할 어트리뷰트 값
	 */
	void SetAttributeValue(float InValue);

protected:
	/** 표시할 어트리뷰트. 비워 두면 값이 들어오지 않고 디자이너에 적은 텍스트가 그대로 남음 */
	UPROPERTY(EditAnywhere, Category = "ChainBurst|UI")
	FGameplayAttribute Attribute;

	/** 소수점 아래 최대 자릿수 (0 = 정수로 반올림, 끝의 0 은 생략). 배율 어트리뷰트(AttackSpeed 등)는 1~2 로 둘 것 */
	UPROPERTY(EditAnywhere, Category = "ChainBurst|UI", meta = (ClampMin = "0", ClampMax = "3"))
	int32 MaximumFractionalDigits = 0;
};
