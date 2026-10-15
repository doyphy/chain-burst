#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CBMusicSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * 배경 음악(BGM)을 재생하는 월드 서브시스템. 각 머신이 자기 화면의 BGM 을 로컬로 재생함 (네트워크 없음).
 *
 * 곡은 두 자리로 나뉨.
 * - 기본 곡: 그 레벨의 평소 BGM. 레벨 BP 가 BeginPlay 에서 지정
 * - 덮어쓰기 곡: 잠깐 바꿔 트는 BGM (보스전). 보스가 각 머신에서 준비 완료 때 걸고, 사망·파괴 때 해제
 * 덮어쓰기 곡이 있으면 그것을, 없으면 기본 곡을 틀고, 곡이 바뀌면 크로스페이드함. 호출 순서와 무관하게 결과가 같음.
 *
 * 레벨업 정지 중에도 BGM 이 끊기지 않도록 UI 사운드로 재생함. 볼륨은 사운드 에셋의 Sound Class(옵션 메뉴 Music)가 정함.
 */
UCLASS()
class CHAINBURST_API UCBMusicSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * 이 레벨의 기본 BGM 을 지정함. 덮어쓰기 곡이 걸려 있으면 그것이 끝난 뒤 이 곡으로 돌아옴.
	 * @param InMusic 기본 BGM (nullptr = 무음)
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Music")
	void SetBaseMusic(USoundBase* InMusic);

	/**
	 * 기본 곡 대신 틀 BGM 을 걸거나 해제함 (보스전 등).
	 * @param InMusic 덮어쓰기 BGM (nullptr = 해제 - 기본 곡으로 돌아옴)
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Music")
	void SetOverrideMusic(USoundBase* InMusic);

private:
	/** 지금 틀어야 할 곡(덮어쓰기 > 기본)이 재생 중인 곡과 다르면 크로스페이드로 바꿈 */
	void UpdatePlayingMusic();

	/** 곡이 바뀔 때 이전 곡이 사라지고 새 곡이 나타나는 시간(초). 상황마다 달라질 근거가 없어 상수 */
	static constexpr float CrossFadeTime = 1.5f;

	/** 레벨 기본 BGM */
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> BaseMusic = nullptr;

	/** 덮어쓰기 BGM (없으면 nullptr) */
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> OverrideMusic = nullptr;

	/** 지금 재생 중인 BGM 컴포넌트. 페이드아웃 중인 이전 곡은 들고 있지 않음 (끝나면 스스로 파괴) */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> PlayingMusicComponent = nullptr;
};
