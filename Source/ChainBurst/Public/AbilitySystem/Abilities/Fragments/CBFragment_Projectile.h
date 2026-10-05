#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "CBFragment_Projectile.generated.h"

class ACBProjectile;
class UGameplayAbility;
class UGameplayEffect;

/**
 * 투사체 발사 기능 (투사체 조각).
 * 투사체를 몇 발, 어디서, 어느 쪽으로 쏠지와 명중 데미지를 정하는 기능 객체. 날아가는 방식은 투사체 BP(ACBProjectile)가 정함.
 * - 조준     : 발사 이벤트마다 타겟의 그 순간 위치를 다시 조준 (연사 중 움직이는 타겟을 따라감). 타겟이 사라지면 마지막 조준 위치
 * - 발사 시점 : 몽타주 노티파이가 보내는 발사 이벤트(Event.Combat.FireProjectile). 노티파이가 여러 개면 매번 발사 (연사)
 * - 발사     : [서버] 데미지 스펙 1회 생성 → 소켓 위치에서 ProjectileCount 발을 SpreadAngle 부채꼴로 스폰·발사
 * 발사 후 명중 판정·데미지 적용은 투사체 클래스에서 처리 (발사 이후의 일은 투사체에서 담당) 
 * 서버에서 스폰하므로 서버가 곧 로컬인 AI 공격용. (플레이어가 쓰려면 로컬 예측 발사 경로가 따로 필요)
 */
UCLASS()
class CHAINBURST_API UCBFragment_Projectile : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 공격 시작. 조준 대상 기록 + [서버] 발사 이벤트 대기.
	 * @param InTargetActor 조준할 타겟 (발사마다 위치를 다시 읽고, 유도 투사체는 이 대상을 쫓음). 없으면 발사 이벤트가 와도 쏘지 않음
	 */
	void Start(AActor* InTargetActor);

protected:
	/** 발사할 투사체 BP. 비어 있는데 노티파이가 오면 경고 후 발사 생략 */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	TSubclassOf<ACBProjectile> ProjectileClass;

	/**
	 * 투사체가 나갈 시전자 메시의 소켓(또는 본) 이름. None 이면 시전자 중심.
	 * 이름을 못 찾으면 경고 후 시전자 중심에서 발사.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	FName SpawnSocketName = NAME_None;

	/** 한 번의 발사 이벤트에 동시에 쏘는 투사체 수 */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile", meta = (ClampMin = "1"))
	int32 ProjectileCount = 1;

	/**
	 * 여러 발을 펼칠 부채꼴 전체 각도 (도). 가운데 발이 조준점을 향하고 좌우로 균등 분배.
	 * 직선은 방향이, 포물선은 착탄점이 발사 지점을 축으로 돎. 360 이면 전방위로 균등 분배.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile", meta = (EditCondition = "ProjectileCount > 1", ClampMin = "0.0", ClampMax = "360.0"))
	float SpreadAngle = 30.f;

	/** 명중한 적에게 적용할 데미지 GE 클래스. 비어 있는데 노티파이가 오면 경고 후 발사 생략 */
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 데미지 계수 (FinalDamage = AttackPower * DamageCoefficient - DefensePower). 공격력은 발사 순간 값으로 고정 */
	UPROPERTY(EditDefaultsOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float DamageCoefficient = 1.f;

	/**
	 * 피격자에게 실행할 피격 연출 큐. 비우면 연출 없이 데미지만 들어감.
	 * 투사체는 무기가 아니므로 무기의 피격 큐를 쓰지 않고 따로 지정.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Damage", meta = (Categories = "GameplayCue"))
	FGameplayTag HitCueTag;

private:
	/** 이 기능을 소유한 어빌리티 (Outer) */
	UGameplayAbility* GetOwningAbility() const;

	/** [서버] 발사 이벤트 수신 → 투사체 스폰·발사 */
	UFUNCTION()
	void OnFireProjectile(FGameplayEventData Payload);

	/**
	 * 발사 위치 (소켓 → 없으면 시전자 중심).
	 * @param InAvatar 시전자
	 * @return 발사 위치
	 */
	FVector GetSpawnLocation(const AActor& InAvatar) const;

	/** 마지막 조준점. 발사마다 타겟 위치로 갱신하고, 타겟이 사라지면 이 위치로 계속 쏨 */
	FVector AimLocation = FVector::ZeroVector;

	/** Start 에서 받은 타겟 (유도 대상). 약참조라 타겟이 사라지면 유도 없이 날아감 */
	TWeakObjectPtr<AActor> AimTarget;

	/** 이번 발동에 조준할 타겟이 있었는지 */
	bool bHasAim = false;
};
