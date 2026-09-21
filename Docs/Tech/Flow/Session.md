# 접속과 세션 (`UCBSessionSubsystem`)

> 메인 메뉴에서 로비까지 **어떻게 붙는가**. 방을 만들고 찾아 들어가고, 실패하면 어디로 돌아가고, 어떻게 나가는가. 접속에 관한 것은 전부 `UCBSessionSubsystem` 하나에 모인다.
>
> 레벨·게임모드 구조와 맵을 넘는 데이터는 [GameFlow.md](GameFlow.md), 로비 안에서 일어나는 일은 [Lobby.md](Lobby.md). 서버 권위·접두사 같은 **일반 규칙**은 [Multiplayer.md](../Conventions/Multiplayer.md).

## 호스트·참가 — `UCBSessionSubsystem`

메인 메뉴의 버튼 2개가 접속의 전부다. **아직 세션 개념이 없고 주소로 직접 붙는다** — EOS를 넣기 전까지의 형태다.

| 버튼 | 호출 | 실제 동작 |
|---|---|---|
| 호스트 | `Local_HostLobby(로비 레벨)` | `OpenLevelBySoftObjectPtr(..., Options="listen")` |
| 참가 | `Local_JoinServerByAddress("127.0.0.1")` | 로컬 PC의 `ClientTravel(TRAVEL_Absolute)` |

- **`listen` 옵션이 빠지면 조용히 단독 실행이 된다.** 에러 없이 아무도 접속하지 못하는 상태가 되므로 호스트 경로에서 항상 확인할 것
- **주소는 `IP` 또는 `IP:포트`.** 비워서 호출하면 `127.0.0.1`을 쓴다(같은 PC 2인 테스트용 기본값)
- **로비 레벨은 인자로 받는다.** 그 값은 이미 팩 Global Config의 `New Game Level`에 있고, 레벨 에셋 등록소는 Global Config로 정해져 있다(→ [EasyGameUI.md](../Presentation/EasyGameUI.md)). C++이 경로를 또 들고 있으면 등록소가 둘이 된다

### 왜 서브시스템인가 — 접속에 관한 것은 전부 여기 모은다

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

### EOS로 갈 때 무엇이 바뀌나

바뀌는 것은 **접속 앞단뿐이다.** EOS도 세션을 찾은 뒤 `GetResolvedConnectString()`으로 주소를 얻어 결국 위 두 함수를 부른다 — 지금 만든 travel 경로가 종착점이라 그대로 남는다. 추가되는 것은 로그인(Auth/Connect), 세션 생성·검색·참가, 그리고 그 실패 콜백이며 전부 `UCBSessionSubsystem` 안이다.

**지금 구조로 못 하는 것은 NAT 통과 하나다.** 같은 PC·같은 LAN은 주소 직접 입력으로 충분하고, 외부 접속은 포트포워딩 없이는 안 된다. 그게 EOS를 넣는 실질적 이유다.

## 세션 — 방을 만들고 찾아 들어간다

**세션은 "붙을 주소를 알아내는 일"만 한다.** 실제 접속은 위 두 함수(`Local_HostLobby` / `Local_JoinServerByAddress`)로 그대로 수렴한다.

```
[메인 메뉴] LAN/EOS 버튼 → UCBAuthSubsystem::RequestLogin(모드)
          └ 로그인 성공 → 계정 ID 확보 (아래 "로그인")

[호스트] Local_CreateAndHostSession(로비 레벨, 방 이름, 최대 인원)
          └ CreateSession 성공 → Local_HostLobby(로비 레벨)

[참가]   Local_FindSessions()
          └ 완료 → OnSessionSearchCompleted 방송 → 목록 위젯 갱신
         Local_JoinFoundSession(인덱스)
          └ JoinSession 성공 → 세션 속성에서 호스트 주소 → Local_JoinServerByAddress(주소)
```

### 제공자는 런타임에 고른다 — 설정이 아니라 버튼이다

**OnlineServices(v2) API로 작성한다.** 실제 제공자는 **메인 메뉴에서 플레이어가 고른다.**

`GetServices()`는 제공자를 매개변수로 받고, 구체적인 값을 넘기면 ini를 보지 않는다(`OnlineServicesRegistry.cpp`의 `ResolveServiceName`은 `Default`·`Platform`만 해석한다). 인스턴스는 제공자별로 따로 캐시되므로 Null과 Epic이 동시에 존재해도 서로 간섭하지 않는다.

```
ECBOnlineMode::LAN → EOnlineServices::Null     (LAN 비콘)
ECBOnlineMode::EOS → EOnlineServices::Epic     (EOS)
```

- **LAN 여부를 따로 설정하지 않는다.** `UCBSessionSubsystem::Local_IsLANMode()`가 `UCBAuthSubsystem::IsLANMode()`에서 파생되므로, **제공자와 LAN 플래그가 어긋날 수 없다.** 예전에는 ini 두 곳을 짝 맞춰야 했고 하나만 바꾸면 조용히 깨졌다 — `Local_ResetOnlineServices()`가 LAN 조건에서 `DestroyService()`를 부르는 탓에 "Epic + LAN=True" 조합은 EOS 서비스를 쓰기 직전에 스스로 파괴했다
- **`DestroyService`·`IsLoaded`에 `EOnlineServices::Default`를 넘기지 않는다.** 런타임 선택과 ini 값이 다를 수 있어, 쓰지도 않는 제공자를 파괴하게 된다
- **개발 중에는 LAN이 필수다.** Device ID는 기기당 하나라 한 PC의 2인 PIE가 EOS로는 불가능하다(같은 ProductUserId → 자기 자신에게 접속). LAN 모드는 계정이 필요 없어 그대로 된다

남은 ini 설정은 EOS 쪽뿐이다.

```ini
[OnlineServices]
DefaultServices=Epic          ; 코드가 제공자를 직접 넘기므로 세션 경로엔 쓰이지 않음(엔진 기본값용)
bUseBuildIdOverride=True      ; 세션 버킷을 가르는 빌드 식별자
BuildIdOverride=1

[EOSSDK]
DefaultPlatformConfigName=ChainBurst   ; 자격증명 섹션 이름 (값은 Config/Windows/WindowsEngine.ini, 저장소 제외)
```

- **플랫폼 설정 이름은 `DefaultPlatformConfigName`이다.** OSSv1의 `DefaultArtifactName`은 `OnlineSubsystemEOS`만 읽으므로 OSSv2에서는 아무 효과가 없다. 못 찾았을 때의 실패 메시지가 `Verbose`라 **기본 로그 레벨에서는 보이지도 않는다**
- **`BuildIdOverride`를 지정하지 않으면 세션이 서로 보이지 않을 수 있다.** `SessionsEOSGS::CreateSession`은 버킷 커스텀 세팅이 없으면 BuildId를 버킷 ID로 쓰는데, 기본값(`GetNetworkCompatibleChangelist()`)은 에디터와 패키징 빌드에서 달라진다. 네트워크 호환성이 깨지는 변경을 하면 숫자를 올린다
- **v1(`OnlineSubsystem`)이 아니라 v2를 쓴 이유**: EOS의 Device ID 익명 로그인이 v2에만 있고(v1 `ToEOS_ELoginCredentialType`에 항목 자체가 없다), v2를 골라도 `OnlineServicesOSSAdapter`로 Steam이 열려 있다. v2 EOS의 세션 구현도 생성·검색·참가·나가기에 빈 구현이 없음을 확인했다(미구현은 presence 2개뿐)

### 로그인 — 세션보다 먼저, 그리고 세션은 그걸 모른다

EOS는 **로그인해서 계정 ID를 얻어야** 세션을 만들 수 있다. Null은 로그인 개념이 아예 없다. 그 차이를 세션 코드에 들이지 않으려고 **`UCBAuthSubsystem`** 을 따로 뒀다.

```
[메인 메뉴 버튼] RequestLogin(모드)
 ├ 진행 중이면 무시 / 같은 모드로 로그인돼 있으면 무시
 ├ 모드가 바뀌었으면 Local_ResetLogin() 으로 비우고 새로
 │
 ├ LAN → Local_AdoptLocalAccount()
 │        └ GetLocalOnlineUserByPlatformUserId 로 자동 등록된 계정 조회
 │
 └ EOS → ResolveServices()          ← 먼저! 온라인 서비스가 EOS 플랫폼을 만들게 한다
         Local_CreateDeviceId()     ← EOS SDK 직접 호출
          └ Success 또는 DuplicateNotAllowed → 둘 다 정상
         Local_LoginWithDeviceId()  ← IAuth::Login (ExternalAuth / DeviceIdAccessToken)
          └ 계정 ID 보관 → OnLoginStateChanged 방송
```

- **Null 에서는 `Login()` 을 부르면 안 된다.** `FAuthNull` 에 구현이 없어 베이스의 `NotImplemented` 로 실패한다(`AuthCommon.cpp`). 대신 플랫폼 유저가 생길 때 계정이 자동 등록되므로 레지스트리에서 꺼내 쓴다
- **모드는 메인 메뉴에서 언제든 바꿀 수 있다.** 메뉴로 돌아오는 경로(`Local_LeaveToMainMenu`)가 이미 세션을 정리하므로, 전환 시점에는 정리할 세션이 없다
- **로그인 진행 중에는 모드 전환 요청을 무시한다.** EOS 로그인은 SDK 콜백이 비동기로 오는데, 그 사이에 상태를 갈아엎으면 뒤늦게 도착한 콜백이 새 상태를 덮어쓴다. UI 는 `OnLoginStateChanged` 로 버튼을 잠그면 된다

- **`UCBSessionSubsystem`은 제공자를 모른다.** 계정 ID는 `GetLocalAccountId()`로, 서비스는 `ResolveServices()`로 받아 쓸 뿐이다. **프로젝트에서 EOS 전용 코드는 `CBAuthSubsystem.cpp` 하나뿐이다**
- **서브시스템 `Initialize()` 에서 로그인하면 안 된다.** 로그인에 `PlatformUserId` 가 필요하고 그건 로컬 플레이어에게서 나오는데, `Initialize()` 는 `SetupInitialLocalPlayer()` 보다 이르다. 메인 메뉴 위젯이 호출하는 지금 구조에서는 이미 로컬 플레이어가 있으므로 문제가 없다
- **`ResolveServices()`를 Device ID 생성보다 먼저 부르는 이유.** SDK 매니저는 `(설정 이름 × 인스턴스 이름)` 조합으로 플랫폼 핸들을 캐시한다. 온라인 서비스가 먼저 플랫폼을 만들게 해야 같은 핸들을 얻는다. 순서가 뒤집히면 **플랫폼이 둘 생겨서 "로그인은 됐는데 세션이 계정을 못 찾는"** 상태가 된다
- **Device ID 생성만 EOS SDK 직접 호출이다.** `EOS_Connect_CreateDeviceId`를 감싸는 OSSv2 경로가 없다(엔진 전체 검색 0건). 로그인 자체는 `IAuth::Login`으로 한다
- **SDK 콜백에 `this`를 그대로 넘기지 않는다.** EOS 콜백은 `void*` 하나만 나르므로, 콜백 전에 서브시스템이 사라지면(PIE 정지) 댕글링이 된다. 약참조를 힙에 담아 넘기고 콜백이 회수한다
- **Device ID는 기기당 하나다.** 한 PC에서 클라이언트를 여럿 띄우면 같은 계정으로 로그인될 수 있다

> 데브 포털 참고: 세션 기능은 포털에서 **"매치메이킹"** 이라고 부른다. 클라이언트 정책의 **"Connect"** 토글은 로그인이 아니라 *다른 유저의 계정 매핑 조회*(둘 다 신뢰할 수 있는 서버 전용)라 **꺼져 있어도 Device ID 로그인은 된다**. 리슨 서버 + P2P 구조에는 사전 정의 정책 `Peer2Peer`가 맞는다.

### 호스트 주소는 우리가 정한 속성 하나로 주고받는다

제공자마다 주소를 얻는 방법이 다르다 — Null은 `FSessionLAN::OwnerInternetAddr`, EOS는 `EOSGS_HOST_ADDRESS_ATTRIBUTE_KEY` 속성이다. **그 차이를 코드에 들이지 않으려고 호스트가 직접 자기 주소를 세션 속성(`CB_HostAddress`)에 실어 보낸다.**

```
호스트(LAN): ISocketSubsystem::GetLocalHostAddr()      → CustomSettings["CB_HostAddress"]
호스트(EOS): UCBAuthSubsystem::GetLocalEOSAddress()    → CustomSettings["CB_HostAddress"]
참가       : GetSessionByName → CustomSettings["CB_HostAddress"] → Local_JoinServerByAddress()
```

**조회 코드가 제공자와 무관해진다.** 호스트가 넣는 값만 갈리고 참가 측은 그대로다 — 주소 형식이 곧 전송 방식을 정하므로(아래 "P2P") 참가 측은 무엇이 실려 있든 그대로 travel 하면 된다.

> 포트는 싣지 않는다. 참가 측이 기본 포트로 접속한다 — 호스트가 기본 포트를 쓰지 않게 되면 여기에 포트를 함께 실어야 한다.

### P2P — 리슨 서버는 그대로, 전송만 EOS로 바꾼다

**리슨 서버(토폴로지)와 P2P(전송)는 다른 축이다.** 서버 권위·복제 코드는 한 줄도 바뀌지 않고, 패킷이 오가는 경로만 갈린다. 직접 IP로는 호스트가 NAT 뒤에 있으면 외부에서 붙을 수 없고, 포트포워딩을 일반 플레이어에게 요구할 수 없다 — 그게 P2P를 쓰는 이유다.

```ini
[/Script/Engine.Engine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/SocketSubsystemEOS.NetDriverEOS",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")
+NetDriverDefinitions=(DefName="BeaconNetDriver",...)   ; ClearArray 로 날아가므로 함께 복원
+NetDriverDefinitions=(DefName="DemoNetDriver",...)

[/Script/SocketSubsystemEOS.NetDriverEOS]
NetConnectionClassName="/Script/SocketSubsystemEOS.NetConnectionEOS"
```

- **클래스 경로 주의.** 인터넷 자료 대부분이 쓰는 `/Script/OnlineSubsystemEOS.NetDriverEOS`는 옛 경로다. 클래스가 `SocketSubsystemEOS`로 옮겨졌고 `BaseEngine.ini`의 `ClassRedirects`가 옛 경로를 넘겨준다. OSSv2에서는 `OnlineSubsystemEOS`를 켜지도 않으므로 새 경로로 적는다
- **섹션은 `[/Script/Engine.Engine]`이다.** `[/Script/Engine.GameEngine]`이 아니다 — 엔진 기본값이 `Engine` 쪽에 있고, 에디터 엔진에도 적용돼야 PIE에서 검증할 수 있다
- **`bIsUsingP2PSockets`는 5.6에서 폐기됐다.** 튜토리얼이 넣으라고 해도 넣지 않는다

#### 하나의 빌드로 LAN과 EOS를 모두 태운다

`UNetDriverEOS`는 `UIpNetDriver`를 상속하고, 다음 조건이면 스스로 기존 IP 경로로 넘어간다(passthrough).

| 쪽 | passthrough 조건 |
|---|---|
| 리슨 서버 | URL에 `bIsLanMatch` 또는 `bUseIPSockets`가 있을 때 |
| 클라이언트 | 접속 주소가 `EOS:`로 시작하지 않을 때 |

클라이언트는 주소만 보고 알아서 갈리지만, **리슨 서버는 URL 옵션이 없으면 EOS 모드로 들어가 로그인된 P2P 주소를 요구한다.** 그래서 LAN 경로에서는 `Local_HostLobby()`가 `listen?bIsLanMatch`를 붙인다 — 이걸 빼면 LAN에서 방 만들기 자체가 실패한다.

#### 주소에 대괄호가 필수다

EOS 주소는 `[EOS:<ProductUserId>]` 형식이며 **대괄호가 없으면 동작하지 않는다.**

`FURL` 파서(`URL.cpp`)가 콜론을 보고 `EOS`를 **프로토콜로 잘라내기** 때문이다 — ProductUserId는 점이 없는 순수 16진수라 프로토콜 판정 조건이 전부 성립한다. 그러면 `ConnectURL.Host`가 비어 `NetDriverEOS::InitConnect`의 `Host.StartsWith("EOS")` 검사에 걸리지 않고 **조용히 passthrough로 빠진다.**

대괄호를 씌우면 FURL이 IPv6 리터럴로 인식해 프로토콜 파싱을 건너뛰고, 괄호만 벗겨 `Host = "EOS:<PUID>"`를 정확히 만든다. 엔진 자신의 OSSv1 경로(`FOnlineSessionEOS::GetConnectStringFromSessionInfo`)도 같은 형식을 쓴다.

ProductUserId 문자열은 `UE::Online::ToString(FAccountId)`로 얻는다 — `CoreOnline`에 있어 **EOS 모듈에 의존하지 않는다.** 형식 지식은 `UCBAuthSubsystem::GetLocalEOSAddress()` 한 곳에 둔다.

#### 검증 로그

| 로그 | 의미 |
|---|---|
| `LogEOSP2P: A new connection request listener has been bound. SocketId=[GameNetDriver]` | 리슨 서버가 EOS 모드로 진입 |
| `LogNet: NetConnectionEOS_N` | `NetConnectionClassName` 적용됨 (`IpConnection`이면 미적용) |
| `LogEOSP2P: Connection established. NetworkType=[EOS_NCT_DirectConnection]` | P2P 소켓 연결 |
| `LogNet: IpNetDriver listening on port 7777` | **나오면 안 됨** — EOS 모드가 아니라는 뜻 |

#### 한 PC에서는 접속까지 검증할 수 없다

**Device ID는 기기당 하나라 같은 PC의 두 인스턴스는 같은 ProductUserId를 받는다.** P2P는 PUID가 곧 주소이므로 자기 자신에게 접속하는 꼴이 되고, EOS P2P 소켓은 맺어지지만 언리얼 핸드셰이크(Challenge 교환)가 진행되지 않는다. 세션 생성·검색·참가와 NetDriver 진입까지는 확인할 수 있고, **실제 접속 검증은 PC 2대가 필요하다.**

반복 테스트 중 참가가 `EOS_Sessions_SessionAlreadyExists`로 막히는 것도 같은 원인이다(이전 실행의 멤버십이 백엔드에 남음). 에디터를 재시작하면 풀린다.

### 세션 속성 목록

호스트가 실어 보내고 검색 측이 읽는 값 전부. 모두 `ESchemaAttributeVisibility::Public` 이어야 광고된다 — `NboSerializerCommonSvc`가 Public 이 아닌 항목은 LAN 패킷에서 아예 뺀다.

| 키 | 타입 | 쓰임 | 갱신 시점 |
|---|---|---|---|
| `EOSGS_BUCKET_ID_ATTRIBUTE_KEY` | String | EOS 버킷 + 검색 필터 (EOS 전용, 아래) | 생성 시 1회 |
| `CB_HostAddress` | String | 참가 측 접속 주소 | 생성 시 1회 |
| `CB_DisplayName` | String | 목록에 보일 방 이름 | 생성 시 1회 |
| `CB_CurrentPlayers` | Int64 | 목록에 보일 현재 인원 | 로비 인원 변화마다 (아래) |

### EOS 검색에는 버킷 필터가 필수다 — 없으면 조건이 비어 거부당한다

**`EOS_SessionSearch_Find`는 조건이 하나도 없는 검색을 `EOS_InvalidParameters`로 거부한다.** `MaxResults`만 넣은 검색은 EOS 입장에서 빈 쿼리다.

```
LogEOSSDK: Error: LogEOSSessions: Search query is empty, search invalid.
LogOnlineServices: Warning: EOS_SessionSearch_Find failed with result [EOS_InvalidParameters]
```

`SessionsEOSGS::WriteSessionSearchHandle`가 검색에 쓰는 것은 `Filters` / `SessionId` / `TargetUser` 셋뿐이고, **버킷을 자동으로 채워주지 않는다.**

**생성 쪽만 자동 처리가 있다는 게 함정이다.** `CreateSession`은 버킷 커스텀 세팅이 없으면 `GetBuildUniqueId()`를 버킷으로 쓴다 — 그래서 **방 만들기는 되고 검색만 실패한다.** 게다가 그 자동 버킷은 `CustomSettings`에 없으므로 **검색 가능한 어트리뷰트로 써지지도 않아**, 자동 버킷에 의존하면 매칭할 방법 자체가 없다.

그래서 **버킷을 명시적으로 넣는다.** 커스텀 세팅에 넣으면 두 가지가 동시에 일어난다:

- `CreateSessionModificationOptions.BucketId`가 된다 (EOS 서버 측 파티션)
- `WriteCreateSessionModificationHandle`의 루프가 모든 `CustomSettings`를 `AddAttribute`로 쓰므로 **검색 가능한 어트리뷰트도 된다**

값은 `ChainBurst_<BuildId>`다. `[OnlineServices] BuildIdOverride`가 그 숫자를 정하므로, 네트워크 호환성이 깨지는 변경 후 숫자를 올리면 이전 빌드의 방과 자동으로 갈라진다.

- **키 이름은 엔진 상수 `EOSGS_BUCKET_ID_ATTRIBUTE_KEY`(`SessionsEOSGSTypes.h:20`)와 문자열이 같아야 한다.** 그 헤더를 include 하면 세션 서브시스템이 EOS를 알게 되므로(제공자 독립 원칙 위반) 문자열만 맞춰 두었다 — 엔진이 값을 바꾸면 드리프트하므로 주석에 출처를 남겨 둔다
- **생성과 검색 모두 EOS 모드일 때만 적용한다.** LAN은 비콘으로 찾으므로 버킷이 필요 없고, 검증된 경로를 건드리지 않기 위해서다

### 현재 인원도 우리가 실어 보내야 한다 — 엔진 값은 못 쓴다

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

### 매치가 시작되면 세션을 닫는다

게임플레이 레벨로 travel하면 `ACBLobbyGameMode`가 사라져 인원 광고도 멈춘다. **난입을 받지 않으므로 이동 직전에 세션을 닫는다** — `Auth_TravelToGameplayLevel()`이 `Auth_SetSessionAcceptingPlayers(false)`로 `bAllowNewMembers`를 내린다. 목록에 참가 불가로 보이므로 인원 값이 로비 시점에 굳어도 오해가 없다.

> 나중에 난입을 열게 되면 이 호출을 빼고 `ACBGameplayGameMode`에서도 같은 인원 갱신을 이어가야 한다.

### PIE는 월드 컨텍스트로 서비스 인스턴스를 구분한다

`GetServices(EOnlineServices::Default, InstanceName)`의 두 번째 인자에 **월드 컨텍스트 핸들**을 넘긴다. 한 프로세스에 게임 인스턴스가 여럿인 PIE에서 이걸 빠뜨리면 **두 인스턴스가 같은 서비스를 공유해 서로의 세션이 섞인다.**

### 계정 ID — 로그인이 들어올 자리

모든 세션 작업이 `FAccountId`를 요구한다. **Null 제공자는 시작 시 계정을 자동 생성**하므로(`FAuthNull::InitializeUsers`) 로그인 없이 바로 얻어진다. EOS는 로그인이 끝나야 유효해진다.

그래서 조회를 `Local_ResolveLocalAccountId()` 하나로 감쌌다 — **로그인 단계는 이 함수 앞에 붙기만 하면 되고 나머지 구조는 바뀌지 않는다.**

> **같은 PC 2인 테스트는 Null이 오히려 유리하다.** `FAuthNull::GenerateRandomUserId`가 **에디터이거나 첫 인스턴스가 아니면 랜덤 계정 ID**를 만들어, 두 인스턴스가 서로 다른 사용자가 된다. 반대로 EOS의 Device ID는 캐시 디렉터리가 사용자 폴더 하나라(`FEOSSDKManager::GetCacheDirBase`) **두 인스턴스가 같은 계정이 된다** — EOS 도입 후 로컬 2인 테스트는 Dev Auth Tool이 필요하다.

### 접속 거부 — 실패 종류와 PIE 가드

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

### 엔진의 자동 복귀 — 접속 시도 실패에서만 끊는다

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

### 정원 판정은 세션이 아니라 서버가 한다

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

### 참가 직전 재확인은 "새로 조회"가 아니다

`Local_JoinFoundSession()`은 참가 전에 `GetSessionById()`로 방 상태를 다시 읽고 `bIsJoinable`이 false면 멈춘다. **다만 이건 새 조회가 아니다** — `FSessionsCommon::GetSessionById()`는 `AllSessionsById` 맵을 뒤지는 **동기 로컬 조회**이고, 그 맵은 `AddSearchResult()`가 **검색할 때만** 채운다. 즉 마지막 검색 시점의 스냅샷을 다시 읽는 것이라, **검색 이후 시작된 방은 이 가드로 걸러지지 않는다.**

그래도 두는 이유:

- 목록에서 사라졌거나 **LAN 세션 ID가 만료된** 방을 잡아 "새로고침해 주세요" 안내로 돌릴 수 있다
- 인덱스가 어긋나 엉뚱한 방에 붙는 사고를 막는다
- 재확인한 값으로 `FoundSessions[Index]`를 갱신하므로 위젯이 새로고침 없이 최신 상태를 볼 수 있다

**낡은 목록으로 진행 중인 매치에 붙는 것은 서버가 막는다** (바로 아래). 세션을 닫은 건 광고를 닫은 것이지 커넥션을 막은 게 아니다.

> 검색 결과 항목을 만드는 로직은 `Local_BuildSearchEntry()` 하나에 모여 있다. 목록 표시와 참가 가드가 **같은 기준**을 쓰게 하려는 것으로, 한쪽만 고쳐 판정이 어긋나는 걸 막는다.

### 난입 차단은 `ACBGameplayGameMode::PreLogin`

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

#### 정원 값은 URL 옵션으로 흘려보낸다

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

### 세션 정리는 우리가 해야 한다 — 엔진이 안 치운다

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

### LAN 검색은 5초 고정이고, 같은 방이 여러 번 올라온다

Null 제공자(LAN 비콘)의 검색 동작 두 가지는 **버그가 아니라 설계**다. 모르고 보면 둘 다 우리 코드 문제로 보인다.

| 상수 | 값 | 결과 |
|---|---|---|
| `LAN_QUERY_TIMEOUT` | 5초 | **응답이 즉시 와도 5초를 채운 뒤** 콜백이 온다 |
| `LAN_QUERY_RETRY_TIME` | 1초 | 5초 동안 **1초마다 질의를 다시 브로드캐스트**한다 |

- **검색은 항상 5초 걸린다.** `OnValidResponsePacketReceived`는 결과를 담기만 하고, promise 는 `OnLANSearchTimeout`에서만 채워진다. 조기 종료가 없다 → **UI에 "검색 중" 표시가 필수**다
- **같은 방이 최대 5개로 보인다.** 호스트는 질의를 받을 때마다 응답하고, 엔진은 `AddSearchResult`에서 `AddUnique`가 아닌 **`Add`** 로 쌓는다. 그래서 `Local_HandleFindSessionsComplete`가 `TSet`으로 **세션 ID 중복을 걸러낸다**. EOS 로 바꿔도 중복이 없으면 그냥 통과하므로 그대로 둔다

### LAN 비콘은 자기가 본 모든 방을 광고한다 — 방 열기 직전 서비스 리셋

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

- **LAN 모드일 때만 한다.** 그 조건이 곧 제공자가 Null 이라는 뜻이다. 예전에는 별도 플래그였고, 조건을 제공자 이름에 걸지 않은 이유는 `FSessionsEOSGS` 가 `FSessionsLAN` 을 상속해 **EOS 라도 LAN 세션이면 같은 비콘 경로를 타기** 때문이다(`SessionsEOSGS.cpp` 의 `CreateSessionImpl`·`FindSessionsImpl` 이 LAN 구현으로 위임한다). 정상 EOS 경로에서는 광고 주체가 백엔드라 ①이 남아도 무해하다 — 그래서 EOS 로 옮기면 이 코드는 자동으로 죽는다
- **나가기 경로에는 넣지 않는다.** 비동기 `LeaveSession` 이 떠 있는 직후라 EOS 에서는 실제 백엔드 호출이 날아간다. 유령을 뿌리는 건 호스팅하는 쪽뿐이므로 "방 열기 직전" 한 곳이면 충분하다
- **세션 인터페이스·계정 ID 조회보다 반드시 먼저 부른다.** `ISessionsPtr` 을 들고 있는 채로 파괴하면 엔진이 참조 경고를 찍고, 미리 얻어둔 값은 죽은 인스턴스의 것이 된다
- 파괴와 함께 이전 검색 결과의 세션 ID 가 무효가 되므로 `FoundSessions`·`FoundSessionIds` 도 함께 비운다
- 계정 ID 가 새로 생성된다(Null 은 랜덤). 세션 작업마다 새로 조회하므로 영향 없다

> **전제 — 검색 중에는 UI 가 버튼을 모두 잠근다.** 이 리셋에는 C++ 가드가 없고, 그 자리를 UI 가 맡는다.
>
> LAN 검색은 5초 고정이라(위 "LAN 검색은 5초 고정") 그 사이 방 만들기를 누를 수 있는데, 그러면 `DestroyService` 가 진행 중인 검색 op 를 `Flush(Shutdown)` 으로 끊는다. 검색 실패 방송이 **`Local_ResetOnlineServices()` 실행 도중 재진입으로** 터지므로, 이를 구독한 위젯이 팝업이나 스택을 건드리면 흐름이 꼬인다. 재검색을 눌러도 엔진이 `Errors::AlreadyPending()` 으로 거절해(`FSessionsCommon::CheckState(FFindSessions)`) 목록이 잠깐 비었다 다시 채워진다.
>
> 둘 다 UI 에서 막는 쪽이 훨씬 싸므로 **C++ 에는 진행 중 플래그를 두지 않았다.** 검색 시작~완료 사이 버튼 잠금이 이 코드의 전제이니, 검색 UI 를 새로 만들 때 이 잠금을 빠뜨리지 말 것.

### 알려진 무해한 로그 (조치 불필요)

| 로그 | 원인 | 판단 |
|---|---|---|
| `LogOnlineSchema: Error: Invalid schema category Lobby/LobbyMember`<br>`LogOnlineServices: Error: [FLobbiesCommon::Initialize] Failed to initialize schema registry` | **우리가 쓰지 않는 Lobbies 기능의 초기화 실패.** `FOnlineServicesNull::RegisterComponents()`가 `FLobbiesNull`을 `FSessionsNull`과 **무조건 함께** 등록해, Null 제공자용 Lobby 스키마 설정이 없는 상태로 초기화가 돌아 나온다 | **그대로 둔다.** `FLobbiesCommon::Initialize()`는 실패해도 **로그만 찍고 그냥 반환**한다(`return` 없음, 컴포넌트 비활성화 없음). 우리는 `ILobbies`를 어디서도 호출하지 않으므로 영향 범위 밖이며, 서비스 초기화 시 **한 번만** 뜬다 |

> **에디터에서 `Previously active world ... not cleaned up by GC` + `REINST_...` + `TransBuffer` 조합이 뜨면 세션과 무관하다.** PIE 중 블루프린트를 컴파일해 생긴 리인스턴스 찌꺼기를 에디터 Undo 버퍼가 붙잡아 옛 PIE 월드가 GC되지 못하는 것으로, **에디터를 재시작하면 해소된다.** 패키징 빌드에는 없는 문제다.

## 나가기 — `Local_LeaveToMainMenu()`

들어오는 두 경로와 **같은 창구에서 나간다.** 접속에 관한 것은 전부 여기 모으기 때문이다(위).

| 위치 | 동작 |
|---|---|
| 클라이언트 | 로컬 PC의 **`ClientTravel(메인 메뉴, TRAVEL_Absolute)`** |
| 호스트·단독 | **`OpenLevel(메인 메뉴)`** |

- **메인 메뉴 맵은 `GameDefaultMap` 하나에서 온다.** 접속 실패 복귀와 같은 판정(`Local_ResolveMainMenuTravelMap()`)을 공유하며, 이미 메인 메뉴면 빈 문자열을 반환해 아무것도 하지 않는다
- **클라이언트만 `ClientTravel`인 이유는 커넥션 정리다.** 나가는 순간 서버가 `Logout`을 받아 로비 인원 집계가 갱신된다(→ [Lobby.md](Lobby.md) "준비 완료와 게임 시작"의 `Logout` 제외 처리와 한 쌍)
- **호스트가 나가면 남은 클라이언트는 자동으로 따라 나온다.** 커넥션이 끊겨 각자 `ConnectionLost` → 기존 실패 복귀 경로를 탄다. 호스트 자신에게도 실패가 올라오지만 넷 드라이버가 `NM_ListenServer`라 기존 가드에 걸려 무시된다 — **추가 코드가 없다**
- **레벨을 가리지 않는다.** 넷 모드만 보므로 게임플레이 레벨(일시정지 메뉴)에서도 그대로 쓸 수 있다. 다만 그 배선은 아직 하지 않았다(아래 "진행 상황")
- **세션에 들어가 있으면 먼저 빠져나온다.** 호스트는 파괴(`bDestroySession=true`), 참가자는 떠나기만 한다. **완료를 기다리지 않는다** — 이동은 다음 프레임이고, 기다리면 나가기가 온라인 서비스 응답에 묶인다

### 나가는 중의 접속 끊김은 경고로 띄우지 않는다 — `bLeaveRequested`

클라이언트가 스스로 커넥션을 닫으면 **그 끊김이 자기 자신의 실패 핸들러로 돌아올 수 있다.** 그대로 두면 "나가기를 눌렀는데 접속 끊김 경고창이 뜨는" 형태가 된다(모달을 배선하는 순간 드러난다).

```
Local_LeaveToMainMenu()      → bLeaveRequested = true
Local_HandleConnectionFailure → 방송은 건너뛰고, 메뉴 복귀는 그대로 수행
Local_HostLobby / Local_JoinServerByAddress → bLeaveRequested = false  (새 세션 = 재무장)
```

**막는 것은 방송뿐이고 복귀는 막지 않는다.** 나가기 이동 자체가 실패해도 메뉴에는 도착해야 하기 때문이다.

### 위젯을 먼저 빼고 부른다

게임 시작 때와 **같은 규칙이다** — [Lobby.md](Lobby.md)의 "이동 전에 로비 위젯을 반드시 스택에서 뺀다"의 근거가 그대로 적용된다. 나가기도 맵 전환이고, 로비 위젯은 `Should Remove Gameplay IMCs = true`다.

```
[BP] 나가기 버튼 / IA_CB_Leave
 ├ Remove This Widget            ← 스택 재계산 → IMC 복구
 └ Local_LeaveToMainMenu()
```

- **신호를 새로 만들지 않는다.** 게임 시작이 `Multicast_NotifyMatchStarting`이었던 건 **서버가 전원을 대신 이동시키기 때문**이고, 나가기는 **누른 사람 혼자의 로컬 동작**이라 위젯이 스스로 빠지면 끝이다
- **같은 프레임에 제거 → 이동 요청이어도 안전하다.** `OpenLevel`·`ClientTravel`은 `TravelURL`만 세팅하고 실제 이동은 `UEngine::TickWorldTravel`(다음 프레임)이다
- **증상이 늦게 나타나므로 더 위험하다.** 로비는 게임플레이 IMC를 애초에 안 붙이고 도착지인 메인 메뉴엔 조작할 캐릭터가 없어, 빠뜨려도 그 자리에서는 멀쩡해 보인다. 문제는 **메뉴 → 호스트 → 로비 → 게임플레이로 한 바퀴 돈 뒤**에 드러난다

## 진행 상황

**완료**
- **호스트·참가(주소 직접 입력)** — `UCBSessionSubsystem` (접속 실패·맵 이동 실패 복귀 포함, 위 "호스트·참가")
- **메인 메뉴 호스트·참가 버튼 배선** (`WBP_CB_MainMenu`) — 2인 접속 확인 완료
- **세션 시스템(Null 제공자)** — 방 생성·검색·참가·나가기. OnlineServices(v2) API, 제공자는 ini 한 줄 (위 "세션"). **PIE 2창 검증 완료**
- **접속 실패 처리 분기** — `UCBOnlineSession` + `UCBGameInstance::GetOnlineSessionClass()` 로 엔진의 자동 기본 맵 복귀를 가로채, **참가 실패는 제자리 유지 / 접속 후 끊김은 메뉴 복귀** (위 "엔진의 자동 복귀"). **컴파일까지 확인, 실행 검증 남음**
- **로비 나가기** — `UCBSessionSubsystem::Local_LeaveToMainMenu()` (클라 `ClientTravel` / 호스트 `OpenLevel`, 위 "나가기"). **BP 배선·실행 검증 남음**

**미구현**
- **EOS 제공자 전환** — 플러그인 활성화 + 자격증명 + Device ID 로그인 + `NetDriverEOS`(P2P) + 호스트 주소를 EOS 주소로. 제공자는 메인 메뉴에서 런타임에 고르고, 세션 호출 코드는 그대로다
- 세션 목록 UI (`WBP_CB_SessionList`, `Menu` 레이어) + 메인 메뉴에 방 만들기/찾기 버튼 — C++ 창구는 완료
- 접속 실패 사유를 모달로 표시 — C++은 완료, **위젯 배선만 남음**. 참가 실패는 `OnConnectionFailed` 구독, 접속 후 끊김은 `Local_ConsumePendingFailureReason()` — 두 경로 다 물릴 것 (위 "엔진의 자동 복귀")
- 나가기 BP 배선 (`WBP_CB_Lobby_Footer` / `IA_CB_Leave`) — C++ 창구는 완료. **`Remove This Widget` → `Local_LeaveToMainMenu()` 순서**를 지킬 것 (위 "위젯을 먼저 빼고 부른다")
- 나가기 사유 구분 — 호스트가 나갔을 때 남은 클라이언트에 "호스트가 종료했습니다"를 표시할지. **모달 배선 뒤에 판단한다** — 지금은 아무 메시지도 안 보이고, 이동 직전 멀티캐스트는 월드 teardown과 경합해 도착이 보장되지 않는다. 붙인다면 사유를 미리 저장해두고 실패 시 우선 사용하는 형태
- 게임플레이 레벨의 나가기(일시정지 메뉴) — C++은 레벨을 가리지 않으므로 배선만 남음. 다만 팩 일시정지 메뉴에 **자체 메인 메뉴 복귀 경로**가 있어 둘 중 무엇이 이길지 정해야 하고, 호스트가 나가는 건 곧 매치 종료라 `ACBGameplayGameMode` 규칙과 함께 설계한다

## 관련 문서

- 레벨·게임모드 구조, 맵을 넘는 데이터: [GameFlow.md](GameFlow.md)
- 로비 인원 집계·준비 완료·게임 시작: [Lobby.md](Lobby.md)
- 서버 권위·접두사·복제 정책: [Multiplayer.md](../Conventions/Multiplayer.md)
- 메인 메뉴 위젯·HUD 스택: [EasyGameUI.md](../Presentation/EasyGameUI.md)
