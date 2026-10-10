#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "Curves/CurveFloat.h"
#include "CBGCN_SpawnFX.generated.h"

class ACBBaseCharacter;
class UNiagaraSystem;

/**
 * 캐릭터 생성·소멸 연출 큐 (기본 커브 = 생성 연출: 나타남. 커브를 반대로 두면 소멸 연출에도 씀).
 * 연출 방식은 텔레포트 디졸브 - 대상 캐릭터의 모든 메시(의상 파츠 포함)와 붙어 있는 액터(무기)의 텔레포트 머티리얼 파라미터(MF_CB_TeleportDissolve 의 Teleport_*)를
 * 커브대로 바꾸고, 시작할 때 발밑에 나이아가라를 띄움.
 * 파라미터 이름이 머티리얼 함수에 고정돼 있어, 그 함수가 연결된 머티리얼에서만 보임 (다른 방식의 연출이 필요하면 별도 큐로 둘 것).
 * 두 경로로 시작함:
 * - 실행(OnExecute): 생성 연출. 캐릭터가 준비 완료 때 그 머신에서만 실행함. 제거 이벤트가 없으므로 연출이 끝나면 스스로 정리함.
 * - WhileActive: 소멸 연출. 기간 GE 의 큐가 상태로 복제되어 옴 (늦게 받은 클라이언트에는 OnActive 없이 WhileActive 만 옴).
 *   메시·머티리얼은 로드아웃이 비동기로 적용하므로, 캐릭터 준비 완료 전에 큐가 오면 준비 완료를 기다렸다 시작함.
 * 연출이 끝나거나 큐가 먼저 제거되면 커브 끝 값으로 고정함.
 */
UCLASS(Abstract)
class CHAINBURST_API ACBGCN_SpawnFX : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	ACBGCN_SpawnFX();

	//~ Begin AGameplayCueNotify_Actor Interface
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;
	//~ End AGameplayCueNotify_Actor Interface

	//~ Begin AActor Interface
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

protected:
	/** 시작할 때 대상 발밑(캡슐 바닥)에 띄울 나이아가라. 비우면 머티리얼 연출만 함 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Teleport")
	TObjectPtr<UNiagaraSystem> NiagaraSystem = nullptr;

	/** 연출 길이(초). 소멸 연출 GE 기간은 이 값과 같게 둘 것 (GE 가 짧으면 연출이 잘림) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Teleport", meta = (ClampMin = "0.01"))
	float Duration = 1.6f;

	/** Teleport_Dissolve 커브 (가로축 = 진행률 0~1). 1 = 완전히 보임, 약 0.33 아래부터 줄무늬로 사라짐 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Teleport")
	FRuntimeFloatCurve DissolveCurve;

	/** Teleport_Glow 커브 (가로축 = 진행률 0~1). 노이즈 줄무늬의 발광 세기 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Teleport")
	FRuntimeFloatCurve GlowCurve;

	/** Teleport_VertexNoise 커브 (가로축 = 진행률 0~1). 노이즈만큼 정점을 위로 밀어 올리는 세기(cm) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Teleport")
	FRuntimeFloatCurve VertexNoiseCurve;

private:
	/**
	 * 대상 캐릭터를 잡고 연출을 시작하는 함수 (준비 전이면 준비 완료를 기다림).
	 * @return 대상이 캐릭터가 아니면 false
	 */
	bool StartOnTarget(AActor* InTarget);

	/** 캐릭터 준비 완료 후(또는 이미 준비된 상태면 즉시) 연출을 시작하는 함수 */
	void BeginDissolve();

	/** 진행률에 맞는 커브 값을 대상 캐릭터와 붙은 액터의 메시 머티리얼에 넣는 함수 */
	void ApplyProgress(float InAlpha) const;

	/** 연출을 끝 값으로 고정하고 틱을 멈추는 함수. 실행형으로 시작했으면 스스로 정리함 */
	void FinishDissolve();

	/** 준비 완료 대기·진행 상태를 비우는 함수 (제거·재활용·파괴 공용) */
	void ResetState();

	/** 연출 대상 캐릭터 */
	TWeakObjectPtr<ACBBaseCharacter> TargetCharacter;

	/** 캐릭터 준비 완료 델리게이트 구독 해제용 핸들 */
	FDelegateHandle SystemReadyHandle;

	/** 연출 시작 후 지난 시간(초) */
	float ElapsedTime = 0.f;

	/** 연출 진행 중인지 */
	bool bIsDissolving = false;

	/** 실행형으로 시작했는지 (제거 이벤트가 오지 않으므로 연출이 끝나면 스스로 정리해야 함) */
	bool bEndWhenFinished = false;
};
