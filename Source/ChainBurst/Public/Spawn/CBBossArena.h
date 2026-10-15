#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "CBBossArena.generated.h"

class UArrowComponent;
class UBoxComponent;
class UCameraComponent;
class UAbilitySystemComponent;
class UGameplayEffect;
class APlayerController;
class ACBAICharacter;
class ACBBaseCharacter;

/** 보스 구역의 진행 단계 */
enum class ECBBossArenaPhase : uint8
{
	Waiting,	// 전원 진입 대기 (구역 안 플레이어에게 대기 태그를 붙임)
	Countdown,	// 전원 진입 - 보스 등장까지 대기 (한 명이라도 빠지면 Waiting 으로)
	Fighting,	// 보스전 진행 중 (입구 차단)
	Cleared		// 보스 처치 - 입구 개방 후 더 이상 진행하지 않음
};

/** 플레이어 한 명에게 붙은 보스전 졸개 묶음 (서버 전용) */
struct FCBArenaMinionSquad
{
	/** 소환해 아직 살아 있는 졸개 */
	TArray<TWeakObjectPtr<ACBBaseCharacter>> Minions;

	/** 다음 소환을 허용하는 월드 시간(초). 묶음이 비어 있을 때만 봄 */
	float NextSpawnTime = 0.f;
};

/**
 * [서버] 보스 구역. 세션의 모든 플레이어가 구역에 들어오면 BossAppearDelay 뒤에 보스를 소환하고 입구를 막는 레벨 배치 액터.
 *
 * 진행 규칙:
 * - Waiting   : 살아서 구역 안에 있는 플레이어에게 Status.Arena.Waiting 태그를 붙임 (HUD 의 "다른 플레이어 대기 중" 표시)
 * - Countdown : 접속한 플레이어 전원이 살아서 구역 안에 들어오면 시작. 플레이어마다 카운트다운 기간 GE 를 걸고 BossAppearDelay 타이머를 셈.
 *               한 명이라도 나가거나 죽으면 GE 를 걷어내고 Waiting 으로
 * - Fighting  : 보스 소환 + 입구 차단 액터 소환. 보스가 죽으면 입구를 열고 Cleared,
 *               구역 안에 살아있는 플레이어가 없으면(전멸) 보스를 없애고 입구를 열어 Waiting 으로 되돌림
 *
 * 인원 비례 내구도: 보스가 (서버에서) 준비되면 최대 체력·최대 기절 게이지를 접속 인원만큼 곱하고 체력을 가득 채움.
 * 로드아웃 시작 GE 가 1인 기준 값을 정한 뒤여야 덮이지 않으므로 소환 직후가 아니라 준비 완료에서 함
 *
 * 등장 연출 (보스 소환과 동시):
 * - 모든 플레이어 카메라를 IntroCamera 로 블렌드하고 시네마틱 모드(이동·시점 입력 차단)를 켬.
 *   서버가 원격 PC 의 뷰 타겟·시네마틱 모드를 바꾸면 엔진이 클라이언트로 RPC 를 보내므로 네트워크 코드가 없음
 * - 공격·대시·점프 등 어빌리티로 하는 행동은 차단 GE(IntroBlockEffectClass)를 플레이어 ASC 에 걸어 막음
 * - 보스가 (서버에서) 준비되면 Event.Combat.Intro 를 보내 보스의 등장 어빌리티를 발동 (몽타주는 그 어빌리티가 큐로 재생)
 * - 보스 ASC 에서 Status.Combat.Intro 가 빠지면(= 등장 어빌리티 종료) 카메라·입력을 되돌림. 연출 길이는 몽타주 길이 하나뿐
 *
 * 졸개 (MinionClass 가 있을 때, 등장 연출이 끝난 뒤부터):
 * - 구역 안 생존 플레이어마다 따로 MinionRespawnDelay 뒤 그 플레이어 주변에 MinionsPerPlayer 마리를 소환하고, 그 플레이어만 쫓게 지정함
 * - 그 플레이어의 졸개가 전부 죽으면 MinionRespawnDelay 뒤 다시 소환. 보스 처치 시 남은 졸개는 사망 처리, 전멸 리셋 시 보스와 함께 제거
 *
 * 구역 판정·소환은 전부 서버에서만 돌고, 스포너처럼 자신은 복제하지 않음.
 * - 보스: 복제되는 캐릭터라 그대로 전 클라에 나타남. 생성 연출·보스 바는 캐릭터가 스스로 처리
 * - 입구: 복제되는 차단 액터를 소환/파괴함. 클라 이동 예측에도 충돌이 있어야 하므로 켜고 끄는 대신 액터 자체를 복제
 * - 대기 표시: 플레이어 ASC 의 TagOnly 루스 태그를 HUD 위젯이 구독 (UI 용 복제 코드 없음)
 * - 카운트다운 표시: 플레이어 ASC 의 기간 GE(Status.Arena.Countdown 부여)를 HUD 위젯이 구독하고 남은 시간도 그 GE 에서 읽음
 *
 * 인원 집계·전멸 감지는 ScanInterval 주기 폴링 하나로 처리 (ACBEnemySpawner 와 같은 이유 - 리스폰마다 폰이 갈림).
 * 보스 등장만은 표시(GE 만료)와 같은 순간이어야 하므로 스캔 비교가 아니라 1회 타이머로 셈.
 * 보스전 중 사망한 플레이어의 부활 위치는 RespawnPoint 이며, 사망 시점에 게임모드가 GetRespawnTransform 으로 받아감.
 */
UCLASS()
class CHAINBURST_API ACBBossArena : public AActor
{
	GENERATED_BODY()

public:
	ACBBossArena();

	/** [서버] 보스전이 진행 중인지 (입구가 막혀 있는 동안) */
	FORCEINLINE bool IsBossFightActive() const { return Phase == ECBBossArenaPhase::Fighting; }

	/**
	 * [서버] 보스전 중 사망한 플레이어가 부활할 위치. (ACBGameplayGameMode 가 플레이어 사망 시점에 호출)
	 * @param InPawnClass 부활시킬 폰 클래스 (캡슐 절반 높이만큼 띄우는 데 씀)
	 */
	FTransform GetRespawnTransform(TSubclassOf<APawn> InPawnClass) const;

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	/** [서버] 스캔 주기마다 호출. 인원을 다시 세어 현재 단계에 맞는 처리를 진행함. */
	void Auth_ScanTick();

	/**
	 * [서버] 전원 진입 - 대기 태그를 모두 떼고, 플레이어마다 카운트다운 GE 를 걸고, 보스 등장 타이머를 시작함.
	 * BossAppearDelay 가 0 이면 곧바로 보스전을 시작함.
	 * @param InInsidePlayers 구역 안 플레이어 (= 접속 인원 전원)
	 */
	void Auth_StartCountdown(const TArray<APlayerController*>& InInsidePlayers);

	/**
	 * [서버] 카운트다운 취소 - 타이머·카운트다운 GE 를 걷어내고 Waiting 으로 돌아가 남은 플레이어에게 대기 태그를 다시 붙임.
	 * @param InInsidePlayers 아직 구역 안에 있는 플레이어
	 */
	void Auth_CancelCountdown(const TArray<APlayerController*>& InInsidePlayers);

	/**
	 * [서버] 보스 등장 타이머 콜백. 마지막 스캔 뒤에 누가 나갔을 수 있으므로 인원을 다시 확인하고,
	 * 전원이 그대로면 보스전을 시작하고 아니면 카운트다운을 취소함.
	 */
	void Auth_HandleCountdownFinished();

	/** [서버] 카운트다운 GE 를 건 플레이어들에게서 걷어냄. (취소·보스 등장 공용 - 이미 만료됐으면 아무 일 없음) */
	void Auth_RemoveCountdownEffects();

	/** [서버] 카운트다운 종료 - 보스와 입구 차단 액터를 소환하고 등장 연출을 시작함. 보스 소환에 실패하면 입구를 막지 않고 Cleared 로 끝냄. */
	void Auth_StartBossFight();

	/**
	 * [서버] 보스 내구도를 접속 인원만큼 곱함 - 최대 체력·최대 기절 게이지 x 인원, 체력은 늘어난 최대까지 채움. 1명이면 아무 일 없음.
	 * 로드아웃 시작 GE 가 정한 1인 기준 값에 곱하므로 보스 준비 완료(Auth_HandleBossReady)에서 부름
	 * @param InBossASC 보스 ASC
	 */
	void Auth_ScaleBossByPlayerCount(UAbilitySystemComponent* InBossASC) const;

	/** [서버] 보스 처치 - 남은 졸개를 사망 처리하고 입구를 열고 스캔을 멈춤. (등장 연출 중이었으면 끝냄) */
	void Auth_ClearArena();

	/** [서버] 전멸 - 보스와 졸개를 없애고 입구를 열어 Waiting 으로 되돌림. (다시 전원이 들어오면 새 보스가 나옴. 등장 연출 중이었으면 끝냄) */
	void Auth_ResetBossFight();

#pragma region BossIntro
	/**
	 * [서버] 등장 연출 시작 - 모든 플레이어 카메라를 IntroCamera 로 돌리고 입력을 막은 뒤,
	 * 보스가 준비되면(이미 준비됐으면 즉시) 등장 어빌리티를 발동함.
	 */
	void Auth_StartBossIntro();

	/**
	 * [서버] 보스 준비 완료 - 내구도를 인원만큼 올린 뒤 Event.Combat.Intro 로 등장 어빌리티를 발동하고 그 종료(태그 제거)를 기다림.
	 * 발동된 어빌리티가 없으면(로드아웃 누락) 경고 후 곧바로 연출을 끝냄 - 카메라가 묶인 채 남지 않게.
	 */
	void Auth_HandleBossReady();

	/** 보스 ASC 의 Status.Combat.Intro 카운트 변경 콜백. 0 이 되면(= 등장 어빌리티 종료) 연출을 끝냄 */
	void HandleBossIntroTagChanged(const FGameplayTag InTag, int32 NewCount);

	/** [서버] 등장 연출 종료 - 구독을 풀고 모든 플레이어 카메라·입력을 되돌림. 연출 중이 아니면 아무 일 없음 */
	void Auth_EndBossIntro();

	/**
	 * [서버] 모든 플레이어의 카메라와 입력을 연출용 또는 평소로 맞춤.
	 * @param bInIntro true = IntroCamera + 이동·시점 입력 차단 + 행동 어빌리티 차단 GE, false = 자기 폰 + 차단 해제 + GE 제거
	 */
	void Auth_SetPlayersIntroView(bool bInIntro);
#pragma endregion

#pragma region Minion
	/**
	 * [서버] 보스전 스캔마다 졸개 진행 - 죽은 졸개를 정리하고, 졸개가 없고 대기가 끝난 플레이어 주변에 새로 소환함.
	 * 묶음이 빈 순간부터 MinionRespawnDelay 를 셈. 처음 보는 플레이어도 같은 지연 뒤 첫 소환
	 * @param InInsidePlayers 살아서 구역 안에 있는 플레이어 (이들에게만 소환)
	 */
	void Auth_UpdateMinions(const TArray<APlayerController*>& InInsidePlayers);

	/**
	 * [서버] 한 플레이어 주변에 졸개를 MinionsPerPlayer 마리 소환하고 그 플레이어를 타겟으로 지정함. 자리를 못 찾은 마리는 건너뜀.
	 * @param InPlayer 졸개가 쫓을 플레이어
	 * @param InOutSquad 소환한 졸개를 담을 그 플레이어의 묶음
	 * @param InPlayerLocations 구역 안 생존 플레이어 위치 (어느 플레이어 코앞에도 소환하지 않게)
	 */
	void Auth_SpawnMinionSquad(const APlayerController* InPlayer, FCBArenaMinionSquad& InOutSquad, const TArray<FVector>& InPlayerLocations);

	/**
	 * [서버] 플레이어 주변 내비메시에서 졸개 소환 지점을 하나 찾음.
	 * 그 플레이어에게서 걸어갈 수 있고, 구역 박스 안이며, 모든 플레이어에게서 MinionSpawnRadius 의 절반 밖인 지점만 채택.
	 * @param InCenter 졸개가 쫓을 플레이어 위치
	 * @param InPlayerLocations 최소 거리 검사에 쓸 플레이어 위치 목록
	 * @param OutLocation 찾은 지점 - 바닥 높이 (실패 시 건드리지 않음)
	 * @return MaxMinionSpawnLocationTries 안에 찾았는지 여부
	 */
	bool Auth_FindMinionSpawnLocation(const FVector& InCenter, const TArray<FVector>& InPlayerLocations, FVector& OutLocation) const;

	/**
	 * [서버] 살아 있는 졸개를 전부 정리하고 묶음을 비움.
	 * @param bInKill true = 사망 처리 (사망 몽타주·소멸 연출이 평소처럼 진행 - 보스 처치), false = 즉시 파괴 (전멸 리셋 - 보스와 같은 처리)
	 */
	void Auth_RemoveMinions(bool bInKill);
#pragma endregion

	/** [서버] 입구 차단 액터를 파괴함. */
	void Auth_OpenEntrance();

	/**
	 * [서버] 대기 태그를 InInsidePlayers 에게만 붙인 상태로 맞춤. 새로 들어온 플레이어는 붙이고 빠진 플레이어는 뗌.
	 * @param InInsidePlayers 태그를 가져야 할 플레이어 (빈 배열이면 전부 뗌)
	 */
	void Auth_UpdateWaitingTags(const TArray<APlayerController*>& InInsidePlayers);

	/**
	 * [서버] 접속한 플레이어 수를 세고, 그중 살아서 구역 안에 있는 플레이어를 모음.
	 * @param OutInsidePlayers 살아서 구역 안에 있는 플레이어
	 * @return 접속한 플레이어 수 (구역 밖·사망·리스폰 대기 포함)
	 */
	int32 Auth_CollectPlayersInArena(TArray<APlayerController*>& OutInsidePlayers) const;

	/** 위치가 구역 박스 안인지 */
	bool IsInsideArena(const FVector& InLocation) const;

	/**
	 * 화살표 위치(바닥)에서 InPawnClass 의 캡슐 절반 높이만큼 띄운 스폰 트랜스폼. 회전은 화살표의 Yaw 만 씀.
	 * @param InPoint 스폰 지점 화살표 (바닥에 둠)
	 * @param InPawnClass 스폰할 폰 클래스 (캐릭터가 아니면 띄우지 않음)
	 */
	static FTransform MakeGroundedSpawnTransform(const UArrowComponent* InPoint, TSubclassOf<APawn> InPawnClass);

	/**
	 * 바닥 위치에서 InPawnClass 의 캡슐 절반 높이만큼 띄운 스폰 트랜스폼.
	 * @param InGroundLocation 스폰 지점 (바닥 높이)
	 * @param InYaw 바라볼 방향 (캐릭터는 똑바로 서야 하므로 Yaw 만 받음)
	 * @param InPawnClass 스폰할 폰 클래스 (캐릭터가 아니면 띄우지 않음)
	 */
	static FTransform MakeGroundedSpawnTransform(const FVector& InGroundLocation, float InYaw, TSubclassOf<APawn> InPawnClass);

protected:
	/** 보스 구역. 콜리전 없이 범위 값과 에디터 시각화 용도로만 씀 (인원 판정은 오버랩이 아니라 스캔 폴링) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|BossArena")
	TObjectPtr<UBoxComponent> ArenaBounds = nullptr;

	/** 보스가 나타날 위치·방향. 바닥에 두고 플레이어가 들어오는 쪽을 보게 둘 것 (최초 타겟은 시야 퍼셉션으로 잡음) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|BossArena")
	TObjectPtr<UArrowComponent> BossSpawnPoint = nullptr;

	/** 입구 차단 액터를 소환할 위치·방향. 구역 박스 바깥(통로 쪽)에 둘 것 - 안으로 판정된 플레이어가 벽에 끼지 않게 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|BossArena")
	TObjectPtr<UArrowComponent> EntranceBlockerPoint = nullptr;

	/** 보스전 중 사망한 플레이어가 부활할 위치·방향. 구역 박스 바깥의 트인 바닥에 둘 것 (동시에 여럿이 부활할 수 있음) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|BossArena")
	TObjectPtr<UArrowComponent> RespawnPoint = nullptr;

	/**
	 * 등장 연출 카메라. 레벨에서 보스 등장 장면이 잘 보이게 구도를 잡아 둠.
	 * 이 액터는 레벨 배치라 복제하지 않아도 모든 클라에 같은 위치로 존재하므로, 서버가 이 액터를 뷰 타겟으로 보내면 각 클라가 자기 쪽 카메라로 봄
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|BossArena")
	TObjectPtr<UCameraComponent> IntroCamera = nullptr;

	/** 소환할 보스 클래스. 비어 있으면 구역이 동작하지 않음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|BossArena")
	TSubclassOf<ACBAICharacter> BossClass;

	/** 입구를 막을 액터 클래스 (벽 메시·이펙트·충돌은 BP 에서). 복제는 소환 직후 코드가 켬. 비어 있으면 입구를 막지 않음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|BossArena")
	TSubclassOf<AActor> EntranceBlockerClass;

	/** 전원이 들어온 뒤 보스가 나타나기까지의 시간(초). 카운트다운 GE 의 기간도 이 값으로 덮어씀 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|BossArena", meta = (ClampMin = "0.0"))
	float BossAppearDelay = 5.f;

	/**
	 * 카운트다운 표시용 기간 GE (Has Duration + GrantedTags 에 Status.Arena.Countdown). 기간 값은 BossAppearDelay 로 덮어씀.
	 * 비어 있거나 조건이 맞지 않으면 경고 후 표시 없이 카운트다운만 진행함
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|BossArena")
	TSubclassOf<UGameplayEffect> CountdownEffectClass;

	/** 등장 연출 카메라로 들어갈 때·폰으로 돌아올 때의 블렌드 시간(초). 0 이면 즉시 전환 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|BossArena", meta = (ClampMin = "0.0"))
	float IntroCameraBlendTime = 0.5f;

	/**
	 * 등장 연출 동안 플레이어에게 거는 행동 차단 GE (Infinite + Block Abilities with Tags 컴포넌트 - 공격·장착·해제·버스트·이동 계열).
	 * 시네마틱 모드는 이동·시점 입력만 막아, 대시·점프·공격처럼 어빌리티로 움직이는 행동은 이 GE 가 막음.
	 * GE 라 소유 클라에도 복제되어 클라가 예측 발동 자체를 하지 않음. 비어 있거나 조건이 맞지 않으면 경고 후 이동·시점만 막음
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|BossArena")
	TSubclassOf<UGameplayEffect> IntroBlockEffectClass;

	/**
	 * 보스전 동안 플레이어 주변에 소환해 그 플레이어만 쫓게 하는 졸개. 비어 있으면 졸개 없음 (졸개 없는 구역도 정상 설정).
	 * 등장 연출이 끝난 뒤부터 플레이어마다 따로 소환 - 그 플레이어의 졸개가 전부 죽으면 MinionRespawnDelay 뒤 다시 소환
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|BossArena|Minion")
	TSubclassOf<ACBAICharacter> MinionClass;

	/** 플레이어 한 명에게 한 번에 소환하는 졸개 수 (= 그 플레이어에게 붙는 최대 수) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|BossArena|Minion", meta = (ClampMin = "1"))
	int32 MinionsPerPlayer = 3;

	/** 한 플레이어의 졸개가 전부 죽은 뒤 다시 소환하기까지의 시간(초). 등장 연출이 끝난 뒤 첫 소환도 이 시간 뒤 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|BossArena|Minion", meta = (ClampMin = "0.0", Units = "s"))
	float MinionRespawnDelay = 10.f;

	/** 졸개를 소환할 플레이어 주변 반경. 절반 안쪽(코앞)은 제외하고, 구역 박스 밖은 쓰지 않음 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|BossArena|Minion", meta = (ClampMin = "100.0", Units = "cm"))
	float MinionSpawnRadius = 1000.f;

private:
	/** 인원 집계·전멸 감지 주기(초). 이탈 감지가 최대 이 값만큼 늦을 뿐이라 튜닝 대상이 아님 */
	static constexpr float ScanInterval = 0.5f;

	/** 현재 진행 단계 */
	ECBBossArenaPhase Phase = ECBBossArenaPhase::Waiting;

	/** 카운트다운 GE 를 건 플레이어 ASC (취소·보스 등장 때 걷어낼 대상) */
	TArray<TWeakObjectPtr<UAbilitySystemComponent>> CountdownASCs;

	/** 보스 등장 타이머 핸들. 월드 타이머라 월드 정지(레벨업) 중에는 카운트다운 GE 기간과 함께 멈춤 */
	FTimerHandle CountdownTimerHandle;

	/** 소환한 보스 */
	TWeakObjectPtr<ACBBaseCharacter> SpawnedBoss;

	/** 등장 연출 중인지 (카메라·입력을 바꿔 둔 동안). 연출 종료를 한 번만 처리하는 가드 */
	bool bBossIntroActive = false;

	/** 보스 준비 완료 델리게이트 구독 해제용 핸들 (준비 전에 소환됐을 때만 씀) */
	FDelegateHandle BossReadyHandle;

	/** 보스 ASC 의 Status.Combat.Intro 태그 변경 델리게이트 구독 해제용 핸들 */
	FDelegateHandle BossIntroTagChangedHandle;

	/** 소환한 입구 차단 액터 */
	TWeakObjectPtr<AActor> SpawnedEntranceBlocker;

	/** 대기 태그를 붙여 둔 플레이어 ASC (PlayerState 소유라 폰이 바뀌어도 같음) */
	TSet<TWeakObjectPtr<UAbilitySystemComponent>> WaitingTaggedASCs;

	/** 졸개 한 마리의 소환 지점을 찾을 때 허용하는 시도 횟수. 실패 비용 상한이라 튜닝 대상이 아님 */
	static constexpr int32 MaxMinionSpawnLocationTries = 10;

	/** 플레이어별 졸개 묶음. 키는 컨트롤러 - 접속을 끊은 플레이어의 졸개도 끝날 때 함께 정리되도록 묶음은 보스전 끝까지 둠 */
	TMap<TWeakObjectPtr<APlayerController>, FCBArenaMinionSquad> MinionSquads;

	/** 스캔 타이머 핸들 */
	FTimerHandle ScanTimerHandle;
};
