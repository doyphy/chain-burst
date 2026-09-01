// project
#include "GameplayCues/CBGCN_PlayAction.h"
#include "CBGameplayTags.h"
#include "Characters/CBBaseCharacter.h"

// engine
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"

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
			// 모션 워핑 타겟 등록. 워프 타겟 이름은 액션 태그 자체를 사용.
			if (UMotionWarpingComponent* MotionWarpingComp = Char->GetMotionWarpingComponent())
			{
				// 파라미터에 대상 컴포넌트가 있으면 추종 워프.
				if (USceneComponent* TargetComponent = Parameters.TargetAttachComponent.Get())
				{
					// 좌표가 아니라 대상을 등록해 워프가 도는 동안 매 프레임 타겟 트랜스폼을 다시 읽음.
					// NormalizedMagnitude = 타겟에서 멈출 거리(cm).
					MotionWarpingComp->AddOrUpdateWarpTargetFromComponent(
						ActionTag.GetTagName(), TargetComponent, NAME_None, /*bFollowComponent =*/ true,
						EWarpTargetLocationOffsetDirection::VectorFromTargetToOwner,
						FVector(Parameters.NormalizedMagnitude, 0.f, 0.f));
				}
				// 파라미터에 좌표가 있으면 고정 워프.
				else if (!Parameters.Location.IsZero())
				{
					// 재생 시점에 고정된 좌표로 이동 (대시 등)
					MotionWarpingComp->AddOrUpdateWarpTargetFromLocationAndRotation(
						ActionTag.GetTagName(), Parameters.Location, Parameters.Normal.Rotation());
				}
			}

			// 몽타주 재생 요청 (받은 인덱스로 클립 재생)
			Char->RequestPlayMontage(ActionTag, ComboIndex);
		}
	}

	return Super::OnExecute_Implementation(MyTarget, Parameters);
}
