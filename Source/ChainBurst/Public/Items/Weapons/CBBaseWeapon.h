#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/CBEnumTypes.h"
#include "CBBaseWeapon.generated.h"

class UBoxComponent;
class UCBWeaponSocketData;

UCLASS()
class CHAINBURST_API ACBBaseWeapon : public AActor
{
	GENERATED_BODY()
	
public:	
	ACBBaseWeapon();
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Weapon|Components")
	TObjectPtr<USceneComponent> SceneRoot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Weapon|Components")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Weapon|Components")
	TObjectPtr<UBoxComponent> WeaponCollisionBox;

	/** 무기가 비전투 전환에 따라 부착이 필요한지 여부 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Weapon|Config")
	bool bRequiresSheathSocketAttachment = true;
	
	/** 무기 종류 (소켓 설정 조회에 사용) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Weapon|Config")
	ECBWeaponCombatType WeaponCombatType = ECBWeaponCombatType::None;

	/**
	 * 무기 종류별 소켓 이름 데이터 에셋
	 * SheathSocketOverride, CombatSocketOverride 에 사용됨 
	 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Weapon|Config")
	TObjectPtr<UCBWeaponSocketData> WeaponSocketData;
	
	/** 전투 시 실제 부착될 소켓 이름 (CombatSocketOverride 변경 시 자동 설정됨, 필요 시 수동 수정 가능) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Weapon|Config")
	FName CombatSocketOverride = NAME_None;
	
	/** 비전투 시 실제 부착될 소켓 이름 (SheathSocket 변경 시 자동 설정됨, 필요 시 수동 수정 가능) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Weapon|Config", meta = (EditCondition = "bRequiresSheathSocketAttachment", EditConditionHides))
	FName SheathSocketOverride = NAME_None;

public:
	/** [전투 상태] 무기를 손에 부착하는 함수 */
	void AttachToHand(USceneComponent* TargetMesh);

	/** [비전투 상태] 무기를 칼집에 부착하는 함수 */
	void AttachToSheath(USceneComponent* TargetMesh);
	
#if WITH_EDITOR
	/** 에디터에서 속성 변경 시 호출되는 함수 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
	FORCEINLINE UBoxComponent* GetWeaponCollisionBox() const { return WeaponCollisionBox; }

private:
	/**
	 * 현재 무기가 손에 장착된 상태인지 여부
	 * true  = 손에 들고 있음 (전투 상태)
	 * false = 칼집에 꽂혀 있음 (비전투 상태)
	 * 중복 부착 호출 방지용
	*/
	bool bIsEquipped = false; 
};
