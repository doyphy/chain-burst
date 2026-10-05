// project
#include "AbilitySystem/Abilities/Fragments/CBFragment_AreaAttack.h"
#include "Types/CBCollisionChannels.h"
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"

// engine
#include "Abilities/GameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GenericTeamAgentInterface.h"

namespace
{
	// 디버그 표시 유지 시간(초). 판정은 한 순간이라 눈으로 확인할 만큼만 남김.
	constexpr float AreaAttackDebugDrawDuration = 2.f;
}

void UCBFragment_AreaAttack::Start()
{
	UGameplayAbility* Ability = GetOwningAbility();
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo) return;

	// [서버] 판정은 서버에서만. (노티파이가 로컬에서 보낸 이벤트는 서버로도 전달됨)
	if (ActorInfo->IsNetAuthority())
	{
		UAbilityTask_WaitGameplayEvent* WaitAreaAttack = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			Ability, CBGameplayTags::Event_Combat_AreaAttack, nullptr, false);
		WaitAreaAttack->EventReceived.AddDynamic(this, &ThisClass::OnAreaAttack);
		WaitAreaAttack->ReadyForActivation();
	}
}

UGameplayAbility* UCBFragment_AreaAttack::GetOwningAbility() const
{
	// 호스트 어빌리티의 기본 서브오브젝트라 Outer 가 곧 소유 어빌리티
	return CastChecked<UGameplayAbility>(GetOuter());
}

void UCBFragment_AreaAttack::OnAreaAttack(FGameplayEventData Payload)
{
	UGameplayAbility* Ability = GetOwningAbility();

	// 노티파이는 있는데 데미지 GE 를 안 넣은 설정 실수. 조용히 빗나가지 않게 경고.
	if (!DamageEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] AreaAttack 의 DamageEffectClass 미지정 - 영역 공격 데미지가 적용되지 않음"), *Ability->GetName());
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World) return;

	// 권한·예측키 검사 - 서버이거나 예측 구간 안일 때만 적용 허용
	const FGameplayAbilityActivationInfo ActivationInfo = Ability->GetCurrentActivationInfo();
	if (!Ability->HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo)) return;

	// 판정 중심과 박스 회전. 오프셋·박스 모두 시전자 기준이라 몸이 향한 방향을 따라 돎.
	const FQuat Rotation = Avatar->GetActorQuat();
	const FVector Center = Avatar->GetActorLocation() + Rotation.RotateVector(CenterOffset);

	// 범위 안 컴포넌트 수집. Weapon 채널이라 캐릭터는 캡슐이 아니라 메시(피직스 애셋 바디)로 걸림.
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CBAreaAttack), false, Avatar);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, Center, Rotation, CBCollisionChannels::Weapon,
		FCollisionShape::MakeBox(BoxExtent), QueryParams);

#if ENABLE_DRAW_DEBUG
	// [디버그] 판정 박스 (서버 월드에 그리므로 리슨 서버 호스트 화면에만 보임)
	if (bDrawDebug)
	{
		DrawDebugBox(World, Center, BoxExtent, Rotation, FColor::Orange, false, AreaAttackDebugDrawDuration, 0, 2.f);
	}
#endif

	// 벽 차단 검사 시작점 (판정 중심 또는 시전자 소켓)
	const FVector WallCheckOrigin = GetWallCheckOrigin(*Avatar, Center);

	// 스켈레탈 메시는 바디마다 결과가 따로 나오므로 액터 단위로 한 번만 판정
	TSet<AActor*> CheckedActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* TargetActor = Overlap.GetActor();
		if (!TargetActor) continue;

		bool bAlreadyChecked = false;
		CheckedActors.Add(TargetActor, &bAlreadyChecked);
		if (bAlreadyChecked) continue;

		// 적대 진영만 (무기 트레이스와 같은 전역 attitude solver 기준)
		if (FGenericTeamId::GetAttitude(Avatar, TargetActor) != ETeamAttitude::Hostile) continue;

		// ASC 가 없는 액터는 데미지 대상이 아님
		if (!UCBAbilitySystemLibrary::GetASC(TargetActor)) continue;

		// 벽 차단 검사 (무기 트레이스와 같은 기준. 시전자는 쿼리에서 제외되어 소켓이 몸 안쪽이어도 자기 몸에 막히지 않음)
		const FVector TargetLocation = TargetActor->GetActorLocation();
		const bool bBlockedByWall = UCBAbilitySystemLibrary::IsBlockedByWall(*World, WallCheckOrigin, TargetLocation, QueryParams);

#if ENABLE_DRAW_DEBUG
		// [디버그] 벽 차단 검사 선 - 실제 검사 시작점에서 그림 (초록 = 적중 / 빨강 = 벽에 막힘)
		if (bDrawDebug)
		{
			DrawDebugLine(World, WallCheckOrigin, TargetLocation, bBlockedByWall ? FColor::Red : FColor::Green, false, AreaAttackDebugDrawDuration, 0, 2.f);
		}
#endif

		// 벽 너머 대상 제외
		if (bBlockedByWall) continue;

		// 오버랩 결과엔 타격 지점이 없어 HitResult 를 직접 구성. (타격 지점 = 대상 위치, 법선 = 중심에서 바깥쪽)
		const FHitResult HitResult(TargetActor, Overlap.GetComponent(), TargetLocation, (TargetLocation - Center).GetSafeNormal());

		// 대상 하나씩 데미지 GE 적용 + 피격 연출
		UCBAbilitySystemLibrary::Auth_ApplyDamageToTarget(*Ability, DamageEffectClass, DamageCoefficient, HitResult);
		UCBAbilitySystemLibrary::Auth_ExecuteHitCue(TargetActor, Avatar, HitCueTag, HitResult);
	}
}

// 벽 차단 검사 시작점 (소켓 → 없거나 못 찾으면 판정 중심)
FVector UCBFragment_AreaAttack::GetWallCheckOrigin(const AActor& InAvatar, const FVector& InCenter) const
{
	if (WallCheckSocketName.IsNone()) return InCenter;

	FVector SocketLocation;
	if (UCBAbilitySystemLibrary::FindMeshSocketLocation(&InAvatar, WallCheckSocketName, SocketLocation))
	{
		return SocketLocation;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] 벽 검사 소켓 '%s' 를 찾지 못함 - 판정 중심에서 검사"), *GetOwningAbility()->GetName(), *WallCheckSocketName.ToString());
	return InCenter;
}
