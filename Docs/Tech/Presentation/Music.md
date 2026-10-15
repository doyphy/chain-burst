# 배경 음악 (BGM)

> 레벨마다 평소 BGM 이 흐르고, 보스가 나타나는 순간(소환 = 등장 연출) 보스 BGM 으로 크로스페이드했다가 보스가 죽거나 사라지면 평소 BGM 으로 돌아온다. **BGM 은 각 머신이 자기 화면에서 로컬로 재생하며 새 네트워크 코드가 없다** — 신호는 이미 복제되는 보스의 생성·사망·파괴뿐이다.

## 흐름

```
[각 머신] 레벨 BP BeginPlay
  └ UCBMusicSubsystem::SetBaseMusic(이 레벨 BGM)                    ← 평소 BGM

[각 머신] 보스 준비 완료 (ACBOutlawCharacter::HandleCharacterSystemReady — 소환·등장 연출과 같은 순간)
  └ SetOverrideMusic(로드아웃 BossMusic)                             ← 보스 BGM 으로 크로스페이드
[각 머신] 보스 사망 (Status.Dead 복제 → Local_HandleDeath)
       또는 보스 파괴 (전멸 리셋 → EndPlay(Destroyed))
  └ SetOverrideMusic(nullptr)                                        ← 평소 BGM 으로 크로스페이드
```

## 구성 요소

| 요소 | 위치 | 역할 |
|---|---|---|
| `UCBMusicSubsystem` | `Core/` (월드 서브시스템) | 기본 곡·덮어쓰기 곡 두 자리. "덮어쓰기가 있으면 그것, 없으면 기본"을 재생하고, 곡이 바뀌면 크로스페이드 |
| `SetBaseMusic(Sound)` | 서브시스템 (`BlueprintCallable`) | 레벨 BP 에서 그 레벨의 평소 BGM 지정. `nullptr` = 무음 |
| `SetOverrideMusic(Sound)` | 서브시스템 (`BlueprintCallable`) | 기본 곡 대신 틀 곡. `nullptr` = 해제 → 기본 곡 복귀 |
| `UCBOutlawLoadout::BossMusic` | 로드아웃 | 보스 BGM (하드 참조). **비우면 BGM 을 바꾸지 않음** — 엘리트 등급 Outlaw 는 비워 둠. `ApplyToCharacter`(전 인스턴스 공용 경로)가 캐릭터에 주입 |
| `ACBOutlawCharacter` | 캐릭터 | `SetBossMusic` 로 받고, 준비 완료에 걸고, 사망·파괴에 해제 (`Local_SetBossMusicPlaying`) |

## 왜 이렇게 했나

### 각 머신 로컬 재생

BGM 은 화면 연출이라 복제할 상태가 없다. 보스의 시작·끝은 **이미 복제되는 신호**로 각 머신이 스스로 안다 — 보스 캐릭터 생성(`Always Relevant`), 사망 태그 `Status.Dead`(GE 부여 태그라 Minimal 모드에서도 복제), 파괴. Multicast RPC 를 따로 보내면 같은 일을 두 번 하게 된다.

### 보스 BGM 은 보스가 — 생성 연출·보스 바와 같은 패턴

보스 구역(`ACBBossArena`)은 레벨 배치 액터라 클라에도 있지만 **판정은 서버 전용**이라 클라 쪽 구역은 보스전 단계를 모른다. 그래서 BGM 을 거는 쪽은 보스 자신이다.

- **어떤 곡인지는 로드아웃이, 언제인지는 캐릭터가** 정한다 — 생성 연출(`SpawnCueTag` → [SpawnFX.md](SpawnFX.md))·보스 바(`BossBarWidgetClass` → [UI.md](UI.md))와 같은 분담이다. 보스가 바뀌어도 로드아웃만 바꾸면 되고, 구역·입구 BP 는 음악을 모른다.
- **시작 = 각 머신의 보스 준비 완료.** 보스가 그 화면에 처음 보이는 순간(메시가 붙음)이라 디졸브 등장·등장 연출 카메라와 함께 시작한다. 보스 바처럼 교전 개시(`Status.Combat.Engaged`)를 기다리지 않는다.
- **끝 = 사망 순간.** 시체가 사라질 때(`DespawnDelay`)까지 기다리지 않는다. 사망 태그는 모든 머신에 복제되어 `Local_HandleDeath` 가 각자 불린다.
- **파괴도 끝이다 — 단, `Destroyed` 일 때만.** 전멸 리셋은 보스를 죽이지 않고 파괴한다. 레벨 종료·이동(`EndPlay` 의 다른 이유)에서는 BGM 이 월드 Flush 로 함께 정지되므로 되돌리지 않는다 — 되돌리면 사라질 월드에서 평소 BGM 을 새로 틀게 된다.
- 사망 뒤 시체가 파괴될 때 해제가 한 번 더 오지만, 이미 평소 BGM 이라 서브시스템이 아무것도 하지 않는다.

### 서브시스템 — 두 자리와 "다시 계산"

- 재생할 곡을 **매번 두 자리에서 다시 계산**한다(덮어쓰기 > 기본). 그래서 레벨 BP 와 보스 준비 완료의 순서가 바뀌어도(배치형 보스가 먼저 준비돼도) 결과가 같다.
- **같은 곡이면 처음부터 다시 틀지 않는다.** 해제가 두 번 오는 경우가 무해한 이유다.
- **크로스페이드**: 이전 곡 `FadeOut` + 새 곡 `CreateSound2D` → `FadeIn`. 이전 곡 컴포넌트는 `bAutoDestroy` 라 페이드아웃이 끝나면 스스로 파괴된다. 시간은 상수 `CrossFadeTime`(1.5초).
- **덮어쓰기는 한 자리다.** 보스는 한 번에 하나이고, 전멸 리셋은 이전 보스를 파괴한 뒤 새 보스를 소환해 겹치지 않는다.

### UI 사운드로 재생 — 레벨업 정지

레벨업 카드 선택 중에는 월드가 정지된다(→ [LevelUp.md](../Gameplay/LevelUp.md) "정지"). 오디오 장치는 게임 틱이 멈추면 **UI 사운드만 계속 재생**하므로(`AudioDevice.cpp` — `bGameTicking || bIsUISound`), 서브시스템이 BGM 컴포넌트의 `bIsUISound` 를 켠다. 끄면 카드를 고르는 동안 BGM 이 멈춘다.

- 레벨 이동 때는 UI 사운드여도 정지된다 — 월드 Flush 는 `bIgnoreForFlushing`(레벨을 넘어 유지하는 소리)만 남긴다. 그래서 `bPersistAcrossLevelTransition` 은 끈 채로 두고 레벨마다 기본 곡을 새로 지정한다.
- EasyGameUI 일시정지 메뉴는 게임을 멈추지 않으므로 이와 무관하다.

### 볼륨 — 옵션 슬라이더와 사운드 클래스

옵션 메뉴의 볼륨 슬라이더는 `DA_CB_GlobalConfig` 의 **Game Audio Classes**(키 Master·UI·Music·Effects·Ambient → 사운드 클래스)에 등록된 클래스에 `SetSoundMixClassOverride` 를 건다(`BP_SettingAudioManager`). 코드는 클래스를 강제하지 않는다 — 어느 클래스가 맞는지가 EasyGameUI 설정 데이터에 있어서다.

**현재 상태: Master 만 엔진 `/Engine/EngineSounds/Master`, 나머지는 비어 있다**(팩 베이스 기본값 그대로 — 팩 데모 설정은 전부 `SC_EGUI_*`). 그래서 BGM 의 Sound Class 를 비워 두면 프로젝트 기본 클래스(엔진 Master)로 가서 **Master 슬라이더만** 먹고, Music 슬라이더는 아무 효과가 없다.

**Music 슬라이더까지 쓰려면 — 팩 계층을 그대로 쓴다** (엔진·팩 에셋은 가리키기만 하고 고치지 않음)

| 대상 | 설정 |
|---|---|
| `DA_CB_GlobalConfig` → Game Audio Classes | Master = `SC_EGUI_MasterAudio`, UI/Music/Effects/Ambient = `SC_EGUI_UIAudio`/`MusicAudio`/`EffectsAudio`/`AmbientAudio` (팩 데모와 같음) |
| BGM 에셋 → Sound → Class | `SC_EGUI_MusicAudio` (원래 `SC_EGUI_MasterAudio` 의 자식이라 Master·Music 슬라이더가 둘 다 먹음) |
| Project Settings → Audio → **Default Sound Class** | `SC_EGUI_EffectsAudio` + 에디터 재시작. 없으면 Master 를 바꾼 순간 클래스가 빈 기존 효과음이 Master 슬라이더 밖으로 빠진다 |

**우리 클래스(`SC_CB_Music`)를 엔진 Master 밑에 두는 방식은 쓰지 않는다.** 사운드 클래스의 부모·자식 연결은 **부모 에셋의 `ChildClasses` 목록**에 저장되고(`SoundClass.cpp` — 부모 지정 시 부모를 `Modify()` 하고 자식 추가), 볼륨 전파(슬라이더의 "자식에도 적용")도 그 목록을 따라간다(`AudioDevice.cpp` `RecurseIntoSoundClasses`·`RecursiveApplyAdjuster`). 엔진 Master 밑에 두면 **엔진 에셋을 고쳐 저장**해야 하고(엔진 편집 금지), 저장하지 않으면 다음 실행에서 연결이 사라진다. 팩 `SC_EGUI_MasterAudio` 밑에 새 클래스를 두는 것도 같은 이유로 팩 원본 수정이 된다.

## 에디터 작업

| 대상 | 작업 |
|---|---|
| BGM 사운드 에셋 (`/Game/ChainBurst/Audio/Main_bgm`·`Boss_bgm`) | **Looping 켬**, Volume 0.6, Priority 4, Virtualization Mode = Play When Silent ✅ 적용됨. Sound Class 는 위 "볼륨" 절에 따라. 감쇠 없음(2D 재생) |
| 각 레벨 BP (게임플레이·로비·메인메뉴) | `Event BeginPlay` → `Get CBMusicSubsystem` → `Set Base Music`(그 레벨 곡). **Authority 분기로 감싸지 않는다** — 호스트·클라 모두 실행돼야 각자 재생됨 |
| `DA_Loadout_Outlaw_Hulk` | `Loadout\|Audio` → `BossMusic` = `Boss_bgm` ✅ 적용됨 |
| 공통 | 새 `UPROPERTY` 가 있으므로 코드 반영 후 **에디터 재시작** |

- **Looping 은 에셋 설정이다.** 서브시스템은 에셋을 그대로 재생하므로 꺼져 있으면 곡이 한 번 끝나고 무음이 된다.
- **Volume 0.6** — 두 곡 모두 RMS −13~−15 dBFS 로 BGM 치고 커서 효과음과 균형을 맞추는 출발값. 조정은 에셋 Volume 으로 한다(코드에 볼륨 값 없음).
- **Priority 4 · Play When Silent** — 보스전처럼 효과음이 몰려 채널 한도에 걸려도 BGM 이 덜 밀려나고, 밀려났다 돌아오면 처음부터가 아니라 이어서 재생된다(기본 `Restart` 는 처음부터).
- 두 곡 다 마디 단위로 떨어지는 길이(Main 54.86초 ≈ 140 BPM 32마디, Boss 56.89초 ≈ 135 BPM 32마디 — 길이로 역산한 추정)에 시작·끝 페이드가 없어 반복용으로 맞다. 같은 폴더의 원본 `loop_16bar`·`loop_8bar` 는 `full` 의 앞부분을 자른 것이라 `full` 을 쓴다.
- 보스 BGM 은 로드아웃 하드 참조라 보스 로드아웃과 함께 클라에도 로드된다. 오디오 스트림 캐싱(UE5 기본)이면 처음 일부만 메모리에 올라가므로 길이가 부담되지 않는다.

## 검토했지만 채택하지 않은 것

| 안 | 이유 |
|---|---|
| 보스 구역이 보스 BGM 을 가짐 | 클라의 구역은 보스전 단계를 모름. 보스가 바뀌어도 로드아웃만 바꾸는 편이 보스 바와 같은 구조 |
| Multicast RPC 로 BGM 신호 | 복제되는 생성·사망·파괴가 같은 일을 이미 함 |
| 레벨에 `Ambient Sound` 액터로 기본 BGM | 보스 BGM 이 그것을 찾아 꺼야 함. 서브시스템이 두 곡을 함께 쥐어야 크로스페이드가 됨 |
| 커스텀 WorldSettings 클래스에 기본 곡 | 프로젝트 설정(월드 세팅 클래스)까지 바꾸는 큰 수단. 레벨 BP 한 줄로 충분 |
| 교전 개시(`Engaged`)부터 보스 BGM | 요청은 등장 연출부터 |
| 시체가 사라질 때 BGM 복귀 | 승리 순간과 어긋남 (보스 바는 0 을 보여 주려고 시체까지 유지) |
| 요청자별 덮어쓰기 스택 | 보스는 한 번에 하나. 둘 이상이 동시에 생기면 그때 스택으로 |
| 이미 죽은 채 준비 완료되는 보스 가드 | 등장 연출 중 슈퍼아머 + 인원 비례 체력이라 클라 로드 시간 안에 죽을 수 없음 |
| 크로스페이드 시간·볼륨 `UPROPERTY` | 볼륨은 에셋 Volume·사운드 클래스가, 페이드는 상황마다 달라질 근거가 없음 |
| 프로젝트 사운드 클래스 `SC_CB_Music` 을 엔진 Master 밑에 | 부모·자식 연결이 부모 에셋(엔진 Master)에 저장돼 엔진 에셋 수정이 필요 (위 "볼륨") |
| 보스전 뒤 평소 곡을 끊긴 지점부터 | 요청 없음. 지금은 처음부터 페이드인 |

## 알려진 한계

- **PIE 에서 여러 창을 띄우면 창마다 BGM 이 겹쳐 들린다.** 각 PIE 월드가 자기 서브시스템으로 재생하기 때문이다(실제 게임은 머신당 하나).
- **보스가 둘 이상 동시에 있으면** 먼저 죽은 보스가 덮어쓰기를 해제해 평소 BGM 으로 돌아간다.

## 검증 (2인 리슨 서버)

1. 게임플레이 맵 시작 — 호스트·클라 모두 평소 BGM 이 페이드인된다. 옵션 Master 슬라이더로 볼륨이 바뀐다(Music 슬라이더는 위 "볼륨" 절의 팩 계층을 설정했을 때만)
1-1. 1분쯤 지나 곡이 끝나도 멈추지 않고 처음으로 이어진다 (이음매 클릭 없음)
2. 보스 소환 순간(디졸브 등장·연출 카메라) 양쪽에서 보스 BGM 으로 크로스페이드
3. 보스전 중 레벨업 카드 선택 — 월드는 멈추지만 BGM 은 계속 흐른다
4. 보스 처치 순간 양쪽이 평소 BGM 으로 돌아온다 (시체가 사라질 때가 아님)
5. 보스전 중 전멸 리셋 — 보스가 사라지며 평소 BGM 으로, 다시 들어가 새 보스가 나오면 보스 BGM
6. 로드아웃 `BossMusic` 을 비우면 보스가 나와도 BGM 이 바뀌지 않는다

## 관련 문서

- 보스 소환·등장 연출·전멸 리셋: [BossArena.md](../Gameplay/BossArena.md)
- 같은 분담(로드아웃이 무엇을, 캐릭터가 언제): [SpawnFX.md](SpawnFX.md), [UI.md](UI.md) "보스 바"
- 레벨업 정지: [LevelUp.md](../Gameplay/LevelUp.md)
- 옵션 메뉴·Global Config: [EasyGameUI.md](EasyGameUI.md)
