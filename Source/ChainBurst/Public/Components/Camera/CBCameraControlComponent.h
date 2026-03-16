#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "CBCameraControlComponent.generated.h"

class USpringArmComponent;
class UCameraComponent;
struct FInputActionValue;
class ACBChaserCharacter;

USTRUCT(BlueprintType)
struct FCBZoomConfig
{
	GENERATED_BODY()

	/** 최대 줌 인 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float MinLength = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	FVector MinSocketOffset = FVector(0.f, 50.f, 0.f); // 숄더뷰

	/** 최대 줌 아웃 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float MaxLength = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	FVector MaxSocketOffset = FVector(0.f, 0.f, 50.f); // 탑뷰
};

UCLASS()
class CHAINBURST_API UCBCameraControlComponent : public UCBExtensionComponent
{
	GENERATED_BODY()
	
public:
	UCBCameraControlComponent();

	virtual void BeginPlay() override;
	
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	void InitializeCamera(USpringArmComponent* InSpringArmComponent, UCameraComponent* InCameraComponent);
	
protected:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Camera|References")
	TObjectPtr<ACBChaserCharacter> CachedCharacter;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera|Components")
	TObjectPtr<USpringArmComponent> SpringArm;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera|Components")
	TObjectPtr<UCameraComponent> Camera;
	
	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TSubclassOf<UCameraShakeBase> HitShakeClass; // 피격 흔들림

	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TSubclassOf<UCameraShakeBase> AttackShakeClass; // 공격 흔들림

	/** [설정] 줌 설정 데이터 (Min/Max 길이 및 오프셋) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera|Config")
	FCBZoomConfig ZoomConfig;

	/** [설정] 휠 한 번 굴릴 때 변하는 비율 (0.1 = 10%) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Config")
	float ZoomStep = 0.1f;

	/** [설정] 줌 보간 속도 (높을수록 빠름) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Config")
	float ZoomInterpSpeed = 5.0f;
	
private:
	FVector DefaultSocketOffset = FVector(0.f, 0.f, 0.f);
	float DefaultTargetArmLength = 0.f;

	/** 현재 줌 비율 (0.0 = 최대 줌인, 1.0 = 최대 줌아웃) */
	float TargetZoomAlpha = 0.f;  // 목표 Alpha
	float CurrentZoomAlpha = 0.f; // 현재 Alpha (보간용)
	
public:
#pragma region Inputs
	void Input_Look(const FVector2D& InLookAxisVector);
	void Input_Camera_Zoom(const float& InWheelValue);
#pragma endregion
	
};
