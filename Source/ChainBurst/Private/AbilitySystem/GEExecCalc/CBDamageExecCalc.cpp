// project
#include "AbilitySystem/GEExecCalc/CBDamageExecCalc.h"
#include "AbilitySystem/CBAttributeSet.h"
#include "CBGameplayTags.h"

struct FCBDamageStatics
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(AttackPower)
	DECLARE_ATTRIBUTE_CAPTUREDEF(DefensePower)
	DECLARE_ATTRIBUTE_CAPTUREDEF(CurrentHealth)

	FCBDamageStatics()
	{
		// 공격자 AttackPower — GE 생성 시점 스냅샷
		DEFINE_ATTRIBUTE_CAPTUREDEF(UCBAttributeSet, AttackPower, Source, true)
		// 타겟 DefensePower / CurrentHealth — 적중 시점 실시간 값
		DEFINE_ATTRIBUTE_CAPTUREDEF(UCBAttributeSet, DefensePower, Target, false)
		DEFINE_ATTRIBUTE_CAPTUREDEF(UCBAttributeSet, CurrentHealth, Target, false)
	}
};

static const FCBDamageStatics& DamageStatics()
{
	static FCBDamageStatics Statics;
	return Statics;
}

UCBDamageExecCalc::UCBDamageExecCalc()
{
	RelevantAttributesToCapture.Add(DamageStatics().AttackPowerDef);
	RelevantAttributesToCapture.Add(DamageStatics().DefensePowerDef);
	RelevantAttributesToCapture.Add(DamageStatics().CurrentHealthDef);
}

void UCBDamageExecCalc::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
                                                FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	// GE Spec과 어트리뷰트 평가 파라미터 가져오기
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const FAggregatorEvaluateParameters EvaluationParams;

	// 캡처된 어트리뷰트 값 추출
	float AttackPower = 0.f;
	float DefensePower = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().AttackPowerDef, EvaluationParams, AttackPower);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().DefensePowerDef, EvaluationParams, DefensePower);

	// 어빌리티에서 SetByCaller로 넘긴 데미지 계수 가져오기 (미설정 시 기본값 1.0)
	const float Coefficient = Spec.GetSetByCallerMagnitude(CBGameplayTags::Data_Damage_Coefficient, false, 1.f);

	// 최종 데미지 계산: 공격력 * 계수 - 방어력, 최소 1 보장
	const float FinalDamage = FMath::Max(AttackPower * Coefficient - DefensePower, 1.f);

	// CurrentHealth에 데미지 적용 (Additive로 음수값 적용)
	OutExecutionOutput.AddOutputModifier(
		FGameplayModifierEvaluatedData(
			DamageStatics().CurrentHealthProperty,
			EGameplayModOp::Additive,
			-FinalDamage));
}
