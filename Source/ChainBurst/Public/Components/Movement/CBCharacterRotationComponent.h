#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "CBCharacterRotationComponent.generated.h"

class ACBChaserCharacter;
class UCharacterMovementComponent;

/**
 * 캐릭터의 회전을 처리하는 컴포넌트.
 * 로컬 플레이어의 입력에 따라 목표 회전을 설정하고, 서버에 이를 전송하여 모든 클라이언트에서 일관된 회전이 적용되도록 함.
 */
UCLASS()
class CHAINBURST_API UCBCharacterRotationComponent : public UCBExtensionComponent
{
	GENERATED_BODY()
	
public:
	UCBCharacterRotationComponent();

protected:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	UPROPERTY(Transient)
	TObjectPtr<ACBChaserCharacter> CachedCharacter;
	
	/** 회전 보간 속도 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Rotation")
	float RotationInterpSpeed = 5.0f;

private:
	UPROPERTY(Transient)
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

protected:
	/** 캐릭터 시스템이 완료되었을 때 실행될 초기화 함수 (Tick 활성화) */
	virtual void OnCharacterSystemReady() override;
	
	/**
	 * Character 를 지연 캐싱해서 가져오는 함수.
	 * @param OutCharacter 캐싱된 캐릭터 포인터를 참조로 전달. 이미 유효한 포인터가 있으면 그대로 반환, 그렇지 않으면 캐싱 시도 후 반환.
	 * @return 성공적으로 가져왔거나 이미 유효하면 true 반환.
	 */
	bool GetCachedCharacter(TObjectPtr<ACBChaserCharacter>& OutCharacter);
	
	/**
	 * CMC 를 지연 캐싱해서 가져오는 함수.
	 * @param OutCMC 캐싱된 CMC 포인터를 참조로 전달. 이미 유효한 포인터가 있으면 그대로 반환, 그렇지 않으면 캐싱 시도 후 반환.
	 * @return 성공적으로 가져왔거나 이미 유효하면 true 반환
	 */
	bool GetCachedCMC(TObjectPtr<UCharacterMovementComponent>& OutCMC);
};
