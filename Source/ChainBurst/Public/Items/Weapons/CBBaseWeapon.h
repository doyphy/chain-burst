#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/CBEnumTypes.h"
#include "CBBaseWeapon.generated.h"

class UBoxComponent;

UCLASS()
class CHAINBURST_API ACBBaseWeapon : public AActor
{
	GENERATED_BODY()
	
public:	
	ACBBaseWeapon();
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	TObjectPtr<USceneComponent> SceneRoot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	TObjectPtr<UBoxComponent> WeaponCollisionBox;
	
	/** 비전투 시 부착할 소켓 위치 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Config")
	ECBWeaponSheathSocket SheathSocket;

	/** 실제 부착될 소켓 이름 (SheathSocket 변경 시 자동 설정됨, 필요 시 수동 수정 가능) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Config")
	FName SheathSocketOverride;
	
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
};
