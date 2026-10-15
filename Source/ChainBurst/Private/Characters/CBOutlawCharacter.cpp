// project
#include "Characters/CBOutlawCharacter.h"
#include "DataAssets/Loadout/CBOutlawLoadout.h"
#include "Components/Combat/CBOutlawCombatComponent.h"
#include "Controllers/CBOutlawController.h"
#include "Core/CBMusicSubsystem.h"

// engine
#include "Engine/World.h"
#include "Sound/SoundBase.h"

ACBOutlawCharacter::ACBOutlawCharacter()
{
	// Combat Component 생성 (보스/엘리트 전용). ASC·AttributeSet·이동/빙의 설정은 AI 베이스에서 처리.
	OutlawCombatComponent = CreateDefaultSubobject<UCBOutlawCombatComponent>(TEXT("CBOutlawCombatComponent"));

	// 빙의할 AI 컨트롤러 지정 (자동 빙의 설정은 AI 베이스). 복잡한 전투 두뇌 담당.
	AIControllerClass = ACBOutlawController::StaticClass();
}

TSoftObjectPtr<UCBAILoadout> ACBOutlawCharacter::GetAILoadout() const
{
	return OutlawLoadout;
}

UCBCombatComponent* ACBOutlawCharacter::GetCBCombatComponent() const
{
	return OutlawCombatComponent.Get();
}

// 보스가 이 머신에 처음 보이는 순간(소환 = 등장 연출)에 보스 BGM 을 틂
void ACBOutlawCharacter::HandleCharacterSystemReady()
{
	Super::HandleCharacterSystemReady();

	Local_SetBossMusicPlaying(true);
}

// 사망 태그(전 클라 복제)로 각 머신에서 호출됨. 시체가 남아 있어도 BGM 은 사망 순간 되돌림
void ACBOutlawCharacter::Local_HandleDeath()
{
	Super::Local_HandleDeath();

	Local_SetBossMusicPlaying(false);
}

// 죽지 않고 파괴되는 경우(전멸 리셋)도 되돌림. 사망 뒤 시체가 사라질 때 한 번 더 와도 이미 레벨 BGM 이라 무해.
// 레벨 종료·이동으로 끝날 때는 BGM 이 월드와 함께 정지되므로 되돌리지 않음 (보스 바의 EndPlay(Destroyed) 와 같은 기준)
void ACBOutlawCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		Local_SetBossMusicPlaying(false);
	}

	Super::EndPlay(EndPlayReason);
}

#pragma region BossMusic
void ACBOutlawCharacter::SetBossMusic(USoundBase* InBossMusic)
{
	BossMusic = InBossMusic;
}

void ACBOutlawCharacter::Local_SetBossMusicPlaying(bool bInPlaying) const
{
	// 보스 BGM 이 없는 Outlaw(엘리트 등)는 레벨 BGM 을 건드리지 않음
	if (!BossMusic) return;

	if (UCBMusicSubsystem* MusicSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UCBMusicSubsystem>() : nullptr)
	{
		MusicSubsystem->SetOverrideMusic(bInPlaying ? BossMusic.Get() : nullptr);
	}
}
#pragma endregion
