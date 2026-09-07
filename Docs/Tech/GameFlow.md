# 게임 플로우 (레벨·게임모드·전환)

> 어떤 레벨들이 있고, 그 사이를 어떻게 오가며, **무엇이 살아남는가**. 레벨을 추가하거나 게임모드를 만들 때, 맵을 넘어 데이터를 넘겨야 할 때 이 문서를 따른다.
>
> 서버 권위·접두사·복제 정책 같은 **일반 규칙**은 [Multiplayer.md](Multiplayer.md). 여기는 **구체적인 흐름**만 다룬다.

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

메인 메뉴만 C++ 클래스가 없다 — 담을 규칙이 없어 BP 설정(HUD·Default Pawn)만으로 충분하다. → [EasyGameUI.md](EasyGameUI.md)

### 게임 스테이트 계층

```
ACBGameStateBase (AGameStateBase)   ← 플레이어 목록 변경 신호(OnPlayerListChanged)
 └─ ACBLobbyGameState               ← 준비 인원 집계
```

`ACBGameModeBase` 생성자가 `GameStateClass`를 `ACBGameStateBase`로 고정하고, 로비 게임모드만 자기 것으로 덮는다. 목록 신호는 HUD 플레이어 목록이 쓴다 → [UI.md](UI.md)

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
| 메인 메뉴 → 로비 (호스트) | `OpenLevel` + **`listen`** — `UCBSessionSubsystem::Local_HostLobby()` (아래) |
| 메인 메뉴 → 로비 (참가) | **`ClientTravel`** — `UCBSessionSubsystem::Local_JoinServerByAddress()` (아래) |
| 방 만들기 / 방 찾기 | 세션 생성·검색·참가 후 위 두 경로로 수렴 — `UCBSessionSubsystem` (아래 "세션") |
| 로비 → 게임플레이 (전원) | **`ServerTravel`** — 접속한 클라이언트가 함께 이동 |
| 로비 → 메인 메뉴 (나가기) | 클라 **`ClientTravel`** / 호스트 `OpenLevel` — `UCBSessionSubsystem::Local_LeaveToMainMenu()` (아래) |

### 호스트·참가 — `UCBSessionSubsystem`

메인 메뉴의 버튼 2개가 접속의 전부다. **아직 세션 개념이 없고 주소로 직접 붙는다** — EOS를 넣기 전까지의 형태다.

| 버튼 | 호출 | 실제 동작 |
|---|---|---|
| 호스트 | `Local_HostLobby(로비 레벨)` | `OpenLevelBySoftObjectPtr(..., Options="listen")` |
| 참가 | `Local_JoinServerByAddress("127.0.0.1")` | 로컬 PC의 `ClientTravel(TRAVEL_Absolute)` |

- **`listen` 옵션이 빠지면 조용히 단독 실행이 된다.** 에러 없이 아무도 접속하지 못하는 상태가 되므로 호스트 경로에서 항상 확인할 것
- **주소는 `IP` 또는 `IP:포트`.** 비워서 호출하면 `127.0.0.1`을 쓴다(같은 PC 2인 테스트용 기본값)
- **로비 레벨은 인자로 받는다.** 그 값은 이미 팩 Global Config의 `New Game Level`에 있고, 레벨 에셋 등록소는 Global Config로 정해져 있다(→ [EasyGameUI.md](EasyGameUI.md)). C++이 경로를 또 들고 있으면 등록소가 둘이 된다

#### 왜 서브시스템인가 — 접속에 관한 것은 전부 여기 모은다

접속 로직은 **맵을 넘어 살아야 한다** — 참가 실패 콜백은 메뉴 레벨을 떠난 뒤에 오기 때문이다. 그 조건을 만족하는 그릇은 `UGameInstance`와 `UGameInstanceSubsystem` 둘이고, **둘 다 가능하므로 성격이 같은 것을 한 곳에 모으는 쪽**을 택했다. `UCBGameInstance`에는 온라인 세션 클래스 지정(`GetOnlineSessionClass()`) 한 줄만 남는다 — 엔진이 게임 인스턴스에게만 묻는 값이라 서브시스템으로 옮길 수 없다(아래 "엔진의 자동 복귀").

**엔진 실패 훅을 직접 못 쓰는 것은 제약이 맞다.** `HandleNetworkError` / `HandleTravelError`는 **`BlueprintImplementableEvent`라 C++에서 오버라이드할 수 없어서**(엔진 `GameInstance.h`), `GEngine->OnNetworkFailure()` / `OnTravelFailure()`에 직접 구독해야 한다. 다만 **그 구독자가 GameInstance일 필요는 없다** — 서브시스템의 `Initialize()` / `Deinitialize()`에서 구독·해제하면 되고, `UObject::GetWorld()`가 아우터를 타고 올라가므로(`Within = GameInstance`) 월드도 같은 값을 얻는다.

```
실패 발생
 └ UCBSessionSubsystem::Local_HandleNetworkFailure / Local_HandleTravelFailure
     └ Local_HandleConnectionFailure(사유, 현재 레벨 유지 여부)
         ├ OnConnectionFailed 방송 (위젯이 모달 표시)
         ├ 레벨을 넘겨야 하면 PendingFailureReason 에 사유 보관
         ├ 들어가 있던 세션에서 이탈 (호스트=파괴 / 참가자=떠나기)
         └ 현재 레벨 유지가 아니면 GameDefaultMap 으로 복귀
```

- **내 월드의 실패만 처리한다.** `GEngine`은 프로세스에 하나인데 PIE(Run Under One Process)는 게임 인스턴스가 인스턴스 수만큼 있어, 가드가 없으면 **클라이언트 쪽 실패로 호스트까지 메뉴로 돌아간다**
- **월드가 `NULL`인 실패는 월드 비교로 걸러지지 않는다** — `Local_IsOwnFailure()`가 내 월드 컨텍스트의 `PendingNetGame` 유무로 판별한다 (아래 "접속 거부")
- **호스트는 클라이언트가 나가도 로비에 남는다.** 넷 드라이버의 넷 모드가 `NM_Client`일 때만 복귀 처리한다(엔진 `UEngine::HandleNetworkFailure`의 판단과 같다). 단 **리슨 서버를 열지 못한 실패**(`NetDriverCreateFailure`·`NetDriverListenFailure`·`NetDriverAlreadyExists`)는 호스트 쪽이어도 처리한다 — 방을 못 연 것이고, 특히 드라이버 생성 실패는 엔진이 손도 대지 않는다(아래)
- **이미 메인 메뉴면 레벨을 다시 열지 않는다.** 참가 실패는 메뉴에 머문 채 일어나므로, 다시 열면 입력한 주소가 날아간다. PIE는 패키지 이름에 인스턴스 접두사가 붙으므로 `UWorld::RemovePIEPrefix`로 걷어내고 비교한다
- **구독은 `Initialize()`, 해제는 `Deinitialize()`.** 서브시스템은 `UGameInstance::Init()`의 마지막 줄에서 만들어져 `Shutdown()` 중간에 해제되므로, 네트워크 실패가 날 수 있는 구간은 전부 덮는다
- **복귀 대상은 ini의 `GameDefaultMap`을 읽는다**(`UGameMapsSettings::GetGameDefaultMap()`). 메인 메뉴 경로를 코드가 또 들고 있지 않기 위해서다. 이 호출 때문에 `ChainBurst.Build.cs`에 `EngineSettings` 모듈이 추가됐다

> **엔진도 스스로 되돌린다 — 그래서 어디서 끊을지가 설계 지점이다.** `UEngine::HandleNetworkFailure`의 복귀 경로(`CallHandleDisconnectForFailure`)는 `UOnlineSession` 객체를 통로로 쓰는데, 이 객체는 온라인 서브시스템과 무관하게 **`UGameInstance::Init()`이 항상 만든다.** 즉 우리가 아무것도 안 해도 엔진이 기본 맵을 다시 로드한다. 아래 "엔진의 자동 복귀"를 볼 것.

#### EOS로 갈 때 무엇이 바뀌나

바뀌는 것은 **접속 앞단뿐이다.** EOS도 세션을 찾은 뒤 `GetResolvedConnectString()`으로 주소를 얻어 결국 위 두 함수를 부른다 — 지금 만든 travel 경로가 종착점이라 그대로 남는다. 추가되는 것은 로그인(Auth/Connect), 세션 생성·검색·참가, 그리고 그 실패 콜백이며 전부 `UCBSessionSubsystem` 안이다.

**지금 구조로 못 하는 것은 NAT 통과 하나다.** 같은 PC·같은 LAN은 주소 직접 입력으로 충분하고, 외부 접속은 포트포워딩 없이는 안 된다. 그게 EOS를 넣는 실질적 이유다.

### 세션 — 방을 만들고 찾아 들어간다

**세션은 "붙을 주소를 알아내는 일"만 한다.** 실제 접속은 위 두 함수(`Local_HostLobby` / `Local_JoinServerByAddress`)로 그대로 수렴한다.

```
[호스트] Local_CreateAndHostSession(로비 레벨, 방 이름, 최대 인원)
          └ CreateSession 성공 → Local_HostLobby(로비 레벨)

[참가]   Local_FindSessions()
          └ 완료 → OnSessionSearchCompleted 방송 → 목록 위젯 갱신
         Local_JoinFoundSession(인덱스)
          └ JoinSession 성공 → 세션 속성에서 호스트 주소 → Local_JoinServerByAddress(주소)
```

#### 제공자는 설정이 정하고, 코드는 모른다

**OnlineServices(v2) API로 작성한다.** 실제 제공자는 `Config/DefaultEngine.ini` 한 곳에서 정한다.

```ini
[OnlineServices]
DefaultServices=Null          ; 나중에 EOS 도입 시 Epic

[/Script/ChainBurst.CBSessionSubsystem]
bUseLANSessions=True          ; EOS 로 바꾸면 반드시 False
```

- **`bUseLANSessions`를 코드에 박지 않는다.** Null은 LAN 비콘으로만 검색되고 EOS는 아니다. 박아두면 제공자를 바꿔도 **에러 없이 LAN만 검색된다.** 그래서 두 설정을 ini에서 나란히 두었다 — 하나만 바꾸는 사고를 막기 위해서다
- **v1(`OnlineSubsystem`)이 아니라 v2를 쓴 이유**: EOS의 Device ID 익명 로그인이 v2에만 있고(v1 `ToEOS_ELoginCredentialType`에 항목 자체가 없다), v2를 골라도 `OnlineServicesOSSAdapter`로 Steam이 열려 있다. v2 EOS의 세션 구현도 생성·검색·참가·나가기에 빈 구현이 없음을 확인했다(미구현은 presence 2개뿐)

#### 호스트 주소는 우리가 정한 속성 하나로 주고받는다

제공자마다 주소를 얻는 방법이 다르다 — Null은 `FSessionLAN::OwnerInternetAddr`, EOS는 `EOSGS_HOST_ADDRESS_ATTRIBUTE_KEY` 속성이다. **그 차이를 코드에 들이지 않으려고 호스트가 직접 자기 주소를 세션 속성(`CB_HostAddress`)에 실어 보낸다.**

```
호스트: ISocketSubsystem::GetLocalHostAddr()  → CustomSettings["CB_HostAddress"]
참가 : GetSessionByName → CustomSettings["CB_HostAddress"] → Local_JoinServerByAddress()
```

**조회 코드가 제공자와 무관해진다.** EOS를 붙일 때 호스트가 넣는 값만 EOS 주소로 바뀌고 참가 측은 그대로다. (EOS는 P2P 라우팅을 위해 자기 키에도 같은 값을 넣어줘야 하는데, 그건 EOS 도입 시 추가한다)

> 포트는 싣지 않는다. 참가 측이 기본 포트로 접속한다 — 호스트가 기본 포트를 쓰지 않게 되면 여기에 포트를 함께 실어야 한다.

#### 세션 속성 목록

호스트가 실어 보내고 검색 측이 읽는 값 전부. 모두 `ESchemaAttributeVisibility::Public` 이어야 광고된다 — `NboSerializerCommonSvc`가 Public 이 아닌 항목은 LAN 패킷에서 아예 뺀다.

| 키 | 타입 | 쓰임 | 갱신 시점 |
|---|---|---|---|
| `CB_HostAddress` | String | 참가 측 접속 주소 | 생성 시 1회 |
| `CB_DisplayName` | String | 목록에 보일 방 이름 | 생성 시 1회 |
| `CB_CurrentPlayers` | Int64 | 목록에 보일 현재 인원 | 로비 인원 변화마다 (아래) |

#### 현재 인원도 우리가 실어 보내야 한다 — 엔진 값은 못 쓴다

**`ISession::GetNumOpenConnections()`는 검색 결과의 인원 조회에 쓸 수 없다.** 구현이 `NumMaxConnections - SessionMembers.Num()` 하나뿐인데(`SessionsCommon.h`), 그 `SessionMembers`를 채우는 제공자가 사실상 없다:

| 제공자 | `SessionMembers` | 결과 |
|---|---|---|
| Null (LAN) | 채우지 않음 — `AddLANSession()`이 **호스트조차 등록하지 않고**, `JoinSessionImpl()`도 참가자를 넣지 않음 | 항상 `Max - 0 = Max` |
| EOS (`OnlineServicesEOSGS`) | 채우지 않음 — `EOS_SessionDetails_Info`에서 `NumMaxConnections`만 옮기고 백엔드의 `NumOpenPublicConnections`는 버림 | 항상 `Max` |
| Steam 등 (`OnlineServicesOSSAdapter`) | V1의 `RegisteredPlayers`/`MemberSettings`에서 옮김 | 조건부로 동작 |

엔진 주석도 이걸 인정한다 — `ISessions::CreateSession` 위에 *"the creating user might not be added to the Session Members automatically, so a subsequent call to AddSessionMember is recommended"*.

그런데 `AddSessionMember`는 **`LocalAccountId`만 받아 자기 자신만 추가할 수 있다.** Null·EOS에는 이 목록을 동기화하는 백엔드 경로가 없어, 참가자가 자기 머신에서 불러봐야 호스트의 세션 객체에 닿지 않는다. 결국 **멤버 목록으로는 인원을 셀 수 없다.**

→ **호스트가 직접 세어 `CB_CurrentPlayers` 속성으로 광고한다.**

```
ACBLobbyGameMode::PostLogin / Logout
 └ Auth_RefreshReadyState()                       ← TotalCount 를 이미 세고 있음 (나가는 PlayerState 제외 처리까지)
    ├ LobbyGameState->Auth_SetReadyState(...)     ← 기존: 로비 UI 용 복제
    └ SessionSubsystem->Auth_UpdateAdvertisedPlayerCount(TotalCount)
       └ UpdateSessionSettings(Mutations.UpdatedCustomSettings["CB_CurrentPlayers"])
```

**재광고가 필요 없다.** LAN 검색 응답 패킷은 질의가 올 때마다 호스트의 현재 세션 객체에서 즉석으로 만들어지므로(`FSessionsLAN::OnValidQueryPacketReceived` → `AppendSessionToPacket`), 값을 고쳐두면 다음 검색부터 그대로 잡힌다. 커스텀 속성은 세 제공자 모두 광고를 지원하므로 **EOS·Steam으로 옮겨도 이 코드는 그대로 산다.**

- 생성 시점에 1(호스트)로 미리 채운다 — 로비가 열리기 전에 검색되면 0으로 보이기 때문
- 속성이 없는 방은 `CurrentPlayers = 0`으로 두되 **참가는 막지 않는다** (알 수 없음과 "빈 방"을 구분하지 않음)
- `Entry.bIsJoinable`은 `IsJoinable() && CurrentPlayers < MaxPlayers`다. `IsJoinable()` 단독으로는 위 이유로 **언제나 true**라 정원 판정이 되지 않는다

#### 매치가 시작되면 세션을 닫는다

게임플레이 레벨로 travel하면 `ACBLobbyGameMode`가 사라져 인원 광고도 멈춘다. **난입을 받지 않으므로 이동 직전에 세션을 닫는다** — `Auth_TravelToGameplayLevel()`이 `Auth_SetSessionAcceptingPlayers(false)`로 `bAllowNewMembers`를 내린다. 목록에 참가 불가로 보이므로 인원 값이 로비 시점에 굳어도 오해가 없다.

> 나중에 난입을 열게 되면 이 호출을 빼고 `ACBGameplayGameMode`에서도 같은 인원 갱신을 이어가야 한다.

#### PIE는 월드 컨텍스트로 서비스 인스턴스를 구분한다

`GetServices(EOnlineServices::Default, InstanceName)`의 두 번째 인자에 **월드 컨텍스트 핸들**을 넘긴다. 한 프로세스에 게임 인스턴스가 여럿인 PIE에서 이걸 빠뜨리면 **두 인스턴스가 같은 서비스를 공유해 서로의 세션이 섞인다.**

#### 계정 ID — 로그인이 들어올 자리

모든 세션 작업이 `FAccountId`를 요구한다. **Null 제공자는 시작 시 계정을 자동 생성**하므로(`FAuthNull::InitializeUsers`) 로그인 없이 바로 얻어진다. EOS는 로그인이 끝나야 유효해진다.

그래서 조회를 `Local_ResolveLocalAccountId()` 하나로 감쌌다 — **로그인 단계는 이 함수 앞에 붙기만 하면 되고 나머지 구조는 바뀌지 않는다.**

> **같은 PC 2인 테스트는 Null이 오히려 유리하다.** `FAuthNull::GenerateRandomUserId`가 **에디터이거나 첫 인스턴스가 아니면 랜덤 계정 ID**를 만들어, 두 인스턴스가 서로 다른 사용자가 된다. 반대로 EOS의 Device ID는 캐시 디렉터리가 사용자 폴더 하나라(`FEOSSDKManager::GetCacheDirBase`) **두 인스턴스가 같은 계정이 된다** — EOS 도입 후 로컬 2인 테스트는 Dev Auth Tool이 필요하다.

#### 접속 거부 — 실패 종류와 PIE 가드

정원 초과 거부가 실제로 어떤 경로로 오는지는 두 번 틀리기 쉬운 지점이라 남긴다.

**① 거부 사유는 실려 오지만, 실패 종류가 단계마다 다르다.**

`AGameSession::ApproveLogin`이 반환한 `"Server full."`은 `ErrorString`에 그대로 담겨 오는데, **어느 단계에서 거부됐느냐에 따라 종류가 갈린다.**

| 단계 | 실패 종류 | 경로 |
|---|---|---|
| **접속 중** (맵 로드 전) | **`PendingConnectionFailure`** | `UPendingNetGame::NotifyControlMessage`가 `ConnectionError`에 담아두고, `TickWorldTravel`이 방송 |
| 접속 후 (게임 중 강퇴 등) | `FailureReceived` | `UWorld::NotifyControlMessage`가 즉시 방송 |

**세션 목록에서 참가하다 정원 초과로 거부되는 건 앞쪽(`PendingConnectionFailure`)이다.** 두 종류를 같은 케이스로 묶고 문자열로 판별한다 — 한쪽만 처리하면 `default`로 떨어져 "네트워크 오류"라는 엉뚱한 문구가 나간다.

**② 접속 시도 중의 실패는 월드가 `NULL`로 방송된다.**

```cpp
// UnrealEngine.cpp — PendingNetGame(맵 로드 전) 단계
BroadcastNetworkFailure(NULL, Context.PendingNetGame->NetDriver, ENetworkFailure::PendingConnectionFailure, ...);
//                      ^^^^ 월드 없음
```

`InWorld != GetWorld()` 가드는 **`InWorld`가 `NULL`이면 통과**하고, `PendingNetGame`의 넷 드라이버는 `NM_Client`라 `bIsClientFailure` 가드도 통과한다. **가드 두 겹이 다 뚫린다** — PIE에서 클라이언트의 접속 실패에 호스트까지 메인 메뉴로 끌려간 원인이었다.

→ **`Local_IsOwnFailure()`가 내 월드 컨텍스트의 `PendingNetGame` 유무로 판별한다.** 접속을 시도 중인 인스턴스에만 `PendingNetGame`이 있으므로, 그것 자체가 "이 실패는 내 것"이라는 표식이 된다.

| 방송된 값 | 판별 |
|---|---|
| 월드가 유효 | `InFailureWorld == GetWorld()` 로 충분 |
| 월드가 `NULL` | 내 `FWorldContext::PendingNetGame` 이 있어야 내 것. 넷 드라이버까지 왔으면 `PendingNetGame->NetDriver` 와 대조 |

> **"월드가 없으면 우리 것"이라는 옛 전제는 단독 실행에서만 맞았다.** PIE 다중 인스턴스에서는 "월드가 없다"가 "내 것"을 뜻하지 않는다.

#### 엔진의 자동 복귀 — 접속 시도 실패에서만 끊는다

**우리가 `OpenLevel`을 부르지 않아도 엔진이 기본 맵을 다시 로드한다.** 커넥션이 닫히면 `?closed` 옵션이 붙은 URL로 `Browse`가 불리고, 그 분기가 무조건 `GameDefaultMap`을 로드하기 때문이다.

```
BroadcastNetworkFailure
 └ UEngine::HandleNetworkFailure            (UEngine::Init 에서 스스로 구독 — 우리 핸들러보다 먼저 실행)
     └ CallHandleDisconnectForFailure
         └ GameInstance->GetOnlineSession()->HandleDisconnect()   ★ 유일한 통로
             └ GEngine->HandleDisconnect() → SetClientTravel("?closed")
                 └ 다음 틱 Browse("?closed") → LoadMap(GameDefaultMap)
```

로그의 `Connection failed; returning to Entry` 가 그 신호다.

**그대로 두면 참가 실패 시 모달을 띄울 수 없다.** 메인 메뉴에 머문 채 실패했는데도 맵이 재로드돼, 모달이 뜨자마자 위젯과 함께 사라진다.

→ **`UCBOnlineSession`이 그 통로를 가로챈다.** 엔진이 게임 인스턴스에게 온라인 세션 클래스를 묻는 `GetOnlineSessionClass()`를 `UCBGameInstance`가 오버라이드해 이 클래스를 물린다. 판단은 서브시스템이 하고 이 클래스는 전달만 한다.

```cpp
// UCBOnlineSession::HandleDisconnect
if (SessionSubsystem && SessionSubsystem->Local_ShouldKeepCurrentLevel(InNetDriver)) return;  // 엔진 복귀 차단
Super::HandleDisconnect(InWorld, InNetDriver);                                                // 그 외는 엔진에 맡김
```

**단계 구분은 넷 드라이버 이름으로 한다.** `NAME_PendingNetDriver`면 맵 로드 전 접속 시도, `NAME_GameNetDriver`면 인게임 커넥션이다. **월드로는 구분할 수 없다** — `CallHandleDisconnectForFailure`가 맵 로드 전 실패에도 `Context.World()`(=떠나온 메인 메뉴 월드)를 채워 넘기기 때문이다. 델리게이트로 오는 `World == NULL`과 헷갈리기 쉬운 지점이다.

| | 접속 **시도** 실패 | 접속 **후** 끊김 |
|---|---|---|
| 대표 종류 | `PendingConnectionFailure` | `ConnectionLost` / `ConnectionTimeout` / `FailureReceived` |
| 넷 드라이버 이름 | `PendingNetDriver` | `GameNetDriver` |
| 서 있는 레벨 | 메인 메뉴가 그대로 살아 있음 | 이미 로비·게임플레이 |
| 처리 | **제자리 + `OnConnectionFailed` 방송** | 사유 보관 → 메인 메뉴 복귀 |
| 사유 표시 | 살아 있는 위젯이 방송을 받아 모달 | `Local_ConsumePendingFailureReason()` |

**접속 후 끊김만 사유를 보관한다.** 레벨을 넘기면 방송을 받을 위젯이 그 순간에 없기 때문이다 — 옛 레벨의 위젯은 곧 파괴되고 메인 메뉴 위젯은 아직 없다. 반대로 제자리 실패까지 보관하면 **다음에 정상적으로 메인 메뉴에 들어왔을 때 지난 사유가 뒤늦게 뜬다.**

| | |
|---|---|
| 참가 실패 (제자리) | `OnConnectionFailed` 구독 → 모달 |
| 접속 후 끊김 | `Event Construct` → `Local_ConsumePendingFailureReason()` → 모달 |

> **인게임 끊김은 막지 않는다.** 서버가 사라진 월드에는 머물 수 없고, `UWorld::TickNetClient`가 커넥션이 닫힌 채로 있으면 **매 프레임 `ConnectionLost`를 다시 방송**하기 때문이다. 막으면 모달이 무한히 뜬다.

> `Local_HandleConnectionFailure` 의 `if (MainMenuMap.IsEmpty()) return;` 는 **우리 쪽 중복 `OpenLevel` 을 막는 가드로 여전히 유효하다.** 엔진이 재로드하는 것과는 별개다.

#### 정원 판정은 세션이 아니라 서버가 한다

**세션 계층으로는 정원을 막을 수 없다.** 검색 결과는 스냅샷이고(LAN은 5초짜리), 클릭과 접속 사이에 남이 들어갈 수 있다. 게다가 **엔진의 `IsJoinable()`은 언제나 true다** — `GetNumOpenConnections()`가 멤버 목록에서 계산되는데 그 목록을 채우는 제공자가 없기 때문이다(위 "현재 인원도 우리가 실어 보내야 한다"). `CB_CurrentPlayers`로 정원 판정을 흉내내지만 그것도 **표시용 스냅샷일 뿐 권위가 아니다.**

그래서 판정은 **접속 시점의 서버**가 한다. 엔진에 이미 있는 경로다.

```
Local_JoinFoundSession()          ← 세션 계층. 권위 없음. 아래 가드는 스냅샷 기준일 뿐
 ├ GetSessionById() 재확인        ← 만료·정원 참 방을 걸러 헛된 접속 시도를 막음 (아래)
 └ Local_JoinServerByAddress() → ClientTravel
     └ [서버] AGameModeBase::PreLogin
         ├ AGameSession::ApproveLogin() → AtCapacity() → "Server full."      → 거부
         └ ACBGameplayGameMode → 진행 중인 매치       → "Match in progress." → 거부
```

**경합이 있어도 안전하다** — 마지막 한 자리를 두고 둘이 들어와도 서버가 한 명만 받는다. 세션 목록의 정확도와 무관해진다.

#### 참가 직전 재확인은 "새로 조회"가 아니다

`Local_JoinFoundSession()`은 참가 전에 `GetSessionById()`로 방 상태를 다시 읽고 `bIsJoinable`이 false면 멈춘다. **다만 이건 새 조회가 아니다** — `FSessionsCommon::GetSessionById()`는 `AllSessionsById` 맵을 뒤지는 **동기 로컬 조회**이고, 그 맵은 `AddSearchResult()`가 **검색할 때만** 채운다. 즉 마지막 검색 시점의 스냅샷을 다시 읽는 것이라, **검색 이후 시작된 방은 이 가드로 걸러지지 않는다.**

그래도 두는 이유:

- 목록에서 사라졌거나 **LAN 세션 ID가 만료된** 방을 잡아 "새로고침해 주세요" 안내로 돌릴 수 있다
- 인덱스가 어긋나 엉뚱한 방에 붙는 사고를 막는다
- 재확인한 값으로 `FoundSessions[Index]`를 갱신하므로 위젯이 새로고침 없이 최신 상태를 볼 수 있다

**낡은 목록으로 진행 중인 매치에 붙는 것은 서버가 막는다** (바로 아래). 세션을 닫은 건 광고를 닫은 것이지 커넥션을 막은 게 아니다.

> 검색 결과 항목을 만드는 로직은 `Local_BuildSearchEntry()` 하나에 모여 있다. 목록 표시와 참가 가드가 **같은 기준**을 쓰게 하려는 것으로, 한쪽만 고쳐 판정이 어긋나는 걸 막는다.

#### 난입 차단은 `ACBGameplayGameMode::PreLogin`

`ACBGameplayGameMode`가 `PreLogin`을 오버라이드해 접속을 거부한다. **게임플레이 레벨에 새로 접속을 시도한다는 것 자체가 "매치가 이미 시작됐다"는 뜻**이므로 별도 상태 판단이 필요 없다.

```cpp
Super::PreLogin(...);                                    // 정원 판정 등 엔진 기본 절차 먼저
if (!ErrorMessage.IsEmpty()) return;                     // 이미 거부됐으면 사유를 덮어쓰지 않음
if (bAllowJoinInProgress) return;                        // 난입을 열었으면 통과
ErrorMessage = UCBSessionSubsystem::MatchInProgressError;
```

- **로비에서 함께 출발한 인원은 영향받지 않는다.** `bUseSeamlessTravel = true`(`ACBGameModeBase`)라 기존 플레이어는 `HandleSeamlessTravelPlayer`로 이관되며 `PreLogin`을 타지 않는다. **이 전제가 깨지면 전원이 튕긴다** — seamless travel을 끄게 되면 이 거부 조건부터 다시 봐야 한다
- **거부 문자열은 `UCBSessionSubsystem::MatchInProgressError` 한 곳에 있다.** 서버가 실어 보내고 클라이언트의 `Local_HandleNetworkFailure`가 읽어 "이미 시작된 게임입니다"로 가른다 — 양쪽이 같은 값을 봐야 하므로 상수를 공유한다. 엔진의 `"Server full."` 과 같은 자리(`ErrorMessage`)에 실려 온다
- **난입·재접속을 열려면** `bAllowJoinInProgress`(게임플레이 게임모드 BP)를 켜고, 로비 게임모드의 `Auth_SetSessionAcceptingPlayers(false)` 호출을 빼면 된다

> **`AGameSession`은 `UCBSessionSubsystem`과 이름만 같고 완전히 별개다.** 전자는 서버에만 있는 게임 프레임워크 액터(권위), 후자는 각 클라이언트의 방 목록 계층(비권위)이다.

##### 정원 값은 URL 옵션으로 흘려보낸다

`AGameSession::MaxPlayers`는 `globalconfig` 기본값을 갖지만 그건 **우리가 이 방에 원하는 인원이 아니다.** `InitOptions`가 URL의 `?MaxPlayers=N`을 읽으므로 그 통로를 쓴다.

```
Local_CreateAndHostSession(레벨, 이름, N)
 ├ 세션 광고: NumMaxConnections = N              (표시용, 비권위)
 └ Local_HostLobby(레벨, N)
     └ OpenLevel(..., "listen?MaxPlayers=N")     (권위)

ACBLobbyGameMode::Auth_TravelToGameplayLevel()
 └ ServerTravel(맵 + "?listen?MaxPlayers=" + GameSession->MaxPlayers)
```

- **출처가 하나(`InMaxPlayers`)라 표시와 실제 정원이 어긋날 수 없다.** 클래스 기본값으로 고정하지 않은 이유는 방 크기를 만들 때 고르게 하기 위해서다
- **맵을 넘으면 정원이 초기화된다.** `AGameSession`은 레벨마다 새로 스폰되어 `InitOptions`를 다시 읽으므로, `ServerTravel`에도 다시 실어야 게임플레이 레벨이 엔진 기본값으로 돌아가지 않는다
- **거부 사유는 `ErrorString`에 `"Server full."`로 실려 온다.** `Local_HandleNetworkFailure`가 이를 보고 "방이 가득 찼습니다"로 구분한다 — 검색 목록이 낡아 생기는 정상적인 경우라 일반 접속 실패와 섞으면 안 된다

> **표시용 현재 인원은 `CB_CurrentPlayers` 속성에서 온다** (위 "현재 인원도 우리가 실어 보내야 한다"). 정원 출처가 하나여도 **표시 인원은 검색 시점의 스냅샷**이라 실제와 어긋날 수 있다 — 권위는 여전히 `AGameSession`에 있으므로 표시가 낡아도 안전에는 영향이 없다.

#### 세션 정리는 우리가 해야 한다 — 엔진이 안 치운다

**온라인 서비스 인스턴스는 게임 인스턴스보다 오래 산다.** 엔진 어디에서도 `DestroyService`를 부르지 않아(OSSAdapter 모듈이 자기 것을 정리하는 경우가 유일하다) 서비스가 레지스트리에 계속 남고, PIE 는 월드 컨텍스트 핸들을 재사용하므로 **지난 실행의 세션이 다음 실행의 검색에 그대로 잡힌다.** 에디터를 껐다 켜지 않는 한 계속 쌓인다.

그래서 `UCBSessionSubsystem::Deinitialize()`에서 `Local_LeaveActiveSession()`을 부른다. 세션 생성의 대칭 짝이다.

**접속 실패도 같은 정리를 탄다.** `Local_HandleConnectionFailure()`가 사유를 알린 뒤 `Local_LeaveActiveSession()`을 부른다 — 커넥션이 끊긴 이상 그 세션에 남아 있을 이유가 없고, 정리하지 않으면 **참가 실패 후 다시 참가할 때 이전 세션이 남아 걸린다.** 특히 제자리에 머무는 참가 실패는 그 자리에서 재시도하는 흐름이라 이 정리가 없으면 두 번째 시도가 조용히 실패한다.

**여기에 순서 함정이 하나 있다.** `UGameInstance::Shutdown()`은 이 순서로 진행된다:

```
RemoveLocalPlayer(...)          ← 로컬 플레이어 제거가 먼저
SubsystemCollection.Deinitialize()  ← 그 다음 우리 Deinitialize
WorldContext = nullptr              ← 월드 컨텍스트는 마지막
```

즉 우리 `Deinitialize()` 시점에는 **`GetFirstGamePlayer()`가 이미 nullptr** 이라 계정 ID 를 다시 조회할 수 없다. 조회에 실패하면 이탈이 **조용히 건너뛰어진다.**

→ **세션에 들어간 시점의 계정 ID 를 `ActiveSessionAccountId` 에 보관**해 두고 이탈에 쓴다. `WorldContext` 는 아직 살아 있어 서비스 인스턴스 조회는 정상 동작한다.

> **방을 열 때는 서비스를 아예 새로 만든다** — 아래 "LAN 비콘은 자기가 본 모든 방을 광고한다"를 볼 것. 여기의 이탈 정리와 목적이 다르다 — 그쪽이 치우는 것은 **내가 만든 세션**이고, 저쪽이 비우는 것은 **남에게서 주워 온 세션 캐시**다.

#### LAN 검색은 5초 고정이고, 같은 방이 여러 번 올라온다

Null 제공자(LAN 비콘)의 검색 동작 두 가지는 **버그가 아니라 설계**다. 모르고 보면 둘 다 우리 코드 문제로 보인다.

| 상수 | 값 | 결과 |
|---|---|---|
| `LAN_QUERY_TIMEOUT` | 5초 | **응답이 즉시 와도 5초를 채운 뒤** 콜백이 온다 |
| `LAN_QUERY_RETRY_TIME` | 1초 | 5초 동안 **1초마다 질의를 다시 브로드캐스트**한다 |

- **검색은 항상 5초 걸린다.** `OnValidResponsePacketReceived`는 결과를 담기만 하고, promise 는 `OnLANSearchTimeout`에서만 채워진다. 조기 종료가 없다 → **UI에 "검색 중" 표시가 필수**다
- **같은 방이 최대 5개로 보인다.** 호스트는 질의를 받을 때마다 응답하고, 엔진은 `AddSearchResult`에서 `AddUnique`가 아닌 **`Add`** 로 쌓는다. 그래서 `Local_HandleFindSessionsComplete`가 `TSet`으로 **세션 ID 중복을 걸러낸다**. EOS 로 바꿔도 중복이 없으면 그냥 통과하므로 그대로 둔다

#### LAN 비콘은 자기가 본 모든 방을 광고한다 — 방 열기 직전 서비스 리셋

**증상**: 방을 만들고 나간 뒤에도 그 방이 남의 검색 목록에 계속 잡힌다. 검색을 반복할수록 목록이 불어나고, 프로세스를 재시작하면 사라진다.

**원인은 결함 두 개가 겹친 것이다.**

| | 위치 | 내용 |
|---|---|---|
| ① 캐시 누수 | `FSessionsCommon` (**공통층**) | 검색으로 발견한 세션이 `AllSessionsById`에 들어간 뒤 제거되지 않는다. 빼는 함수 `ClearSessionById()`는 `LeaveSession` 경로에서만 불리는데, 발견만 한 세션은 로컬 이름이 없어 그 경로를 못 탄다. 다음 검색은 `SearchResultsUserMap`만 `Reset()`할 뿐 이 맵은 건드리지 않는다 |
| ② 캐시 전체 재광고 | `FSessionsLAN` (**LAN 전용**) | `OnValidQueryPacketReceived()`가 질의에 응답할 때 `AllSessionsById`를 **전부** 순회해 `JoinPolicy == Public`인 것을 모두 뿌린다. **소유자 검사가 없다** |

→ **한 번이라도 검색한 프로세스가 방을 열면, 그때까지 본 모든 방(이미 죽은 방 포함)을 자기 비콘으로 함께 광고한다.**

죽은 방이 남의 검색에 잡히고, 그걸 받은 쪽이 자기 캐시에 담았다가 나중에 다시 뿌리므로 **유령이 프로세스 사이를 왕복하며 늘어난다.** 내가 만든 방은 나갈 때 정상 제거되지만, 상대가 되뿌린 것을 검색으로 다시 주워 담아 부활한다.

**오염된 쪽은 자기 오염을 볼 수 없다.** 내 검색 결과는 `SearchResultsUserMap`(매 검색 리셋)에서 나오고 `AllSessionsById`는 결과에 쓰이지 않는다. 오염은 내가 호스팅을 시작하는 순간 **남의 눈에만** 드러난다. 두 프로세스의 로그를 겹쳐 보기 전에는 원인이 잡히지 않는 이유다.

**대응**: `Local_CreateAndHostSession()` 맨 앞에서 `Local_ResetOnlineServices()`가 `UE::Online::DestroyService()`로 이 게임 인스턴스의 서비스를 파괴한다. 다음 조회가 빈 캐시로 새로 만들므로, **비콘이 켜지는 시점의 캐시에는 방금 만든 방 하나만 남는다.** 프로세스를 갓 재시작한 것과 같은 상태다.

- **`bUseLANSessions` 일 때만 한다.** 조건을 제공자 이름에 걸지 않는 이유는 `FSessionsEOSGS` 가 `FSessionsLAN` 을 상속해 **EOS 라도 LAN 세션이면 같은 비콘 경로를 타기** 때문이다(`SessionsEOSGS.cpp` 의 `CreateSessionImpl`·`FindSessionsImpl` 이 LAN 구현으로 위임한다). 정상 EOS 경로에서는 광고 주체가 백엔드라 ①이 남아도 무해하다 — 그래서 EOS 로 옮기면 이 코드는 자동으로 죽는다
- **나가기 경로에는 넣지 않는다.** 비동기 `LeaveSession` 이 떠 있는 직후라 EOS 에서는 실제 백엔드 호출이 날아간다. 유령을 뿌리는 건 호스팅하는 쪽뿐이므로 "방 열기 직전" 한 곳이면 충분하다
- **세션 인터페이스·계정 ID 조회보다 반드시 먼저 부른다.** `ISessionsPtr` 을 들고 있는 채로 파괴하면 엔진이 참조 경고를 찍고, 미리 얻어둔 값은 죽은 인스턴스의 것이 된다
- 파괴와 함께 이전 검색 결과의 세션 ID 가 무효가 되므로 `FoundSessions`·`FoundSessionIds` 도 함께 비운다
- 계정 ID 가 새로 생성된다(Null 은 랜덤). 세션 작업마다 새로 조회하므로 영향 없다

> **전제 — 검색 중에는 UI 가 버튼을 모두 잠근다.** 이 리셋에는 C++ 가드가 없고, 그 자리를 UI 가 맡는다.
>
> LAN 검색은 5초 고정이라(위 "LAN 검색은 5초 고정") 그 사이 방 만들기를 누를 수 있는데, 그러면 `DestroyService` 가 진행 중인 검색 op 를 `Flush(Shutdown)` 으로 끊는다. 검색 실패 방송이 **`Local_ResetOnlineServices()` 실행 도중 재진입으로** 터지므로, 이를 구독한 위젯이 팝업이나 스택을 건드리면 흐름이 꼬인다. 재검색을 눌러도 엔진이 `Errors::AlreadyPending()` 으로 거절해(`FSessionsCommon::CheckState(FFindSessions)`) 목록이 잠깐 비었다 다시 채워진다.
>
> 둘 다 UI 에서 막는 쪽이 훨씬 싸므로 **C++ 에는 진행 중 플래그를 두지 않았다.** 검색 시작~완료 사이 버튼 잠금이 이 코드의 전제이니, 검색 UI 를 새로 만들 때 이 잠금을 빠뜨리지 말 것.

#### 알려진 무해한 로그 (조치 불필요)

| 로그 | 원인 | 판단 |
|---|---|---|
| `LogOnlineSchema: Error: Invalid schema category Lobby/LobbyMember`<br>`LogOnlineServices: Error: [FLobbiesCommon::Initialize] Failed to initialize schema registry` | **우리가 쓰지 않는 Lobbies 기능의 초기화 실패.** `FOnlineServicesNull::RegisterComponents()`가 `FLobbiesNull`을 `FSessionsNull`과 **무조건 함께** 등록해, Null 제공자용 Lobby 스키마 설정이 없는 상태로 초기화가 돌아 나온다 | **그대로 둔다.** `FLobbiesCommon::Initialize()`는 실패해도 **로그만 찍고 그냥 반환**한다(`return` 없음, 컴포넌트 비활성화 없음). 우리는 `ILobbies`를 어디서도 호출하지 않으므로 영향 범위 밖이며, 서비스 초기화 시 **한 번만** 뜬다 |

> **에디터에서 `Previously active world ... not cleaned up by GC` + `REINST_...` + `TransBuffer` 조합이 뜨면 세션과 무관하다.** PIE 중 블루프린트를 컴파일해 생긴 리인스턴스 찌꺼기를 에디터 Undo 버퍼가 붙잡아 옛 PIE 월드가 GC되지 못하는 것으로, **에디터를 재시작하면 해소된다.** 패키징 빌드에는 없는 문제다.

### 나가기 — `Local_LeaveToMainMenu()`

들어오는 두 경로와 **같은 창구에서 나간다.** 접속에 관한 것은 전부 여기 모으기 때문이다(위).

| 위치 | 동작 |
|---|---|
| 클라이언트 | 로컬 PC의 **`ClientTravel(메인 메뉴, TRAVEL_Absolute)`** |
| 호스트·단독 | **`OpenLevel(메인 메뉴)`** |

- **메인 메뉴 맵은 `GameDefaultMap` 하나에서 온다.** 접속 실패 복귀와 같은 판정(`Local_ResolveMainMenuTravelMap()`)을 공유하며, 이미 메인 메뉴면 빈 문자열을 반환해 아무것도 하지 않는다
- **클라이언트만 `ClientTravel`인 이유는 커넥션 정리다.** 나가는 순간 서버가 `Logout`을 받아 로비 인원 집계가 갱신된다(아래 "준비 완료와 게임 시작"의 `Logout` 제외 처리와 한 쌍)
- **호스트가 나가면 남은 클라이언트는 자동으로 따라 나온다.** 커넥션이 끊겨 각자 `ConnectionLost` → 기존 실패 복귀 경로를 탄다. 호스트 자신에게도 실패가 올라오지만 넷 드라이버가 `NM_ListenServer`라 기존 가드에 걸려 무시된다 — **추가 코드가 없다**
- **레벨을 가리지 않는다.** 넷 모드만 보므로 게임플레이 레벨(일시정지 메뉴)에서도 그대로 쓸 수 있다. 다만 그 배선은 아직 하지 않았다(아래 미구현)
- **세션에 들어가 있으면 먼저 빠져나온다.** 호스트는 파괴(`bDestroySession=true`), 참가자는 떠나기만 한다. **완료를 기다리지 않는다** — 이동은 다음 프레임이고, 기다리면 나가기가 온라인 서비스 응답에 묶인다

#### 나가는 중의 접속 끊김은 경고로 띄우지 않는다 — `bLeaveRequested`

클라이언트가 스스로 커넥션을 닫으면 **그 끊김이 자기 자신의 실패 핸들러로 돌아올 수 있다.** 그대로 두면 "나가기를 눌렀는데 접속 끊김 경고창이 뜨는" 형태가 된다(모달을 배선하는 순간 드러난다).

```
Local_LeaveToMainMenu()      → bLeaveRequested = true
Local_HandleConnectionFailure → 방송은 건너뛰고, 메뉴 복귀는 그대로 수행
Local_HostLobby / Local_JoinServerByAddress → bLeaveRequested = false  (새 세션 = 재무장)
```

**막는 것은 방송뿐이고 복귀는 막지 않는다.** 나가기 이동 자체가 실패해도 메뉴에는 도착해야 하기 때문이다.

#### 위젯을 먼저 빼고 부른다

게임 시작 때와 **같은 규칙이다** — 아래 "이동 전에 로비 위젯을 반드시 스택에서 뺀다"의 근거가 그대로 적용된다. 나가기도 맵 전환이고, 로비 위젯은 `Should Remove Gameplay IMCs = true`다.

```
[BP] 나가기 버튼 / IA_CB_Leave
 ├ Remove This Widget            ← 스택 재계산 → IMC 복구
 └ Local_LeaveToMainMenu()
```

- **신호를 새로 만들지 않는다.** 게임 시작이 `Multicast_NotifyMatchStarting`이었던 건 **서버가 전원을 대신 이동시키기 때문**이고, 나가기는 **누른 사람 혼자의 로컬 동작**이라 위젯이 스스로 빠지면 끝이다
- **같은 프레임에 제거 → 이동 요청이어도 안전하다.** `OpenLevel`·`ClientTravel`은 `TravelURL`만 세팅하고 실제 이동은 `UEngine::TickWorldTravel`(다음 프레임)이다
- **증상이 늦게 나타나므로 더 위험하다.** 로비는 게임플레이 IMC를 애초에 안 붙이고 도착지인 메인 메뉴엔 조작할 캐릭터가 없어, 빠뜨려도 그 자리에서는 멀쩡해 보인다. 문제는 **메뉴 → 호스트 → 로비 → 게임플레이로 한 바퀴 돈 뒤**에 드러난다

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

같은 경계가 UI에도 그대로 나타난다 — 팩 HUD는 맵 단위로 파괴되지만 위젯 인스턴스 캐시(`BP_EGUI_GameUIRegistry`)는 GameInstance가 소유해 게임 전체를 산다. → [EasyGameUI.md](EasyGameUI.md)

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

**엔진 기본 값에 이미 있는 것은 다시 옮기지 않는다.** 닉네임(`PlayerName`)은 `Super::CopyProperties`가 `SetPlayerNameInternal`로 옮기므로 여기에 줄을 추가할 필요가 없다 (아래 "닉네임" 참고).

**PlayerState에 맵을 넘어야 할 값을 추가하면 이 함수도 함께 고친다.** 빠뜨려도 컴파일은 통과하고 로비 안에서는 정상 동작하므로, 게임 레벨에 도착해서야 값이 비어 있는 것으로 발견된다.

#### PIE는 seamless travel을 끈다 — `net.AllowPIESeamlessTravel=1`

엔진이 **PIE에서 seamless travel을 강제로 하드 travel로 되돌린다**(`GameModeBase.cpp:503`). 하드 travel이면 `SeamlessTravelFrom` → `SeamlessTravelTo` → `DispatchCopyProperties` 경로 자체가 실행되지 않아 **`CopyProperties`가 아예 불리지 않는다.**

```
ProcessServerTravel: Seamless travel is disabled in PIE, set net.AllowPIESeamlessTravel=1 to enable.
```

이 경고가 유일한 단서다. `Config/DefaultEngine.ini`의 `[ConsoleVariables]`에 켜 두었다(에디터 재시작 필요). **별도 프로세스 Standalone은 `EWorldType::Game`이라 이 제한을 받지 않는다.**

#### 값이 넘어와도 적용은 별개다

`CopyProperties`가 성공해도 **새 폰에 반영되는 것은 다른 문제다.** 실제로 의상 이관이 실패했을 때 값은 4개 모두 정상적으로 넘어와 있었고, 적용 함수가 폰이 없는 시점에 한 번 불리고 끝나 있었다. seamless travel 직후에는 **폰이 붙은 뒤에 새 PlayerState의 `BeginPlay`가 실행**될 수 있어, 구독만으로는 방송을 놓친다. → [Cosmetic.md](Cosmetic.md) "적용 타이밍"

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
- **HUD는 로컬에만 존재** — 서버가 원격 플레이어의 PC로 `GetHUD()`를 부르면 영원히 `nullptr` → [UI.md](UI.md)

## 로비는 실제 Chaser를 스폰한다

프리뷰 전용 액터를 만들지 않고 **`ACBChaserCharacter`를 그대로 스폰**한다.

| | 프리뷰 전용 액터 | **캐릭터 스폰 (채택)** |
|---|---|---|
| 복제·배치 | 직접 구현 | **엔진 기본 스폰 흐름** (PlayerStart) |
| 프리뷰 정확도 | 실제와 어긋날 수 있음 | **완전 일치** |
| 카탈로그 주입 | 별도 경로 신설 | **로드아웃 경로 그대로** |
| 로드 비용 | 적음 | 큼 (**로비 대기 중에 미리 로드되어 게임 진입이 빨라짐**) |

대신 로비에서 처리할 것:

- **캐릭터가 조작되면 안 된다** → **로비에서는 매핑 컨텍스트를 아예 등록하지 않는다.** `ACBChaserController`가 로비가 아닐 때만 `AllowGameplayInput()`을 호출한다 → [Input.md](Input.md)
- **카메라** → 빙의하면 3인칭 뷰가 되므로 고정 카메라로 덮어쓴다 (아래)

### 무기 변경 = 캐릭터 변경 — 폰을 다시 스폰한다

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

**ASC 부여분은 반드시 회수한다.** 플레이어의 ASC는 PlayerState 소유라 **폰을 갈아도 살아남는다**(→ [ASC-Ownership.md](ASC-Ownership.md)). 회수하지 않으면 이전 무기의 어빌리티와 스탯 GE가 남은 채 새 로드아웃이 얹혀 **바꿀수록 쌓인다.** 반대로 무기 액터는 `UCBCombatComponent::EndPlay`가 정리하므로 폰만 파괴하면 된다.

**자리를 기억해야 한다.** 엔진 기본 `ChoosePlayerStart`는 비어 있는 스타트 중에서 고르므로, 폰을 파괴하고 다시 스폰하면 **다른 자리로 옮겨간다.** `ACBLobbyGameMode`가 컨트롤러별로 처음 배정한 스타트를 기억해(`AssignedPlayerStarts`) 재스폰에도 같은 자리를 쓰고, `Logout`에서 비운다.

**준비 상태에서는 바꿀 수 없다.** 준비는 무기 장착 어빌리티를 실행하므로(아래 "준비 완료와 게임 시작") 재스폰과 엉킨다. 서버가 요청을 거부하며, 위젯은 `ACBPlayerState::OnPlayerReadyChanged`를 구독해 준비 중에 버튼을 비활성화한다.

**반대로 로드 중에는 준비할 수 없다.** 재스폰된 폰은 로드아웃을 다시 비동기 로드하므로(→ [SystemReady.md](SystemReady.md)), 그 사이의 준비 요청은 ASC·로드아웃이 미완성인 캐릭터에 무기 장착 어빌리티를 걸어 **조용히 실패한다.** 두 방향이 대칭으로 잠긴다.

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

**복제하지 않는다.** "내 화면의 내 폰이 로드됐나"는 각 인스턴스가 스스로 판정할 수 있고, UI는 로컬이다(→ [UI.md](UI.md)). 서버 가드는 서버가 자기 쪽 폰을 직접 보므로 이 값에 의존하지 않는다.

**클래스는 로비에서 미리 로드한다.** 카탈로그의 캐릭터 클래스는 소프트 참조라 스폰 시점의 동기 로드가 히칭이 된다. `ACBLobbyGameState::BeginPlay`가 전부 비동기로 미리 로드하는데, **게임 스테이트는 서버·전 클라이언트에 모두 존재**하므로 복제로 도착하는 남의 폰 클래스까지 한 번에 확보된다(게임모드에 두면 서버만 데워진다).

**의상 선택은 유지된다.** `ACBPlayerState::HandlePawnSet`이 새 폰에 조합을 다시 적용한다(→ [Cosmetic.md](Cosmetic.md)). 단 새 캐릭터의 의상 카탈로그에 없는 파츠는 검증에서 걸러져 무시되므로, **캐릭터끼리 카탈로그를 공유하지 않으면 조합이 부분적으로 빠진다.**

> 재빙의는 뷰 타겟도 다시 정하게 만든다. 커스터마이징 뷰 중이라면 `bInCosmeticView` 게이트가 클로즈업을 유지한다 (아래 "`AutoManageActiveCameraTarget`은 빙의 한 번에 5~7회 불린다").

### 로비 UI는 누가 띄우나 — 신호는 C++, 표시는 BP

팩의 스택 API가 **BP 전용**이라 C++에서 위젯을 올리려면 `ACBHUD`에 다리를 하나 더 뚫어야 하고, 그만큼 **팩 에셋 수정 항목이 늘어난다**(→ [EasyGameUI.md](EasyGameUI.md)). 그래서 표시는 BP에 두고 **C++은 "언제인지"만 알려준다.**

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

리슨 서버 호스트는 **빙의가 매우 일러서 방송이 액터 `BeginPlay`보다 먼저 갈 수 있다.** 구독만 하면 호스트에서 위젯이 뜨지 않는다. [SystemReady.md](SystemReady.md)의 "구독 또는 즉시" 패턴과 같다.

> **`OnLocalPlayerReady`는 1회성이다 — 캐릭터 재스폰에는 쓸 수 없다.** `UCBLocalReadySubsystem::NotifyLocalPlayerReady()`와 `ACBChaserController::Local_ApplyReadyState()` 양쪽에 one-shot 가드가 있어, 첫 캐릭터가 준비될 때 한 번 방송하고 끝난다(페이드 인이 두 번 일어나지 않게 하려는 의도이므로 가드를 풀면 안 된다). **무기 변경으로 폰이 재스폰될 때마다 필요한 신호는 `ACBPlayerState::OnCharacterLoadedChanged`** 다(위 "무기 변경 = 캐릭터 변경"). 둘은 이름이 비슷하지만 수명이 다르다 — "레벨에 처음 들어왔나"와 "지금 폰이 로드됐나"의 차이다.

> **선언은 타입이지 인스턴스가 아니다.** 공용 헤더에 `DECLARE_DYNAMIC_MULTICAST_DELEGATE`를 두어도 공유되는 것은 타입뿐이며, 실제 신호는 인스턴스를 소유한 객체가 갖는다. `static` 전역 델리게이트는 **BP에서 바인딩할 수 없고**(BlueprintAssignable은 UObject의 UPROPERTY여야 한다), PIE는 서버·클라가 한 프로세스라 **인스턴스가 공유되어 신호가 섞인다.**

#### 로비 조작 차단은 UI에 기대지 않는다

처음에는 "로비 UI가 `Menu` 레이어에 뜨면 게임플레이 IMC가 걷힌다"로 설계했으나 **두 가지가 깨져서 바꿨다.**

| 문제 | 내용 |
|---|---|
| **순서 경합** | 팩의 제거는 **그 시점의 스냅샷**을 대상으로 한다. UI가 캐릭터보다 먼저 뜨면 걷어낼 IMC가 없고, 그 뒤 로드아웃이 붙인 IMC는 사정권 밖이라 **로비에서 캐릭터가 움직인다** |
| **UI 의존** | 클라이언트에서 UI 삽입이 실패하면(HUD가 늦게 생기는 경우) 조작이 그대로 살아난다 |

**해법: 로비에서는 IMC를 붙이지 않는다.** 걷어낼 것이 없으므로 순서도 UI 성공 여부도 무관해진다. `ACBChaserController::Local_TryAllowGameplayInput()`이 `ACBLobbyCamera` 유무로 판별해, **게임플레이 레벨에서만** `AllowGameplayInput()`을 호출한다 → [Input.md](Input.md)

"캐릭터는 게임플레이 레벨에서만 조작된다"는 사실을 코드에 드러낸 것이고, **실패 모드도 조용한 쪽(로비에서 움직임)에서 시끄러운 쪽(게임플레이에서 안 움직임)으로 뒤집힌다.**

> 이 판별이 `ACBLobbyCamera`에 의존하므로, 카메라 액터가 **"로비 표식" 역할을 겸하게 됐다.** 로비 판별에 걸리는 것이 더 늘어나면 복제되는 별도 표식(GameState 플래그 등)으로 옮긴다.

**회전은 배치만으로 해결된다.** `UCBCharacterRotationComponent`는 **가속도가 있을 때만** 회전 타겟을 갱신하고 초기값은 스폰 당시 액터 회전이다. 입력이 걷힌 로비에서는 `PlayerStart`의 Yaw가 그대로 유지되므로, 스타트를 카메라 쪽으로 돌려두면 캐릭터가 정면을 본다. 코드가 필요 없다.

### 닉네임 — 새 변수를 만들지 않고 엔진 `PlayerName`을 쓴다

**닉네임 저장 필드를 따로 두지 않는다.** `APlayerState`의 `PlayerName`이 이미 복제되고, `CopyProperties`로 맵도 넘어가고, `SetPlayerName()`은 **리슨 서버·단독 실행이면 `OnRep_PlayerName()`을 직접 호출**해 준다(`PlayerState.cpp:276`). 커스텀 필드를 만들면 이 셋을 전부 다시 짜야 하는 데다, 엔진·플러그인이 보는 이름과 화면에 보이는 이름이 갈라진다.

그래서 `ACBPlayerState`가 하는 일은 **변경 신호를 얹는 것뿐**이다.

```cpp
void ACBPlayerState::OnRep_PlayerName()
{
    Super::OnRep_PlayerName();                             // 이전 이름 갱신·환영 메시지
    OnPlayerNicknameChanged.Broadcast(GetPlayerName());    // 위젯이 표기를 갱신
}
```

> 다른 값들과 달리 **`Auth_*` 함수에서 `OnRep_`을 수동으로 다시 부르지 않는다.** `SetPlayerName`이 리슨 서버에서 알아서 불러주기 때문이다. (→ [Multiplayer.md](Multiplayer.md) "서버는 OnRep이 안 불린다")

#### 기본 닉네임은 게임모드가 매긴다 — 엔진 기본값은 `Player256`

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

#### 변경 요청 — 검증은 게임모드, 반영은 엔진 경로

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

### 준비 완료와 게임 시작 — 규칙은 `bIsReady`, 연출은 어빌리티

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

**연출을 서버에서 발동할 수 있는 근거**: 서버가 원격 플레이어의 LocalPredicted 어빌리티를 `TryActivate` 하면 GAS가 `ClientTryActivateAbility`로 소유 클라에 넘겨주고(엔진 `AbilitySystemComponent_Abilities.cpp:1648`), 호스트 자신은 로컬이라 즉시 실행된다. 몽타주는 어차피 GameplayCue로 나가므로(→ [Montage.md](Montage.md)) 시뮬 프록시까지 보인다. **서버 한 곳에서 부르면 전원 화면에 나온다.**

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

**구독하는 쪽은 구독 직후 현재 값을 한 번 읽는다.** 델리게이트는 "바뀔 때"만 오므로, 위젯이 늦게 생기면 이미 지나간 변경을 놓친다 (`IsReady()`로 즉시 초기 상태를 맞출 것). 로비 전반에서 쓰는 "구독 또는 즉시" 패턴과 같다 → [SystemReady.md](SystemReady.md)

**`bIsReady`는 `CopyProperties`에 넣지 않는다.** 로비 안에서만 의미가 있고, 게임플레이의 새 PlayerState는 `false`로 시작하는 것이 맞다 (위 "PlayerState 이관" 참고).

**PlayerState와 GameState는 도착 순서가 보장되지 않는다.** 그래서 `OnRep_IsReady`도 게임 스테이트의 `NotifyReadyStateChanged()`를 불러 현재 값으로 다시 방송한다. 위젯은 매번 현재 상태에서 다시 계산하므로 중복 방송이 무해하다.

#### 이동 전에 로비 위젯을 반드시 스택에서 뺀다

**위젯을 남긴 채 travel하면 새 레벨에서 캐릭터가 움직이지 않는다.** 수명이 서로 다른 두 가지가 엇갈려서 생기는 증상이다.

| | travel 시 |
|---|---|
| HUD·위젯 스택 | **파괴** (맵 소속) |
| `ULocalPlayer` + `EnhancedInputLocalPlayerSubsystem` | **생존** |

로비 위젯은 `Should Remove Gameplay IMCs = true`라 **떠 있는 동안 게임플레이 IMC가 걷혀 있고**, 팩은 그 복구를 **스택 상태가 변할 때** 수행한다(→ [EasyGameUI.md](EasyGameUI.md)). 그런데 위젯을 빼지 않고 맵을 넘기면 스택은 재계산 없이 HUD째로 사라지고, **걷어낸 IMC를 되돌릴 기회가 사라진다.** IMC를 실제로 들고 있던 서브시스템은 `ULocalPlayer` 소속이라 travel을 넘어 살아남으므로, 빠진 상태 그대로 새 레벨에 도착한다.

**해법은 이동 직전에 신호를 한 번 방송하는 것이다.**

```
ACBLobbyGameMode::Auth_TravelToGameplayLevel()
 ├ ACBLobbyGameState::Multicast_NotifyMatchStarting()   ← 서버 자신 + 전 클라이언트
 │    └ 각 인스턴스에서 OnMatchStarting 방송 → 위젯이 스스로를 스택에서 제거
 └ World->ServerTravel(...)
```

- **호스트/클라이언트를 분기하지 않는다.** Multicast RPC는 **서버 자신에게서도 실행**되므로, 호스트도 신호를 받는 쪽 중 하나가 된다. 위젯 제거는 어디서나 로컬 작업이다
- **GameMode가 아니라 GameState가 방송한다.** GameMode는 클라이언트에 존재하지 않아 Multicast의 주체가 될 수 없다 (위 "클래스별 존재 범위")
- **서버는 "언제인지"만 알린다.** 실제 제거는 각자의 HUD 스택에서 일어난다 — 로비 UI 표시를 BP에 맡긴 것과 같은 분업이다

#### 시작 버튼은 UI가 아니라 서버가 판정한다

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

### 로비 카메라 — 레벨에 있으면 켜지고 없으면 꺼진다

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
| **SystemReady를 기다리지 않는다** | 카메라 전환은 ASC·로드아웃과 무관하다. 두 타이밍 문제를 섞지 말 것 → [SystemReady.md](SystemReady.md) |

**배치를 빠뜨리면 에러 없이 3인칭 그대로다.** 2개 이상 놓으면 경고 로그를 남긴다(대기실 카메라는 1개 전제).

#### 왜 `SetViewTarget`을 직접 부르지 않나

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

#### 화면은 뷰 타겟 확정 + 캐릭터 준비까지 검게 덮는다

빙의 전에는 뷰 타겟이 PlayerController 자신이라 스폰 지점에서 보는 화면이 잠깐 나온다. 그리고 빙의 직후에도 **의상 등 비동기 에셋이 아직 안 붙어 있다**(→ [SystemReady.md](SystemReady.md), [Cosmetic.md](Cosmetic.md)). 그래서 두 조건이 모두 충족될 때까지 화면을 덮는다.

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
| **모든 레벨에 적용** | 분기가 없어 단순하고, 리스폰·맵 전환에도 같은 연출이 따라온다. 메인 메뉴 폰이 이미 같은 방식으로 페이드 인한다 → [EasyGameUI.md](EasyGameUI.md) |

**걷기는 자체 플래그로 1회만 수행한다**(`bReadyStateApplied`). `PlayerCameraManager::FadeAmount`는 `DoUpdateCamera`에서만 갱신되므로 **프레임 안에서는 신뢰할 수 없고**, `AutoManageActiveCameraTarget`이 여러 번 불리는 만큼 `StartCameraFade`가 재호출되어 **화면이 알파 1(완전 검정)에서 다시 시작**한다. 실측에서 클라이언트가 3회 재시작했다.

구독은 핸들이 유효한지로 중복을 막는다.

> 뷰 타겟을 즉시 컷으로 바꿀 때는 `PlayerCameraManager::SetGameCameraCutThisFrame()`을 함께 부른다. 알리지 않으면 렌더러가 **직전 프레임의 오클루전 쿼리·모션 벡터를 재사용**한다.

#### 이 페이드는 로딩 화면이 아니다 — 덮을 수 있는 구간의 한계

레벨을 직접 열면 **화면을 덮기 전에 이미 몇 프레임이 그려진다**(스폰 지점의 눈높이 뷰 = 대개 벽). PlayerController 쪽 어느 훅으로도 잡히지 않는다 — 그 시점엔 PC나 카메라 매니저가 아직 없기 때문이다.

로딩을 덮는 수단은 계층이 셋이고, 서로 대체할 수 없다.

| 계층 | 덮는 구간 | 수단 |
|---|---|---|
| ① | **`LoadMap` 자체** — 게임 스레드가 블로킹되어 월드 기반 UI가 못 돈다 | **MoviePlayer** (별도 슬레이트 스레드). 이것만 가능 |
| ② | **seamless travel** 중 | **`Transition Map`** |
| ③ | 맵 로드 후 **시스템 초기화**(비동기 로드·복제 대기) | **이 페이드** |

**우리가 만든 것은 ③이고 그 범위 안에서는 완전히 통제된다.** 남는 벽은 ①의 미구현이 드러난 것이며, 팩의 `Loading` 레이어도 맵 전환을 가로지르지 못하므로 대안이 되지 않는다 → [EasyGameUI.md](EasyGameUI.md)

**메인 메뉴 ↔ 로비 ↔ 게임플레이 전환을 붙일 때 ①·②를 함께 설계한다.** 맵을 넘어 사는 계층(`UCBGameInstance` 또는 그 서브시스템)이 그 자리다.

#### `AutoManageActiveCameraTarget`은 빙의 한 번에 5~7회 불린다

실측값이다. 두 가지가 겹친다:

1. **빙의 전에 관전 단계를 거친다.** `Suggested`가 `PlayerController 자신 → SpectatorPawn → 캐릭터` 순으로 바뀌며 매 전이마다 불린다
2. **빙의 하나에 진입점이 3~4개다.** `OnPossess`(서버) / `ClientRestart`(클라) / `APawn::PawnClientRestart` / `APawn::OnRep_Controller` — 복제 도착 순서가 보장되지 않아 어느 쪽이 먼저 와도 되도록 건 보험이다

**정상 동작이므로 이 함수는 멱등해야 한다.** 그래서 우리 오버라이드도 매번 같은 답을 내고, 반복 비용만 줄인다 — 로비 카메라는 **찾은 결과를 캐시**하고(못 찾은 건 캐시하지 않는다. 맵을 넘으면 영영 못 찾게 되므로), **이미 그 카메라를 보고 있으면 `SetViewTarget`을 생략**한다.

여기서 나오는 규칙 하나: **"캐릭터에 빙의했는가"를 화면을 열 조건으로 삼아야 한다.** 관전 폰 단계에서 열면 캐릭터가 없는 화면이 보인다.

> `ACameraActor`의 `Auto Activate for Player`를 쓰면 코드가 필요 없지만, 클라이언트 경로는 "이미 그 카메라를 보고 있을 때만" 유지하는 구조라 멀티플레이에서 신뢰할 수 없다.

배치 기준값 — 4인 라인업: `PlayerStart` 간격 250, 카메라 거리 850, 높이 130, FOV 60~65.

#### `ACameraActor`의 화면비 고정은 꺼야 한다

`ACameraActor`는 **생성자에서 화면비를 16:9로 고정한다**(`bConstrainAspectRatio = true` — 엔진 `CameraActor.cpp`). 창 비율이 다르면 남는 영역이 검게 칠해진다.

**그런데 UI는 그 검은 영역 위에도 그려진다.** UMG는 카메라의 화면비 제약을 모르고 언제나 뷰포트 전체에 그리므로, 위젯만 띠 위로 삐져나온다. 둘을 자동으로 연동시킬 수단은 없다.

**게임플레이에서는 이 문제가 없다** — 폰의 `UCameraComponent`는 기본값이 `false`다(`CameraComponent.cpp`). 로비만 화면 규격이 달라지는 셈이라 맞춰서 끈다.

| 대상 | 처리 |
|---|---|
| `ACBLobbyCamera` | **생성자에서 해제.** 배치할 때 체크박스로 하지 않는 이유는 새 카메라를 놓을 때 **누락되면 조용하기 때문**이다(게임모드 공통 기본값과 같은 판단) |
| 커스터마이징 카메라 | 런타임 스폰이므로 스폰 직후 해제 |

띠를 없애면 **창 비율에 따라 보이는 범위가 달라진다**(세로 FOV 유지 → 넓은 창은 좌우가 더 보임). 정상 동작이며, 그래서 로비 UI는 화면 가장자리보다 **중앙 기준 앵커**가 안전하다.

> 반대로 **고정 구도를 원한다면** 카메라를 그대로 두고 UI 전체를 `Scale Box`로 같은 비율에 가두는 길이 있다. 다만 모든 스택 위젯에 적용하거나 팩의 `WBP_EGUI_MainUIPanel`(모든 레이어가 얹히는 단일 캔버스)을 고쳐야 하고 → [EasyGameUI.md](EasyGameUI.md), 폰트 크기가 창 크기를 따라 변한다. **채택하지 않았다.**

> 클래스 기본값을 바꿔도 **이미 배치된 인스턴스가 그 값을 개별 저장하고 있으면 따라오지 않는다.** 빌드 후 디테일 패널에서 해당 항목에 되돌리기 화살표가 보이면 눌러 기본값으로 복귀시킬 것.

### 로비 UI 구성 — 화면 분할이 아니라 월드 투영

`Menu` 레이어에 위젯 하나를 올리고, 조작 UI가 **자기 캐릭터 머리 위를 따라간다.**

| 위젯 | 역할 |
|---|---|
| `WBP_CB_Lobby_Main` | 스택에 들어가는 **하나**. 선언 3함수 소유, 머리 위 배치 계산 |
| `WBP_CB_Lobby_PlayerMenu` | 머리 위 진입 버튼(의상·직업) — **로컬 플레이어 것 하나뿐** |
| `WBP_CB_CosmeticSelector` | 의상 버튼을 누르면 열리는 부위 4개 × 화살표 → [Cosmetic.md](Cosmetic.md) |
| `WBP_CB_Lobby_Footer` | 하단 준비완료·나가기 |

닉네임 표시는 이 스택 위젯들과 별개다. **발밑 이름표는 `UCBUIComponent`가 캐릭터에 붙이는 `UWidgetComponent`** 이고(버튼이 없어 스택에 올릴 이유가 없다), 로비에서만 생성된다 → [UI.md](UI.md) "로비 발밑 이름표"

머리 위 배치의 구현 레시피(투영·앵커·DPI)는 → [UI.md](UI.md)

**두 위젯이 서로 다른 배치 방식을 쓰는 데는 이유가 있다.**

| 위젯 | 어떤 뷰에서 뜨나 | 배치 |
|---|---|---|
| `PlayerMenu` | 공용 뷰 — **내 자리가 매번 다름** | 월드 투영 |
| `CosmeticSelector` | 클로즈업 — **구도가 항상 같음** | 고정 앵커 |

클로즈업 카메라를 폰 기준으로 배치하므로(아래) 진입한 순간의 화면 구도는 언제나 동일하다. 따라서 "캐릭터 옆"은 투영이 아니라 고정 좌표로 충분하다. 클로즈업에서는 캐릭터가 화면을 크게 차지해 **머리가 화면 밖으로 나갈 수 있으므로**, 진입 시 `PlayerMenu`는 명시적으로 숨긴다.

**남의 머리 위에는 아무것도 띄우지 않는다.** `Server_RequestCosmeticPart`는 **호출한 컨트롤러의 폰에만** 적용되므로 남의 버튼은 눌러도 의미가 성립하지 않는다. 준비 상태를 머리 위에 보여야 해지면 조작이 없는 이름표 위젯을 따로 만든다.

#### 슬롯 인덱스 복제가 필요 없는 이유

한때 **화면을 4등분해 각 칸에 플레이어를 배정**하는 안을 검토했다. 4명이 같은 깊이에 등간격으로 서 있어 화면에서도 등간격이므로 기하학적으로는 성립한다(Size Box 폭 = 간격 × 4를 그 깊이에서 투영한 폭).

깨지는 것은 **"누가 몇 번째 칸인가"** 다.

- `GameState->PlayerArray`는 도착 순이라 **서버와 클라이언트가 다를 수 있다**
- 3명만 접속하면 엔진 기본 `ChoosePlayerStart`가 **연속되지 않은 스타트**를 고를 수 있다
- 칸과 실제 위치가 어긋나면 **남의 캐릭터 위에 내 버튼이 뜬다**

그래서 4등분 방식은 `ACBLobbyGameMode`의 스타트 배정 + PlayerState 복제를 **전제로 요구한다.** 투영은 실제 위치를 매번 구하므로 그 전제가 통째로 사라지고, `PlayerStart` 간격·카메라 거리·FOV를 조정해도 위젯을 다시 튜닝할 필요가 없다.

> **슬롯 순서 고정 자체는 여전히 가치가 있다** — 재접속해도 자리가 안 바뀌는 것은 좋은 성질이다. 다만 그건 로비 경험의 문제이지 UI의 전제가 아니며, **나중에 도입해도 UI를 고치지 않는다.**

#### 커스터마이징 클로즈업 — 카메라는 하나, 위치는 폰에서 계산

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

## 사망과 리스폰 — 폰을 다시 스폰한다

플레이어가 죽으면 `RespawnDelay`(기본 5초) 뒤 **폰을 새로 스폰**한다. 시체를 되살리지 않는 이유는 무기 변경과 같다 — 재스폰 경로가 이미 있고, 그 경로가 로드아웃·무기·입력을 처음부터 다시 깔아주기 때문이다.

| 갈래 | 담당 |
|---|---|
| 사망 통지 | `ACBChaserCharacter::Auth_OnDeath` → 게임모드에 알리기만 함 |
| 지연·스폰 지점 규칙 | **`ACBGameplayGameMode`** (`RespawnDelay`, `Auth_HandlePlayerDeath`) |
| 사망 상태 정리 | `ACBGameplayGameMode::Auth_ClearDeathState` |
| 실제 재스폰 | `ACBGameplayGameMode::Auth_RespawnPlayer` → `RestartPlayer` |

### ASC가 PlayerState 소유라 청소가 필수다

플레이어의 ASC는 폰이 아니라 `ACBPlayerState`가 갖는다 (→ [ASC-Ownership.md](ASC-Ownership.md)). **폰을 갈아도 ASC는 그대로 살아남으므로**, 청소 없이 재스폰하면:

- `GE_Dead`가 남아 새 캐릭터가 태어나자마자 `Status.Dead` 상태가 되고, `UCBGameplayAbility::CanActivateAbility`가 모든 어빌리티를 막는다
- 이전 로드아웃이 부여한 어빌리티·이펙트가 새 폰의 부여분과 겹친다

그래서 스폰 **전에** 두 가지를 걷어낸다. 사망 GE는 클래스가 아니라 **부여 태그(`Status.Dead`)로 제거**한다 — 어떤 GE를 쓸지는 사망 어빌리티 BP가 정하므로 게임모드가 클래스를 알 수 없다. 로드아웃 부여분 회수는 로비의 캐릭터 변경과 같은 `UCBAbilitySystemComponent::Auth_ClearLoadoutGrants()`를 쓴다.

**체력은 따로 되돌리지 않는다.** 새 폰이 초기화되면서 로드아웃의 `StartupEffects`(`GE_Chaser_Sword_Init` 등, `CurrentHealth`/`MaxHealth`를 Override)를 다시 적용하므로 자동으로 원복된다.

### 타이머는 게임모드가 든다

죽은 폰에 타이머를 걸면 그 폰이 어떤 이유로든 먼저 파괴될 때 리스폰이 통째로 사라진다. 그래서 **게임모드가 컨트롤러별 타이머를 `TMap`으로 들고**, `Logout`에서 나간 플레이어의 예약을 취소한다. 대기 중 컨트롤러가 사라질 수 있으므로 타이머 델리게이트는 약참조로 잡는다.

`ACBChaserCharacter::DespawnDelay = 0` 은 그대로다. **시체는 스스로 사라지지 않고 리스폰 시점에 게임모드가 폰을 교체하면서 파괴된다** — 폰을 먼저 없애면 컨트롤러가 폰을 잃어 화면·입력이 끊기기 때문. 파괴 시 `Pawn::EndPlay`가 돌면서 무기 액터와 HUD 위젯도 함께 정리된다.

AI(Rogue/Outlaw)는 이 경로를 타지 않는다. 기존대로 `DespawnDelay` 뒤 스스로 파괴되고, 다음 소환은 스포너가 전멸을 보고 결정한다 (→ [Spawner.md](Spawner.md)).

### 아직 없는 것

- **관전 화면 없음** — 죽은 뒤 리스폰까지 카메라가 시체 위치에 그대로 있다. 관전 카메라나 사망 UI는 매치 규칙과 함께 설계한다.
- 스폰 지점은 엔진 기본 규칙(`ChoosePlayerStart`)에 맡긴다. 로비처럼 자리를 고정하지 않으므로, 적이 몰린 자리에 다시 나올 수 있다.

## 진행 상황

**완료**
- 메인 메뉴 레벨 + `GM_CB_MainMenu` (표시 확인) → [EasyGameUI.md](EasyGameUI.md)
- `ACBGameModeBase` 공통 기본값 (seamless travel, PC·PlayerState 클래스)
- `ACBLobbyGameMode` / `ACBGameplayGameMode` 생성 (빈 껍데기)
- `ACBPlayerState::CopyProperties` — 의상 조합 이관
- **`L_CB_Lobby` + `GM_CB_Lobby`** (Default Pawn = `BP_Chaser_Sword`, HUD = `BP_EasyMainGameHUD`), World Settings 오버라이드
- **로비 배경** — 속 빈 상자(스태틱 메시, `Use Complex Collision As Simple`) + 포인트 라이트, `PlayerStart` 배치
- **로비 고정 카메라** — `ACBLobbyCamera` + `ACBChaserController::AutoManageActiveCameraTarget` 오버라이드
- **레벨 진입 페이드 인** — 뷰 타겟 확정 + 캐릭터 준비 완료까지 검은 화면 (모든 레벨 공통)
- **로비 UI** — `Menu` 레이어 위젯 + 머리 위 투영 배치, 의상 교체 2인 검증 완료 (위 "로비 UI 구성")
- **호스트·참가(주소 직접 입력)** — `UCBSessionSubsystem` (접속 실패·맵 이동 실패 복귀 포함, 위 "호스트·참가")
- **메인 메뉴 호스트·참가 버튼 배선** (`WBP_CB_MainMenu`) — 2인 접속 확인 완료
- **로비 → 게임플레이 의상 이관** — `CopyProperties` + 준비 완료까지 기다렸다 적용 (2인 확인)
- **로비 준비 완료 → 게임 시작** — `bIsReady` 복제 + `ACBLobbyGameState` 집계 + 호스트 시작 요청 검증 후 `ServerTravel` (위 "준비 완료와 게임 시작")
- **세션 시스템(Null 제공자)** — 방 생성·검색·참가·나가기. OnlineServices(v2) API, 제공자는 ini 한 줄 (위 "세션"). **PIE 2창 검증 완료**
- **접속 실패 처리 분기** — `UCBOnlineSession` + `UCBGameInstance::GetOnlineSessionClass()` 로 엔진의 자동 기본 맵 복귀를 가로채, **참가 실패는 제자리 유지 / 접속 후 끊김은 메뉴 복귀** (위 "엔진의 자동 복귀"). **컴파일까지 확인, 실행 검증 남음**
- **로비 나가기** — `UCBSessionSubsystem::Local_LeaveToMainMenu()` (클라 `ClientTravel` / 호스트 `OpenLevel`, 위 "나가기"). **BP 배선·실행 검증 남음**
- **무기(캐릭터) 변경 C++ 경로** — 카탈로그·선택 값 복제·스폰 클래스 결정·같은 자리 재스폰·ASC 부여분 회수 (위 "무기 변경 = 캐릭터 변경"). **컴파일까지 확인, 에셋·위젯 배선과 실행 검증 남음**
- **사망 후 리스폰** — `ACBGameplayGameMode`가 컨트롤러별 타이머로 5초 뒤 `RestartPlayer`, 스폰 전에 사망 GE·로드아웃 부여분 회수 (위 "사망과 리스폰"). **컴파일까지 확인, 실행 검증 남음**
- **닉네임 C++ 경로** — 기본 닉네임 부여(`Player1`, `Player2` ...), 다듬기·중복 접미사, 변경 요청 RPC, 변경 신호 (위 "닉네임"). **컴파일까지 확인, 위젯 배선과 실행 검증 남음**

**미구현**
- **무기(캐릭터) 변경 UI 배선** — C++ 창구는 완료(위 "무기 변경 = 캐릭터 변경"). 남은 것: `UCBCharacterCatalog` 데이터 에셋 작성, 무기별 캐릭터 BP·로드아웃, `GI_EasyMainGameInstance`에 카탈로그 지정, 로비 위젯의 버튼 목록 + `Server_RequestCharacterSelection` 호출 + `OnCharacterSelectionChanged` 구독
- **EOS 제공자 전환** — 플러그인 활성화 + 자격증명 + Device ID 로그인 + `DefaultServices=Epic` / `bUseLANSessions=False`. 세션 호출 코드는 그대로. 추가로 `NetDriverEOS` 설정(P2P)과 호스트 주소를 EOS 키에도 넣는 작업이 붙는다
- 세션 목록 UI (`WBP_CB_SessionList`, `Menu` 레이어) + 메인 메뉴에 방 만들기/찾기 버튼 — C++ 창구는 완료
- 접속 실패 사유를 모달로 표시 — C++은 완료, **위젯 배선만 남음**. 참가 실패는 `OnConnectionFailed` 구독, 접속 후 끊김은 `Local_ConsumePendingFailureReason()` — 두 경로 다 물릴 것 (위 "엔진의 자동 복귀")
- 로비 위젯 배선 (`WBP_CB_Lobby_Footer`) — 준비/시작 위젯 스위처, `OnReadyStateChanged` 구독, 두 RPC 호출
- **닉네임 UI 배선** — C++ 창구는 완료(위 "닉네임"). 남은 것: 로비 위젯에 입력창(커밋 시 `Server_RequestSetNickname`) + 이름 표기(`GetPlayerName()` + `OnPlayerNicknameChanged` 구독, 구독 직후 현재 값 1회 읽기)
- 닉네임 로컬 저장 + 로비 입력창 프리필 후 `Server_RequestSetNickname` 1회 전송 — 매번 입력하지 않도록. 서버 수신부는 완료. **접속 URL `?Name=` 방식은 채택하지 않는다** (위 "닉네임")
- 게임플레이 레벨의 게임모드 BP (`ACBGameplayGameMode` 파생) + 로비 게임모드의 `GameplayLevel` 지정 — **없으면 시작해도 이동할 곳이 없다**
- 나가기 BP 배선 (`WBP_CB_Lobby_Footer` / `IA_CB_Leave`) — C++ 창구는 완료. **`Remove This Widget` → `Local_LeaveToMainMenu()` 순서**를 지킬 것 (위 "위젯을 먼저 빼고 부른다")
- 나가기 사유 구분 — 호스트가 나갔을 때 남은 클라이언트에 "호스트가 종료했습니다"를 표시할지. **모달 배선 뒤에 판단한다** — 지금은 아무 메시지도 안 보이고, 이동 직전 멀티캐스트는 월드 teardown과 경합해 도착이 보장되지 않는다. 붙인다면 사유를 미리 저장해두고 실패 시 우선 사용하는 형태
- 게임플레이 레벨의 나가기(일시정지 메뉴) — C++은 레벨을 가리지 않으므로 배선만 남음. 다만 팩 일시정지 메뉴에 **자체 메인 메뉴 복귀 경로**가 있어 둘 중 무엇이 이길지 정해야 하고, 호스트가 나가는 건 곧 매치 종료라 `ACBGameplayGameMode` 규칙과 함께 설계한다
- 게임플레이 매치 규칙 (승패 판정 등) — **사망 후 리스폰은 구현됨** (위 "사망과 리스폰")
- Global Config → `New Game Level`을 `L_CB_Lobby`로 지정
- **로딩 시스템 (①·② 계층)** — MoviePlayer + `Transition Map`. **맵 전환(메인 메뉴 → 로비 → 게임플레이)을 붙일 때 한 덩어리로 설계한다.** 지금 레벨을 직접 열면 화면을 덮기 전 몇 프레임이 노출되는데, 그 원인이자 해법이 여기다 (위 "이 페이드는 로딩 화면이 아니다")

**확인됨**
- `GM_CB_GameModeBase`(구 `BP_GameModeBase` — 리네임됨)에 `Player Controller Class` / `Use Seamless Travel` 오버라이드 없음. C++ 베이스 기본값이 그대로 적용된다

## 관련 문서

- 서버 권위·접두사·복제 정책: [Multiplayer.md](Multiplayer.md)
- 시스템 UI·메인 메뉴 구현: [EasyGameUI.md](EasyGameUI.md)
- HUD·캐릭터 UI의 로컬 원칙: [UI.md](UI.md)
- 캐릭터 에셋 주입: [Loadout.md](Loadout.md)
