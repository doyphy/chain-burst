// project
#include "GameplayCues/CBGCN_PlayAction.h"

#include "CBGameplayTags.h"
#include "Characters/CBBaseCharacter.h"

bool UCBGCN_PlayAction::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	FGameplayTagContainer SourceTags = Parameters.AggregatedSourceTags;

	// 액션 태그 꺼내기
	FGameplayTag ActionTag;
	for (const FGameplayTag& Tag : SourceTags)
	{
		// 부모 태그에 "Action" 이 있다면
		if (Tag.MatchesTag(CBGameplayTags::Action))
		{
			ActionTag = Tag;
			break;
		}
	}

	// 콤보 인덱스 꺼내기 (어빌리티가 RawMagnitude에 실어 보냄. 단일 액션은 0)
	const int32 ComboIndex = FMath::RoundToInt(Parameters.RawMagnitude);

	if (ActionTag.IsValid())
	{
		if (ACBBaseCharacter* Char = Cast<ACBBaseCharacter>(MyTarget))
		{
			// 몽타주 재생 요청 (받은 인덱스로 클립 재생)
			Char->RequestPlayMontage(ActionTag, ComboIndex);
		}
	}

	return Super::OnExecute_Implementation(MyTarget, Parameters);
}
