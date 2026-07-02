// project
#include "AbilitySystem/CBAttributeSet.h"
#include "Characters/CBBaseCharacter.h"
#include "DataAssets/Movement/CBCharacterMovementData.h"
#include "CBGameplayTags.h"

// engine
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemBlueprintLibrary.h"

UCBAttributeSet::UCBAttributeSet()
{
	// 기본값 설정
	InitCurrentHealth(1.f);
	InitMaxHealth(1.f);
	InitAttackPower(1.f);
	InitDefensePower(1.f);
	InitAttackSpeed(1.f);
}

// [서버/클라] 어트리뷰트 값이 최종 확정된 후 호출.
void UCBAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMovementSpeedAttribute())
	{
		UpdateMovementSpeed(NewValue);
	}
}

// [서버] GE가 어트리뷰트에 적용된 직후 호출.
void UCBAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// 체력이 변경된 경우에만 처리
	if (Data.EvaluatedData.Attribute == GetCurrentHealthAttribute())
	{
		// 체력 클램프 (데미지 원인과 무관하게 항상 0 ~ MaxHealth 보장)
		SetCurrentHealth(FMath::Clamp(GetCurrentHealth(), 0.f, GetMaxHealth()));

		// 데미지(음수 변화)가 아니면 피격 반응 처리 안 함 (회복 등)
		if (Data.EvaluatedData.Magnitude >= 0.f) return;

		// 사망(체력 0)이면 피격 반응 처리 안 함 (사망 연출은 별도)
		if (GetCurrentHealth() <= 0.f) return;

		// 이 GE가 "피격 반응을 유발"한다고 선언했는지 확인 (Opt-in)
		// 지속 데미지(DoT)/환경 데미지 등 Effect.HitReact 태그가 없는 GE는 체력만 깎고 반응 X
		FGameplayTagContainer AssetTags;
		Data.EffectSpec.GetAllAssetTags(AssetTags);
		if (!AssetTags.HasTagExact(CBGameplayTags::Effect_HitReact)) return;

		// 피격자에게 피격 반응 이벤트 발행 (이 함수는 서버에서만 실행됨)
		if (UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent())
		{
			if (AActor* TargetActor = TargetASC->GetAvatarActor())
			{
				FGameplayEventData Payload;
				Payload.EventTag = CBGameplayTags::Event_Combat_HitReact;
				Payload.Instigator = Data.EffectSpec.GetContext().GetInstigator();
				Payload.Target = TargetActor;

				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
					TargetActor, CBGameplayTags::Event_Combat_HitReact, Payload);
			}
		}
	}
}

void UCBAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UCBAttributeSet, MovementSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCBAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCBAttributeSet, CurrentHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCBAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCBAttributeSet, DefensePower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCBAttributeSet, AttackSpeed, COND_None, REPNOTIFY_Always);
}

void UCBAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCBAttributeSet, MovementSpeed, OldMovementSpeed);
}

void UCBAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCBAttributeSet, MaxHealth, OldMaxHealth);
}

void UCBAttributeSet::OnRep_CurrentHealth(const FGameplayAttributeData& OldCurrentHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCBAttributeSet, CurrentHealth, OldCurrentHealth);
}

void UCBAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCBAttributeSet, AttackPower, OldAttackPower);
}

void UCBAttributeSet::OnRep_DefensePower(const FGameplayAttributeData& OldDefensePower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCBAttributeSet, DefensePower, OldDefensePower);
}

void UCBAttributeSet::OnRep_AttackSpeed(const FGameplayAttributeData& OldAttackSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCBAttributeSet, AttackSpeed, OldAttackSpeed);
}

void UCBAttributeSet::UpdateMovementSpeed(float NewValue)
{
	// ASC 가져오기
	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		// 캐릭터 가져오기
		if (ACBBaseCharacter* OwnerCharacter = Cast<ACBBaseCharacter>(ASC->GetAvatarActor()))
		{
			OwnerCharacter->OnMovementSpeedChanged(NewValue);
		}
	}
}

void UCBAttributeSet::OnCharacterSystemReady()
{
	// ASC 가져오기
	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		// 캐릭터 가져오기
		if (ACBBaseCharacter* OwnerCharacter = Cast<ACBBaseCharacter>(ASC->GetAvatarActor()))
		{
			// 이동 데이터 에셋 가져오기
			if (UCBCharacterMovementData* MovementData = OwnerCharacter->GetMovementDataAsset())
			{
				// 이동 속도 설정
				float InitialSpeed = MovementData->GetSpeedForTag(CBGameplayTags::Movement_Run);
				SetMovementSpeed(InitialSpeed);
			}
		}
	}
}
