#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "CBAbilitySystemLibrary.h"
#include "CBLocomotionProcessor.generated.h"

class UCharacterMovementComponent;
class UCBAbilitySystemComponent;

UCLASS()
class CHAINBURST_API UCBLocomotionProcessor : public UCBExtensionComponent
{
	GENERATED_BODY()
public:
	UCBLocomotionProcessor();
	
protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float CalculateMaxAcceleration();
	float CalculateBrakingDeceleration();

	TWeakObjectPtr<UCharacterMovementComponent> CachedCMC;
	
	TWeakObjectPtr<UCBAbilitySystemComponent> CachedASC;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Acceleration")
	float WalkMaxAcceleration = 1000.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Acceleration")
	float RunMaxAcceleration = 1000.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Acceleration")
	float SprintMaxAcceleration = 1000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Deceleration")
	float WalkBrakingDeceleration = 1000.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Deceleration")
	float RunBrakingDeceleration = 1000.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Deceleration")
	float SprintBrakingDeceleration = 1000.0f;

protected:
	/** 캐릭터 시스템이 완료되었을 때 실행될 초기화 함수 */
	void OnCharacterSystemReady();
	
	/**
	 * CMC 를 지연 캐싱해서 가져오는 함수.
	 * @param OutCMC 캐싱된 CMC 포인터를 참조로 전달. 이미 유효한 포인터가 있으면 그대로 반환, 그렇지 않으면 캐싱 시도 후 반환.
	 * @return 성공적으로 가져왔거나 이미 유효하면 true 반환
	 */
	bool GetCachedCMC(TWeakObjectPtr<UCharacterMovementComponent>& OutCMC);

	/**
	 * CBLocomotionProcessor 전용 내부 헬퍼 함수. ASC 를 지연 캐싱해서 가져오는 함수.
	 * @return ASC 를 지연 캐싱해서 가져오는 함수. 이미 유효한 포인터가 있으면 그대로 반환, 그렇지 않으면 캐싱 시도 후 반환.
	 */
	FORCEINLINE UCBAbilitySystemComponent* GetASC() { 
		UCBAbilitySystemLibrary::GetCBCachedASC(GetOwner(), CachedASC); 
		return CachedASC.Get(); 
	}
};
