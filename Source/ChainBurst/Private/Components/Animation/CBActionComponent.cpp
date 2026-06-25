// project
#include "Components/Animation/CBActionComponent.h"
#include "CBGameplayTags.h"
#include "DataAssets/Animation/CBActionMontageData.h"
#include "AnimInstances/CBCharacterAnimInstance.h"
#include "Characters/CBBaseCharacter.h"

// engine
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UCBActionComponent::UCBActionComponent()
{
	SetIsReplicatedByDefault(true);
}

bool UCBActionComponent::RequestPlaySingleMontage(const FGameplayTag& InActionTag)
{
	// 태그 유효성 검사
	if (!InActionTag.IsValid()) return false;

	// 몽타주 데이터 에셋 유효성 검사
	if (!MontageData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] MontageData 가 없음"), *GetOwner()->GetName());
		return false;
	}

	// 싱글 몽타주 가져오기
	UAnimMontage* Montage = MontageData->FindSingleMontage(InActionTag);
	if (!Montage) return false;

	// 현재 액션 정보 업데이트
	CurrentActionTag = InActionTag;
	CurrentActionDuration = Montage->GetPlayLength();

	// 콤보 초기화
	ResetComboIndex();

	// 몽타주 재생
	return PlayMontage(Montage);
}

bool UCBActionComponent::RequestPlayComboMontage(const FGameplayTag& InActionTag)
{
	// 태그 유효성 검사
	if (!InActionTag.IsValid()) return false;

	// 몽타주 데이터 에셋 유효성 검사
	if (!MontageData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] MontageData 가 없음"), *GetOwner()->GetName());
		return false;
	}

	// 다른 액션 태그로 전환 시 콤보 초기화
	if (CurrentActionTag != InActionTag)
	{
		// 콤보 초기화 타이머 취소
		CancelComboResetTimer();
		// 콤보 초기화
		ResetComboIndex();
	}

	// 인덱스가 최대 콤보 수를 초과하면 콤보 초기화
	const int32 ComboCount = MontageData->GetComboCount(InActionTag);
	if (CurrentComboIndex >= ComboCount)
	{
		ResetComboIndex();
	}
	
	// 콤보 카운트에 맞는 몽타주 찾기
	UAnimMontage* Montage = MontageData->FindComboMontage(InActionTag, CurrentComboIndex);
	if (!Montage) return false;

	// 몽타주 재생
	if (!PlayMontage(Montage)) return false;

	// 현재 액션 정보 업데이트
	CurrentActionTag = InActionTag;
	CurrentActionDuration = Montage->GetPlayLength();

	// 콤보 초기화 타이머 설정
	StartComboResetTimer(CurrentActionDuration);
	
	// 다음 콤보 단계로 진행
	CurrentComboIndex++;
	
	return true;
}

void UCBActionComponent::StopMontage(float BlendOutTime /* = 0.25 */, bool IsResetCombo /* = false */)
{
	if (GetCachedAnimInstance(CachedAnimInstance))
	{
		// 현재 재생중인 몽타주 중지
		CachedAnimInstance->Montage_Stop(BlendOutTime);
	}
	if (IsResetCombo)
	{
		// 콤보 초기화 타이머 취소
		CancelComboResetTimer();
		// 콤보 초기화
		ResetComboIndex();
	}
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

void UCBActionComponent::StartComboResetTimer(float MontageDuration)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 만약 이미 돌고 있는 콤보 타이머가 있다면 즉시 취소
	CancelComboResetTimer();

	// 타이머 설정
	World->GetTimerManager().SetTimer(
		ComboResetTimerHandle,		// 타이머 핸들
		this,							// 함수가 속한 객체
		&UCBActionComponent::OnComboTimeout, // 실행할 콜백 함수
		MontageDuration,				// 대기 시간 (초)
		false							// 반복 실행 여부 (false = 1번만 실행)
	);
}

void UCBActionComponent::CancelComboResetTimer()
{
	UWorld* World = GetWorld();
	if (World && ComboResetTimerHandle.IsValid())
	{
		// 타이머 강제 종료
		World->GetTimerManager().ClearTimer(ComboResetTimerHandle);
	}
}

void UCBActionComponent::ResetComboIndex()
{
	CurrentComboIndex = 0;
}

void UCBActionComponent::OnComboTimeout()
{
	ResetComboIndex();
}

