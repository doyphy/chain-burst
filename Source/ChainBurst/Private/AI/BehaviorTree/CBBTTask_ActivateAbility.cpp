// project
#include "AI/BehaviorTree/CBBTTask_ActivateAbility.h"
#include "CBAbilitySystemLibrary.h"

// engine
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"

UCBBTTask_ActivateAbility::UCBBTTask_ActivateAbility()
{
	NodeName = TEXT("Activate Ability");
}

EBTNodeResult::Type UCBBTTask_ActivateAbility::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 이미 재생 중이면 다시 활성화할 필요 없음.
	if (const UAbilitySystemComponent* ASC = GetPawnASC(OwnerComp))
	{
		if (FindActiveAbilityHandle(*ASC).IsValid())
		{
			return EBTNodeResult::Succeeded;
		}
	}

	UAbilitySystemComponent* ActivationASC = nullptr;
	FGameplayAbilitySpecHandle ActivatedHandle;

	// 활성화 성공 여부만 보고 끝낸다 (종료는 기다리지 않음)
	return TryActivateAbilityByTag(OwnerComp, ActivationASC, ActivatedHandle)
		? EBTNodeResult::Succeeded
		: EBTNodeResult::Failed;
}

// 빙의된 폰의 ASC 에서 AbilityTag 로 어빌리티를 활성화
bool UCBBTTask_ActivateAbility::TryActivateAbilityByTag(const UBehaviorTreeComponent& OwnerComp, UAbilitySystemComponent*& OutASC, FGameplayAbilitySpecHandle& OutHandle) const
{
	// 어빌리티 태그가 유효한지 검사
	if (!AbilityTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 활성화할 어빌리티 태그가 지정되지 않음."), *GetName());
		return false;
	}

	// 빙의된 폰의 ASC 조회 (AI는 캐릭터가 ASC를 소유)
	UAbilitySystemComponent* ASC = GetPawnASC(OwnerComp);
	if (!ASC)
	{
		return false;
	}

	// 태그로 활성화 가능한 스펙을 조회.
	// 엔진의 TryActivateAbilitiesByTag 를 쓰지 않는 이유 두 가지:
	// 1. TryActivateAbilitiesByTag는 상위 태그를 지정하면 하위 어빌리티가 모두 활성화되기 때문에 쓰지않고, 직접 순회해서 하나만 활성화하고 멈추기 위함.
	// 2. TryActivateAbilitiesByTag는 성공 여부인 bool 값만 반환하기 때문에 쓰지않고, 직접 순회해서 어떤 스펙이 활성화됐는지 핸들을 따로 저장하기 위함. (파생클래스(대기형)에서 핸들을 사용함) 
	const FGameplayTagContainer AbilityTagContainer(AbilityTag);
	TArray<FGameplayAbilitySpec*> MatchingSpecs;
	ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(AbilityTagContainer, MatchingSpecs);

	// 반환된 포인터는 ASC 내부 배열(ActivatableAbilities.Items)을 직접 가리킴.
	// 활성화 도중 어빌리티가 부여·제거되면 그 배열이 재할당되어 포인터(MatchingSpecs)가 무효해지므로 복사해서 사용.
	TArray<FGameplayAbilitySpecHandle> CandidateHandles;
	CandidateHandles.Reserve(MatchingSpecs.Num()); // 공간 할당
	// CandidateHandles 배열에 MatchingSpecs 배열 복사
	for (const FGameplayAbilitySpec* Spec : MatchingSpecs)
	{
		if (Spec)
		{
			CandidateHandles.Add(Spec->Handle);
		}
	}

	// 어빌리티 배열 순회
	for (const FGameplayAbilitySpecHandle& CandidateHandle : CandidateHandles)
	{
		// 활성화하고 상태 저장하고 멈춤.
		if (ASC->TryActivateAbility(CandidateHandle))
		{
			OutASC = ASC;
			OutHandle = CandidateHandle;
			return true;
		}
	}

	// 쿨다운·차단 태그·조건 미충족·이미 활성 중 등으로 실패
	return false;
}

// 빙의된 폰의 ASC 조회 (AI는 캐릭터가 ASC를 소유)
UAbilitySystemComponent* UCBBTTask_ActivateAbility::GetPawnASC(const UBehaviorTreeComponent& OwnerComp) const
{
	const AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* ControlledPawn = AIController ? AIController->GetPawn() : nullptr;

	return UCBAbilitySystemLibrary::GetASC(ControlledPawn);
}

// AbilityTag 어빌리티가 이미 활성 중이면 핸들을 반환 (없으면 무효 핸들)
FGameplayAbilitySpecHandle UCBBTTask_ActivateAbility::FindActiveAbilityHandle(const UAbilitySystemComponent& InASC) const
{
	if (!AbilityTag.IsValid())
	{
		return FGameplayAbilitySpecHandle();
	}

	// 현재 활성화중인 어빌리티 순회
	const FGameplayTagContainer AbilityTagContainer(AbilityTag);
	for (const FGameplayAbilitySpec& Spec : InASC.GetActivatableAbilities())
	{
		// 이미 활성화중이면 스펙 반환
		if (Spec.Ability && Spec.Ability->GetAssetTags().HasAll(AbilityTagContainer) && Spec.IsActive())
		{
			return Spec.Handle;
		}
	}

	return FGameplayAbilitySpecHandle();
}

FString UCBBTTask_ActivateAbility::GetStaticDescription() const
{
	const FString TagText = AbilityTag.IsValid() ? AbilityTag.ToString() : TEXT("(태그 없음)");

	return FString::Printf(TEXT("%s\n활성화 후 즉시 반환"), *TagText);
}
