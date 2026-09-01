// project
#include "AbilitySystem/Abilities/Combat/CBAIAttackAbility.h"
#include "CBGameplayTags.h"
#include "Components/Combat/CBCombatComponent.h"
#include "Components/Animation/CBActionComponent.h"
#include "CBAbilitySystemLibrary.h"
#include "Controllers/CBAIController.h"

// engine
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

UCBAIAttackAbility::UCBAIAttackAbility()
{
	// AI 컨트롤러(두뇌)가 서버에만 존재하므로 서버에서만 실행한다.
	// 몽타주는 UCBActionAbility 가 GameplayCue 로 전 클라이언트에 동기화하므로 복제 실행이 필요 없음.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

// 발동 전제 조건 (무기가 없으면 활성화 단계에서 막음)
bool UCBAIAttackAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// 공통 검사(쿨다운·비용·차단 태그 등)를 먼저 통과해야 함.
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 무기가 없으면 공격할 수 없음. (컴뱃 컴포넌트가 없는 AI 포함)
	const UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo();
	if (!CombatComp || !CombatComp->HasValidWeapon())
	{
		return false;
	}

	return true;
}

void UCBAIAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 전제 조건은 CanActivateAbility 가 이미 검사했지만, 그 사이 상태가 바뀔 수 있으니 재검사
	const UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo();
	if (!CombatComp || !CombatComp->HasValidWeapon())
	{
		// 여기서 끝나면 bWasCancelled = true (취소 처리)
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 쿨다운 GE 적용 (쿨다운 중 활성화는 CanActivateAbility 가 이미 차단함)
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 몽타주 재생
	PlayActionMontage();

	UAbilityTask_WaitGameplayEvent* WaitTraceStart = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, CBGameplayTags::Event_Combat_TraceStart, nullptr, true);
	WaitTraceStart->EventReceived.AddDynamic(this, &ThisClass::OnTraceStart);
	WaitTraceStart->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* WaitTraceEnd = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, CBGameplayTags::Event_Combat_TraceEnd, nullptr, true);
	WaitTraceEnd->EventReceived.AddDynamic(this, &ThisClass::OnTraceEnd);
	WaitTraceEnd->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* WaitAttackHit = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, CBGameplayTags::Event_Combat_Attack_Hit, nullptr, true);
	WaitAttackHit->EventReceived.AddDynamic(this, &ThisClass::OnAttackHit);
	WaitAttackHit->ReadyForActivation();
}

void UCBAIAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// 캔슬 등으로 트레이스 종료 노티파이를 못 받았을 수 있으므로 강제 종료
	if (UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo())
	{
		CombatComp->StopWeaponTrace();
	}
}

// 이 액션 태그에 등록된 변형 몽타주 중 하나를 무작위 선택 (서버에서 1회 결정 → 큐로 전파)
int32 UCBAIAttackAbility::SelectActionMontageIndex()
{
	if (!bRandomizeMontage)
	{
		return 0;
	}

	UCBActionComponent* ActionComp = GetCBActionComponentFromActorInfo();
	if (!ActionComp)
	{
		return 0;
	}

	const int32 MontageCount = ActionComp->GetMontageCount(BoundActionTag);
	return MontageCount > 1 ? FMath::RandRange(0, MontageCount - 1) : 0;
}

// 모션 워핑 타겟을 큐 파라미터에 실음.
void UCBAIAttackAbility::BuildActionCueParameters(FGameplayCueParameters& CueParams)
{
	if (!bWarpToTarget) return;

	// 블랙보드 가져오기.
	const APawn* Avatar = Cast<APawn>(GetAvatarActorFromActorInfo());
	const AAIController* AIController = Avatar ? Cast<AAIController>(Avatar->GetController()) : nullptr;
	const UBlackboardComponent* Blackboard = AIController ? AIController->GetBlackboardComponent() : nullptr;
	if (!Blackboard) return;

	// 블랙보드에서 타겟 액터 가져오기.
	const AActor* TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(ACBAIController::TargetActorKey));
	if (!TargetActor) return;

	// 큐 파라미터로 타겟 컴포넌트와 접근 거리 전달.
	CueParams.TargetAttachComponent = TargetActor->GetRootComponent();
	CueParams.NormalizedMagnitude = WarpStopDistance;
}

void UCBAIAttackAbility::OnTraceStart(FGameplayEventData Payload)
{
	if (UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo())
	{
		CombatComp->StartWeaponTrace();
	}
}

void UCBAIAttackAbility::OnTraceEnd(FGameplayEventData Payload)
{
	if (UCBCombatComponent* CombatComp = GetCBCombatComponentFromActorInfo())
	{
		CombatComp->StopWeaponTrace();
	}
}

// 히트 이벤트 수신 → 타겟마다 데미지 GE 적용
void UCBAIAttackAbility::OnAttackHit(FGameplayEventData Payload)
{
	// GE 클래스 유효성 검사
	if (!DamageEffectClass) return;

	// 타겟 데이터 순회
	for (int32 i = 0; i < Payload.TargetData.Num(); i++)
	{
		// 타겟 데이터 가져오기
		const FGameplayAbilityTargetData* TargetData = Payload.TargetData.Get(i);
		if (!TargetData) continue;

		// HitResult 가져오기
		const FHitResult* HitResult = TargetData->GetHitResult();
		if (!HitResult) continue;

		// 타겟 Actor 가져오기
		AActor* HitActor = HitResult->GetActor();
		if (!HitActor) continue;

		// 타겟 Actor 의 ASC 가져오기
		UAbilitySystemComponent* TargetASC = UCBAbilitySystemLibrary::GetASC(HitActor);
		if (!TargetASC) continue;

		// Context 만들기
		FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponentFromActorInfo()->MakeEffectContext();
		ContextHandle.AddHitResult(*HitResult);

		// Spec 만들기
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageEffectClass, GetAbilityLevel());

		// 데미지 계수 설정 (SetByCaller 등록)
		SpecHandle.Data->SetSetByCallerMagnitude(CBGameplayTags::Data_Damage_Coefficient, DamageCoefficient);

		// GE 적용
		ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, Payload.TargetData);
	}
}
