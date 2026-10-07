# 캐릭터 UI (체력바·이름표·플레이어 목록·보스 바)

> 캐릭터 부착형 UI(HUD 체력·머리 위 체력바)의 구조와 규칙. **UI는 각 클라이언트 로컬이며, 값 동기화는 어트리뷰트 리플리케이션이 전담한다 — UI를 위한 RPC/복제 코드를 만들지 않는다.**

## 새 UI는 어디로 — 배치 판단 기준

새 위젯을 만들 때 **가장 먼저** 정할 것. 세 갈래다.

| 위젯 성격 | 생성 주체 | 배치·관리 |
|---|---|---|
| **화면 + 캐릭터 데이터 필요**<br>(체력바, 추후 쿨타임·버프) | `UCBUIComponent` | 만들어서 **HUD 스택에 넘김** |
| **화면 + 캐릭터 무관**<br>(메뉴·옵션·확인창·로딩) | 요청자 또는 HUD<br>(클래스는 Global Config에 등록) | **HUD 스택** |
| **월드 + 조작 없음**<br>(머리 위 바, 오브젝트 체력바, 상호작용 프롬프트) | **그 UI가 따라다닐 액터** | `UWidgetComponent` — **스택 밖** |
| **월드 + 조작 필요**<br>(로비 머리 위 메뉴) | 그 위젯을 담은 화면 위젯 | **HUD 스택** + 위치만 월드 투영 |

판단은 세 질문으로 끝난다.

1. **월드의 특정 액터·지점을 따라다녀야 하나?**
   - 아니다 → 2번으로
   - 그렇다 → **버튼·포커스가 필요한가?**
     - 필요 없다(보기만 한다) → `UWidgetComponent`
     - **필요하다 → HUD 스택에 올리고 위치만 투영으로 계산** (아래 레시피)
2. (화면이라면) **캐릭터에서만 얻을 수 있는 데이터(ASC·로드아웃 위젯 클래스)가 필요한가?** → 그렇다면 `UCBUIComponent`가 생성

**"월드를 따라다니니까 `UWidgetComponent`"로 곧장 가면 안 된다.** 그 경로는 **스택 밖**이라 오클루전·포커스·게임플레이 IMC 제거가 전부 적용되지 않는다(아래 "주의"). 보기만 하는 체력바에는 문제가 없지만, **버튼이 달린 위젯은 그 자체가 메뉴**이므로 스택 안에 있어야 한다. 로비의 머리 위 메뉴가 이 경우다 → [Lobby.md](../Flow/Lobby.md)

**원칙 세 줄**

- 화면에 뜨는 것은 **예외 없이 HUD 스택**을 거친다 (`AddToViewport` / `Remove From Parent` 금지 → [EasyGameUI.md](EasyGameUI.md))
- **캐릭터 데이터가 필요할 때만** `UCBUIComponent`가 만든다
- 월드에 뜨는 것은 **그 UI가 붙을 액터**가 소유한다. 캐릭터의 경우 `UCBUIComponent`가 대행한다

**주의**

- Global Config는 "어떤 클래스를 쓸지"의 **등록소**이지 띄우는 주체가 아니다. 스택 삽입은 언제나 HUD 인터페이스(`BPI_EGUI_HUDInterface`) 호출로 일어난다.
- `UWidgetComponent` 경로는 **스택 밖**이라 오클루전·포커스·입력 관리가 적용되지 않는다. 숨김이 필요하면 직접 처리한다(`SetOverheadBarVisible`) — 숨기면 위젯이 파괴된다는 함정이 있다(→ 아래 "숨김의 함정 — 숨기면 위젯이 파괴된다").
- HUD 스택에 올릴 **로컬 플레이어 자신의 캐릭터 종속 위젯이 2개 이상** 되면 생성 주체를 `UCBUIComponent`에서 HUD로 옮기는 것을 재검토한다. 로드아웃 → HUD 전달 경로를 한 번 만들면 여러 위젯이 공유하므로 그 시점에 비용 대비 이득이 역전된다.
  - **보스 바는 이 셈에 들어가지 않는다.** 데이터(보스 ASC·위젯 클래스)가 로컬 플레이어가 아니라 **다른 액터(보스)** 에 있어, 보스의 UI 컴포넌트가 만드는 것이 위 판단 기준 그대로다. HUD가 만들게 하면 HUD가 보스를 찾아내는 경로가 따로 필요하다 (아래 "보스 바").

### 머리 위 배치 레시피 (스택 위젯 + 월드 투영)

스택 안의 위젯을 월드 액터 위에 붙일 때. 부모 위젯의 `Tick`에서 매 프레임 계산한다.

```
① 대상 폰            Get Owning Player Pawn        → 없으면 Hidden
② 머리 위 월드 좌표   ActorLocation + (0, 0, ScaledCapsuleHalfHeight + Margin)
③ 화면 좌표          Project World Location to Widget Position → 실패면 Hidden
④ 적용              Slot as Canvas Slot → Set Position + Visible
```

**②의 높이는 캡슐에서 구한다.** 숫자를 박으면 캡슐이 바뀔 때 조용히 어긋난다. 머리 소켓을 쓰면 애니메이션에 따라 위젯이 흔들리므로 대기 모션이 있는 화면에서는 캡슐이 안정적이다. `UCBUIComponent`의 머리 위 바가 같은 방식이다.

**③은 `…to Widget Position`을 쓴다.** 이름이 비슷한 `Project World Location to **Screen**`은 **DPI 보정이 없어** 고해상도에서 위젯이 미끄러진다. Widget Position 쪽은 뷰포트 스케일 나눗셈이 노드 안에 들어 있다.

**침묵하는 함정 셋** — 전부 에러 없이 위치만 어긋난다.

| 증상 | 원인 |
|---|---|
| 위젯이 화면 구석에 박혀 안 움직임 | **앵커가 좌상단(0,0)이 아니다.** `Set Position`은 절대 좌표가 아니라 **앵커 기준 오프셋**이다 |
| 위젯이 디자이너에 놓인 자리에 그대로 있음 | **직속 부모가 Canvas Panel이 아니다.** `Slot as Canvas Slot`이 `None`을 반환하고 `Set Position`이 조용히 무시된다 (Size Box 등으로 감싼 경우) |
| 위젯이 대상의 오른쪽 아래로 쏠림 | **Alignment가 (0,0)이다.** `(0.5, 1.0)`으로 두면 위젯 아래 중앙이 그 지점에 붙는다 |

크기는 **위젯 자신이 선언한다** — 배치하는 쪽에서 Size Box로 감싸지 말고 그 위젯의 root에 둔다. 감싸면 위의 두 번째 함정에 그대로 걸린다. 캔버스 슬롯은 `Size To Content`를 켠다.

> 배치 위젯을 변수로 쓰려면 디테일 패널의 **`Is Variable`을 직접 켜야 한다.** 이 프로젝트는 `bAuthorizeAutomaticWidgetVariableCreation=False`라 자동 생성되지 않는다.

## 왜 네트워크 코드가 필요 없나

- `CurrentHealth`/`MaxHealth`는 AttributeSet의 복제 프로퍼티라 **복제 모드(Minimal/Mixed)와 무관하게 전 클라에 항상 복제**된다(→ [ASC-Ownership.md](../Foundation/ASC-Ownership.md)). Minimal인 AI의 체력도 정상 표시된다.
- 각 클라이언트의 위젯은 `ASC->GetGameplayAttributeValueChangeDelegate()`에 구독만 하면 되고, 복제 값이 도착하면 클라이언트에서도 이 델리게이트가 발화한다.
- [Multiplayer.md](../Conventions/Multiplayer.md)의 "UI 띄우기 = 로컬 전용" 원칙 그대로, 서버는 UI에 관여하지 않는다.

## 구성 요소

| 클래스 | 역할 |
|---|---|
| `UCBHealthBarWidget` | 체력 위젯 공용 베이스(UUserWidget). `InitializeWithASC()`(`BlueprintCallable`)로 대상 ASC를 캐싱하고 구독 + 초기값 반영. **구독은 슬레이트가 있는 동안만, 대상 캐시는 대상 수명을 따른다** — 슬레이트 없이 초기화되면 캐시만 하고, `NativeDestruct`는 구독만 끊고 대상 캐시는 남기며, `NativeConstruct`에서 스스로 (재)구독한다(아래 함정 둘 참조). 비주얼은 `OnHealthChanged(Current, Max)` BP 이벤트로 WBP에 위임. 머리 위 바 WBP가 직접 상속하고, HUD에서는 이 위젯을 **자식으로 담는** HUD 컨테이너 WBP가 뜬다 |
| `UCBSkillSlotWidget` | HUD 스킬 슬롯 하나의 공용 베이스(UUserWidget). `EditDefaultsOnly` 쿨다운 태그 하나를 대상으로 카운트 변화를 구독해 쿨다운 시작·종료를 감지하고, 쿨다운 중에는 매 틱 활성 GE에서 남은 시간·전체 길이를 조회해 진행률을 계산. 비주얼은 `OnCooldownStarted` / `OnCooldownProgress(Progress, RemainingTime)` / `OnCooldownEnded` BP 이벤트로 WBP에 위임하므로 C++는 위젯 구성(서드파티 프로그레스바 등)을 알지 않는다. 구독 수명 계약은 `UCBHealthBarWidget`과 동일 |
| `UCBBurstGaugeWidget` | HUD **버스트 게이지**의 공용 베이스(UUserWidget). 한 바가 두 모드로 동작 — 충전 중에는 `BurstGauge` 어트리뷰트를 구독해 `OnBurstGaugeChanged(Current, Max)`, 버스트 중에는 `Status.Combat.Burst` 태그로 시작·종료를 감지하고 매 틱 버스트 GE 남은 시간으로 `OnBurstProgress(RemainingRatio, RemainingTime)`. 두 모드의 이벤트는 섞이지 않는다(아래 "버스트 게이지 표시"). 발동 가능 여부가 바뀌면 `OnBurstReadyChanged(bIsReady)`(BlueprintAssignable)를 방송해 HUD 등 외부 위젯이 구독한다. 구독 수명 계약은 `UCBHealthBarWidget`과 동일 |
| `UCBStunGaugeWidget` | **기절 게이지**의 공용 베이스(UUserWidget). `StunGauge` 어트리뷰트를 구독해 `OnStunGaugeChanged(Current, Max)` BP 이벤트로 위임. 최대값은 어트리뷰트가 아니라 상수 `UCBAttributeSet::MaxStunGauge`(→ [Stun.md](../Gameplay/Stun.md)). 구독 수명 계약은 `UCBHealthBarWidget`과 동일 |
| `UCBExperienceBarWidget` | **HUD 경험치 바**의 공용 베이스(UUserWidget). `ACBGameplayGameState::OnExperienceChanged`를 구독해 `OnExperienceChanged(Level, Current, Required)` BP 이벤트로 위임. 필요량은 복제하지 않고 게임 인스턴스의 레벨업 데이터에서 계산(데이터가 없으면 0). 게임 스테이트가 늦게 오면 `GameStateSetEvent`로 기다렸다 구독(`UCBPlayerListWidget`과 같은 방식), 게임플레이 레벨이 아니면 아무것도 안 함. HUD 컨테이너 WBP 안에 배치 → [LevelUp.md](../Gameplay/LevelUp.md) |
| `UCBLevelUpWidget` | **레벨업 카드 선택 창**의 공용 베이스(UUserWidget). 캐릭터 데이터가 필요 없어 `UCBUIComponent`가 아니라 **소유 컨트롤러(`ACBChaserController`)가 생성**해 `Menu` 레이어에 올린다 — 폰이 바뀌어도 살아 있어야 리스폰 중에도 카드를 받는다. 위젯은 표시(`OnCardsOffered`)와 선택 전송(`Local_SelectCard` → 서버 RPC)만 하고, 남은 시간은 월드 정지 중에도 흐르는 실제 시간으로 센다 → [LevelUp.md](../Gameplay/LevelUp.md) |
| `UCBBossBarWidget` | **화면 상단 보스 바**의 컨테이너 베이스(UUserWidget). `InitializeWithASC()` 하나로 자식 `HealthBarWidget`(`UCBHealthBarWidget`)·`StunGaugeWidget`(`UCBStunGaugeWidget`)을 배선한다(`BindWidgetOptional` — 플레이어 목록 행과 같은 패턴). 값 구독은 자식이 각자 한다 |
| `UCBNamePlateWidget` | **로비 발밑 이름표**의 공용 베이스(UUserWidget). `InitializeWithPlayerState()`(`BlueprintCallable`)로 대상 PlayerState를 캐싱하고 `OnPlayerNicknameChanged` 구독 + 현재 이름 반영. 비주얼은 `OnNicknameChanged(Nickname)` BP 이벤트로 WBP에 위임. `IsLocalPlayerTarget()`로 자기 이름표만 다르게 꾸밀 수 있다. **구독 수명 계약은 `UCBHealthBarWidget`과 동일** |
| `UCBPlayerListWidget` | **HUD 플레이어 목록**의 공용 베이스(UUserWidget). `ACBGameStateBase::OnPlayerListChanged`를 구독해 행을 다시 만들고, 자기 자신과 봇은 제외한다. 행을 담을 패널은 WBP에서 `EntryContainer` 이름으로 배치(`BindWidget`). 게임 스테이트 복제가 위젯보다 늦으면 `UWorld::GameStateSetEvent`로 기다렸다 다시 구독하고, 로컬 PlayerState 확정은 `ACBChaserController::OnLocalPlayerStateSet`으로 기다린다(아래 "자기 자신 제외") |
| `UCBPlayerListEntryWidget` | 목록의 **행 하나**. `InitializeWithPlayerState()` 하나로 이름표·체력바 자식을 배선한다. 자식은 `BindWidgetOptional`이라 WBP에 같은 이름으로 두면 자동 연결되고 없으면 그 부분만 생략 |
| `UCBUIComponent` | 캐릭터 부착형 UI의 공용 관리자(`UCBExtensionComponent` 상속, `ACBBaseCharacter`가 소유 — 전 캐릭터 공통). 준비 완료 훅에서 오너 유형별로 위젯을 생성·소유하고, 머리 위 바의 표시 정책(피격 표시·유지 시간·거리 숨김)도 이 컴포넌트가 쥔다. 보스면 교전 개시 태그를 구독해 보스 바를 로컬 HUD 스택에 올린다. HUD 위젯 캐시는 `TSubclassOf<UUserWidget>`(체력바만이 아닌 임의 HUD 컨테이너 허용), 머리 위 바 캐시는 `UCBHealthBarWidget`로 좁게 유지 |
| `ICBUIInterface` | UI 접근 인터페이스. `GetCBUIComponent()` 하나만 가진 얇은 형태(`ICBCombatInterface` 선례). `ACBBaseCharacter`가 구현하며, 캐릭터가 아닌 액터(파괴 가능 오브젝트 등)에 체력바가 필요해지면 그쪽도 구현 |
| `ACBHUD` | 프로젝트 공용 HUD 베이스. **EasyGameUI 팩 HUD(`BP_EasyMainGameHUD`)의 C++ 부모**로 리페어런팅되어 있다. 팩의 스택 조작은 BP 인터페이스로만 가능하므로 `PushGameLayerWidget` / `PushMenuLayerWidget` / `PopWidgetFromStack`(`BlueprintImplementableEvent`)만 선언한 다리다 (→ [EasyGameUI.md](EasyGameUI.md)) |

## 생성 흐름 (준비 완료 후)

`UCBUIComponent::OnCharacterSystemReady()`에서 분기 — 준비 전에는 ASC/위젯 클래스가 미완성일 수 있으므로 반드시 준비 신호를 기다린다(→ [SystemReady.md](../Foundation/SystemReady.md)):

- **로비 게이트**: `GetWorld()->GetGameState<ACBLobbyGameState>()`가 유효하면(= 지금 로비) HUD·머리 위 바 대신 **발밑 이름표만 만들고 리턴**한다(아래 "로비 발밑 이름표"). 로비도 무기 선택 때 폰을 재스폰하므로 로드아웃→준비 완료 경로를 그대로 타지만, 로비 화면은 `WBP_CB_Lobby_*` 전용 위젯이 담당한다. WBP에서 숨기지 않고 여기서 막는 이유: 머리 위 바(순수 C++ 경로)까지 한 곳에서 커버되고, 위젯 인스턴스·ASC 바인딩·스택 삽입이 아예 일어나지 않는다. `ACBLobbyGameState`는 로비 전용 클래스라 존재만으로 신호가 되고, 클라에 폰보다 먼저 복제된다.
- **로컬 조작 플레이어** (`IsPlayerControlled() && IsLocallyControlled()`): HUD 위젯 생성 → **`ACBHUD`를 통해 HUD 스택의 `Game` 레이어에 삽입**. AI는 서버에서 `IsLocallyControlled()`가 참이므로 `IsPlayerControlled()` 병행 검사가 필수.
	- **ASC 바인딩은 컴포넌트가 하지 않는다.** HUD 위젯은 체력바를 내부에 담는 컨테이너이므로, 자식 체력바의 `InitializeWithASC()` 호출은 HUD WBP가 직접 수행한다:
		- 자식 체력바(`WBP_CB_HealthBar`)를 `Is Variable`로 노출 (프로젝트가 `bAuthorizeAutomaticWidgetVariableCreation=False`라 수동).
		- `Event Construct` → `Get Owning Player Pawn` → `Get Ability System Component`(폰이 `IAbilitySystemInterface` 구현) → `Cast To CBAbilitySystemComponent` → 자식 체력바의 `Initialize With ASC`.
		- 타이밍: 컴포넌트는 `OnCharacterSystemReady()`에서만 HUD 위젯을 만들므로 `Event Construct` 시점엔 PlayerState ASC가 캐싱·초기화 완료 상태. 폴링 불필요.
	- 머리 위 바는 종전대로 컴포넌트(`CreateOverheadWidget`)가 `InitializeWithASC()`를 호출한다 — 그쪽은 World outer라 `Get Owning Player Pawn`이 소유자가 아닌 관전자 폰을 반환하므로 자기 초기화로 통일하면 안 된다.
	- 삽입·제거는 `Local_PushWidgetToStack(PC, Widget)` / `Local_RemoveWidgetFromStack(HUD, Widget)`가 담당한다(HUD 위젯·보스 바 공용). **`AddToViewport` / `RemoveFromParent`를 쓰면 안 된다** — 스택 배열에 항목이 남아 위젯이 화면에 그대로 남는다.
	- 제거 시점엔 컨트롤러 연결이 이미 끊겼을 수 있어(사망 후 파괴 등) **삽입 시점의 HUD를 `CachedHUD`에 캐싱**해 제거에 쓴다.
	- **`EndPlay`에서 스택 제거는 `EEndPlayReason::Destroyed`일 때만** 한다. 폰만 사라지고 HUD는 남는 경우(사망·무기 변경 재스폰)가 유일하게 정리가 필요한 상황이고, 월드가 통째로 끝나는 경우(맵 전환·PIE 종료)엔 HUD도 함께 파괴돼 정리가 무의미하다. 가드가 없으면 **2인 PIE 종료 시** 클라이언트 쪽 PlayerController가 먼저 정리된 뒤 pop이 불려, 팩의 스택 재계산이 PC 조회에 실패한다(`Unregister Input Listener`의 `RemoveMappingContext`에서 Accessed None). 1인 PIE는 클라이언트 월드가 없어 재현되지 않는다.
	- HUD 캐스팅이 실패하면(리페어런팅 누락·HUD Class 오지정) 경고 로그를 남기고 생성을 건너뛴다.
- **그 외 (AI 전부 + 원격 캐릭터)**: `UWidgetComponent`(Screen 모드, DrawAtDesiredSize)를 런타임 생성해 루트에 부착. 부착 높이는 `캡슐 상단 + OverheadZMargin` 자동 계산(로드아웃 바디 셋업 적용 후 시점이라 캡슐 크기 확정 상태). 원격 팀원(다른 Chaser)도 머리 위 바가 뜨며, `bShowOverheadBar`로 끌 수 있다. 생성 직후 표시 여부는 아래 "표시 정책"을 따른다.
	- **보스 바 위젯 클래스가 주입돼 있으면(보스)** 교전 개시 태그 구독도 건다 → 아래 "보스 바".

## 머리 위 바 표시 정책 (평소 숨김)

머리 위 바는 **평소 숨김**이고 **맞았을 때만** 잠깐 뜬다. **표시는 거리 안의 모든 클라이언트에서 각자 일어나고, 숨김도 각자 로컬 판단**이다. 새 네트워크 코드는 없다.

| 전환 | 신호 | 구현 |
|---|---|---|
| 숨김 → 표시 | **오너의 `CurrentHealth` 감소 + 거리 안** | `UCBUIComponent`가 오너 ASC의 어트리뷰트 변경 델리게이트를 구독. `NewValue < OldValue`이고 `IsWithinOverheadBarDistance()`가 참일 때만 표시 |
| 표시 → 숨김 | **유지 시간 만료** | `OverheadBarShowDuration`(기본 5초) 원샷 타이머. 표시 중에 다시 맞으면 `SetTimer`가 덮어써 시간이 리셋된다 |
| 표시 → 숨김 | **오너 사망** | `ACBBaseCharacter::OnCharacterDiedDelegate` 구독 → 피격 구독을 끊고 즉시 숨김. 사망 자체는 복제되는 `Status.Dead`가 알려주므로 시뮬 프록시 화면에서도 시체 위에 바가 남지 않는다 (→ [Abilities.md](../Gameplay/Abilities.md)) |
| 표시 → 숨김 | **거리 이탈** | `OverheadBarDistanceCheckInterval`(기본 0.2초) 반복 타이머가 같은 거리 판정을 돌려 벗어나면 숨김 |

거리 기준은 `OverheadBarVisibleDistance`(기본 2000cm) 하나를 **표시 게이트와 숨김 검사가 공유**한다. 설정값은 전부 `UCBUIComponent`의 `EditDefaultsOnly`라 캐릭터 BP별로 조정한다. **`bHideOverheadBarUntilDamaged`를 끄면** 종전처럼 생성 직후부터 항상 표시된다(보스 등 예외용 + 롤백 스위치).

**왜 이 구조인가**

- 표시 트리거를 "때린 쪽이 호출"이 아니라 **맞은 쪽의 어트리뷰트 구독**으로 잡았다. `CurrentHealth`는 복제 프로퍼티라 그 액터가 리플리케이트되는 **모든 클라이언트에서 각자 델리게이트가 발화**한다 — 때린 사람뿐 아니라 주변 플레이어 전원에게 뜨는 게 이 구조에서 공짜로 나온다. 위의 "게임플레이가 원인인 UI 변화는 복제 신호를 UI가 구독" 규칙을 그대로 만족하고 RPC도 필요 없다. 대신 **데미지 0(빗맞음·완전 방어)은 바가 뜨지 않는다** — 뜨게 하려면 피격 GameplayCue 같은 별도 복제 신호를 함께 구독해야 한다.
- **표시에도 거리 게이트를 건다.** 없으면 멀리 있는 클라이언트도 일단 바를 켠 뒤 다음 거리 검사 타이머(최대 0.2초)에서야 꺼서, 먼 곳에서 바가 한 번 깜빡였다 사라진다. 표시와 숨김이 같은 판정 함수를 쓰므로 이 틈이 생기지 않는다.
- 구독은 `InitializeWithASC()`(초기값 반영) **이후**에 건다. 초기화 과정의 값 변동으로 생성 직후 바가 번쩍이는 걸 막는다.
- 표시/숨김 거리를 하나로 둔 만큼 경계선에 정확히 서 있으면 맞을 때마다 켜졌다 한 틱 뒤 꺼지는 미세한 깜빡임이 남는다. 히스테리시스(표시 거리 < 숨김 거리)로 없앨 수 있지만, 필드를 하나 더 두는 만큼의 값어치가 없다고 보고 단일 거리로 뒀다.
- 거리 검사는 **표시 중일 때만** 타이머를 돌리고 숨길 때 정지한다. 숨어 있는 대다수 AI가 상시 비용을 갖지 않는다. 그 대가로 **거리 때문에 숨은 바는 다시 가까이 가도 저절로 켜지지 않는다** — 다음 피격이 트리거다.
- 거리 기준점은 카메라가 아니라 **로컬 플레이어 폰**이다(폰이 없으면 카메라로 대체). 3인칭 스프링암 길이만큼 기준이 뒤로 밀리지 않게 하기 위함.
- 공개 함수 `SetOverheadBarVisible()`은 그대로 남아 있지만, 이 정책이 켜져 있으면 **이후의 피격·타이머·거리 판단이 그 값을 덮어쓴다.**

### 숨김의 함정 — 숨기면 위젯이 파괴된다 (Screen 모드 `UWidgetComponent`)

에러 없이 조용히 죽는다. 엔진 코드(`UWidgetComponent::TickComponent` → `UpdateWidgetOnScreen`)를 따라가야 보인다.

컴포넌트가 보이지 않게 되면 다음 틱에 `RemoveWidgetFromScreen()`이 불려 위젯이 스크린 레이어에서 빠지고, `SObjectWidget`이 파괴되며 **`UUserWidget::NativeDestruct()`가 호출된다.** 여기서 어트리뷰트 구독을 해제하면서 **대상 캐시까지 비우면 다시 보일 때 되살릴 방법이 없어**, 바는 정상적으로 다시 뜨지만 값이 처음 그대로 멈춘다.

그래서 `UCBHealthBarWidget`은 `NativeDestruct`에서 **핸들만 해제**하고 `CachedASC`는 남긴 뒤, `NativeConstruct`에서 `BindToASC()`로 재구독 + 현재 값 반영을 한다.

숨김 경로마다 호출자가 `InitializeWithASC()`를 다시 불러주는 방식으로도 되지만, 그러면 `SetOwnerPlayer` 변경이나 게임 레이어 재구성처럼 **우리가 부르지 않은 제거 경로**에서 그대로 죽는다. 위젯이 자기 계약을 지키게 두는 편이 낫다.

### 첫 표시의 함정 — 슬레이트 없이 구독하면 1타가 삼켜진다

증상: 평소 숨김인 머리 위 바가 **첫 피격에 가득 찬 채로** 뜨고, 2타부터 정상 반영된다.

서드파티 프로그레스 바는 **슬레이트 생성 전에 받은 값을 머티리얼 없이 저장만 하고, 생성 후 같은 값이 오면 무시한다.** 그래서 슬레이트보다 먼저 값이 한 번이라도 들어가면 머티리얼이 기본값으로 굳는다.

머리 위 바는 숨긴 채 생성되므로(위 "생성 순서의 함정") `InitializeWithASC()` 시점에 슬레이트가 없다. 이때 구독까지 걸어 두면 첫 피격에서:

1. 위젯의 체력 구독이 발화 → 슬레이트 없는 바에 새 체력이 들어감 (머티리얼 미반영)
2. UI 컴포넌트가 `SetVisibility(true)` — 하지만 화면에 올리는 건 **다음 틱**(`TickComponent` → `UpdateWidget` → `AddWidgetToScreen`)
3. 다음 틱 슬레이트 생성 → `NativeConstruct`가 같은 값을 다시 보냄 → 무시됨

화면 추가가 항상 다음 틱이라 두 구독의 발화 순서와 무관하게 매번 재현된다. 한 번 숨겨진 뒤에는 `NativeDestruct`가 구독을 끊어 두므로 **첫 표시에서만** 생긴다.

그래서 `BindToASC()`는 슬레이트가 없으면 **초기값 반영뿐 아니라 구독도 미룬다.** 구독은 `NativeConstruct`에서 걸리고, 그때 최신 값을 한 번 보낸다. 결과적으로 구독은 슬레이트가 있는 동안만 존재해 `NativeDestruct` 쪽과 짝이 맞는다. HUD 플레이어 목록처럼 초기화 후 슬레이트를 만드는 경로의 같은 틈도 함께 막힌다.

> 버스트 게이지·스킬 슬롯은 구조상 같은 틈이 있지만 HUD `Event Construct`에서 초기화되어(항상 슬레이트 있음) 도달하지 않으므로 그대로 두었다. 슬레이트 생성 전에 초기화하는 경로가 생기면 같은 방식으로 옮길 것.

### 생성 순서의 함정 — `SetWidget()`은 그 자리에서 화면에 올린다

**`SetWidget()`을 부르기 전에 위치를 잡고, 숨길 것이면 숨겨 놓아야 한다.** 순서를 지키지 않으면 소환 직후 한 프레임 깜빡인다.

`UWidgetComponent::SetWidget()`은 내부에서 `UpdateWidget()` → **`UpdateWidgetOnScreen()`을 즉시 호출**하고, 그 함수는 `IsVisible()`이 참이면 곧바로 `AddWidgetToScreen()`을 부른다. `USceneComponent::IsVisible()`은 **등록 여부를 보지 않고 `bVisible`만** 보므로, `RegisterComponent()` 전이라도 새로 만든 컴포넌트(`bVisible` 기본값 `true`)는 전부 통과한다. 반대로 **제거는 즉시 일어나지 않는다** — `RemoveWidgetFromScreen()`은 `TickComponent` 경로에만 있어 다음 틱으로 밀린다.

그래서 "붙이고 → 위치 잡고 → 숨김" 순서로 쓰면 **첫 프레임에 캐릭터 원점 자리에서 한 번 보이고** 사라진다. AI가 여러 마리 동시에 소환될 때 눈에 띈다.

```
NewObject → SetWidgetSpace / SetDrawAtDesiredSize
          → SetRelativeLocation      ← 위치 먼저
          → 숨길 판단 후 SetVisibility(false)   ← 숨김도 먼저
          → SetWidget                ← 여기서 화면에 올라감 (숨겨져 있으면 안 올라감)
          → RegisterComponent
```

머리 위 바(`bHideOverheadBarUntilDamaged`)와 이름표(거리 컬링 첫 판정 `StartNamePlateDistanceCheck()`) 둘 다 이 순서를 지킨다.

> **위젯 에셋(WBP)의 Visibility를 숨김으로 두는 것으로 대신하면 안 된다.** `SetOverheadBarVisible()`은 **컴포넌트만** 토글하고 유저 위젯 자체의 Visibility는 건드리지 않아, 피격해도 영영 뜨지 않는다.

> **`TickMode`를 바꾼다면 숨김도 `SetHiddenInGame`으로 바꿔야 한다.** 지금은 `TickMode` 기본값이 `Enabled`(CVar `WidgetComponent.UseAutomaticTickModeByDefault`가 false)라 숨겨도 컴포넌트 틱이 계속 돌고, `SetVisibility(true)`로 되돌리면 같은 경로로 다시 붙는다. 하지만 숨은 바의 틱 비용을 없애려고 `TickMode`를 `Automatic`으로 바꾸면 `!IsWidgetVisible()` 분기가 살아나 틱이 스스로 꺼지는데, 이를 되살리는 훅은 `OnHiddenInGameChanged()`(=`SetHiddenInGame`)와 **유저 위젯**의 `OnNativeVisibilityChanged` 뿐이다. 컴포넌트의 `SetVisibility`에는 훅이 없어 다시 켜도 안 붙는다.

## 로비 발밑 이름표

로비에서 서로의 닉네임을 캐릭터 발밑에 띄운다. **머리 위 체력바와 같은 경로**(`UCBUIComponent`가 만드는 Screen 모드 `UWidgetComponent`)이고, 방향만 아래로 뒤집힌 것이다.

| | 머리 위 체력바 | 발밑 이름표 |
|---|---|---|
| 부착 높이 | `+(캡슐 상단 + OverheadZMargin)` | `-(캡슐 하단 + NamePlateZMargin)` |
| 대상 데이터 | ASC의 체력 어트리뷰트 | **PlayerState의 `PlayerName`** |
| 표시 정책 | 평소 숨김, 피격 시 일정 시간 | **항상 표시 + 거리 컬링** (로비는 컬링도 없음) |
| 만드는 레벨 | 게임플레이 | **로비·게임플레이 공통** |
| 대상 | 자기 자신 외 전원 + AI | 게임플레이는 **자기 자신 제외**, 로비는 자기 것도 표시 |

**버튼이 없으니 `UWidgetComponent`다.** 스택(`WBP_CB_Lobby_Main`)에 올려 위치만 투영하는 길도 있지만, 그러면 인원이 들고 날 때 위젯 풀을 BP에서 직접 관리해야 한다. 컴포넌트로 두면 캐릭터에 수명이 묶여 **무기 변경 재스폰에도 새 폰이 자기 이름표를 새로 만들고 옛 것은 폰과 함께 사라진다.** 위 "새 UI는 어디로" 판단 기준 그대로다.

### PlayerState 도착 순서는 이미 보장된다 — 추가 훅이 필요 없다

이름의 출처가 PlayerState라 "폰이 먼저 도착하면 이름이 빈다"를 걱정하기 쉽지만, **Chaser는 초기화 진입점 자체가 PlayerState를 전제**하므로 준비 완료 시점엔 언제나 유효하다.

| | 초기화 진입점 | PlayerState 상태 |
|---|---|---|
| 서버 | `ACBChaserCharacter::PossessedBy` | `Super::PossessedBy`(`APawn`)가 **바로 앞에서** `SetPlayerState(Controller->PlayerState)` |
| 클라이언트 | `ACBChaserCharacter::OnRep_PlayerState` | **도착했기 때문에** 이 함수가 불린 것 |

`ACBBaseCharacter`에는 `BeginPlay` 오버라이드가 없고 초기화는 위 두 곳에서만 시작하므로, `Ready`는 정의상 PlayerState가 있는 상태에서만 도달한다. 그래서 `OnRep_PlayerState`에서 UI 컴포넌트에 다시 알리는 훅을 **넣었다가 걷어냈다** — 아무 일도 하지 않는 경로만 늘었다.

`CreateNamePlateWidget()`의 "PlayerState 없으면 리턴" 가드는 남아 있다. 위 보장이 깨지거나 **PlayerState가 없는 대상(AI)** 이 들어와도 크래시 대신 이름표만 생기지 않게 하는 자리이며, AI를 거르는 별도 분기가 따로 없는 이유이기도 하다.

> 게임플레이 레벨로 이름표를 넓힐 때는 이 전제를 다시 확인할 것. 리스폰·재빙의로 **PlayerState가 교체되는 경로**가 생기면 그때는 알림 훅이 실제로 필요해진다.

### 레벨별 정책 — 같은 위젯, 다른 규칙

이름표는 두 레벨에서 같은 경로로 만들어지지만 규칙이 갈린다. 판단은 `CreateNamePlateWidget()` 안에서 게임 스테이트로 한다(`ACBLobbyGameState`가 있으면 로비).

| | 로비 | 게임플레이 |
|---|---|---|
| 자기 이름표 | **띄운다** — 라인업에서 자기 자리를 확인해야 한다 | **띄우지 않는다** — 3인칭 카메라가 자기 캐릭터에 붙어 있어 화면 한가운데를 계속 차지한다 |
| 거리 컬링 | 없음 (전원이 가까이 모여 있다) | `NamePlateVisibleDistance`(로드아웃, 기본 1500cm) 밖이면 숨김 |
| 체력 UI | 만들지 않음 | HUD(로컬) / 머리 위 바(그 외) |

**거리 컬링은 머리 위 바와 정책이 다르다.** 바는 "평소 숨김 → 피격 시 일정 시간 표시"라 표시 중일 때만 거리 타이머를 돌리지만, 이름표는 "항상 표시 → 거리로만 숨김"이라 살아 있는 동안 계속 돈다. 그래서 타이머는 각자 두고, **거리 판정 함수만 공유**한다(`IsWithinDistanceFromLocalViewer(기준 거리)`).

**팀 필터는 넣지 않았다.** 지금은 플레이어끼리 모두 같은 편(추격자)이라 "전원 표시"와 "아군만 표시"가 같은 결과다. PvP가 생기면 `ECBTeam` 조건을 생성 단계에 추가한다 (→ [Teams.md](../Foundation/Teams.md)).

### 자기 이름표 판정 — 복제 '참조'가 아니라 폰의 넷 롤로 한다

내 이름표만 다르게 꾸미려면 "이 이름표의 주인이 나인가"를 알아야 하는데, **위젯이 스스로 알아내려 하면 클라이언트에서 틀린다.** 실제로 밟았던 증상: 호스트에서는 자기 이름표가 `true`인데 **클라이언트에서는 자기 것도 `false`** 였고, 1초 뒤에 다시 물으면 `true`였다.

위젯이 쓸 수 있는 단서가 전부 **따로 복제되는 참조**이기 때문이다.

| 단서 | 문제 |
|---|---|
| `GetOwningPlayer()` | 위젯 생성 시점의 `PlayerContext`에 고정된다 |
| `PC->PlayerState` | PlayerController 액터의 프로퍼티. 폰의 것과 **도착 순서 보장 없음** |
| `Pawn->Controller` | 이것도 `DOREPLIFETIME`이라 같은 문제 |

특히 `PC->PlayerState`는 접속 순서상 **PlayerState 액터보다 PC 채널이 먼저 열려** 참조가 unmapped 상태로 보류됐다가 `UNetDriver::UpdateUnmappedObjects()`가 나중에 채운다. 반면 폰은 나중에 스폰돼 그 시점엔 PlayerState 액터가 이미 있으니 즉시 해석된다 — 그래서 **폰 쪽이 먼저 완성되는** 역전이 생긴다.

**`OnCharacterSystemReady`는 이 문제를 해결해 주지 않는다.** 그 신호는 "이 폰의 로드아웃 비동기 로드가 끝났다"는 뜻일 뿐이고, 시작점이 폰의 PlayerState 도착이라 **에셋이 캐시돼 있으면 그 직후 1~2프레임 만에** 떨어진다. 네트워크 전반의 도착 상태와는 무관하다 (→ [SystemReady.md](../Foundation/SystemReady.md)).

그래서 판정은 **UI 컴포넌트가 자기 오너 캐릭터를 보고** 내려서 위젯에 주입한다. 컴포넌트가 쥔 캐릭터 포인터는 복제를 거치지 않는 로컬 참조다.

```cpp
// UCBUIComponent::CreateNamePlateWidget()
const bool bIsLocalTarget = OwnerCharacter->IsLocallyControlled()                 // 호스트: PossessedBy 에서 동기 설정
    || OwnerCharacter->GetLocalRole() == ROLE_AutonomousProxy;                    // 클라: 내 폰만 AutonomousProxy

NamePlateWidget->InitializeWithPlayerState(OwnerPlayerState, bIsLocalTarget);
```

**넷 롤은 참조가 아니라 액터와 함께 오는 값**이라 폰이 존재하면 이미 확정이다. 호스트에서는 모든 폰이 `Authority`라 롤로 구분되지 않으므로 빙의 여부가 그 자리를 메운다 — 두 조건이 서로의 빈틈을 덮는다.

주입은 **위젯 컴포넌트를 등록하기 전에** 한다. Screen 모드 위젯은 다음 틱에 화면에 붙으므로 지금도 `Construct`보다 앞서지만, 순서를 코드로 못박아 둔다. 덕분에 WBP는 `Event Construct`에서 `IsLocalPlayerTarget()`을 그냥 읽으면 되고, 숨겨졌다 다시 떠도 값이 위젯 객체에 남아 있어 표시가 복구된다.

## HUD 플레이어 목록 (남의 닉네임·체력)

게임플레이 HUD에서 **자기 자신을 제외한 플레이어**의 닉네임과 체력을 보여준다. 새 네트워크 코드는 없다 — 목록도 값도 전부 복제된 데이터를 각 클라이언트가 로컬로 읽는다.

### PlayerState 하나면 이름과 체력이 다 나온다

Chaser는 **ASC를 PlayerState가 소유**하므로(→ [ASC-Ownership.md](../Foundation/ASC-Ownership.md)) 남의 폰을 찾을 필요가 없다.

```
GameState->PlayerArray  →  각 ACBPlayerState
                            ├ GetPlayerName() + OnPlayerNicknameChanged  →  UCBNamePlateWidget
                            └ GetCBAbilitySystemComponent()              →  UCBHealthBarWidget::InitializeWithASC()
```

**이름표 위젯을 그대로 재사용한다.** `UCBNamePlateWidget`은 발밑 전용이 아니라 "PlayerState의 이름을 구독해 표시"하는 클래스라, HUD 행 안에 자식으로 넣어도 그대로 동작한다(목록에는 남만 오르므로 `bIsLocalTarget = false`).

**폰이 죽거나 다시 스폰돼도 구독이 끊기지 않는다.** ASC가 PlayerState와 함께 살아남기 때문이며, 폰에 붙는 머리 위 바에는 없는 이점이다.

### 목록 변경 신호 — `ACBGameStateBase`

`AGameStateBase`에는 "플레이어 목록이 바뀌었다"는 신호가 없어, 위젯이 매 틱 `PlayerArray`를 훑어 diff해야 한다. 그래서 공용 베이스를 두고 엔진 훅에서 한 번만 방송한다.

```cpp
void ACBGameStateBase::AddPlayerState(APlayerState* PlayerState)    // 서버·클라 공통
{
    Super::AddPlayerState(PlayerState);
    OnPlayerListChanged.Broadcast();                                 // 누가 들어왔는지는 전달하지 않음
}
```

`ACBGameModeBase` 생성자가 `GameStateClass`를 이 클래스로 고정하고(누락 시 조용히 신호가 사라지므로 BP에 맡기지 않는다), `ACBLobbyGameState`가 이를 상속한다.

**행은 diff하지 않고 통째로 다시 만든다.** 인원이 적고 변경이 드물어 diff 로직의 복잡도가 값어치가 없다. 행이 파괴되면 자식 위젯의 구독은 각자 `NativeDestruct`에서 정리된다.

### 자기 자신 제외 — 목록 신호만으로는 부족하다

제외 기준은 `GetOwningPlayer()->PlayerState`인데, 클라이언트에서는 **이 값이 목록 변경 방송보다 항상 늦게 확정된다.** 순서를 보면 우연이 아니라 구조다.

1. 로컬 PlayerState 액터가 복제로 스폰됨 → 자기 `PostInitializeComponents`에서 `AddPlayerState` 호출 → **그 안에서 `OnPlayerListChanged` 즉시 방송**
2. 위젯이 갱신되는데, `PC->PlayerState`는 **다른 액터(PC)의 프로퍼티**다. PlayerState 액터가 존재하기 전에는 참조가 풀리지 않아 패키지맵이 나중에 채운다 → 이 시점엔 아직 `nullptr`
3. 자기 자신이 행으로 들어감
4. `PC->PlayerState`가 채워져도 **목록 변경 방송은 다시 오지 않는다**

"신호가 올 때마다 다시 평가되니 자기 복구된다"는 건 **뒤에 다른 사람이 들어오거나 나갈 때만** 성립한다. 혼자 접속한 경우엔 잘못된 행이 영영 남는다.

그래서 신호를 하나 더 만들었다. `ACBChaserController::OnRep_PlayerState`가 `OnLocalPlayerStateSet`을 방송하고, 목록 위젯이 그것도 구독해 다시 만든다. **확정 전에는 목록을 비워 둔다** — 잘못된 행이 한 번 보였다 사라지는 깜빡임을 없애기 위함이며, 확정 신호가 곧 채워 준다.

> 리슨 서버 호스트는 `InitPlayerState`에서 동기적으로 정해지므로 `OnRep_PlayerState`를 타지 않고, 애초에 어긋나지도 않는다. 이 신호는 클라이언트 경로를 메우는 것이다.

**AI는 목록에 잡히지 않는다.** PlayerState가 없기 때문이며, 나중에 봇에 PlayerState를 붙이면 `IsABot()` 필터가 그때 작동한다(코드에는 이미 있다).

## 스킬 쿨다운 표시

HUD 스킬 아이콘 위에 검은 오버레이가 12시부터 시계방향으로 걷히는 연출. 값은 전부 쿨다운 GE에서 나오고, **새 네트워크 코드는 없다.**

### 대상 지정 — 쿨다운 태그 직접 지정

슬롯 위젯은 `CooldownTag`(`EditDefaultsOnly`, `meta = (Categories = "Cooldown")`) 하나만 갖는다. `Cooldown.Combat.Attack.Skill.A~D`가 네이티브 태그로 이미 정의돼 있어 슬롯이 하나씩 잡으면 끝난다.

입력 태그로 어빌리티 스펙을 찾아 `GetCooldownTags()`로 자동 해석하는 방법도 있지만 **`ActivatableAbilities` 복제가 HUD 생성보다 늦게 도착할 수 있어** 최초 해석 실패를 재시도로 메워야 한다. 지금 태그 네이밍이 이미 슬롯 규약처럼 잡혀 있으므로 그 복잡도를 사지 않았다. **무기·캐릭터별로 쿨다운 태그가 갈라지는 시점**에 자동 해석으로 바꿀 것.

### 갱신 흐름

| 전환 | 신호 | 구현 |
|---|---|---|
| 대기 → 쿨다운 | 쿨다운 태그 카운트 0 → 1 | `RegisterGameplayTagEvent(CooldownTag, NewOrRemoved)` 구독 → `OnCooldownStarted()` + 진행률 1회 발화 |
| 진행 중 | 매 틱 | 활성 GE 재조회 → `OnCooldownProgress(Progress, RemainingTime)` |
| 쿨다운 → 대기 | 카운트 1 → 0 | `OnCooldownEnded()` + 틱 조기 반환 |

`Progress`는 0(방금 시전) → 1(완료). 겹친 쿨다운 GE가 있으면 **가장 늦게 끝나는 것**을 채택한다.

**왜 매 틱 재조회인가** — 시작 시점에 EndTime을 캐싱해 로컬 보간하면 더 싸지만, **쿨타임 감소 적용이나 쿨다운 GE 갱신(재시전)은 태그 카운트를 바꾸지 않아** 이벤트가 오지 않고 눈금이 조용히 어긋난다. 활성 GE 배열 순회일 뿐이라 슬롯 몇 개로는 비용이 문제되지 않는다. 같은 이유로 **전체 지속시간도 하드코딩하지 않고** `GetActiveEffectsTimeRemainingAndDuration`이 돌려주는 쌍에서 함께 받는다(→ [GrowthSystemDesign.md](../../GrowthSystemDesign.md)).

**왜 복제 걱정이 없나** — Chaser의 ASC는 PlayerState에서 `Mixed`라 자기 쿨다운 GE가 소유 클라이언트에 완전 복제되고, 로컬 예측 어빌리티는 커밋 순간 클라에서 바로 태그가 붙는다. 태그는 붙었는데 GE가 아직 도착하지 않은 프레임을 대비해 조회 실패 시 그 프레임 갱신만 건너뛴다.

### 함정

- **`TickFrequency`는 건드리지 않는다.** private 멤버라 접근 불가이고 값도 `Never` / `Auto` 둘뿐이다. `Auto`는 `bClassRequiresNativeTick = !NativeParent->HasMetaData(DisableNativeTick)`이라 **네이티브 부모가 `NativeTick`을 재정의하면 그것만으로 틱이 돈다**(BP에 Tick 이벤트를 만들 필요 없음). 대신 `NativeTick` 첫 줄에서 쿨다운 중이 아니면 조기 반환해 평소 비용을 bool 검사 하나로 묶는다.
- **무한 지속 쿨다운**(Duration ≤ 0)은 진행률을 만들 수 없어 `Progress = 0`으로 고정, 오버레이가 가득 찬 상태로 남는다.
- 구독 수명은 체력바와 같은 계약이다 — `NativeDestruct`는 핸들만 끊고 `CachedASC`는 남기며, `NativeConstruct`의 `BindToASC()`가 재구독하면서 **현재 태그 카운트로 상태를 다시 맞춘다**(숨어 있는 동안 시작·종료된 쿨다운도 이때 반영).

### 오버레이 연출 (에디터 쪽)

프로그레스바는 서브모듈의 `Content/_ThirdParty/UI/ExtendedProgressBar` 팩을 쓴다. `BP_ExtendedProgressBar`를 아이콘 위에 겹치고 `ProgressBarType = Circular`, 채움 색은 검정.

- **증가·감소 애니메이션(`bEnableIncreaseAnimation` / `bEnableDecreaseAnimation`)은 끈다.** 체력바용 보간이라 켜두면 GAS 시간과 눈금이 어긋난다.
- **`FillColor`의 알파는 먹지 않는다.** UI 도메인 머티리얼은 Final Color와 Opacity가 별개 핀인데 이 팩은 Opacity를 형태 마스크가 전담하고 색의 A는 버린다(배경만 `BackgroundOpacity` 스칼라를 따로 갖는다). 반투명은 위젯의 **`Render Opacity`**로 준다.
- 팩 원본은 수정하지 않는다(서브모듈 + 팩 에셋 수정 정책 → [EasyGameUI.md](EasyGameUI.md)). 색·마스크를 바꾸려면 `MI_ExtendedProgressBar_Circular`를 부모로 하는 MI를 `Content/ChainBurst/UI/` 아래에 만든다.

### ASC 바인딩

체력바와 같은 경로다. `WBP_CB_HUD`의 `Event Construct`에서 `Get Owning Player Pawn` → `Get Ability System Component` → `Cast To CBAbilitySystemComponent` 를 한 번 하고, 각 슬롯의 `Initialize With ASC`를 호출한다. 슬롯 위젯은 `Is Variable`을 직접 켜야 한다.

## 버스트 게이지 표시

HUD에서 버스트 게이지(→ [Burst.md](../Gameplay/Burst.md))를 **바 하나로** 보여준다. 충전 중에는 게이지만큼 차고, 버스트 중에는 남은 시간만큼 줄어든다. 새 네트워크 코드는 없다.

### 두 모드 — 이벤트가 섞이지 않는다

| 모드 | 신호 | BP 이벤트 |
|---|---|---|
| 충전 | `BurstGauge` 어트리뷰트 변경 | `OnBurstGaugeChanged(CurrentGauge, MaxGauge)` — 가득 참은 BP에서 `Current >= Max` |
| 충전 → 버스트 | `Status.Combat.Burst` 카운트 0 → 1 (`NewOrRemoved`) | `OnBurstStarted()` + 남은 시간 1회 |
| 버스트 | 매 틱, 활성 GE 재조회 | `OnBurstProgress(RemainingRatio, RemainingTime)` — 1(방금 발동) → 0(종료) |
| 버스트 → 충전 | 카운트 1 → 0 | `OnBurstEnded()` + **현재 게이지로 `OnBurstGaugeChanged` 1회** |

C++가 모드를 나눠서, WBP는 오는 이벤트대로 바에 값을 넣기만 하면 된다.

- **버스트 중에는 게이지 변경을 보내지 않는다.** 게이지는 서버가 소모하므로 소유 클라에는 발동보다 왕복 지연만큼 늦게 0이 도착한다. 반면 `Status.Combat.Burst`는 예측 적용된 GE가 즉시 붙인다. 모드를 태그로 전환하고 버스트 중의 게이지 변경을 무시하면, **늦게 온 0이 남은 시간 표시를 덮어쓰지 않고** 발동 직후 게이지가 가득 찬 채 머무는 구간도 보이지 않는다.
- **종료 직후 현재 게이지를 다시 보낸다.** 버스트 동안 무시한 변경(→ 0)을 여기서 한 번에 따라잡는다. 서버가 발동을 거부해 예측 태그가 빠진 경우에도 같은 경로로 **가득 찬 게이지로 되돌아간다.**
- **남은 시간은 매 틱 재조회한다** — 스킬 쿨다운 표시와 같은 방식(`GetActiveEffectsTimeRemainingAndDuration`, 태그로 GE 쿼리). 여러 개가 잡히면 가장 늦게 끝나는 것을 쓴다. 서버 승인 직후 예측 사본과 복제 사본이 잠깐 함께 있을 수 있기 때문이다. 태그는 붙었는데 GE가 아직 조회되지 않는 프레임은 건너뛴다.
- `NativeTick` 첫 줄에서 버스트 중이 아니면 조기 반환한다. 평소 비용은 bool 검사 하나다.

### 발동 가능 알림 — `OnBurstReadyChanged`

게이지 위젯 **밖**(HUD의 발동 안내 버튼 등)이 "지금 버스트를 쓸 수 있는가"를 알아야 할 때 구독하는 델리게이트다(`FCBOnBurstReadyChanged`, `Types/CBDelegates.h`). **값이 바뀔 때만** 방송한다.

**발동 가능 = 게이지 ≥ `MaxBurstGauge` && 버스트 중이 아님.** 게이지 값만으로 판정하지 않는 이유는 게이지 소모가 서버에서 일어나 소유 클라에 늦게 도착하기 때문이다. 게이지만 보면 발동 입력 후에도 왕복 지연만큼 "가능"이 남는다.

| 상황 | 결과 | 경로 |
|---|---|---|
| 게이지가 100에 도달 | true | 게이지 변경 → `BroadcastBurstGaugeChanged()` → `UpdateBurstReady()` |
| 발동 입력 | **즉시** false | 예측 적용된 `GE_Burst`가 태그를 붙임 → `SetBurstActive(true)` → `UpdateBurstReady()` |
| 서버가 발동 거부 | true 복귀 | 예측 태그가 빠짐 → 종료 후 게이지(100 그대로) 재전송 → `UpdateBurstReady()` |
| 버스트 종료 | false 유지 | 게이지는 이미 0 |
| 가득 찬 채 사망 | false | 패시브 종료가 게이지를 0으로 |

- **구독은 `InitializeWithASC` 호출 전에 한다.** 초기화 중의 첫 판정이 방송되기 때문이다. 지금은 HUD가 생길 때마다(스폰·리스폰·캐릭터 변경) 게이지가 0이라 순서가 뒤바뀌어도 놓칠 방송이 없지만, 그 전제에 기대지 않는다.
- 현재 값을 읽는 getter는 두지 않았다. 위와 같은 이유로 HUD 생성 시점의 초기 상태는 언제나 false다.
- 발동 안내가 **클릭해서 발동하는 버튼**이 되면 UI 입력을 어빌리티 발동으로 잇는 경로가 별도로 필요하다(현재는 키 입력 `Input.Action.Combat.Burst`만 발동한다).

### 구독 수명과 초기값

체력바와 같은 계약이다. `NativeDestruct`는 구독만 끊고 `CachedASC`는 남기며, `NativeConstruct`의 `BindToASC()`가 재구독하면서 **현재 태그로 모드를 맞추고**(숨어 있던 동안 시작·종료된 버스트도 반영) 모드가 그대로면 현재 값만 다시 보낸다.

슬레이트가 없을 때는 현재 상태 반영을 미룬다(`BindToASC()`의 `GetCachedWidget()` 가드). 서드파티 프로그레스 바가 슬레이트 생성 전에 받은 값을 머티리얼 없이 삼키고, 생성 후 같은 값을 무시해 기본값으로 굳는 문제 때문이다 — 체력바가 HUD 플레이어 목록에서 겪은 것과 같다. 지금 배치(HUD `Event Construct`에서 초기화)에서는 항상 슬레이트가 있지만, `InitializeWithASC`는 BP에서 부를 수 있으므로 호출 순서에 기대지 않고 위젯이 지킨다.

### 에디터 작업

- `WBP_CB_BurstGauge` — `UCBBurstGaugeWidget` 자식. 프로그레스 바 하나에 `OnBurstGaugeChanged`(Current / Max), `OnBurstProgress`(RemainingRatio)를 연결하고, `OnBurstStarted` / `OnBurstEnded`에서 색·연출을 전환한다.
  - 서드파티 바(`BP_ExtendedProgressBar`)를 쓰면 **증가·감소 애니메이션을 끈다.** 켜 두면 남은 시간 눈금이 GAS 시간과 어긋나고, 모드 전환(가득 참 → 남은 시간 1.0 → 빈 게이지)이 보간으로 뭉개진다.
- `WBP_CB_HUD`에 배치하고 `Is Variable`을 켠다. `Event Construct`의 기존 ASC 캐스트 결과로 `Initialize With ASC`를 호출한다(체력바·스킬 슬롯과 같은 자리).
- 발동 안내 버튼: 같은 `Event Construct`에서 **`Initialize With ASC`보다 먼저** 게이지 위젯의 `OnBurstReadyChanged`에 바인딩하고, `bIsReady`로 버튼 Visibility를 전환한다. 버튼의 기본 Visibility는 숨김으로 둔다.

## 보스 바 (화면 상단 체력·기절 게이지)

보스가 **처음 플레이어를 발견하는 순간** 모든 플레이어 화면 상단에 보스의 체력과 기절 게이지(→ [Stun.md](../Gameplay/Stun.md))를 띄운다. 새 네트워크 코드는 없다 — 신호는 ASC 태그 복제, 값은 어트리뷰트 복제다.

```
[서버] ACBAIController::UpdateTargetInBlackboard — 첫 타겟을 쓰는 순간 1회
   └ 보스 ASC 에 Status.Combat.Engaged (TagOnly 복제, 사망까지 유지)
        ▼ ASC 태그 복제
[각 클라] 보스의 UCBUIComponent — 준비 완료 때 태그 구독 (이미 붙어 있으면 즉시)
   └ UCBBossBarWidget 생성 (소유자 = 로컬 플레이어 컨트롤러)
        → InitializeWithASC(보스 ASC) → 자식 체력바·기절 게이지 배선
        → 로컬 플레이어 HUD 스택 Game 레이어에 삽입
   └ 보스 액터 파괴(EndPlay, Destroyed) 시 스택에서 제거
```

### "등장" = 첫 발견인 이유

보스(`BP_Outlaw_Hulk`)는 게임플레이 맵에 **처음부터 배치**돼 있다. "스폰·준비 완료"를 기준으로 삼으면 레벨 시작부터 모든 화면에 바가 떠 있게 된다. 첫 타겟 획득은 보스가 스폰되는 방식(배치·스포너)과 무관하게 "싸움이 시작됐다"는 같은 뜻이다.

### 생성 주체 — 보스의 UI 컴포넌트

"새 UI는 어디로" 기준 그대로다 — **화면 + 캐릭터 데이터(보스 ASC·위젯 클래스) 필요 → `UCBUIComponent`**. 보스는 모든 클라이언트에 시뮬 프록시로 존재하므로, 각 클라의 보스 UI 컴포넌트가 **자기 화면의 로컬 플레이어 컨트롤러**(`GetFirstLocalPlayerController` — 호스트는 호스트, 클라는 자기)로 위젯을 만들어 그 HUD에 넣는다. HUD가 생성 주체였다면 HUD가 보스를 찾아내는 경로가 따로 필요했다.

- **위젯 클래스는 `UCBOutlawLoadout::BossBarWidgetClass`.** 전 클라이언트에 필요하므로 `ApplyToCharacter()`(공용 경로)로 주입한다. **비우면 보스 바가 없다** — 엘리트 등급 Outlaw 는 비워 둔다. 클래스 유무가 곧 "보스인가"이므로 별도 플래그를 두지 않는다.
- **HUD 위젯과 같은 스택 함수**(`Local_PushWidgetToStack` / `Local_RemoveWidgetFromStack`)를 쓴다. Game 레이어는 오클루더가 아니라 플레이어 HUD 와 함께 보인다(→ [EasyGameUI.md](EasyGameUI.md)).
- **플레이어의 사망·리스폰과 무관하다.** 바는 보스가 소유하고 HUD(맵 단위)에 꽂혀 있어, 로컬 플레이어 폰이 바뀌어도 그대로 남는다.

### 표시 규칙 — 교전 개시 후 보스가 사라질 때까지

| 상황 | 결과 | 이유 |
|---|---|---|
| 보스가 첫 타겟을 잡음 | 전 클라에 표시 | 태그 복제 → 각 클라 구독 콜백 |
| 보스 타겟이 죽거나 잠시 비어도 | **유지** | 태그를 떼지 않는다. 남은 플레이어가 인지 밖이면 타겟이 잠깐 비는데, 그때마다 꺼졌다 켜지면 안 된다. 스택에서 빠지는 중인 위젯을 다시 넣는 함정(→ [EasyGameUI.md](EasyGameUI.md) "지연 제거")도 피한다 |
| 보스 사망 | 0 인 채로 남음 | 마지막 타격으로 0 이 되는 순간이 보인다 |
| 보스 파괴 (`DespawnDelay` 뒤 디스폰) | 제거 | `EndPlay(Destroyed)` — 시체와 함께 사라진다 |
| 교전 중에 이 클라에 보스가 복제돼 들어옴 | 즉시 표시 | 준비 완료 때 태그가 이미 있으면 바로 띄운다 |

- **사망 즉시 숨기려면** 머리 위 바처럼 `OnCharacterDiedDelegate`를 구독하면 된다. 지금은 0 을 보여 주는 쪽을 택했다.
- **보스 리셋(전멸 시 회복·원위치)** 이 생기면 리셋 지점이 태그를 떼고, 컴포넌트가 태그 제거(`NewCount == 0`)에서 바를 빼도록 넓힌다. 지금은 붙는 경우만 처리한다.
- **보스가 동시에 둘 이상이면 바가 겹친다.** 지금은 보스가 하나라 다루지 않는다.

### ⚠️ 연관성 — 멀리 있는 플레이어에게도 띄우려면

클라이언트 화면의 보스는 **넷 연관성(Net Cull Distance) 안에 있을 때만 존재**한다. 캐릭터 기본 컬 거리(150m) 밖의 플레이어에게는 보스 액터가 없거나 파괴되어(→ `EndPlay(Destroyed)`) **바도 없다.** "모든 플레이어 화면"이 되려면 보스 BP 에서 **`Always Relevant`** 를 켠다. 보스는 하나뿐이라 비용이 문제되지 않는다.

### 기절 게이지 — 최대값은 상수

`MaxStunGauge` 는 어트리뷰트가 아니라 `UCBAttributeSet` 의 상수다(차는 속도는 적립량으로 조절 → [Stun.md](../Gameplay/Stun.md)). 그래서 `UCBStunGaugeWidget` 은 `StunGauge` 하나만 구독하고 Max 는 상수를 실어 보낸다. 기절이 시작되면 게이지가 비워지므로 바는 0 으로 떨어진다. **"기절 중" 상태 표시는 없다** — `Status.Combat.Stunned` 는 서버 전용(경로 ②)이라 클라가 알 수 없고, 표시하려면 복제를 추가해야 한다.

### 슬레이트 전 초기화

보스 바는 **스택에 올리기 전에** `InitializeWithASC()` 를 부른다(슬레이트 없음). 자식 체력바·기절 게이지는 둘 다 "슬레이트가 없으면 캐시만 하고 구독은 `NativeConstruct` 에서" 계약이라(위 "첫 표시의 함정") 첫 값이 삼켜지지 않는다. 기절 게이지 위젯을 새로 만들 때 이 계약을 그대로 옮긴 이유다.

### 검토했지만 채택하지 않은 것

| 안 | 이유 |
|---|---|
| 스폰(준비 완료) 시 표시 | 보스가 맵에 처음부터 배치돼 있어 레벨 시작부터 뜬다 |
| 첫 피격 시 표시 | 보스가 먼저 공격해도 바가 뜨지 않는다 |
| 아레나 진입 볼륨 | 레벨마다 배치해야 하고, 들어간 사람에게만 뜬다 |
| GameState 에 "현재 보스" 복제 프로퍼티 | UI 때문에 복제 코드가 생긴다. ASC 태그 복제가 이미 있다 |
| HUD 가 보스 바 생성 | HUD 가 보스를 찾아내야 한다. 데이터를 가진 쪽(보스)이 만드는 게 배치 기준과 맞다 |
| 교전 종료(타겟 상실) 시 숨김 | 위 "표시 규칙" — 깜빡임과 스택 재삽입 함정 |
| 체력바를 범용 어트리뷰트 바로 일반화 | 세 곳(HUD·머리 위·플레이어 목록)에서 쓰는, 잘 동작하는 위젯을 고쳐야 한다. 버스트 게이지처럼 게이지별 베이스를 둔다 |
| 보스 이름을 C++ 데이터로 | 위젯 클래스가 보스별로 로드아웃에 등록되므로 WBP 에 직접 둔다 |

### 에디터 작업

- `WBP_CB_StunGauge` — `UCBStunGaugeWidget` 자식. 프로그레스 바 하나에 `OnStunGaugeChanged`(Current / Max) 연결.
- `WBP_CB_BossBar` — `UCBBossBarWidget` 자식. 화면 상단 중앙 배치. 자식 위젯을 **`HealthBarWidget` / `StunGaugeWidget` 이름으로** 두고 `Is Variable` 을 켠다(이름이 다르면 배선되지 않고 그 부분만 조용히 빈다). 보스 이름 텍스트는 여기에 직접.
- `DA_Loadout_Outlaw_Hulk` — `BossBarWidgetClass = WBP_CB_BossBar`.
- `BP_Outlaw_Hulk` — **`Always Relevant` 켜기**(위 ⚠️), UI 컴포넌트의 `bShowOverheadBar` 끄기(머리 위 바와 중복).
- 새 `UCLASS`·`UPROPERTY` 가 있으므로 코드 반영 후 **에디터를 재시작**해야 BP 에서 보인다.

## 위젯 클래스 등록 — 로드아웃

위젯 클래스도 에셋이므로 로드아웃 원칙(→ [Loadout.md](../Foundation/Loadout.md))대로 로드아웃에 등록하고, 컴포넌트 멤버는 런타임 캐시(세터 주입)다. 주입은 모두 Ready 방송 **전**에 실행되므로 준비 완료 훅 시점엔 클래스가 확정돼 있다.

| 필드 | 위치 | 주입 경로 |
|---|---|---|
| `OverheadHealthBarWidgetClass` | `UCBCharacterLoadout`(베이스) | `ApplyToCharacter()` — 전 인스턴스 공용 (시뮬 프록시 포함 전 클라 필요) |
| `bShowNamePlate`<br>`NamePlateWidgetClass`<br>`NamePlateZMargin` | `UCBCharacterLoadout`(베이스) | `ApplyToCharacter()` — 전 인스턴스 공용 (남의 이름표도 보여야 하므로). 부착 높이 여유값을 클래스와 같은 자리에 둔 이유는 **캡슐 크기가 캐릭터마다 다르기 때문** |
| `BossBarWidgetClass` | `UCBOutlawLoadout` | `ApplyToCharacter()` — 전 인스턴스 공용 (모든 클라 화면에 뜨므로). 비우면 보스 바 없음 |
| `HUDWidgetClass` | `UCBChaserLoadout` | `Local_ApplyToCharacter()` — 소유 클라이언트 전용 (InputConfig와 동일 경로). 타입은 `TSubclassOf<UUserWidget>` — 체력바가 아닌 임의 HUD 컨테이너 WBP를 지정. 구 필드명 `HUDHealthWidgetClass`는 `[CoreRedirects]` PropertyRedirect로 승계 |

## 외부에서 UI 접근 (설계 규칙)

- **로컬 UI 연출**(조준 대상 하이라이트, 거리 컬링 등)만 `ICBUIInterface::GetCBUIComponent()` 경유 직접 호출로 처리한다. 호출한 클라이언트 화면에만 반영된다.
- **게임플레이가 원인인 UI 변화**(피격 연출, 사망 시 숨김, 버프 표시)는 직접 호출 금지 — 복제되는 신호(어트리뷰트·태그·GameplayCue)를 UI 쪽에서 구독해 각 클라가 스스로 반영한다.

## 미구현 (추후 과제)

- **HUD 체력바 WBP에 `BPI_EGUI_UINavigationInterface` 구현** — 3개 선언 함수를 전부 `false`로. 미구현이어도 인터페이스 메시지 호출은 기본값(`false`)을 반환하므로 **현재 동작에 문제는 없다.** 의도를 명시하는 용도이며 위젯 개편 시 함께 처리 (→ [EasyGameUI.md](EasyGameUI.md))
- 가림(오클루전) 처리, 데미지 숫자, HUD 확장(버프 표시)
- 플레이어 목록의 팀 필터 — 팀이 갈리는 매치가 되면 `ECBTeam`으로 아군만 추리는 조건이 필요하다 (→ [Teams.md](../Foundation/Teams.md))
- 스킬 슬롯 아이콘의 로드아웃 등록 — 현재는 WBP에서 직접 지정. 캐릭터·무기별로 아이콘이 갈리는 시점에 `UCBChaserLoadout`으로 옮길 것
- 스킬 슬롯의 자원 부족·사용 불가 표시 (쿨다운과 신호가 다름)
- 데미지 0(빗맞음·완전 방어) 시에도 머리 위 바 표시 (피격 GameplayCue 구독이 필요)

## 관련 문서

- 시스템 UI(메인메뉴·일시정지·옵션) 프레임워크: [EasyGameUI.md](EasyGameUI.md)
- 어트리뷰트 복제·ASC 소유: [ASC-Ownership.md](../Foundation/ASC-Ownership.md)
- 준비 완료까지 초기화 지연: [SystemReady.md](../Foundation/SystemReady.md)
- 위젯 클래스 등록(로드아웃): [Loadout.md](../Foundation/Loadout.md)
- 컴포넌트 역할 요약: [Components.md](../Foundation/Components.md)
