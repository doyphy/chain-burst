// project
#include "Components/Perception/CBNoiseEmitterComponent.h"
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"
#include "Characters/CBBaseCharacter.h"

// engine
#include "Perception/AISense_Hearing.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

// [서버 전용] 소음 보고 타이머 시작. AI 퍼셉션이 서버에만 있으므로 클라에서는 보고하지 않음.
void UCBNoiseEmitterComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	// 서버가 아니면 타이머 시작하지 않음
	if (!Owner || !Owner->HasAuthority()) return;

	// 타이머 시작
	Owner->GetWorldTimerManager().SetTimer(
		NoiseTimerHandle, this, &UCBNoiseEmitterComponent::ReportMovementNoise, NoiseInterval, true);
}

void UCBNoiseEmitterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 타이머 정리
	if (const AActor* Owner = GetOwner())
	{
		Owner->GetWorldTimerManager().ClearTimer(NoiseTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

// [서버] 이동 중이면 개이트에 맞는 크기로 소음을 1회 보고.
void UCBNoiseEmitterComponent::ReportMovementNoise()
{
	const APawn* OwningPawn = GetOwningPawn();
	if (!OwningPawn) return;

	// 정지 상태면 소음 없음 (가만히 있으면 청각으로 감지되지 않음 — 의도된 스텔스 규칙)
	if (OwningPawn->GetVelocity().SizeSquared() < FMath::Square(MinSpeedToMakeNoise)) return;

	// 개이트별 소음 크기로 청각 이벤트 보고 (유효 청취 거리 = 청취자 HearingRange x Loudness)
	// MaxRange는 0으로 두어 청취자의 HearingRange 기준만 사용
	UAISense_Hearing::ReportNoiseEvent(
		this, OwningPawn->GetActorLocation(), GetLoudnessForCurrentGait(), GetOwner(), 0.f);
}

// 현재 개이트 태그에 대응하는 Loudness 반환
float UCBNoiseEmitterComponent::GetLoudnessForCurrentGait() const
{
	const ACBBaseCharacter* OwningCharacter = Cast<ACBBaseCharacter>(GetOwner());
	const UAbilitySystemComponent* ASC = OwningCharacter ? OwningCharacter->GetAbilitySystemComponent() : nullptr;

	// ASC가 없으면 기본 Run 취급 (라이브러리도 nullptr에 Run을 반환)
	const FGameplayTag GaitTag = UCBAbilitySystemLibrary::GetCurrentGaitTag(ASC);

	if (GaitTag == CBGameplayTags::Status_Movement_Gait_Sprint)
	{
		return SprintLoudness;
	}
	if (GaitTag == CBGameplayTags::Status_Movement_Gait_Walk)
	{
		return WalkLoudness;
	}
	return RunLoudness;
}
