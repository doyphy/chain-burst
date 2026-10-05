// project
#include "UI/Widgets/CBStunGaugeWidget.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/CBAttributeSet.h"

void UCBStunGaugeWidget::InitializeWithASC(UCBAbilitySystemComponent* InASC)
{
	// ASC 유효성 검사
	if (!InASC) return;

	// 값 조회·재구독용 대상 캐시 (위젯이 화면에서 빠졌다 돌아와도 유지)
	CachedASC = InASC;

	// 구독 + 초기값 반영
	BindToASC();
}

void UCBStunGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 화면에 붙을 때 재구독하고, 빠져 있는 동안 바뀐 값을 한 번에 반영함 (UCBHealthBarWidget 과 같은 계약)
	BindToASC();
}

void UCBStunGaugeWidget::NativeDestruct()
{
	// 슬레이트가 사라지는 동안은 구독을 끊음 (댕글링 방지). 대상 캐시는 남겨 재구성 때 다시 구독함
	UnbindFromASC();

	Super::NativeDestruct();
}

void UCBStunGaugeWidget::BindToASC()
{
	// 대상이 아직 없거나 이미 사라졌으면 할 일 없음
	UCBAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC) return;

	// 재호출·재구성 대비: 기존 구독이 있으면 먼저 해제
	UnbindFromASC();

	// 슬레이트가 아직 없으면 구독도 초기값 반영도 미룸 - 슬레이트가 생기면 NativeConstruct 가 다시 부름.
	// 슬레이트 생성 전에 프로그레스 바에 값을 넣으면 머터리얼에 반영되지 않고, 생성 후 같은 값은 무시되어 머터리얼이 기본값으로 굳음.
	// 보스 바는 스택에 올리기 전(슬레이트 없음)에 초기화됨 (UCBUIComponent::Local_CreateBossBarWidget).
	if (!GetCachedWidget().IsValid()) return;

	// 기절 게이지 어트리뷰트 변경 델리게이트 구독 (복제 값 도착 시 클라이언트에서도 발화됨)
	StunGaugeChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCBAttributeSet::GetStunGaugeAttribute())
		.AddUObject(this, &UCBStunGaugeWidget::HandleStunGaugeChanged);

	// 구독 전에 이미 확정된 값 반영
	BroadcastStunGaugeChanged();
}

void UCBStunGaugeWidget::HandleStunGaugeChanged(const FOnAttributeChangeData& /*Data*/)
{
	BroadcastStunGaugeChanged();
}

void UCBStunGaugeWidget::BroadcastStunGaugeChanged()
{
	// ASC가 유효하지 않으면 갱신하지 않음
	if (!CachedASC.IsValid()) return;

	// 현재 확정된 어트리뷰트 값을 읽어 BP 이벤트로 전달. 최대값은 상수
	const float CurrentGauge = CachedASC->GetNumericAttribute(UCBAttributeSet::GetStunGaugeAttribute());
	OnStunGaugeChanged(CurrentGauge, UCBAttributeSet::MaxStunGauge);
}

void UCBStunGaugeWidget::UnbindFromASC()
{
	// 구독 중이던 델리게이트 해제
	if (CachedASC.IsValid())
	{
		CachedASC->GetGameplayAttributeValueChangeDelegate(UCBAttributeSet::GetStunGaugeAttribute()).Remove(StunGaugeChangedHandle);
	}

	// 핸들만 초기화 (CachedASC는 재구독 대상이므로 유지)
	StunGaugeChangedHandle.Reset();
}
