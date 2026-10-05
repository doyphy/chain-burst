# 기절 (보스 그로기)

> 보스(Outlaw)의 기절 게이지(`StunGauge`)가 피격과 자기 스킬 사용으로 쌓이고, 가득 차면 일정 시간 기절해 플레이어에게 공격 기회를 준다. **게이지 적립은 패시브 하나(C++), 기절 동작은 기존 이벤트 액션 어빌리티의 BP** 로 만든다 — 버스트([Burst.md](Burst.md))와 같은 "적립 패시브 + 발동" 구조다.

## 흐름

```
[서버] 피격 (Event.Combat.HitReact)                   ─┐
[서버] 자기 어빌리티 정상 종료 (ASC OnAbilityEnded)      ─┤→ UCBStunGaugeAbility (패시브)
                                                       │     StunGauge = min(현재 + 적립량, 100)
                                                       │     기절 중(Status.Combat.Stunned)이면 쌓지 않음
                                                       └→    100 도달: StunGauge = 0 + Event.Combat.Stunned
Event.Combat.Stunned ─→ GA_Stun (UCBEventActionAbility BP — 코드 없음)
                          · CancelAbilityTag = Ability.Combat.Attack           진행 중 공격 끊음
                          · ActivationOwnedTags = Status.Combat.Stunned
                                                + Status.Combat.Staggered        BT 경직 분기가 받음
                                                + Status.Combat.SuperArmor       피격 반응이 기절 모션을 끊지 않게
                          · 몽타주 Action.Combat.Stunned (쓰러짐 → 누움 → 일어남, 길이 = 기절 시간)
BT ─→ Status.Combat.Staggered → 블랙보드 bIsStaggered → 경직 분기(Lower Priority 중단)
```

## 구성 요소

| 요소 | 위치 | 역할 |
|---|---|---|
| `StunGauge` | `UCBAttributeSet` | 0 ~ `MaxStunGauge`(상수 100). 서버만 기록하고 전 클라에 복제(보스 UI 용) |
| `UCBStunGaugeAbility` | 로드아웃 `PassiveAbilities` | `OnGiven` + `ServerOnly`. 피격·자기 어빌리티 종료마다 적립, 가득 차면 비우고 기절 이벤트 |
| `GA_Stun` | `UCBEventActionAbility` BP | 트리거 `Event.Combat.Stunned`. 공격 캔슬 + 기절 몽타주 + 상태 태그 셋 |
| `Status.Combat.Stunned` | 기절 어빌리티 `ActivationOwnedTags` | 기절 중 적립 중단. 이후 UI·추가 피해의 기준 |

## 적립 — 두 경로

### ① 피격 — 맞을 때마다 고정값

- **피격 반응 이벤트(`Event.Combat.HitReact`)를 구독한다.** 새 배관 없이 AI 위협 판정(→ [AI.md](AI.md) "위협 입력")과 같은 신호를 쓴다. 공격자 쪽(무기 트레이스·투사체 등)은 기절을 모른다.
- **고정값(`GaugeGainPerHit`)** — 무기·피해량과 무관하게 "몇 대 맞으면 쓰러지는지" 예측하기 쉽게. 피해량 비례는 택하지 않았다.
- **슈퍼아머 중에도 쌓인다.** 이 이벤트는 반응이 차단돼도 발행되므로(`UCBAttributeSet` 은 `Effect.HitReact` 만 본다) 보스가 스킬을 쓰는 중에 맞아도 적립된다 — "보스가 기술을 쓸 때 몰아치면 빨리 쓰러진다"가 자연히 성립한다.
- 한계: 이 이벤트는 GE 의 `Effect.HitReact` **옵트인**이라 태그 없는 지속(DoT)·환경 피해는 쌓지 않는다. 체력이 0 이 되는 타격도 스킵되지만 죽는 순간이라 무관.

### ② 자기 어빌리티 — 정상 종료 시점

- **ASC `OnAbilityEnded` 를 구독**해, 끝난 어빌리티의 식별 태그(Asset Tags)를 **`AbilityGaugeGains` 표**에서 **정확히 같은 태그로** 찾아 그 양만큼 쌓는다. 표에 없는 어빌리티(피격·기절·패시브 등)는 무시된다. 공격 어빌리티는 기절을 모른다.
- **활성화가 아니라 정상 종료 시점인 이유**: 활성화 때 쌓으면 게이지를 채운 그 스킬이 **시작하자마자 기절에 끊긴다.** 종료 때 쌓으면 "큰 스킬을 다 쓰고 나서 지쳐 쓰러지는" 흐름이 되고, 피격·기절에 **끊긴(캔슬) 스킬은 쌓이지 않는다.**
- **정확한 태그만 매칭한다.** 부모 태그(예: `Ability.Combat.Attack.Skill`)로 묶으면 부모·자식 키가 함께 걸릴 때 어느 값을 쓸지 규칙이 필요해진다. 보스 어빌리티는 몇 개뿐이라 하나씩 적는 편이 단순하다.

### 가득 차면

- **게이지를 먼저 비우고 이벤트를 발행한다.** 기절 어빌리티가 같은 호출 안에서 `Status.Combat.Stunned` 를 소유하므로, 기절이 끝날 때까지 게이지는 0 에 머문다(기절 중 적립 중단).
- **발동된 어빌리티가 없으면 경고 로그**(`HandleGameplayEvent` 반환값 0) — 로드아웃 부여·트리거 설정·발동 조건 누락. 게이지만 조용히 비워지는 것을 막는다.
- **피격으로 가득 찬 경우의 순서**: `HandleGameplayEvent` 는 트리거 어빌리티를 먼저 발동하고 대기 태스크에 나중에 알린다. 그래서 피격 반응이 먼저 시작되고, 바로 이어 기절이 발동해 **기절 몽타주가 피격 몽타주를 덮는다**(피격 어빌리티는 끊김 → 캔슬 종료).

## 기절 — 코드 없는 BP

`UCBEventActionAbility` 가 이미 "이벤트로 발동 → 지정 어빌리티 캔슬 → 몽타주 재생"을 한다(피격 반응·사망과 같은 베이스). 기절 고유의 것은 **태그 설정뿐**이라 C++ 클래스를 두지 않았다.

- **`Status.Combat.Staggered` 를 함께 소유해 BT 를 재사용한다.** 컨트롤러가 이미 경직 태그를 블랙보드(`bIsStaggered`)로 옮기고 BT 경직 분기가 `Lower Priority` 중단으로 이동·대기를 끊는다(→ [AI.md](AI.md) "피격 경직"). 컨트롤러·BT 코드 변경이 없다.
- **`Status.Combat.Stunned` 를 따로 두는 이유**: 경직(피격 반응) 중에는 기절 게이지가 **계속 쌓여야** 한다. 경직 태그 하나로 "기절 중 적립 중단"을 판단하면 피격 경직마다 적립이 멈춘다.
- **`Status.Combat.SuperArmor` 를 소유하는 이유**: 없으면 기절한 보스를 때릴 때마다 피격 반응이 재발동해 **기절 몽타주를 끊는다**(새 몽타주 재생 → 기절 몽타주 블렌드 아웃 → 기절 어빌리티 캔슬 종료). 슈퍼아머는 피격 반응 발동만 막고 데미지는 그대로 들어간다(→ [Abilities.md](Abilities.md) "슈퍼아머").
- **기절 시간 = 몽타주 길이.** 지속 시간 파라미터를 두지 않는다. 쓰러짐 → 누움 → 일어남을 한 몽타주로 만든다. ⚠️ **루프 섹션을 쓰면 블렌드 아웃이 오지 않아 어빌리티가 끝나지 않는다**(→ [Montage.md](Montage.md) "루프 섹션 몽타주는 블렌드 아웃이 오지 않는다").
- **`bIgnoreAbilityBlocking` 을 켠다.** 진행 중인 공격이 `BlockAbilitiesWithTag` 로 자기 계열(`Ability.Combat`)을 막고 있어도 기절은 발동해야 한다(피격 반응과 같은 설정).
- **기절 중 사망**: 사망 어빌리티의 `CancelAllAbilities` 가 기절을 끊고 태그도 함께 빠진다. 따로 처리할 것이 없다.

## 네트워크

| 값 | 기록 | 복제 |
|---|---|---|
| `StunGauge` | 서버 (`SetNumericAttributeBase`) | 어트리뷰트 복제 — 전 클라. 화면 상단 보스 바(`UCBStunGaugeWidget`)가 네트워크 코드 없이 표시 (→ [UI.md](../Presentation/UI.md) "보스 바") |
| 기절 상태 태그 | `GA_Stun` `ActivationOwnedTags` | 경로 ② (→ [GameplayTags.md](../Foundation/GameplayTags.md)) — 소비자가 서버뿐이라 충분 |
| 기절 모션 | `GameplayCue.PlayAction` | 전 클라 (→ [Montage.md](Montage.md)) |

AI 어빌리티는 서버 전용이라 예측·거부 처리가 없다.

## 초기화

**종료 시 게이지를 비우지 않는다.** AI 는 ASC 가 캐릭터에 있어 폰과 함께 사라지므로(→ [ASC-Ownership.md](../Foundation/ASC-Ownership.md)) 넘겨받을 다음 폰이 없다. 버스트 패시브가 종료 시 비우는 것은 플레이어 ASC 가 PlayerState 소유라 폰보다 오래 살기 때문이다. **이 패시브를 플레이어에게 쓰게 되면** 버스트처럼 `EndAbility` 에서 0 으로 되돌려야 한다.

패시브 종료 시 하는 일은 `OnAbilityEnded` 구독 해제뿐이다(피격 대기는 어빌리티 태스크라 함께 정리된다).

## 에디터 작업

| 에셋 | 설정 |
|---|---|
| `GA_StunGauge` | `UCBStunGaugeAbility` BP 자식. `GaugeGainPerHit`, `AbilityGaugeGains`(예: `Ability.Combat.Attack.Skill.A` 20 / `Skill.D` 30 / `Basic` 5) |
| `GA_Stun` | `UCBEventActionAbility` BP 자식. Triggers `Event.Combat.Stunned`(Gameplay Event), Asset Tags `Ability.Combat.Stunned`, `BoundActionTag = Action.Combat.Stunned`, `CancelAbilityTag = Ability.Combat.Attack`, `ActivationOwnedTags = Status.Combat.Stunned + Status.Combat.Staggered + Status.Combat.SuperArmor`, `bIgnoreAbilityBlocking = true` |
| 몽타주 데이터 | Outlaw 의 `Action.Combat.Stunned` 에 기절 몽타주 등록 (루프 섹션 없음, 길이 = 기절 시간) |
| Outlaw 로드아웃 | `PassiveAbilities += GA_StunGauge`, 어빌리티 목록 `+= GA_Stun` |

- 새 `UPROPERTY` 가 있으므로 코드 반영 후 **에디터를 재시작**해야 BP 에서 값이 보인다.
- 적립량의 기준은 `MaxStunGauge = 100` 이다(예: `GaugeGainPerHit` 5 = 20 대에 기절, 스킬 사용과 섞이면 더 빨리).

## 검토했지만 채택하지 않은 것

| 안 | 이유 |
|---|---|
| 자기 어빌리티를 활성화 시점에 적립 | 게이지를 채운 스킬이 시작하자마자 기절에 끊긴다 (위 ②) |
| 피해량 비례 적립 | 무기마다 기절까지의 타수가 달라 예측하기 어렵다 — 고정값 |
| 시간에 따른 게이지 감소 | 요청에 없음. 필요해지면 패시브에 주기 감소를 추가한다 |
| 공격 어빌리티가 스스로 적립 (어빌리티마다 적립량 프로퍼티) | 공격 기능이 기절을 알게 되고, 모든 AI 공격 BP 에 값 칸이 생긴다 — 버스트가 "무기 트레이스 조각에서 적립"을 택하지 않은 것과 같은 이유 |
| 기절 전용 C++ 어빌리티 | 이벤트 발동·캔슬·몽타주를 `UCBEventActionAbility` 가 이미 한다. 고유한 것은 태그 설정뿐 |
| 기절 시간 파라미터 + 루프 몽타주 | 루프 섹션은 블렌드 아웃이 오지 않아 종료 경로가 따로 필요하다. 몽타주 길이로 정하면 코드가 없다 |
| `MaxStunGauge` 어트리뷰트 | 차는 속도는 적립량으로 조절된다. 최대값을 바꿔야 할 소비자가 없다 (버스트와 같은 판단) |

## 검증

1. UBT 컴파일 통과 ✅
2. 보스를 때릴 때마다 `StunGauge` 가 `GaugeGainPerHit` 만큼 오른다 (`showdebug abilitysystem`) — 보스 스킬 중(슈퍼아머)에 때려도 오른다
3. 표에 있는 스킬을 **끝까지** 쓰면 그 양만큼 오르고, 피격으로 **끊긴** 스킬은 오르지 않는다
4. 100 이 되면 게이지가 0 이 되고 기절 몽타주가 재생되며, 진행 중이던 공격이 끊긴다
5. 기절 동안 BT 가 경직 분기로 대기하고(이동·공격 없음), 때려도 피격 반응 없이 데미지만 들어가며 게이지도 오르지 않는다
6. 기절 몽타주가 끝나면 보스가 정상 전투로 돌아오고 게이지가 다시 쌓인다
7. 클라이언트 화면에서도 기절 모션이 보인다
8. 기절 중 사망 → 사망 몽타주로 넘어간다
9. `GA_Stun` 을 빼고 게이지를 채우면 경고 로그가 남는다

## 관련 문서

- 같은 구조의 플레이어 게이지: [Burst.md](Burst.md)
- 피격 반응·슈퍼아머·이벤트 액션 베이스: [Abilities.md](Abilities.md)
- BT 경직 분기·위협 입력(같은 피격 이벤트): [AI.md](AI.md)
- 상태 태그 복제 경로: [GameplayTags.md](../Foundation/GameplayTags.md)
