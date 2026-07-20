// project
#include "UI/Widgets/CBHealthBarWidget.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/CBAttributeSet.h"

void UCBHealthBarWidget::InitializeWithASC(UCBAbilitySystemComponent* InASC)
{
	// ASC 유효성 검사
	if (!InASC) return;

	// 재호출 대비: 기존 구독이 있으면 먼저 해제
	UnbindFromASC();

	// 값 조회·구독 해제용 캐시
	CachedASC = InASC;

	// 체력 어트리뷰트 변경 델리게이트 구독 (복제 값 도착 시 클라이언트에서도 발화됨)
	CurrentHealthChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UCBAttributeSet::GetCurrentHealthAttribute())
		.AddUObject(this, &UCBHealthBarWidget::HandleHealthAttributeChanged);
	MaxHealthChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UCBAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UCBHealthBarWidget::HandleHealthAttributeChanged);

	// 초기값 반영 (구독 전에 이미 확정된 값 표시)
	BroadcastHealthChanged();
}

void UCBHealthBarWidget::NativeDestruct()
{
	// 위젯 파괴 시 델리게이트 구독 해제 (댕글링 방지)
	UnbindFromASC();

	Super::NativeDestruct();
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

	// 캐시·핸들 초기화
	CachedASC = nullptr;
	CurrentHealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
}
