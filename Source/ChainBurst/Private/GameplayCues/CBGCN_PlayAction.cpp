// project
#include "GameplayCues/CBGCN_PlayAction.h"

#include "CBGameplayTags.h"
#include "Characters/CBBaseCharacter.h"

bool UCBGCN_PlayAction::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	FGameplayTagContainer SourceTags = Parameters.AggregatedSourceTags;

	// 액션 태그와 콤보 여부 꺼내기
	FGameplayTag ActionTag;
	bool IsCombo = false;

	for (const FGameplayTag& Tag : SourceTags)
	{
		// 부모 태그에 "Action" 이 있다면
		if (Tag.MatchesTag(CBGameplayTags::Action))
		{
			ActionTag = Tag;
		}
		// 콤보 여부 태그가 있다면
		else if (Tag == CBGameplayTags::Context_Action_IsCombo)
		{
			IsCombo = true;
		}
	}

	if (ActionTag.IsValid())
	{
		ACBBaseCharacter* Char = Cast<ACBBaseCharacter>(MyTarget);
		// 몽타주 재생 요청
		Char->RequestPlayMontage(ActionTag, IsCombo);
	}

	return Super::OnExecute_Implementation(MyTarget, Parameters);
}
