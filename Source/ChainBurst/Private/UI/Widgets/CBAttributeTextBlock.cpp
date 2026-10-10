// project
#include "UI/Widgets/CBAttributeTextBlock.h"

void UCBAttributeTextBlock::SetAttributeValue(float InValue)
{
	// 정해진 자릿수까지만 표시 (천 단위 구분 기호 포함)
	FNumberFormattingOptions FormattingOptions = FNumberFormattingOptions::DefaultWithGrouping();
	FormattingOptions.SetMaximumFractionalDigits(MaximumFractionalDigits);

	SetText(FText::AsNumber(InValue, &FormattingOptions));
}
