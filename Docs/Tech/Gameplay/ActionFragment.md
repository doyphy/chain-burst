# 어빌리티 기능 조각 — 베이스는 공용 로직만, 기능은 조각으로

> **상태: 적용됨 (3판).** 조각 셋 — `UCBFragment_WeaponTrace`(무기 트레이스), `UCBFragment_AreaAttack`(영역 공격), `UCBFragment_Projectile`(투사체). 사용법은 [Abilities.md](Abilities.md) "기능 조각"에 있고, 이 문서는 **왜 이 구조인가**와 **거쳐 온 안**을 기록한다.
>
> 참조 문서: [Abilities.md](Abilities.md) · [Combat.md](Combat.md) · [Multiplayer.md](../Conventions/Multiplayer.md) · [DesignReview.md](../Conventions/DesignReview.md)

---

## 1. 결정

- **`UCBGameplayAbility`에는 전 어빌리티 공용 로직만 둔다.** 활성화 정책, 사망 게이트, 차단 무시 옵션처럼 모든 어빌리티에 해당하는 것만.
- **여러 어빌리티가 쓰는 기능은 새 베이스 클래스로 만들지 않고 기능 조각으로 뺀다.** 조각은 그 기능이 필요한 어빌리티가 **멤버로 소유하고 수명 시점마다 직접 호출**한다. 베이스가 조각을 순회하거나 훅을 대신 불러주지 않는다.
- **조각의 크기는 "늘 함께 쓰이는 기능 묶음"이다.** 무기 공격은 무기 검사·트레이스·데미지 GE가 항상 같이 필요하므로 조각 하나(`UCBFragment_WeaponTrace`)다.

---

## 2. 왜 — 상속 계층이 막고 있던 것

```
UCBGameplayAbility
└── UCBActionAbility (Abstract)                 몽타주 파이프라인
    ├── UCBInputActionAbility (Abstract)        발동: 입력
    │   └── UCBChaserAttackAbility              트레이스 · 데미지  ← 복붙
    ├── UCBEventActionAbility (Abstract)        발동: 이벤트
    └── UCBAIAttackAbility                      트레이스 · 데미지  ← 복붙
```

- **트레이스·데미지 로직이 두 벌 있었다.** `OnTraceStart` / `OnTraceEnd` / `OnAttackHit`이 주석까지 같은 채로 Chaser 공격과 AI 공격에 각각 있었다. 두 클래스가 형제가 아니라 사촌이라 공유할 조상이 무기 개념을 둘 수 없는 `UCBActionAbility`뿐이었다. 데미지 규칙(N² 방지 등)을 고치면 두 곳을 봐야 했다.
- **공통 조상에 올리는 해법은 계층을 더 깊게 만든다.** "무기 공격 베이스"를 끼우려면 발동 방식 축(입력/BT)과 충돌해 다이아몬드가 되거나, 한쪽 발동 베이스를 포기해야 한다.
- **조각으로 빼면 발동 방식과 무관하게 붙일 수 있다.** 입력으로 발동하는 Chaser 공격이든 BT로 발동하는 AI 공격이든 같은 조각을 멤버로 가지면 된다.

---

## 3. 구조

### 3.1 원칙

1. **베이스(`UCBGameplayAbility`)와 조각 모두 특정 기능 이름을 모른다.** 베이스에 `Hit`·`Trace`·`Combo` 같은 이름이 보이면 잘못 설계된 것이다. 베이스는 공용 로직만, 기능의 "무엇"은 조각이 안다.
2. **조각은 호스트가 명시적으로 호출한다.** 호스트 코드를 읽으면 어떤 기능을 언제 쓰는지 그대로 보인다(`CanActivateAbility` → `CanActivate`, 몽타주 재생 직후 → `Start`, `EndAbility` → `Stop`). 호출 순서도 호스트가 정한다.
3. **조각끼리 데이터를 주고받아야 하면 GAS 게임플레이 이벤트·태그를 쓴다.** 호스트를 중계자로 만들지 않는다. (예: 히트는 `UCBCombatComponent`가 ASC로 `Event.Combat.Attack.Hit`를 보내고, 무기 트레이스 조각이 그것을 직접 기다린다)
4. **조각은 기능이 필요한 C++ 어빌리티가 `CreateDefaultSubobject`로 만든다.** 프로퍼티는 `EditDefaultsOnly, Instanced, NoClear` — BP에서 조각 안의 값은 편집하되 조각 자체를 비울 수 없으므로 호스트는 null 검사를 하지 않는다.
5. **조각의 Outer가 곧 호스트 어빌리티다.** 기본 서브오브젝트라 어빌리티 인스턴스가 만들어질 때 함께 만들어지고(`InstancedPerActor` → ASC당 하나), BP 값은 아키타입에서 복사된다. 런타임 상태를 멤버로 가져도 개체끼리 섞이지 않는다.
6. **공통 조각 베이스 클래스는 두지 않는다.** 원래 "두 번째 조각이 생길 때 만든다"였으나, 영역 공격 조각이 생겼을 때 보니 두 조각의 공통점이 Outer 캐스팅(`GetOwningAbility()`) 한 줄뿐이었다. 원칙 2에 따라 베이스가 조각을 순회하지 않으므로 공통 인터페이스(`Start`/`Stop` 등)도 필요 없다 — 실제로 영역 공격 조각에는 `CanActivate`·`Stop`이 없다. 공통 동작이 둘 이상 생기면 그때 만든다 — [DesignReview.md](../Conventions/DesignReview.md) §1.
7. **조각이 나누는 것은 판정이고, 적용은 공유한다.** "맞은 대상 하나에 데미지 GE 적용"은 판정 방식과 무관하므로 `UCBAbilitySystemLibrary::Auth_ApplyDamageToTarget()` 하나를 두 조각이 부른다. 조각마다 복사하면 N² 방지 같은 적용 규칙을 고칠 때 두 곳을 봐야 한다. 판정과 적용이 떨어진 투사체는 이 함수의 두 단계(`MakeDamageSpec()` → `Auth_ApplyDamageSpecToTarget()`)를 따로 부르므로 적용 규칙은 여전히 한 곳이다.

### 3.2 무기 트레이스 조각 (`UCBFragment_WeaponTrace`)

| 함수 | 하는 일 | 실행 머신 |
|---|---|---|
| `CanActivate(ActorInfo)` | 무기 검사 | 서버·예측 클라 같은 결과 |
| `Start()` | 트레이스 구간 이벤트 대기(로컬) + 히트 이벤트 대기 → 타겟마다 데미지 GE·피격 큐(서버) | 로컬 / 서버 분기 |
| `Stop()` | 트레이스 강제 정지 | 각 머신 |

사용 어빌리티: `UCBChaserAttackAbility`, `UCBAIAttackAbility`. 호출 위치·BP 설정은 [Abilities.md](Abilities.md) "기능 조각".

### 3.3 영역 공격 조각 (`UCBFragment_AreaAttack`)

| 함수 | 하는 일 | 실행 머신 |
|---|---|---|
| `Start()` | 영역 공격 이벤트 대기 → 박스 오버랩 → 벽 차단 검사 → 타겟마다 데미지 GE·피격 큐 | 서버 |

사용 어빌리티: `UCBAIAttackAbility`(무기 트레이스와 함께 소유). 판정 순서·주의점은 [Abilities.md](Abilities.md) "기능 조각 — 영역 공격".

- **발동 방식이 아니라 몽타주가 기능을 고른다.** 두 조각 모두 노티파이 이벤트가 와야만 동작하므로, 한 어빌리티가 둘을 함께 소유해도 몽타주에 없는 쪽은 아무 일도 하지 않는다. 영역 공격 전용 어빌리티 클래스를 따로 만들지 않은 이유다(만들면 AI 공격의 서버 전용·쿨다운·무작위 몽타주·타겟 워프를 복사해야 했다).

### 3.4 투사체 조각 (`UCBFragment_Projectile`) + 투사체 액터 (`ACBProjectile`)

| 함수 | 하는 일 | 실행 머신 |
|---|---|---|
| `Start(TargetActor)` | 조준 대상 기록 + 발사 이벤트 대기 → 타겟 현재 위치로 조준 → 데미지 스펙 1회 생성 → 부채꼴로 N발 스폰·발사 | 서버 |

사용 어빌리티: `UCBAIAttackAbility`(무기 트레이스·영역 공격과 함께 소유). 사용법은 [Abilities.md](Abilities.md) "기능 조각 — 투사체", 명중 판정은 [Combat.md](Combat.md) "투사체 판정".

- **처음으로 조각 밖에 짝이 되는 액터가 생겼다.** 조각은 호스트 어빌리티와 수명을 같이하는데(원칙 5), 투사체는 어빌리티가 끝난 뒤에 맞는다. 그래서 **"언제·어디서·몇 발·데미지"는 조각이, "어떻게 날아가고 무엇에 맞나"는 액터가** 맡는다. 조각은 발사 후의 일을 모른다.
- **데이터도 같은 선으로 나눴다.** 궤적(직선·포물선·유도·속도·중력)은 투사체의 성질이라 투사체 BP 에, 발사 수·각도·소켓·데미지는 공격마다 달라 조각(공격 BP)에 둔다. 속도·중력·유도 세기·수명·판정 크기는 **엔진 기존 값**(ProjectileMovement·루트 모양·`InitialLifeSpan`)을 그대로 쓰고 새로 만든 값은 `LaunchMode`·`ArcParam` 둘뿐이다.
- **타겟은 호스트가 주입한다**(`Start(TargetActor)`) — CodingConventions §5 의 fetch 대신 inject. 호스트는 모션 워핑이 이미 읽던 블랙보드 조회를 헬퍼(`GetBlackboardTarget()`)로 모아 두 곳에서 쓴다.
- **포물선 다발용 산포 반경을 두지 않았다.** 부채꼴 회전 대상을 방향이 아니라 조준점으로 잡아, `SpreadAngle` 하나가 직선(방향)과 포물선(착탄점) 모두에 맞는다.

---

## 4. 거쳐 온 안 — 버린 이유

같은 목표(상속 대신 조립)로 두 번 설계를 고쳤다. 각 안이 **왜 틀렸는지**가 3.1 원칙의 근거다.

### 4.1 2판 — 베이스가 조각 배열을 들고 훅을 순회 (버림)

`UCBGameplayAbility`에 `TArray<UCBAbilityFragment*> Fragments`를 두고, 베이스가 수명 시점마다 전 조각의 훅(`CanActivate` / `OnMontageStarted` / `Auth_OnHit` / `OnAbilityEnded`)을 불렀다. 조각은 `Require Weapon` / `Weapon Trace` / `Damage` 셋으로 쪼개 BP에서 조립했다. 조각끼리 직접 참조하지 않도록 트레이스 → 데미지 연결은 베이스(`Auth_BroadcastHit`)가 중계했다.

| 문제 | 설명 |
|---|---|
| **베이스가 특정 기능을 알게 됐다** | 조각 간 중계를 베이스가 맡으면서 `Auth_BroadcastHit`·`Auth_ApplyEffectSpecToHitTarget`(베이스)와 `Auth_OnHit`(조각 베이스 훅)이 생겼다. 전부 "히트"라는 특정 기능이다. 원거리·흡혈 같은 기능이 생길 때마다 베이스가 다시 두꺼워지는, 상속 구조와 같은 문제가 재발한다 |
| **중계가 애초에 필요 없었다** | 히트 이벤트는 트레이스 조각이 만드는 게 아니라 `UCBCombatComponent`가 ASC로 보낸다(`HandleGameplayEvent`). GAS 이벤트가 이미 기능 사이의 통로였다 → 원칙 3 |
| **조각이 너무 잘게 나뉘었다** | 무기 검사·트레이스·데미지는 항상 같이 쓰인다. 셋으로 나누면 공격 BP마다 셋을 빠짐없이 붙여야 하고, 하나만 빠져도 조용히 망가진다(트레이스는 도는데 데미지가 없는 공격). 나눠서 얻는 조합의 자유는 쓰일 일이 없었다 → 결정 3 |
| **호출이 보이지 않았다** | 베이스가 배열을 순회하니 호스트 코드만 봐서는 어떤 기능이 언제 도는지 알 수 없고, BP 배열 순서가 곧 실행 순서가 되었다 → 원칙 2 |

### 4.2 1판 — 동작만 조각, 발동 베이스는 상속 유지 (2판에 흡수됨)

동작(트레이스·콤보 등)을 조각으로 빼고 입력/이벤트 발동 베이스는 상속으로 남기는 안. 조각을 베이스 배열로 조립한다는 점에서 2판과 같은 문제를 가졌다.

---

## 5. 네트워크

- **조각은 호스트 어빌리티가 도는 머신에서만 돈다.** 어빌리티는 로컬(소유 클라)과 서버에서만 실행되고 Simulated Proxy에서는 실행되지 않는다 — [Multiplayer.md](../Conventions/Multiplayer.md). 다른 클라에 보여야 하는 것(몽타주·피격 연출)은 GameplayCue, 게임플레이 상태는 GE·복제 태그로 전한다.

| 호스트 | `NetExecutionPolicy` | 무기 트레이스 조각이 도는 머신 |
|---|---|---|
| `UCBChaserAttackAbility` | `LocalPredicted` | 소유 클라(트레이스) + 서버(히트 → 데미지) |
| `UCBAIAttackAbility` | `ServerOnly` | 서버 (AI 폰은 서버에서 로컬·권위 둘 다 참이라 두 분기가 모두 걸림) |

- **데미지는 서버에서만 적용한다.** 히트 대기는 `IsNetAuthority()` 분기에만 걸리고, 적용 직전에 엔진과 같은 권한·예측키 검사(`HasAuthorityOrPredictionKey`)를 한 번 더 한다. 클라가 보내는 것은 "누구를 맞췄는가"뿐이다 → [Combat.md](Combat.md)
- **N² 방지**: 히트는 배칭되어 한 이벤트에 여러 피격자가 실려 온다. 타겟마다 `FGameplayAbilityTargetData_SingleTargetHit`를 새로 만들어 적용한다 — 배칭된 핸들 전체를 쓰면 타겟 수만큼 도는 루프에서 N²번 적용된다.

---

## 6. 엔진 확인 사항

**GE 적용은 엔진 경로를 직접 밟는다.** `UGameplayAbility::ApplyGameplayEffectSpecToTarget`은 protected라(`GameplayAbility.h:642`) 조각이 부를 수 없다. 그 내부(`GameplayAbility.cpp:2131`)는 아래가 전부이고 모두 public이라 그대로 수행한다 — 첫 줄(권한 게이트)은 조각이 이벤트 단위로, 나머지 둘은 공용 함수 `UCBAbilitySystemLibrary::Auth_ApplyDamageToTarget()`가 타겟 단위로.

```
HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo)      ← 권한·예측키 게이트 (public)
FScopedTargetListLock(ASC, Ability)                          ← TARGETLIST_SCOPE_LOCK 과 같음 (public 구조체)
TargetData.ApplyGameplayEffectSpec(Spec, ASC->GetPredictionKeyForNewAction())
```

**AI의 로컬 판정**: `FGameplayAbilityActorInfo::IsLocallyControlled()`는 PlayerController가 없으면 폰의 `IsLocallyControlled()`로 판정하고, 서버의 AI 컨트롤러는 `IsLocalController()`가 참이다. 그래서 AI 공격에서는 로컬·서버 분기가 둘 다 걸린다.

---

## 7. 구현 기록

### 7.1 진영 어빌리티 베이스 삭제

`UCBChaserGameplayAbility`·`UCBOutlawGameplayAbility`를 상속하는 C++ 클래스가 없어 삭제했다.

> ⚠️ **BP 자식이 하나 있었다** — C++만 확인해서 처음에 놓쳤다. `GA_Chaser_Local_OpenPauseMenu`의 부모가 `UCBChaserGameplayAbility`였다(그 클래스의 함수는 쓰지 않음). `DefaultEngine.ini` `[CoreRedirects]`에 `ClassRedirects`를 넣어 부모를 `UCBGameplayAbility`로 넘기고, BP를 재저장해 새 부모가 에셋에 기록된 것을 확인한 뒤 리다이렉트 줄을 지웠다. **C++ 클래스를 지울 때는 에셋의 부모 참조도 확인할 것** — `.uasset`에 `/Script/ChainBurst.<클래스명>` 문자열이 있는지 검색하면 된다.

> **이름 변경 후 검색 함정** — `CBANS_WeaponTraceWindow` → `CBANS_GameplayEventWindow` 변경(리다이렉트 → 몽타주 17개 재저장 → 리다이렉트 제거) 때, 재저장한 뒤에도 옛 이름이 모든 파일에 남아 있었다. **클래스 참조가 아니라 인스턴스 이름**이다. 노티파이 같은 서브오브젝트는 생성 시 클래스 이름으로 이름이 붙고(`CBANS_WeaponTraceWindow_0`, 번호는 이름 테이블에 따로 저장), 클래스를 바꿔도 이미 있는 인스턴스 이름은 그대로다. 재저장이 끝났는지는 **새 이름이 들어갔는지**로 판단한다.

### 7.2 무기 트레이스 조각

- 두 공격 클래스에서 트레이스·데미지 복붙 코드와 `DamageEffectClass`·`DamageCoefficient`를 걷어내고, 각자 `WeaponTrace` 조각을 소유해 호출하도록 했다.
- **동작 변화 (의도)**: Chaser 공격이 무기가 없을 때 "활성화 후 즉시 종료"에서 **활성화 실패**로 바뀌었다. [Abilities.md](Abilities.md)의 "전제 조건은 `CanActivateAbility`에" 규칙에 맞춘 것이다. AI 공격은 원래 `CanActivateAbility`에서 검사했고 `ActivateAbility` 안전망 재검사도 그대로 유지한다.
- 트레이스 강제 정지는 원래 코드와 같이 `Super::EndAbility` **뒤**에 한다.

**BP 작업 (에디터 재시작 후)** — 데미지 값이 공격 클래스에서 조각으로 옮겨가 **다시 입력**해야 한다. 입력 전에는 데미지가 들어가지 않는다.

| BP | `Combat > Weapon Trace` 값 |
|---|---|
| `GA_Chaser_Attack_Basic` | `DamageEffectClass = GE_Damage`, `DamageCoefficient = 1.0` |
| `GA_Chaser_Attack_Skill_A` | `GE_Damage`, **1.5** |
| `GA_Rogue_Attack_Basic` | `GE_Damage`, 1.0 |

값은 작업 전에 `.uasset`에서 직접 읽은 것이다(1.0은 기본값이라 에셋에 저장돼 있지 않았다). 2판 구조로 잠시 BP의 `Fragments` 배열에 조각을 붙였다면, 그 항목은 클래스가 사라져 로드 시 경고가 나고 BP를 저장하면 사라진다.

### 7.3 영역 공격 조각

- `UCBFragment_AreaAttack` 추가, `UCBAIAttackAbility`가 무기 트레이스와 함께 소유. 판정 시점 이벤트 `Event.Combat.AreaAttack`(네이티브 태그) 추가 — 노티파이는 기존 `CBAN_SendGameplayEventToOwner` 재사용.
- 무기 트레이스의 타겟당 적용 코드를 `UCBAbilitySystemLibrary::Auth_ApplyDamageToTarget()`으로 옮겼다. **동작 변화 없음** — 무기 트레이스는 같은 순서(권한 검사 → 타겟마다 적용 → 피격 큐)로 이 함수를 부른다.
- 새 조각이 기본 서브오브젝트라 **기존 AI 공격 BP(`GA_Rogue_Attack_Basic`)에도 `Area Attack` 칸이 생긴다.** 비어 있어도 몽타주에 영역 공격 노티파이가 없으므로 동작은 그대로다.

### 7.4 투사체 조각

- `UCBFragment_Projectile`·`ACBProjectile` 추가, `UCBAIAttackAbility` 가 셋째 조각으로 소유. 발사 이벤트 `Event.Combat.FireProjectile`(네이티브 태그), 발사 방식 enum `ECBProjectileLaunchMode`(`CBEnumTypes.h`) 추가.
- `Auth_ApplyDamageToTarget()` 를 `MakeDamageSpec()` + `Auth_ApplyDamageSpecToTarget()` 로 나누고 기존 함수는 둘을 잠금 안에서 차례로 부른다. **동작 변화 없음** — 예전에 손으로 넣던 `AddHitResult` 를 뺐는데, 엔진 `FGameplayAbilityTargetData::ApplyGameplayEffectSpec()` 이 컨텍스트에 히트가 없으면 같은 HitResult 를 넣으므로 결과가 같다.
- `Auth_ApplyDamageSpecToTarget()` 은 시전자 ASC 가 없으면 적용하지 않는다. 엔진 적용 함수가 이 상태를 `ensure` 로 막아, 시전자가 파괴된 뒤 명중한 투사체가 콜스택을 찍기 때문.
- `UCBAIAttackAbility::BuildActionCueParameters()` 의 블랙보드 조회를 `GetBlackboardTarget()` 으로 옮겼다(동작 동일). 투사체 조준과 공유.
- 기존 AI 공격 BP 에도 `Projectile` 칸이 생긴다. 발사 노티파이가 없으면 동작은 그대로다.
- **후속 수정 (연사 테스트)**: 한 몽타주에서 4연사 + 모션 워핑 회전을 쓰자 두 문제가 드러났다.
  - 투사체가 늘 첫 발의 소켓 자리(몸 기준)에서 나감 → 서버가 보스를 그리지 않으면 엔진 기본 `AlwaysTickPose` 가 본을 갱신하지 않아서였다. `ACBAICharacter` 메시를 `OnlyTickMontagesAndRefreshBonesWhenPlayingMontages` 로 바꿨다(AI 근접 트레이스도 같은 문제였음) → [Montage.md](Montage.md).
  - 몸은 플레이어를 따라 도는데 탄은 발동 순간 위치로 날아감 → 조준을 **발사마다 타겟 현재 위치**로 바꿨다.

---

## 8. 검증

1. UBT 컴파일 통과 ✅
2. 공격 BP 디테일 패널에서 `Weapon Trace` 안의 데미지 값이 편집된다 (§9)
3. 리슨 서버 호스트·클라 양쪽에서 **데미지 GE가 한 번만** 적용된다
4. **두 대상을 동시에 때렸을 때 각자 1회씩만** (N² 재발 없음)
5. 콤보가 끝까지 이어진다 (재활성화 경로에서 트레이스가 끊기지 않음)
6. AI 공격이 플레이어에게 데미지를 준다
7. Simulated Proxy에서 공격 몽타주와 피격 연출이 그대로 보인다

---

## 9. 미확인 항목

| 항목 | 왜 미확인인가 | 확인 방법 |
|---|---|---|
| `Instanced, NoClear` 기본 서브오브젝트의 BP 디테일 편집 | `EditInlineNew`가 없는 `UObject` 기본 서브오브젝트를 인라인 편집하는 것은 표준 동작으로 알고 있으나 실측 전 | 에디터에서 `GA_Chaser_Attack_Basic` → `Combat > Weapon Trace` 펼쳐 값 입력. 안 되면 조각 클래스에 `EditInlineNew` 추가 검토 |

---

## 10. 보류 — 재논의 필요

2판에서 계획했던 아래 항목은 이번 방향(베이스는 공용 로직만, 기능은 조각)과 맞지 않거나 우선순위가 정해지지 않아 **보류**한다. 삭제하지 않고 기록만 남긴다.

- **발동 방식 베이스(`UCBInputActionAbility`/`UCBEventActionAbility`) 제거.** 근거는 유효하다 — `BoundInputTag`는 어떤 .cpp에서도 읽지 않고(입력 매칭은 로드아웃의 스펙 동적 태그), 이벤트 발동·캔슬은 엔진 `AbilityTriggers`/`CancelAbilitiesWithTag`로 대체된다. 다만 콤보 입력 윈도우를 어디로 옮길지와 BP 리페어런트 비용을 함께 다시 정해야 한다.
- **장착/해제 C++ 클래스 통합.** 두 클래스는 `IsCombatMode()` 비교값과 `SetCombatMode()` 인자만 다르다. 조각으로 뺄지, 한 클래스에 bool 프로퍼티로 합칠지 미정.
- **콤보·조준 정렬·무작위 변형·타겟 워프의 조각화.** 두 번째 사용처가 생기면 그때 뺀다.

---

## 관련 문서

- 어빌리티 베이스·`NetExecutionPolicy`·기능 조각 사용법: [Abilities.md](Abilities.md)
- 트레이스·히트 검증·N² 주의: [Combat.md](Combat.md)
- `Auth_`/`Local_` 규칙, Simulated Proxy 반영: [Multiplayer.md](../Conventions/Multiplayer.md)
- 덜어내기 점검표: [DesignReview.md](../Conventions/DesignReview.md)
