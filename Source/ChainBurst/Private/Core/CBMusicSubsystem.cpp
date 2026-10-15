// project
#include "Core/CBMusicSubsystem.h"

// engine
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void UCBMusicSubsystem::SetBaseMusic(USoundBase* InMusic)
{
	BaseMusic = InMusic;
	UpdatePlayingMusic();
}

void UCBMusicSubsystem::SetOverrideMusic(USoundBase* InMusic)
{
	OverrideMusic = InMusic;
	UpdatePlayingMusic();
}

// 틀어야 할 곡을 다시 계산해 바뀌었을 때만 크로스페이드
void UCBMusicSubsystem::UpdatePlayingMusic()
{
	USoundBase* DesiredMusic = OverrideMusic ? OverrideMusic.Get() : BaseMusic.Get();

	// 이미 그 곡을 틀고 있으면 처음부터 다시 틀지 않음 (사망 뒤 파괴처럼 해제가 두 번 와도 무해)
	USoundBase* PlayingMusic = PlayingMusicComponent ? PlayingMusicComponent->Sound.Get() : nullptr;
	if (DesiredMusic == PlayingMusic) return;

	// 이전 곡은 페이드아웃. 끝나면 정지되고 자동 파괴됨 (bAutoDestroy)
	if (PlayingMusicComponent)
	{
		PlayingMusicComponent->FadeOut(CrossFadeTime, 0.f);
		PlayingMusicComponent = nullptr;
	}

	if (!DesiredMusic) return;

	// 2D - 듣는 위치와 무관. 레벨 이동 때는 월드 Flush 로 함께 정지됨 (레벨을 넘어 유지하지 않음)
	PlayingMusicComponent = UGameplayStatics::CreateSound2D(this, DesiredMusic, 1.f, 1.f, 0.f, nullptr,
		/*bPersistAcrossLevelTransition =*/ false, /*bAutoDestroy =*/ true);

	// 오디오 장치가 없으면(헤드리스 등) 만들어지지 않음
	if (!PlayingMusicComponent) return;

	// 레벨업 카드 선택 중에는 월드가 정지되는데, UI 사운드가 아니면 그동안 BGM 이 멈춤
	PlayingMusicComponent->bIsUISound = true;
	PlayingMusicComponent->FadeIn(CrossFadeTime);
}
