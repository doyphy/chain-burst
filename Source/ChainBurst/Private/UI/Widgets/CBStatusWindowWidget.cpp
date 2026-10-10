// project
#include "UI/Widgets/CBStatusWindowWidget.h"
#include "UI/Widgets/CBAttributeTextBlock.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "CBGameplayTags.h"

// engine
#include "Blueprint/WidgetTree.h"

void UCBStatusWindowWidget::InitializeWithASC(UCBAbilitySystemComponent* InASC)
{
	// ASC 유효성 검사
	if (!InASC) return;

	// 값 조회·재구독용 대상 캐시 (위젯이 화면에서 빠졌다 돌아와도 유지)
	CachedASC = InASC;

	// 구독 + 현재 상태 반영
	BindToASC();
}

void UCBStatusWindowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// WBP 에 배치된 어트리뷰트 텍스트 수집. 어트리뷰트를 고르지 않은 텍스트는 받을 값이 없으므로 제외.
	// 위젯 트리만 훑으므로 안에 넣은 다른 유저 위젯 속 텍스트는 잡히지 않음.
	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		UCBAttributeTextBlock* AttributeText = Cast<UCBAttributeTextBlock>(Widget);
		if (AttributeText && AttributeText->GetAttribute().IsValid())
		{
			AttributeTexts.Add(AttributeText);
		}
	});
}

void UCBStatusWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 화면에 붙을 때 재구독하고, 빠져 있는 동안 바뀐 값·열림 상태를 한 번에 반영함 (UCBHealthBarWidget 과 같은 계약)
	BindToASC();
}

void UCBStatusWindowWidget::NativeDestruct()
{
	// 슬레이트가 사라지는 동안은 구독을 끊음 (댕글링 방지). 대상 캐시는 남겨 재구성 때 다시 구독함
	UnbindFromASC();

	Super::NativeDestruct();
}

void UCBStatusWindowWidget::BindToASC()
{
	// 대상이 아직 없거나 이미 사라졌으면 할 일 없음
	UCBAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC) return;

	// 재호출·재구성 대비: 기존 구독이 있으면 먼저 해제
	UnbindFromASC();

	// 슬레이트가 아직 없으면 구독도 현재 상태 반영도 미룸 - 슬레이트가 생기면 NativeConstruct 가 다시 부름.
	// 구독이 슬레이트 수명(NativeDestruct 해제)과 짝을 이루고, 화면에 없는 위젯에 연출 이벤트가 가지 않게 함.
	if (!GetCachedWidget().IsValid()) return;

	for (UCBAttributeTextBlock* AttributeText : AttributeTexts)
	{
		const FGameplayAttribute& Attribute = AttributeText->GetAttribute();

		// 같은 어트리뷰트를 고른 텍스트가 여럿이어도 구독은 한 번 (콜백이 그 텍스트를 전부 갱신함)
		if (!AttributeChangedHandles.Contains(Attribute))
		{
			// 어트리뷰트 변경 델리게이트 구독 (복제 값 도착 시 클라이언트에서도 발화됨)
			AttributeChangedHandles.Add(Attribute, ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
				.AddUObject(this, &UCBStatusWindowWidget::HandleAttributeChanged));
		}

		// 구독 전에 이미 확정된 값 반영
		AttributeText->SetAttributeValue(ASC->GetNumericAttribute(Attribute));
	}

	// 열림 태그가 추가/제거될 때 호출되는 델리게이트 구독
	OpenTagChangedHandle = ASC->RegisterGameplayTagEvent(CBGameplayTags::Status_UI_StatusWindow, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UCBStatusWindowWidget::HandleOpenTagChanged);

	// 화면에 없던 동안 열리거나 닫혔을 수 있으므로 현재 태그 상태로 맞춤
	SetOpen(ASC->GetTagCount(CBGameplayTags::Status_UI_StatusWindow) > 0);
}

void UCBStatusWindowWidget::UnbindFromASC()
{
	// 구독 중이던 델리게이트 해제
	if (UCBAbilitySystemComponent* ASC = CachedASC.Get())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Pair : AttributeChangedHandles)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
		}

		if (OpenTagChangedHandle.IsValid())
		{
			ASC->UnregisterGameplayTagEvent(OpenTagChangedHandle, CBGameplayTags::Status_UI_StatusWindow, EGameplayTagEventType::NewOrRemoved);
		}
	}

	// 핸들만 초기화 (CachedASC는 재구독 대상이므로 유지)
	AttributeChangedHandles.Reset();
	OpenTagChangedHandle.Reset();
}

// 어트리뷰트 변경 콜백
void UCBStatusWindowWidget::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	// 바뀐 어트리뷰트를 고른 텍스트만 갱신
	for (UCBAttributeTextBlock* AttributeText : AttributeTexts)
	{
		if (AttributeText->GetAttribute() == Data.Attribute)
		{
			AttributeText->SetAttributeValue(Data.NewValue);
		}
	}
}

// 열림 태그 변경 콜백
void UCBStatusWindowWidget::HandleOpenTagChanged(const FGameplayTag /*InTag*/, int32 NewCount)
{
	// 태그가 붙어 있는 동안(= 상태창 어빌리티 활성 동안)이 열린 구간
	SetOpen(NewCount > 0);
}

void UCBStatusWindowWidget::SetOpen(bool bNewOpen)
{
	// 상태가 바뀌지 않으면 아무 일도 하지 않음.
	// 닫힌 채로 처음 구독할 때 닫힘 이벤트가 가지 않아야 함 - 역재생은 끝 지점(열린 모습)부터 시작해 한 번 열렸다 닫히는 것처럼 보임.
	if (bIsOpen == bNewOpen) return;

	bIsOpen = bNewOpen;

	if (bIsOpen)
	{
		OnStatusWindowOpened();
	}
	else
	{
		OnStatusWindowClosed();
	}
}
