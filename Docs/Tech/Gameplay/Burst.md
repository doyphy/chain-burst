# 버스트 (피버 타임)

> 적을 때릴 때마다 게이지(`BurstGauge`)가 쌓이고, 가득 차면 입력으로 버스트를 발동해 일정 시간 공격 속도·공격력이 오른다. **게이지 적립과 발동을 두 어빌리티로 나누고, 버프 자체는 GE 에셋만으로** 만든다.

## 흐름

```
적중(서버 검증 통과) ─→ UCBBurstGaugeAbility (패시브, 서버)
                          BurstGauge = min(현재 + GaugeGainPerHit × 맞은 적 수, 100)
                          버스트 중(Status.Combat.Burst)이면 쌓지 않음

입력(Input.Action.Combat.Burst) ─→ UCBBurstAbility (예측 실행)
                          CanActivate: BurstGauge ≥ 100
                          GE_Burst 적용(예측) → [서버] 게이지 0 → 발동 몽타주(Action.Combat.Burst)

버스트 중 적중 ─→ UCBLifeOnHitAbility (범용 적중 회복 패시브, 서버)
                          GE_Burst 가 더해 둔 LifeOnHit × 맞은 적 수만큼 회복

폰 수명 종료(사망 / 캐릭터 변경) ─→ UCBBurstGaugeAbility::EndAbility
                          게이지 0 + Status.Combat.Burst 를 부여한 GE 제거
```

## 구성 요소

| 요소 | 위치 | 역할 |
|---|---|---|
| `BurstGauge` | `UCBAttributeSet` | 0 ~ `MaxBurstGauge`(상수 100). 서버만 기록하고 전 클라에 복제 |
| `UCBBurstGaugeAbility` | 로드아웃 `PassiveAbilities` | `OnGiven` + `ServerOnly`. 적중마다 적립, 종료 시 초기화 |
| `UCBBurstAbility` | 로드아웃 입력 어빌리티 목록 | `UCBInputActionAbility` 상속, `LocalPredicted`. 게이지 가득 참 검사 → 버프 적용 → 소모 → 몽타주 |
| `GE_Burst` | 에셋 | 지속형. `AttackSpeed`·`AttackPower` 곱셈 모디파이어 + `LifeOnHit` Add 모디파이어 + GrantedTags `Status.Combat.Burst` |

### 버프에 코드가 필요 없는 이유

두 수치를 소비하는 경로가 이미 있다. GE 모디파이어만 얹으면 그대로 반영된다.

- `AttackSpeed` → 몽타주 재생 속도 (`UCBActionComponent::PlayMontage`, 몽타주 데이터 엔트리의 `bAffectedByAttackSpeed`가 켜진 것만)
- `AttackPower` → 데미지 (`UCBDamageExecCalc`, 공격자 값을 스펙 생성 시점에 스냅샷)

재생 속도는 **몽타주가 시작될 때** 읽는다. 공격 중에는 버스트를 발동할 수 없으므로 "휘두르던 한 타만 옛 속도" 같은 경계 문제는 생기지 않는다. 시뮬 프록시도 복제된 `AttackSpeed`로 각자 재생 속도를 정하는데, 어트리뷰트가 발동 몽타주가 끝나기 훨씬 전에 도착하므로 다음 공격부터 어긋나지 않는다.

### 버스트 중 적중 회복 — 이것도 모디파이어 하나

버스트 어빌리티는 회복을 **켜지 않는다.** 적중 회복은 범용 패시브 `UCBLifeOnHitAbility`가 항상 켜진 채 `LifeOnHit` 어트리뷰트를 읽어 처리하고(→ [Abilities.md](Abilities.md) "적중 회복"), `GE_Burst`는 `LifeOnHit` Add 모디파이어로 **버스트 동안만** 그 값을 올린다. 공격 속도·공격력과 똑같은 방식이다.

| 상황 | 회복 |
|---|---|
| 버스트 중 | `GE_Burst`의 모디파이어만큼 회복 |
| 만료 / 버스트 중 사망 | GE가 빠지며 모디파이어도 빠져 회복 멈춤 (사망은 게이지 패시브 종료가 GE 제거) |
| 서버 거부 | 예측 GE의 모디파이어는 소유 클라에만 잠깐 있었고, 회복 판정은 서버 값으로 하므로 회복 없음 |
| 다른 출처와 겹침 | 모디파이어가 합산되어 함께 회복 |

"버스트 동안 회복 어빌리티를 GE로 부여"하는 방식은 채택하지 않았다. 출처별 회복량·중첩 처리가 어렵고, `GA_Burst`의 `BlockAbilitiesWithTag = Ability.Combat`이 방금 부여된 어빌리티의 활성화를 막는 함정이 있다(상세는 Abilities.md).

## 연출 — 버스트는 무기 오라를 켤 뿐

버스트 동안 무기에 붙는 오라는 **범용 무기 오라**(→ [Combat.md](Combat.md) "무기 오라")다. 버스트가 하는 일은 **`GE_Burst`의 Gameplay Cues에 `GameplayCue.Weapon.Aura`를 넣는 것뿐**이고 코드는 없다. 큐는 GE가 붙어 있는 동안 활성이고, GE가 빠지면 모든 머신에서 On Cease Relevant(`OnRemove`)가 불려 오라 요청이 거둬진다.

| 상황 | 오라 |
|---|---|
| 발동 | 예측 적용된 GE가 소유 클라에서 즉시 큐를 켬 → 서버 확정 후 다른 클라에도 |
| 만료 / 버스트 중 사망 | GE 제거와 함께 전 머신에서 On Cease Relevant |
| 서버 거부 | 예측 GE 롤백으로 소유 클라에서 On Cease Relevant. 다른 클라에는 애초에 켜지지 않음 |
| 버스트 중 관련성을 얻은 클라 | On Become Relevant(`WhileActive`)로 켜진 상태를 따라잡음 |
| 다른 출처(버프 등)와 겹침 | 무기가 요청 수를 세므로 버스트가 끝나도 다른 출처가 켜 두면 유지 |

**HUD 게이지 위젯의 `OnBurstEnded`로 끄면 안 된다.** HUD는 소유 클라에만 있는 로컬 UI라, 다른 플레이어 화면에서는 끌 신호가 없어 오라가 영원히 남는다. 게임플레이가 원인인 연출은 복제되는 신호(GE·태그·큐)를 따른다 (→ [UI.md](../Presentation/UI.md) "외부에서 UI 접근", `GameplayCue.Death`와 같은 패턴 → [Abilities.md](Abilities.md)).

큐 BP·무기 설정·함정은 전부 [Combat.md](Combat.md) "무기 오라"에 있다. 버스트 쪽에서 확인할 것은 **무기 BP마다 `AuraComponent`에 Niagara 에셋이 지정되어 있는지**뿐이다(비우면 그 무기는 버스트 중에도 오라가 없다).

**특정 프레임의 연출과 나눈다.** `GE_Burst`는 발동 입력 순간(몽타주 시작 전)에 적용되므로 큐도 그때 켜진다. 기합 순간의 번쩍임 같은 1회성 연출은 버스트 몽타주의 Niagara 애님 노티파이로 둔다 — 몽타주가 전 클라에서 재생되므로 노티파이도 모두에게 보인다. 지속 오라만 큐가 맡는다.

## 왜 어빌리티 둘인가

게이지 적립은 **항상 켜져 있어야** 적중 이벤트를 받을 수 있고, 발동은 **키를 누를 때만** 돈다. 한 어빌리티로 합치면 "상시 활성인 채로 입력을 받는" 구조가 되어 입력 이벤트를 따로 복제해야 한다.

나눠서 공짜로 얻는 것이 하나 더 있다 — **패시브의 수명이 곧 폰의 수명**이라 초기화 지점이 자연히 생긴다.

## 초기화 — 패시브 종료 한 곳

플레이어 ASC는 `ACBPlayerState` 소유라 폰보다 오래 산다(→ [ASC-Ownership.md](../Foundation/ASC-Ownership.md)). 비우지 않으면 새 폰이 게이지와 버스트를 그대로 이어받는다. 폰이 끝나는 두 경로가 모두 패시브의 `EndAbility`를 거치므로 여기 한 곳에서 비운다.

| 경로 | 패시브가 끝나는 이유 |
|---|---|
| 사망 | `UCBDeathAbility`가 `CancelAllAbilities(this)` → 캔슬 |
| 캐릭터(무기) 변경 | 게임모드의 `Auth_ClearLoadoutGrants()` → `ClearAbility` → 엔진이 활성 인스턴스를 `EndAbility` (`AbilitySystemComponent_Abilities.cpp` `OnRemoveAbility`) |

`EndAbility`에서 하는 일은 둘이다.

- `BurstGauge`를 0으로
- `RemoveActiveEffectsWithGrantedTags(Status.Combat.Burst)` — **버스트 도중 죽으면 버프도 끝난다.** 리스폰 대기(5초)가 버스트 지속시간보다 짧으면 새 폰이 남은 버프를 물려받기 때문이다.

리스폰하면 로드아웃이 패시브를 다시 부여하고, `OnGiven`이라 즉시 다시 켜진다.

> 패시브의 `EndAbility`는 `ServerOnly`라 서버에서만 불린다. 그래서 권한 검사 없이 어트리뷰트 기본값을 쓴다.

## 발동 — 상태를 먼저, 몽타주는 나중

`UCBBurstAbility::ActivateAbility`의 순서가 설계다(사망 어빌리티와 같은 형태).

1. **`GE_Burst` 적용(예측)** + **[서버] 게이지 0**
2. 발동 몽타주 재생 (`GameplayCue.PlayAction`, 전 클라 동기화)

> **`CommitAbility`를 호출하지 않는다.** 발동 자원은 게이지이고 게이지는 서버가 직접 소모하므로 GAS 비용·쿨다운 경로를 쓰지 않는다. 그래서 **`GA_Burst`에 `CooldownGameplayEffectClass`·`CostGameplayEffectClass`를 지정해도 적용되지 않는다.** 쿨다운이 필요해지면 커밋 호출부터 되살린다.

상태를 먼저 확정하므로 **발동 모션이 피격에 끊기거나 몽타주 재생에 실패해도 버스트는 유지된다.** 모션이 끊기지 않게 하려면 `GA_Burst`의 `ActivationOwnedTags`에 `Status.Combat.SuperArmor`를 넣는다(→ [Abilities.md](Abilities.md) "슈퍼아머").

- **게이지 소모는 서버만 한다.** 게이지의 기록 주체를 서버로 통일하기 위함이다. 소유 클라에는 왕복 지연만큼 늦게 0이 도착하지만, HUD 게이지 위젯은 예측된 `Status.Combat.Burst`가 붙는 순간 남은 시간 표시로 넘어가고 버스트 중의 게이지 변경을 보내지 않으므로 화면에는 드러나지 않는다 (→ [UI.md](../Presentation/UI.md) "버스트 게이지 표시").
- **버스트 GE는 예측 적용한다.** 활성화 예측 키로 적용되어 소유 클라의 다음 공격부터 바로 빨라진다.
- **발동 조건(`BurstGauge ≥ 100`)을 예측 판정에 써도 안전하다.** 복제되는 어트리뷰트이고, 서버에서만 늘어나므로 클라가 서버보다 높은 값을 볼 일이 없다(소모 직후는 버스트가 이미 켜져 있다).

### 공격 중에는 발동되지 않는다 — 코드 없음

`UCBBurstAbility`의 식별 태그가 `Ability.Combat.Burst`라서, 공격 BP들의 `BlockAbilitiesWithTag = Ability.Combat`에 그대로 걸린다(부모 태그 매칭). 반대로 **버스트 모션 중 공격을 막으려면** `GA_Burst`에도 같은 `BlockAbilitiesWithTag = Ability.Combat`을 건다(에셋 설정).

> 패시브(`Ability.Combat.BurstGauge`)도 같은 차단 범위에 들지만, 활성화는 부여 시점 한 번뿐이다. 부여는 로드아웃 회수 직후(공격 어빌리티가 이미 제거된 상태)에만 일어나므로 걸리지 않는다.

### 재발동 차단 태그를 두지 않은 이유

"버스트 중 재발동"은 도달할 수 없다. 버스트 중에는 패시브가 게이지를 쌓지 않아 게이지가 0에 머물고, 발동 모션 중의 재입력은 "이미 활성 중"으로 엔진이 막는다. 그래서 `ActivationBlockedTags = Status.Combat.Burst`는 넣지 않았다.

## 적립 — 맞은 적 1명당

- 적중 이벤트(`Event.Combat.Attack.Hit`)는 `UCBCombatComponent::Server_NotifyAttackHit`가 **진영·거리 검증을 통과한 대상만** 담아 서버 ASC로 보낸다. 무기 트레이스 조각(`UCBFragment_WeaponTrace`)과 같은 이벤트를 따로 기다리므로 **공격 어빌리티는 버스트를 모른다.**
- 한 이벤트에 여러 피격자가 배칭되어 오지만 같은 대상은 한 스윙에 한 번만 실리므로(`AlreadyHitActors`) `TargetData.Num()`이 곧 맞은 적 수다.
- **헛스윙으로는 쌓이지 않는다.** 적중 기준이라 허공을 휘둘러 채우는 것이 불가능하다.
- **AI에는 영향이 없다.** 패시브를 Chaser 로드아웃에만 넣는다. AI의 어트리뷰트셋에도 `BurstGauge`가 있지만 한 번도 바뀌지 않는다.

## 네트워크

| 값 | 기록 | 복제 |
|---|---|---|
| `BurstGauge` | 서버 (`SetNumericAttributeBase`) | 어트리뷰트 복제 — 전 클라 |
| 버프 모디파이어·`Status.Combat.Burst` | `GE_Burst` (소유 클라 예측 + 서버 확정) | GE 복제 경로 ① (→ [GameplayTags.md](../Foundation/GameplayTags.md)) |
| 발동 모션 | `GameplayCue.PlayAction` | 전 클라 (→ [Montage.md](Montage.md)) |

HUD 게이지(`UCBBurstGaugeWidget`)는 `BurstGauge` 어트리뷰트 변경과 `Status.Combat.Burst` 태그 변화를 구독하고, 버스트 중에는 `GE_Burst`의 남은 시간을 읽는다 — 새 네트워크 코드가 필요 없다(→ [UI.md](../Presentation/UI.md) "버스트 게이지 표시").

### 서버가 발동을 거부하면

예측 실행이라 소유 클라가 먼저 발동하고, 서버의 `CanActivateAbility`가 실패하면 `ClientActivateAbilityFailed`로 되돌린다. 현실적인 원인은 **공격 직후의 입력**이다 — 공격 차단(`BlockAbilitiesWithTag`)은 복제되지 않고 머신마다 따로 걸리는데, 공격 어빌리티는 서버에서 RPC 지연만큼 늦게 끝난다. 게이지 부족으로 거부될 일은 사실상 없다(클라가 보는 게이지는 서버의 과거 값이고, 서버 게이지는 소모·초기화 외에 줄지 않는다).

| 대상 | 거부 시 |
|---|---|
| `GE_Burst` (배율·`Status.Combat.Burst`) | **자동 롤백.** 엔진이 예측 적용 시 예측 키에 제거 콜백을 걸어 두어(`GameplayEffect.cpp` `NewRejectOrCaughtUpDelegate` → `RemoveActiveGameplayEffect_AllowClientRemoval`) GE와 함께 태그·모디파이어가 빠진다. 같은 콜백이 승인 시(caught up)에도 예측 사본을 지워 서버 사본과 겹치지 않는다 |
| `BurstGauge` | 되돌릴 것 없음 — 서버만 소모하므로 거부되면 애초에 줄지 않았다 |
| 데미지 | 영향 없음 — 서버 ExecCalc가 서버의 `AttackPower`로 계산한다 |
| 어빌리티 인스턴스 | `K2_EndAbility()` → 캔슬이 아닌 정상 종료 |
| **발동 몽타주** | **남는다.** `ExecuteGameplayCue`는 롤백되지 않고, 정상 종료라 정지 큐도 나가지 않아 소유 클라만 모션을 끝까지 재생한다. 모든 액션 어빌리티가 공유하는 한계다 (→ [Montage.md](Montage.md)) |

HUD 게이지는 예측 태그가 빠지면서 버스트 종료 → 가득 찬 게이지로 되돌아간다. 게이지는 가득 찬 채 남아 있으므로 다시 누르면 된다.

## 에디터 작업

| 에셋 | 설정 |
|---|---|
| `GE_Burst` | Duration `HasDuration`(지속시간), Modifiers `AttackSpeed` Multiply / `AttackPower` Multiply / `LifeOnHit` Add(적 1명당 회복량), 부여 태그 `Status.Combat.Burst`. 회복이 동작하려면 Chaser 로드아웃에 `GA_LifeOnHit`이 있어야 한다 (→ [Abilities.md](Abilities.md) "적중 회복") |
| `GA_Burst` | `UCBBurstAbility` BP 자식. `BurstEffectClass = GE_Burst`, `BlockAbilitiesWithTag = Ability.Combat`(권장). 선택: `ActivationOwnedTags += Status.Combat.SuperArmor`, `ActivationRequiredTags += Status.Combat.InCombat` |
| `GA_BurstGauge` | `UCBBurstGaugeAbility` BP 자식. `GaugeGainPerHit` |
| 몽타주 데이터 | Chaser마다 `Action.Combat.Burst`에 발동 몽타주 등록 |
| 입력 | `IA_Burst` + `UCBInputConfig::AbilityInputActions`에 `Input.Action.Combat.Burst` ↔ `IA_Burst` + IMC 키 매핑 (→ [Input.md](Input.md)) |
| Chaser 로드아웃 | `PassiveAbilities += GA_BurstGauge`, 입력 어빌리티 목록 `+= GA_Burst`(입력 태그 `Input.Action.Combat.Burst`) |
| HUD 게이지 | `WBP_CB_BurstGauge`(`UCBBurstGaugeWidget` 자식)를 `WBP_CB_HUD`에 배치 → [UI.md](../Presentation/UI.md) "버스트 게이지 표시" |
| 무기 오라 | `GE_Burst`의 Gameplay Cues에 `GameplayCue.Weapon.Aura` 추가 + 무기 BP마다 `AuraComponent`에 Niagara 에셋 지정·배치. 태그·`GCN_FX_WeaponAura`는 범용 무기 오라 설정 → [Combat.md](Combat.md) "무기 오라" |

- `BurstEffectClass`를 비워 두면 발동 시 경고 로그를 남기고 **게이지를 소모하지 않는다**(버프 없이 게이지만 날아가는 것을 막음).
- 새 `UPROPERTY`가 있으므로 코드 반영 후 **에디터를 재시작**해야 BP에서 값이 보인다.

## 검토했지만 채택하지 않은 것

| 안 | 이유 |
|---|---|
| `MaxBurstGauge` 어트리뷰트 | 차는 속도는 적립량 하나로 조절된다. 최대값을 바꿔야 할 소비자가 없다 |
| 게이지 증감용 GE + SetByCaller 태그 | 기록 주체가 서버의 두 어빌리티뿐이라 GE 에셋·데이터 태그를 늘릴 이유가 없다(`SetMovementSpeed`와 같은 직접 기록) |
| 어트리뷰트셋의 클램프 | 기록하는 쪽이 적립 시 100에서 멈추고, 소모·초기화는 0으로만 쓴다. 범위를 벗어나는 경로가 없다 |
| 가득 차면 자동 발동 | 기획 결정 — 입력 발동 |
| 무기 트레이스 조각에서 적립 | 무기 공격 기능이 버스트를 알게 되고, 공격 BP마다 값을 넣고 AI BP는 0으로 둬야 한다 |
| GAS 비용 GE(Cost)로 소모 | 비용 검사는 "음수가 되는가"만 보므로 에셋에 `-100`을 적어야 하고, 최대값이 코드와 에셋 두 곳에 생긴다 |
| 게임모드 두 곳에서 초기화 | 리스폰(`ACBGameplayGameMode`)·캐릭터 변경(`ACBLobbyGameMode`)을 따로 챙겨야 한다. 패시브 종료 한 곳이 둘 다 덮는다 |
| 애님 노티파이 시점에 버프 적용 | 이벤트 태그·노티파이 배치가 늘고, 모션이 끊기면 게이지만 날아간다 |

## 알려진 한계

- **패시브 부여 시 클라가 서버에 활성화를 한 번 요청하고 거절당한다.** 클라에서도 스펙 복제로 `OnGiveAbility`가 불려(`FGameplayAbilitySpec::PostReplicatedAdd`) 베이스의 `OnGiven` 처리가 `TryActivateAbility`를 부르고, `ServerOnly`라 서버에 원격 요청이 간다. 서버는 이미 활성이라 거절한다. 동작 영향은 없다.
- **서버가 발동을 거부하면 소유 클라에만 발동 몽타주가 끝까지 재생된다** (위 "서버가 발동을 거부하면"). 버프·태그는 자동으로 되돌아간다.

## 검증

1. UBT 컴파일 통과 ✅
2. 적 한 명을 때릴 때마다 `BurstGauge`가 `GaugeGainPerHit`만큼 오르고, 여럿을 한 번에 베면 그 수만큼 오른다 (`showdebug abilitysystem`)
3. 100에서 멈추고, 100 미만에서는 버스트 키가 반응하지 않는다
4. 공격 모션 중에는 버스트 키가 무시되고, 공격 사이에는 발동된다
5. 발동하면 모션이 재생되고 게이지가 0, `Status.Combat.Burst`가 붙고 다음 공격부터 빨라지고 세진다
6. 버스트 중에는 게이지가 오르지 않고, 지속시간이 끝나면 태그·버프가 사라진다
7. 버스트 중 사망 → 버프·게이지 모두 사라지고, 리스폰 후 다시 쌓인다
8. 리슨 서버 호스트와 클라 양쪽에서 동일하게 동작하고, 다른 플레이어 화면에서도 빨라진 공격 모션이 보인다
9. AI는 게이지가 쌓이지 않는다
10. HUD 게이지가 적립에 맞춰 차고, 발동하면 즉시 남은 시간 표시로 넘어가 줄어들다가, 종료되면 빈 게이지로 돌아온다 (발동 직후 게이지가 가득 찬 채 잠깐 머무르지 않는다)
11. 버스트 중 사망 → HUD 게이지가 버스트 종료 후 빈 상태로 돌아온다
12. 버스트 중에만 적중할 때마다 체력이 `LifeOnHit` × 맞은 적 수만큼 오르고, MaxHealth를 넘지 않는다. 버스트가 끝나면 회복도 멈춘다
13. 무기 오라가 발동과 함께 **무기에** 켜지고 만료·사망 시 꺼진다(한 번만 생기고 남지 않음) — **다른 플레이어 화면에서도** 같게 보이고, 버스트 중에 다가온 플레이어에게도 보인다. 쌍수는 양손 무기 모두

## 관련 문서

- 어빌리티 베이스·입력 액션·슈퍼아머: [Abilities.md](Abilities.md)
- 적중 이벤트를 만드는 히트 검증: [Combat.md](Combat.md)
- 상태 태그 복제 경로: [GameplayTags.md](../Foundation/GameplayTags.md)
- 로드아웃 어빌리티 부여·회수: [Loadout.md](../Foundation/Loadout.md), [GameFlow.md](../Flow/GameFlow.md)
- 입력 바인딩: [Input.md](Input.md)
