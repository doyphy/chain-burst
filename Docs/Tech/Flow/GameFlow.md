# 게임 플로우 (레벨·게임모드·전환)

> 어떤 레벨들이 있고, 그 사이를 어떻게 오가며, **무엇이 살아남는가**. 레벨을 추가하거나 게임모드를 만들 때, 맵을 넘어 데이터를 넘겨야 할 때 이 문서를 따른다. **폰을 다시 스폰하는 두 갈래**(무기 변경·리스폰)도 여기 있다.
>
> 접속·세션·나가기는 [Session.md](Session.md), 로비 레벨 안에서 일어나는 일은 [Lobby.md](Lobby.md)로 나뉘어 있다. 서버 권위·접두사·복제 정책 같은 **일반 규칙**은 [Multiplayer.md](../Conventions/Multiplayer.md). 여기는 **구체적인 흐름**만 다룬다.

## 전체 흐름

```
메인 메뉴  ──(호스트)──▶  로비  ──(모두 준비)──▶  게임플레이
    │                      ▲
    └──────(참가)──────────┘
```

- **메인 메뉴**는 세션 밖이다. 항상 단독 실행이며 서버·클라이언트 구분이 없다.
- **로비부터** 세션 안이다. 복제되는 월드이고, 게임플레이 레벨과 같은 규칙이 적용된다.
- 참가자는 메인 메뉴에서 **로비로 곧장 travel**한다.

## 레벨과 게임모드

| 레벨 | 게임모드 | 역할 |
|---|---|---|
| `L_CB_MainMenu` | `GM_CB_MainMenu` (BP만) | 메뉴 표시. **Default Pawn = None**, 규칙 없음 |
| `L_CB_Lobby` | `ACBLobbyGameMode` | 외형 커스터마이징 대기, 준비 완료 판정, `ServerTravel` |
| 게임플레이 | `ACBGameplayGameMode` | 매치 규칙 (승패·리스폰) |

메인 메뉴만 C++ 클래스가 없다 — 담을 규칙이 없어 BP 설정(HUD·Default Pawn)만으로 충분하다. → [EasyGameUI.md](../Presentation/EasyGameUI.md)

### 게임 스테이트 계층

```
ACBGameStateBase (AGameStateBase)   ← 플레이어 목록 변경 신호(OnPlayerListChanged)
 └─ ACBLobbyGameState               ← 준비 인원 집계
```

`ACBGameModeBase` 생성자가 `GameStateClass`를 `ACBGameStateBase`로 고정하고, 로비 게임모드만 자기 것으로 덮는다. 목록 신호는 HUD 플레이어 목록이 쓴다 → [UI.md](../Presentation/UI.md)

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

**`AGameModeBase` vs `AGameMode`는 미결이다.** 후자는 `MatchState`(대기·진행·종료) 상태 머신을 제공한다. 로비의 "모두 준비 → travel" 정도는 `AGameModeBase`로 충분하므로, 매치 시작·종료 판정이 실제로 필요해질 때 **게임플레이 게임모드만** 갈아타는 것을 검토한다.

## 레벨 전환

| 상황 | 방법 |
|---|---|
| 메인 메뉴 → 로비 (호스트) | `OpenLevel` + **`listen`** — `UCBSessionSubsystem::Local_HostLobby()` → [Session.md](Session.md) |
| 메인 메뉴 → 로비 (참가) | **`ClientTravel`** — `UCBSessionSubsystem::Local_JoinServerByAddress()` → [Session.md](Session.md) |
| 방 만들기 / 방 찾기 | 세션 생성·검색·참가 후 위 두 경로로 수렴 — `UCBSessionSubsystem` → [Session.md](Session.md) |
| 로비 → 게임플레이 (전원) | **`ServerTravel`** — 접속한 클라이언트가 함께 이동 |
| 로비 → 메인 메뉴 (나가기) | 클라 **`ClientTravel`** / 호스트 `OpenLevel` — `UCBSessionSubsystem::Local_LeaveToMainMenu()` → [Session.md](Session.md) |

### Seamless Travel — 켜져 있어야 데이터가 넘어간다

`bUseSeamlessTravel = false`면 클라이언트가 **완전히 재접속**하는 형태라 PlayerState가 백지에서 시작한다. **에러 없이 조용히 값이 사라지므로** 새 게임모드를 만들 때마다 확인 대상이다(베이스에서 고정한 이유).

- **전환 맵**: Project Settings → Maps & Modes → `Transition Map`. 비워두면 엔진이 임시 빈 월드를 쓰지만 명시하는 편이 안전하다.
- **PlayerController 클래스 일치**: 로비와 게임플레이가 다르면 travel 중 PC가 새로 만들어진다. 베이스에서 고정해 자동으로 일치시킨다.

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
| 사망 상태 정리 | `ACBGameplayGameMode::Auth_ClearDeathState` |
| 실제 재스폰 | `ACBGameplayGameMode::Auth_RespawnPlayer` → `RestartPlayer` |

### ASC가 PlayerState 소유라 청소가 필수다

플레이어의 ASC는 폰이 아니라 `ACBPlayerState`가 갖는다 (→ [ASC-Ownership.md](../Foundation/ASC-Ownership.md)). **폰을 갈아도 ASC는 그대로 살아남으므로**, 청소 없이 재스폰하면:

- `GE_Dead`가 남아 새 캐릭터가 태어나자마자 `Status.Dead` 상태가 되고, `UCBGameplayAbility::CanActivateAbility`가 모든 어빌리티를 막는다
- 이전 로드아웃이 부여한 어빌리티·이펙트가 새 폰의 부여분과 겹친다

그래서 스폰 **전에** 두 가지를 걷어낸다. 사망 GE는 클래스가 아니라 **부여 태그(`Status.Dead`)로 제거**한다 — 어떤 GE를 쓸지는 사망 어빌리티 BP가 정하므로 게임모드가 클래스를 알 수 없다. 로드아웃 부여분 회수는 로비의 캐릭터 변경과 같은 `UCBAbilitySystemComponent::Auth_ClearLoadoutGrants()`를 쓴다.

**체력은 따로 되돌리지 않는다.** 새 폰이 초기화되면서 로드아웃의 `StartupEffects`(`GE_Chaser_Sword_Init` 등, `CurrentHealth`/`MaxHealth`를 Override)를 다시 적용하므로 자동으로 원복된다.

### 타이머는 게임모드가 든다

죽은 폰에 타이머를 걸면 그 폰이 어떤 이유로든 먼저 파괴될 때 리스폰이 통째로 사라진다. 그래서 **게임모드가 컨트롤러별 타이머를 `TMap`으로 들고**, `Logout`에서 나간 플레이어의 예약을 취소한다. 대기 중 컨트롤러가 사라질 수 있으므로 타이머 델리게이트는 약참조로 잡는다.

`ACBChaserCharacter::DespawnDelay = 0` 은 그대로다. **시체는 스스로 사라지지 않고 리스폰 시점에 게임모드가 폰을 교체하면서 파괴된다** — 폰을 먼저 없애면 컨트롤러가 폰을 잃어 화면·입력이 끊기기 때문. 파괴 시 `Pawn::EndPlay`가 돌면서 무기 액터와 HUD 위젯도 함께 정리된다.

AI(Rogue/Outlaw)는 이 경로를 타지 않는다. 기존대로 `DespawnDelay` 뒤 스스로 파괴되고, 다음 소환은 스포너가 전멸을 보고 결정한다 (→ [Spawner.md](../Gameplay/Spawner.md)).

### 아직 없는 것

- **관전 화면 없음** — 죽은 뒤 리스폰까지 카메라가 시체 위치에 그대로 있다. 관전 카메라나 사망 UI는 매치 규칙과 함께 설계한다.
- 스폰 지점은 엔진 기본 규칙(`ChoosePlayerStart`)에 맡긴다. 로비처럼 자리를 고정하지 않으므로, 적이 몰린 자리에 다시 나올 수 있다.

## 진행 상황

**완료**
- 메인 메뉴 레벨 + `GM_CB_MainMenu` (표시 확인) → [EasyGameUI.md](../Presentation/EasyGameUI.md)
- `ACBGameModeBase` 공통 기본값 (seamless travel, PC·PlayerState 클래스)
- `ACBLobbyGameMode` / `ACBGameplayGameMode` 생성 (빈 껍데기)
- `ACBPlayerState::CopyProperties` — 의상 조합 이관
- **로비 → 게임플레이 의상 이관** — `CopyProperties` + 준비 완료까지 기다렸다 적용 (2인 확인)
- **무기(캐릭터) 변경 C++ 경로** — 카탈로그·선택 값 복제·스폰 클래스 결정·같은 자리 재스폰·ASC 부여분 회수 (위 "무기 변경 = 캐릭터 변경"). **컴파일까지 확인, 에셋·위젯 배선과 실행 검증 남음**
- **사망 후 리스폰** — `ACBGameplayGameMode`가 컨트롤러별 타이머로 5초 뒤 `RestartPlayer`, 스폰 전에 사망 GE·로드아웃 부여분 회수 (위 "사망과 리스폰"). **컴파일까지 확인, 실행 검증 남음**

**미구현**
- **무기(캐릭터) 변경 UI 배선** — C++ 창구는 완료(위 "무기 변경 = 캐릭터 변경"). 남은 것: `UCBCharacterCatalog` 데이터 에셋 작성, 무기별 캐릭터 BP·로드아웃, `GI_EasyMainGameInstance`에 카탈로그 지정, 로비 위젯의 버튼 목록 + `Server_RequestCharacterSelection` 호출 + `OnCharacterSelectionChanged` 구독
- 게임플레이 레벨의 게임모드 BP (`ACBGameplayGameMode` 파생) + 로비 게임모드의 `GameplayLevel` 지정 — **없으면 시작해도 이동할 곳이 없다**
- 게임플레이 매치 규칙 (승패 판정 등) — **사망 후 리스폰은 구현됨** (위 "사망과 리스폰")
- Global Config → `New Game Level`을 `L_CB_Lobby`로 지정
- **로딩 시스템 (①·② 계층)** — MoviePlayer + `Transition Map`. **맵 전환(메인 메뉴 → 로비 → 게임플레이)을 붙일 때 한 덩어리로 설계한다.** 지금 레벨을 직접 열면 화면을 덮기 전 몇 프레임이 노출되는데, 그 원인이자 해법이 여기다 (→ [Lobby.md](Lobby.md) "이 페이드는 로딩 화면이 아니다")

**확인됨**
- `GM_CB_GameModeBase`(구 `BP_GameModeBase` — 리네임됨)에 `Player Controller Class` / `Use Seamless Travel` 오버라이드 없음. C++ 베이스 기본값이 그대로 적용된다

## 관련 문서

- 접속·세션·방 만들기·나가기: [Session.md](Session.md)
- 로비 레벨 안에서 일어나는 일: [Lobby.md](Lobby.md)
- 서버 권위·접두사·복제 정책: [Multiplayer.md](../Conventions/Multiplayer.md)
- 시스템 UI·메인 메뉴 구현: [EasyGameUI.md](../Presentation/EasyGameUI.md)
- HUD·캐릭터 UI의 로컬 원칙: [UI.md](../Presentation/UI.md)
- 캐릭터 에셋 주입: [Loadout.md](../Foundation/Loadout.md)
