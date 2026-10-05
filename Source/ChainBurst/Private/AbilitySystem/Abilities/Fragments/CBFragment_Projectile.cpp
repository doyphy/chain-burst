// project
#include "AbilitySystem/Abilities/Fragments/CBFragment_Projectile.h"
#include "Items/Projectiles/CBProjectile.h"
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"

// engine
#include "Abilities/GameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"

void UCBFragment_Projectile::Start(AActor* InTargetActor)
{
	// 조준 대상 기록. 위치는 발사할 때마다 다시 읽고, 여기서 읽은 값은 대상이 사라졌을 때의 대체값.
	bHasAim = InTargetActor != nullptr;
	AimTarget = InTargetActor;
	AimLocation = bHasAim ? InTargetActor->GetActorLocation() : FVector::ZeroVector;

	UGameplayAbility* Ability = GetOwningAbility();
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo) return;

	// [서버] 스폰은 서버에서만. (노티파이가 로컬에서 보낸 이벤트는 서버로도 전달됨)
	if (ActorInfo->IsNetAuthority())
	{
		UAbilityTask_WaitGameplayEvent* WaitFire = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			Ability, CBGameplayTags::Event_Combat_FireProjectile, nullptr, false);
		WaitFire->EventReceived.AddDynamic(this, &ThisClass::OnFireProjectile);
		WaitFire->ReadyForActivation();
	}
}

UGameplayAbility* UCBFragment_Projectile::GetOwningAbility() const
{
	// 호스트 어빌리티의 기본 서브오브젝트라 Outer 가 곧 소유 어빌리티
	return CastChecked<UGameplayAbility>(GetOuter());
}

void UCBFragment_Projectile::OnFireProjectile(FGameplayEventData Payload)
{
	UGameplayAbility* Ability = GetOwningAbility();

	// 노티파이는 있는데 투사체·데미지 GE 를 안 넣은 설정 실수. 조용히 빗나가지 않게 경고.
	if (!ProjectileClass || !DamageEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Projectile 의 ProjectileClass 또는 DamageEffectClass 미지정 - 발사하지 않음"), *Ability->GetName());
		return;
	}

	// 조준할 타겟 없이 발동된 경우 (BT 는 타겟이 있을 때만 공격하므로 정상 경로에서는 오지 않음)
	if (!bHasAim)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 타겟 없이 발동됨 - 투사체를 발사하지 않음"), *Ability->GetName());
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;
	if (!World) return;

	// 권한·예측키 검사 - 서버이거나 예측 구간 안일 때만 허용
	const FGameplayAbilityActivationInfo ActivationInfo = Ability->GetCurrentActivationInfo();
	if (!Ability->HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo)) return;

	// 매 발 다시 조준. 연사 중 타겟이 움직이면 따라감. (타겟이 사라졌으면 마지막으로 조준한 위치)
	if (const AActor* Target = AimTarget.Get())
	{
		AimLocation = Target->GetActorLocation();
	}

	// 발사 순간의 공격력으로 스펙 1회 생성. 모든 발이 공유하고, 적용할 때마다 엔진이 복사함.
	const FGameplayEffectSpecHandle DamageSpec = UCBAbilitySystemLibrary::MakeDamageSpec(*Ability, DamageEffectClass, DamageCoefficient);

	const FVector SpawnLocation = GetSpawnLocation(*Avatar);
	const FVector ToAim = AimLocation - SpawnLocation;

	// 부채꼴 분할. 가운데 발이 조준점을 향하도록 좌우 대칭으로 펼침.
	// 360 이면 처음과 끝 발이 같은 방향에 겹치므로 발 수로 나눔.
	float StepAngle = 0.f;
	if (ProjectileCount > 1)
	{
		StepAngle = SpreadAngle >= 360.f ? 360.f / ProjectileCount : SpreadAngle / (ProjectileCount - 1);
	}
	const float StartAngle = -StepAngle * (ProjectileCount - 1) * 0.5f;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Avatar;
	SpawnParams.Instigator = Cast<APawn>(Avatar); // 투사체가 시전자를 판정에서 빼고 진영을 물을 때 씀
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* HomingTarget = AimTarget.Get();
	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		// 조준점을 발사 지점 기준으로 수평 회전 (직선 = 방향이, 포물선 = 착탄점이 돎)
		const FVector ShotAim = SpawnLocation + ToAim.RotateAngleAxis(StartAngle + StepAngle * Index, FVector::UpVector);
		const FTransform SpawnTransform((ShotAim - SpawnLocation).Rotation(), SpawnLocation);

		ACBProjectile* Projectile = World->SpawnActor<ACBProjectile>(ProjectileClass, SpawnTransform, SpawnParams);
		if (!Projectile) continue;

		// 스폰(컴포넌트 초기화)이 끝난 뒤 발사해야 속도가 덮어써지지 않음
		Projectile->Auth_Launch(ShotAim, HomingTarget, DamageSpec, HitCueTag);
	}
}

// 발사 위치 (소켓 → 없으면 시전자 중심)
FVector UCBFragment_Projectile::GetSpawnLocation(const AActor& InAvatar) const
{
	if (SpawnSocketName.IsNone()) return InAvatar.GetActorLocation();

	// 소켓·본 이름 모두 허용. 못 찾으면 엉뚱한 곳에서 나가지 않게 경고 후 시전자 중심.
	FVector SocketLocation;
	if (UCBAbilitySystemLibrary::FindMeshSocketLocation(&InAvatar, SpawnSocketName, SocketLocation))
	{
		return SocketLocation;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] 발사 소켓 '%s' 를 찾지 못함 - 시전자 중심에서 발사"), *GetOwningAbility()->GetName(), *SpawnSocketName.ToString());
	return InAvatar.GetActorLocation();
}
