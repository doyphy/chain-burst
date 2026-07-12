// project
#include "AbilitySystem/Abilities/Movement/CBGAChangeSpeed.h"
#include "Characters/CBBaseCharacter.h"
#include "DataAssets/Movement/CBCharacterMovementData.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "CBGameplayTags.h"

// engine
#include "GameplayEffect.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

UCBGAChangeSpeed::UCBGAChangeSpeed()
{
	// 어빌리티 인스턴싱 정책 설정
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 입력(질주/걷기 홀드)으로 발동되므로 예측 실행이 기본값
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UCBGAChangeSpeed::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	ACBBaseCharacter* BaseChar = Cast<ACBBaseCharacter>(GetAvatarActorFromActorInfo());

	if (!ASC || !BaseChar || !MovementModifierGEClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 기존에 적용된 '이동 오버라이드' GE가 있다면 먼저 제거.
	// Status.Movement.Overridden 태그를 가진 모든 GE를 찾아서 제거.
	FGameplayTagContainer CleanUpContainer;
	CleanUpContainer.AddTag(CBGameplayTags::Status_Movement_Overridden);
	ASC->RemoveActiveEffectsWithGrantedTags(CleanUpContainer);

	// 데이터 에셋에서 태그에 맞는 속도 값 가져오기
	float TargetSpeed = BaseChar->GetMovementDataAsset()->GetSpeedForTag(SpeedDataTag);

	// GE Spec 생성
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(MovementModifierGEClass);
	if (SpecHandle.IsValid())
	{
		// SetByCaller를 통해 데이터 설정
		SpecHandle.Data.Get()->SetSetByCallerMagnitude(
			CBGameplayTags::Data_Movement_Speed, 
			TargetSpeed
		);

		// 캐릭터에 속도 태그 부여 (애니메이션 블루프린트에서 이 태그를 보고 애니메이션 상태를 변경할 수 있도록)
		SpecHandle.Data.Get()->DynamicGrantedTags.AddTag(SpeedDataTag);

		// 캐릭터에게 효과 적용 및 핸들 저장
		ActiveGEHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);

		// 가속도 소실 자동 종료 옵션이 켜져 있으면 감시 타이머 시작 (정지·피벗 잠금 시 속도 태그도 함께 해제되도록)
		if (bEndWhenNoAcceleration)
		{
			ZeroAccelStartTime = -1.0;
			GetWorld()->GetTimerManager().SetTimer(
				AccelCheckTimerHandle, this, &ThisClass::CheckAcceleration, 0.05f, true);
		}
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UCBGAChangeSpeed::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 가속도 감시 타이머 정리
	if (AccelCheckTimerHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AccelCheckTimerHandle);
		}
	}

	// 어빌리티 종료 시 적용했던 GE를 제거 (자동으로 기본 속도인 Run으로 복구됨)
	if (ActiveGEHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveActiveGameplayEffect(ActiveGEHandle);
		}
		// 핸들 무효화 (안전하게)
		ActiveGEHandle.Invalidate();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UCBGAChangeSpeed::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	if (IsActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UCBGAChangeSpeed::CheckAcceleration()
{
	// CMC 가져오기
	const ACBBaseCharacter* BaseChar = Cast<ACBBaseCharacter>(GetAvatarActorFromActorInfo());
	const UCharacterMovementComponent* CMC = BaseChar ? BaseChar->GetCharacterMovement() : nullptr;
	if (!CMC) return;

	// 가속(이동 입력)이 살아있으면 무가속 시작 시각 리셋
	// 0으로 초기화하면 무가속중인지 가속의 첫 시작 (0초) 인지 모르기 때문에 -1로 초기화
	if (CMC->GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER)
	{
		ZeroAccelStartTime = -1.0;
		return;
	}

	// 현재 시간 가져오기
	const double Now = GetWorld()->GetTimeSeconds();

	// 무가속시 시작 시간 기록
	if (ZeroAccelStartTime < 0.0)
	{
		ZeroAccelStartTime = Now;
		return;
	}

	// [현재 시간 - 무가속 시각 시간]이 NoAccelerationGraceTime 이상이면 어빌리티 종료
	if (Now - ZeroAccelStartTime >= NoAccelerationGraceTime)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}
