// project
#include "UI/Widgets/CBHealthBarWidget.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/CBAttributeSet.h"

void UCBHealthBarWidget::InitializeWithASC(UCBAbilitySystemComponent* InASC)
{
	// ASC 유효성 검사
	if (!InASC) return;

	// 값 조회·재구독용 대상 캐시 (위젯이 화면에서 빠졌다 돌아와도 유지)
	CachedASC = InASC;

	// 구독 + 초기값 반영
	BindToASC();
}

void UCBHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 머리 위 바를 숨기면 위젯이 스크린 레이어에서 제거되며 NativeDestruct 호출되어 구독이 끊긴다.
	// 다시 붙을 때 여기서 재구독하고, 숨어 있는 동안 바뀐 값을 한 번에 반영함.
	BindToASC();
}

void UCBHealthBarWidget::NativeDestruct()
{
	// 슬레이트가 사라지는 동안은 구독을 끊음 (댕글링 방지). 대상 캐시는 남겨 재구성 때 다시 구독함
	UnbindFromASC();

	Super::NativeDestruct();
}

void UCBHealthBarWidget::BindToASC()
{
	// 대상이 아직 없거나 이미 사라졌으면 할 일 없음
	UCBAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC) return;

	// 재호출·재구성 대비: 기존 구독이 있으면 먼저 해제
	UnbindFromASC();

	// 체력 어트리뷰트 변경 델리게이트 구독 (복제 값 도착 시 클라이언트에서도 발화됨)
	CurrentHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCBAttributeSet::GetCurrentHealthAttribute())
		.AddUObject(this, &UCBHealthBarWidget::HandleHealthAttributeChanged);
	MaxHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCBAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UCBHealthBarWidget::HandleHealthAttributeChanged);

	// 구독 전에 이미 확정된 값 반영
	BroadcastHealthChanged();
}

void UCBHealthBarWidget::HandleHealthAttributeChanged(const FOnAttributeChangeData& /*Data*/)
{
	// Current/Max 어느 쪽이 바뀌어도 두 값을 함께 읽어 갱신
	BroadcastHealthChanged();
}

void UCBHealthBarWidget::BroadcastHealthChanged()
{
	// ASC가 유효하지 않으면 갱신하지 않음
	if (!CachedASC.IsValid()) return;

	// 현재 확정된 어트리뷰트 값을 읽어 BP 이벤트로 전달
	const float CurrentHealth = CachedASC->GetNumericAttribute(UCBAttributeSet::GetCurrentHealthAttribute());
	const float MaxHealth = CachedASC->GetNumericAttribute(UCBAttributeSet::GetMaxHealthAttribute());
	OnHealthChanged(CurrentHealth, MaxHealth);
}

void UCBHealthBarWidget::UnbindFromASC()
{
	// 구독 중이던 델리게이트 해제
	if (CachedASC.IsValid())
	{
		CachedASC->GetGameplayAttributeValueChangeDelegate(UCBAttributeSet::GetCurrentHealthAttribute()).Remove(CurrentHealthChangedHandle);
		CachedASC->GetGameplayAttributeValueChangeDelegate(UCBAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
	}

	// 핸들만 초기화 (CachedASC는 재구독 대상이므로 유지)
	CurrentHealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
}
