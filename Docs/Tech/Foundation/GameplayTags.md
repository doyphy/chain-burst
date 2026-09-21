# 게임플레이 태그 시스템 (`CBGameplayTags`)

> 태그의 4역할 분류, 생성 기준, 상태 태그의 소유권·복제 규칙, 어빌리티 태그 프로퍼티(GE vs 자체 설정) 선택 기준, 네임스페이스 인덱스. 태그를 추가·변경할 때 반드시 이 문서의 기준을 따른다.

## 태그의 4역할

모든 태그는 아래 4역할 중 정확히 하나에 속한다. 역할이 정해지면 네임스페이스·소유권·복제 규칙이 따라온다.

| 역할 | 정의 | 질의 방향 | 중복 | 네임스페이스 |
|---|---|---|---|---|
| **식별** | "무엇인지" 이름표. 태그로 대상을 **찾는다** | 태그 → 대상 | 유일해야 함 (중복 = 버그) | `Ability.*` `Action.*` `Input.*` `Item.*` `Data.*` `GameplayCue.*` |
| **속성** | 에셋에 붙여 그 에셋의 성격을 나타냄. 대상이 성질을 갖는지 **검사한다** | 대상 → 태그 | 여러 에셋 공유가 정상 | `Effect.*` |
| **상태** | ASC에 부여되는 동적 태그. **소유자 + 복제 규칙 필수** (아래 절) | ASC 질의 | — | `Status.*` `Cooldown.*` |
| **이벤트** | 순간 신호. 부여되지 않고 흘러감 (애님노티파이 등) | — | — | `Event.*` |

## 태그를 만드는 기준

새 개념이 생기면 아래 순서로 판정한다:

1. **ASC에 부여되어 외부가 질의하는가?** → 상태
2. **순간 신호인가?** → 이벤트
3. **에셋의 성격을 표시하는가?** (여러 에셋 공유 가능) → 속성
4. **유일한 이름표로 대상을 조회하는가?** → 식별

**태그로 만들면 안 되는 것:**
- 연속값(속도, 각도, 체력) → Attribute/멤버. 태그는 이산 상태만 (`Data.*`는 값이 아니라 SetByCaller의 키라서 예외)
- 한 클래스 내부에서만 쓰는 값(콤보 인덱스, `bIsInAir` 캐시) → 멤버 변수
- 기존 태그 조합으로 파생 가능한 상태("Sprint 중 + 공중") → 새 태그가 아니라 쿼리
- 소유자(누가 Add/Remove)를 정할 수 없는 상태 → 설계 미완, 태그 보류

## 상태 태그의 소유권·복제 규칙

상태 태그는 반드시 **소유자(Add/Remove 담당 한 곳)** 와 **복제 경로(아래 ①~④ 중 하나)** 를 정하고 태그 정의 주석에 남긴다.

| 경로 | 서버 | 오너 클라 | 시뮬 프록시 | 예측 롤백 | 제거 |
|---|---|---|---|---|---|
| **① GE GrantedTags** (기본) | ✅ | ✅ 예측 | ✅ 자동 복제 | 자동 | GE 제거와 함께 자동 |
| **② ActivationOwnedTags** | ✅ | ✅ | ❌ | 어빌리티 종료와 연동 | EndAbility 시 자동 |
| **③ 수동 루스 태그** | ✅ | ✅ 예측 | ✅ 복제 (`TagOnly` 등 복제 상태 지정 시) | **없음 (수동)** | **수동 (실패 경로 포함)** |
| **④ 파생 상태 로컬 미러링** | ✅ 각자 계산 | ✅ 각자 계산 | ✅ 각자 계산 | 불필요 | 미러링 로직이 담당 |

- **① GE GrantedTags (기본)**: 어빌리티가 GE로 부여. 프록시 복제·예측 롤백을 GAS가 전부 처리하므로 고민되면 이 방식. 예: 개이트 태그(`GA_Walk`/`GA_Sprint`의 속도 GE).
- **② ActivationOwnedTags**: "어빌리티 활성 구간 = 태그 구간"이 정확히 일치하고 프록시가 필요 없을 때. 복제가 아니라 **어빌리티가 실행되는 머신(서버+오너)에서의 병렬 실행**이므로 프록시에는 도달하지 않음. 예: `Status.Movement.Dashing`.
- **③ 수동 루스 태그**: GE로 표현하기 어려운 전환 타이밍(애님노티파이 등)만. **서버와 예측 클라가 동일한 `EGameplayTagReplicationState`로 `AddLooseGameplayTag()`를 1회씩만 호출**한다 — 표준 구현은 `UCBCombatComponent::OnEnterCombatMode/OnExitCombatMode`. 온/오프 상태면 `TagOnly`(카운트 무의미, 엔진이 클라 카운트를 1로 클램프), 스택이 의미 있으면 `TagAndCountToAll`. 제거도 같은 복제 상태로 호출. **몽타주 캔슬·예측 실패 등 실패 경로에서의 제거 누락이 단골 버그** (리뷰 체크포인트). 예: `Status.Combat.InCombat`.
  - **폰이 사라질 때도 반드시 뺀다.** 플레이어 ASC는 PlayerState 소유라 폰 재스폰(사망 리스폰·무기 변경)을 넘어 살아남으므로, 루스 태그를 남기면 다음 폰이 그 상태를 물려받는다. 붙인 컴포넌트가 **`EndPlay`에서 자기가 붙인 것만**(적용 플래그로 판별 — 복제로 받기만 한 시뮬 프록시는 제외) 걷어내는 것이 규칙이다. `UCBCombatComponent`(`Status.Combat.InCombat`)와 `UCBLocomotionProcessor`(`Idle`/`InAir`/`Run`)가 이 형태다.
  - ⚠️ **금지 패턴**: "로컬에 `None`으로 추가 + `HasAuthority()`면 복제 상태로 또 추가". UE 5.5+ API 계약(*"Any state besides None must be added on the server and can be predicted on clients"*)에 어긋나 서버 카운트가 2가 되고, 클라에는 `Attempting to predict <Tag> tag addition, but potentially non-replicated tag already exists` 경고가 뜬다.
- **④ 파생 상태 로컬 미러링**: 어빌리티가 진입점이 될 수 없는 물리/이동 파생 상태(Idle, InAir). 각 머신이 자기 CMC 데이터를 **읽어서**(변경 없음) 비복제 루스 태그를 로컬 부여/제거 — CMC 자체가 복제되므로 서버/오너/프록시가 각자 같은 결론에 도달하고, **서버도 자기 미러링 태그로 권위 판정 가능**. 소유자는 `UCBLocomotionProcessor`로 단일화. 주의: 반드시 비복제 API 사용(복제 API와 섞으면 카운트 꼬임), 임계값 경계 상태는 히스테리시스로 플래핑 방지, 프록시는 보간 데이터라 전환 타이밍이 몇 프레임 어긋날 수 있음.

**②③은 어빌리티 NetExecutionPolicy에 따라 적용 범위가 달라진다**: `LocalPredicted`/`ServerInitiated` = 서버+오너, `ServerOnly` = 서버만(오너도 못 봄 — 상태 태그는 ①로만), `LocalOnly` = 로컬만(게임플레이 판정용 상태 태그 부여 금지, 연출 전용).

## 어빌리티가 태그를 다루는 두 계열 — 쓰기와 읽기

어빌리티 클래스 디폴트의 태그 프로퍼티 8개는 성격이 둘로 갈리고, **어느 쪽이냐에 따라 복제를 걱정할 지점이 달라진다.**

| 계열 | 프로퍼티 | 하는 일 | 복제 |
|---|---|---|---|
| **쓰기** | `ActivationOwnedTags` | `PreActivate`에 루스 태그 부여 / `EndAbility`에 제거 | 기본 없음. 옵트인(`ShouldReplicateActivationOwnedTags()`) 시에도 `CountToOwner`(오너 전용) |
| | `BlockAbilitiesWithTag` | ASC의 `BlockedAbilityTags` 카운터 증감 | **복제 옵션 자체가 없음** — 순수 로컬 |
| | `CancelAbilitiesWithTag` | `CancelAbilities()` 즉시 호출 | 없음 (호출 자체가 로컬 행위) |
| **읽기** | `ActivationRequiredTags` / `ActivationBlockedTags` / `SourceRequired·BlockedTags` / `TargetRequired·BlockedTags` | **아무것도 적용하지 않는다.** 활성화를 시도하는 머신에서 ASC의 현재 태그를 조회만 함 | — |

- **쓰기 계열이 미치는 범위는 "오너 클라"가 아니라 "어빌리티가 실행되는 머신"이다.** `NetExecutionPolicy`가 그걸 정한다 — `LocalPredicted`/`ServerInitiated`=서버+오너, `ServerOnly`=서버만, **AI는 오너 클라가 없으므로 정책과 무관하게 언제나 서버만**. 시뮬 프록시는 어빌리티 자체가 돌지 않아 절대 생기지 않는다.
- **읽기 계열은 "로컬이냐"를 물을 대상이 아니다.** 관건은 **읽히는 태그의 출처**다. 부여와 검사가 같은 머신에서 끝나면 어긋날 일이 없다(`Status.Combat.SuperArmor`가 그 형태).
  - ⚠️ `LocalPredicted` 어빌리티가 **복제되지 않는 태그**(②·④)를 검사에 쓰면 클라는 통과·서버는 거부 → **예측 롤백**. 예측 어빌리티의 검사 대상은 ① GE GrantedTags처럼 복제되는 출처에서 와야 한다.

## ① GE와 ② 어빌리티 자체 프로퍼티 중 무엇을 고를까

②는 부여·제거가 어빌리티 수명에 자동으로 묶여 실수할 여지가 없고 에셋도 늘지 않는다. 그래서 **②로 될 때는 ②를 쓰고, 안 될 때만 ①로 간다.** 판정은 두 질문이다:

> **1. 시뮬 프록시에 이 태그의 소비자가 있는가?**
> **2. 있다면, 그 소비가 다른 동기화 경로로 이미 대체되는가?**

| 상황 | 경로 |
|---|---|
| 소비자가 서버(또는 서버+오너)뿐 — 어빌리티 발동 판정·차단·AI 두뇌 | **② ActivationOwnedTags** |
| 프록시도 결과를 봐야 하지만, 그 결과가 **GameplayCue 몽타주 등 별도 동기화 경로**로 이미 전달됨 | **② ActivationOwnedTags** — 태그까지 복제할 이유가 없다 |
| 프록시가 **태그 자체를** 읽어야 하는데 대체할 연출 경로가 없음 — ABP 상태머신 분기, 머리 위 UI, 시체 정리 | **① GE GrantedTags** |

- `Status.Combat.SuperArmor` → ②. 판정도 몽타주 스킵도 어빌리티 발동 단계에서 끝난다.
- `Status.Combat.Staggered`(경직) → ②. 경직 연출인 피격 몽타주는 `GameplayCue.PlayAction`으로 이미 전 클라에 가고, 태그의 소비자는 서버의 BT/블랙보드뿐이다. **"프록시가 경직을 알아야 한다"처럼 보이지만, 알아야 하는 건 태그가 아니라 모션이고 그 경로는 따로 있다.**
- `Status.Dead` → ①. 시뮬 프록시 캐릭터가 태그 콜백으로 충돌·UI를 각자 정리해야 하는데, 그 일을 대신해 줄 연출 경로가 없다 (→ [Abilities.md](../Gameplay/Abilities.md) "사망 처리의 세 갈래").

> 어빌리티가 부여 주체가 아니거나(BT 서비스·컴포넌트) 어빌리티 수명과 태그 구간이 어긋나면 ②가 성립하지 않는다. 그때는 ③·④로 간다 — `Status.Movement.Strafe`는 ABP가 직접 읽어야 해 복제가 필수인데 부여 주체가 BT 서비스라 ③이다.

## 네임스페이스 인덱스

| 네임스페이스 | 역할 | 용도 |
|---|---|---|
| `Input.*` | 식별 | InputAction과 바인딩되는 태그. `Input.Action.*`(액션 입력), `Input.UI.*`(예정) |
| `Item.*` | 식별 | 아이템/무기 식별 (`Item.Weapon.Sword`), 의상 파츠 식별 (`Item.Cosmetic.*` — 아래 구조) |
| ↳ `Item.Weapon.*` | 식별 | **무기 종류가 곧 캐릭터 종류**라 캐릭터 선택 키를 겸한다 (`UCBCharacterCatalog` 조회 → 스폰할 캐릭터 클래스) → [GameFlow.md](../Flow/GameFlow.md) |
| `Data.*` | 식별 | SetByCaller 전용 키 (Damage, Speed, AttackPower 등) |
| `Ability.*` | 식별 | 어빌리티 식별 태그 (AssetTags) |
| `Action.*` | 식별 | 몽타주 식별 태그 (UCBActionComponent에서 몽타주 선택) |
| `GameplayCue.*` | 식별 | 게임플레이 큐 라우팅 |
| `Effect.*` | 속성 | GE 동작 의도 선언 (Opt-in, 여러 GE 공유 가능) |
| `Status.*` | 상태 | 캐릭터 상태. `Status.Combat.*`(전투 — `InCombat`, `SuperArmor`, `Staggered`), `Status.Movement.*`(이동 — 아래 구조), `Status.Dead`(사망) |
| `Cooldown.*` | 상태 | 어빌리티 쿨다운 (쿨다운 GE의 GrantedTags — GAS가 자동 검사. 관례상 별도 루트 유지) |
| `Event.*` | 이벤트 | 애님노티파이 등 이벤트 트리거 |

**`Status.Movement.*` 구조** (2026-07 재편 — 구 `Movement.*`는 리다이렉트):

```
Status.Movement.Gait.Walk / .Run / .Sprint  ← 개이트 축 — 항상 정확히 1개 (Walk/Sprint는 ① GE 의도 태그, Run은 ④ LocomotionProcessor가 "둘 다 없음"에서 파생)
Status.Movement.Idle / .InAir               ← 파생 상태 축 — ④ LocomotionProcessor 로컬 미러링 (개이트와 공존, Idle은 지상 전용이라 InAir와는 배타)
Status.Movement.Dashing                     ← ② ActivationOwnedTags (GA_Dash)
Status.Movement.Strafe                      ← ③ 수동 루스 태그(TagOnly) — UCBBTService_StrafeFocus 가 경계 브랜치 진입/이탈에 맞춰 토글
Status.Movement.Overridden                  ← 속도 오버라이드 GE
```

`Status.Movement.Strafe` 는 **애님이 2D 블렌드스페이스로 전환하는 기준**이다. BT(서버)에서만 갱신되므로 `TagOnly` 로 복제해야 시뮬 프록시에서도 같은 모션이 나온다. 소비자는 `UCBAIAnimInstance` (→ [AnimInstance.md](../Presentation/AnimInstance.md), [AI.md](../Gameplay/AI.md)).

**`Status.Dead`** — 소유자는 `GE_Dead`(무한 지속)의 GrantedTags, 복제 경로 ①. 부여 주체는 `UCBDeathAbility`이며, 이 태그를 들고 있으면 `UCBGameplayAbility::CanActivateAbility`가 사망 전용 외 모든 어빌리티를 차단한다 (→ [Abilities.md](../Gameplay/Abilities.md)). 제거 주체는 `ACBGameplayGameMode`로, 리스폰 직전에 이 태그를 부여한 GE를 `RemoveActiveEffectsWithGrantedTags`로 지운다 (→ [GameFlow.md](../Flow/GameFlow.md) "사망과 리스폰"). AI는 제거하지 않고 그대로 파괴된다.

**`Status.Combat.Staggered`** — 복제 경로 ②(`ActivationOwnedTags`). 부여 주체는 `UCBHitReactAbility` 하나이고, **어빌리티 활성 구간이 곧 경직 구간**이다(= 피격 몽타주의 `Event.Action.EndAbility` 노티파이 위치가 경직 길이를 정한다). 소비자는 `ACBAIController` 로, 태그 변화를 블랙보드 bool 키로 미러링해 BT 가 경직 분기로 빠지게 한다 (→ [AI.md](../Gameplay/AI.md) "피격 경직"). 시뮬 프록시는 이 태그를 알 필요가 없다 — 프록시가 봐야 하는 경직 연출은 피격 몽타주이고 그건 GameplayCue 로 이미 동기화된다.

**`Status.Combat.SuperArmor`** — 복제 경로 ②(`ActivationOwnedTags`). 부여 주체는 "피격에 끊기지 않아야 하는" 어빌리티 자신(스킬 BP 등)이고, 소비자는 `UCBHitReactAbility`의 `ActivationBlockedTags` 하나뿐이다 (→ [Abilities.md](../Gameplay/Abilities.md) "슈퍼아머"). 서버와 오너 클라에서만 존재하면 충분하다 — 판정도 몽타주 스킵도 어빌리티 발동 단계에서 끝나므로 시뮬 프록시는 이 태그를 알 필요가 없다.

`Gait` 중간 계층 덕에 `HasTag(Status.Movement.Gait)` 부모 매칭으로 "개이트 태그 보유 여부"를 한 번에 검사할 수 있다. 개이트 판별은 `UCBAbilitySystemLibrary::GetCurrentGaitTag()` 공용 헬퍼 사용 (Sprint > Walk > 기본 Run).

**`Item.Cosmetic.*` 구조** — 모듈러 의상 파츠 식별 (시스템 전반은 → [Cosmetic.md](../Presentation/Cosmetic.md)):

```
Item.Cosmetic.<슬롯>.<테마><번호>      예: Item.Cosmetic.Torso.SciFi02
```

- **슬롯**은 `ECBCosmeticSlot`(Helmet/Torso/Legs/Feet)과 같은 이름을 쓴다. 단 슬롯의 **권위는 태그가 아니라 `FCBCosmeticPart::Slot` 필드**다 — 태그 이름은 사람이 읽기 위한 것이라 어긋날 수 있다.
- **에셋 이름에서 의미 있는 부분만 옮긴다.** 타입 접두사(`SKM_`)·엔진 버전(`UE5`)·슬롯과 중복되는 종류(`Jacket`)는 버리고, 테마와 팩 번호는 남겨 원본 에셋을 역추적할 수 있게 한다.
  `SKM_SciFi_Jacket_P02_UE5` → `Item.Cosmetic.Torso.SciFi02`
- 사람에게 보여줄 이름은 태그가 아니라 `FCBCosmeticPart::DisplayName`이 담당하므로, **태그 이름을 예쁘게 지으려 애쓰지 않는다.**

**`Item.Cosmetic.None`만 예외적으로 네이티브다.** 파츠 태그는 전부 ini지만, 이 하나는 **코드가 이름으로 비교**하므로 `CBGameplayTags`에 선언한다. 세 상태를 구분하기 위한 값이다:

| 값 | 의미 | 동작 |
|---|---|---|
| **빈 태그** | 선택 안 함 | 아무것도 하지 않음 → **로드아웃의 기본 의상 유지** |
| **`Item.Cosmetic.None`** | 명시적으로 벗음 | 그 슬롯 메시를 `nullptr`로 |
| 그 외 파츠 태그 | 그 파츠 착용 | 카탈로그 조회 → 비동기 로드 → 적용 |

빈 태그를 "벗기"로 쓰면 **`ACBPlayerState::Cosmetics`의 초기값(빈 태그 4개)이 그대로 적용되어 기본 의상이 전부 벗겨진다.** 그래서 "선택 안 함"과 "벗기"를 반드시 나눠야 한다.

## 태그 추가·변경 절차

**등록 경로는 둘이고, 판별 기준은 한 줄이다 — "C++ 코드가 그 태그를 이름으로 직접 참조하는가?"**

| | 네이티브 (`CBGameplayTags.h/.cpp`) | ini (`DefaultGameplayTags.ini`) |
|---|---|---|
| 대상 | 코드가 이름으로 비교·부여하는 태그 | **데이터로 계속 늘어나는 식별 태그** |
| 예 | `Status.Movement.Gait.Sprint`, `Item.Weapon.Sword` | `Item.Cosmetic.Torso.SciFi02` |
| 추가 방법 | 헤더 선언 + cpp 정의 | 에디터 **태그 픽커 → Add New Gameplay Tag** |
| 항목 추가 시 | **재컴파일 필요** | 재컴파일 없음 |

- **추가(네이티브)**: `CBGameplayTags.h`에 선언 → `CBGameplayTags.cpp`에 정의(역할·소유자·복제 경로를 주석으로). 네이티브 태그는 자동 등록되므로 **ini 등록 불필요**.
- **추가(ini)**: 태그 픽커에서 바로 생성한다. 상위 계층은 자동으로 만들어지므로 따로 등록하지 않는다. 코드는 이 태그를 이름으로 참조하지 않고 데이터 에셋에서 조회만 하므로, 컴파일 타임 안전성이 필요 없다. 픽커 오염을 막기 위해 프로퍼티에 `meta = (Categories = "...")` 필터를 건다.
  - **의상 파츠를 네이티브로 만들면 안 되는 이유**: 의상팩 하나 추가할 때마다 C++ 수정·재컴파일이 필요해져 데이터 확장이 코드에 묶인다.
- **이름 변경/이동**: 코드 치환 + `DefaultGameplayTags.ini`에 `GameplayTagRedirects` 추가 (구 태그를 참조하는 GE/DA/BP 에셋이 로드 시 자동 리맵). 리다이렉트 후 해당 에셋을 열어 재저장해서 새 태그로 확정.
- 태그 픽커 필터(`meta = (Categories = "...")`)를 쓰는 프로퍼티가 있으면 경로 변경 시 함께 갱신할 것 (예: `CBGAChangeSpeed::SpeedDataTag`, `CBCharacterMovementData::MovementDataMap`).

## 관련 문서
- 상태 태그가 많이 쓰이는 로코모션(개이트/파생 상태): [Locomotion.md](../Gameplay/Locomotion.md)
- 서버 권위·NetExecutionPolicy 상세: [Multiplayer.md](../Conventions/Multiplayer.md)
