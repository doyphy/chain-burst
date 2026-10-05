#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Types/CBEnumTypes.h"
#include "CBProjectile.generated.h"

class UProjectileMovementComponent;

/**
 * 투사체 베이스. 기능 조각(UCBFragment_Projectile)이 서버에서 스폰해 발사함.
 * - 모양 : BP 에서 루트를 Sphere/Box/Capsule 컴포넌트로 교체하면 그 모양·크기 그대로 판정 (루트가 모양이 아니면 경고 후 선으로 판정)
 * - 연출 : BP 에서 Niagara·메시 등을 루트 아래에 부착. 모든 컴포넌트의 콜리전은 런타임에 꺼짐
 * - 비행 : 발사 방식(직선/포물선) + ProjectileMovement 설정(속도·중력·유도). 발사 조건만 복제하고 각 머신이 시뮬레이션, 이동 복제로 보정
 * - 명중 : [서버] 매 틱 지난 위치 → 현재 위치를 Weapon 채널로 스윕. 첫 적대 대상에게 데미지 GE·피격 큐 후 소멸, 벽·지면에 닿으면 소멸
 * 데미지 스펙은 발사 시점에 어빌리티가 만들어 넘기므로, 명중 시점에 어빌리티가 끝났어도 적용됨.
 */
UCLASS(Abstract)
class CHAINBURST_API ACBProjectile : public AActor
{
	GENERATED_BODY()

public:
	ACBProjectile();

	/**
	 * [서버] 발사. 스폰 직후 1회 호출.
	 * 속도는 컴포넌트 초기화가 끝난 뒤에 넣어야 함 (초기화가 속도 크기를 InitialSpeed 로 덮어씀).
	 * @param InAimLocation 조준점 (직선 = 향할 지점 / 포물선 = 떨어질 지점)
	 * @param InHomingTarget 유도 대상. ProjectileMovement 의 유도가 꺼져 있으면 쓰지 않음
	 * @param InDamageSpec 명중 시 적용할 데미지 스펙 (발사 시점에 생성)
	 * @param InHitCueTag 피격자에게 실행할 피격 연출 큐. 비우면 연출 없음
	 */
	void Auth_Launch(const FVector& InAimLocation, AActor* InHomingTarget, const FGameplayEffectSpecHandle& InDamageSpec, const FGameplayTag& InHitCueTag);

protected:
	//~ Begin AActor Interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	/** 서버 이동 복제로 받은 속도를 이동 컴포넌트에 반영 (엔진 기본은 아무것도 안 함) */
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;
	//~ End AActor Interface

	/** 비행을 담당하는 이동 컴포넌트. 속도·중력·유도·최대 속도는 BP 에서 설정 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Projectile|Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/**
	 * 발사 방식.
	 * Direct : 조준점을 향해 InitialSpeed 로 직선 발사. 중력 0 권장 (중력을 주면 떨어지며 날아감).
	 * Arc    : 조준점에 떨어지는 포물선. 속도는 궤적에서 계산하므로 InitialSpeed 는 무시되고, 중력(ProjectileGravityScale > 0)이 필요함.
	 *          MaxSpeed 를 걸면 계산된 속도가 잘려 궤적이 짧아지므로 0 권장.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Projectile")
	ECBProjectileLaunchMode LaunchMode = ECBProjectileLaunchMode::Direct;

	/**
	 * 포물선 높이 (0.1 = 거의 수직으로 높이 / 0.9 = 거의 직사로 낮게). 엔진 SuggestProjectileVelocity_CustomArc 의 ArcParam.
	 * 양 끝(0 = 수직, 1 = 직사)은 해가 없거나 불안정해 잘라둠.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Projectile", meta = (EditCondition = "LaunchMode == ECBProjectileLaunchMode::Arc", EditConditionHides, ClampMin = "0.1", ClampMax = "0.9"))
	float ArcParam = 0.5f;

private:
	/**
	 * 조준점까지 발사 방식에 맞는 초기 속도 계산.
	 * 포물선을 풀 수 없으면(중력 0·너무 높은 조준점) 경고 후 직선 속도로 대체.
	 */
	FVector ComputeLaunchVelocity(const FVector& InAimLocation) const;

	/** 발사 조건(속도·유도 대상)을 이동 컴포넌트에 반영. 서버는 발사 시, 클라는 BeginPlay 에서 복제값으로 호출 */
	void ApplyLaunchState();

	/** [서버] 이번 틱 이동 구간을 스윕해 명중 처리. 소멸했으면 true */
	bool Auth_SweepForHit(const FVector& InStart, const FVector& InEnd);

	/** 판정 모양 (루트 컴포넌트의 콜리전 모양. 루트가 모양 컴포넌트가 아니면 선) */
	FCollisionShape GetSweepShape() const;

	/** 발사 초기 속도. 클라가 같은 궤적으로 시뮬레이션하도록 최초 1회 복제 */
	UPROPERTY(Replicated)
	FVector_NetQuantize10 LaunchVelocity;

	/** 유도 대상. 클라도 같은 대상을 쫓도록 최초 1회 복제 */
	UPROPERTY(Replicated)
	TObjectPtr<AActor> HomingTarget = nullptr;

	/** [서버] 명중 시 적용할 데미지 스펙 */
	FGameplayEffectSpecHandle DamageSpec;

	/** [서버] 피격 연출 큐 태그 */
	FGameplayTag HitCueTag;

	/** [서버] 지난 틱 위치 (스윕 시작점) */
	FVector PreviousLocation = FVector::ZeroVector;
};
