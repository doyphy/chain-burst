// project
#include "AI/BehaviorTree/CBBTService_StrafeFocus.h"
#include "CBGameplayTags.h"
#include "CBAbilitySystemLibrary.h"

// engine
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UCBBTService_StrafeFocus::UCBBTService_StrafeFocus()
{
	NodeName = TEXT("Strafe Focus");

	// 노티파이 플래그는 오버라이드한 가상 함수를 보고 결정되므로 파생 클래스에서도 다시 호출해야 함
	INIT_SERVICE_NODE_NOTIFY_FLAGS();

	// 경계 중에는 걷기 속도로 이동 (에디터에서 다른 개이트로 교체 가능)
	SpeedAbilityTag = CBGameplayTags::Ability_Movement_Walk;

	// 포커스 우선순위 설정.
	// 이동 중에도 주시를 유지하려면 PathFollowing 이 거는 Move(1) 포커스보다 높아야 함.
	FocusPriority = EAIFocusPriority::Gameplay;
}

// 브랜치 진입: 포커스(베이스) + 스트레이프 회전 모드
void UCBBTService_StrafeFocus::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	if (bUseStrafeRotation)
	{
		SetStrafeModeEnabled(OwnerComp, true);
	}

	// 이동 속도 어빌리티 활성화
	SetSpeedAbilityEnabled(OwnerComp, true);
}

// 브랜치 이탈: 중단(abort)으로 빠져나온 경우에도 호출되므로 여기서 되돌린다
void UCBBTService_StrafeFocus::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);

	if (bUseStrafeRotation)
	{
		SetStrafeModeEnabled(OwnerComp, false);
	}

	// 이동 속도 어빌리티 비활성화
	SetSpeedAbilityEnabled(OwnerComp, false);
}

// 스트레이프 모드 토글 (회전 모드 + 애님이 읽는 상태 태그를 짝으로 처리)
void UCBBTService_StrafeFocus::SetStrafeModeEnabled(const UBehaviorTreeComponent& OwnerComp, bool bEnabled) const
{
	const AAIController* AIController = OwnerComp.GetAIOwner();
	const ACharacter* ControlledCharacter = AIController ? Cast<ACharacter>(AIController->GetPawn()) : nullptr;
	UCharacterMovementComponent* MovementComp = ControlledCharacter ? ControlledCharacter->GetCharacterMovement() : nullptr;
	if (!MovementComp) return;

	// 상태 태그 추가/제거.
	if (UAbilitySystemComponent* ASC = UCBAbilitySystemLibrary::GetASC(ControlledCharacter))
	{
		if (bEnabled)
		{
			ASC->AddLooseGameplayTag(CBGameplayTags::Status_Movement_Strafe, 1, EGameplayTagReplicationState::TagOnly);
		}
		else
		{
			ASC->RemoveLooseGameplayTag(CBGameplayTags::Status_Movement_Strafe, 1, EGameplayTagReplicationState::TagOnly);
		}
	}

	// bEnabled = true: 이동 방향 따라가지 않고 컨트롤러 회전 따라감 (스트레이프)
	if (bEnabled)
	{
		// bOrientRotationToMovement 만 끄면 아무것도 회전시키지 않음.
		// (CMC의 PhysicsRotation 이 둘 중 하나가 켜져 있을 때만 동작)
		MovementComp->bOrientRotationToMovement = false; // 이동 방향 따라가지 않음.
		MovementComp->bUseControllerDesiredRotation = true; // 컨트롤러의 회전 따라감.
		return;
	}

	// 복원값은 그 캐릭터 클래스의 CDO 값으로 복원.
	const ACharacter* DefaultCharacter = ControlledCharacter->GetClass()->GetDefaultObject<ACharacter>();
	const UCharacterMovementComponent* DefaultMovementComp = DefaultCharacter ? DefaultCharacter->GetCharacterMovement() : nullptr;

	MovementComp->bOrientRotationToMovement = DefaultMovementComp ? DefaultMovementComp->bOrientRotationToMovement : true;
	MovementComp->bUseControllerDesiredRotation = DefaultMovementComp ? DefaultMovementComp->bUseControllerDesiredRotation : false;
}

// 이동 속도 어빌리티 토글 (켜기 = 활성화 / 끄기 = 태그로 취소)
void UCBBTService_StrafeFocus::SetSpeedAbilityEnabled(const UBehaviorTreeComponent& OwnerComp, bool bEnabled) const
{
	if (!SpeedAbilityTag.IsValid()) return;

	const AAIController* AIController = OwnerComp.GetAIOwner();
	UAbilitySystemComponent* ASC = AIController ? UCBAbilitySystemLibrary::GetASC(AIController->GetPawn()) : nullptr;
	if (!ASC) return;

	const FGameplayTagContainer AbilityTagContainer(SpeedAbilityTag);

	if (!bEnabled)
	{
		// 태그로 취소. 어빌리티의 EndAbility 가 속도 GE 를 제거하므로 기본 개이트로 복구됨.
		ASC->CancelAbilities(&AbilityTagContainer);
		return;
	}

	// 어빌리티 순회. 일치하는 모든 어빌리티 배열에 담음.
	TArray<FGameplayAbilitySpecHandle> CandidateHandles;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.Ability->GetAssetTags().HasAll(AbilityTagContainer)) continue;

		// 이미 켜져 있으면 재활성화하지 않음.
		if (Spec.IsActive()) return;

		CandidateHandles.Add(Spec.Handle);
	}

	// 어빌리티 활성화 시도. 여러 개가 있으면 첫 번째 성공한 것만 켬.
	for (const FGameplayAbilitySpecHandle& CandidateHandle : CandidateHandles)
	{
		if (ASC->TryActivateAbility(CandidateHandle))
		{
			return;
		}
	}
}

FString UCBBTService_StrafeFocus::GetStaticDescription() const
{
	const FString KeyText = BlackboardKey.IsSet() ? BlackboardKey.SelectedKeyName.ToString() : TEXT("(키 없음)");
	const FString RotationText = bUseStrafeRotation ? TEXT("주시 + 스트레이프 회전") : TEXT("주시만");

	const FString SpeedText = SpeedAbilityTag.IsValid()
		? FString::Printf(TEXT("\n속도: %s"), *SpeedAbilityTag.ToString())
		: FString();

	return FString::Printf(TEXT("%s\n%s%s"), *KeyText, *RotationText, *SpeedText);
}
