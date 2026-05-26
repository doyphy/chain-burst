// project
#include "Components/Animation/CBActionComponent.h"
#include "CBGameplayTags.h"
#include "DataAssets/Animation/CBActionMontageData.h"
#include "AnimInstances/CBCharacterAnimInstance.h"
#include "Characters/CBBaseCharacter.h"

// engine
#include "Net/UnrealNetwork.h"

UCBActionComponent::UCBActionComponent()
{
	SetIsReplicatedByDefault(true);
}

bool UCBActionComponent::RequestPlaySingleMontage(const FGameplayTag& InActionTag)
{
	if (!InActionTag.IsValid()) return false;

	if (!MontageData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] MontageData 가 없음"), *GetOwner()->GetName());
		return false;
	}
	
	UAnimMontage* Montage = MontageData->FindSingleMontage(InActionTag);
	if (!Montage) return false;

	// 현재 액션 정보 업데이트
	CurrentActionTag = InActionTag;
	CurrentActionDuration = Montage->GetPlayLength();
	
	ResetComboIndex();

	return PlayMontage(Montage);
}

bool UCBActionComponent::RequestPlayComboMontage(const FGameplayTag& InActionTag)
{
	if (!InActionTag.IsValid()) return false;

	if (!MontageData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] MontageData 가 없음"), *GetOwner()->GetName());
		return false;
	}

	// 다른 액션 태그로 전환 시 인덱스 초기화
	if (CurrentActionTag != InActionTag)
	{
		ResetComboIndex();
	}

	// 인덱스가 최대 콤보 수를 초과하면 초기화
	const int32 ComboCount = MontageData->GetComboCount(InActionTag);
	if (CurrentComboIndex >= ComboCount)
	{
		CurrentComboIndex = 0;
	}
	
	// 콤보 카운트에 맞는 몽타주 찾기
	UAnimMontage* Montage = MontageData->FindComboMontage(InActionTag, CurrentComboIndex);
	if (!Montage) return false;

	if (!PlayMontage(Montage)) return false;

	// 현재 액션 정보 업데이트
	CurrentActionTag = InActionTag;
	CurrentActionDuration = Montage->GetPlayLength();
	
	// 다음 콤보 단계로 진행
	CurrentComboIndex++;
	
	return true;
}

void UCBActionComponent::StopMontage(float BlendOutTime)
{
	if (GetCachedAnimInstance(CachedAnimInstance))
	{
		CachedAnimInstance->Montage_Stop(BlendOutTime);
	}
	ResetComboIndex();
}

void UCBActionComponent::ResetComboIndex()
{
	CurrentComboIndex = 0;
}

void UCBActionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCBActionComponent, CurrentComboIndex);
	DOREPLIFETIME(UCBActionComponent, CurrentActionTag);
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
	if (InTag.MatchesTag(CBGameplayTags::Action_Combat_Hit))
		return 100;

	if (InTag.MatchesTag(CBGameplayTags::Action_Combat_Block))
		return 80;

	if (InTag.MatchesTag(CBGameplayTags::Action_Combat_Skill))
		return 60;

	if (InTag.MatchesTag(CBGameplayTags::Action_Combat_Attack))
		return 40;

	return 0;
}

bool UCBActionComponent::PlayMontage(UAnimMontage* InMontage)
{
	if (!InMontage) return false;

	if (!GetCachedAnimInstance(CachedAnimInstance))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] AnimInstance 캐싱 실패"), *GetOwner()->GetName());
		return false;
	}

	CachedAnimInstance.Get()->PlayMontage(InMontage);
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
