# 로비 (`L_CB_Lobby`)

> 로비 레벨 안에서 일어나는 일 — 캐릭터 스폰, UI 신호 분담, 닉네임, 준비 완료와 게임 시작, 고정 카메라와 페이드.
>
> 접속·세션은 [Session.md](Session.md), 레벨·게임모드 구조와 **무기(캐릭터) 변경 = 폰 재스폰** 규칙은 [GameFlow.md](GameFlow.md).

## 로비는 실제 Chaser를 스폰한다

프리뷰 전용 액터를 만들지 않고 **`ACBChaserCharacter`를 그대로 스폰**한다.

| | 프리뷰 전용 액터 | **캐릭터 스폰 (채택)** |
|---|---|---|
| 복제·배치 | 직접 구현 | **엔진 기본 스폰 흐름** (PlayerStart) |
| 프리뷰 정확도 | 실제와 어긋날 수 있음 | **완전 일치** |
| 카탈로그 주입 | 별도 경로 신설 | **로드아웃 경로 그대로** |
| 로드 비용 | 적음 | 큼 (**로비 대기 중에 미리 로드되어 게임 진입이 빨라짐**) |

대신 로비에서 처리할 것:

- **캐릭터가 조작되면 안 된다** → **로비에서는 매핑 컨텍스트를 아예 등록하지 않는다.** `ACBChaserController`가 로비가 아닐 때만 `AllowGameplayInput()`을 호출한다 → [Input.md](../Gameplay/Input.md)
- **카메라** → 빙의하면 3인칭 뷰가 되므로 고정 카메라로 덮어쓴다 (아래)

## 로비 UI는 누가 띄우나 — 신호는 C++, 표시는 BP

팩의 스택 API가 **BP 전용**이라 C++에서 위젯을 올리려면 `ACBHUD`에 다리를 하나 더 뚫어야 하고, 그만큼 **팩 에셋 수정 항목이 늘어난다**(→ [EasyGameUI.md](../Presentation/EasyGameUI.md)). 그래서 표시는 BP에 두고 **C++은 "언제인지"만 알려준다.**

| 역할 | 담당 |
|---|---|
| 델리게이트 **타입** 선언 | `Types/CBDelegates.h` |
| 델리게이트 **인스턴스** 소유 | **`UCBLocalReadySubsystem`** (`UWorldSubsystem`) |
| **방송** | `ACBChaserController::Local_ApplyReadyState()` |
| **구독 + 위젯 삽입** | 레벨에 배치한 위젯 컨트롤러 BP 액터 |

**왜 컨트롤러가 델리게이트를 들고 있으면 안 되나:** 레벨 배치 액터의 `BeginPlay`는 맵 로드 직후인데, **클라이언트의 PlayerController는 복제로 나중에 온다.** 구독하려는 순간 컨트롤러가 없어 구독 자체가 실패한다(호스트만 되는 형태). 월드 서브시스템은 월드와 함께 먼저 생성되므로 이 문제가 없다.

**왜 GameInstance가 아닌가:** 신호는 **레벨 하나짜리 수명**이다. GameInstance에 두면 맵을 넘어 살아남아 수동 리셋이 필요해진다 — 그릇이 내용물보다 오래 사는 것이 곧 스코프가 틀렸다는 신호다. 월드 서브시스템은 맵과 함께 사라져 리셋이 필요 없다.

**구독하는 쪽은 두 갈래를 모두 처리해야 한다.**

```
BeginPlay
 ├ OnLocalPlayerReady 구독
 └ IsLocalPlayerReady() 확인 → 이미 true 면 즉시 처리
```

리슨 서버 호스트는 **빙의가 매우 일러서 방송이 액터 `BeginPlay`보다 먼저 갈 수 있다.** 구독만 하면 호스트에서 위젯이 뜨지 않는다. [SystemReady.md](../Foundation/SystemReady.md)의 "구독 또는 즉시" 패턴과 같다.

> **`OnLocalPlayerReady`는 1회성이다 — 캐릭터 재스폰에는 쓸 수 없다.** `UCBLocalReadySubsystem::NotifyLocalPlayerReady()`와 `ACBChaserController::Local_ApplyReadyState()` 양쪽에 one-shot 가드가 있어, 첫 캐릭터가 준비될 때 한 번 방송하고 끝난다(페이드 인이 두 번 일어나지 않게 하려는 의도이므로 가드를 풀면 안 된다). **무기 변경으로 폰이 재스폰될 때마다 필요한 신호는 `ACBPlayerState::OnCharacterLoadedChanged`** 다(→ [GameFlow.md](GameFlow.md) "무기 변경 = 캐릭터 변경"). 둘은 이름이 비슷하지만 수명이 다르다 — "레벨에 처음 들어왔나"와 "지금 폰이 로드됐나"의 차이다.

> **선언은 타입이지 인스턴스가 아니다.** 공용 헤더에 `DECLARE_DYNAMIC_MULTICAST_DELEGATE`를 두어도 공유되는 것은 타입뿐이며, 실제 신호는 인스턴스를 소유한 객체가 갖는다. `static` 전역 델리게이트는 **BP에서 바인딩할 수 없고**(BlueprintAssignable은 UObject의 UPROPERTY여야 한다), PIE는 서버·클라가 한 프로세스라 **인스턴스가 공유되어 신호가 섞인다.**

### 로비 조작 차단은 UI에 기대지 않는다

처음에는 "로비 UI가 `Menu` 레이어에 뜨면 게임플레이 IMC가 걷힌다"로 설계했으나 **두 가지가 깨져서 바꿨다.**

| 문제 | 내용 |
|---|---|
| **순서 경합** | 팩의 제거는 **그 시점의 스냅샷**을 대상으로 한다. UI가 캐릭터보다 먼저 뜨면 걷어낼 IMC가 없고, 그 뒤 로드아웃이 붙인 IMC는 사정권 밖이라 **로비에서 캐릭터가 움직인다** |
| **UI 의존** | 클라이언트에서 UI 삽입이 실패하면(HUD가 늦게 생기는 경우) 조작이 그대로 살아난다 |

**해법: 로비에서는 IMC를 붙이지 않는다.** 걷어낼 것이 없으므로 순서도 UI 성공 여부도 무관해진다. `ACBChaserController::Local_TryAllowGameplayInput()`이 `ACBLobbyCamera` 유무로 판별해, **게임플레이 레벨에서만** `AllowGameplayInput()`을 호출한다 → [Input.md](../Gameplay/Input.md)

"캐릭터는 게임플레이 레벨에서만 조작된다"는 사실을 코드에 드러낸 것이고, **실패 모드도 조용한 쪽(로비에서 움직임)에서 시끄러운 쪽(게임플레이에서 안 움직임)으로 뒤집힌다.**

> 이 판별이 `ACBLobbyCamera`에 의존하므로, 카메라 액터가 **"로비 표식" 역할을 겸하게 됐다.** 로비 판별에 걸리는 것이 더 늘어나면 복제되는 별도 표식(GameState 플래그 등)으로 옮긴다.

**회전은 배치만으로 해결된다.** `UCBCharacterRotationComponent`는 **가속도가 있을 때만** 회전 타겟을 갱신하고 초기값은 스폰 당시 액터 회전이다. 입력이 걷힌 로비에서는 `PlayerStart`의 Yaw가 그대로 유지되므로, 스타트를 카메라 쪽으로 돌려두면 캐릭터가 정면을 본다. 코드가 필요 없다.

## 닉네임 — 새 변수를 만들지 않고 엔진 `PlayerName`을 쓴다

**닉네임 저장 필드를 따로 두지 않는다.** `APlayerState`의 `PlayerName`이 이미 복제되고, `CopyProperties`로 맵도 넘어가고, `SetPlayerName()`은 **리슨 서버·단독 실행이면 `OnRep_PlayerName()`을 직접 호출**해 준다(`PlayerState.cpp:276`). 커스텀 필드를 만들면 이 셋을 전부 다시 짜야 하는 데다, 엔진·플러그인이 보는 이름과 화면에 보이는 이름이 갈라진다.

그래서 `ACBPlayerState`가 하는 일은 **변경 신호를 얹는 것뿐**이다.

```cpp
void ACBPlayerState::OnRep_PlayerName()
{
    Super::OnRep_PlayerName();                             // 이전 이름 갱신·환영 메시지
    OnPlayerNicknameChanged.Broadcast(GetPlayerName());    // 위젯이 표기를 갱신
}
```

> 다른 값들과 달리 **`Auth_*` 함수에서 `OnRep_`을 수동으로 다시 부르지 않는다.** `SetPlayerName`이 리슨 서버에서 알아서 불러주기 때문이다. (→ [Multiplayer.md](../Conventions/Multiplayer.md) "서버는 OnRep이 안 불린다")

### 기본 닉네임은 게임모드가 매긴다 — 엔진 기본값은 `Player256`

엔진의 `AGameModeBase::InitNewPlayer`는 접속 옵션에 이름이 없으면 `DefaultPlayerName + PlayerId`로 이름을 짓는데, **`AGameSession::GetNextPlayerID()`가 256부터 시작**한다(0·255가 특수값). 그대로 두면 첫 플레이어가 `Player256`이 된다.

`ACBGameModeBase::InitNewPlayer`가 `Super`를 먼저 부른 뒤(세션 등록·스폰 지점 배정이 거기 있다) 이름만 다시 정한다.

```
InitNewPlayer(Options)
 ├ Super::InitNewPlayer(...)         ← 엔진 기본 처리 (Player256 이 붙음)
 ├ Auth_MakeDefaultNickname()        ← "Player1", "Player2" ...  (접속 옵션은 보지 않음)
 └ ChangeName(...)                   ← SetPlayerName + K2_OnChangeName 훅
```

**접속 URL 의 이름 옵션(`?Name=`)은 일부러 읽지 않는다.** 엔진은 `ULocalPlayer::GetNickname()` 값을 URL 에 **자동으로** 실어 보낸다 — 호스트의 로컬 플레이어는 `LocalPlayer.cpp:299`(`SpawnPlayActor`), 원격 클라이언트는 `PendingNetGame.cpp:328`(`SendInitialJoin`)에서 붙인다. 그런데 그 값의 출처가 온라인 서브시스템이라 접속마다 규칙이 달라진다.

| 서브시스템 | `GetNickname()` 결과 |
|---|---|
| Null (현재) | `"<HostName>-<LoginId>"` — 컴퓨터 이름이 그대로 노출됨 (`OnlineIdentityNull.cpp:75`) |
| EOS | 계정 표시 이름. 단 로그인 계정을 못 찾으면 **빈 문자열** (`UserManagerEOS.cpp:2320`) |
| Steam 등 플랫폼 | `GetPlayerPlatformNickname()` 이 먼저 잡혀 페르소나 이름 |

이 값을 존중하면 (1) 서브시스템·로그인 상태에 따라 기본 닉네임 규칙이 갈리고, (2) `Auth_MakeDefaultNickname()` 이 사실상 죽은 코드가 되며, (3) **클라이언트가 보내는 값이라 위조를 막을 수 없다**(`open <주소>?Name=호스트`). 그래서 **접속 시점에는 무조건 우리 번호를 매기고**, 실제 이름은 아래 변경 요청 경로로만 정한다. 서브시스템을 나중에 EOS·Steam 으로 바꿔도 동작이 그대로다.

**앞부분(`Player`)은 엔진이 채워 주지 않는다.** `AGameModeBase::DefaultPlayerName`은 `EditAnywhere` FText인데 **엔진 어디에서도 값을 넣지 않아 비어 있다**(엔진 기본 이름이 실제로는 `256`이 되는 이유). `ACBGameModeBase` 생성자에서 `"Player"`로 지정했다 — BP 체크박스처럼 조용히 빠질 수 있는 값이라 코드에서 고정한다.

**번호는 단순 증가 카운터가 아니라 `PlayerArray`에서 비어 있는 가장 작은 번호를 찾는다.** 카운터로 매기면 누가 나갔다 들어올 때 3인 로비에 `Player5`가 생긴다. 인원이 적어 완전탐색 비용은 무시할 수 있고, 이 탐색은 중복 검사와 같은 함수(`Auth_IsNicknameTaken`)를 쓴다.

### 변경 요청 — 검증은 게임모드, 반영은 엔진 경로

```
[클라] 닉네임 입력 확정
 └ ACBChaserController::Server_RequestSetNickname(원본 문자열)
     ├ 로비 게임모드 없음 / 이미 준비 완료  → 무시 (무기·의상 변경과 같은 정책)
     ├ ACBGameModeBase::Auth_SanitizeNickname()   ← 다듬기 + 중복 해소
     └ ChangeName()  →  SetPlayerName()  →  OnRep_PlayerName()  →  OnPlayerNicknameChanged
```

| 단계 | 규칙 |
|---|---|
| 다듬기 | 제어문자 제거 → 연속 공백 1칸 → 모든 공백을 일반 스페이스로 통일 → 앞뒤 트림 → `MaxNicknameLength`(기본 10자) 클램프 |
| 거절 | 남는 글자가 없으면 `false` — **지금 이름을 그대로 둔다** |
| 중복 | **허용하지 않는다.** 이미 쓰는 이름이면 뒤에 번호를 붙임 (`이름` → `이름2` → `이름3`). 대소문자만 다른 이름도 같은 이름으로 본다 |

**중복 검사는 요청자 자신을 제외한다.** 빼지 않으면 자기 이름을 다시 확정할 때마다 번호가 계속 붙는다.

**거절 사유를 클라이언트에 보내지 않는다.** 이름이 복제 값이라 거절되면 값이 그대로고, 위젯이 복제된 이름을 표시하면 입력창이 원래 값으로 되돌아간다. 사유 문구가 필요해지면 그때 `Client_` RPC를 붙인다.

**게임플레이 레벨에서는 바꿀 수 없다.** 로비 게임모드가 아니면 요청 자체를 무시한다. 로비에서 정한 이름은 `CopyProperties`(엔진 기본)로 게임플레이까지 그대로 따라간다.

> **아직 안 한 것**: 마지막 닉네임을 로컬에 저장했다가 매번 입력하지 않도록 하는 것. **접속 URL 로 실어 보내는 방식은 쓰지 않는다**(위 "`?Name=`을 읽지 않는 이유"). 대신 로비 위젯이 저장값으로 입력창을 미리 채우고 `Server_RequestSetNickname` 을 한 번 보내면 된다 — 검증이 이미 서버 한 곳에 모여 있어 창구를 늘릴 이유가 없다. 저장 위치는 `UCBGameInstance`(실행 중 유지) 또는 `USaveGame`(재실행 후에도 유지) 중 선택.

## 준비 완료와 게임 시작 — 규칙은 `bIsReady`, 연출은 어빌리티

```
[클라] 준비 버튼
 └ ACBChaserController::Server_RequestToggleReady()
     ├ ACBPlayerState::Auth_SetReady(!IsReady())      ← 규칙(복제)
     ├ Auth_PlayReadyAbility()  무기 장착/해제 어빌리티 ← 연출(best-effort)
     └ ACBLobbyGameMode::Auth_RefreshReadyState()      → ACBLobbyGameState 에 인원 반영
                                                          ↓ 복제
[전 클라] OnReadyStateChanged 방송 → 위젯 갱신, 호스트는 전원 준비 시 시작 버튼으로 전환(위젯 스위처)
[호스트] 시작 버튼 → Server_RequestStartMatch() → 게임모드 검증 → ServerTravel
```

**준비 상태와 전투 모드를 하나로 합치지 않는다.** `Status.Combat.InCombat` 태그를 준비 상태로 삼으면 상태가 하나 줄지만, 게임플레이 레벨에서는 전원이 전투 모드라 의미가 겹치고, **어빌리티가 실패하면 준비 자체가 불가능**해진다(무기 없는 로드아웃 등). `bIsReady`가 규칙이고 어빌리티는 그 표현이다.

**연출을 서버에서 발동할 수 있는 근거**: 서버가 원격 플레이어의 LocalPredicted 어빌리티를 `TryActivate` 하면 GAS가 `ClientTryActivateAbility`로 소유 클라에 넘겨주고(엔진 `AbilitySystemComponent_Abilities.cpp:1648`), 호스트 자신은 로컬이라 즉시 실행된다. 몽타주는 어차피 GameplayCue로 나가므로(→ [Montage.md](../Gameplay/Montage.md)) 시뮬 프록시까지 보인다. **서버 한 곳에서 부르면 전원 화면에 나온다.**

**연출 실패는 준비를 막지 않는다.** 원격 전달 경로는 즉시 `true`를 반환해 서버가 성공 여부를 알 수 없다. 애니메이션만 빠지고 진행되는 쪽이 로비에서 안전하다.

| 무엇 | 어디 | 왜 |
|---|---|---|
| 플레이어별 준비 여부 | `ACBPlayerState::bIsReady` (복제) + `OnPlayerReadyChanged` | 전 클라가 봐야 하는 플레이어별 값 |
| "몇 명 중 몇 명" | **`ACBLobbyGameState`** (복제) + `OnReadyStateChanged` | 게임 전역 상태. 위젯이 **신호 하나만** 구독하면 됨 |
| 판정·이동 | `ACBLobbyGameMode` | 게임 규칙은 서버 권위 |

**신호가 둘인 이유는 묻는 질문이 다르기 때문이다.** 인원 표시(`Footer`)는 "몇 명 중 몇 명"을 묻고, 준비 중에 잠겨야 하는 위젯(무기 변경·의상 교체)은 **"내가 준비했나"** 를 묻는다. 후자를 집계 신호로 처리하면 남이 준비 버튼을 눌러도 내 위젯이 갱신되고, 정작 값은 다시 `PlayerState`에서 읽어야 해서 신호와 출처가 어긋난다.

```
ACBPlayerState::OnRep_IsReady()      ← 서버는 Auth_SetReady 가 직접 호출 (호스트 화면 반영)
 ├ OnPlayerReadyChanged.Broadcast(bIsReady)          ← 그 플레이어의 위젯 잠금/해제
 └ ACBLobbyGameState::NotifyReadyStateChanged()      ← 인원 표시 갱신
```

**구독하는 쪽은 구독 직후 현재 값을 한 번 읽는다.** 델리게이트는 "바뀔 때"만 오므로, 위젯이 늦게 생기면 이미 지나간 변경을 놓친다 (`IsReady()`로 즉시 초기 상태를 맞출 것). 로비 전반에서 쓰는 "구독 또는 즉시" 패턴과 같다 → [SystemReady.md](../Foundation/SystemReady.md)

**`bIsReady`는 `CopyProperties`에 넣지 않는다.** 로비 안에서만 의미가 있고, 게임플레이의 새 PlayerState는 `false`로 시작하는 것이 맞다 (→ [GameFlow.md](GameFlow.md) "PlayerState 이관").

**PlayerState와 GameState는 도착 순서가 보장되지 않는다.** 그래서 `OnRep_IsReady`도 게임 스테이트의 `NotifyReadyStateChanged()`를 불러 현재 값으로 다시 방송한다. 위젯은 매번 현재 상태에서 다시 계산하므로 중복 방송이 무해하다.

### 이동 전에 로비 위젯을 반드시 스택에서 뺀다

**위젯을 남긴 채 travel하면 새 레벨에서 캐릭터가 움직이지 않는다.** 수명이 서로 다른 두 가지가 엇갈려서 생기는 증상이다.

| | travel 시 |
|---|---|
| HUD·위젯 스택 | **파괴** (맵 소속) |
| `ULocalPlayer` + `EnhancedInputLocalPlayerSubsystem` | **생존** |

로비 위젯은 `Should Remove Gameplay IMCs = true`라 **떠 있는 동안 게임플레이 IMC가 걷혀 있고**, 팩은 그 복구를 **스택 상태가 변할 때** 수행한다(→ [EasyGameUI.md](../Presentation/EasyGameUI.md)). 그런데 위젯을 빼지 않고 맵을 넘기면 스택은 재계산 없이 HUD째로 사라지고, **걷어낸 IMC를 되돌릴 기회가 사라진다.** IMC를 실제로 들고 있던 서브시스템은 `ULocalPlayer` 소속이라 travel을 넘어 살아남으므로, 빠진 상태 그대로 새 레벨에 도착한다.

**해법은 이동 직전에 신호를 한 번 방송하는 것이다.**

```
ACBLobbyGameMode::Auth_TravelToGameplayLevel()
 ├ ACBLobbyGameState::Multicast_NotifyMatchStarting()   ← 서버 자신 + 전 클라이언트
 │    └ 각 인스턴스에서 OnMatchStarting 방송 → 위젯이 스스로를 스택에서 제거
 └ World->ServerTravel(...)
```

- **호스트/클라이언트를 분기하지 않는다.** Multicast RPC는 **서버 자신에게서도 실행**되므로, 호스트도 신호를 받는 쪽 중 하나가 된다. 위젯 제거는 어디서나 로컬 작업이다
- **GameMode가 아니라 GameState가 방송한다.** GameMode는 클라이언트에 존재하지 않아 Multicast의 주체가 될 수 없다 (→ [GameFlow.md](GameFlow.md) "클래스별 존재 범위")
- **서버는 "언제인지"만 알린다.** 실제 제거는 각자의 HUD 스택에서 일어난다 — 로비 UI 표시를 BP에 맡긴 것과 같은 분업이다

### 시작 버튼은 UI가 아니라 서버가 판정한다

버튼을 숨기는 것은 표시일 뿐이므로 `Auth_TryStartMatch()`가 넷을 다시 본다.

| 검증 | 방법 |
|---|---|
| 호스트인가 | 서버에서 `InRequester->IsLocalController()` — 리슨 서버에서 로컬인 컨트롤러는 호스트뿐이다 |
| 전원 준비인가 | `ACBLobbyGameState::IsAllReady()` (0명은 false) |
| 최소 인원인가 | `MinPlayersToStart` (기본 1 — 혼자서도 흐름 확인 가능) |
| 중복 요청인가 | `bTravelStarted` 플래그 |

- **이동은 `ServerTravel(레벨 + "?listen")`.** `listen`을 유지해야 이동한 뒤에도 클라이언트가 붙어 있다
- **게임플레이 레벨은 `GameplayLevel`(EditDefaultsOnly)로 로비 게임모드 BP가 지정한다.** 레벨 규칙은 게임모드 소관이므로, 메뉴 쪽 `New Game Level`(Global Config)과 등록소가 겹치지 않는다
- **나가는 플레이어는 `Logout`에서 집계 대상에서 제외한다.** 그 시점엔 아직 `PlayerArray`에 남아 있어, 빼지 않으면 준비했던 사람이 나갔는데 "전원 준비"가 유지된다

## 로비 카메라 — 레벨에 있으면 켜지고 없으면 꺼진다

**대기실 공용 카메라 1개**를 두고 전원이 같은 화면을 본다(배틀그라운드 대기실 형태). 접속한 캐릭터들이 한 줄로 함께 보인다.

구현은 `ACBChaserController`의 **`AutoManageActiveCameraTarget` 오버라이드 하나**다. 엔진이 "이 플레이어의 카메라를 무엇에 맞출까"를 결정하는 지점이라, 뷰 타겟을 우리가 따로 설정하는 대신 **그 결정 자체를 바꾼다.**

```
AutoManageActiveCameraTarget(SuggestedTarget)
 ├ ACBLobbyCamera 없음 → Super (엔진 기본: 폰을 봄)
 └ 있음               → SetViewTarget(로비 카메라)
```

| 결정 | 이유 |
|---|---|
| **로비 전용 PlayerController를 만들지 않는다** | seamless travel은 로비와 게임플레이의 PC 클래스가 같아야 한다. `ACBGameModeBase`가 PC를 고정한 것과 같은 이유 |
| **로비인지 판별하지 않는다** | 레벨에서 **`ACBLobbyCamera`** 를 찾아보고 없으면 `Super`에 맡긴다. GameMode는 클라이언트에 없어 물어볼 수 없고, 이 방식이면 게임플레이 레벨에서 자동으로 꺼진다 |
| **액터 태그가 아니라 전용 클래스** | 이 프로젝트는 GameplayTag를 많이 써서 `AActor::Tags`와 혼동된다. 타입을 식별자로 쓰면 매직 스트링이 사라지고 컴파일러가 검사한다. 액터 이름은 `GetActorLabel()`이 에디터 전용이라 후보가 못 된다 |
| **인스턴스를 구분하지 않는다** | 서버·클라 어디서 불려도 같은 답을 낸다. 서버가 원격 PC에 대해 부르면 그 값이 `ClientSetViewTarget`으로 클라에 전달되어 **양쪽이 같은 값으로 일치**한다 |
| **SystemReady를 기다리지 않는다** | 카메라 전환은 ASC·로드아웃과 무관하다. 두 타이밍 문제를 섞지 말 것 → [SystemReady.md](../Foundation/SystemReady.md) |

**배치를 빠뜨리면 에러 없이 3인칭 그대로다.** 2개 이상 놓으면 경고 로그를 남긴다(대기실 카메라는 1개 전제).

### 왜 `SetViewTarget`을 직접 부르지 않나

먼저 그 방식으로 만들었다가 두 번 실패해서 갈아탄 것이라 근거를 남긴다.

`APlayerController`는 **빙의 흐름의 마지막에** `AutoManageActiveCameraTarget(GetPawn())`을 불러 뷰 타겟을 폰으로 맞춘다(클라는 `ClientRestart_Implementation`, 리슨 서버 호스트는 `OnPossess`). 그래서 어디서 `SetViewTarget`을 부르든 **그 뒤에 덮어써진다.**

`bAutoManageActiveCameraTarget = false`로 막을 수는 있는데, 여기에 함정이 하나 더 있었다:

```cpp
// PlayerCameraManager.cpp — 뷰 타겟 배정 시
if (!PCOwner->IsLocalPlayerController() && (GetNetMode() != NM_Client))
{
    PCOwner->ClientSetViewTarget(VT.Target, TransitionParams);
}
```

**서버가 원격 플레이어의 PC(로컬이 아닌 인스턴스)의 뷰 타겟을 바꾸면 RPC로 클라에 강제된다.** 따라서 플래그 해제를 `IsLocalController()` 안쪽에 두면 클라가 스스로 잡은 카메라를 서버가 폰으로 되돌린다 — **호스트만 되고 클라만 안 되는** 형태로 나타난다.

오버라이드 방식은 이 두 문제가 **함께 사라진다.** 엔진이 부르는 지점을 그대로 쓰므로 순서를 다툴 일이 없고, 서버가 원격 PC에 대해 불러도 로비 카메라를 답으로 내므로 RPC가 밀어내는 값도 옳다. 플래그를 만지지 않으니 레벨 간 복구 로직도 필요 없다.

### 화면은 뷰 타겟 확정 + 캐릭터 준비까지 검게 덮는다

빙의 전에는 뷰 타겟이 PlayerController 자신이라 스폰 지점에서 보는 화면이 잠깐 나온다. 그리고 빙의 직후에도 **의상 등 비동기 에셋이 아직 안 붙어 있다**(→ [SystemReady.md](../Foundation/SystemReady.md), [Cosmetic.md](../Presentation/Cosmetic.md)). 그래서 두 조건이 모두 충족될 때까지 화면을 덮는다.

```
ReceivedPlayer                → SetManualCameraFade(1, 검정)   // 덮기
AutoManageActiveCameraTarget  → 뷰 타겟 확정 → Local_RequestFadeIn()
     ├ 폰이 캐릭터가 아님(관전 단계) → 대기
     ├ 폰이 아직 Ready 아님          → 준비 완료 델리게이트 구독하고 대기
     └ Ready                        → StartCameraFade(1→0)     // 걷기
준비 완료 콜백                 → StartCameraFade(1→0)          // 걷기
```

**덮는 곳이 `BeginPlay`면 안 된다.** 리슨 서버 호스트는 `BeginPlay`보다 **빙의가 먼저** 끝나서, 걷기 요청이 덮기보다 앞서 실행되고 버려진다(호스트만 영원히 검은 화면). `ReceivedPlayer()`는 `SetPlayer()` 안에서 불리고 그 다음 줄이 `PostLogin`이므로(`LevelActor.cpp:1110`) **캐릭터 빙의보다 앞선다.**

> 단 **관전 폰 단계보다는 뒤다.** 실측 로그에서 `AutoManage(SpectatorPawn)`가 `ReceivedPlayer`보다 먼저 두 번 불렸다. 그 호출들은 아직 화면을 안 덮은 상태라 걷기 요청이 버려지는데, 어차피 폰이 캐릭터가 아니라 열려서도 안 되는 시점이므로 결과는 같다. **"모든 카메라 결정보다 앞"이 아니라 "캐릭터 빙의보다 앞"이 정확한 보장 범위다.**

걷는 조건이 둘(뷰 타겟 확정 + 캐릭터 준비)이고 순서가 보장되지 않아 **두 지점에서 같은 함수를 부르고 안에서 나머지 조건을 확인**한다.

> **준비 대기 경로는 아직 실측된 적이 없다.** PIE에서는 호스트·클라 모두 빙의 시점에 이미 `Ready=true`였다 — 에디터에 에셋이 이미 메모리에 있어 로드아웃의 async 로드가 사실상 즉시 끝나기 때문으로 보인다. 콜드 로드(패키징 빌드·첫 실행)에서 실제로 대기가 걸리는지는 확인되지 않았다.

| 결정 | 이유 |
|---|---|
| **자기 폰만 기다린다** | 로비 공용 카메라엔 남의 캐릭터도 보이지만, 전원을 기다리려면 GameState에 준비 인원을 복제해야 하고 늦은 접속까지 생각하면 "다 준비됨"의 정의부터 어려워진다. 남의 의상이 늦게 붙는 건 감수한다 |
| **안전 타임아웃을 두지 않는다** | 로드가 실패하면 정상 플레이가 불가능하므로, 깨진 화면을 보여주느니 **검은 화면으로 멈추는 편이 정직하다.** 필요해지면 그때 추가한다 |
| **모든 레벨에 적용** | 분기가 없어 단순하고, 리스폰·맵 전환에도 같은 연출이 따라온다. 메인 메뉴 폰이 이미 같은 방식으로 페이드 인한다 → [EasyGameUI.md](../Presentation/EasyGameUI.md) |

**걷기는 자체 플래그로 1회만 수행한다**(`bReadyStateApplied`). `PlayerCameraManager::FadeAmount`는 `DoUpdateCamera`에서만 갱신되므로 **프레임 안에서는 신뢰할 수 없고**, `AutoManageActiveCameraTarget`이 여러 번 불리는 만큼 `StartCameraFade`가 재호출되어 **화면이 알파 1(완전 검정)에서 다시 시작**한다. 실측에서 클라이언트가 3회 재시작했다.

구독은 핸들이 유효한지로 중복을 막는다.

> 뷰 타겟을 즉시 컷으로 바꿀 때는 `PlayerCameraManager::SetGameCameraCutThisFrame()`을 함께 부른다. 알리지 않으면 렌더러가 **직전 프레임의 오클루전 쿼리·모션 벡터를 재사용**한다.

### 이 페이드는 로딩 화면이 아니다 — 덮을 수 있는 구간의 한계

레벨을 직접 열면 **화면을 덮기 전에 이미 몇 프레임이 그려진다**(스폰 지점의 눈높이 뷰 = 대개 벽). PlayerController 쪽 어느 훅으로도 잡히지 않는다 — 그 시점엔 PC나 카메라 매니저가 아직 없기 때문이다.

로딩을 덮는 수단은 계층이 셋이고, 서로 대체할 수 없다.

| 계층 | 덮는 구간 | 수단 |
|---|---|---|
| ① | **`LoadMap` 자체** — 게임 스레드가 블로킹되어 월드 기반 UI가 못 돈다 | **MoviePlayer** (별도 슬레이트 스레드). 이것만 가능 |
| ② | **seamless travel** 중 | **`Transition Map`** |
| ③ | 맵 로드 후 **시스템 초기화**(비동기 로드·복제 대기) | **이 페이드** |

**우리가 만든 것은 ③이고 그 범위 안에서는 완전히 통제된다.** 남는 벽은 ①의 미구현이 드러난 것이며, 팩의 `Loading` 레이어도 맵 전환을 가로지르지 못하므로 대안이 되지 않는다 → [EasyGameUI.md](../Presentation/EasyGameUI.md)

**메인 메뉴 ↔ 로비 ↔ 게임플레이 전환을 붙일 때 ①·②를 함께 설계한다.** 맵을 넘어 사는 계층(`UCBGameInstance` 또는 그 서브시스템)이 그 자리다.

### `AutoManageActiveCameraTarget`은 빙의 한 번에 5~7회 불린다

실측값이다. 두 가지가 겹친다:

1. **빙의 전에 관전 단계를 거친다.** `Suggested`가 `PlayerController 자신 → SpectatorPawn → 캐릭터` 순으로 바뀌며 매 전이마다 불린다
2. **빙의 하나에 진입점이 3~4개다.** `OnPossess`(서버) / `ClientRestart`(클라) / `APawn::PawnClientRestart` / `APawn::OnRep_Controller` — 복제 도착 순서가 보장되지 않아 어느 쪽이 먼저 와도 되도록 건 보험이다

**정상 동작이므로 이 함수는 멱등해야 한다.** 그래서 우리 오버라이드도 매번 같은 답을 내고, 반복 비용만 줄인다 — 로비 카메라는 **찾은 결과를 캐시**하고(못 찾은 건 캐시하지 않는다. 맵을 넘으면 영영 못 찾게 되므로), **이미 그 카메라를 보고 있으면 `SetViewTarget`을 생략**한다.

여기서 나오는 규칙 하나: **"캐릭터에 빙의했는가"를 화면을 열 조건으로 삼아야 한다.** 관전 폰 단계에서 열면 캐릭터가 없는 화면이 보인다.

> `ACameraActor`의 `Auto Activate for Player`를 쓰면 코드가 필요 없지만, 클라이언트 경로는 "이미 그 카메라를 보고 있을 때만" 유지하는 구조라 멀티플레이에서 신뢰할 수 없다.

배치 기준값 — 4인 라인업: `PlayerStart` 간격 250, 카메라 거리 850, 높이 130, FOV 60~65.

### `ACameraActor`의 화면비 고정은 꺼야 한다

`ACameraActor`는 **생성자에서 화면비를 16:9로 고정한다**(`bConstrainAspectRatio = true` — 엔진 `CameraActor.cpp`). 창 비율이 다르면 남는 영역이 검게 칠해진다.

**그런데 UI는 그 검은 영역 위에도 그려진다.** UMG는 카메라의 화면비 제약을 모르고 언제나 뷰포트 전체에 그리므로, 위젯만 띠 위로 삐져나온다. 둘을 자동으로 연동시킬 수단은 없다.

**게임플레이에서는 이 문제가 없다** — 폰의 `UCameraComponent`는 기본값이 `false`다(`CameraComponent.cpp`). 로비만 화면 규격이 달라지는 셈이라 맞춰서 끈다.

| 대상 | 처리 |
|---|---|
| `ACBLobbyCamera` | **생성자에서 해제.** 배치할 때 체크박스로 하지 않는 이유는 새 카메라를 놓을 때 **누락되면 조용하기 때문**이다(게임모드 공통 기본값과 같은 판단) |
| 커스터마이징 카메라 | 런타임 스폰이므로 스폰 직후 해제 |

띠를 없애면 **창 비율에 따라 보이는 범위가 달라진다**(세로 FOV 유지 → 넓은 창은 좌우가 더 보임). 정상 동작이며, 그래서 로비 UI는 화면 가장자리보다 **중앙 기준 앵커**가 안전하다.

> 반대로 **고정 구도를 원한다면** 카메라를 그대로 두고 UI 전체를 `Scale Box`로 같은 비율에 가두는 길이 있다. 다만 모든 스택 위젯에 적용하거나 팩의 `WBP_EGUI_MainUIPanel`(모든 레이어가 얹히는 단일 캔버스)을 고쳐야 하고 → [EasyGameUI.md](../Presentation/EasyGameUI.md), 폰트 크기가 창 크기를 따라 변한다. **채택하지 않았다.**

> 클래스 기본값을 바꿔도 **이미 배치된 인스턴스가 그 값을 개별 저장하고 있으면 따라오지 않는다.** 빌드 후 디테일 패널에서 해당 항목에 되돌리기 화살표가 보이면 눌러 기본값으로 복귀시킬 것.

## 로비 UI 구성 — 화면 분할이 아니라 월드 투영

`Menu` 레이어에 위젯 하나를 올리고, 조작 UI가 **자기 캐릭터 머리 위를 따라간다.**

| 위젯 | 역할 |
|---|---|
| `WBP_CB_Lobby_Main` | 스택에 들어가는 **하나**. 선언 3함수 소유, 머리 위 배치 계산 |
| `WBP_CB_Lobby_PlayerMenu` | 머리 위 진입 버튼(의상·직업) — **로컬 플레이어 것 하나뿐** |
| `WBP_CB_CosmeticSelector` | 의상 버튼을 누르면 열리는 부위 4개 × 화살표 → [Cosmetic.md](../Presentation/Cosmetic.md) |
| `WBP_CB_Lobby_Footer` | 하단 준비완료·나가기 |

닉네임 표시는 이 스택 위젯들과 별개다. **발밑 이름표는 `UCBUIComponent`가 캐릭터에 붙이는 `UWidgetComponent`** 이고(버튼이 없어 스택에 올릴 이유가 없다), 로비에서만 생성된다 → [UI.md](../Presentation/UI.md) "로비 발밑 이름표"

머리 위 배치의 구현 레시피(투영·앵커·DPI)는 → [UI.md](../Presentation/UI.md)

**두 위젯이 서로 다른 배치 방식을 쓰는 데는 이유가 있다.**

| 위젯 | 어떤 뷰에서 뜨나 | 배치 |
|---|---|---|
| `PlayerMenu` | 공용 뷰 — **내 자리가 매번 다름** | 월드 투영 |
| `CosmeticSelector` | 클로즈업 — **구도가 항상 같음** | 고정 앵커 |

클로즈업 카메라를 폰 기준으로 배치하므로(아래) 진입한 순간의 화면 구도는 언제나 동일하다. 따라서 "캐릭터 옆"은 투영이 아니라 고정 좌표로 충분하다. 클로즈업에서는 캐릭터가 화면을 크게 차지해 **머리가 화면 밖으로 나갈 수 있으므로**, 진입 시 `PlayerMenu`는 명시적으로 숨긴다.

**남의 머리 위에는 아무것도 띄우지 않는다.** `Server_RequestCosmeticPart`는 **호출한 컨트롤러의 폰에만** 적용되므로 남의 버튼은 눌러도 의미가 성립하지 않는다. 준비 상태를 머리 위에 보여야 해지면 조작이 없는 이름표 위젯을 따로 만든다.

### 슬롯 인덱스 복제가 필요 없는 이유

한때 **화면을 4등분해 각 칸에 플레이어를 배정**하는 안을 검토했다. 4명이 같은 깊이에 등간격으로 서 있어 화면에서도 등간격이므로 기하학적으로는 성립한다(Size Box 폭 = 간격 × 4를 그 깊이에서 투영한 폭).

깨지는 것은 **"누가 몇 번째 칸인가"** 다.

- `GameState->PlayerArray`는 도착 순이라 **서버와 클라이언트가 다를 수 있다**
- 3명만 접속하면 엔진 기본 `ChoosePlayerStart`가 **연속되지 않은 스타트**를 고를 수 있다
- 칸과 실제 위치가 어긋나면 **남의 캐릭터 위에 내 버튼이 뜬다**

그래서 4등분 방식은 `ACBLobbyGameMode`의 스타트 배정 + PlayerState 복제를 **전제로 요구한다.** 투영은 실제 위치를 매번 구하므로 그 전제가 통째로 사라지고, `PlayerStart` 간격·카메라 거리·FOV를 조정해도 위젯을 다시 튜닝할 필요가 없다.

> **슬롯 순서 고정 자체는 여전히 가치가 있다** — 재접속해도 자리가 안 바뀌는 것은 좋은 성질이다. 다만 그건 로비 경험의 문제이지 UI의 전제가 아니며, **나중에 도입해도 UI를 고치지 않는다.**

### 커스터마이징 클로즈업 — 카메라는 하나, 위치는 폰에서 계산

공용 뷰에서는 자기 캐릭터가 프레임의 일부라 의상을 고르기엔 작다. 그래서 의상·직업 버튼을 누르면 **내 캐릭터 앞으로 카메라를 당긴다.**

```
ACBChaserController::Local_EnterCosmeticView()
 ├ 로비인지 확인 (ACBLobbyCamera 없으면 아무것도 안 함)
 ├ 카메라 위치 = 폰 위치 + ActorForward × Distance + (0,0,Height)
 ├ 회전 = 캐릭터를 바라본 뒤 Yaw 오프셋      ← 캐릭터를 화면 한쪽으로 밈
 ├ bInCosmeticView = true                   ← 게이트 먼저
 └ SetViewTargetWithBlend
```

- **캐릭터마다 카메라를 두지 않는다.** 폰의 트랜스폼에서 계산하므로 어느 자리에 서 있든 맞고, 위 "슬롯 인덱스가 필요 없는 이유"와 같은 해법이다
- **`ActorForward` 방향으로 당기는 것이 핵심이다.** 캐릭터는 `PlayerStart`의 Yaw로 이미 공용 카메라를 보고 있으므로, 그 방향은 곧 공용 카메라 쪽이다. **회전 없는 돌리 인**이 되어 블렌드가 자연스럽다 — 옆으로 돌아 들어가면 블렌드 중 배경이 휩쓸린다
- **캐릭터를 옆으로 미는 것은 카메라 이동이 아니라 Yaw 오프셋이다.** 카메라를 옆으로 옮기고 캐릭터를 바라보게 하면 캐릭터가 다시 화면 중앙으로 돌아온다. 회전만 틀면 **튜닝할 숫자가 하나**다
- 카메라는 **첫 진입 시 로컬 스폰 후 재사용**한다. 매번 스폰·파괴하면 블렌드가 끝나기 전에 대상이 사라진다

> **`ACBLobbyCamera`에 용도 필드를 두는 방식은 채택하지 않았다.** 이 문서가 한때 그렇게 제안했지만, 그 클래스는 카메라이면서 동시에 **"로비 표식"** 을 겸한다(위 각주). `AutoManageActiveCameraTarget`과 `Local_TryAllowGameplayInput` 둘 다 이 클래스를 찾으므로, 클로즈업용을 같은 클래스로 만들면 **뷰 타겟 선택이 그것을 집어 레벨 시작부터 클로즈업으로 시작**할 수 있다. 평범한 `ACameraActor`를 쓰면 검색에 걸리지 않아 아무것도 깨지지 않는다.

**`bInCosmeticView`는 상태 표시이자 게이트다.** `AutoManageActiveCameraTarget`은 빙의마다 5~7회 불리므로(아래), 이 플래그가 없으면 **재빙의하는 순간 클로즈업이 공용 뷰로 풀린다.** 직업 변경을 캐릭터 재스폰으로 구현하면 바로 부딪히는 지점이라, 카메라 상태를 위젯이 아니라 **컨트롤러가 들고 있어야 하는 이유**가 이것이다.

> 재빙의 시 지금은 뷰를 **유지**만 한다. 새 폰 기준으로 카메라를 다시 놓지는 않으므로, 직업 변경을 붙일 때 `Local_EnterCosmeticView()` 재호출이 필요한지 함께 판단한다.

## 진행 상황

**완료**
- **`L_CB_Lobby` + `GM_CB_Lobby`** (Default Pawn = `BP_Chaser_Sword`, HUD = `BP_EasyMainGameHUD`), World Settings 오버라이드
- **로비 배경** — 속 빈 상자(스태틱 메시, `Use Complex Collision As Simple`) + 포인트 라이트, `PlayerStart` 배치
- **로비 고정 카메라** — `ACBLobbyCamera` + `ACBChaserController::AutoManageActiveCameraTarget` 오버라이드
- **레벨 진입 페이드 인** — 뷰 타겟 확정 + 캐릭터 준비 완료까지 검은 화면 (모든 레벨 공통)
- **로비 UI** — `Menu` 레이어 위젯 + 머리 위 투영 배치, 의상 교체 2인 검증 완료 (위 "로비 UI 구성")
- **로비 준비 완료 → 게임 시작** — `bIsReady` 복제 + `ACBLobbyGameState` 집계 + 호스트 시작 요청 검증 후 `ServerTravel` (위 "준비 완료와 게임 시작")
- **닉네임 C++ 경로** — 기본 닉네임 부여(`Player1`, `Player2` ...), 다듬기·중복 접미사, 변경 요청 RPC, 변경 신호 (위 "닉네임"). **컴파일까지 확인, 위젯 배선과 실행 검증 남음**

**미구현**
- 로비 위젯 배선 (`WBP_CB_Lobby_Footer`) — 준비/시작 위젯 스위처, `OnReadyStateChanged` 구독, 두 RPC 호출
- **닉네임 UI 배선** — C++ 창구는 완료(위 "닉네임"). 남은 것: 로비 위젯에 입력창(커밋 시 `Server_RequestSetNickname`) + 이름 표기(`GetPlayerName()` + `OnPlayerNicknameChanged` 구독, 구독 직후 현재 값 1회 읽기)
- 닉네임 로컬 저장 + 로비 입력창 프리필 후 `Server_RequestSetNickname` 1회 전송 — 매번 입력하지 않도록. 서버 수신부는 완료. **접속 URL `?Name=` 방식은 채택하지 않는다** (위 "닉네임")

## 관련 문서

- 레벨·게임모드 구조, **무기 변경 = 캐릭터 변경**: [GameFlow.md](GameFlow.md)
- 접속·세션·나가기: [Session.md](Session.md)
- 의상 선택 UI와 조합 적용: [Cosmetic.md](../Presentation/Cosmetic.md)
- 위젯 스택·IMC 제거 규칙: [EasyGameUI.md](../Presentation/EasyGameUI.md)
- 로비에서 입력을 붙이지 않는 이유: [Input.md](../Gameplay/Input.md)
