# 어빌리티 계층 구조 (베이스/추상 클래스)

> 새 어빌리티를 만들 때 어떤 베이스를 상속하고, NetExecutionPolicy를 어떻게 정할지 결정하는 문서.

실제 기능을 구현하는 구체(엣지) 어빌리티는 아래 베이스 중 하나를 상속해서 만든다. 어떤 베이스를 골라야 하는지만 파악하면 된다.

```
UCBGameplayAbility (베이스) ← 모든 어빌리티의 루트
│   • AbilityActivationPolicy: OnTrigger / OnGiven
│
├── UCBGAChangeSpeed ← 속도 변경 베이스 (BP 확장 전제 — GA_Walk / GA_Sprint 등 BP 자식이 SpeedDataTag를 지정)
│       • 옵션: bEndWhenNoAcceleration — 가속도(이동 입력)가 유예 시간 이상 0이면 자동 종료 (GA_Sprint용)
│
├── UCBGAJump (점프 — C++ 최종 엣지. CMC Jump()/StopJumping()만 트리거, 몽타주 없음 — 공중 애니메이션은 ABP가 IsInAir()로 처리. 대시 중 차단)
│
└── UCBActionAbility (Abstract) ← 몽타주 액션 공통 베이스
    │   • PlayActionMontage() → GameplayCue.PlayAction 경유 (전 클라 동기화)
    │   • BoundActionTag / 재생 인덱스는 SelectActionMontageIndex() 훅이 단독 결정(기본 0) → 인덱스를 큐로 전달
    │   • 종료: Event.Action.EndAbility (애님노티파이) 또는 폴백 딜레이
    │   • 확장 훅: SelectActionMontageIndex / BuildActionCueParameters / OnActionMontageStarted / CleanupActionState
    │
    ├── UCBInputActionAbility (Abstract) ← 입력으로 발동되는 액션 베이스
    │   │   • BoundInputTag, 콤보 입력 윈도우(Event.Action.CheckInput)
    │   │   • NetExecutionPolicy 기본값: LocalPredicted
    │   │
    │   ├── UCBChaserAttackAbility ← 공격 베이스 (BP 확장 전제 — 기본/특수/약/강 공격 등 BP 자식으로 다양화, GAS 표준 쿨다운(CommitAbility) 적용 — CooldownGameplayEffectClass는 BP 자식이 지정)
    │   ├── UCBGAChaserEquipWeapon (무기 장착 — 개이트로 몽타주 인덱스 분기: Idle 0/Walk 1/Run·Sprint 2)
    │   ├── UCBGAChaserUnequipWeapon (무기 해제 — 개이트로 몽타주 인덱스 분기: Idle 0/Walk 1/Run·Sprint 2)
    │   └── UCBGADash (전방 대시 — C++ 최종 엣지. Sprint 진입 연출 겸용, GAS 표준 쿨다운, 활성 중 Status.Movement.Dashing 부여, 전투 상태로 몽타주 인덱스 분기: 비전투 0/전투 1)
    │
    ├── UCBEventActionAbility (Abstract) ← 게임플레이 이벤트로 발동되는 액션 베이스
    │   │   • RegisterEventTrigger(EventTag)로 트리거 등록, CancelActionTag로 진행 액션 캔슬
    │   │   • NetExecutionPolicy 기본값: ServerInitiated
    │   │
    │   ├── UCBHitReactAbility (피격 반응, Event.Combat.HitReact 트리거 — 슈퍼아머 중에는 발동 차단)
    │   └── UCBDeathAbility (사망, Event.Combat.Death 트리거 — 전체 캔슬 + 사망 상태 GE + 사망 몽타주)
    │
    └── UCBAIAttackAbility ← AI 공격 베이스 (BP 확장 전제 — 몬스터별 공격 BP로 다양화)
            • 트리거는 입력도 이벤트도 아닌 **BT 태스크의 태그 직접 활성화**
            • NetExecutionPolicy: ServerOnly (AI 컨트롤러가 서버 전용)
            • 콤보 없음. bRandomizeMontage 로 변형 몽타주 무작위 선택
```

> C++ 클래스가 곧 최종 구현인 것(**C++ 최종 엣지**, 예: `UCBGADash`)과, **BP 자식으로 다양화되는 C++ 베이스**(예: `UCBChaserAttackAbility`, `UCBGAChangeSpeed`)를 구분한다. 후자는 BP 자식(GA_Sprint, GA_Walk, 각종 공격 BP)이 실제 엣지다.

**어떤 베이스를 상속할지:**
- 단순 어빌리티 (입력·몽타주 무관): `UCBGameplayAbility`
- 입력으로 발동되는 몽타주 액션 (예: 공격, 무기 장착/해제): `UCBInputActionAbility`
- 게임플레이 이벤트로 발동되는 몽타주 액션 (예: 피격 반응, 사망): `UCBEventActionAbility`
- **AI 두뇌(BT)가 발동하는 공격**: `UCBAIAttackAbility` — 아래 "AI 공격" 참조

## 피격 반응 (`UCBHitReactAbility`)

서버가 데미지 적용 시 `Event.Combat.HitReact`를 발행하면 발동해, 피격 몽타주(`Action.Combat.HitReact`)를 GameplayCue로 전 클라 재생한다. 전투 상태면 인덱스 1(전투 피격), 비전투면 0.

### 슈퍼아머 — 끊기지 않는 어빌리티

**끊기지 않을 어빌리티가 스스로 선언한다.** 스킬 어빌리티가 `ActivationOwnedTags`로 `Status.Combat.SuperArmor`를 부여하면, `UCBHitReactAbility`의 `ActivationBlockedTags`가 그 태그를 보고 피격 반응 자체를 발동시키지 않는다. 데미지 GE는 그대로 적용되고 반응 모션만 생략된다.

주체를 뒤집은 이유: 피격 어빌리티가 "누구를 봐줄지" 목록을 들고 있으면 스킬이 늘 때마다 피격 쪽을 고쳐야 한다. 반대로 두면 스킬 BP에서 태그 하나만 켜면 끝이고, `GA_Jump`가 `Status.Movement.Dashing`으로 차단되는 기존 패턴과도 같다.

**캔슬 예외가 아니라 발동 차단인 이유** — 어빌리티 캔슬만 막아도 피격 몽타주가 같은 슬롯의 시전 몽타주를 덮어써서(`Montage_PlayWithBlendIn`) 결국 동작이 끊겨 보인다. 애니메이션까지 지키려면 피격 반응이 아예 시작되지 않아야 한다.

> **알려진 불일치**: 베이스의 `CancelActionTag = Action.Combat`은 현재 아무것도 캔슬하지 않는다. `UAbilitySystemComponent::CancelAbilities`는 어빌리티의 **AssetTags**(이 프로젝트에선 `Ability.*`)와 비교하는데 넘기는 값은 `Action.*`(몽타주 네임스페이스)이라 절대 매칭되지 않는다. 부분 캔슬을 실제로 쓰게 되면 `Ability.*` 태그로 교정해야 한다.

## 사망 (`UCBDeathAbility`)

체력이 0이 되면 **`UCBAttributeSet::PostGameplayEffectExecute`(서버)가 `Event.Combat.Death`를 발행**하고, 그 이벤트가 `UCBDeathAbility`를 발동시킨다. 피격 반응(`UCBHitReactAbility`)과 완전히 같은 경로이며, 같은 함수에서 체력 0이면 피격 대신 사망으로 갈라진다.

`ActivateAbility`의 순서가 곧 설계다.

1. **`CancelAllAbilities(this)`** — 공격·대시·질주 등 진행 중인 어빌리티를 종류를 가리지 않고 전부 끊는다. 그래서 베이스의 부분 캔슬(`CancelActionTag`)은 쓰지 않는다.
2. **사망 상태 GE 적용** (`DeadStateEffectClass`, 서버 권위) — **몽타주보다 먼저** 한다. 몽타주가 등록되어 있지 않거나 재생에 실패해도 사망 상태는 확정되어야 하기 때문.
3. **베이스가 사망 몽타주 재생** (`Action.Combat.Death` → GameplayCue로 전 클라 동기화)

### 사망 처리의 세 갈래 — 어빌리티는 정리를 하지 않는다

`UCBDeathAbility`는 **캐릭터의 정리 함수를 호출하지 않는다.** GE로 `Status.Dead`만 붙이면 캐릭터가 알아서 반응한다.

| 담당 | 무엇 | 어디 | 실행 위치 |
|---|---|---|---|
| **`Status.Dead` 태그** | 상태의 진실 — 어빌리티 차단, 캐릭터 정리 트리거 | `GE_Dead`의 GrantedTags | 전 클라 복제 |
| **`GameplayCue.Death`** | 순수 연출 — 파티클·사운드·디졸브 | `GE_Dead`의 GameplayCues (BP 노티파이) | 전 클라 |
| **`ACBBaseCharacter`** | 권위 정리 + 표현 정리 | `Status.Dead` 구독 콜백 | 서버는 권위, 전 인스턴스는 표현 |

**왜 어빌리티가 직접 부르지 않는가**

- 어빌리티는 `ServerInitiated`라 **시뮬레이티드 프록시에서 실행되지 않는다.** 다른 플레이어 화면의 시체 연출·UI 정리는 명시 호출로 도달할 수 없고, 복제되는 신호(태그)여야만 한다.
- 진입점이 태그 하나면 **어빌리티를 거치지 않는 죽음**(낙사, 즉사 GE, 디버프, 치트)이 나중에 생겨도 정리 코드를 다시 짜지 않는다.
- 캐릭터 쪽 구현은 `ACBBaseCharacter`의 `#pragma region Death`다. 준비 완료 시점에 `Status.Dead`를 구독하고(`BindDeathStateEvent`), 콜백에서 `HasAuthority()`면 `Auth_HandleDeath()`(이동 정지·`ECC_Pawn` 충돌 해제·자식 훅·디스폰 예약), 전 인스턴스는 `Local_ApplyDeathVisuals()`(`OnCharacterDiedDelegate` 방송)를 수행한다. 자식 훅 `Auth_OnDeath()`는 `ACBAICharacter`가 오버라이드해 BT를 정지시킨다 — **이동은 GAS 밖(BT/CMC)이라 사망 차단 게이트에 걸리지 않으므로 별도 정지가 필요하다.**
- 구독은 과거 변화를 소급 발화하지 않으므로, `BindDeathStateEvent()`가 구독 직후 현재 태그 카운트를 1회 반영한다. 이게 없으면 **이미 죽은 캐릭터에 뒤늦게 관련성을 얻은 클라이언트가 시체를 멀쩡한 상태로 본다.**

**공중에서 죽으면 이동 정지를 착지까지 미룬다.** `DisableMovement()`는 `MOVE_None` 으로 바꾸므로 중력이 적용되지 않는다 — 사망 시점에 바로 부르면 **시체가 공중에 뜬 채 사망 몽타주만 재생된다.**

- `Auth_HandleDeath()` 는 `IsFalling()` 이면 **수평 속도만 0으로** 두고 낙하는 그대로 둔다. 사망 후에는 입력과 BT 가 모두 멈춰 가속이 들어오지 않으므로 수직으로 떨어진다.
- 실제 정지는 `ACBBaseCharacter::Landed()` 오버라이드가 `bIsDead` 일 때 `Auth_StopMovementForDeath()` 를 불러 수행한다. 지상 사망은 기존과 같이 즉시 정지.
- **네트워크 코드가 따로 필요 없다.** 낙하와 착지 판정은 서버에서만 결정하고(`HasAuthority()` 가드), 클라이언트는 CMC 이동 복제로 따라온다.
- ⚠️ **`ECC_Pawn` 만 `ECR_Ignore` 로 꺼야 한다.** 시체가 산 캐릭터·무기 트레이스를 통과하게 하되 `WorldStatic` 은 살려둬야 바닥에 닿는다. 충돌을 통째로 끄면 영원히 떨어진다.
- 라그돌을 도입하면 이 분기는 물리가 대체한다. 그때까지의 몽타주 기반 처리다.

**`GameplayCue.Death`는 ini 태그로 만든다.** C++이 이름으로 참조하지 않고 GE·노티파이 에셋만 쓰므로 네이티브로 만들 이유가 없다 (→ [GameplayTags.md](GameplayTags.md)). GE에 붙는 큐는 `OnActive`(그 순간) + `WhileActive`(나중에 관련성을 얻은 클라)를 둘 다 태우므로, 멀리 있다가 접근한 클라이언트도 시체 상태를 따라잡는다.

> **큐에 게임플레이 로직을 넣지 말 것.** `UGameplayCueManager::ShouldSuppressGameplayCues()`가 데디케이트 서버에서 큐를 통째로 억제하고(`GameplayCueRunOnDedicatedServer` 기본 0), 원샷 큐(`ExecuteGameplayCue`)는 신뢰성 없는 멀티캐스트라 유실될 수 있다. 큐가 사라져도 게임플레이는 정확해야 한다.

### 사망은 다른 어빌리티의 차단을 뚫는다 — `bIgnoreAbilityBlocking`

공격 어빌리티 BP들이 `BlockAbilitiesWithTag`에 **`Ability.Combat`** 을 걸어 공격 중 다른 공격을 막는다. 그런데 사망 어빌리티의 AssetTag가 `Ability.Combat.Death`라 **그 차단에 함께 걸린다.** 엔진이 활성화 직전에 `AreAbilityTagsBlocked(GetAssetTags())`로 검사하고, 부모 태그 매칭이므로 `Ability.Combat`이 `Ability.Combat.Death`를 막는다.

그 결과가 **"스킬 쓰다 죽으면 안 죽는다"** 였다. 사망 어빌리티가 활성화되지 못해 `CancelAllAbilities`도 `GE_Dead` 적용도 몽타주도 실행되지 않고, **사망 이벤트는 데미지 GE 실행 시 1회성**이라 스킬이 끝난 뒤에도 다시 오지 않아 체력 0인 채로 살아남았다. (피격 어빌리티는 AssetTag가 비어 있어 이 차단에 걸리지 않았고, 그래서 "맞기는 하는데 죽지는 않는" 모습이 됐다)

**해법은 사망만 차단 검사를 건너뛰는 것이다.** `UCBGameplayAbility::bIgnoreAbilityBlocking`을 켜면 `DoesAbilitySatisfyTagRequirements`가 통째로 통과하고, 프로젝트가 추가한 부모 태그 차단 검사(`CanActivateAbility`)도 건너뛴다. 현재 켜는 건 `UCBDeathAbility` 하나다.

> **켜면 요구 태그도 함께 무시된다.** 엔진이 요구·차단을 한 함수에서 판정하므로 차단만 골라 뺄 수 없다. 이 플래그를 켠 어빌리티는 BP에서 `ActivationRequiredTags` / `ActivationBlockedTags`를 설정해도 걸리지 않는다.

> **`BlockAbilitiesWithTag = Ability.Combat`은 의도보다 넓다.** 지금은 사망만 우회했지만, 피격 어빌리티에 AssetTag를 달면 같은 문제가 재발한다. 공격 BP의 차단 태그를 `Ability.Combat.Attack`으로 좁히는 편이 안전하다 (에셋 작업).

### 사망 차단 게이트 — `bActivatableWhileDead`

`Status.Dead`를 들고 있으면 **`UCBGameplayAbility::CanActivateAbility`가 어빌리티 활성화를 일괄 거부**한다. 예외는 `bActivatableWhileDead = true`인 어빌리티뿐이고, 현재는 `UCBDeathAbility` 하나다.

어빌리티마다 `ActivationBlockedTags`에 `Status.Dead`를 다는 방법도 있지만, **새 어빌리티를 만들 때 빼먹으면 죽은 캐릭터가 공격하는 버그가 조용히 생긴다.** 모든 어빌리티의 루트에 게이트를 한 번 두면 누락이 구조적으로 불가능해진다.

### 재발행 가드

시체에 추가 타격이 들어오면 체력이 0 → 0으로 또 변해 이벤트가 다시 나간다. `Auth_SendDeathEvent()`는 **대상이 이미 `Status.Dead`면 발행하지 않는다.** GE 적용이 이벤트 발행과 같은 호출 스택에서 동기적으로 끝나므로, 첫 사망 처리가 반환된 시점엔 태그가 이미 붙어 있다.

### 필요한 에셋 (엔진 에디터)

| 에셋 | 내용 |
|---|---|
| `GE_Dead` | Duration `Infinite`, GrantedTags에 `Status.Dead` |
| `GA_Death` | `UCBDeathAbility`의 BP 자식. `DeadStateEffectClass = GE_Dead` |
| 로드아웃 | `ReactiveAbilities`에 `GA_Death` 등록 |
| 몽타주 맵 | 사망 몽타주를 `Action.Combat.Death`로 등록 (→ [Montage.md](Montage.md)) |

AssetTag(`Ability.Combat.Death`)는 예외적으로 **C++ 생성자에서** 지정한다. 사망 어빌리티는 종류가 하나뿐이고 BP 자식은 GE 지정용 데이터 컨테이너라, 공격 어빌리티처럼 자식마다 다른 태그가 필요하지 않기 때문이다.

### 미구현 (후속)

- **죽은 뒤 관련성을 얻은 클라이언트는 서 있는 시체를 본다** — 몽타주 재생 큐를 못 받기 때문. 마지막 프레임 유지 자체는 구현됨 (→ [Montage.md](Montage.md) "마지막 프레임을 유지하는 액션").
- **공중에서 죽으면 그 자리에 멈춘다** (`DisableMovement()`). 라그돌 도입 시 함께 해소된다.
- ~~`Status.Dead` 제거 책임(리스폰) 없음~~ → **플레이어는 해소됨.** `ACBGameplayGameMode`가 리스폰 직전에 `Status.Dead`를 부여한 GE를 제거하고 폰을 다시 스폰한다 (→ [GameFlow.md](GameFlow.md) "사망과 리스폰"). 시체가 `DespawnDelay = 0`으로 남는 것도 그 재스폰 시점에 정리된다. **AI는 여전히 부활 없이 파괴로 끝난다.**
- `GameplayCue.Death` 노티파이(파티클·사운드) 미작성

## AI 공격 (`UCBAIAttackAbility`)

플레이어 공격(`UCBChaserAttackAbility`)을 재사용하지 않고 별도 베이스를 두는 이유는 **트리거 방식이 다르기 때문**이다. Chaser 공격은 `UCBInputActionAbility` 상속이라 입력 태그·콤보 입력 윈도우를 전제하는데, AI에는 입력이 없다. 그래서 `UCBActionAbility`를 직접 상속한다.

| 항목 | Chaser 공격 | AI 공격 |
|---|---|---|
| 트리거 | 입력 태그 | **BT 태스크가 어빌리티 태그로 직접 활성화** |
| NetExecutionPolicy | `LocalPredicted` | **`ServerOnly`** |
| 몽타주 인덱스 | 콤보 전진(예측+거부 롤백) | 무작위 변형(`bRandomizeMontage`) |
| 전투 상태 검사 | 필요 (`IsCombatMode`) | 하지 않음 (아래 참조) |

- **`ServerOnly`인 이유**: AI 컨트롤러는 서버에만 존재하므로 예측할 클라이언트가 없다. 몽타주는 `UCBActionAbility`가 `GameplayCue.PlayAction`으로 전 클라이언트에 동기화하므로 복제 실행이 불필요하다. → [Montage.md](Montage.md)
- **로컬/서버 분기가 없다**: AI 폰은 서버에서 AI 컨트롤러에 빙의되어 있어 서버가 곧 권위이자 로컬 컨트롤러다(`IsLocallyControlled()`·`IsNetAuthority()` 둘 다 참). 그래서 트레이스·히트 이벤트 대기를 Chaser처럼 두 갈래로 나누지 않고 한 곳에서 등록한다. 무기 트레이스 파이프라인 자체는 [Combat.md](Combat.md) 그대로 재사용된다.
- **쿨다운은 GAS 표준**(`CooldownGameplayEffectClass` + `CommitAbility`). 쿨다운 중이면 활성화가 실패하고 **BT 태스크가 Failed를 받아** 다른 분기로 흐르므로, BT 쪽에 쿨다운 데코레이터를 둘 필요가 없다.
- **전투 상태를 요구하지 않는다**: `SetCombatMode()`를 호출하는 것은 Chaser의 무기 장착/해제 어빌리티뿐이라, AI는 현재 전투 상태에 진입하지 않는다. 여기서 `IsCombatMode()`를 검사하면 AI 공격이 영영 발동하지 않으므로 무기 유효성만 검사한다.
  - ⚠️ 그 결과 **AI 무기는 칼집(Sheath) 소켓에 붙은 채로 공격 모션이 재생되고, 애님도 비전투 상태머신을 쓴다.** 몬스터가 무장 상태로 보이게 하려면 AI 캐릭터가 준비 완료 시점에 `SetCombatMode(true)`를 한 번 호출해야 한다(미구현).

**같은 입력 태그에 복수 어빌리티 바인딩 가능:** `UCBAbilitySystemComponent::OnAbilityInputPressed`는 입력 태그가 일치하는 어빌리티를 **전부** 순회 활성화한다. 활성화에 실패해도 `InputPressed` 플래그는 세팅되므로, 나중에 다른 경로로 활성화된 어빌리티도 홀드/릴리즈 입력을 정상 수신한다. 어빌리티별 쿨다운은 GAS 표준(`CooldownGameplayEffectClass` + `CommitAbility`)을 쓴다.

**Sprint는 대시에 종속 (Sprint 진입 = 무조건 대시부터):** `Input.Action.Sprint`에 `GA_Dash`와 `GA_Sprint`를 함께 바인딩하되, 활성화 체인은 다음과 같다.
1. `GA_Sprint`는 `ActivationRequiredTags = Status.Movement.Dashing` — 대시 중이 아니면 활성화가 차단된다. 대시가 쿨다운이면 대시 실패 → 태그 없음 → **Sprint도 발동 불가**.
2. `UCBGADash`가 대시 성공 시 `TryActivateAbilitiesByTag(Ability.Movement.Sprint)`로 **Sprint를 직접 활성화** — 입력 순회 순서(로드아웃 부여 순서)에 의존하지 않는다. 순회 쪽에서 오는 직접 활성화는 1의 RequiredTags가 막는다(순서와 무관하게 안전).
3. Sprint 종료: 입력 릴리즈(기존) + **가속도 소실 자동 종료**(`UCBGAChangeSpeed::bEndWhenNoAcceleration`, GA_Sprint만 켬) — 이동 입력이 `NoAccelerationGraceTime`(기본 0.2초) 이상 끊기면(정지, 피벗 입력 잠금 등) 자동으로 EndAbility → 속도 GE·`Status.Movement.Gait.Sprint` 태그 제거. 다시 질주하려면 Sprint 키 재입력(=대시)이 필요하다.

**발동 전제 조건은 `CanActivateAbility` 에 둔다 (`ActivateAbility` 안에서 튕기지 않는다):** 무기 보유·자원 등 "지금 발동할 수 있는가"는 `CanActivateAbility` 오버라이드에서 검사한다. `ActivateAbility` 안에서 `EndAbility` 로 튕기면 **"활성화는 성공했는데 즉시 끝난"** 상태가 되어, 호출자가 그것을 정상적으로 즉시 완료된 어빌리티와 구분하지 못한다(BT 대기형 태스크가 대표적인 피해자 — [AI.md](AI.md)).
- `ActivateAbility` 에 남기는 검사는 **안전망**이다(Can~ 과 Activate~ 사이에 상태가 바뀔 수 있음). 이때는 반드시 `EndAbility(..., bWasCancelled = true)` 로 끝내 호출자가 실패로 식별할 수 있게 한다.
- 쿨다운·비용·차단 태그는 GAS 가 `CanActivateAbility` 에서 이미 검사하므로 따로 넣지 않는다. `CommitAbility` 실패는 레이스일 때만 발생하며 역시 캔슬로 끝낸다.

**NetExecutionPolicy (필수):** 모든 구체(엣지) 어빌리티는 생성자에서 `NetExecutionPolicy`를 명시한다. 입력 트리거→`LocalPredicted`, 이벤트 트리거→`ServerInitiated`가 기본 방향(트리거 베이스가 기본값을 깔지만 엣지에서 재명시). 단, 실행 위치는 **"트리거 이벤트를 어디서 발행하느냐"**로 결정되므로, 로컬에서 유발되는 이벤트(UI 등)면 이벤트 트리거라도 `LocalOnly`/`LocalPredicted`를 쓴다. BP 엣지 클래스도 에디터에서 정책을 명시한다.

**AssetTags (필수):** 모든 어빌리티는 자신을 식별하는 어빌리티 태그(`Ability.*`)를 AssetTags로 가진다. 지정 위치는 엣지가 어디냐에 따른다:
- **C++ 최종 엣지** (예: `UCBGADash`): 생성자에서 `SetAssetTags(FGameplayTagContainer(태그))`로 지정 (UE 5.5+ API — `AbilityTags` 직접 접근 금지. 예: `Ability.Movement.Dash`).
- **BP 확장 전제 C++ 베이스** (예: `UCBChaserAttackAbility`, `UCBGAChangeSpeed`): 생성자에서 지정하면 모든 BP 자식이 같은 태그를 공유해버리므로 **지정하지 않는다.** 실제 엣지인 **BP 자식**(GA_Sprint, 각 공격 BP 등)이 클래스 디폴트의 AssetTags에서 각자 지정한다.

이 태그로 취소(`CancelAbilities`)·차단(`BlockAbilitiesWithTag`)·조회(HUD 쿨다운 등)가 가능해야 한다.

> **문서 유지 규칙:** 이 계층도에는 **베이스/추상 클래스만** 나열하고, 구체(엣지) 어빌리티는 대표 예시 외에는 나열하지 않는다. 베이스/추상 어빌리티 클래스를 추가·변경·삭제하면 **같은 작업에서 이 문서의 계층도도 갱신**한다.

## 관련 문서
- 로코모션 어빌리티(Walk/Sprint/Dash/Jump)의 시스템 전체 맥락: [Locomotion.md](Locomotion.md)
- NetExecutionPolicy 상세·Simulated Proxy 처리: [Multiplayer.md](Multiplayer.md)
- 몽타주 재생 흐름(인덱스 결정 → GameplayCue): [Montage.md](Montage.md)
- 콤보 소유·전진/리셋: [Combat.md](Combat.md)
