// project
#include "UI/Widgets/CBArenaWaitingWidget.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "CBGameplayTags.h"

// engine
#include "GameplayEffect.h"

void UCBArenaWaitingWidget::InitializeWithASC(UCBAbilitySystemComponent* InASC)
{
	// ASC 유효성 검사
	if (!InASC) return;

	// 재구독용 대상 캐시 (위젯이 화면에서 빠졌다 돌아와도 유지)
	CachedASC = InASC;

	// 구독 + 현재 상태 반영
	BindToASC();
}

void UCBArenaWaitingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 화면에 붙을 때 재구독하고, 빠져 있는 동안 바뀐 상태를 반영함 (UCBHealthBarWidget 과 같은 계약)
	BindToASC();
}

void UCBArenaWaitingWidget::NativeDestruct()
{
	// 슬레이트가 사라지는 동안은 구독을 끊음 (댕글링 방지). 대상 캐시는 남겨 재구성 때 다시 구독함
	UnbindFromASC();

	Super::NativeDestruct();
}

void UCBArenaWaitingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 카운트다운 중이 아니면 할 일 없음
	if (!bIsCountingDown) return;

	// 남은 시간 갱신
	BroadcastCountdownProgress();
}

void UCBArenaWaitingWidget::BindToASC()
{
	// 대상이 아직 없거나 이미 사라졌으면 할 일 없음
	UCBAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC) return;

	// 재호출·재구성 대비: 기존 구독이 있으면 먼저 해제
	UnbindFromASC();

	// 슬레이트가 아직 없으면 구독도 현재 상태 반영도 미룸 - 슬레이트가 생기면 NativeConstruct 가 다시 부름
	if (!GetCachedWidget().IsValid()) return;

	// 대기 태그가 추가/제거될 때 호출되는 델리게이트 구독 (복제로 받은 태그 변화에도 발화됨)
	WaitingTagChangedHandle = ASC->RegisterGameplayTagEvent(CBGameplayTags::Status_Arena_Waiting, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UCBArenaWaitingWidget::HandleWaitingTagChanged);

	// 카운트다운 태그(카운트다운 GE 가 부여)가 추가/제거될 때 호출되는 델리게이트 구독
	CountdownTagChangedHandle = ASC->RegisterGameplayTagEvent(CBGameplayTags::Status_Arena_Countdown, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UCBArenaWaitingWidget::HandleCountdownTagChanged);

	// 구독 전에 이미 붙어 있었거나(리스폰으로 HUD 가 새로 생김) 화면에 없던 동안 바뀌었을 수 있으므로 현재 상태로 맞춤
	OnWaitingChanged(ASC->GetTagCount(CBGameplayTags::Status_Arena_Waiting) > 0);
	SetCountingDown(ASC->GetTagCount(CBGameplayTags::Status_Arena_Countdown) > 0);
}

void UCBArenaWaitingWidget::UnbindFromASC()
{
	// 구독 중이던 델리게이트 해제
	if (UCBAbilitySystemComponent* ASC = CachedASC.Get())
	{
		if (WaitingTagChangedHandle.IsValid())
		{
			ASC->UnregisterGameplayTagEvent(WaitingTagChangedHandle, CBGameplayTags::Status_Arena_Waiting, EGameplayTagEventType::NewOrRemoved);
		}

		if (CountdownTagChangedHandle.IsValid())
		{
			ASC->UnregisterGameplayTagEvent(CountdownTagChangedHandle, CBGameplayTags::Status_Arena_Countdown, EGameplayTagEventType::NewOrRemoved);
		}
	}

	// 핸들만 초기화 (CachedASC는 재구독 대상이므로 유지)
	WaitingTagChangedHandle.Reset();
	CountdownTagChangedHandle.Reset();
}

// 대기 태그 변경 콜백
void UCBArenaWaitingWidget::HandleWaitingTagChanged(const FGameplayTag /*InTag*/, int32 NewCount)
{
	OnWaitingChanged(NewCount > 0);
}

// 카운트다운 태그 변경 콜백
void UCBArenaWaitingWidget::HandleCountdownTagChanged(const FGameplayTag /*InTag*/, int32 NewCount)
{
	// 태그가 붙어 있는 동안(= 카운트다운 GE 가 살아 있는 동안)이 카운트다운 구간
	SetCountingDown(NewCount > 0);
}

void UCBArenaWaitingWidget::SetCountingDown(bool bNewCountingDown)
{
	// 상태가 바뀌지 않으면 아무 일도 하지 않음 (카운트다운이 없는 평소에 종료 이벤트가 가지 않게)
	if (bIsCountingDown == bNewCountingDown) return;

	bIsCountingDown = bNewCountingDown;

	if (bIsCountingDown)
	{
		// 시작 BP 이벤트 + 남은 시간 즉시 반영 (다음 틱까지 빈 숫자가 보이지 않게)
		OnCountdownStarted();
		BroadcastCountdownProgress();
	}
	else
	{
		OnCountdownEnded();
	}
}

void UCBArenaWaitingWidget::BroadcastCountdownProgress()
{
	const UCBAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC) return;

	// 카운트다운 태그를 부여하는 GE 의 남은 시간. 서버가 기간·시작 시각을 복제하므로 클라는 읽기만 함
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CBGameplayTags::Status_Arena_Countdown));
	const TArray<float> RemainingTimes = ASC->GetActiveEffectsTimeRemaining(Query);
	if (RemainingTimes.IsEmpty()) return;

	// 보스 구역은 걷어낸 뒤에만 다시 걸므로 플레이어당 하나뿐
	OnCountdownProgress(FMath::Max(RemainingTimes[0], 0.f));
}
