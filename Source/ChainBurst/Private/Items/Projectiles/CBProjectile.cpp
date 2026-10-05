// project
#include "Items/Projectiles/CBProjectile.h"
#include "Types/CBCollisionChannels.h"
#include "CBAbilitySystemLibrary.h"

// engine
#include "Components/ShapeComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GenericTeamAgentInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ACBProjectile::ACBProjectile()
{
	// 명중 판정용 틱 (서버 전용, 클라는 BeginPlay 에서 끔)
	PrimaryActorTick.bCanEverTick = true;

	// 서버가 스폰하고 클라가 받음. 클라 시뮬레이션이 갈라지면 이동 복제로 보정.
	bReplicates = true;
	SetReplicatingMovement(true);

	// 빗나간 투사체가 끝없이 날아가지 않도록 기본 수명 (BP 에서 조정)
	InitialLifeSpan = 5.f;

	// 루트는 두지 않음. BP 에서 모양 컴포넌트를 루트로 올리면 이동 컴포넌트가 그것을 자동으로 움직임.
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 1500.f;
	ProjectileMovement->ProjectileGravityScale = 0.f; // 직선 기본. 포물선은 BP 에서 중력을 켤 것
	ProjectileMovement->bRotationFollowsVelocity = true; // 박스·메시 등 방향이 있는 모양이 진행 방향을 향하도록
}

void ACBProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 발사 조건은 스폰 순간에 한 번만 필요함
	DOREPLIFETIME_CONDITION(ThisClass, LaunchVelocity, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ThisClass, HomingTarget, COND_InitialOnly);
}

void ACBProjectile::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 판정은 Weapon 채널 스윕으로만 함. 모든 콜리전을 끔.
	// 루트 콜리전이 켜져 있으면 - 이동 컴포넌트가 그것으로 따로 부딪혀 멈추거나 튕기고
	// 연출 메시의 콜리전이 켜져 있으면 - 무기 트레이스·카메라·영역 공격에서 벽 검사에 걸림
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 이동 컴포넌트가 이번 틱 이동을 마친 뒤에 판정하도록 액터 틱을 뒤로 미룸
	AddTickPrerequisiteComponent(ProjectileMovement);
}

void ACBProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 서버인 경우
	if (HasAuthority())
	{
		// 첫 스윕 시작점
		PreviousLocation = GetActorLocation();

		// 루트가 모양 컴포넌트가 아니면 굵기 없는 선으로 판정됨. (경고 출력)
		if (!Cast<UShapeComponent>(GetRootComponent()))
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] 루트가 Sphere/Box/Capsule 컴포넌트가 아님 - 굵기 없는 선으로 판정됨"), *GetClass()->GetName());
		}
	}
	else
	{
		// 명중 판정은 서버 전용
		SetActorTickEnabled(false);

		// 복제받은 발사 조건으로 서버와 같은 궤적을 시뮬레이션
		ApplyLaunchState();
	}
}

void ACBProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// [서버] 이번 틱 이동 구간(지난 위치 → 현재 위치)을 판정
	const FVector CurrentLocation = GetActorLocation();
	if (Auth_SweepForHit(PreviousLocation, CurrentLocation)) return;

	PreviousLocation = CurrentLocation;
}

void ACBProjectile::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	// 서버가 보정한 속도로 시뮬레이션을 이어감 (유도로 궤적이 갈라져도 따라잡음)
	ProjectileMovement->Velocity = NewVelocity;
}

// [서버] 발사 (외부에서 호출)
void ACBProjectile::Auth_Launch(const FVector& InAimLocation, AActor* InHomingTarget, const FGameplayEffectSpecHandle& InDamageSpec, const FGameplayTag& InHitCueTag)
{
	DamageSpec = InDamageSpec;
	HitCueTag = InHitCueTag;
	HomingTarget = InHomingTarget;
	LaunchVelocity = ComputeLaunchVelocity(InAimLocation);

	ApplyLaunchState();
}

// 조준점까지 발사 방식에 맞는 초기 속도 계산
FVector ACBProjectile::ComputeLaunchVelocity(const FVector& InAimLocation) const
{
	// 발사체 속도 벡터 계산
	const FVector StartLocation = GetActorLocation();
	const FVector DirectVelocity = (InAimLocation - StartLocation).GetSafeNormal() * ProjectileMovement->InitialSpeed;

	// 직선 발사면 발사체 속도 벡터 반환
	if (LaunchMode == ECBProjectileLaunchMode::Direct)
	{
		return DirectVelocity;
	}

	// 포물선. 이 투사체에 실제로 걸리는 중력으로 풀어야 조준점에 떨어짐.
	// 엔진 함수는 중력 0 을 "월드 중력 사용"으로 해석해, 중력 없는 투사체를 하늘로 날려 보내므로 여기서 거름.
	const float GravityZ = ProjectileMovement->GetGravityZ();
	FVector ArcVelocity;
	if (GravityZ < 0.f && UGameplayStatics::SuggestProjectileVelocity_CustomArc(this, ArcVelocity, StartLocation, InAimLocation, GravityZ, ArcParam))
	{
		return ArcVelocity;
	}

	UE_LOG(LogTemp, Warning, TEXT("[%s] 포물선 궤적을 풀 수 없음 (중력 %.1f) - 직선으로 발사. ProjectileGravityScale > 0 인지, 조준점이 너무 높지 않은지 확인"),
		*GetClass()->GetName(), GravityZ);
	return DirectVelocity;
}

// 발사 조건(속도·유도 대상)을 이동 컴포넌트에 반영
void ACBProjectile::ApplyLaunchState()
{
	ProjectileMovement->Velocity = LaunchVelocity;
	ProjectileMovement->UpdateComponentVelocity();

	// 유도는 BP 에서 켠 투사체만 (세기 = HomingAccelerationMagnitude)
	if (ProjectileMovement->bIsHomingProjectile && HomingTarget)
	{
		ProjectileMovement->HomingTargetComponent = HomingTarget->GetRootComponent();
	}
}

// [서버] 이동 구간 스윕 → 명중 처리
bool ACBProjectile::Auth_SweepForHit(const FVector& InStart, const FVector& InEnd)
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	// 시전자(와 자기 자신)는 판정 제외
	AActor* InstigatorActor = GetInstigator();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CBProjectile), false, this);
	if (InstigatorActor)
	{
		QueryParams.AddIgnoredActor(InstigatorActor);
	}

	// Weapon 채널이라 캐릭터는 메시(피직스 애셋 바디)에 Overlap 으로 걸리고, 벽·지면(채널 기본 Block)에서 끊김.
	// 결과는 가까운 순이고 막힌 지점이 있으면 맨 마지막.
	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(Hits, InStart, InEnd, GetActorQuat(), CBCollisionChannels::Weapon, GetSweepShape(), QueryParams);

	for (const FHitResult& Hit : Hits)
	{
		// 벽·지면에 닿음 → 소멸 (그 앞의 적은 이미 위에서 처리됨)
		if (Hit.bBlockingHit)
		{
			Destroy();
			return true;
		}

		// 적대 진영 + ASC 보유 대상만 명중. 아군·중립은 통과. (시전자가 파괴됐으면 진영 판정이 중립이 되어 통과)
		AActor* HitActor = Hit.GetActor();
		if (FGenericTeamId::GetAttitude(InstigatorActor, HitActor) != ETeamAttitude::Hostile) continue;
		if (!UCBAbilitySystemLibrary::GetASC(HitActor)) continue;

		// 첫 적에게 데미지 GE + 피격 연출 후 소멸 (단일 명중)
		UCBAbilitySystemLibrary::Auth_ApplyDamageSpecToTarget(DamageSpec, Hit);
		UCBAbilitySystemLibrary::Auth_ExecuteHitCue(HitActor, InstigatorActor, HitCueTag, Hit);
		Destroy();
		return true;
	}

	return false;
}

// 판정 모양 (루트 모양 컴포넌트의 모양·크기·스케일)
FCollisionShape ACBProjectile::GetSweepShape() const
{
	if (const UShapeComponent* ShapeRoot = Cast<UShapeComponent>(GetRootComponent()))
	{
		return ShapeRoot->GetCollisionShape();
	}

	// 모양이 없으면 선 (BeginPlay 에서 경고)
	return FCollisionShape();
}
