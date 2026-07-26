#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "CBNoiseEmitterComponent.generated.h"

/**
 * [서버 전용] 이동 중인 캐릭터가 주기적으로 AI 청각용 소음(FAINoiseEvent)을 발생시키는 컴포넌트.
 * 개이트(Walk/Run/Sprint)에 따라 Loudness를 달리해 들리는 거리를 조절.
 * (유효 청취 거리 = 청취자의 HearingRange x Loudness — 정지 시에는 소음을 내지 않아 감지되지 않음)
 *
 * 애님 노티파이가 아닌 서버 타이머 방식이라, 서버에서 애님 틱이 스킵되어도 소음이 누락되지 않음.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CHAINBURST_API UCBNoiseEmitterComponent : public UCBExtensionComponent
{
	GENERATED_BODY()

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	/** [서버] 타이머로 호출되어 이동 중이면 소음을 1회 보고. */
	void ReportMovementNoise();

	/** 현재 개이트에 대응하는 Loudness를 반환. */
	float GetLoudnessForCurrentGait() const;

	/** 소음 보고 간격(초). 짧을수록 감지가 빨라지지만 이벤트 부하가 늘어남. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Noise", meta = (ClampMin = "0.05"))
	float NoiseInterval = 0.35f;

	/** 이 속도 미만이면 정지로 보고 소음을 내지 않음. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Noise", meta = (ClampMin = "0.0"))
	float MinSpeedToMakeNoise = 10.f;

	/** 걷기 소음 크기 (유효 청취 거리 = HearingRange x 이 값) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Noise", meta = (ClampMin = "0.0"))
	float WalkLoudness = 0.3f;

	/** 달리기 소음 크기 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Noise", meta = (ClampMin = "0.0"))
	float RunLoudness = 0.65f;

	/** 전력 질주 소음 크기 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Noise", meta = (ClampMin = "0.0"))
	float SprintLoudness = 1.f;

private:
	/** 소음 보고 반복 타이머 핸들 (서버에서만 유효) */
	FTimerHandle NoiseTimerHandle;
};
