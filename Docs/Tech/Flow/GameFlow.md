# 게임 플로우 (레벨·게임모드·전환)

> 어떤 레벨들이 있고, 그 사이를 어떻게 오가며, **무엇이 살아남는가**. 레벨을 추가하거나 게임모드를 만들 때, 맵을 넘어 데이터를 넘겨야 할 때 이 문서를 따른다. **폰을 다시 스폰하는 두 갈래**(무기 변경·리스폰)와 **매치 종료**(보스 처치 → 클리어 화면 → 로비 복귀·각자 나가기)도 여기 있다.
>
> 접속·세션·나가기는 [Session.md](Session.md), 로비 레벨 안에서 일어나는 일은 [Lobby.md](Lobby.md)로 나뉘어 있다. 서버 권위·접두사·복제 정책 같은 **일반 규칙**은 [Multiplayer.md](../Conventions/Multiplayer.md). 여기는 **구체적인 흐름**만 다룬다.

## 전체 흐름

```
                                 ┌──(보스 처치 → 호스트가 전원 복귀)──┐
                                 ▼                                   │
메인 메뉴  ──(호스트)──▶  로비  ──(모두 준비)──▶  게임플레이 ─────────┘
    ▲  │                   ▲                        │
    │  └──────(참가)───────┘                        │
    └────────────(각자 나가기 — 세션 이탈)──────────┘
```

- **메인 메뉴**는 세션 밖이다. 항상 단독 실행이며 서버·클라이언트 구분이 없다.
- **로비부터** 세션 안이다. 복제되는 월드이고, 게임플레이 레벨과 같은 규칙이 적용된다.
- 참가자는 메인 메뉴에서 **로비로 곧장 travel**한다.
- 보스를 잡으면 매치가 끝난다. **세션을 유지한 채 로비로 돌아가는 것은 호스트가**, 세션을 나가 메인 메뉴로 가는 것은 **각자** 한다 (아래 "매치 종료").

## 레벨과 게임모드

| 레벨 | 게임모드 | 역할 |
|---|---|---|
| `L_CB_MainMenu` | `GM_CB_MainMenu` (BP만) | 메뉴 표시. **Default Pawn = None**, 규칙 없음 |
| `L_CB_Lobby` | `ACBLobbyGameMode` | 외형 커스터마이징 대기, 준비 완료 판정, `ServerTravel` |
| 게임플레이 | `ACBGameplayGameMode` | 매치 규칙 (보스 처치 = 클리어·리스폰·레벨업 → [LevelUp.md](../Gameplay/LevelUp.md)) |

메인 메뉴만 C++ 클래스가 없다 — 담을 규칙이 없어 BP 설정(HUD·Default Pawn)만으로 충분하다. → [EasyGameUI.md](../Presentation/EasyGameUI.md)

### 게임 스테이트 계층

```
ACBGameStateBase (AGameStateBase)   ← 플레이어 목록 변경 신호(OnPlayerListChanged)
 ├─ ACBLobbyGameState               ← 준비 인원 집계
 └─ ACBGameplayGameState            ← 전 플레이어 공유 레벨·경험치
```

`ACBGameModeBase` 생성자가 `GameStateClass`를 `ACBGameStateBase`로 고정하고, 로비·게임플레이 게임모드가 각자 자기 것으로 덮는다. 목록 신호는 HUD 플레이어 목록이 쓴다 → [UI.md](../Presentation/UI.md). 레벨·경험치는 → [LevelUp.md](../Gameplay/LevelUp.md)

### 게임모드 계층

```
ACBGameModeBase (AGameModeBase)
 ├─ ACBLobbyGameMode
 └─ ACBGameplayGameMode
```

**`ACBGameModeBase` 생성자가 공통 기본값을 고정한다.** BP 체크박스에 맡기지 않는 이유는 누락 시 증상이 조용하기 때문이다.

```cpp
bUseSeamlessTravel = true;
PlayerControllerClass = ACBChaserController::StaticClass();
PlayerStateClass      = ACBPlayerState::StaticClass();
```

> **플레이어 = 추격자(Chaser)다.** 플레이어 컨트롤러는 `ACBPlayerController`가 아니라 **`ACBChaserController`** 이며, AI 컨트롤러(`ACBAIController` 계열)와 형제가 아니라 `APlayerController` 직속이다.

**`AGameMode`로 갈아타지 않았다 — 매치 종료가 생긴 뒤에 다시 검토한 결과다.** `AGameMode`는 `MatchState`(대기·진행·종료) 상태 머신을 주지만, 대가가 크다.

- **게임 스테이트가 `AGameState`여야 한다.** 지금 계층은 `ACBGameStateBase : AGameStateBase`라 로비·게임플레이 게임 스테이트를 함께 갈아엎어야 한다.
- **스폰·리스폰 기본 동작이 `MatchState`에 묶인다**(`PlayerCanRestart` 가 `IsMatchInProgress()` 를 보는 등). 레벨 시작·리스폰 경로 전체가 영향 범위가 된다.
- 지금 필요한 매치 상태는 **"보스를 잡아 끝났다" 하나**(`ACBGameplayGameMode::bMatchCleared`)뿐이다 (아래 "매치 종료").

대기·카운트다운·결과 화면처럼 단계가 여럿 생기면 그때 다시 본다.

## 레벨 전환

| 상황 | 방법 |
|---|---|
| 메인 메뉴 → 로비 (호스트) | `OpenLevel` + **`listen`** — `UCBSessionSubsystem::Local_HostLobby()` → [Session.md](Session.md) |
| 메인 메뉴 → 로비 (참가) | **`ClientTravel`** — `UCBSessionSubsystem::Local_JoinServerByAddress()` → [Session.md](Session.md) |
| 방 만들기 / 방 찾기 | 세션 생성·검색·참가 후 위 두 경로로 수렴 — `UCBSessionSubsystem` → [Session.md](Session.md) |
| 로비 → 게임플레이 (전원) | **`ServerTravel`** — 접속한 클라이언트가 함께 이동 — `ACBGameModeBase::Auth_ServerTravelToLevel()` |
| 로비 → 메인 메뉴 (나가기) | 클라 **`ClientTravel`** / 호스트 `OpenLevel` — `UCBSessionSubsystem::Local_LeaveToMainMenu()` → [Session.md](Session.md) |
| 게임플레이 → 로비 (전원, 보스 처치 후) | **`ServerTravel`** — **호스트만** 요청, 세션 유지 — `ACBGameplayGameMode::Auth_TryReturnToLobby()` (아래 "매치 종료") |
| 게임플레이 → 메인 메뉴 (각자, 보스 처치 후) | 위 나가기와 같은 경로 — 클리어 화면에서는 `ACBChaserController::Local_LeaveMatchToMainMenu()` (아래 "매치 종료") |

### Seamless Travel — 켜져 있어야 데이터가 넘어간다

`bUseSeamlessTravel = false`면 클라이언트가 **완전히 재접속**하는 형태라 PlayerState가 백지에서 시작한다. **에러 없이 조용히 값이 사라지므로** 새 게임모드를 만들 때마다 확인 대상이다(베이스에서 고정한 이유).

- **전환 맵**: Project Settings → Maps & Modes → `Transition Map`. 비워두면 엔진이 임시 빈 월드를 쓰지만 명시하는 편이 안전하다.
- **PlayerController는 클래스가 같아도 새로 만들어진다 (UE 5.8).** `AGameModeBase::HandleSeamlessTravelPlayer`가 이전 PC를 데리고 온 뒤 **항상** 새 PC를 스폰해 바꿔 끼운다(`SeamlessTravelTo/From` → `SwapPlayerControllers`). 넘어오는 것은 PlayerState 복사(`CopyProperties`)뿐이고, 컨트롤러의 로컬 상태(1회성 플래그 `bReadyStateApplied`, 위젯 참조)는 **레벨마다 새로 시작한다.** 새 PC 도 `ReceivedPlayer`를 다시 타므로 페이드 덮기도 레벨마다 다시 걸린다 — 로비로 돌아왔을 때 로비 UI 가 다시 뜨는 이유다. 클래스는 여전히 베이스에서 고정한다(BP 누락이 조용하므로).

## 맵을 넘을 때 무엇이 살아남나

| | 대상 |
|---|---|
| **파괴 (월드 소속)** | 모든 Actor — GameMode, GameState, **PlayerController, PlayerState**, Pawn, **HUD**, 레벨 배치 액터<br>`UWorldSubsystem`, 레벨 블루프린트 |
| **생존** | **`UGameInstance`** 계열, `UGameInstanceSubsystem`, `ULocalPlayer`, 설정(`UGameUserSettings` 등), 디스크에 쓴 것 |

**맵을 넘어 살아야 하는 것은 사실상 `UGameInstance` 계열 하나다.** 세션·매치메이킹·누적 진행도가 갈 자리이며, 실제로 접속 로직은 그 수명을 공유하는 `UGameInstanceSubsystem`(→ `UCBSessionSubsystem`)에 두었다. `UCBGameInstance` 자체는 아직 빈 껍데기다.

같은 경계가 UI에도 그대로 나타난다 — 팩 HUD는 맵 단위로 파괴되지만 위젯 인스턴스 캐시(`BP_EGUI_GameUIRegistry`)는 GameInstance가 소유해 게임 전체를 산다. → [EasyGameUI.md](../Presentation/EasyGameUI.md)

### PlayerState 이관 — `CopyProperties` 오버라이드 필수

Seamless travel이어도 **PlayerState 객체 자체는 새로 만들어진다.** 엔진이 새 인스턴스를 만든 뒤 `CopyProperties()`로 값을 옮기는데, **오버라이드하지 않으면 엔진 기본 값(이름·점수 등)만 복사되고 커스텀 프로퍼티는 사라진다.**

```cpp
void ACBPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
    Super::CopyProperties(NewPlayerState);
    if (ACBPlayerState* NewCBPlayerState = Cast<ACBPlayerState>(NewPlayerState))
    {
        NewCBPlayerState->Cosmetics = Cosmetics;                     // 로비에서 고른 의상
        NewCBPlayerState->SelectedCharacterId = SelectedCharacterId; // 로비에서 고른 캐릭터(무기)
    }
}
```

**엔진 기본 값에 이미 있는 것은 다시 옮기지 않는다.** 닉네임(`PlayerName`)은 `Super::CopyProperties`가 `SetPlayerNameInternal`로 옮기므로 여기에 줄을 추가할 필요가 없다 (→ [Lobby.md](Lobby.md) "닉네임").

**PlayerState에 맵을 넘어야 할 값을 추가하면 이 함수도 함께 고친다.** 빠뜨려도 컴파일은 통과하고 로비 안에서는 정상 동작하므로, 게임 레벨에 도착해서야 값이 비어 있는 것으로 발견된다.

#### PIE는 seamless travel을 끈다 — `net.AllowPIESeamlessTravel=1`

엔진이 **PIE에서 seamless travel을 강제로 하드 travel로 되돌린다**(`GameModeBase.cpp:503`). 하드 travel이면 `SeamlessTravelFrom` → `SeamlessTravelTo` → `DispatchCopyProperties` 경로 자체가 실행되지 않아 **`CopyProperties`가 아예 불리지 않는다.**

```
ProcessServerTravel: Seamless travel is disabled in PIE, set net.AllowPIESeamlessTravel=1 to enable.
```

이 경고가 유일한 단서다. `Config/DefaultEngine.ini`의 `[ConsoleVariables]`에 켜 두었다(에디터 재시작 필요). **별도 프로세스 Standalone은 `EWorldType::Game`이라 이 제한을 받지 않는다.**

#### 값이 넘어와도 적용은 별개다

`CopyProperties`가 성공해도 **새 폰에 반영되는 것은 다른 문제다.** 실제로 의상 이관이 실패했을 때 값은 4개 모두 정상적으로 넘어와 있었고, 적용 함수가 폰이 없는 시점에 한 번 불리고 끝나 있었다. seamless travel 직후에는 **폰이 붙은 뒤에 새 PlayerState의 `BeginPlay`가 실행**될 수 있어, 구독만으로는 방송을 놓친다. → [Cosmetic.md](../Presentation/Cosmetic.md) "적용 타이밍"

## 클래스별 존재 범위

수명과 별개로 **어느 인스턴스에 존재하는지**도 다르다. 흐름을 설계할 때 자주 걸리는 지점이다.

| 클래스 | 서버 | 소유 클라 | 다른 클라 |
|---|---|---|---|
| **GameMode** | ✅ | ❌ | ❌ |
| GameState | ✅ | ✅ | ✅ |
| **PlayerController** | ✅ | ✅ | ❌ |
| **PlayerState** | ✅ | ✅ | ✅ |
| Pawn | ✅ | ✅ | ✅ |
| **HUD** | 호스트 자신 것만 | ✅ | ❌ |

여기서 따라 나오는 규칙들:

- **게임 규칙은 전부 서버 권위** — GameMode가 클라이언트에 없다
- **플레이어별 + 전 클라가 봐야 하는 값은 PlayerState** — 로비의 의상 선택이 여기 있는 이유
- **게임 전역 상태는 GameState** — "준비 4명 중 3명" 같은 것
- **HUD는 로컬에만 존재** — 서버가 원격 플레이어의 PC로 `GetHUD()`를 부르면 영원히 `nullptr` → [UI.md](../Presentation/UI.md)

## 무기 변경 = 캐릭터 변경 — 폰을 다시 스폰한다

**무기 종류가 곧 캐릭터 종류다.** 무기마다 캐릭터 BP와 로드아웃이 따로 있으므로, 무기를 바꾼다는 것은 어빌리티·무기·메시·입력이 전부 다른 캐릭터로 갈아타는 일이다. 로드아웃만 갈아끼우는 경로는 만들지 않고 **폰을 통째로 다시 스폰**한다.

| 갈래 | 담당 |
|---|---|
| 고를 수 있는 목록 | **`UCBCharacterCatalog`** — 태그 → 캐릭터 클래스(소프트)·표시 이름·아이콘 |
| 목록의 등록소 | **`UCBGameInstance::CharacterCatalog`** — 서버(스폰 클래스 결정)와 전 클라(선택 UI)가 **폰 없이** 읽어야 하고 맵을 넘어 살아야 함 |
| 고른 값 | **`ACBPlayerState::SelectedCharacterId`** (복제 + `CopyProperties` 이관) |
| 요청·검증 | `ACBChaserController::Server_RequestCharacterSelection` |
| 스폰 클래스 결정 | **`ACBGameModeBase::GetDefaultPawnClassForController`** |
| 재스폰 | `ACBLobbyGameMode::Auth_RespawnWithSelectedCharacter` |

```
[클라] 무기 버튼 → Server_RequestCharacterSelection(태그)
  └ [서버] 로비인가 / 준비 전인가 / 카탈로그에 있는 태그인가
       ├ ACBPlayerState::Auth_SetSelectedCharacterId()      ← 복제 → 위젯 선택 표시 갱신
       └ ACBLobbyGameMode::Auth_RespawnWithSelectedCharacter()
            ├ UCBAbilitySystemComponent::Auth_ClearLoadoutGrants()   ← 이전 로드아웃 부여분 회수
            ├ UnPossess + 기존 폰 Destroy                            ← 무기 액터는 컴뱃 컴포넌트가 함께 정리
            └ RestartPlayerAtPlayerStart(배정받은 자리)
                 └ GetDefaultPawnClassForController → 카탈로그가 준 클래스로 스폰
```

**스폰 클래스 결정을 `ACBGameModeBase`에 둔 이유**: 로비의 재스폰과 게임플레이 레벨의 첫 스폰이 **같은 규칙 하나**를 쓴다. `CopyProperties`로 넘어온 선택 값을 게임플레이 게임모드가 따로 해석할 필요가 없다. 고른 값이 카탈로그에서 사라졌거나 클래스 로드에 실패하면 경고를 남기고 **게임모드 BP의 기본 폰 클래스로 이어간다** — 스폰을 막으면 로비에 폰이 없어지는 쪽이 더 나쁘다.

**ASC 부여분은 반드시 회수한다.** 플레이어의 ASC는 PlayerState 소유라 **폰을 갈아도 살아남는다**(→ [ASC-Ownership.md](../Foundation/ASC-Ownership.md)). 회수하지 않으면 이전 무기의 어빌리티와 스탯 GE가 남은 채 새 로드아웃이 얹혀 **바꿀수록 쌓인다.** 반대로 무기 액터는 `UCBCombatComponent::EndPlay`가 정리하므로 폰만 파괴하면 된다.

**자리를 기억해야 한다.** 엔진 기본 `ChoosePlayerStart`는 비어 있는 스타트 중에서 고르므로, 폰을 파괴하고 다시 스폰하면 **다른 자리로 옮겨간다.** `ACBLobbyGameMode`가 컨트롤러별로 처음 배정한 스타트를 기억해(`AssignedPlayerStarts`) 재스폰에도 같은 자리를 쓰고, `Logout`에서 비운다.

**준비 상태에서는 바꿀 수 없다.** 준비는 무기 장착 어빌리티를 실행하므로(→ [Lobby.md](Lobby.md) "준비 완료와 게임 시작") 재스폰과 엉킨다. 서버가 요청을 거부하며, 위젯은 `ACBPlayerState::OnPlayerReadyChanged`를 구독해 준비 중에 버튼을 비활성화한다.

**반대로 로드 중에는 준비할 수 없다.** 재스폰된 폰은 로드아웃을 다시 비동기 로드하므로(→ [SystemReady.md](../Foundation/SystemReady.md)), 그 사이의 준비 요청은 ASC·로드아웃이 미완성인 캐릭터에 무기 장착 어빌리티를 걸어 **조용히 실패한다.** 두 방향이 대칭으로 잠긴다.

```
로드 중        → 준비 요청 차단   (ACBChaserController::Server_RequestToggleReady)
준비 완료 상태  → 캐릭터 변경 차단 (ACBChaserController::Server_RequestCharacterSelection)
```

| 갈래 | 담당 |
|---|---|
| 로드 완료 여부 | **`ACBPlayerState::IsCharacterLoaded()`** (`BlueprintPure`) — 복제하지 않는 로컬 상태 |
| 위젯 신호 | **`ACBPlayerState::OnCharacterLoadedChanged`** (`BlueprintAssignable`) |
| 상태 갱신 | `HandlePawnSet`(폰 교체 → false) / `HandleCharacterSystemReady`(준비 완료 → true) / `ApplyCosmeticsWhenReady`의 즉시-준비 분기(→ true) |
| 서버 가드 | `ACBChaserController::Server_RequestToggleReady` |

**갱신 지점이 셋인 이유:** 복제 순서에 따라 **폰이 이미 준비된 채로 붙는 경우**가 있는데, 그 경로는 준비 완료 델리게이트를 타지 않아 신호가 빠진다. 세 번째 갱신 지점이 그 구멍을 막는다. 상태는 **값이 실제로 바뀌었을 때만 방송**해 위젯이 같은 상태로 반복 갱신되지 않게 한다.

**복제하지 않는다.** "내 화면의 내 폰이 로드됐나"는 각 인스턴스가 스스로 판정할 수 있고, UI는 로컬이다(→ [UI.md](../Presentation/UI.md)). 서버 가드는 서버가 자기 쪽 폰을 직접 보므로 이 값에 의존하지 않는다.

**클래스는 로비에서 미리 로드한다.** 카탈로그의 캐릭터 클래스는 소프트 참조라 스폰 시점의 동기 로드가 히칭이 된다. `ACBLobbyGameState::BeginPlay`가 전부 비동기로 미리 로드하는데, **게임 스테이트는 서버·전 클라이언트에 모두 존재**하므로 복제로 도착하는 남의 폰 클래스까지 한 번에 확보된다(게임모드에 두면 서버만 데워진다).

**의상 선택은 유지된다.** `ACBPlayerState::HandlePawnSet`이 새 폰에 조합을 다시 적용한다(→ [Cosmetic.md](../Presentation/Cosmetic.md)). 단 새 캐릭터의 의상 카탈로그에 없는 파츠는 검증에서 걸러져 무시되므로, **캐릭터끼리 카탈로그를 공유하지 않으면 조합이 부분적으로 빠진다.**

> 재빙의는 뷰 타겟도 다시 정하게 만든다. 커스터마이징 뷰 중이라면 `bInCosmeticView` 게이트가 클로즈업을 유지한다 (→ [Lobby.md](Lobby.md) "`AutoManageActiveCameraTarget`은 빙의 한 번에 5~7회 불린다").

## 사망과 리스폰 — 폰을 다시 스폰한다

플레이어가 죽으면 `RespawnDelay`(기본 5초) 뒤 **폰을 새로 스폰**한다. 시체를 되살리지 않는 이유는 무기 변경과 같다 — 재스폰 경로가 이미 있고, 그 경로가 로드아웃·무기·입력을 처음부터 다시 깔아주기 때문이다.

| 갈래 | 담당 |
|---|---|
| 사망 통지 | `ACBChaserCharacter::Auth_OnDeath` → 게임모드에 알리기만 함 |
| 지연·스폰 지점 규칙 | **`ACBGameplayGameMode`** (`RespawnDelay`, `Auth_HandlePlayerDeath`) |
| 보스전 중 사망자 부활 위치 | `ACBGameplayGameMode::Auth_FindBossFightRespawnTransform` → 보스 구역의 `RespawnPoint` (아래 "보스전 중 사망") |
| 사망 상태 정리 | `ACBGameplayGameMode::Auth_ClearDeathState` |
| 실제 재스폰 | `ACBGameplayGameMode::Auth_RespawnPlayer` → `RestartPlayer` (보스전 중 사망이면 `RestartPlayerAtTransform`) |

### ASC가 PlayerState 소유라 청소가 필수다

플레이어의 ASC는 폰이 아니라 `ACBPlayerState`가 갖는다 (→ [ASC-Ownership.md](../Foundation/ASC-Ownership.md)). **폰을 갈아도 ASC는 그대로 살아남으므로**, 청소 없이 재스폰하면:

- `GE_Dead`가 남아 새 캐릭터가 태어나자마자 `Status.Dead` 상태가 되고, `UCBGameplayAbility::CanActivateAbility`가 모든 어빌리티를 막는다
- 이전 로드아웃이 부여한 어빌리티·이펙트가 새 폰의 부여분과 겹친다

그래서 스폰 **전에** 두 가지를 걷어낸다. 사망 GE는 클래스가 아니라 **부여 태그(`Status.Dead`)로 제거**한다 — 어떤 GE를 쓸지는 사망 어빌리티 BP가 정하므로 게임모드가 클래스를 알 수 없다. 로드아웃 부여분 회수는 로비의 캐릭터 변경과 같은 `UCBAbilitySystemComponent::Auth_ClearLoadoutGrants()`를 쓴다.

**체력은 따로 되돌리지 않는다.** 새 폰이 초기화되면서 로드아웃의 `StartupEffects`(`GE_Chaser_Sword_Init` 등, `CurrentHealth`/`MaxHealth`를 Override)를 다시 적용하므로 자동으로 원복된다. 레벨업 카드로 오른 최대 체력은 사망 정리에 걸리지 않고 남으므로, Init 다음의 `GE_Chaser_FullHeal`이 현재 체력을 보너스 포함 최대치로 채운다 (→ [LevelUp.md](../Gameplay/LevelUp.md) "리스폰 체력").

### 타이머는 게임모드가 든다

죽은 폰에 타이머를 걸면 그 폰이 어떤 이유로든 먼저 파괴될 때 리스폰이 통째로 사라진다. 그래서 **게임모드가 컨트롤러별 타이머를 `TMap`으로 들고**, `Logout`에서 나간 플레이어의 예약을 취소한다. 대기 중 컨트롤러가 사라질 수 있으므로 타이머 델리게이트는 약참조로 잡는다.

`ACBChaserCharacter::DespawnDelay = 0` 은 그대로다. **시체는 스스로 사라지지 않고 리스폰 시점에 게임모드가 폰을 교체하면서 파괴된다** — 폰을 먼저 없애면 컨트롤러가 폰을 잃어 화면·입력이 끊기기 때문. 파괴 시 `Pawn::EndPlay`가 돌면서 무기 액터와 HUD 위젯도 함께 정리된다.

**소멸·생성 연출은 캐릭터가 스스로 한다.** 게임모드는 리스폰만 하고 연출을 모른다. 플레이어의 시체 유지 시간은 `GetCorpseLifetime()` 이 게임모드의 `RespawnDelay` 로 답하므로, 소멸 연출은 리스폰 지연에서 연출 길이만큼 먼저 시작해 **폰이 교체되는 순간 끝나고**, 새 폰은 런타임 생성이라 **생성 연출로 나타난다.** ASC 가 PlayerState 에 남으므로 폰이 사라질 때(`EndPlay`) 남은 소멸 GE 를 걷어낸다 → [SpawnFX.md](../Presentation/SpawnFX.md)

AI(Rogue/Outlaw)는 이 경로를 타지 않는다. 기존대로 `DespawnDelay` 뒤 스스로 파괴되고, 다음 소환은 스포너가 전멸을 보고 결정한다 (→ [Spawner.md](../Gameplay/Spawner.md)).

### 보스전 중 사망 — 구역 밖 부활 지점

보스 구역이 보스전 중(입구 봉쇄)일 때 죽은 플레이어는 PlayerStart 가 아니라 **보스 구역의 `RespawnPoint`(구역 밖)** 에서 부활한다. 입구가 막혀 있으므로 보스전이 끝날 때까지 다시 들어가지 못한다 (→ [BossArena.md](../Gameplay/BossArena.md)).

- **부활 위치는 죽은 순간 정한다.** `Auth_HandlePlayerDeath` 가 그 자리에서 `Auth_FindBossFightRespawnTransform` 으로 위치를 구해 리스폰 타이머 람다에 싣고, `Auth_RespawnPlayer` 는 받은 위치가 있으면 `RestartPlayerAtTransform`, 없으면 `RestartPlayer` 를 부른다. 부활 시점에 물으면 전멸 리셋(보스전 종료) 뒤에 부활하는 마지막 사망자들만 맵 시작 PlayerStart 로 가 버린다.
- 게임모드는 구역을 **사망 시점에 레벨을 훑어** 찾는다. 구역이 등록·해제하지 않으므로 해제 누락이 없다.
- 두 경로 모두 `SpawnDefaultPawnAtTransform` → `GetDefaultPawnClassForController` 를 거치므로 스폰 클래스 규칙(선택 캐릭터)은 같다.

### 아직 없는 것

- **관전 화면 없음** — 죽은 뒤 리스폰까지 카메라가 시체 위치에 그대로 있다. 관전 카메라나 사망 UI는 매치 규칙과 함께 설계한다.
- 스폰 지점은 엔진 기본 규칙(`ChoosePlayerStart`)에 맡긴다(보스전 중 사망만 예외 — 위). 로비처럼 자리를 고정하지 않으므로, 적이 몰린 자리에 다시 나올 수 있다.

## 매치 종료 — 보스를 잡으면 끝난다

보스 구역의 보스를 처치하면 매치가 끝난다. **보스 시체가 사라진 뒤**(사망 몽타주 → 소멸 연출 → 파괴) 전원 화면에 **클리어 위젯**(클리어 문구 + **클리어 기록**(경과 시간·내가 넣은 데미지) + 버튼 둘)이 뜨고, 나가는 길은 둘이다.

| 버튼 | 누가 | 동작 |
|---|---|---|
| 로비로 | **호스트만** | 세션을 유지한 채 **전원을 로비로** (`ServerTravel`). 클라이언트 화면에는 버튼 대신 대기 표시 |
| 메인 메뉴로 | **각자** | 호스트를 기다리지 않고 **혼자 세션을 나가** 메인 메뉴로. 호스트가 누르면 세션이 끝나 남은 클라도 메뉴로 돌아간다(기존 접속 끊김 경로 → [Session.md](Session.md)) |

```
[서버] 매치 내내 — 적에게 데미지가 들어갈 때마다 (아래 "클리어 기록")
  └ UCBAttributeSet::PostGameplayEffectExecute → 공격한 플레이어의 ACBPlayerState::Auth_AddDamageDealt

[서버] ACBBossArena 스캔이 보스 사망 확인 → Auth_ClearArena
  ├ ACBGameplayGameMode::Auth_HandleBossDefeated()        ← 졸개 사망 처리보다 먼저
  │    ├ bMatchCleared = true   (이후 Auth_AddExperience 무시)
  │    └ MatchClearTime = 월드 시간 (레벨 시작부터, 레벨업 정지 제외)
  ├ 졸개 사망 처리 · 입구 개방 · Phase = Cleared   (스캔은 계속)
  └ 보스가 이미 없거나 스스로 사라지지 않으면(DespawnDelay ≤ 0, 경고) 바로 아래로
[서버] Cleared 스캔 — 보스 약참조가 비면 (DespawnDelay 뒤 보스가 Auth_Despawn 으로 파괴됨)
  └ ACBBossArena::Auth_FinishClear → 스캔 중지 + ACBGameplayGameMode::Auth_HandleBossDespawned()
       └ PC 마다: Client_ShowMatchClear(FCBMatchResult{ MatchClearTime, 그 플레이어의 DamageDealt }) (Reliable)
[각 클라] ACBChaserController → 기록 보관(GetMatchResult) → 클리어 위젯을 HUD Menu 레이어에 (게임플레이 IMC 가 걷힘)
       └ WBP 가 Construct 에서 GetMatchResult 로 시간·데미지 표시

[호스트] "로비로" → Server_RequestReturnToLobby()       (호스트는 그 자리에서 실행)
  └ Auth_TryReturnToLobby — 클리어됐나 / 호스트인가(IsLocalController) / 이미 이동 중인가 / LobbyLevel 지정됐나
       └ ACBGameModeBase::Auth_ServerTravelToLevel(LobbyLevel)   (?listen + ?MaxPlayers)
            └ 엔진이 PC 마다 PreClientTravel → ACBChaserController 가 클리어 위젯을 스택에서 내림 (IMC 복구)
[로비 도착] ACBLobbyGameMode
  ├ HandleSeamlessTravelPlayer → 준비 인원 재집계 (seamless 도착은 PostLogin 을 타지 않음)
  └ BeginPlay → 세션을 다시 참가 가능으로

[각자] "메인 메뉴로" → ACBChaserController::Local_LeaveMatchToMainMenu()
  ├ 클리어 위젯을 먼저 내림
  └ UCBSessionSubsystem::Local_LeaveToMainMenu()   (클라 ClientTravel / 호스트 OpenLevel)
```

| 갈래 | 담당 |
|---|---|
| 처치·소멸 판정 | `ACBBossArena` 스캔 (기존) → 게임모드에 알리기만 함 (처치 `Auth_HandleBossDefeated` / 소멸 `Auth_HandleBossDespawned`) |
| 매치 상태 | `ACBGameplayGameMode::bMatchCleared` / `bReturnToLobbyStarted` (서버 전용, 복제 안 함) |
| 클리어 화면 | `ACBChaserController::Client_ShowMatchClear` → Menu 레이어. 클래스는 `UCBGameInstance::MatchClearWidgetClass` |
| 클리어 기록 | `FCBMatchResult`(`ClearTimeSeconds`·`DamageDealt`) — 시간은 `ACBGameplayGameMode::MatchClearTime`, 데미지는 `ACBPlayerState::DamageDealt`(서버 전용 누적, `UCBAttributeSet`이 더함). 위젯은 `ACBChaserController::GetMatchResult`로 읽음 |
| 로비 복귀 | `Server_RequestReturnToLobby` → `ACBGameplayGameMode::Auth_TryReturnToLobby` → `LobbyLevel`(게임모드 BP) |
| 메인 메뉴 나가기 | `ACBChaserController::Local_LeaveMatchToMainMenu` → `UCBSessionSubsystem::Local_LeaveToMainMenu` |
| 이동 직전 위젯 제거 | `ACBChaserController::PreClientTravel` |
| 로비 도착 처리 | `ACBLobbyGameMode::HandleSeamlessTravelPlayer`(재집계) / `BeginPlay`(세션 다시 엶) → [Lobby.md](Lobby.md) |

### 왜 이렇게 했나

**판정은 구역, 규칙은 게임모드.** 보스 사망은 보스 구역이 이미 스캔으로 보고 있고(→ [BossArena.md](../Gameplay/BossArena.md)), "보스 처치 = 매치 끝"은 게임 규칙이라 게임모드 몫이다. 구역은 알리기만 한다 — 플레이어 사망을 캐릭터가 게임모드에 알리는 것과 같은 형태다.

**클리어 화면은 보스 시체가 사라진 뒤 — 시간을 재지 않고 파괴를 기다린다.** 보스가 쓰러지고 소멸 연출로 사라지는 장면을 다 보여준 다음 화면을 띄운다. 보스(AI)는 사망 후 `DespawnDelay` 뒤 스스로 파괴되고 소멸 연출도 그 순간 끝나도록 맞춰져 있으므로(→ [SpawnFX.md](../Presentation/SpawnFX.md)), 구역이 **클리어 뒤에도 스캔을 멈추지 않고 보스 약참조가 비는 순간**을 잡아 게임모드에 알린다.

- **지연 값·타이머를 따로 두지 않는다.** 두면 보스 BP 의 `DespawnDelay`와 같은 값이 두 곳에 생겨, 한쪽만 고치면 위젯이 시체보다 먼저 또는 한참 늦게 뜬다. 파괴를 기다리면 어떤 경로로 사라지든 맞는다.
- **보스 사망을 델리게이트 대신 스캔으로 보는 것과 같은 이유**(해제 누락이 없음)로 파괴도 스캔으로 본다. 위젯은 파괴 뒤 최대 스캔 주기(0.5초) 안에 뜬다.
- **경험치 차단은 처치 순간, 화면은 소멸 뒤.** 둘을 한 번에 하면 졸개 처치 보상을 막지 못하거나(소멸 때 막으면 늦음) 화면이 일찍 뜬다. 그래서 게임모드에 알리는 지점이 둘이다.
- **스스로 사라지지 않는 보스(`DespawnDelay` ≤ 0)는 기다리지 않는다.** 기다리면 화면이 영영 뜨지 않는 조용한 실패라, 경고를 남기고 처치 순간 바로 띄운다. 클리어 판정 때 보스가 이미 없을 때도 바로 띄운다.
- 기다리는 동안(기본 5초) 플레이어는 평소처럼 움직이고 싸운다. 경험치는 이미 막혀 레벨업은 없고, 죽으면 평소대로 리스폰한다. 보스 바는 보스 파괴(`EndPlay`)로 내려가므로 위젯과 같은 때 사라진다.

**클리어 상태를 복제하지 않고 RPC 로 보낸다.** 난입이 막혀 있어(`bAllowJoinInProgress = false` → [Session.md](Session.md)) 클리어 뒤에 새로 오는 클라이언트가 없고, 위젯을 띄우는 컨트롤러는 리스폰 중에도 살아 있다. 그래서 상태를 들고 있다가 나중에 온 쪽에 알려줄 일이 없다. 위젯을 캐릭터 HUD 가 아니라 컨트롤러가 띄우는 이유는 레벨업 카드와 같다 — 죽어서 리스폰을 기다리는 플레이어에게도 떠야 한다(→ [LevelUp.md](../Gameplay/LevelUp.md)).

**클리어 뒤에는 경험치를 받지 않는다.** 보스와 함께 쓰러지는 졸개도 사망 처리라 처치 보상이 들어간다(→ [BossArena.md](../Gameplay/BossArena.md) "졸개"). 그 보상으로 레벨이 오르면 월드가 정지되고 카드 창이 클리어 화면 위에 뜬다. 그래서 구역이 **졸개 사망 처리보다 먼저** 게임모드에 알리고, `Auth_AddExperience`가 `bMatchCleared`면 바로 돌아간다.

- **보스 자신의 보상은 이 가드에 걸리지 않는다.** 보스가 쓰러질 때(`Auth_OnDeath`) 바로 들어가고 클리어 판정은 그 뒤 스캔이기 때문이다. 보상으로 레벨이 오르면 정지 동안 스캔과 보스의 소멸 타이머도 멈추므로 **카드를 고른 뒤 보스가 사라지고 클리어 화면이 뜬다.** 매치가 끝나므로 보스 로드아웃의 `ExperienceReward`는 0 을 권장한다.

**클리어 기록 — 경과 시간과 내가 넣은 데미지.**

| 값 | 규칙 | 이유 |
|---|---|---|
| 경과 시간 | **보스 처치 순간의 월드 시간**(`GetTimeSeconds`). 화면이 뜨는 소멸 시점이 아니라 처치 시점 | 게임플레이 월드는 레벨이 시작될 때 0 부터 흐르므로 **시작 시각을 따로 기록하지 않는다.** 레벨업 정지 동안 월드 시간이 멈춰 **카드를 고르는 시간은 빠진다**(정지형 레벨업 장르의 클리어 타임과 같은 기준). 클라이언트가 늦게 로딩된 몇 초는 들어간다 |
| 내 데미지 | 레벨 전체(일반 몬스터·보스·졸개)에서 **적의 체력을 실제로 깎은 양**의 합. 오버킬·시체 타격은 빠짐 | 막타에 체력 10 남은 적을 500 으로 때려도 10 — 총합이 부풀지 않는다. 계산된 데미지 그대로를 원하면 `Auth_RecordDamageDealt`의 한 줄을 바꾼다 |

- **데미지는 적용되는 곳(서버의 `UCBAttributeSet::PostGameplayEffectExecute`)에서 센다.** 데미지 계산(`UCBDamageExecCalc`)은 클램프 전 값만 알고, 적중 이벤트(`Event.Combat.Attack.Hit` — 버스트 게이지가 씀)에는 데미지 값이 없다. 체력 클램프 **전에** 세야 0 아래로 간 오버킬을 뺄 수 있고, 사망 이벤트로 빠지기 전이라 막타도 포함된다. 모든 적이 같은 어트리뷰트 셋을 쓰므로 일반 몬스터·보스·졸개가 따로 갈리지 않는다.
- **누가 때렸는지는 GE 컨텍스트의 instigator ASC 로 찾는다.** 그 ASC 의 주인이 `ACBPlayerState`면 플레이어다(플레이어 ASC 는 PlayerState 소유 → [ASC-Ownership.md](../Foundation/ASC-Ownership.md)). 폰이 먼저 죽었어도 PlayerState 로 찾아가고, AI 의 공격은 주인이 캐릭터라 걸러진다.
- **누적은 PlayerState 에, 복제하지 않는다.** 리스폰해도 이어지고, 나가면 함께 사라지고, 레벨을 넘으면 새 PlayerState 라 0 부터 시작한다(`CopyProperties`에 넣지 않음). 맞을 때마다 복제할 이유가 없어, 클리어 화면 RPC 가 끝에 한 번 실어 보낸다.
- **데미지는 화면을 띄울 때 읽는다.** 그래서 보스 처치 뒤 시체가 사라지기까지(기본 5초) 다른 적에게 넣은 데미지도 들어간다. 보스와 함께 쓰러지는 졸개는 사망 이벤트로 처리되어 데미지가 없다.
- **위젯은 컨트롤러에서 읽는다(`GetMatchResult`).** 컨트롤러가 RPC 로 받은 기록을 먼저 보관하고 위젯을 만든다. 위젯 C++ 베이스에 이벤트를 두는 방식은 WBP 가 `WBP_EasyMasterWidget` 부모를 버리고 내비게이션 인터페이스를 직접 구현해야 해서(레벨업 카드 창처럼) 쓰지 않았다.
- 표시 형식(mm:ss, 데미지 반올림)은 WBP 몫이다.

**로비 복귀는 호스트만.** 세션의 주인이 호스트이므로 전원을 옮기는 것도 호스트 몫이다. 판정은 로비의 시작 버튼과 같다 — 서버에서 `IsLocalController()`인 컨트롤러는 호스트뿐이다(→ [Lobby.md](Lobby.md) "시작 버튼은 UI가 아니라 서버가 판정한다"). 버튼을 숨기는 것은 표시일 뿐이다.

**메인 메뉴 나가기는 기존 나가기 경로 그대로다.** 세션 이탈·`bLeaveRequested`(나가는 중 끊김을 경고로 띄우지 않음)가 이미 `Local_LeaveToMainMenu()` 안에 있다(→ [Session.md](Session.md) "나가기"). 컨트롤러 함수는 그 앞에서 클리어 위젯을 내리기만 한다.

**위젯은 레벨을 떠나기 직전에 내린다 — `PreClientTravel`.** 클리어 위젯은 Menu 레이어라 떠 있는 동안 게임플레이 IMC 를 걷어 둔다. HUD 와 함께 사라지면 걷은 IMC 가 되돌아오지 않는다(→ [Lobby.md](Lobby.md) "이동 전에 로비 위젯을 반드시 스택에서 뺀다"). 엔진이 이동 직전에 PC 마다 `PreClientTravel`을 부르므로 그 오버라이드에서 내린다.

| 이동 | `PreClientTravel` 을 부르는 곳 |
|---|---|
| 로비 복귀 — 호스트 | `AGameModeBase::ProcessClientTravel` 이 로컬 PC 에 직접 |
| 로비 복귀 — 클라 | `ClientTravelInternal`(엔진 RPC) 안에서. 같은 PC 채널의 신뢰성 RPC 라 이동보다 먼저 처리됨이 보장된다 |
| 메인 메뉴 — 클라 | `Local_LeaveToMainMenu` 의 `ClientTravel` → 위와 같음 |
| 메인 메뉴 — 호스트 | **부르지 않는다**(`OpenLevel`). 그래서 `Local_LeaveMatchToMainMenu`가 직접 먼저 내린다 |

- 로비 게임 시작처럼 **멀티캐스트를 새로 만들지 않는다.** 위젯을 띄운 것이 컨트롤러라 내리는 것도 컨트롤러가 하고, 엔진 훅이 "언제"를 이미 준다.
- WBP 의 "메인 메뉴로" 버튼은 `Remove This Widget`을 직접 부르지 말고 컨트롤러 함수만 부른다 — 위젯 참조를 컨트롤러가 쥐고 있어 두 번 내리게 된다.

**이동 URL 은 공용 함수다.** `?listen`과 `?MaxPlayers` 재부착(→ [Session.md](Session.md) "정원 값은 URL 옵션으로 흘려보낸다")은 빠뜨려도 조용히 틀어지는 부분이라, 로비 → 게임플레이와 게임플레이 → 로비가 `ACBGameModeBase::Auth_ServerTravelToLevel` 하나를 쓴다.

**로비 레벨은 게임플레이 게임모드 BP 가 정한다(`LobbyLevel`).** 로비의 `GameplayLevel`과 같은 형태다 — "다음에 어디로 가나"는 그 레벨 게임모드의 규칙이다. 메인 메뉴의 `New Game Level`(Global Config)도 로비를 가리키지만 C++ 이 BP 데이터 에셋 필드를 읽을 수 없고, 등록소의 주인이 다르다.

### 로비에 돌아오면

| 무엇 | 어떻게 |
|---|---|
| 준비 인원 | **seamless 도착은 `PostLogin`을 타지 않는다**(`HandleSeamlessTravelPlayer` → `InitSeamlessTravelPlayer` → `GenericPlayerInitialization` → `HandleStartingNewPlayer`). 그대로면 준비 인원이 0/0 으로 보이고 방 목록 인원도 틀린다 → `ACBLobbyGameMode::HandleSeamlessTravelPlayer`가 다시 센다 (→ [Lobby.md](Lobby.md)) |
| 세션 | 매치 시작 때 닫은 세션을 `ACBLobbyGameMode::BeginPlay`가 다시 연다 (→ [Session.md](Session.md)) |
| 로비 UI·페이드 | 처음 들어올 때와 같다 — PC 가 새로 만들어져 1회성 플래그가 초기화되고 `ReceivedPlayer`가 화면을 다시 덮는다 (위 "Seamless Travel") |
| 이어지는 것 | 의상·고른 캐릭터(`CopyProperties`), 닉네임(엔진 기본 복사) |
| 사라지는 것 | 레벨·경험치(새 게임 스테이트), 레벨업 카드 효과(새 PlayerState 의 새 ASC), 준비 상태(`false`로 시작) |

### 검토했지만 채택하지 않은 것

| 안 | 이유 |
|---|---|
| `AGameMode`(MatchState)로 전환 | 게임 스테이트 계층을 `AGameState`로 갈아야 하고 스폰·리스폰 기본 동작이 묶임 (위 "게임모드 계층") |
| 클리어 상태를 게임 스테이트에 복제 | 늦게 오는 클라가 없고(난입 거부) 컨트롤러는 리스폰에도 살아 있음. 복제할 이유가 없음 |
| 클리어 위젯 C++ 베이스 클래스 | 클리어 기록이 생긴 뒤에도 쓰지 않음 — 베이스를 두면 WBP 가 `WBP_EasyMasterWidget` 부모를 버리고 내비게이션 인터페이스를 직접 구현해야 함. 기록은 컨트롤러 getter, 버튼은 컨트롤러 함수·RPC 직접 호출, 호스트 판정은 `Is Server` 노드 |
| 데미지를 PlayerState 에 실시간 복제 | 맞을 때마다 복제가 나가는데 쓰는 것은 마지막 한 번. 파티원 데미지 표가 필요해지면 다시 봄 |
| 클리어 시간을 게임 스테이트에 복제 | 클리어 화면 RPC 하나에 실으면 충분 |
| 적중 이벤트에 데미지 값을 실어 집계 | 이벤트 쪽은 클램프 전 값만 있어 오버킬·시체 타격을 뺄 수 없음 |
| 매치 시작 시각 기록 | 게임플레이 월드 시간이 레벨 시작 때 0 부터 흘러 처치 순간의 월드 시간이 곧 경과 시간 |
| 경과 시간에 레벨업 정지 포함(실제 시간) | 카드를 고르는 동안은 시계를 멈추기로 함 |
| 클리어 화면 표시 지연 값·타이머 | 보스 `DespawnDelay`와 값이 두 곳에 생김. 스캔으로 보스 파괴를 기다림 (위 "클리어 화면은 보스 시체가 사라진 뒤") |
| 보스 `OnDestroyed` 구독 | 다이내믹 델리게이트라 `UFUNCTION` 핸들러·해제 관리가 따라옴. 이미 도는 스캔으로 충분 |
| 전원이 확인해야 로비로 | 호스트가 결정하기로 함. 확인 집계·이탈 처리·잠수 대비 타임아웃이 따라옴 |
| 로비 레벨을 세션 서브시스템이 기억(연 레벨) | PIE 를 게임플레이 맵에서 바로 시작하면 돌아갈 곳이 없음 |
| 이동 직전 위젯 제거를 멀티캐스트로 | 엔진 `PreClientTravel`이 모든 PC 에 같은 시점을 줌 |
| 클리어 뒤 무적·스포너 정지 | 입력이 걷혀 있고 보스 구역 밖 적만 해당 → 아래 한계 |

### 알려진 한계

- **클리어 화면 동안 보스 구역 밖 적은 그대로다.** 입력이 막힌 채 맞을 수 있다(죽으면 평소대로 리스폰).
- **로비에서의 조작 차단은 이 경로에서 로비 위젯에 기댄다.** 게임플레이 IMC 는 `ULocalPlayer`에 남아 로비로 넘어오고(메인 메뉴를 거쳐 다시 방을 열어도 같다), 로비 위젯(`Should Remove Gameplay IMCs`)이 그것을 걷는다. 입력 바인딩은 로드아웃이 적용될 때 생기고 로비 위젯은 준비 완료 신호에 뜨므로, 열려 있는 틈은 **그 사이(의상 소프트 참조 로드, 검은 화면)** 뿐이라 그대로 둔다 — 그동안 이동 키를 누르고 있어야 시작 자리에서 조금 밀린다. 로비 위젯이 아예 뜨지 못하면 로비에서 조작되는데, 처음 들어올 때는 IMC 가 없어 생기지 않는 실패다(→ [Lobby.md](Lobby.md) "로비 조작 차단은 UI에 기대지 않는다"). 레벨을 떠날 때 IMC 를 걷는 방식은 리스폰이 IMC 잔류에 기대는 지금 입력 흐름(`bReadyStateApplied` 1회성)까지 고쳐야 해서 채택하지 않았다.
- **위젯이 내려간 뒤 이동 로딩 동안 잠깐 조작된다.** 로비 → 게임플레이 이동과 같다.
- **클리어 위젯 클래스가 없으면 경고만 남는다.** 호스트가 로비로 돌아갈 버튼이 없으므로 각자 일시정지 메뉴로 나가야 한다.

### 에디터 작업

| 대상 | 작업 |
|---|---|
| `WBP_CB_GameClear` | 부모 `WBP_EasyMasterWidget`. 선언값은 Menu 레이어 그대로(Input Config ✅ / Focus Handler ✅ / Global Occluder ❌ / Remove Gameplay IMCs ✅ → [EasyGameUI.md](../Presentation/EasyGameUI.md)), 배경을 막는 요소 하나 |
| ↳ 클리어 기록 | `Event Construct`(Parent 먼저) → `Get Owning Player` → `Cast to CBChaserController` → `Get Match Result` → `Break CBMatchResult`. 시간은 `Clear Time Seconds`를 분·초로 나눠(`Floor(초/60)`, `초 % 60`) mm:ss, 데미지는 `Round` 후 텍스트 |
| ↳ "로비로" 버튼 | `Get Owning Player` → `Cast to CBChaserController` → `Server_RequestReturnToLobby`. **`Is Server`가 아니면(클라) 비활성화**하고 "호스트를 기다리는 중" 표시 — 막는 것은 서버라 표시일 뿐. 호스트가 누르면 비활성화(서버도 중복을 막음) |
| ↳ "메인 메뉴로" 버튼 | 호스트·클라 모두 활성. 같은 캐스트 → `Local_LeaveMatchToMainMenu`. `Remove This Widget`은 부르지 않음. **호스트가 누르면 세션이 끝나 클라도 메인 메뉴로 나가므로** 호스트 쪽에만 확인 모달을 두는 것도 고려 |
| ↳ 포커스 대상 | `Get Widget Desired Focus Target` — 호스트는 "로비로", 클라는 "메인 메뉴로". 비활성 버튼은 포커스를 받지 못해 게임패드로 아무것도 고를 수 없게 됨 |
| `GI_EasyMainGameInstance` | `Match Clear Widget Class` = `WBP_CB_GameClear` |
| `GM_CB_Gameplay` | `Lobby Level` = `L_CB_Lobby` |
| 보스 로드아웃 (`DA_Loadout_Outlaw_Hulk`) | `ExperienceReward` = 0 권장 (위 "클리어 뒤에는 경험치를 받지 않는다") |
| 공통 | 새 `UPROPERTY`·`UFUNCTION`이 있으므로 **에디터 재시작** |

### 검증 (2인 리슨 서버)

1. 보스 처치 → 보스 사망 몽타주·소멸 연출·졸개 쓰러짐이 그대로 진행되고, 그동안 캐릭터는 조작된다. **보스가 사라지고(보스 바도 사라짐) 0.5초 안에** 호스트·클라 모두 클리어 위젯이 뜨고 조작이 막힌다. 서버 로그에 "보스 처치" → (`DespawnDelay` 뒤) "보스 소멸 - 클리어 화면"
   - 보스 BP 의 `DespawnDelay`를 바꾸면 위젯이 뜨는 시점도 그만큼 따라 바뀐다. 0 으로 두면 경고 로그와 함께 처치 순간 바로 뜬다
2. 클라 화면에는 "로비로" 버튼이 없고 대기 표시, 호스트에게만 있다
3. 호스트 "로비로" → 둘 다 로비에 도착해 로비 UI 가 뜬다. 의상·고른 무기 유지, 준비 인원이 "0/2" 로 맞게 보인다. 다시 준비 → 시작하면 게임플레이에서 정상 조작
4. 다른 인스턴스에서 방을 검색하면 로비로 돌아온 방이 참가 가능 + 인원 2 로 보인다
5. 클라 "메인 메뉴로" → 클라만 메인 메뉴, 접속 끊김 경고 없음. 호스트 화면은 클리어 위젯 그대로이고 이어서 "로비로"를 누르면 혼자 로비로
6. 호스트 "메인 메뉴로" → 클라도 메인 메뉴로 돌아간다
7. 죽어서 리스폰을 기다리는 동안 보스가 쓰러져도 그 플레이어에게 클리어 위젯이 뜬다
8. 졸개 `ExperienceReward`를 0 보다 크게 둬도 클리어 뒤 레벨업 정지가 일어나지 않는다
9. `LobbyLevel`을 비우고 "로비로" → 에러 로그만 남고 이동하지 않는다
10. 메인 메뉴로 나간 뒤 다시 방을 열어 게임플레이까지 가도 정상 조작된다 (위젯을 내리고 나갔으므로)
11. 클리어 위젯의 경과 시간이 서버 로그 "매치 클리어 N초" 와 같다. 레벨업 카드를 고르며 오래 기다린 판은 그 시간만큼 실제 시간보다 짧다
12. 클리어 위젯의 데미지가 호스트·클라 화면에서 서로 다르다(각자 자기 것). 혼자 보스를 잡으면 보스 최대 체력 + 잡은 일반 몬스터·졸개 체력 합 언저리 — 체력이 남은 적을 때린 만큼만 더해지고, 막타 오버킬은 안 들어감
13. 중간에 죽었다 리스폰해도 그 전 데미지가 유지된다. 로비로 돌아갔다 다시 시작하면 0 부터

## 진행 상황

**완료**
- 메인 메뉴 레벨 + `GM_CB_MainMenu` (표시 확인) → [EasyGameUI.md](../Presentation/EasyGameUI.md)
- `ACBGameModeBase` 공통 기본값 (seamless travel, PC·PlayerState 클래스)
- `ACBLobbyGameMode` / `ACBGameplayGameMode` 생성 (빈 껍데기)
- `ACBPlayerState::CopyProperties` — 의상 조합 이관
- **로비 → 게임플레이 의상 이관** — `CopyProperties` + 준비 완료까지 기다렸다 적용 (2인 확인)
- **무기(캐릭터) 변경 C++ 경로** — 카탈로그·선택 값 복제·스폰 클래스 결정·같은 자리 재스폰·ASC 부여분 회수 (위 "무기 변경 = 캐릭터 변경"). **컴파일까지 확인, 에셋·위젯 배선과 실행 검증 남음**
- **사망 후 리스폰** — `ACBGameplayGameMode`가 컨트롤러별 타이머로 5초 뒤 `RestartPlayer`, 스폰 전에 사망 GE·로드아웃 부여분 회수 (위 "사망과 리스폰"). **컴파일까지 확인, 실행 검증 남음**
- **매치 종료** — 보스 처치 → 보스가 사라진 뒤 전원 클리어 위젯 → 호스트가 전원 로비 복귀(세션 유지) / 각자 메인 메뉴 나가기, 로비 도착 시 인원 재집계·세션 재개방 (위 "매치 종료"). **컴파일까지 확인, WBP·에셋 배선과 실행 검증 남음**

**미구현**
- **무기(캐릭터) 변경 UI 배선** — C++ 창구는 완료(위 "무기 변경 = 캐릭터 변경"). 남은 것: `UCBCharacterCatalog` 데이터 에셋 작성, 무기별 캐릭터 BP·로드아웃, `GI_EasyMainGameInstance`에 카탈로그 지정, 로비 위젯의 버튼 목록 + `Server_RequestCharacterSelection` 호출 + `OnCharacterSelectionChanged` 구독
- 게임플레이 레벨의 게임모드 BP (`ACBGameplayGameMode` 파생) + 로비 게임모드의 `GameplayLevel` 지정 — **없으면 시작해도 이동할 곳이 없다**
- 게임플레이 매치 규칙 — **사망 후 리스폰·보스 처치 클리어는 구현됨** (위 "사망과 리스폰", "매치 종료"). 패배 조건은 없다(보스전 전멸은 보스전만 처음부터 → [BossArena.md](../Gameplay/BossArena.md))
- Global Config → `New Game Level`을 `L_CB_Lobby`로 지정
- **로딩 시스템 (①·② 계층)** — MoviePlayer + `Transition Map`. **맵 전환(메인 메뉴 → 로비 → 게임플레이)을 붙일 때 한 덩어리로 설계한다.** 지금 레벨을 직접 열면 화면을 덮기 전 몇 프레임이 노출되는데, 그 원인이자 해법이 여기다 (→ [Lobby.md](Lobby.md) "이 페이드는 로딩 화면이 아니다")

**확인됨**
- `GM_CB_GameModeBase`(구 `BP_GameModeBase` — 리네임됨)에 `Player Controller Class` / `Use Seamless Travel` 오버라이드 없음. C++ 베이스 기본값이 그대로 적용된다

## 관련 문서

- 접속·세션·방 만들기·나가기: [Session.md](Session.md)
- 로비 레벨 안에서 일어나는 일: [Lobby.md](Lobby.md)
- 매치 종료의 시작점(보스 처치 판정): [BossArena.md](../Gameplay/BossArena.md)
- 서버 권위·접두사·복제 정책: [Multiplayer.md](../Conventions/Multiplayer.md)
- 시스템 UI·메인 메뉴 구현: [EasyGameUI.md](../Presentation/EasyGameUI.md)
- HUD·캐릭터 UI의 로컬 원칙: [UI.md](../Presentation/UI.md)
- 캐릭터 에셋 주입: [Loadout.md](../Foundation/Loadout.md)
