# ASC 소유 구조

> AbilitySystemComponent(ASC)와 AttributeSet을 누가 소유하고 어떻게 캐싱하는지 정의한다.

- **플레이어 캐릭터(Chaser)**: `ACBPlayerState`가 `UCBAbilitySystemComponent`와 `UCBAttributeSet`을 소유. 캐릭터는 PlayerState에서 캐싱(`PossessedBy` / `OnRep_PlayerState`).
- **AI 캐릭터(Outlaw 등)**: Character 자체가 ASC와 AttributeSet을 소유.
- 베이스 클래스의 `CBASC`, `CBAttributeSet`은 항상 포인터 캐싱용이며, 실제 소유는 위 규칙에 따름.

## 복제 모드 (GameplayEffect Replication Mode)

ASC를 누가 소유하느냐에 따라 복제 모드를 다르게 설정해 복제 비용을 최소화한다.

- **기본값 `Minimal`**: `UCBAbilitySystemComponent` 생성자에서 설정. GameplayEffect는 아무에게도 복제하지 않고 GameplayTag·GameplayCue만 전 클라에 복제 → **AI(Outlaw/Rogue)** 용 기본값.
- **플레이어는 `Mixed`로 오버라이드**: `ACBPlayerState`에서 `SetReplicationMode(Mixed)`. GameplayEffect를 **소유 클라이언트에만** 복제(버프 타이머 UI·예측 보정용), Tag·Cue는 전 클라에 복제.

| 모드 | GameplayEffect | Tag/Cue | 용도 |
|---|---|---|---|
| Full | 전 클라 | 전 클라 | 싱글플레이 |
| Mixed | 소유 클라만 | 전 클라 | 플레이어 |
| Minimal | 없음 | 전 클라 | AI/NPC |

**주의**
- Attribute 값은 복제 모드와 무관하게 항상 복제됨(AttributeSet의 복제 프로퍼티). 모드가 줄이는 것은 GameplayEffect 복제뿐 → Minimal AI도 체력 등은 정상 표시.
- `Mixed`는 유효한 소유 커넥션(PlayerState→PlayerController)이 필요 → **AI에는 사용 금지**, 플레이어(PlayerState 소유 ASC)에만.
- 플레이어에 `Minimal` 금지(자기 GE 미수신 → 버프 UI·예측 보정 깨짐).

## 로드아웃 부여분은 회수할 수 있어야 한다

**플레이어의 ASC는 PlayerState 소유라 폰보다 오래 산다.** 캐릭터를 바꿔 다시 스폰하면(→ [GameFlow.md](GameFlow.md) "무기 변경 = 캐릭터 변경") 이전 로드아웃이 부여한 어빌리티·스탯 GE가 그대로 남은 채 새 로드아웃이 얹혀 **바꿀수록 쌓인다.** AI는 ASC가 캐릭터 소유라 이 문제가 없다 — 소유 구조가 만드는 수명 차이다.

그래서 로드아웃의 부여는 전부 **핸들을 기록하는 경로**를 지난다.

| 함수 (`UCBAbilitySystemComponent`) | 역할 |
|---|---|
| `Auth_GiveLoadoutAbility(Spec)` | 어빌리티 부여 + 핸들 기록 |
| `Auth_ApplyLoadoutEffect(Spec)` | 이펙트 적용 + 핸들 기록 (즉시 이펙트는 핸들이 남지 않아 추적 대상이 아님) |
| `Auth_ClearLoadoutGrants()` | 기록된 것만 회수 (이펙트 → 어빌리티 순) |

- **로드아웃이 `GiveAbility`/`ApplyGameplayEffectSpecToSelf`를 직접 부르지 않는다.** 직접 부르면 기록에서 빠져 회수되지 않는다 (`UCBCharacterLoadout`·`UCBChaserLoadout` 양쪽 모두 이 경로를 쓴다).
- **회수 대상은 로드아웃이 준 것뿐이다.** 전투 중 붙은 버프 등 로드아웃 밖의 것은 건드리지 않으므로 `ClearAllAbilities()`로 대체하지 않는다.

## 공용 라이브러리 (`UCBAbilitySystemLibrary`)

ASC 소유 구조가 캐릭터마다 달라서(플레이어=PlayerState, AI=Character), ASC 접근·질의는 소유 구조를 캡슐화한 **`UCBAbilitySystemLibrary`** 를 경유한다. 컴포넌트·애님 인스턴스 등 ASC를 자주 쓰는 곳이 소유 구조를 몰라도 되게 하는 것이 목적.

**함수 배치 기준** — "이 함수 어디에 두지?"의 답:

| 위치 | 기준 | 예 |
|---|---|---|
| **`UCBAbilitySystemLibrary`** | **상태 없는(stateless) ASC/태그 질의·GE 적용 유틸리티.** 특히 두 곳 이상이 공유하는 공용 판정은 여기 두고 중복 구현 금지 | `GetASC`, `HasGameplayTag`, `IsCombatMode`, `GetCurrentGaitTag`, `GetGaitMontageIndex`, GE 스펙 생성/적용 |
| 컴포넌트 | 상태를 소유(캐싱·타이머)하거나 **태그를 부여/제거**하는 로직 — 라이브러리는 읽기 전용, 쓰기는 소유자 컴포넌트가 담당 | `UCBCombatComponent`의 InCombat 부여, `UCBLocomotionProcessor`의 파생 상태 미러링(예정) |
| 어빌리티 | 해당 어빌리티 전용 로직 (공용화 필요가 생기면 라이브러리로 승격) | `UCBGADash::SelectActionMontageIndex`의 전투 분기 |

- 라이브러리 함수는 BP에서도 쓸 수 있게 필요 시 `UFUNCTION(BlueprintPure/Callable)` + `DefaultToSelf` 지정 (C++ 전용이면 생략).
- **라이브러리에 태그 Add/Remove 함수를 만들지 않는다** — 상태 태그의 소유자 단일화 규칙([GameplayTags.md](GameplayTags.md))이 깨진다.

## 관련 문서
- 초기화 흐름(`PossessedBy` / `OnRep_PlayerState` → `InitializePlayerSystem`): [Multiplayer.md](Multiplayer.md)
- 캐릭터가 참조하는 에셋 일괄 적용: [Loadout.md](Loadout.md)
