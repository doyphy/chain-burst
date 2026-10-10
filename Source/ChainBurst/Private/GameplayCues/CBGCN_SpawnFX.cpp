// project
#include "GameplayCues/CBGCN_SpawnFX.h"
#include "Characters/CBBaseCharacter.h"

// engine
#include "Components/MeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "NiagaraFunctionLibrary.h"

namespace
{
	// MF_CB_TeleportDissolve 의 파라미터 이름 (머티리얼 함수와 반드시 같아야 함)
	const FName DissolveParamName(TEXT("Teleport_Dissolve"));
	const FName GlowParamName(TEXT("Teleport_Glow"));
	const FName VertexNoiseParamName(TEXT("Teleport_VertexNoise"));

	// 키를 지정한 보간·기울기로 추가 (기울기 = 진행률 1 당 값 변화량, 엔진 자동 기울기 대신 고정값)
	void AddCurveKey(FRichCurve& Curve, float Time, float Value, ERichCurveInterpMode InterpMode, float LeaveTangent = 0.f)
	{
		const FKeyHandle Handle = Curve.AddKey(Time, Value);
		Curve.SetKeyInterpMode(Handle, InterpMode);
		Curve.SetKeyTangentMode(Handle, RCTM_User);

		FRichCurveKey& Key = Curve.GetKey(Handle);
		Key.ArriveTangent = 0.f;
		Key.LeaveTangent = LeaveTangent;
	}
}

ACBGCN_SpawnFX::ACBGCN_SpawnFX()
{
	// 연출 중에만 틱을 켬
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// WhileActive 가 RPC 와 상태 복제 양쪽으로 와도 연출을 한 번만 시작함
	bAllowMultipleWhileActiveEvents = false;

	// 큐가 제거되면 액터를 정리함
	bAutoDestroyOnRemove = true;

	// 기본 커브 (생성 연출 = 나타남). 팩 데모(BP_Teleport_8) 타임라인의 나타남 구간(2.5~4.0초)을 진행률로 옮김.
	// Dissolve 는 -0.5(완전히 안 보임)에서 시작해 1(평소 값)로 끝남
	AddCurveKey(*DissolveCurve.GetRichCurve(), 0.f, -0.5f, RCIM_Cubic);
	AddCurveKey(*DissolveCurve.GetRichCurve(), 0.625f, 1.f, RCIM_Linear);
	AddCurveKey(*DissolveCurve.GetRichCurve(), 1.f, 1.f, RCIM_Linear);

	AddCurveKey(*GlowCurve.GetRichCurve(), 0.f, 0.844f, RCIM_Cubic, -0.9f);
	AddCurveKey(*GlowCurve.GetRichCurve(), 0.9375f, 0.f, RCIM_Linear);
	AddCurveKey(*GlowCurve.GetRichCurve(), 1.f, 0.f, RCIM_Linear);

	// 일렁임 항 (1-노이즈)^Pow 가 대부분 0 에 가까워 수백 단위 이상이어야 보임. 시작 기울기는 값과 같은 비율로 줄여 곡선 모양을 유지
	AddCurveKey(*VertexNoiseCurve.GetRichCurve(), 0.f, 500.f, RCIM_Cubic, -533.35f);
	AddCurveKey(*VertexNoiseCurve.GetRichCurve(), 0.9375f, 0.f, RCIM_Linear);
	AddCurveKey(*VertexNoiseCurve.GetRichCurve(), 1.f, 0.f, RCIM_Linear);
}

// 생성 연출 (캐릭터가 준비 완료 때 그 머신에서만 실행)
bool ACBGCN_SpawnFX::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// 실행형은 제거 이벤트가 오지 않으므로 연출이 끝나면 스스로 정리함
	bEndWhenFinished = true;
	return StartOnTarget(MyTarget);
}

// 소멸 연출 (기간 GE 의 큐가 상태로 복제되어 옴)
bool ACBGCN_SpawnFX::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	return StartOnTarget(MyTarget);
}

bool ACBGCN_SpawnFX::StartOnTarget(AActor* InTarget)
{
	ACBBaseCharacter* Character = Cast<ACBBaseCharacter>(InTarget);
	if (!Character) return false;

	TargetCharacter = Character;

	// 메시·머티리얼은 준비 완료 때 로드아웃이 적용함. 그 전이면 준비 완료를 기다렸다 시작
	if (Character->IsCharacterSystemReady())
	{
		BeginDissolve();
	}
	else
	{
		SystemReadyHandle = Character->OnCharacterSystemReadyDelegate.AddUObject(this, &ACBGCN_SpawnFX::BeginDissolve);
	}

	return true;
}

bool ACBGCN_SpawnFX::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// 연출 도중에 큐가 제거되면 끝 값으로 고정 (늦게 준비된 클라이언트 등)
	if (bIsDissolving)
	{
		FinishDissolve();
	}

	ResetState();
	return true;
}

bool ACBGCN_SpawnFX::Recycle()
{
	// 풀에 돌아갈 때 이전 대상의 상태가 남지 않게 비움
	ResetState();
	return Super::Recycle();
}

void ACBGCN_SpawnFX::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetState();
	Super::EndPlay(EndPlayReason);
}

void ACBGCN_SpawnFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsDissolving) return;

	// 진행률대로 머티리얼 값 갱신, 끝나면 고정
	ElapsedTime += DeltaSeconds;
	const float Alpha = FMath::Clamp(ElapsedTime / Duration, 0.f, 1.f);
	if (Alpha >= 1.f)
	{
		FinishDissolve();
		return;
	}

	ApplyProgress(Alpha);
}

void ACBGCN_SpawnFX::BeginDissolve()
{
	ACBBaseCharacter* Character = TargetCharacter.Get();
	if (!Character) return;

	// 준비 완료 대기 해제 (즉시 시작한 경우에는 핸들이 비어 있음)
	if (SystemReadyHandle.IsValid())
	{
		Character->OnCharacterSystemReadyDelegate.Remove(SystemReadyHandle);
		SystemReadyHandle.Reset();
	}

	// 첫 값을 바로 넣음. 준비 완료 방송과 같은 프레임이라 원래 모습이 한 프레임도 보이지 않음
	ElapsedTime = 0.f;
	bIsDissolving = true;
	ApplyProgress(0.f);
	SetActorTickEnabled(true);

	// 발밑(캡슐 바닥)에 나이아가라. 캡슐 크기는 준비 완료 시점에 로드아웃이 확정함
	if (NiagaraSystem)
	{
		const float HalfHeight = Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
		const FVector FootLocation = Character->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, NiagaraSystem, FootLocation, Character->GetActorRotation());
	}
}

void ACBGCN_SpawnFX::ApplyProgress(float InAlpha) const
{
	ACBBaseCharacter* Character = TargetCharacter.Get();
	if (!Character) return;

	const float Dissolve = DissolveCurve.GetRichCurveConst()->Eval(InAlpha);
	const float Glow = GlowCurve.GetRichCurveConst()->Eval(InAlpha);
	const float VertexNoise = VertexNoiseCurve.GetRichCurveConst()->Eval(InAlpha);

	// 캐릭터 본체·의상 파츠 메시와 붙어 있는 액터(무기)의 메시 전부. 무기는 늦게 붙을 수 있어 매번 다시 모음
	TArray<AActor*> Actors;
	Character->GetAttachedActors(Actors, true /*bResetArray*/, true /*bRecursivelyIncludeAttachedActors*/);
	Actors.Add(Character);

	for (const AActor* Actor : Actors)
	{
		Actor->ForEachComponent<UMeshComponent>(false, [&](UMeshComponent* Mesh)
		{
			// 파라미터가 있는 슬롯마다 다이나믹 머티리얼 인스턴스를 만들어 값을 넣음 (각 클라이언트 로컬, 없는 메시는 무시됨)
			Mesh->SetScalarParameterValueOnMaterials(DissolveParamName, Dissolve);
			Mesh->SetScalarParameterValueOnMaterials(GlowParamName, Glow);
			Mesh->SetScalarParameterValueOnMaterials(VertexNoiseParamName, VertexNoise);
		});
	}
}

void ACBGCN_SpawnFX::FinishDissolve()
{
	ApplyProgress(1.f);
	bIsDissolving = false;
	SetActorTickEnabled(false);

	// 실행형으로 시작했으면 스스로 풀로 돌아감 (기간 GE 로 시작한 경우는 큐 제거 때 정리됨)
	if (bEndWhenFinished)
	{
		GameplayCueFinishedCallback();
	}
}

void ACBGCN_SpawnFX::ResetState()
{
	// 준비 완료 대기 중이었으면 구독 해제
	if (ACBBaseCharacter* Character = TargetCharacter.Get())
	{
		if (SystemReadyHandle.IsValid())
		{
			Character->OnCharacterSystemReadyDelegate.Remove(SystemReadyHandle);
		}
	}

	SystemReadyHandle.Reset();
	TargetCharacter.Reset();
	bIsDissolving = false;
	bEndWhenFinished = false;
	ElapsedTime = 0.f;
	SetActorTickEnabled(false);
}
