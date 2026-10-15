#pragma once

#include "CoreMinimal.h"
#include "Characters/CBAICharacter.h"
#include "CBOutlawCharacter.generated.h"

class UCBCombatComponent;
class UCBOutlawCombatComponent;
class UCBOutlawLoadout;
class UCBAILoadout;
class USoundBase;

/**
 * Outlaw(보스·엘리트) AI 캐릭터. 컴뱃 컴포넌트를 갖고 Outlaw 컨트롤러가 빙의함.
 * 로드아웃에 보스 BGM 이 있으면 각 머신에서 준비 완료(소환 = 등장 연출) 때 레벨 BGM 대신 틀고, 사망·파괴 때 되돌림.
 */
UCLASS()
class CHAINBURST_API ACBOutlawCharacter : public ACBAICharacter
{
	GENERATED_BODY()

public:
	ACBOutlawCharacter();

protected:
	//~ Begin AActor Interface
	/** 보스 BGM 을 해제함 (전멸 리셋처럼 죽지 않고 파괴될 때만 - 레벨 종료·이동은 BGM 이 월드와 함께 정지됨). */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	//~ Begin ACBBaseCharacter Interface
	/** 준비 완료 - 보스 BGM 을 틂 (각 머신). */
	virtual void HandleCharacterSystemReady() override;
	/** 사망 로컬 정리 - 보스 BGM 을 해제함 (각 머신). */
	virtual void Local_HandleDeath() override;
	//~ End ACBBaseCharacter Interface

	//~ Begin ACBAICharacter Interface
	virtual TSoftObjectPtr<UCBAILoadout> GetAILoadout() const override;
	//~ End ACBAICharacter Interface

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|Combat")
	TObjectPtr<UCBOutlawCombatComponent> OutlawCombatComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChainBurst|CharacterData")
	TSoftObjectPtr<UCBOutlawLoadout> OutlawLoadout = nullptr;

public:
	//~ Begin ICBCombatInterface Interface.
	virtual UCBCombatComponent* GetCBCombatComponent() const override;
	//~ End ICBCombatInterface Interface.

	FORCEINLINE UCBOutlawCombatComponent* GetOutlawCombatComponent() const { return OutlawCombatComponent.Get(); }

#pragma region BossMusic
	/** 보스 BGM. 로드아웃이 주입하고(전 인스턴스 공용 경로), 이 캐릭터가 각 머신의 BGM 서브시스템에 걸고 해제함 */
public:
	/**
	 * [공용] 로드아웃이 보스 BGM 을 주입하는 함수 (준비 완료 전에 호출됨).
	 * @param InBossMusic 보스 BGM (nullptr = BGM 을 바꾸지 않는 Outlaw - 엘리트 등급 등)
	 */
	void SetBossMusic(USoundBase* InBossMusic);

private:
	/**
	 * [로컬] 이 머신의 BGM 서브시스템에 보스 BGM 을 걸거나 해제함. 보스 BGM 이 없으면 아무것도 하지 않음.
	 * @param bInPlaying true = 레벨 BGM 대신 보스 BGM / false = 레벨 BGM 으로 복귀
	 */
	void Local_SetBossMusicPlaying(bool bInPlaying) const;

	/** 로드아웃이 주입한 보스 BGM (없으면 nullptr) */
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> BossMusic = nullptr;
#pragma endregion
};
