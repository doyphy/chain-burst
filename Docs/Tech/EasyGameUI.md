# EasyGameUI (시스템 UI 프레임워크)

> 시스템 UI(메인 메뉴·일시정지·옵션)는 서드파티 에셋팩 **Easy Game UI 3.0**을 도입해 구현한다. 이 문서는 **우리 프로젝트의 결정 사항과 접점 규칙**만 담는다 — 팩 자체의 사용법은 공식 문서가 갱신되므로 여기에 복사하지 않는다.
>
> 캐릭터에 붙는 UI(체력바 등)는 이 팩이 아니라 [UI.md](UI.md)의 `UCBUIComponent` 계통이 담당한다. 경계는 아래 "로드아웃 vs Global Config" 참조.

## 도입 정보

| 항목 | 값 |
|---|---|
| 팩 | Easy Game UI **3.0** (통합팩) |
| 엔진 | UE 5.8 |
| 에셋 위치 | `/Game/EasyGameUI` (팩 원본, **직접 수정 금지** — 아래 정책 참조) |
| 우리 에셋 위치 | `/Game/ChainBurst/UI` |
| 필요 플러그인 | `JsonBlueprintUtilities`, `BlueprintFileUtils` (둘 다 `ChainBurst.uproject`에 활성화됨) |

## 공식 문서

통합팩 기준 — https://www.nerwy-ue.com/docs/easy-game-ui-ultimate-3-0/
(개별 제품판은 `.../docs/easy-game-ui-3-0/`. 하위 경로와 내용은 대부분 동일)

| 하위 경로 | 언제 보나 |
|---|---|
| `ui-navigation-system/` | **위젯을 새로 만들거나 스택에 올릴 때 — 가장 중요** |
| `global-config-styling-system/` | Config·Styling 데이터 에셋을 다룰 때 |
| `settings-system/` | 옵션 메뉴에 설정을 추가·제거할 때 |
| `input-prompts-system/` | 키 아이콘 프롬프트를 표시할 때 |
| `intro-screen-main-menu/` / `pause-menu/` / `options-menu/` | 해당 메뉴를 손볼 때 |
| `project-setup/` / `game-instance-setup/` / `game-hud-setup/` | 초기 세팅·재설치 시 |
| `additional-setup-system-testing/` | 트러블슈팅 (플러그인 로드 순서, UE5.7+ 마이그레이션 픽스) |
| `save-game-system/` / `photo-mode/` / `game-credits/` | **미채택** — 아래 참조 |

지원: Discord / nerwy.ytb@gmail.com

**팩 구조를 확인하는 순서는 공식 문서 → 에디터다. `.uasset` 바이트를 grep해서 추론하지 말 것.** grep으로 알 수 있는 건 "어떤 이름 문자열이 등장하는가"까지고, 배치된 액터인지 클래스 참조인지·언제 호출되는지는 구분되지 않는다. 실제로 이 방식으로 `BP_EasyMainMenuController`의 부모 클래스(Pawn을 PlayerController로 오판)와 배치 방식(레벨 배치를 World Settings 오버라이드로 오판)을 두 번 잘못 짚었다. 참조 관계는 **Reference Viewer**(`Alt+Shift+R`), 호출 지점은 **BP 전역 검색**(`Ctrl+Shift+F`로 `Insert Widget Instance in Stack` 등)이 정본이다.

## 채택 범위

**모든 기능을 쓰지 않는다.** ChainBurst는 리슨 서버·서버 권위 멀티플레이(→ [Multiplayer.md](Multiplayer.md))라 싱글플레이 전제 기능과 충돌한다.

| 기능 | 채택 | 비고 |
|---|---|---|
| UI 내비게이션 시스템 | ✅ | 이 팩의 핵심. 우리 위젯도 전부 이 규칙을 따른다 |
| 메인 메뉴 | ✅ | |
| 일시정지 메뉴 | ✅ | **`Pause Game in Pause Menu?` = False** — 멀티플레이에서 게임을 멈출 수 없다 |
| 옵션 메뉴 / 세팅 시스템 | ✅ | 그래픽·오디오·키바인딩. 팩의 가장 큰 가치 |
| 인풋 프롬프트 | ✅ | |
| 글로벌 스타일링 | ✅ | |
| 인트로 화면 | ❌ | `Intro Screen Widget`을 비워 스킵 |
| **세이브/로드** | ❌ | 서버 권위 설계와 충돌. `Save/Load Operations Allowed?` = **False** → 관련 버튼 자동 제거 |
| 포토 모드 | ❌ | `Enable Photo Mode in Game?` = False |
| 게임 크레딧 | ❌ | `Display Game Credits Button in Menus?` = False. 추후 켤 수 있음 |
| 벤치마크 툴 | ❌ | `Enable Game Benchmarking Tool?` = False |

## 통합 구조

| 역할 | 우리 선택 | 이유 |
|---|---|---|
| **GameInstance** | `UCBGameInstance`(C++)를 **`GI_EasyMainGameInstance`의 부모로 리페어런팅** | 세션·매치메이킹 등 게임 전역 로직이 올 자리 확보. 지금은 빈 껍데기 |
| **HUD** | `ACBHUD`(C++)를 **`BP_EasyMainGameHUD`의 부모로 리페어런팅** | **C++에서 스택을 조작하려면 이 방법뿐**이다. 팩의 스택 조작은 BP 인터페이스로만 노출되므로, `ACBHUD`에 `BlueprintImplementableEvent`를 선언하고 팩 BP가 구현한다 |
| **GameMode (게임플레이)** | `BP_GameModeBase` — HUD Class = `BP_EasyMainGameHUD` | |
| **GameMode (메인 메뉴)** | `GM_CB_MainMenu` — HUD 지정, **`Default Pawn Class` = None**, PlayerController는 기본값 | **팩은 메뉴 레벨의 게임모드에 아무 요구도 하지 않는다**(공식 문서에 언급 없음, 데모도 게임플레이용 데모 게임모드를 그대로 쓴다). 우리가 따로 두는 유일한 이유는 `BP_GameModeBase`를 쓰면 **메뉴 레벨에 Chaser가 스폰**되기 때문이다 |
| **Global Config** | `DA_CB_GlobalConfig` (`PDA_EGUI_GlobalConfigSystem` 상속) | 데모 에셋을 쓰면 팩 업데이트 때 설정이 날아감 |
| **Global Styling** | `DA_CB_Styling` (`PDA_EGUI_GlobalStylingSystem` 상속) | |

**왜 자식 BP가 아니라 리페어런팅인가**: 상속은 한 줄이라 C++ 베이스와 팩 BP를 나란히 둘 수 없다. C++가 팩 BP를 상속할 수도, 팩 BP가 C++와 형제가 될 수도 없으므로 **우리 클래스를 팩 BP의 부모로 끼워넣는 것**이 유일한 경로다(공식 문서 Situation C). 대가로 **팩 에셋 수정이 발생**하므로 아래 업데이트 체크리스트에 반드시 반영한다.

**`ACBHUD`가 노출하는 다리** (`BlueprintImplementableEvent` — 팩 BP가 구현):

| C++ 이벤트 | BP 구현 |
|---|---|
| `PushGameLayerWidget(UUserWidget*)` | `Insert Widget Instance in Stack` (Layer = `Game`) |
| `PopWidgetFromStack(UUserWidget*)` | `Remove Widget Instance from Stack` |

구현은 **각각 노드 하나**다. 복잡한 처리는 전부 팩 함수 안에 있으므로 여기서 재구현하지 않는다. **이 이벤트가 복잡해지면 팩 기능을 중복 구현하고 있다는 신호다.**

**제약: 이 다리는 `Game` 레이어 전용이다.** `PushGameLayerWidget`의 BP 구현이 Layer=`Game`으로 고정되어 있다. 따라서 **C++에서 `Menu`/`Modals` 레이어에 위젯을 올릴 수 없다.** 지금은 체력바(=`Game`)만 이 경로를 쓰므로 문제가 없지만, C++에서 메뉴 계열 위젯을 띄워야 하는 순간이 오면 **레이어를 인자로 받는 이벤트를 추가**해야 한다(기존 이벤트를 레이어 인자 형태로 통합할지, 별도로 둘지는 그때 판단). BP에서만 띄운다면 `BPI_EGUI_HUDInterface`를 직접 호출하면 되므로 이 제약과 무관하다.

## 팩 에셋 수정 정책

**원칙: `/Game/EasyGameUI` 아래 원본은 수정하지 않는다.** 커스터마이징이 필요하면 `/Game/ChainBurst/UI`로 복제하고 Global Config에서 참조를 바꾼다.

공식 문서는 원본 직접 편집을 전제로 쓰여 있지만(복제/커스텀 위젯 언급 없음), Global Config에 위젯 클래스 참조 필드가 있어 교체 경로가 정식으로 열려 있다. 멀티플레이용 버튼(호스트/참가 등)이 추가될 것이 확실하므로 복제 전략을 택한다.

| 대상 | 방식 |
|---|---|
| 메인 메뉴 위젯 | `WBP_EasyMainMenu` 복제 → `WBP_CB_MainMenu` |
| HUD / GameInstance | **리페어런팅** (아래 "불가피한 팩 에셋 수정" 참조) |
| Config / Styling | 상속 데이터 에셋 |

**단, 자식 위젯 BP로는 커스터마이징할 수 없다.** UMG는 상속받은 위젯 트리를 디자이너에서 수정할 수 없어 버튼 추가·순서 변경이 불가능하다. 위젯은 복제만이 선택지다.

**불가피한 팩 에셋 수정** (업데이트마다 재적용 필요):
- `BP_EasyMainGameHUD` — 부모를 `ACBHUD`로 리페어런팅 + `Push Game Layer Widget` / `Pop Widget From Stack` 이벤트 구현
- `GI_EasyMainGameInstance` — 부모를 `UCBGameInstance`로 리페어런팅
- `BP_EGUI_GlobalConfigSelector` — `Global Config Data Asset`을 우리 에셋으로 지정해야 하는데, 이 BP 자체가 팩 소속이다.

### 팩 인터페이스에는 함수를 추가하지 않는다

`BPI_EGUI_UINavigationInterface`처럼 **팩 전체가 구현하는 계약**에 함수를 더하면 대가가 크다. 실제로 겪은 것:

- **클래스의 함수 목록은 구현한 인터페이스에서 계산된다.** 인터페이스에 함수를 추가하면 `WBP_EasyMasterWidget`이 **에셋 수정 없이도** 그 이름을 갖게 된다. git에는 그 위젯이 변경으로 잡히지 않아 원인을 찾기 어렵다
- 그 상태에서 우리 위젯이 같은 이름을 다른 시그니처로 요구하면 `Cannot override … declared in a parent with a different signature`가 뜬다. **UFunction 이름은 클래스 안에서 유일해야 하므로 인터페이스가 달라도 충돌한다**
- 인터페이스에서 함수를 지워도 **구현체에 남은 노드는 같이 사라지지 않는다.** 고아가 된 채 그 이름을 계속 점유한다 (`Ctrl+Shift+F` 전역 검색으로 찾는다)
- 팩을 업데이트하면 덮어써지고, 그 순간 위 에러가 전면적으로 되살아난다

**우리 계약은 우리 인터페이스에 둔다.** 위젯은 인터페이스를 여러 개 구현할 수 있으므로 팩 계약을 건드릴 이유가 없다. 단 **함수 이름을 팩과 겹치지 않게 짓는다** — 인터페이스를 새로 만들어도 이름이 같으면 위 충돌이 그대로 재현된다.

> **인터페이스 시그니처를 바꾼 뒤 설명되지 않는 컴파일 에러가 남으면 에디터를 재시작한다.** 이미 로드된 구현 클래스가 옛 함수 목록을 메모리에 들고 있어서 생기며, 재시작하면 인터페이스로부터 전부 다시 계산된다. **노드를 지웠다 다시 만들기 전에 이것부터 시도할 것** — 이번에 여기서 한참 돌아갔다.

## UI 내비게이션 규칙 — 미구현 (적용 예정)

> 상세는 공식 문서 `ui-navigation-system/`. 여기엔 **우리 코드가 지켜야 할 것**만 적는다.

위젯이 자기 가시성·입력·포커스를 직접 관리하지 않고, **HUD가 스택 전체를 보고 실제 상태를 계산**한다. 위젯은 "원하는 상태"를 선언만 한다.

**구성 4요소** — 인터페이스가 양방향 2개로 나뉘어 있어 HUD와 위젯이 서로의 클래스를 몰라도 되고, 상속이 불가능한 C++ 위젯도 끼어들 수 있다.

| 에셋 | 역할 |
|---|---|
| `BP_EasyMainGameHUD` | 중재자. 스택 보유, 상태 계산·적용 |
| `BPI_EGUI_HUDInterface` | HUD 공개 API (위젯 → HUD) |
| `BPI_EGUI_UINavigationInterface` | 위젯 계약 (HUD → 위젯) |
| `WBP_EasyMasterWidget` | 계약의 기본 구현을 제공하는 부모 위젯 |

**스택 5레이어** (아래→위): `Game` → `Menu` → `Modals` → `Loading` → `System`
위젯은 자기보다 아래 레이어에만 영향을 줄 수 있고, 위쪽은 건드릴 수 없다. 레이어는 단일 캔버스(`WBP_EGUI_MainUIPanel`) 위에서 ZOrder 100 간격으로 구간 분할된다.

**우리 위젯의 레이어 배정**:

| 위젯 | 레이어 |
|---|---|
| 캐릭터 HUD(체력 등) | `Game` |
| 메인 메뉴 / 일시정지 / 옵션 | `Menu` |
| 확인창·경고 | `Modals` |

> **확인 필요**: 팩의 `BP_EasyMainMenuController`는 인트로·메인 메뉴 둘 다 `E_WidgetLayers`의 **세 번째 항목**(내부명 `NewEnumerator2`)을 넘긴다. 유저 정의 열거형은 내부명과 표시 순서가 어긋날 수 있으므로, 노드에서 **드롭다운 표시 이름을 직접 확인**할 것. 실제 값이 `Menu`가 아니면 위 표를 실제에 맞춰 고친다.

**`Loading` 레이어 제약 — 맵 전환을 가로지를 수 없다.** HUD는 맵이 바뀔 때 파괴·재생성되므로, 스택에 올린 로딩 화면도 HUD와 함께 사라진다. 맵 전환 자체를 덮는 로딩 연출이 필요하면 스택이 아니라 전환 맵이나 GameInstance 계층을 써야 한다.

**상태 계산 규칙 — 항목마다 참조 범위가 다르다.** "최상단 위젯만 권한을 갖는다"는 절반만 맞다.

| 결정 | 참조 범위 | 규칙 |
|---|---|---|
| 입력 설정 | 최상단부터 아래로 탐색 | `Use Desired Input Config?`=false면 아래에서 제공 가능한 위젯을 찾고 → 등록된 fallback → default |
| 포커스 | 최상단의 focus handler | `Widget Is Focus Handler?`=true인 위젯의 `Focus Target` |
| 오클루전 | **스택 전체** | `Widget Is Global Occluder?`=true가 하나라도 있으면 그 아래 전부 숨김. **활성 위젯이 아니어도 적용된다** |
| 게임플레이 IMC | **스택 전체 OR** | 하나라도 `Should Remove Gameplay IMCs?`=true면 제거. **전부 false가 되어야 복구** |

→ 옵션 메뉴 위에 확인 모달이 떴다가 모달만 닫혀도, 옵션 메뉴가 남아 있는 한 게임 입력은 돌아오지 않는다. 의도된 동작이다.

**그래서 맵을 넘기 전에는 IMC를 걷어낸 위젯을 반드시 빼야 한다.** 복구가 스택 변화에 붙어 있는데, HUD는 맵과 함께 통째로 파괴되므로 **재계산 없이 사라지면 복구가 영영 실행되지 않는다.** 정작 IMC를 들고 있는 `EnhancedInputLocalPlayerSubsystem`은 `ULocalPlayer` 소속이라 travel을 넘어 살아남아, **새 레벨에서 입력이 죽은 채로 시작한다.** 로비 → 게임플레이 이동에서 실제로 겪은 증상이며, 해법(이동 직전 방송)은 → [GameFlow.md](GameFlow.md)

**나가기(로비 → 메인 메뉴)도 같은 대상이다.** 맵 전환인 것은 똑같으므로 위젯을 먼저 빼야 한다. 다만 서버가 전원을 이동시키는 게임 시작과 달리 **누른 사람 혼자의 로컬 동작**이라, 신호를 방송할 것 없이 위젯이 스스로 `Remove This Widget` 한 뒤 이동을 호출한다 → [GameFlow.md](GameFlow.md)

**IMC 제거가 게임 입력을 막는 *유일한* 수단이다.** `E_InputModes`에는 `Game Only` / `Game and UI` 둘뿐이고 언리얼 기본에 있는 **`UI Only`가 없다.** 메뉴를 띄울 때 입력 모드로 게임 입력을 통째로 막는 게 아니라, `Game and UI`를 유지한 채 게임플레이 IMC만 걷어내는 방식이다(내비게이션 IMC는 살려둬야 하므로). 따라서 **Global Config에 등록하지 않은 IMC는 메뉴가 떠도 그대로 살아있다** — 아래 필수 등록 체크리스트의 근본 이유다.

**등록은 "이 IMC를 걷어내도 좋다"는 권한 부여다.** 그래서 같은 성질이 상황에 따라 버그도 되고 기능도 된다 — 게임플레이 IMC를 빠뜨리면 메뉴가 떠도 캐릭터가 움직이고(버그), 반대로 **로비 전용 조작 IMC를 일부러 등록하지 않으면** 메뉴가 떠 있어도 그 조작만 살아남는다(기능).

**제거·복구는 위젯 하나의 수명이 아니라 스택 상태 변화에 붙어 있다.** 삽입·제거가 있을 때마다 스택 전체를 다시 계산해 OR 결과를 적용한다. "메뉴를 닫으면 복구"가 아니라 **"남은 스택에 true가 하나도 없으면 복구"** 다.

### `Add Mapping Context on Game Load?` 는 복구와 무관하다 (검증됨)

`F_GameplayMappingContextDefinition`의 필드다. 이름 때문에 복구와 얽혀 보이지만 실측 결과 그렇지 않다.

| 값 | 의미 |
|---|---|
| `True` | **최초 게임 로드 시** 팩이 이 IMC를 직접 추가 |
| `False` | 팩은 추가하지 않음. **등록 목록으로서의 역할(제거 권한)은 그대로** |

복구는 이 값과 무관하게 **제거 직전에 실제로 적용돼 있던 IMC를 되돌린다.** 팩 툴팁이 *"기본으로 추가되지 않을 IMC도 등록하라(boolean = False로)"* 고 안내하는 것과 일치한다 — 복구가 `True` 항목만 되돌린다면 `False` 항목은 메뉴를 처음 여는 순간 영구히 사라질 것이기 때문이다.

**우리 설정은 `False`다.** `IMC_Default`의 추가 주체는 로드아웃(`UCBInputManagerComponent::RegisterMappingContexts`) 하나로 유지한다 → [Input.md](Input.md). `True`로 두면 추가 주체가 팩과 로드아웃 둘로 갈리고, 팩 쪽이 더 일러서 **캐릭터 준비 전에 입력이 살아난다.**

**절대 규칙**:
- **`AddToViewport` / `Remove From Parent` 금지.** `Insert Widget Instance in Stack` / `Remove Widget Instance from Stack`을 쓴다. 스택 밖의 위젯은 메뉴가 떠도 가려지지 않고 포커스 관리에서도 빠진다.
- 리페어런팅(`WBP_EasyMasterWidget`) 후 `Event On Initialized` / `On Preview Key Down` / `On Mouse Move`를 오버라이드했다면 **Parent 호출을 내 로직보다 먼저** 실행한다. 누락이 가장 흔한 버그 원인.
- 마우스 동작을 위해 배경을 막는 요소(Visibility=`Visible`)를 하나 둔다.
- 제거를 직접 처리할 때 지연 제거 주의 — 제거 중인 위젯을 다시 스택에 넣으면 문제가 생긴다.

**위젯 계약 10함수** (`Interfaces > Easy Game UI Widgets > UI Navigation`). **굵은 5개는 `WBP_EasyMasterWidget`이 기본 구현을 제공**하므로, 상속받은 WBP는 필요한 것만 오버라이드하면 된다.

| 구분 | 함수 | 정하는 것 / 불리는 시점 |
|---|---|---|
| 선언 | **`Get Widget Desired Input Config`** | 활성화 시 입력 모드, 제거 시 폴백, 입력 이벤트 수신 여부(`Register Input Events Listener?`) |
| 선언 | **`Get Widget Desired Focus Target`** | 포커스 핸들러 여부 + 포커스 대상(동적 가능 → 패드↔마우스 전환 자동) |
| 선언 | **`Get Widget Desired Occlusion Rules`** | `Global Occluder`(아래 전부 숨김), `Should Remove Gameplay IMCs` |
| 생명주기 | `On Widget Added to Stack` | 스택에 들어갔으나 **아직 화면에 안 보임** — 데이터 채우기 |
| 생명주기 | `On Widget Activated` | 활성 포커스 핸들러가 됨 |
| 생명주기 | `On Widget Deactivated` | 활성 포커스 핸들러에서 내려옴 |
| 생명주기 | **`On Widget New Visibility Requested`** | 시스템이 가시성 변경을 요구 |
| 생명주기 | **`On Widget Removed from Stack`** | 스택에서 완전 제거, 삭제 대상 |
| 입력 | `On Any Key Triggered` | 아무 키 입력 (`Register Input Events Listener?` 필요) |
| 입력 | `On Named Input Event Triggered` | 이름 붙은 내비 입력(back, header nav 등 → `E_UI_NavInputList`) |

`HandledByWidget?`(가시성·제거 두 이벤트의 반환값)을 true로 하면 위젯이 직접 숨기거나 `Remove From Parent` 해야 한다 — 제거 애니메이션용 탈출구다. false면 시스템이 처리하고 위젯은 아무것도 안 해도 된다. 위 "지연 제거 주의"가 이 값을 true로 둔 경우의 이야기다.

**우리 위젯의 선언값** (적용 시 기준):

| 위젯 | Input Config | Focus Handler | Global Occluder | Remove Gameplay IMCs |
|---|---|---|---|---|
| 체력바 (`Game`) | ❌ | ❌ | ❌ | ❌ |
| 메인 메뉴 / 일시정지 / 옵션 (`Menu`) | ✅ | ✅ | ❌ | ✅ |
| **로비 (`WBP_CB_Lobby_Main`, `Menu`)** | ✅ | ✅ | ❌ | ✅ |
| 확인창·경고 (`Modals`) | ✅ | ✅ | ❌ (뒤 메뉴가 보여야 함) | ✅ |

> 로비의 `Should Remove Gameplay IMCs`는 **지금은 아무 효과가 없다** — 로비에서는 게임플레이 IMC를 애초에 붙이지 않으므로 걷어낼 것이 없다(→ [GameFlow.md](GameFlow.md)). 그래도 `Menu` 레이어 규약대로 켜 둔다. 로비에 조작이 생기거나 이 위젯이 다른 레벨에서 뜨는 순간 값이 맞아 있어야 한다.

`Global Occluder`의 **정석 용례는 `Loading` 레이어**다 — 열거형 툴팁부터가 "아래 전부를 숨기는 로딩/전환 화면용"으로 정의되어 있다. 반대로 모달에 켜면 배경 메뉴가 사라져 "무엇에 대한 확인인지" 맥락을 잃으므로 끈다.

**C++ 위젯**(`UCBHealthBarWidget` 등)은 리페어런팅이 불가하므로 **`BPI_EGUI_UINavigationInterface` 직접 구현** 경로를 쓴다. `On Mouse Move` / `On Preview Key Down`에 `Handle on Mouse Move` / `Handle on Preview Key Down` 매크로를 배선하고, HUD 참조를 변수로 캐싱한다. `WBP_EasyMasterWidget`이 `HudReference` 변수 + `Get Easy Game HUD` 함수로 `BPI_EGUI_HUDInterface`에 캐스팅하는 구조이므로, **그 배선을 그대로 흉내내면 된다.**

### 위젯 인스턴스 캐시는 HUD가 아니라 GameInstance에 있다

HUD는 맵마다 파괴되지만 위젯 인스턴스까지 매번 버려지지는 않는다. `BP_EGUI_GameUIRegistry`(부모 클래스가 `Object` — Actor가 아니다)가 그 캐시를 들고, **`GI_EasyMainGameInstance`가 이 레지스트리를 소유**한다.

| 계층 | 보관하는 것 | 수명 |
|---|---|---|
| HUD `BP_EasyMainGameHUD` | `WidgetsStack` — 스택 상태 | **맵 단위** |
| Registry (GameInstance 소유) | `WidgetsInstance` / `WidgetHardClasses` | **게임 전체** |

`F_WidgetClassDefinition`의 **`KeepPersistentInstance?`** 를 켜면 인스턴스가 레지스트리에 남아 맵이 바뀌어도 재사용된다(새 맵의 HUD가 다시 스택에 꽂는다). 무겁고 상태가 있는 위젯(옵션 메뉴 등)이 대상이다. **캐릭터에 종속된 위젯(체력바)은 캐릭터가 매 맵 새로 생기므로 끈다.**

### 스택 조작 타이밍 — HUD의 `BeginPlay` 이후여야 한다

레벨에 배치된 액터의 `BeginPlay`에서 곧장 스택에 넣으면 아래 경고가 뜨고 **위젯이 안 뜬다**.

```
Accessed None trying to read property GameUIRegistry in BP_EasyMainGameHUD_C
Function ...:CreateWidgetFromDefinition
```

**HUD 객체가 없는 게 아니다.** 로그가 지목하는 대상은 실제 HUD 인스턴스(`BP_EasyMainGameHUD_C_0`)이고, None인 것은 그 안의 `GameUIRegistry` 변수다. HUD는 플레이어 로그인 과정에서 이미 스폰되지만, **자기 `BeginPlay`에서 레지스트리를 받기 전**이라 변수가 비어 있는 상태다.

**팩의 대응**: `BP_EasyMainMenuController`의 `BeginPlay`는 초기 세팅(폰 빙의·카메라·Config 캐싱) 직후 **`Delay Until Next Tick`** 을 하나 두고, 그 뒤에 스택을 건드린다.

**한 틱이 보장하는 것과 아닌 것** — 그대로 복사하기 전에 구분할 것.

| | 내용 |
|---|---|
| ✅ 보장 | 같은 프레임에 `BeginPlay`가 예정된 액터들은 전부 실행을 마쳤다. 레벨 시작 시점에 **HUD와 내 액터가 둘 다 존재**하면 순서 불확정 문제는 사라진다 |
| ❌ 미보장 | HUD 초기화에 **비동기**가 끼면 한 틱으로 부족하다 |
| ❌ 미보장 | HUD가 **나중에 생기는** 경우(레벨 이동·리스폰·늦은 접속)엔 내 `BeginPlay` 기준 한 틱이 아무 의미가 없다 |

즉 팩의 한 틱은 **범용 보장이 아니라 "메뉴 레벨"이라는 통제된 상황에 맞춘 가정**이다. 같은 상황이면 따라 써도 되지만, 게임플레이 중에 위젯을 띄우는 코드에 그대로 옮기면 안 된다.

#### 런타임에 띄울 때는 문제가 "해소"되는 게 아니라 "달라진다"

게임플레이 도중이라면 HUD는 이미 오래 전에 `BeginPlay`를 마쳤다. **경합할 것이 없으므로 지연도 필요 없다.** 그런데 대신 다른 위험이 셋 생긴다.

| 위험 | 내용 | 대응 |
|---|---|---|
| **HUD가 없는 컨트롤러** | HUD는 **로컬 플레이어의 컨트롤러에만** 존재한다. 서버가 원격 클라이언트의 PC로 `GetHUD()`를 부르면 None이고 **영원히 안 생긴다** | 로컬 여부를 먼저 판별. `UCBUIComponent`의 `IsPlayerControlled() && IsLocallyControlled()` 병행 검사가 그 선례 |
| **맵 전환 직후** | HUD는 **맵마다 파괴·재생성**된다. 새 맵 로드 시점에 스폰된 액터는 다시 "레벨 시작" 상황이라 위 경합이 되살아난다 | 레벨 시작 케이스로 취급 |
| **데이터 미준비** | HUD는 멀쩡한데 넣을 값(ASC·로드아웃)이 아직 없을 수 있다. **HUD 타이밍과 별개 문제다** | [SystemReady.md](SystemReady.md)의 준비 완료 훅. 두 문제를 섞지 말 것 |

**우리 코드의 원칙 — 틱을 세지 말고 순서대로 확인한다**:

1. **로컬인지 확인** — 아니면 애초에 띄우지 않는다 (원격/AI 분기)
2. **`Get HUD` 유효성 확인** — None이면 경고 로그 남기고 건너뛴다
3. **삽입 결과를 검사** — 반환된 위젯 인스턴스를 `IsValid`로 확인. 팩 자신도 이렇게 하고 실패 시 에러를 찍는다. **검사를 빼면 "조용히 아무 일도 안 일어남"이 된다**
4. 그래도 타이밍이 통제되지 않으면 → 틱이 아니라 **`ACBHUD`의 준비 신호**를 기다린다 (캐릭터 쪽 [SystemReady.md](SystemReady.md)와 같은 해법. 필요해지는 시점에 도입)

**프레임 수는 답이 아니다.** 상황마다 달라지고, 맞아도 우연히 맞는 것이다.

### 데이터 타입 — 전부 `/Game/EasyGameUI/Datas`

C++ 정의는 없고 **블루프린트 열거형·구조체 에셋**이다. 필드의 정확한 타입·기본값은 에디터에서 열어 확인할 것.

| 에셋 | 내용 |
|---|---|
| `E_WidgetLayers` | 5개 레이어 |
| `E_InputModes` | `Game Only` / `Game and UI` (**`UI Only` 없음**) |
| `F_WidgetLayerDefinition` | 스택 원소 — `Layer`, `WidgetReference`, `ZOrder` |
| `F_WidgetClassDefinition` | `WidgetClass`, `KeepPersistentInstance?` |
| `F_InputModeConfig` | `InputMode`, `HideMouseCursor`, `LockMouse`, `FlushInputs` |
| `F_GameplayMappingContextDefinition` | Global Config에 등록하는 게임플레이 IMC 정의 (단순 IMC 배열이 아님 — 등록 시 필드 확인) |

`Get Widget Desired Input Config`가 반환하는 "입력 설정"의 실체가 `F_InputModeConfig`의 **위 4개 값**이다. C++ 위젯이 이 인터페이스를 구현할 때 채워야 할 값이기도 하다.

## 새 위젯 만들기 — 순서

> 상세는 공식 문서 `ui-navigation-system/`. 여기엔 **순서와 우리 값**만 적는다.

**시스템 위젯(메인메뉴·일시정지·옵션)을 손볼 때는 새로 만들지 말고 팩 위젯을 복제해 개조한다.** 버튼 로직이 팩 내부 흐름(`New Game Level` 로드, 메뉴 복귀 등)과 얽혀 있어 재구현하면 중복 구현이 된다. 디자이너 탭에서 비주얼만 바꾸는 것이 정석이다. 아래 절차는 **팩이 모르는 우리 고유 위젯**(인벤토리·로비 등)을 만들 때의 것이다.

1. **부모 클래스 = `WBP_EasyMasterWidget`** (Class Settings). 계약 10함수 중 5개의 기본 구현을 물려받는다
   - 이미 다른 BP/C++를 상속 중이라 불가능하면 → `BPI_EGUI_UINavigationInterface` 직접 구현 경로 (위 "C++ 위젯" 문단)
2. `Event On Initialized` / `On Preview Key Down` / `On Mouse Move`를 오버라이드하면 **Parent 호출을 먼저**
3. **선언 3함수** 오버라이드 → 값은 위 "우리 위젯의 선언값" 표
4. 배경을 막는 요소(`Visibility = Visible`)를 하나 깐다 — 패드↔마우스 전환 시 포커스 튐 방지
5. **스타일 통일이 필요하면 `WBP_EGUI_Common*`을 재료로 조립**한다. `DA_CB_Styling`은 그 위젯들에만 적용되며, 처음부터 직접 그린 위젯에는 먹지 않는다
6. 스택에 올린다 (아래 표). **`AddToViewport` 금지**
7. 제거는 위젯 자신이 **`Remove This Widget`**, 외부에서는 `Remove Widget Instance from Stack`. **`Remove From Parent` 금지**

| 목적 | 함수 |
|---|---|
| 생성 + 삽입 한 번에 | `Add Widget of Class to Stack` (`F_WidgetClassDefinition` + `E_WidgetLayers`) |
| 데이터를 채워 넣고 삽입 | `Create Widget From Definition` → 데이터 세팅 → `Insert Widget Instance in Stack` |

데이터 주입은 `On Widget Added to Stack`(스택에 들어갔으나 아직 화면에 안 보이는 시점)을 쓰는 방법도 있다.

**호출 형태**는 팩을 따른다 — HUD를 캐스팅하지 말고 인터페이스 메시지로 호출한다. HUD 클래스를 몰라도 되고, 잘못 세팅됐을 때 크래시 대신 조용히 실패한다.

```
PlayerController → Get HUD → (BPI_EGUI_HUDInterface 메시지) Add Widget of Class to Stack
```

**타이밍은 위 "스택 조작 타이밍" 절을 반드시 확인할 것.** 이 절차를 다 지켜도 호출 시점이 이르면 아무 일도 일어나지 않는다.

## 로드아웃 vs Global Config

에셋 등록소가 둘이 되므로 경계를 고정한다.

| 대상 | 등록 위치 | 근거 |
|---|---|---|
| **캐릭터 종속 UI** — 체력바, 추후 쿨타임·버프 | **로드아웃** (→ [Loadout.md](Loadout.md)) | 캐릭터마다 달라야 한다 |
| **게임 전체 시스템 UI** — 메인메뉴·일시정지·옵션·로딩 | **Global Config** | 캐릭터와 무관하다 |

> Global Config는 **"어떤 클래스를 쓸지"의 등록소**일 뿐 띄우는 주체가 아니다. 실제 스택 삽입은 언제나 `BPI_EGUI_HUDInterface` 호출로 일어난다.
>
> 위젯을 **어디에 띄울지**(HUD 스택 vs `UWidgetComponent`)와 **누가 생성할지**의 판단 기준은 → [UI.md "새 UI는 어디로"](UI.md)

**"Global Config를 안 쓴다"는 선택지는 없다.** HUD(`BP_EasyMainGameHUD`)·레지스트리(`BP_EGUI_GameUIRegistry`)·Common 위젯 전부가 이미 Config를 읽고 있어서, **스택을 쓰는 순간 Config도 쓰고 있는 것**이다. `DA_CB_GlobalConfig`를 만들지 않는 선택은 "Config를 안 쓴다"가 아니라 **"데모 Config(`DA_EGUI_DemoGlobalConfig`)를 쓴다"** 이고, 그건 팩 업데이트 때 덮어써진다. 우리 것을 따로 만드는 이유가 이것이다.

**단, 우리 고유 위젯을 Config에 등록할 필요는 없다.** Config는 "팩이 스스로 띄워야 하는 위젯"의 슬롯이다. 우리가 직접 띄우는 위젯은 띄우는 쪽이 클래스를 들고 있으면 되고, 스택 삽입 경로만 지키면 된다. 나중에 "여러 곳에서 참조해야 하는데 마땅한 주인이 없다"는 상황이 오면 `PDA_EGUI_GlobalConfigSystem`을 **상속한 BP 클래스**를 만들어 필드를 추가하는 경로가 있다(단순히 "Data Asset 생성 → 부모 선택"으로 만든 인스턴스에는 필드를 못 늘린다). 다만 그건 위 등록소 경계를 흐리는 결정이므로 이 문서도 함께 고쳐야 한다.

## 필수 등록 체크리스트 — 미구현 (적용 예정)

빠뜨리면 **에러 없이 조용히 오동작**하는 항목들이다.

| 항목 | 누락 시 증상 |
|---|---|
| **`BP_EGUI_GlobalConfigSelector` → `Global Config Data Asset` = `DA_CB_GlobalConfig`** | **우리 Config를 아무도 안 읽고 데모 설정으로 조용히 동작한다.** 팩 전체가 `BPFL_EGUI_EasyFunctionUtilities` → 이 Selector → Config 순으로 읽으므로 **Config 접근의 유일한 진입점**이다 |
| **Project Settings → Enhanced Input → Enable User Settings = True** | 실행 시 `Critical Error: The project setting 'Enable User Settings' is not True` |
| Global Config → **`Gameplay Input Mapping Contexts`** | **메뉴를 열어도 캐릭터가 계속 움직인다.** IMC 제거가 게임 입력을 막는 유일한 수단이므로(`UI Only` 모드 없음), 로드아웃이 적용하는 IMC를 **전부** 등록해야 한다(기본으로 안 켜는 것 포함) → [Input.md](Input.md) |
| **`Menu` 레이어 위젯의 `Should Remove Gameplay IMCs = true`** | 위와 **증상이 똑같다.** 등록만 해두고 위젯이 선언하지 않으면 아무도 걷어내라고 하지 않는다. `WBP_EasyMasterWidget`의 기본 구현은 false이므로 **`Get Widget Desired Occlusion Rules`를 오버라이드**해야 한다. 입력 모드를 정하는 `Get Widget Desired Input Config`와 혼동하기 쉬운데, IMC 제거는 **오클루전 함수 소관**이다 |
| Global Config → `Settings List` | 옵션 메뉴에 설정이 하나도 안 나옴 |
| Global Config → `Main Menu Level` / `New Game Level` | 메뉴 복귀·게임 시작이 동작 안 함 |
| Global Config → 각 위젯 참조 | 해당 메뉴가 안 뜸 |
| GameMode의 HUD Class (레벨 오버라이드 포함) | `Easy Game Hud Initialized` 로그가 안 뜨고 UI 전체가 죽음 |

## 팩 업데이트 시 재적용 체크리스트

팩을 갱신하면 `/Game/EasyGameUI` 원본이 덮어써진다. 아래는 **매번 다시 확인**한다.

- [ ] `BP_EasyMainGameHUD` → Parent Class = `ACBHUD`, 이벤트 2개 구현 **(누락 시 체력바가 안 뜨고 경고 로그만 남는다)**
- [ ] `GI_EasyMainGameInstance` → Parent Class = `UCBGameInstance`
- [ ] `BP_EGUI_GlobalConfigSelector` → `Global Config Data Asset` = `DA_CB_GlobalConfig`
- [ ] 복제본(`WBP_CB_MainMenu` 등)에 팩 버그픽스·개선 수동 반영 여부 판단
- [ ] 키바인딩·인풋 프롬프트가 빈칸이면 → `BPFL_EGUI_EasyFunctionUtilities`의 `Get Action Key Mappings From Mapping Context`에서 `Default Key Mappings` 핀 Split 후 배열 핀 재연결 (UE5.7+ 마이그레이션 픽스)
- [ ] JSON 노드가 사라지면 → 플러그인 로드 순서 이슈. 공식 문서 `additional-setup-system-testing/` 참조

> **미해결: 서브모듈의 수정 목록이 이 문서보다 많다.** `git -C Content/EasyGameUI status`에 10개가 수정으로 잡히는데, 여기 기록된 의도적 수정은 `BP_EasyMainGameHUD` / `BP_EGUI_GlobalConfigSelector` / `BPFL_EGUI_EasyFunctionUtilities`(마이그레이션 픽스) 셋뿐이다. 나머지가 의도한 변경인지 단순 5.8 리세이브인지 구분되지 않으면 **팩 업데이트 때 무엇을 다시 적용해야 할지 알 수 없다.** 한 번 정리해 이 목록을 실제와 맞출 것.

## 진행 상황

**완료 — 선행 세팅 + 기존 UI 이관**
- `GameInstanceClass` = `GI_EasyMainGameInstance`, GameMode의 HUD Class = `BP_EasyMainGameHUD`
- `UCBGameInstance` / `ACBHUD` 생성 후 팩 BP 2개 리페어런팅, 스택 이벤트 2개 구현
- `UCBUIComponent`의 HUD 체력 위젯을 `AddToViewport` → **`Game` 레이어 스택 삽입**으로 교체 (→ [UI.md](UI.md))
- 실행 확인: 서버·클라이언트 양쪽에서 `Easy Game HUD Initialized` 로그 확인
- `Enable User Settings`는 **UE 5.8 기본값이 True**라 별도 설정 불필요 (끄지 않았는지만 확인)

**완료 — 메인 메뉴 표시**
- `DA_CB_GlobalConfig` / `DA_CB_Styling` 생성 → `BP_EGUI_GlobalConfigSelector`에 연결
- `WBP_EasyMainMenu` → `WBP_CB_MainMenu` 복제
- `L_CB_MainMenu` + 메뉴 전용 GameMode(HUD Class = `BP_EasyMainGameHUD`)
- Config의 `Main Menu Widget` = `WBP_CB_MainMenu`
- **`BP_EasyMainMenuController`를 레벨에 배치** → 메인 메뉴 정상 표시 확인

> 설정을 전부 맞춰도 **메뉴 폰을 배치하지 않으면 아무것도 뜨지 않는다.** 에러도 안 난다 — 읽는 주체가 없을 뿐이기 때문이다. 위젯이 안 뜨면 **폰 배치부터 의심**할 것.

**완료 — 로비 UI (우리 고유 위젯 첫 사례)**

- `WBP_CB_Lobby_Main`(`Menu` 레이어) 안에 `WBP_CB_Lobby_PlayerMenu` / `WBP_CB_CosmeticSelector` / `WBP_CB_Lobby_Footer`
- 머리 위 배치는 `UWidgetComponent`가 아니라 **스택 위젯 + 월드 투영** — 버튼이 달린 위젯은 포커스·오클루전 관리를 받아야 하므로 스택 안에 있어야 한다 → [UI.md](UI.md)
- 의상 교체 2인 리슨 서버 검증 완료 → [Cosmetic.md](Cosmetic.md)

> 팩이 모르는 우리 위젯이므로 **Global Config에 등록하지 않는다.** 띄우는 쪽(레벨 배치 위젯 컨트롤러 BP)이 클래스를 들고 있고, 스택 삽입 경로만 지키면 된다 (아래 "로드아웃 vs Global Config").

## 미구현 (진행 예정)

**메인 메뉴 마무리** (공식 문서 `intro-screen-main-menu/` 기준)

- `WBP_CB_MainMenu` 버튼 정리 — **게임 시작 / 종료**만 남김 (옵션·크레딧·세이브 계열 비활성)
- 레벨에 `CameraActor`(또는 `CineCameraActor`) 배치 → 메뉴 폰의 `Target Camera Bindings`에 **버튼 ↔ 카메라** 연결·전환 시간 지정
- Config 나머지 값 — `Main Menu Level`, `New Game Level`, `Intro Screen Widget`**비움**, `Play Intro Credits On Game Launch?`=False, `Game Logo`
- `GameDefaultMap` = `L_CB_MainMenu` (`Editor Startup Map`은 개발 편의상 게임플레이 맵 유지)

**완료 — 게임플레이 IMC 차단**

- `Gameplay Input Mapping Contexts`에 `IMC_Default` 등록 (`Add Mapping Context on Game Load?` = **False**)
- 위젯의 `Should Remove Gameplay IMCs = true` 선언 → 메뉴 표시 중 캐릭터 정지, 닫으면 복구 확인
- **게임플레이 IMC 분할은 하지 않는다.** 지금은 `IMC_Default` 하나뿐이다. `IMC_Movement`/`IMC_Combat` 같은 분할은 **부분 차단**(예: 기절 시 공격만 금지) 요구가 실제로 생길 때 검토한다 — 나누는 만큼 등록 누락 위험이 늘어난다 → [Input.md](Input.md)

**UI 전용 IMC는 등록 대상이 아니다.** 로비 작업에서 `IMC_CB_UI`(+ `IA_CB_Ready` / `IA_CB_Leave`)가 생겼는데, 이것을 `Gameplay Input Mapping Contexts`에 넣으면 안 된다. 그 목록은 **"메뉴가 뜨면 걷어내도 좋다"는 권한 부여**이므로, UI 조작용 IMC를 넣으면 메뉴가 뜨는 순간 자기 자신이 걷혀 나간다. 등록하는 것은 **게임플레이 IMC뿐**이다.

**검증**: Standalone 실행 → 첫 버튼 포커스 / **Widget Reflector로 실제 클래스가 `WBP_CB_MainMenu`인지 확인**(`WBP_EasyMainMenu`가 뜨면 Selector 연결 누락) / 게임 시작 후 체력바 정상 표시 / 메뉴 중 캐릭터 정지(안 멈추면 IMC 등록 누락) / 리슨 서버 2인

### 메인 메뉴는 누가 띄우나 — `BP_EasyMainMenuController`

**이름과 달리 PlayerController가 아니라 Pawn이다.** `Player Controller Class`에 지정할 수 없고 **레벨에 배치**한다. `Target Camera Bindings`가 레벨의 카메라 액터를 가리키므로 클래스 기본값으로는 채울 수 없어 배치가 강제된다.

이 폰의 `BeginPlay`가 메인 메뉴 표시의 전부다:

```
BeginPlay
 ├ 카메라 유효성 검사 (Dev Only)
 ├ GetPlayerController(0) 캐싱 → Possess(self) → 초기 카메라 → 페이드 인
 │   → GetGlobalConfigDataAsset() 캐싱
 ├ ★ Delay Until Next Tick          ← 스택 조작 타이밍 가드
 ├ AddWidgetOfClassToStack(Config.IntroScreenWidget, Layer)
 └ Cast to WBP_EasyMasterWidget
     ├ 성공 → WidgetClosed 델리게이트 → IntroScreenClosed
     └ 실패 → IntroScreenClosed 즉시 호출     ← 인트로 스킵의 실체

IntroScreenClosed
 └ AddWidgetOfClassToStack(Config.MainMenuWidget, Layer)
     └ 유효? → RegisterMainMenuController
        아니면 → "Failed to construct Easy Main Menu... HUD Class ..." 에러 출력
```

여기서 읽어둘 것 셋:

- **인트로 스킵은 캐스트 실패로 구현되어 있다.** `Intro Screen Widget`을 비우면 위젯이 안 만들어지고, 캐스트가 실패하면서 곧장 메인 메뉴로 넘어간다
- **HUD 접근은 캐스팅이 아니라 인터페이스 메시지**(`PlayerControllerRef → Get HUD → BPI_EGUI_HUDInterface 메시지`)다. 우리 코드도 이 형태를 따른다
- 메뉴가 안 뜨는데 **"Failed to construct Easy Main Menu..."** 가 찍히면 HUD Class 쪽 문제고, **아무 메시지도 없으면** 이 폰이 레벨에 없는 것이다

#### 이 패턴은 메인 메뉴 레벨 전용이다

**"레벨에 배치된 액터가 위젯을 띄운다"를 게임플레이 레벨에 복사하면 안 된다.**

메뉴 레벨에서 성립하는 건 그곳이 **항상 단독 실행**이기 때문이다 — 메인 메뉴는 세션에 들어가기 *전* 단계고, 호스트는 여기서 리슨 서버를 열고 참가자는 곧장 게임플레이 레벨로 travel하므로 **메뉴 폰이 클라이언트로서 서버에 붙어 있는 상황 자체가 없다.** 게임플레이 레벨에서는 전제가 무너지고, 셋 다 조용히 어긋난다.

| # | 어긋나는 것 |
|---|---|
| ① | **레벨 배치 액터는 서버와 모든 클라이언트에 각각 존재**한다. `BeginPlay`도 각각 실행되므로 로컬 판별 없이 띄우면 서버 인스턴스에서도 실행된다 |
| ② | **`Possess`는 서버 권위 함수**다. 이 폰은 `BeginPlay`에서 `Possess(self)`를 호출한다 — 클라이언트에서는 무효고 이어지는 카메라 세팅도 어긋난다 |
| ③ | **`GetPlayerController(0)`은 "로컬 0번"**이지 "그 플레이어"가 아니다. 서버에서는 호스트 자신을 반환하며, 원격 플레이어의 PC로는 HUD를 얻을 수 없다 |

게임플레이 레벨의 정석은 → 위 "런타임에 띄울 때" + [UI.md](UI.md)의 "UI는 로컬 전용" 원칙. 로컬 전용 로직에는 `Local_` 접두사를 붙인다(→ [Multiplayer.md](Multiplayer.md)).

#### 호스트/참가 버튼

버튼 흐름을 확장할 지점은 이 폰(또는 `WBP_CB_MainMenu`)이 맞다. 다만 **접속 로직 자체를 폰이나 위젯이 들고 있으면 안 된다** — 메뉴 레벨을 떠나는 순간 함께 사라져 접속 실패 콜백을 받을 주체가 없어진다. 맵을 넘어 살아야 하므로 **`UCBSessionSubsystem`**(게임 인스턴스 서브시스템)이 그 자리이며, 위젯은 호출만 한다 → [GameFlow.md](GameFlow.md)

| 버튼 | 배선 |
|---|---|
| 호스트 | `Get Game Instance Subsystem (CB Session Subsystem)` → `Local_HostLobby(로비 레벨)` |
| 참가 | 같은 노드 → `Local_JoinServerByAddress(주소 입력값)` — 비워두면 `127.0.0.1` |
| 실패 표시 | 서브시스템의 `OnConnectionFailed`(사유 텍스트) 구독 → `Modals` 레이어 위젯 |

**로비 레벨은 Global Config의 `New Game Level`을 그대로 넘긴다.** 레벨 에셋 등록소는 Global Config이므로(위 "로드아웃 vs Global Config") C++이 경로를 따로 들고 있지 않는다.

**위젯 인터페이스**
- HUD 체력바 WBP에 `BPI_EGUI_UINavigationInterface` 구현 (3선언 함수 전부 `false`). 미구현이어도 인터페이스 메시지 호출은 기본값을 반환하므로 **동작에는 문제없다** — 의도 명시용

**추후**
- 옵션 메뉴 활성화 (버튼 하나 되돌리면 됨)
- `WBP_CB_MainMenu`에 호스트·참가 버튼 + 주소 입력란 배선 (C++ 창구는 완료, 위 "호스트/참가 버튼")

## 알려진 무해한 경고 (조치 불필요)

추적에 시간을 쓰지 않도록 원인이 규명된 것만 기록한다.

| 경고 | 원인 | 판단 |
|---|---|---|
| `LogBlueprintUserMessages: Failed to find console variable ''.`<br>(실행 시 HUD 개수만큼 반복) | **FSR 플러그인 미설치.** `BP_EasyMainGameHUD`가 시작 시 설정을 적용하며 `BPFL_EGUI_EasyFunctionUtilities::SetSharpness` → `MakeFsrConsoleCommand`를 호출하는데, FSR 버전이 `None`이면 **빈 CVar 이름**이 나오고 팩이 그걸 걸러내지 않고 조회한다 | **그대로 둔다.** CVar 조회 실패는 `false`를 반환하고 넘어가므로 기능 영향이 없다. 팩 원본을 고치면 업데이트 재적용 항목이 늘어나므로, 로그 한 줄과 맞바꾸지 않는다. FSR을 실제로 도입하면 자연히 해소된다 |

## 관련 문서

- 캐릭터 부착 UI: [UI.md](UI.md)
- 서버 권위·로컬 UI 원칙: [Multiplayer.md](Multiplayer.md)
- IMC·InputConfig: [Input.md](Input.md)
- 에셋 등록 원칙: [Loadout.md](Loadout.md)
