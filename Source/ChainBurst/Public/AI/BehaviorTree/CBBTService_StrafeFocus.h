#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Services/BTService_DefaultFocus.h"
#include "GameplayTagContainer.h"
#include "CBBTService_StrafeFocus.generated.h"

/**
 * [BT] 브랜치가 살아 있는 동안 블랙보드 타겟을 계속 주시하게 하는 서비스 (경계 모드의 스트레이프 이동용).
 *
 * 엔진 UBTService_DefaultFocus 와 두 가지 다른 점.
 *  1. 포커스 우선순위가 Gameplay.
 *	- 베이스는 Default(0) 라 이동이 시작되면 PathFollowing 이 거는 Move(1) 포커스에 밀려 주시가 풀림.
 *  2. 주시하는 동안 회전 모드를 스트레이프용으로 바꾸고 상태 태그(Status.Movement.Strafe)를 걸며,
 *	- 브랜치를 벗어나면 둘 다 되돌림. 애님은 이 태그를 보고 2D 블렌드스페이스로 전환.
 *  3. 경계하는 동안 이동 속도 어빌리티(기본 걷기)를 켜고, 벗어나면 취소해 원래 개이트로 복구.
 *
 * 서버 전용: BT 는 서버에만 있으므로 회전 설정 변경도 서버에서만 일어나고, 결과 회전만 복제.
 * 다만 상태 태그는 애님이 전 클라에서 읽어야 하므로 TagOnly 로 복제.
 */
UCLASS()
class CHAINBURST_API UCBBTService_StrafeFocus : public UBTService_DefaultFocus
{
	GENERATED_BODY()

public:
	UCBBTService_StrafeFocus();

protected:
	//~ Begin UBTService Interface
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	//~ End UBTService Interface

	//~ Begin UBTNode Interface
	virtual FString GetStaticDescription() const override;
	//~ End UBTNode Interface

	/**
	 * 주시하는 동안 스트레이프 모드(회전 모드 전환 + 상태 태그)를 적용할지 여부.
	 * 끄면 포커스만 걸고 회전·애님은 건드리지 않음. (몸은 진행 방향을 보되 시선 판정만 타겟으로 둘 때).
	 */
	UPROPERTY(EditAnywhere, Category = "ChainBurst")
	bool bUseStrafeRotation = true;

	/**
	 * 경계하는 동안 적용할 이동 속도 어빌리티의 태그 (기본: 걷기).
	 * 속도·개이트 태그·소음 크기가 한 번에 따라오도록 어빌리티 파이프라인을 그대로 씀.
	 * 비워두면 속도는 건드리지 않음.
	 */
	UPROPERTY(EditAnywhere, Category = "ChainBurst", meta = (Categories = "Ability.Movement"))
	FGameplayTag SpeedAbilityTag;

private:
	/**
	 * 빙의된 캐릭터의 스트레이프 모드를 켜거나 원래대로 되돌림 (회전 모드 + 상태 태그를 짝으로 처리).
	 * @param OwnerComp 실행 중인 BT 컴포넌트
	 * @param bEnabled  true = 타겟을 향해 회전(스트레이프) / false = 캐릭터 클래스 기본값으로 복원
	 */
	void SetStrafeModeEnabled(const UBehaviorTreeComponent& OwnerComp, bool bEnabled) const;

	/**
	 * SpeedAbilityTag 이동 속도 어빌리티를 켜거나 취소함.
	 * @param OwnerComp 실행 중인 BT 컴포넌트
	 * @param bEnabled  true = 어빌리티 활성화 / false = 태그로 취소(어빌리티가 속도 GE 를 제거해 복구)
	 */
	void SetSpeedAbilityEnabled(const UBehaviorTreeComponent& OwnerComp, bool bEnabled) const;
};
