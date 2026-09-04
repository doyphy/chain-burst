// project
#include "UI/Widgets/CBSkillSlotWidget.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"

void UCBSkillSlotWidget::InitializeWithASC(UCBAbilitySystemComponent* InASC)
{
	// ASC 유효성 검사
	if (!InASC) return;

	// 값 조회·재구독용 대상 캐시
	CachedASC = InASC;

	// 구독 + 현재 쿨다운 상태 반영
	BindToASC();
}

void UCBSkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 위젯이 화면에서 빠지면 NativeDestruct가 구독을 끊으므로, 다시 붙을 때 여기서 재구독함.
	BindToASC();
}

void UCBSkillSlotWidget::NativeDestruct()
{
	// 슬레이트가 사라지는 동안은 구독을 끊음 (댕글링 방지). 대상 캐시는 남겨 재구성 때 다시 구독함
	UnbindFromASC();

	Super::NativeDestruct();
}

void UCBSkillSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 쿨다운 중이 아니면 할 일 없음.
	if (!bIsOnCooldown) return;

	// 쿨다운 진행률 갱신
	BroadcastCooldownProgress();
}

void UCBSkillSlotWidget::BindToASC()
{
	// 대상이 아직 없거나 이미 사라졌으면 할 일 없음
	UCBAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC) return;

	// 표시할 쿨다운 태그가 지정되지 않으면 구독할 대상이 없음.
	if (!CooldownTag.IsValid())
	{
		return;
	}

	// 재호출·재구성 대비: 기존 구독이 있으면 먼저 해제
	UnbindFromASC();

	// 쿨다운 태그가 추가/제거 될 때 호출되는 델리게이트를 구독
	CooldownTagChangedHandle = ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UCBSkillSlotWidget::HandleCooldownTagChanged);

	// 구독 전에 이미 쿨다운이 진행 중일 수 있으므로, 현재 태그 카운트로 상태를 갱신
	SetCooldownActive(ASC->GetTagCount(CooldownTag) > 0);
}

void UCBSkillSlotWidget::UnbindFromASC()
{
	// 구독 중이던 델리게이트 해제
	if (CachedASC.IsValid() && CooldownTagChangedHandle.IsValid())
	{
		CachedASC->UnregisterGameplayTagEvent(CooldownTagChangedHandle, CooldownTag, EGameplayTagEventType::NewOrRemoved);
	}

	// 핸들만 초기화 (CachedASC는 재구독 대상이므로 유지)
	CooldownTagChangedHandle.Reset();
}

void UCBSkillSlotWidget::HandleCooldownTagChanged(const FGameplayTag /*InTag*/, int32 NewCount)
{
	// 태그가 붙어 있는 동안이 쿨다운 구간
	SetCooldownActive(NewCount > 0);
}

void UCBSkillSlotWidget::SetCooldownActive(bool bNewCooldownActive)
{
	// 상태가 바뀌지 않으면 아무 일도 하지 않음
	if (bIsOnCooldown == bNewCooldownActive) return;

	bIsOnCooldown = bNewCooldownActive;

	// 쿨다운 시작/종료
	if (bIsOnCooldown)
	{
		// 쿨다운 시작 BP 이벤트
		OnCooldownStarted();

		// 쿨다운 진행률 갱신
		BroadcastCooldownProgress();
	}
	else
	{
		// 쿨다운 종료 BP 이벤트
		OnCooldownEnded();
	}
}

// 쿨다운 진행률을 계산해 BP 이벤트를 호출 (매 틱 호출)
void UCBSkillSlotWidget::BroadcastCooldownProgress()
{
	// ASC가 유효하지 않으면 갱신하지 않음
	const UCBAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC) return;

	float Remaining = 0.f;
	float Duration = 0.f;

	// 남은 시간과 전체 지속시간을 조회.
	if (!QueryCooldownTime(ASC, Remaining, Duration)) return;

	// 무한 지속(Duration <= 0)은 진행률을 만들 수 없으므로 0으로 고정해.
	// 진행률 = 1 - (남은 시간 / 전체 지속시간), 0~1 범위로 클램프 (완료:1, 방금 시전:0)
	const float Progress = (Duration > 0.f) ? FMath::Clamp(1.f - (Remaining / Duration), 0.f, 1.f) : 0.f;

	// BP 이벤트 호출
	OnCooldownProgress(Progress, Remaining);
}

// 활성 쿨다운 GE에서 남은 시간과 전체 지속시간을 조회
bool UCBSkillSlotWidget::QueryCooldownTime(const UCBAbilitySystemComponent* InASC, float& OutRemaining, float& OutDuration) const
{
	// 쿨다운 태그가 있는 GE를 찾도록 쿼리 생성
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CooldownTag));
	// 쿼리에 해당하는 활성 GE들의 남은 시간과 전체 지속시간 가져오기.
	const TArray<TPair<float, float>> Times = InASC->GetActiveEffectsTimeRemainingAndDuration(Query);

	// 쿨다운 GE가 없으면 실패
	if (Times.Num() == 0) return false;

	// 가장 긴 남은 시간을 가진 GE 찾기 (쿨다운이 중첩될 수 있으므로)
	int32 LongestIndex = 0;
	for (int32 Index = 1; Index < Times.Num(); ++Index)
	{
		if (Times[Index].Key > Times[LongestIndex].Key)
		{
			LongestIndex = Index;
		}
	}

	// 가장 긴 남은 시간과 전체 지속시간을 반환
	OutRemaining = Times[LongestIndex].Key;
	OutDuration = Times[LongestIndex].Value;
	return true;
}
