#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CBEnemySpawner.generated.h"

class USphereComponent;
class ACBAICharacter;
class ACBBaseCharacter;

/** 스포너의 웨이브 진행 상태 */
enum class ECBWaveState : uint8
{
	Idle,		// 소환 대기 (범위 안에 살아있는 플레이어가 생기면 웨이브 시작)
	Active,		// 웨이브 진행 중 (인원이 늘면 증원만 하고, 전멸하면 쿨다운으로)
	Cooldown	// 전멸 후 재소환 대기
};

/**
 * [서버] 범위 안 플레이어 수에 맞춰 적을 소환하는 레벨 배치 스포너.
 *
 * 스폰 범위는 루트 구(SpawnAreaSphere)의 반경이며, 실제 좌표는 그 안의 내비메시 위에서 뽑는다.
 * 판정·소환은 전부 서버에서만 돌고, 소환된 적은 각자 복제되므로 스포너 자신은 복제하지 않음.
 *
 * 웨이브 규칙:
 * - 범위 안 살아있는 플레이어 N명 → N * EnemiesPerPlayer 마리 소환
 * - 웨이브 중 인원이 늘면 증가분만큼 증원. 인원이 줄어도 되돌리지 않음(웨이브 동안 최고치 유지)
 * - 전멸 전에는 새 웨이브를 시작하지 않고, 전멸하면 RespawnDelay 뒤 다시 인원을 세어 소환
 *
 * 인원 집계·전멸 감지는 ScanInterval 주기 폴링 하나로 처리.
 * 오버랩 이벤트를 쓰지 않는 이유는 이후 무기 변경 기능을 추가할 경우 폰 재스폰이라, 폰이 갈릴 때마다 오버랩 상태를 다시 맞춰야 하기 때문.
 * 매 스캔마다 현재 폰 목록을 새로 읽으면 그 문제가 없음.
 */
UCLASS()
class CHAINBURST_API ACBEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	ACBEnemySpawner();

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	/** [서버] 스캔 주기마다 호출. 적 목록을 정리하고 현재 상태에 맞는 처리를 진행함. */
	void Auth_ScanTick();

	/** [서버] 웨이브를 시작함. (커버 인원 확정 + 첫 소환) */
	void Auth_StartWave(int32 InPlayerCount);

	/** [서버] 적을 InCount 마리 소환함. 안전 상한(MaxAliveEnemies)을 넘는 요청은 남은 여유만큼만 처리. */
	void Auth_SpawnEnemies(int32 InCount);

	/** [서버] 전멸 처리. 커버 인원을 비우고 재소환 대기로 전환함. */
	void Auth_EnterCooldown();

	/** [서버] 범위 안에 있는 살아있는 플레이어 수를 셈. */
	int32 Auth_CountPlayersInArea() const;

	/** [서버] 살아있는 플레이어들의 현재 위치를 모음. (소환 지점의 최소 거리 검사용) */
	void Auth_CollectPlayerLocations(TArray<FVector>& OutLocations) const;

	/**
	 * [서버] 범위 안 내비메시에서 소환 지점을 하나 찾음. 플레이어에게서 MinSpawnDistanceFromPlayers 밖인 지점만 채택.
	 * @param InPlayerLocations 최소 거리 검사에 쓸 플레이어 위치 목록
	 * @param OutLocation 찾은 지점 (실패 시 건드리지 않음)
	 * @return MaxSpawnLocationTries 안에 조건을 만족하는 지점을 찾았는지 여부
	 */
	bool Auth_FindSpawnLocation(const TArray<FVector>& InPlayerLocations, FVector& OutLocation) const;

	/** [서버] 죽었거나 사라진 적을 목록에서 제거함. (남은 수가 0이면 전멸) */
	void Auth_PruneDeadEnemies();

	/** 스폰 범위 반경 (구의 스케일 적용 반경) */
	float GetSpawnRadius() const;

protected:
	/** 스폰 범위를 나타내는 구. 콜리전 없이 범위 값과 에디터 시각화 용도로만 씀. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Spawn")
	TObjectPtr<USphereComponent> SpawnAreaSphere = nullptr;

	/** 소환할 적 클래스 목록. 한 마리마다 이 중에서 무작위로 고름. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|Spawn")
	TArray<TSubclassOf<ACBAICharacter>> EnemyClasses;

	/** 플레이어 1명당 소환할 적 수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|Spawn", meta = (ClampMin = "1"))
	int32 EnemiesPerPlayer = 3;

	/** 전멸 후 다음 웨이브까지의 대기 시간(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|Spawn", meta = (ClampMin = "0.0"))
	float RespawnDelay = 10.f;

	/** 인원 집계·전멸 감지 주기(초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|Spawn", meta = (ClampMin = "0.1"))
	float ScanInterval = 1.f;

	/** 소환 지점이 플레이어에게서 떨어져 있어야 하는 최소 거리 (눈앞에 튀어나오는 것 방지) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|Spawn", meta = (ClampMin = "0.0"))
	float MinSpawnDistanceFromPlayers = 800.f;

	/** 동시에 살아있을 수 있는 적 수의 상한 (인원이 비정상적으로 많아도 소환이 폭주하지 않게 하는 안전장치) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|Spawn", meta = (ClampMin = "1"))
	int32 MaxAliveEnemies = 30;

	/** 적 한 마리의 소환 지점을 찾을 때 허용하는 시도 횟수 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|Spawn", meta = (ClampMin = "1"))
	int32 MaxSpawnLocationTries = 10;

private:
	/** 현재 웨이브 상태 */
	ECBWaveState WaveState = ECBWaveState::Idle;

	/** 이번 웨이브가 소환을 마친 인원 수. 이 값을 넘는 인원이 들어오면 그 증가분만큼 증원함. */
	int32 CoveredPlayerCount = 0;

	/** 이번 웨이브로 소환해 아직 살아있는 적들 */
	TArray<TWeakObjectPtr<ACBBaseCharacter>> AliveEnemies;

	/** 재소환 대기가 끝나는 월드 시간(초) */
	float CooldownEndTime = 0.f;

	/** 스캔 타이머 핸들 */
	FTimerHandle ScanTimerHandle;
};
