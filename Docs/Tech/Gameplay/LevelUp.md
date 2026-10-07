# 레벨업 (공유 경험치·카드 선택)

> 플레이어가 AI를 처치하면 **전 플레이어가 같은 경험치를 함께** 얻고, 레벨이 오르면 **모든 플레이어의 게임이 정지**된 채 각자 카드 3장 중 하나를 골라 어트리뷰트를 올린다. 경험치와 레벨 판정은 서버(호스트)만 하고, 카드는 플레이어마다 따로 뽑는다. 최대 레벨은 데이터로 정하며 0이면 무제한이다(→ "최대 레벨").

## 흐름

```
[서버] AI 사망 (ACBAICharacter::Auth_OnDeath)
  └ ACBGameplayGameMode::Auth_AddExperience(로드아웃의 ExperienceReward)
      ├ 넘친 만큼 레벨 증가 → ACBGameplayGameState::Auth_SetExperience (복제 → 경험치 바)
      └ 오른 레벨 수만큼 PendingLevelUps 증가 → 진행 중인 레벨업이 없으면 시작

[서버] Auth_StartLevelUpRound — 레벨업 하나
  ├ 플레이어마다 카드 3장 뽑기 → 기억(PendingCardOffers) + Client_OfferLevelUpCards(인덱스)
  ├ 호스트 PC::SetPause(true, 해제 조건)               ← 전 클라 월드 정지
  └ 코어 티커로 타임아웃 예약 (실제 시간)

[각 클라] 카드 위젯을 Menu 레이어에 띄움 → 고르면 Server_SelectLevelUpCard(슬롯) → 대기 표시

[서버] Auth_SelectLevelUpCard — 받은 카드인지·아직 안 골랐는지 확인 → 카드 GE 적용 → 명단에서 제거
  ├ 타임아웃: 아직 안 고른 플레이어는 받은 카드 중 하나를 무작위 적용
  ├ 나간 플레이어: Logout 에서 명단 제거
  └ 명단이 비면 Auth_FinishLevelUpRound
       ├ 남은 레벨업이 있으면 → 정지 유지한 채 다음 레벨업 (카드 창 내용만 바뀜)
       └ 없으면 → 전원 Client_EndLevelUp + PC::SetPause(false)
```

## 구성 요소

| 요소 | 위치 | 역할 |
|---|---|---|
| 규칙 | `ACBGameplayGameMode` (`#pragma region LevelUp`) | 경험치 누적·레벨 판정·카드 뽑기·선택 검증·적용·정지/해제·타임아웃. 서버 전용 |
| 공유 레벨·경험치 | `ACBGameplayGameState` | `Level`·`Experience` 복제 + `OnExperienceChanged`. 게임모드 생성자가 `GameStateClass`로 고정 |
| 카드 주고받기 | `ACBChaserController` | `Client_OfferLevelUpCards` / `Server_SelectLevelUpCard` / `Client_EndLevelUp`, 카드 위젯 생성·스택 삽입·제거 |
| 데이터 | `UCBLevelUpData` → `UCBGameInstance::LevelUpData` | 경험치 곡선, 카드 목록(`FCBLevelUpCard`), `CardsPerLevelUp`(3), `SelectionTimeout`(20초), 카드 위젯 클래스 |
| AI 보상 | `UCBAILoadout::ExperienceReward` | 로드 완료 시 `ACBAICharacter`가 캐싱, 사망 시 게임모드에 전달 |
| 카드 효과 | 카드마다 GE 에셋 (Infinite) | 어떤 어트리뷰트를 어떻게 올릴지는 GE가, 얼마나는 카드의 `Magnitude`가 SetByCaller(`Data.LevelUp`)로 정함 |
| 카드 위젯 | `UCBLevelUpWidget` | 카드 표시(`OnCardsOffered`)·선택(`Local_SelectCard`)·남은 시간(`GetRemainingSelectionTime`)·대기 상태 |
| 경험치 바 | `UCBExperienceBarWidget` | 게임 스테이트 구독 → `OnExperienceChanged(Level, Current, Required)`. HUD WBP 안에 배치 |
| 메뉴 레이어 다리 | `ACBHUD::PushMenuLayerWidget` | C++에서 Menu 레이어에 올리는 이벤트 (→ [EasyGameUI.md](../Presentation/EasyGameUI.md)) |

### 왜 이 자리인가

- **경험치는 어트리뷰트가 아니다.** 전 플레이어 공유 값이라 플레이어별 ASC에 둘 이유가 없다 — 4명의 ASC에 같은 값을 복제하게 된다. 게임 전역 상태이므로 게임 스테이트다(→ [GameFlow.md](../Flow/GameFlow.md) "클래스별 존재 범위").
- **다음 레벨 필요량은 복제하지 않는다.** 데이터가 게임 인스턴스에 있어 클라이언트도 `GetRequiredExperience(Level)`로 같은 값을 계산한다.
- **데이터는 게임 인스턴스에 둔다.** 서버(뽑기·검증)와 클라이언트(카드 표시·필요량)가 **폰 없이** 읽어야 한다 — `CharacterCatalog`와 같은 이유(→ [Loadout.md](../Foundation/Loadout.md)). 카드는 배열 인덱스로 주고받으므로 양쪽이 같은 에셋을 본다.
- **카드 위젯은 컨트롤러가 띄운다.** 캐릭터 HUD 위젯은 폰과 함께 파괴·재생성되므로, 리스폰 중에 카드가 오면 받을 위젯이 없다. 컨트롤러는 폰이 바뀌어도 살아 있다.
- **처치자를 따지지 않는다.** 경험치가 공유이고, 지금 AI를 죽일 수 있는 건 플레이어뿐이다(Outlaw·Rogue는 같은 팀, 중립은 적대 대상이 아님, 무기 판정은 적대만 맞힘 → [Teams.md](../Foundation/Teams.md)).

### 최대 레벨

`UCBLevelUpData::MaxLevel`(0 = 무제한). 판정은 `IsMaxLevel(Level)` 하나를 서버와 경험치 바가 함께 쓴다.

- **서버**: 최대 레벨이면 `Auth_AddExperience`가 아무것도 하지 않는다. 한 번에 여러 레벨을 넘어도 최대 레벨에서 반복을 멈추고 넘친 경험치는 0으로 버리므로, **최대를 넘는 레벨업(카드 선택)은 생기지 않는다.**
- **경험치 바**: 최대 레벨이면 C++ 위젯이 현재 경험치를 필요량과 같게 넘겨 바가 가득 찬다. BP 이벤트 인자는 그대로라 기존 WBP를 고칠 필요가 없고, "MAX" 표시가 필요하면 WBP에서 `IsMaxLevel(Level)`로 분기한다.
- 넘친 경험치를 따로 쌓아 두지 않는다 — 쓸 곳이 없다. 최대 레벨 이후의 처치 보상(회복 등)이 필요해지면 별도 설계.
- 최대 레벨을 두면 경험치 곡선은 그 레벨까지만 키를 찍으면 되므로, 마지막 키 뒤의 외삽 설정을 신경 쓸 필요가 없다.

## 정지 — 엔진이 정한 제약

레벨업 동안 월드 전체를 정지한다. 정지 중에는 **월드 타이머와 액터 틱이 멈춘다** — AI·몽타주·버스트 남은 시간(GE 지속시간)·리스폰 대기·스포너가 모두 함께 멈추는 것이 의도한 동작이다. EasyGameUI 일시정지 메뉴는 게임을 멈추지 않으므로(`Pause Game in Pause Menu? = False`) 충돌하지 않는다.

**정지 중에는 속성 복제가 사실상 멈춘다.** 서버의 `TimeSeconds`가 멈추는데(`LevelTick.cpp` — `if (!bIsPaused) TimeSeconds += DeltaSeconds`), 복제 대상 선정이 `TimeSeconds`와 액터의 다음 갱신 시각을 비교한다(`NetDriver.cpp` `ServerReplicateActors_BuildConsiderList`). 그래서 `ForceNetUpdate()`한 액터만 나간다. 네트 송수신 자체는 정지와 무관하게 돌므로 **RPC는 정상**이다. 따라서:

- 카드 제시·선택·종료는 **전부 RPC**다.
- 게임 스테이트는 값을 바꾼 직후 `ForceNetUpdate()`한다 — 레벨업 직후 정지돼도 경험치 바가 갱신된다.

**정지는 `APlayerController::SetPause` 경로로 건다.** 이 함수는 정지를 걸면서 WorldSettings를 `ForceNetUpdate()`한다(`PlayerController.cpp`). `AGameModeBase::SetPause`만 부르면 이 처리가 빠져, 다음 틱에 서버 시간이 멈추면서 **정지 신호(`PauserPlayerState`)가 클라이언트로 나가지 않을 수 있다.**

- **호스트 컨트롤러로 건다**(`GEngine->GetFirstLocalPlayerController`). 정지 주체(`PauserPlayerState`)가 나가면 정지가 풀리는데, 호스트는 세션이 끝날 때까지 나가지 않는다.
- **해제 조건 델리게이트(`FCanUnpause`)를 건다.** "진행 중이거나 남은 레벨업이 없음"일 때만 풀리므로, 콘솔 `pause` 같은 다른 해제 요청이 와도 선택 중에는 풀리지 않는다.

## 타임아웃 — 실제 시간

월드 타이머(`SetTimer`)는 정지 중에 멈추므로 타임아웃이 영원히 오지 않는다. **코어 티커(`FTSTicker`)** 는 월드 밖에서 실제 시간으로 돈다.

- 레벨업 시작 때 걸고, 전원이 먼저 고르면 `RemoveTicker`로 해제한다.
- 만료되면 아직 고르지 않은 플레이어마다 **받은 카드 중 하나를 무작위로** 적용하고 레벨업을 끝낸다. 그 뒤에 도착한 선택은 명단에 없어 무시된다.
- **월드 밖이라 게임모드와 함께 사라지지 않는다.** `EndPlay`에서 해제한다.
- 클라이언트 카운트다운은 같은 `SelectionTimeout`으로 **카드를 받은 순간부터 로컬에서** 센다(`GetRealTimeSeconds` — 정지 중에도 흐름). RPC 지연만큼 서버가 먼저 끝내므로 서버를 기준으로 맞출 필요가 없다.

## 카드 효과 — GE 데이터

카드 하나 = `FCBLevelUpCard` { 효과 GE, `Magnitude`, 등급(`ECBLevelUpCardGrade`), 이름, 설명, 아이콘(소프트) }. 서버가 스펙을 만들고 `Data.LevelUp`에 `Magnitude`를 넣어 플레이어 ASC(PlayerState)에 적용한다.

- **효과 GE는 Infinite, 모디파이어 크기는 SetByCaller(`Data.LevelUp`).** Add인지 Multiply인지는 GE가 정한다. 같은 GE를 수치만 다른 여러 카드가 공유할 수 있다.
- **매치 끝까지 유지된다.** 로드아웃 부여 기록(`Auth_ApplyLoadoutEffect`)을 거치지 않으므로 무기 변경(`Auth_ClearLoadoutGrants`)에 회수되지 않고, 사망 정리는 `Status.Dead` 부여 GE만 지운다. ASC가 PlayerState 소유라 폰이 바뀌어도 남는다(→ [ASC-Ownership.md](../Foundation/ASC-Ownership.md)).
- **같은 카드를 또 고르면 GE가 한 번 더 적용되어 누적된다.**

### 등급 — 위젯 하나에 겉모습만 갈아 끼움

등급(Common / Rare / Epic / Legendary)은 카드 데이터의 `Grade`이고, 등급별 겉모습은 `UCBLevelUpData::GradeStyles`(등급 → `FCBLevelUpCardGradeStyle` { 프레임 이미지(소프트), 샤인 색 })에 둔다. 카드 위젯은 `SetCard`에서 `FindGradeStyle(Card.Grade)`로 찾아 프레임 이미지와 샤인 머티리얼의 `ShineColor`만 바꾼다.

- **등급마다 위젯을 따로 만들지 않는다.** 등급이 달라도 카드의 동작(선택 → 슬롯 전달, 등장 애니메이션, 버튼 바인딩)은 같고 겉모습만 다르다. 위젯을 나누면 그 로직이 등급 수만큼 복제되고, 본체가 카드마다 클래스를 고르는 분기가 생긴다. UMG 자식 위젯은 부모의 위젯 트리를 고칠 수 없어 상속으로도 피할 수 없다. **구조·동작이 다른 등급이 생길 때만** 그 등급을 별도 위젯으로 뺀다.
- **겉모습은 위젯이 아니라 데이터에 둔다.** 카드·경험치 곡선과 같은 에셋에서 레벨업 튜닝이 끝나고, 에셋을 위젯에 직접 박지 않는다는 원칙(→ [Loadout.md](../Foundation/Loadout.md))과 맞다.
- **샤인 머티리얼은 등급별로 복제하지 않는다.** `M_UI_CardShine`의 `ShineColor` 파라미터를 동적 머티리얼로 바꾼다.
- 등록하지 않은 등급은 `FindGradeStyle`이 경고를 남기고 `false` — 그 카드는 디자이너 기본 모습으로 뜬다.

### 등급별 뽑기 확률 — 등급 먼저, 그 안에서 균등

`UCBLevelUpData::GradeWeights`(등급 → 가중치)가 있으면 `RollCards`가 **칸마다 등급을 가중치 비율로 먼저 고르고, 그 등급의 남은 카드 중 하나를 균등하게** 고른다. 뽑은 카드는 후보에서 빠지므로 한 번의 제시 안에서 중복은 없다.

```
예: Common 70 / Rare 22 / Epic 7 / Legendary 1
 → 한 칸이 Legendary 일 확률 1%, 3장 중 한 장이라도 Legendary 일 확률 약 3%
```

- **카드마다 가중치를 붙여 한 번에 뽑지 않는다.** 그러면 실제 확률이 그 등급의 **카드 개수**에 끌려가, 카드를 추가할 때마다 체감 확률이 바뀐다. 등급을 먼저 고르면 설정한 비율이 곧 "한 칸이 그 등급일 확률"로 고정된다.
- **다 뽑힌 등급은 빼고 남은 등급끼리 비율을 다시 나눈다.** 그래서 가중치가 있는 카드가 충분하면 항상 `CardsPerLevelUp`장이 채워진다.
- **비어 있으면 예전처럼 전체 카드에서 균등하게** 뽑는다(등급 무관).
- **맵에 없는 등급은 가중치 0** — 그 등급 카드는 나오지 않고 경고 로그를 남긴다. 가중치가 있는 카드가 `CardsPerLevelUp`보다 적으면 그만큼만 제시된다.
- 겉모습(`GradeStyles`)과 맵을 분리했다. 이쪽은 서버의 뽑기 규칙이고 저쪽은 화면 표시라, 표시를 손보다 확률을 건드리는 일이 없게 한다. 뽑기는 서버에서만 하므로 네트워크 변경은 없다.
- 플레이어별 보정(행운 스탯 등)이 필요해지면 `RollCards`가 이미 플레이어마다 따로 불리므로 그 자리에서 가중치를 보정하면 된다. 지금은 없다.

### MaxHealth 카드 — 올린 만큼 회복

`GE_LevelUp_MaxHealth`(Infinite, MaxHealth Add)에 **Additional Effects 컴포넌트**로 `GE_LevelUp_Heal`(Instant, CurrentHealth Add, 둘 다 SetByCaller `Data.LevelUp`)을 연결하고 **`Copy Data From Original Spec`을 켠다.** SetByCaller 값이 회복 스펙으로 복사되므로 수치가 카드 하나에만 있다.

순서가 맞는지는 엔진에서 확인했다 — `ApplyGameplayEffectSpecToSelf`가 지속 GE의 모디파이어를 먼저 반영하고(`ActiveGameplayEffects.ApplyGameplayEffectSpec`) 그 뒤에 컴포넌트의 추가 효과를 적용한다(`Spec.Def->OnApplied`). 회복이 이전 최대 체력에 잘리지 않는다.

### 리스폰 체력 — 시작 GE 다음에 가득 채우기

Chaser 시작 GE(`GE_Chaser_*_Init`)는 Instant Override로 `CurrentHealth`를 커브 값으로 덮어쓴다. MaxHealth 카드를 고른 뒤 부활하면 **보너스만큼 덜 찬 채** 시작한다. 그래서 각 Chaser 로드아웃 `StartupEffects`에서 Init **다음**에 `GE_Chaser_FullHeal`(Instant, CurrentHealth Override = MaxHealth 기반, Target·비스냅샷)을 둔다. Instant 효과의 타깃 캡처는 적용 시점이므로 Init이 반영된 최대 체력(+카드 보너스)을 읽는다. 코드 변경은 없다.

### MovementSpeed 카드는 만들지 않는다

Walk/Sprint가 쓰는 `GE_MovementModifier`가 `MovementSpeed`를 **Override**하므로 카드 보너스가 걷기·질주 중에 사라지고 Run에서만 적용된다. 이동 속도 카드가 필요해지면 개이트 쪽을 Override가 아닌 방식으로 바꿔야 한다(→ [Locomotion.md](Locomotion.md)).

## 네트워크

| 값·신호 | 경로 | 비고 |
|---|---|---|
| 레벨·경험치 | 게임 스테이트 속성 복제 + `ForceNetUpdate` | 정지 중에도 나감 |
| 카드 제시 | `Client_OfferLevelUpCards` (Reliable) | 본인에게만. 인덱스 배열 |
| 카드 선택 | `Server_SelectLevelUpCard` (Reliable) | 슬롯 번호만 받음 — 제시하지 않은 카드는 고를 수 없음 |
| 레벨업 종료 | `Client_EndLevelUp` (Reliable) | 카드 위젯 제거 |
| 월드 정지 | WorldSettings `PauserPlayerState` 복제 | `PC::SetPause`가 즉시 송출 |
| 카드 효과 | GE (플레이어 ASC, Mixed) | 어트리뷰트는 재개 후 평소 주기로 복제 |

### 호스트에서는 RPC가 그 자리에서 실행된다 — 로컬 상태 변경을 먼저

리슨 서버 호스트가 부르는 `Server_` RPC와 자기 자신에게 오는 `Client_` RPC는 네트워크를 거치지 않고 **호출한 줄에서 바로 실행**된다. 그래서 호스트가 카드를 고르면 `Server_SelectLevelUpCard` 호출 하나 안에서 "적용 → 레벨업 종료 → 다음 레벨업 시작 → `Client_OfferLevelUpCards` → 위젯에 새 카드 표시"까지 전부 끝난 뒤에야 돌아온다.

`UCBLevelUpWidget::Local_SelectCard`가 RPC 뒤에 대기 표시(`OnCardSelected`)를 하던 때는, 이 순서 때문에 **방금 도착한 다음 카드가 대기 상태로 덮여** 호스트만 고르지 못하고 타임아웃까지 묶였다(한 번에 여러 레벨이 오를 때). 원격 클라이언트는 RPC가 나중에 도착해 재현되지 않으므로 **혼자 테스트할 때만 보이는** 종류의 버그다.

**규칙: RPC를 보내는 쪽의 로컬 상태 변경(UI 전환·플래그)은 RPC 호출보다 먼저 한다.** 응답이 같은 호출 안에서 돌아올 수 있다고 가정할 것.

## 카드 등장·퇴장 연출 (위젯 BP)

C++ 없이 위젯 BP와 애니메이션 2개(`WBP_CB_LevelUpCard`의 `Intro`, `WBP_CB_LevelUp`의 `CardOut`)로 처리한다.

```
OnCardsOffered(Cards) → PendingCards 에 보관
  ├ 카드가 이미 있음(연속 레벨업) → CardOut 재생(Restore State)
  └ 없음(첫 제시)                → BuildPendingCards
AnimationFinished(CardOut)
  ├ bClosing → Remove From Parent
  └ 아니면   → BuildPendingCards
BuildPendingCards → 기존 카드 생성 체인(PendingCards 기준)
카드 Construct → Intro 재생 + Flush Animations
OnWidgetRemovedFromStack → bClosing = true, CardOut 재생, HandledByWidget? = true
```

- **월드 정지 중에도 돈다.** 위젯 애니메이션은 Slate 틱(`UMGSequenceTickManager`)에서, 위젯 안의 Delay 같은 지연 노드는 위젯 자신의 틱(`UUserWidget::NativeTick` → `ProcessLatentActions`)에서 처리된다. 레벨의 타이머와 달리 정지에 묶이지 않는다.
- **종료 퇴장은 팩의 제거 위임 경로를 쓴다.** `OnWidgetRemovedFromStack`이 `HandledByWidget? = true`를 돌려주면 팩은 스택에서만 빼고 화면 제거는 위젯에 맡긴다(→ [EasyGameUI.md](../Presentation/EasyGameUI.md)). 스택에서는 즉시 빠지므로 게임 입력 복구·재개가 퇴장 시간만큼 늦어지지 않는다. 컨트롤러는 위젯 참조를 바로 비우므로, 퇴장 중에 새 레벨업이 오면 **새 인스턴스**가 떠 지연 제거 중인 위젯을 다시 스택에 넣는 일이 없다.
- **`CardOut`은 Restore State로 재생한다.** 끝나면 애니메이션이 바꾼 속성이 원래대로 돌아오므로 BP가 어떤 위젯이 움직였는지 알 필요 없이 다음 카드를 그 영역에 다시 채울 수 있다.
- **`Intro` 직후 `Flush Animations`.** 재생 요청한 애니메이션을 그 자리에서 평가해, 카드가 다 보인 상태로 한 프레임 그려졌다가 시작되는 깜빡임을 막는다.
- **카드 순차 등장(슬롯별 지연)은 넣지 않았다.** 지연되는 동안 카드를 숨기려면 `Intro`가 움직이는 위젯을 직접 숨겨야 하는데, 그 대상을 애니메이션에 맞춰 따로 관리해야 한다. 필요해지면 카드 루트를 숨긴 뒤 `SlotIndex × 간격` 만큼 Delay 후 재생하면 된다.

## 에디터 작업

| 대상 | 작업 |
|---|---|
| `BP_EasyMainGameHUD` | `Push Menu Layer Widget` 이벤트 구현 — `Insert Widget Instance in Stack` (Layer = **Menu**). 팩 수정이라 재적용 체크리스트 대상 (→ [EasyGameUI.md](../Presentation/EasyGameUI.md)) |
| `GE_LevelUp_*` | Infinite, 모디파이어 크기 SetByCaller `Data.LevelUp`. AttackPower / DefensePower / AttackSpeed / LifeOnHit / MaxHealth |
| `GE_LevelUp_MaxHealth` | 위 + Additional Effects 컴포넌트 → On Application: `GE_LevelUp_Heal`, `Copy Data From Original Spec` = true |
| `GE_LevelUp_Heal` | Instant, CurrentHealth Add, SetByCaller `Data.LevelUp` |
| `GE_Chaser_FullHeal` | Instant, CurrentHealth Override, Attribute Based (MaxHealth, Target, Snapshot 끔). Chaser 로드아웃 4개 `StartupEffects`의 Init **다음**에 추가 |
| `DA_CB_LevelUpData` | 곡선, `MaxLevel`(0 = 무제한), 카드 목록(카드마다 `Grade`), `GradeWeights`(쓰는 등급 전부, 비우면 균등), 카드 위젯 클래스, **`GradeStyles`(쓰는 등급 전부)** → `GI_EasyMainGameInstance`의 `Level Up Data`에 지정. 곡선은 X = 레벨(정수), Y = 다음 레벨까지 필요량. **마지막 키 뒤에도 늘어나게 하려면 `Create External Curve`로 `CurveFloat` 에셋을 만들어 그 에디터에서 Post-Infinity를 Linear로** 둔다 — 디테일 패널의 내장 미니 에디터는 외삽 메뉴가 없어 기본값(Constant, 마지막 값 유지)에 머문다. `GetRequiredExperience`는 외부 에셋이 지정돼 있으면 그쪽을 읽는다 |
| 카드 위젯 WBP | `UCBLevelUpWidget` 상속. `BPI_EGUI_UINavigationInterface` 직접 구현(→ [EasyGameUI.md](../Presentation/EasyGameUI.md) "C++ 위젯") — Menu 레이어 선언값(Input Config ✅ / Focus Handler ✅ / Global Occluder ❌ / Remove Gameplay IMCs ✅). `OnCardsOffered`로 카드 칸 채움, 버튼 → `Local_SelectCard(슬롯)`, `OnCardSelected`로 대기 표시, 남은 시간은 `GetRemainingSelectionTime` 바인딩 |
| 경험치 바 WBP | `UCBExperienceBarWidget` 상속, HUD 컨테이너 WBP(`WBP_CB_HUD`)에 배치. `OnExperienceChanged`로 바·레벨 텍스트 갱신 (필요량 0 = 데이터 없음) |
| Rogue / Outlaw 로드아웃 | `ExperienceReward` |
| `GM_CB_Gameplay` | `Game State Class`가 `CBGameplayGameState`인지 확인 (BP에서 덮어쓰지 않았는지) |

## 검토했지만 채택하지 않은 것

| 대안 | 채택하지 않은 이유 |
|---|---|
| 경험치를 어트리뷰트로 | 공유 값이라 플레이어별 ASC에 둘 이유가 없음 |
| 월드 타이머로 타임아웃 | 정지 중에 멈춤 |
| 카드 위젯을 캐릭터 HUD 위젯이 띄우기 | 리스폰 중에는 HUD 위젯이 없어 카드 제시를 놓침 |
| 카드 수치를 GE 에셋에 직접 | MaxHealth 카드의 회복 GE와 수치가 두 곳에 생김 |
| 다음 레벨 필요량 복제 | 클라이언트도 같은 데이터에서 계산 가능 |
| 남은 시간 서버 동기화 | 같은 설정값으로 로컬 카운트다운, 지연분은 서버가 먼저 끝내므로 무해 |
| 처치자 판정 | 경험치 공유 + AI를 죽일 수 있는 건 플레이어뿐 |
| 난입 플레이어 따라잡기(레벨·보너스) | `bAllowJoinInProgress = false`로 게임 도중 접속을 거부함 |
| 다른 플레이어 선택 현황("2/4") | 요청 없음. 본인 대기 표시만 로컬로 |
| 자동 선택된 카드 알림 | 요청 없음. 카드 창은 다음 레벨업이나 종료로 넘어감 |
| 카드마다 개별 뽑기 가중치 | 등급 확률이 등급별 카드 수에 끌려감. 등급 먼저 고르는 두 단계로 대체 (→ "등급별 뽑기 확률") |
| 등급마다 카드 위젯 따로 | 로직 복제 + 본체의 클래스 선택 분기. 겉모습만 다르므로 데이터로 갈아 끼움 (→ "등급") |
| 맵을 넘어 레벨 유지 | 게임플레이 맵이 하나. 매치마다 초기화(게임 스테이트·PlayerState ASC가 새로 생김) |

## 알려진 한계

- **마지막 대기자가 나간 직후 다음 레벨업이 이어지면** 그 레벨업이 나간 플레이어에게도 카드를 보낸다 — `Logout` 시점에는 컨트롤러가 아직 목록에 있다. 그 플레이어는 고를 수 없으므로 **타임아웃까지 기다린 뒤** 끝난다. 한 번에 여러 레벨이 오르는 순간에 누가 나가는 경우라 그대로 둔다.
- **재개는 서버가 먼저다.** 해제 후 서버 시간이 다시 흐르고 나서 WorldSettings가 평소 주기로 복제되므로, 클라이언트는 그 한 번의 갱신 간격만큼 늦게 재개된다.

## 검증

1. AI를 처치하면 경험치 바가 오르고, 다 차면 리슨 서버·클라 화면이 함께 멈추며 각자 다른 카드 3장이 뜬다
2. 한 명이 고르면 그 사람만 대기 표시, 전원이 고르면 함께 재개된다
3. 아무도 고르지 않으면 타임아웃 뒤 각자 무작위 카드가 적용되고 재개된다 (`showdebug abilitysystem`으로 GE 확인)
4. 한 번에 2레벨이 오르면 재개 없이 카드 선택이 두 번 이어진다
5. MaxHealth 카드 → 최대·현재 체력이 같은 양만큼 오른다. 죽고 리스폰하면 보너스 포함 꽉 찬 체력
6. 선택 도중 클라 한 명이 나가도 나머지가 고르면 재개된다
7. 정지 중 마우스·게임패드 모두로 카드를 고를 수 있다 (EGUI 내비 입력이 정지 중에도 동작하는지)
8. 정지 중 버스트 남은 시간·리스폰 대기도 멈췄다가 재개 후 이어진다

## 관련 문서

- 서버 권위·정지 중 복제 규칙: [Multiplayer.md](../Conventions/Multiplayer.md)
- 게임모드·게임 스테이트 계층, 리스폰: [GameFlow.md](../Flow/GameFlow.md)
- Menu 레이어 다리·C++ 위젯의 내비게이션 인터페이스: [EasyGameUI.md](../Presentation/EasyGameUI.md)
- 위젯 배치 기준·경험치 바: [UI.md](../Presentation/UI.md)
- 카드 효과가 유지되는 이유(ASC 소유): [ASC-Ownership.md](../Foundation/ASC-Ownership.md)
- AI 보상 등록: [Loadout.md](../Foundation/Loadout.md)
