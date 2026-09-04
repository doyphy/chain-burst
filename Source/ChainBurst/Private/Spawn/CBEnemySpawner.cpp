// project
#include "Spawn/CBEnemySpawner.h"
#include "Characters/CBAICharacter.h"
#include "Characters/CBBaseCharacter.h"

// engine
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

ACBEnemySpawner::ACBEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	// 스폰 범위 = 이 구의 반경. 콜리전은 끄고 범위 값·에디터 시각화 용도로만 씀 (인원 판정은 오버랩이 아니라 스캔 폴링)
	SpawnAreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("SpawnAreaSphere"));
	SetRootComponent(SpawnAreaSphere);
	SpawnAreaSphere->SetSphereRadius(2000.f);
	SpawnAreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnAreaSphere->SetGenerateOverlapEvents(false);
	SpawnAreaSphere->ShapeColor = FColor(200, 60, 60);
}

void ACBEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	// 소환 판정은 서버 권위. 클라는 복제된 적만 받으므로 아무것도 하지 않음
	if (!HasAuthority()) return;

	// 소환할 적이 하나도 없으면 조용히 아무 일도 일어나지 않으므로 미리 경고
	if (EnemyClasses.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemySpawner][%s] EnemyClasses 가 비어 있어 소환하지 않음"), *GetName());
		return;
	}

	// 인원 집계와 전멸 감지를 겸하는 단일 스캔 타이머
	GetWorldTimerManager().SetTimer(
		ScanTimerHandle, this, &ACBEnemySpawner::Auth_ScanTick, ScanInterval, true);
}

void ACBEnemySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ScanTimerHandle);

	Super::EndPlay(EndPlayReason);
}

// [서버] 스캔 주기마다 목록을 정리하고 상태별 처리를 진행.
void ACBEnemySpawner::Auth_ScanTick()
{
	// 사망 델리게이트를 일일이 구독하는 대신 이 스캔 하나로 전멸을 감지함.
	Auth_PruneDeadEnemies();

	// 현재 웨이브 상태별 처리.
	switch (WaveState)
	{
	case ECBWaveState::Idle: // 대기 상태
	{
		// 범위 안에 아무도 없으면 계속 대기
		const int32 PlayerCount = Auth_CountPlayersInArea();
		if (PlayerCount <= 0) return;
			
		// 살아있는 인원이 생기면 웨이브 시작	
		Auth_StartWave(PlayerCount);
		return;
	}

	case ECBWaveState::Active: // 웨이브 진행 중
	{
		// 전멸 - 재소환 대기로
		if (AliveEnemies.IsEmpty())
		{
			Auth_EnterCooldown();
			return;
		}

		// 커버한 인원보다 늘어난 만큼만 증원. 인원이 줄어도 되돌리지 않음(웨이브 동안 최고치 유지)
		const int32 PlayerCount = Auth_CountPlayersInArea();
		if (PlayerCount > CoveredPlayerCount)
		{
			const int32 AddedPlayers = PlayerCount - CoveredPlayerCount;
			CoveredPlayerCount = PlayerCount;

			UE_LOG(LogTemp, Log, TEXT("[EnemySpawner][%s] 증원: 플레이어 %d명 추가 → %d마리 소환"),
				*GetName(), AddedPlayers, AddedPlayers * EnemiesPerPlayer);

			Auth_SpawnEnemies(AddedPlayers * EnemiesPerPlayer);
		}
		return;
	}

	case ECBWaveState::Cooldown: // 재소환 대기
	{
		// 대기 쿨다운이 끝날 때까지 대기
		if (GetWorld()->GetTimeSeconds() < CooldownEndTime) return;
		// 대기 상태로 전환	
		WaveState = ECBWaveState::Idle;
		return;
	}
	}
}

// [서버] 웨이브 시작. 지금 범위 안 인원을 커버 인원으로 확정하고 그만큼 소환.
void ACBEnemySpawner::Auth_StartWave(int32 InPlayerCount)
{
	CoveredPlayerCount = InPlayerCount;
	WaveState = ECBWaveState::Active;

	UE_LOG(LogTemp, Log, TEXT("[EnemySpawner][%s] 웨이브 시작: 플레이어 %d명 → %d마리 소환"),
		*GetName(), InPlayerCount, InPlayerCount * EnemiesPerPlayer);

	Auth_SpawnEnemies(InPlayerCount * EnemiesPerPlayer);
}

// [서버] 적 소환. 클래스는 목록에서 무작위, 위치는 범위 안 내비메시에서 뽑음.
void ACBEnemySpawner::Auth_SpawnEnemies(int32 InCount)
{
	UWorld* World = GetWorld();
	if (!World || EnemyClasses.IsEmpty()) return;

	// 안전 상한까지 남은 여유만큼만 소환
	const int32 Room = FMath::Max(0, MaxAliveEnemies - AliveEnemies.Num());
	const int32 SpawnCount = FMath::Min(InCount, Room);
	if (SpawnCount <= 0) return;

	// 최소 거리 검사에 쓸 플레이어 위치는 이 소환 묶음에서 한 번만 모음
	TArray<FVector> PlayerLocations;
	Auth_CollectPlayerLocations(PlayerLocations);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (int32 Index = 0; Index < SpawnCount; ++Index)
	{
		// 소환할 클래스 무작위 선택 (목록에 빈 칸이 있으면 그 마리는 건너뜀)
		const TSubclassOf<ACBAICharacter> EnemyClass = EnemyClasses[FMath::RandRange(0, EnemyClasses.Num() - 1)];
		if (!EnemyClass) continue;

		// 소환 지점 탐색. 조건을 만족하는 자리를 못 찾으면 이 마리는 건너뜀
		FVector SpawnLocation;
		if (!Auth_FindSpawnLocation(PlayerLocations, SpawnLocation)) continue;

		// 내비메시 지점은 바닥 높이라, 캡슐 절반 높이만큼 띄우지 않으면 지면에 박힌 채 스폰됨
		if (const ACharacter* EnemyCDO = EnemyClass->GetDefaultObject<ACharacter>())
		{
			SpawnLocation.Z += EnemyCDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}

		// 바라보는 방향은 무작위 (몰려 나올 때 같은 방향으로 정렬되지 않게)
		const FRotator SpawnRotation(0.f, FMath::FRandRange(0.f, 360.f), 0.f);

		ACBAICharacter* Enemy = World->SpawnActor<ACBAICharacter>(EnemyClass, SpawnLocation, SpawnRotation, SpawnParams);
		if (!Enemy) continue;

		AliveEnemies.Add(Enemy);
	}
}

// [서버] 전멸 처리. 다음 웨이브는 대기가 끝난 뒤 인원을 다시 세어 시작함.
void ACBEnemySpawner::Auth_EnterCooldown()
{
	// 웨이브 상태 전환 + 커버 인원 초기화 + 재소환 대기 시작
	WaveState = ECBWaveState::Cooldown;
	CoveredPlayerCount = 0;
	CooldownEndTime = GetWorld()->GetTimeSeconds() + RespawnDelay;

	UE_LOG(LogTemp, Log, TEXT("[EnemySpawner][%s] 전멸 — %.1f초 뒤 재소환"), *GetName(), RespawnDelay);
}

// [서버] 범위 안 살아있는 플레이어 수. 죽은 플레이어는 시체가 남으므로 인원에서 제외.
int32 ACBEnemySpawner::Auth_CountPlayersInArea() const
{
	TArray<FVector> PlayerLocations;
	Auth_CollectPlayerLocations(PlayerLocations);

	const FVector Origin = GetActorLocation();
	const float RadiusSquared = FMath::Square(GetSpawnRadius());

	int32 Count = 0;
	for (const FVector& PlayerLocation : PlayerLocations)
	{
		if (FVector::DistSquared(PlayerLocation, Origin) <= RadiusSquared)
		{
			++Count;
		}
	}
	return Count;
}

// [서버] 살아있는 플레이어들의 위치 수집. (범위 판정 + 소환 지점 최소 거리 검사 공용)
void ACBEnemySpawner::Auth_CollectPlayerLocations(TArray<FVector>& OutLocations) const
{
	const UWorld* World = GetWorld();
	if (!World) return;

	// 서버에는 접속한 모든 플레이어 컨트롤러가 있으므로 여기서 현재 폰을 그때그때 읽음
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (!PC) continue;

		const ACBBaseCharacter* PlayerCharacter = Cast<ACBBaseCharacter>(PC->GetPawn());
		if (!PlayerCharacter || PlayerCharacter->IsDead()) continue;

		OutLocations.Add(PlayerCharacter->GetActorLocation());
	}
}

// [서버] 범위 안 내비메시에서 소환 지점 탐색.
bool ACBEnemySpawner::Auth_FindSpawnLocation(const TArray<FVector>& InPlayerLocations, FVector& OutLocation) const
{
	// 내비 메시 시스템이 없으면 소환할 수 없음
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem) return false;

	const FVector Origin = GetActorLocation();
	const float Radius = GetSpawnRadius();
	const float MinDistanceSquared = FMath::Square(MinSpawnDistanceFromPlayers);

	for (int32 Try = 0; Try < MaxSpawnLocationTries; ++Try)
	{
		// 도달 가능한 지점만 뽑아, 벽 너머의 끊긴 내비메시 조각에 소환되는 것을 막음
		FNavLocation NavLocation;
		if (!NavSystem->GetRandomReachablePointInRadius(Origin, Radius, NavLocation)) continue;

		// 플레이어 코앞이면 다시 뽑음
		bool bTooCloseToPlayer = false;
		for (const FVector& PlayerLocation : InPlayerLocations)
		{
			if (FVector::DistSquared(NavLocation.Location, PlayerLocation) < MinDistanceSquared)
			{
				bTooCloseToPlayer = true;
				break;
			}
		}
		if (bTooCloseToPlayer) continue;

		OutLocation = NavLocation.Location;
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[EnemySpawner][%s] 소환 지점을 찾지 못함 (내비메시 범위·최소 거리 확인 필요)"), *GetName());
	return false;
}

// [서버] 죽었거나 파괴된 적을 목록에서 제거. 남은 수가 0이면 전멸로 판정됨.
void ACBEnemySpawner::Auth_PruneDeadEnemies()
{
	// AliveEnemies 목록에서 nullptr 이거나 IsDead() 가 true 인 적을 제거.
	AliveEnemies.RemoveAll([](const TWeakObjectPtr<ACBBaseCharacter>& InEnemy)
	{
		const ACBBaseCharacter* Enemy = InEnemy.Get();
		// nullptr 이거나 IsDead() 가 true 면 제거. (IsDead() 는 이미 사망한 적도 true 를 반환하므로, 파괴된 적도 제거됨)
		return (Enemy == nullptr) || Enemy->IsDead();
	});
}

float ACBEnemySpawner::GetSpawnRadius() const
{
	return SpawnAreaSphere ? SpawnAreaSphere->GetScaledSphereRadius() : 0.f;
}
