#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "CBSkillSlotWidget.generated.h"

class UCBAbilitySystemComponent;

/**
 * HUD 스킬 슬롯 하나의 공용 베이스.
 * 지정된 쿨다운 태그의 카운트 변화를 구독해 쿨다운 시작·종료를 감지하고,
 * 쿨다운 중에는 매 틱 활성 쿨다운 GE에서 남은 시간·전체 길이를 조회해 진행률을 계산.
 */
UCLASS(Abstract)
class CHAINBURST_API UCBSkillSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 대상 ASC의 쿨다운 태그에 구독하고 현재 쿨다운 상태를 반영하는 함수.
	 * 재호출 시 기존 구독을 먼저 해제하므로 여러 번 불려도 안전.
	 * @param InASC 쿨다운을 표시할 대상의 ASC
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|UI")
	void InitializeWithASC(UCBAbilitySystemComponent* InASC);

protected:
	//~ Begin UUserWidget Interface.
		/** 슬레이트가 화면에 붙을 때 호출되는 함수 */
	virtual void NativeConstruct() override;
		/** 슬레이트가 화면에서 제거될 때 호출되는 함수 */
	virtual void NativeDestruct() override;
		/** 매 프레임 호출되는 함수 (쿨다운 중에만 실제 작업 수행). */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget Interface.

	/** 쿨다운이 시작될 때 1회 호출되는 BP 이벤트. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnCooldownStarted();

	/**
	 * 쿨다운 진행 중 매 틱(및 시작 시 1회) 호출되는 BP 이벤트.
	 * @param Progress      쿨다운 진행률. 0 = 방금 시전, 1 = 완료
	 * @param RemainingTime 남은 쿨다운 시간(초)
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnCooldownProgress(float Progress, float RemainingTime);

	/** 쿨다운이 끝날 때 1회 호출되는 BP 이벤트. */
	UFUNCTION(BlueprintImplementableEvent, Category = "ChainBurst|UI")
	void OnCooldownEnded();

	/** 이 슬롯이 표시할 쿨다운 태그 (쿨다운 GE의 GrantedTags와 일치해야 함) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainBurst|UI", meta = (Categories = "Cooldown"))
	FGameplayTag CooldownTag;

private:
	/** 캐시된 대상 ASC에 구독하고 현재 쿨다운 상태를 반영하는 함수 (초기화·재구성 공용, 중복 구독 방지 포함) */
	void BindToASC();

	/** 쿨다운 태그 카운트 변경 델리게이트 구독만 해제하는 함수 (대상 캐시는 유지) */
	void UnbindFromASC();

	/** 쿨다운 태그 카운트 변경 델리게이트 내부 콜백 (카운트 유무로 쿨다운 활성 상태를 갱신) */
	void HandleCooldownTagChanged(const FGameplayTag InTag, int32 NewCount);

	/** 쿨다운 활성 상태를 전환. 활성화하면 쿨다운 시작, 비활성화하면 종료하는 함수 */
	void SetCooldownActive(bool bNewCooldownActive);

	/** 현재 남은 시간·전체 길이로 진행률을 계산해 OnCooldownProgress 이벤트를 발화하는 함수 */
	void BroadcastCooldownProgress();

	/**
	 * 활성 쿨다운 GE에서 남은 시간과 전체 지속시간을 조회하는 함수.
	 * @param InASC        조회할 대상 ASC
	 * @param OutRemaining 남은 시간(초)
	 * @param OutDuration  쿨다운 전체 지속시간(초). 무한 지속이면 0 이하
	 * @return 조회에 성공했으면 true. 해당하는 활성 쿨다운 GE가 없으면 false
	 */
	bool QueryCooldownTime(const UCBAbilitySystemComponent* InASC, float& OutRemaining, float& OutDuration) const;

	/** 값 조회·재구독용 대상 ASC 캐시. 위젯이 화면에서 빠졌다 돌아와도 유지 */
	TWeakObjectPtr<UCBAbilitySystemComponent> CachedASC;

	/** 쿨다운 태그 카운트 변경 델리게이트 구독 해제용 핸들 */
	FDelegateHandle CooldownTagChangedHandle;

	/** 현재 쿨다운 중인지 여부. 매 프레임 틱의 조기 반환 게이트 겸 시작·종료 전환 판정에 사용 */
	bool bIsOnCooldown = false;
};
