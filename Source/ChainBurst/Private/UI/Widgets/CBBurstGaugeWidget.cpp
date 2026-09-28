// project
#include "UI/Widgets/CBBurstGaugeWidget.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "AbilitySystem/CBAttributeSet.h"
#include "CBGameplayTags.h"

void UCBBurstGaugeWidget::InitializeWithASC(UCBAbilitySystemComponent* InASC)
{
	// ASC 유효성 검사
	if (!InASC) return;

	// 값 조회·재구독용 대상 캐시 (위젯이 화면에서 빠졌다 돌아와도 유지)
	CachedASC = InASC;

	// 구독 + 현재 상태 반영
	BindToASC();
}

void UCBBurstGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 위젯이 화면에서 빠지면 NativeDestruct 가 구독을 끊으므로, 다시 붙을 때 여기서 재구독하고 그사이 바뀐 상태를 반영함.
	BindToASC();
}

void UCBBurstGaugeWidget::NativeDestruct()
{
	// 슬레이트가 사라지는 동안은 구독을 끊음 (댕글링 방지). 대상 캐시는 남겨 재구성 때 다시 구독함
	UnbindFromASC();

	Super::NativeDestruct();
}

void UCBBurstGaugeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 버스트 중이 아니면 할 일 없음
	if (!bIsBurstActive) return;

	// 버스트 남은 시간 갱신
	BroadcastBurstProgress();
}

void UCBBurstGaugeWidget::BindToASC()
{
	// 대상이 아직 없거나 이미 사라졌으면 할 일 없음
	UCBAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC) return;

	// 재호출·재구성 대비: 기존 구독이 있으면 먼저 해제
	UnbindFromASC();

	// 게이지 어트리뷰트 변경 델리게이트 구독 (복제 값 도착 시 클라이언트에서도 발화됨)
	BurstGaugeChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UCBAttributeSet::GetBurstGaugeAttribute())
		.AddUObject(this, &UCBBurstGaugeWidget::HandleBurstGaugeChanged);

	// 버스트 상태 태그가 추가/제거될 때 호출되는 델리게이트 구독
	BurstTagChangedHandle = ASC->RegisterGameplayTagEvent(CBGameplayTags::Status_Combat_Burst, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UCBBurstGaugeWidget::HandleBurstTagChanged);

	// 슬레이트가 아직 없으면 현재 상태 반영을 미룸 (UCBHealthBarWidget 과 같은 이유).
	if (!GetCachedWidget().IsValid()) return;

	// 화면에 없던 동안 버스트가 시작·종료됐을 수 있으므로 현재 태그 상태로 모드를 맞춤
	const bool bBurstNow = ASC->GetTagCount(CBGameplayTags::Status_Combat_Burst) > 0;
	if (bBurstNow != bIsBurstActive)
	{
		SetBurstActive(bBurstNow);
		return;
	}

	// 모드가 그대로면 현재 값만 다시 보냄
	if (bIsBurstActive)
	{
		BroadcastBurstProgress();
	}
	else
	{
		BroadcastBurstGaugeChanged();
	}
}

void UCBBurstGaugeWidget::UnbindFromASC()
{
	// 구독 중이던 델리게이트 해제
	if (UCBAbilitySystemComponent* ASC = CachedASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UCBAttributeSet::GetBurstGaugeAttribute()).Remove(BurstGaugeChangedHandle);

		if (BurstTagChangedHandle.IsValid())
		{
			ASC->UnregisterGameplayTagEvent(BurstTagChangedHandle, CBGameplayTags::Status_Combat_Burst, EGameplayTagEventType::NewOrRemoved);
		}
	}

	// 핸들만 초기화 (CachedASC는 재구독 대상이므로 유지)
	BurstGaugeChangedHandle.Reset();
	BurstTagChangedHandle.Reset();
}

// 게이지 어트리뷰트 변경 콜백
void UCBBurstGaugeWidget::HandleBurstGaugeChanged(const FOnAttributeChangeData& /*Data*/)
{
	// 버스트 중에는 남은 시간을 표시하므로 게이지 변경을 보내지 않음.
	// 서버의 게이지 소모(→ 0)가 예측된 버스트 시작보다 늦게 도착해도 표시를 덮어쓰지 않음.
	if (bIsBurstActive) return;

	// 버스트 게이지 변경 방송
	BroadcastBurstGaugeChanged();
}

// 버스트 태그 변경 콜백
void UCBBurstGaugeWidget::HandleBurstTagChanged(const FGameplayTag /*InTag*/, int32 NewCount)
{
	// 태그가 붙어 있는 동안이 버스트 구간
	SetBurstActive(NewCount > 0);
}

void UCBBurstGaugeWidget::SetBurstActive(bool bNewBurstActive)
{
	// 상태가 바뀌지 않으면 아무 일도 하지 않음
	if (bIsBurstActive == bNewBurstActive) return;

	bIsBurstActive = bNewBurstActive;

	if (bIsBurstActive)
	{
		// 발동 가능 여부 갱신 (버스트 중에는 발동 불가)
		UpdateBurstReady();

		// 버스트 시작 BP 이벤트 + 남은 시간 즉시 반영
		OnBurstStarted();
		BroadcastBurstProgress();
	}
	else
	{
		// 버스트 종료 BP 이벤트 + 충전 모드로 돌아가며 현재 게이지 반영.
		// 서버 거부로 예측 버스트가 롤백된 경우에도 여기서 가득 찬 게이지로 되돌아감.
		OnBurstEnded();
		BroadcastBurstGaugeChanged();
	}
}

// 버스트 게이지 변경 방송
void UCBBurstGaugeWidget::BroadcastBurstGaugeChanged()
{
	// ASC가 유효하지 않으면 갱신하지 않음
	const UCBAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC) return;

	// 현재 버스트 게이지 값을 읽어 이벤트로 전달
	const float CurrentGauge = ASC->GetNumericAttribute(UCBAttributeSet::GetBurstGaugeAttribute());
	OnBurstGaugeChanged(CurrentGauge, UCBAttributeSet::MaxBurstGauge);

	// 발동 가능 여부 갱신
	UpdateBurstReady();
}

// 발동 가능 여부를 다시 계산해 바뀌었을 때만 방송
void UCBBurstGaugeWidget::UpdateBurstReady()
{
	const UCBAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC) return;

	// 게이지가 가득 차 있어도 버스트 중이면 발동 불가. (서버 지연 고려)
	const bool bNewReady = !bIsBurstActive
		&& ASC->GetNumericAttribute(UCBAttributeSet::GetBurstGaugeAttribute()) >= UCBAttributeSet::MaxBurstGauge;

	if (bIsBurstReady == bNewReady) return;

	bIsBurstReady = bNewReady;
	OnBurstReadyChanged.Broadcast(bIsBurstReady);
}

void UCBBurstGaugeWidget::BroadcastBurstProgress()
{
	// ASC가 유효하지 않으면 갱신하지 않음
	const UCBAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC) return;

	float Remaining = 0.f;
	float Duration = 0.f;

	// 버스트 GE 에서 지속 시간 값 가져오기
	if (!QueryBurstTime(ASC, Remaining, Duration)) return;

	// 무한 지속(Duration <= 0)은 비율을 만들 수 없으므로 1로 고정.
	// 남은 비율 = 남은 시간 / 전체 지속시간, 0~1 범위로 클램프 (방금 발동:1, 종료:0)
	const float RemainingRatio = (Duration > 0.f) ? FMath::Clamp(Remaining / Duration, 0.f, 1.f) : 1.f;

	OnBurstProgress(RemainingRatio, Remaining);
}

bool UCBBurstGaugeWidget::QueryBurstTime(const UCBAbilitySystemComponent* InASC, float& OutRemaining, float& OutDuration) const
{
	// 버스트 상태 태그를 부여하는 GE를 찾도록 쿼리 생성
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CBGameplayTags::Status_Combat_Burst));
	// 쿼리에 해당하는 활성 GE들의 남은 시간과 전체 지속시간 가져오기
	const TArray<TPair<float, float>> Times = InASC->GetActiveEffectsTimeRemainingAndDuration(Query);

	// 버스트 GE가 없으면 실패
	if (Times.Num() == 0) return false;

	// 가장 긴 남은 시간을 가진 GE 선택.
	// 서버 승인 직후 예측 사본과 복제된 사본이 잠깐 함께 있을 수 있음.
	int32 LongestIndex = 0;
	for (int32 Index = 1; Index < Times.Num(); ++Index)
	{
		if (Times[Index].Key > Times[LongestIndex].Key)
		{
			LongestIndex = Index;
		}
	}

	OutRemaining = Times[LongestIndex].Key;
	OutDuration = Times[LongestIndex].Value;
	return true;
}
