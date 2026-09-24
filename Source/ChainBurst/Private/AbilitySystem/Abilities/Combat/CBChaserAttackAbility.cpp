// project
#include "AbilitySystem/Abilities/Combat/CBChaserAttackAbility.h"
#include "AbilitySystem/Abilities/Fragments/CBFragment_WeaponTrace.h"
#include "Components/Combat/CBCombatComponent.h"
#include "Components/Animation/CBActionComponent.h"
#include "Characters/CBChaserCharacter.h"
#include "Components/Movement/CBCharacterRotationComponent.h"

// engine
#include "GameplayPrediction.h"

UCBChaserAttackAbility::UCBChaserAttackAbility()
{
	// 로컬 입력으로 발동되어 즉시 반응해야 하므로 예측 실행
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 무기 트레이스 기능 (무기 검사 · 트레이스 · 데미지 GE)
	WeaponTrace = CreateDefaultSubobject<UCBFragment_WeaponTrace>(TEXT("WeaponTrace"));
}

// 발동 전제 조건 (무기가 없으면 활성화 단계에서 막음)
bool UCBChaserAttackAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags /* = nullptr */,
	const FGameplayTagContainer* TargetTags /* = nullptr */, FGameplayTagContainer* OptionalRelevantTags /* = nullptr */) const
{
	// 공통 검사(쿨다운·비용·차단 태그·사망 등)를 먼저 통과해야 함.
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 무기 검사
	return WeaponTrace->CanActivate(ActorInfo);
}

void UCBChaserAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                             const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                             const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 컴뱃 컴포넌트 가져오기 (베이스 UCBCombatComponent 멤버만 사용하므로 캐스팅 불필요)
	// 무기 유효성은 CanActivateAbility 단계에서 이미 검사함
	UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo();
	if (!CombatComp)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 비전투 상태인지 확인 (비전투 상태면 공격 못함)
	if (CombatComp->IsCombatMode() == false)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 몽타주 재생 가능한지 확인
	if (bCanPlayMontage == false)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 쿨다운 GE 적용 (쿨다운 중 재활성화는 CanActivateAbility가 차단)
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// 조준 방향으로 즉시 정렬
	// 쿨다운 이후 - 몽타주 재생 사이에 호출해야 함. (쿨다운 중에 활성화하면 회전하지 않게)
	if (bAlignToAimOnActivate)
	{
		if (const ACBChaserCharacter* Chaser = Cast<ACBChaserCharacter>(GetAvatarActorFromActorInfo()))
		{
			if (UCBCharacterRotationComponent* RotationComp = Chaser->GetCharacterRotationComponent())
			{
				RotationComp->AlignFacingToControlRotation();
			}
		}
	}

	// 몽타주 재생
	PlayActionMontage();

	// 무기 트레이스 시작 ([로컬] 트레이스 구간 대기 / [서버] 히트 대기 → 데미지 GE)
	WeaponTrace->Start();
}

void UCBChaserAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// 혹시 트레이스가 아직 활성화 중이라면 강제 종료
	WeaponTrace->Stop();
}

// 콤보 액션이면 CombatComponent의 콤보 인덱스를 전진시켜 재생할 인덱스를 가져옴.
int32 UCBChaserAttackAbility::SelectActionMontageIndex()
{
	if (!IsCombo)
	{
		return 0;
	}

	UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo();
	if (!CombatComp)
	{
		return 0;
	}

	// 최대 콤보 수 = 이 액션 태그에 등록된 몽타주 개수
	int32 MaxComboCount = 0;
	if (UCBActionComponent* ActionComp = GetCBActionComponentFromActorInfo())
	{
		MaxComboCount = ActionComp->GetMontageCount(BoundActionTag);
	}

	// 예측 키를 함께 넘겨, 거부됐을 때 정확히 이 전진만 되돌릴 수 있게 한다.
	FPredictionKey ActivationKey = CurrentActivationInfo.GetActivationPredictionKey();
	const int32 KeyValue = ActivationKey.Current;
	const int32 PlayIndex = CombatComp->AdvanceCombo(BoundActionTag, MaxComboCount, KeyValue);

	// 만약 현재 어빌리티의 예측키가 서버에서 활성화 거부되면 콤보를 되돌림.
	// 서버는 등록하지 않음.
	if (!HasAuthority(&CurrentActivationInfo))
	{
		// 현재 어빌리티가 서버에서 거부될 경우 델리게이트 등록
		TWeakObjectPtr<UCBCombatComponent> WeakCombat(CombatComp);
		ActivationKey.NewRejectedDelegate().BindWeakLambda(this, [WeakCombat, KeyValue]()
		{
			// 이전 콤보로 돌아가기
			if (UCBCombatComponent* Comp = WeakCombat.Get())
			{
				Comp->RollbackCombo(KeyValue);
			}
		});
	}

	return PlayIndex;
}

// 액션이 끝났을 때(정상·폴백·캔슬) 콤보 체인을 종료한다.
void UCBChaserAttackAbility::CleanupActionState()
{
	if (!IsCombo)
	{
		return;
	}

	if (UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo())
	{
		CombatComp->ResetCombo();
	}
}
