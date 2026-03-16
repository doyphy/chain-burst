#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "CBCharacterRotationComponent.generated.h"

class ACBChaserCharacter;
class UCharacterMovementComponent;

UCLASS()
class CHAINBURST_API UCBCharacterRotationComponent : public UCBExtensionComponent
{
	GENERATED_BODY()
	
public:
	UCBCharacterRotationComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "References")
	TObjectPtr<ACBChaserCharacter> CachedCharacter;
	
	/** 회전 보간 속도 */
	UPROPERTY(EditDefaultsOnly, Category = "Rotation")
	float RotationInterpSpeed = 5.0f;

private:
	TObjectPtr<UCharacterMovementComponent> CachedMovementComp;
	
	UFUNCTION(Server, Reliable)
	void Server_SetTargetRotation(FRotator NewTargetRotation);

	void UpdateSmoothedTargetRotation(float DeltaTime);
	
	/** 목표 회전 (즉시 갱신) */
	UPROPERTY(ReplicatedUsing = OnRep_TargetRotation)
	FRotator TargetRotation = FRotator::ZeroRotator;

	UFUNCTION()
	void OnRep_TargetRotation();
	
	/** 실제 적용 회전 (MaxYawDeltaPerSecond로 속도 제한된 보간) */
	FRotator SmoothedTargetRotation = FRotator::ZeroRotator;
};
