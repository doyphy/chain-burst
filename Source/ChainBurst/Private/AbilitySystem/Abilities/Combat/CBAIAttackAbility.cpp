// project
#include "AbilitySystem/Abilities/Combat/CBAIAttackAbility.h"
#include "AbilitySystem/Abilities/Fragments/CBFragment_WeaponTrace.h"
#include "Components/Animation/CBActionComponent.h"
#include "Controllers/CBAIController.h"

// engine
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

UCBAIAttackAbility::UCBAIAttackAbility()
{
	// AI 컨트롤러(두뇌)가 서버에만 존재하므로 서버에서만 실행한다.
	// 몽타주는 UCBActionAbility 가 GameplayCue 로 전 클라이언트에 동기화하므로 복제 실행이 필요 없음.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// 무기 트레이스 기능 (무기 검사 · 트레이스 · 데미지 GE)
	WeaponTrace = CreateDefaultSubobject<UCBFragment_WeaponTrace>(TEXT("WeaponTrace"));
}

// 발동 전제 조건 (무기가 없으면 활성화 단계에서 막음)
bool UCBAIAttackAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags /* = nullptr */,
	const FGameplayTagContainer* TargetTags /* = nullptr */, FGameplayTagContainer* OptionalRelevantTags /* = nullptr */) const
{
	// 공통 검사(쿨다운·비용·차단 태그 등)를 먼저 통과해야 함.
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 무기 검사 (컴뱃 컴포넌트가 없는 AI 포함)
	return WeaponTrace->CanActivate(ActorInfo);
}

void UCBAIAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 전제 조건은 CanActivateAbility 가 이미 검사했지만, 그 사이 상태가 바뀔 수 있으니 재검사
	if (!WeaponTrace->CanActivate(ActorInfo))
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

	// 무기 트레이스 시작 (서버의 AI 폰은 로컬·서버 모두 참이라 트레이스 구간·히트 대기가 전부 걸림)
	WeaponTrace->Start();
}

void UCBAIAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// 캔슬 등으로 트레이스 종료 노티파이를 못 받았을 수 있으므로 강제 종료
	WeaponTrace->Stop();
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
