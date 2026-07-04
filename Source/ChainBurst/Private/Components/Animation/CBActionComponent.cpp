// project
#include "Components/Animation/CBActionComponent.h"
#include "CBGameplayTags.h"
#include "DataAssets/Animation/CBActionMontageData.h"
#include "AnimInstances/CBCharacterAnimInstance.h"
#include "Characters/CBBaseCharacter.h"
#include "AbilitySystem/CBAttributeSet.h"

// engine
#include "Net/UnrealNetwork.h"

UCBActionComponent::UCBActionComponent()
{
	SetIsReplicatedByDefault(true);
}

bool UCBActionComponent::RequestPlayMontage(const FGameplayTag& InActionTag, int32 InIndex /* = 0 */)
{
	// 태그 유효성 검사
	if (!InActionTag.IsValid()) return false;

	// 몽타주 데이터 에셋 유효성 검사
	if (!MontageData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] MontageData 가 없음"), *GetOwner()->GetName());
		return false;
	}

	// 인덱스에 맞는 몽타주 가져오기 (콤보/랜덤 인덱스 의미는 호출자가 결정, 인덱스 검사는 FindMontage 내부)
	bool bAffectedByAttackSpeed = false;
	UAnimMontage* Montage = MontageData->FindMontage(InActionTag, InIndex, bAffectedByAttackSpeed);
	if (!Montage) return false;

	// 현재 액션 정보 업데이트 (폴백 딜레이용 지속 시간)
	CurrentActionDuration = Montage->GetPlayLength();

	// [디버그] 어떤 액션 태그의 몇 번 인덱스 몽타주를 재생하는지 로그
	UE_LOG(LogTemp, Log, TEXT("[ActionComp][%s] 몽타주 재생: 태그 '%s' / 인덱스 %d / 몽타주 '%s'"),
		*GetOwner()->GetName(), *InActionTag.ToString(), InIndex, *Montage->GetName());

	// 몽타주 재생
	return PlayMontage(Montage, bAffectedByAttackSpeed);
}

void UCBActionComponent::StopMontage(float BlendOutTime /* = 0.25 */)
{
	if (GetCachedAnimInstance(CachedAnimInstance))
	{
		// 현재 재생중인 몽타주 중지
		CachedAnimInstance->Montage_Stop(BlendOutTime);
	}
}

int32 UCBActionComponent::GetMontageCount(const FGameplayTag& InActionTag) const
{
	// 데이터 에셋 접근을 캡슐화 (외부는 이 함수만 사용)
	return MontageData ? MontageData->GetMontageCount(InActionTag) : 0;
}

void UCBActionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCBActionComponent, CurrentActionDuration);
}

FGameplayTag UCBActionComponent::SelectBestActionTag(const FGameplayTagContainer& InTags)
{
	FGameplayTag BestTag;
	int32 BestPriority = -1;

	// 태그 순회
	for (const FGameplayTag& Tag : InTags)
	{
		// Action 카테고리에 속하는 태그만 고려
		if (!Tag.MatchesTag(CBGameplayTags::Action))
		{
			continue;
		}

		// 태그의 우선 순위 가져오기
		int32 Priority = GetActionPriority(Tag);

		// 가장 우선 순위가 높은 태그 선택
		if (Priority > BestPriority)
		{
			BestPriority = Priority;
			BestTag = Tag;
		}
	}

	return BestTag;
}

int32 UCBActionComponent::GetActionPriority(FGameplayTag InTag)
{
	if (InTag.MatchesTag(CBGameplayTags::Action_Combat_HitReact))
		return 100;

	if (InTag.MatchesTag(CBGameplayTags::Action_Combat_Block))
		return 80;

	if (InTag.MatchesTag(CBGameplayTags::Action_Combat_Skill))
		return 60;

	if (InTag.MatchesTag(CBGameplayTags::Action_Combat_Attack))
		return 40;

	return 0;
}

bool UCBActionComponent::PlayMontage(UAnimMontage* InMontage, bool bAffectedByAttackSpeed)
{
	if (!InMontage) return false;

	if (!GetCachedAnimInstance(CachedAnimInstance))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] AnimInstance 캐싱 실패"), *GetOwner()->GetName());
		return false;
	}

	float PlayRate = 1.f;

	// 공격 속도 영향을 받는 몽타주만 AttackSpeed 어트리뷰트를 PlayRate에 반영
	if (bAffectedByAttackSpeed)
	{
		if (ACBBaseCharacter* Owner = GetOwningPawn<ACBBaseCharacter>())
		{
			if (const UCBAttributeSet* AttributeSet = Owner->GetCBAttributeSet())
			{
				PlayRate = AttributeSet->GetAttackSpeed();
			}
		}
	}

	CachedAnimInstance.Get()->PlayMontage(InMontage, PlayRate);
	return true;
}

bool UCBActionComponent::GetCachedAnimInstance(TWeakObjectPtr<UCBCharacterAnimInstance>& OutAnimInstance)
{
	// 이미 유효하면 바로 반환
	if (OutAnimInstance.IsValid())
	{
		return true;
	}
	
	ACBBaseCharacter* Owner = GetOwningPawn<ACBBaseCharacter>();
	if (!Owner) return false;

	USkeletalMeshComponent* Mesh = Owner->GetMesh();
	if (!Mesh) return false;

	UCBCharacterAnimInstance* AnimInst = Cast<UCBCharacterAnimInstance>(Mesh->GetAnimInstance());
	if (!AnimInst) return false;

	OutAnimInstance = AnimInst;

	return OutAnimInstance.IsValid();
}

