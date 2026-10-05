// project
#include "Controllers/CBOutlawController.h"

// engine
#include "Engine/World.h"

// 현재 타겟 유지 판정. 보스는 인지를 잃어도 타겟을 놓지 않음 (사망·파괴·진영 변화만 해제).
bool ACBOutlawController::IsTargetStillValid(AActor* InActor) const
{
	// 적 판정 + 생존만 확인
	return IsValidTarget(InActor) && IsTargetAlive(InActor);
}

// 집중이 끝났을 때만 다시 고름
void ACBOutlawController::ReevaluateTarget(AActor* InCurrentTarget)
{
	const UWorld* World = GetWorld();
	if (!World) return;

	// 현재 타겟을 잡은(또는 다시 집중한) 뒤 지난 시간
	const float FocusElapsed = World->GetTimeSeconds() - GetTargetAssignedTime();

	// 최대 시간 도달 (0 이하면 순환 없음)
	const bool bMaxFocusExpired = MaxFocusTime > 0.f && FocusElapsed >= MaxFocusTime;

	// 조용함 - 잡은 직후 유예(RecentDamageMemoryTime)가 지났는데 그사이 나를 때리지 않음 (도망·회피)
	const bool bTargetSilent = FocusElapsed >= RecentDamageMemoryTime && !HasRecentlyDamagedMe(InCurrentTarget);

	// 둘 다 아니면 집중 유지
	if (!bMaxFocusExpired && !bTargetSilent) return;

	// 최대 시간이면 현재 타겟을 빼고 골라 다른 사람으로 순환.
	// 조용해서 끝났으면 현재 타겟도 후보에 넣음 - 여전히 가장 가깝다면(근처에서 회피 중) 그대로 다시 집중.
	AActor* NextTarget = FindBestTarget(bMaxFocusExpired ? InCurrentTarget : nullptr);

	// 다른 후보가 없으면(솔로·혼자만 인지) 같은 대상으로 다시 집중. 같은 대상을 다시 써도 지정 시각이 갱신됨
	UpdateTargetInBlackboard(NextTarget ? NextTarget : InCurrentTarget);
}
