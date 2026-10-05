#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "CBFragment_AreaAttack.generated.h"

class UGameplayAbility;
class UGameplayEffect;

/**
 * 영역 공격 기능 (영역 공격 조각).
 * 무기가 닿았는지가 아니라 지정한 범위 안에 있는지로 판정해, 한 순간 범위 안의 적 전원에게 데미지를 주는 기능 객체.
 * - 판정 시점 : 몽타주 노티파이가 보내는 영역 공격 이벤트(Event.Combat.AreaAttack). 노티파이가 여러 개면 매번 판정
 * - 판정      : [서버] 시전자 방향으로 돈 박스 오버랩 쿼리(Weapon 채널) → 적대 진영·ASC 보유 → 벽 차단 검사 (판정 중심 또는 지정 소켓에서)
 * - 데미지 GE : [서버] 걸린 대상마다 데미지 GE 적용 + 피격 큐
 * - 디버그    : [서버] bDrawDebug 로 판정 박스·벽 차단 검사 선을 표시 (리슨 서버 호스트 화면에만 보임)
 * 서버에서 직접 판정하므로 서버가 곧 로컬인 AI 공격용. (플레이어가 쓰려면 로컬 감지 → 서버 검증 경로가 따로 필요)
 */
UCLASS()
class CHAINBURST_API UCBFragment_AreaAttack : public UObject
{
	GENERATED_BODY()

public:
	/** 공격 시작. [서버] 영역 공격 이벤트 대기 */
	void Start();

protected:
	/** 범위 안 타겟에게 적용할 데미지 GE 클래스. 비어 있는데 노티파이가 오면 경고 후 판정 생략 */
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** 데미지 계수 (FinalDamage = AttackPower * DamageCoefficient - DefensePower) */
	UPROPERTY(EditDefaultsOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float DamageCoefficient = 1.f;

	/**
	 * 피격자에게 실행할 피격 연출 큐. 비우면 연출 없이 데미지만 들어감.
	 * 영역 공격은 무기가 아니므로 무기의 피격 큐를 쓰지 않고 따로 지정.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Damage", meta = (Categories = "GameplayCue"))
	FGameplayTag HitCueTag;

	/**
	 * 판정 박스의 절반 크기 (cm, 시전자 기준 로컬 공간: X = 전방 / Y = 좌우 / Z = 위아래). 엔진 Box Extent 와 같은 방식.
	 * 박스는 판정 중심에서 각 축으로 ±이 값만큼 퍼지고, 시전자가 향한 방향을 따라 돎.
	 * Z 를 낮추면 점프로 피할 수 있는 판정이 됨. (중심이 시전자 캡슐 중심 높이라, 발밑까지 닿으려면 Z ≥ 캡슐 절반 높이)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Area", meta = (ClampMin = "1.0"))
	FVector BoxExtent = FVector(200.f, 200.f, 100.f);

	/**
	 * 판정 중심 오프셋 (시전자 기준 로컬 공간, X = 전방). 시전자의 방향을 따라 돎.
	 * 0 이면 시전자 중심 (주위 전방위). 몸 앞을 내려찍는 공격이면 전방으로 옮겨 등 뒤가 맞지 않게 함.
	 * 박스가 몸 앞에서 시작하게 하려면 X = BoxExtent.X 정도로 둠.
	 * 중심이 지면 아래로 내려가면 벽 차단 검사가 지면에 막히므로 Z 는 낮추지 말 것.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Area")
	FVector CenterOffset = FVector::ZeroVector;

	/**
	 * 벽 차단 검사의 시작점이 될 시전자 메시의 소켓(또는 본) 이름.
	 * None 이면 판정 중심에서 검사 - 내려찍기·충격파처럼 터진 자리에서 퍼지는 공격.
	 * 지정하면 그 소켓에서 대상까지 검사 - 입·손에서 전방으로 뿜는 공격. 판정 중심이 벽 너머에 있어도 시전자와 대상 사이의 벽을 봄.
	 * 지면보다 충분히 위(손·입·가슴)여야 함. 발처럼 낮으면 검사 선이 지면에 걸려 전부 제외됨.
	 * 이름을 못 찾으면 경고 후 판정 중심에서 검사.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Area")
	FName WallCheckSocketName = NAME_None;

	/**
	 * [서버 전용] 판정 범위 디버그 표시. 판정 순간의 박스와, 대상마다 검사 시작점에서 그은 벽 차단 검사 선(초록 = 적중 / 빨강 = 벽에 막힘)을 그림.
	 * 판정이 서버에서 돌기 때문에 리슨 서버 호스트 화면에만 보이고, 클라이언트로 접속한 화면에는 보이지 않음.
	 * 쉬핑 빌드에서는 켜 있어도 그리지 않음.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Debug", meta = (DisplayName = "Draw Debug (Server Only)"))
	bool bDrawDebug = false;

private:
	/** 이 기능을 소유한 어빌리티 (Outer) */
	UGameplayAbility* GetOwningAbility() const;

	/** [서버] 영역 공격 이벤트 수신 → 범위 안 적마다 데미지 GE 적용 */
	UFUNCTION()
	void OnAreaAttack(FGameplayEventData Payload);

	/**
	 * 벽 차단 검사 시작점 (WallCheckSocketName 소켓 → 없거나 못 찾으면 판정 중심).
	 * @param InAvatar 시전자
	 * @param InCenter 판정 중심
	 * @return 검사 시작점
	 */
	FVector GetWallCheckOrigin(const AActor& InAvatar, const FVector& InCenter) const;
};
