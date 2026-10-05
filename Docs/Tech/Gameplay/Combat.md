# 전투 컴포넌트 구조

> 무기 스폰/트레이스/히트 검증, 콤보 상태 소유를 담당하는 CombatComponent 계층.

```
UCBCombatComponent (Abstract, UCBExtensionComponent 상속)
├── UCBChaserCombatComponent
├── UCBOutlawCombatComponent
└── UCBRogueCombatComponent
```
- 무기 스폰/파괴/등록: 서버 전용 (`Auth_` 접두사)
- **무기 수명은 컴포넌트가 소유한다.** 부착(attach)도 `Owner`도 파괴를 전파하지 않으므로, 캐릭터가 사라져도 무기는 월드에 그대로 남는다. `UCBCombatComponent::EndPlay`에서 권위일 때 `Auth_DestroyAllWeapons()`로 직접 정리한다 — 복제 액터라 서버에서 파괴하면 전 클라이언트에서 사라진다.
  - **캐릭터의 `Destroyed()`가 아니라 컴포넌트의 `EndPlay`인 이유**: 명시적 파괴뿐 아니라 맵 전환·종료까지 덮고, 무기 저장소(`EquippedWeapons`)와 수명이 한 클래스 안에 모인다. 무기가 스스로 오너의 파괴를 구독하게 하면 관리 주체가 둘로 갈리고 맵 전환 경로도 놓친다
  - **`EndPlayReason`으로 거르지 않는다.** 무기는 전부 런타임 스폰이라 어떤 이유로 끝나든 남길 이유가 없다
  - **AttackPower GE 회수도 여기서 한다**(`Auth_RemoveWeaponAttackPowerEffect()`, 무기 파괴 직전). 전투 상태 태그와 이유가 같다 — 플레이어 ASC는 PlayerState 소유라 폰보다 오래 살고, 걷어내지 않으면 **죽을 때마다·캐릭터를 바꿀 때마다 새 폰이 적용하는 GE가 그 위에 계속 쌓여 공격력이 무한 증가한다.**
    - **ASC가 폰 바깥에 있을 때만 실제로 제거한다** (`ASC->GetOwnerActor() != GetOwner()`). AI는 ASC가 캐릭터 자신에게 있어 폰과 함께 사라지므로 회수할 대상이 없고, 파괴 중인 ASC를 건드릴 이유도 없다 — 핸들 목록만 비우고 빠진다
    - **게임모드의 `Auth_ClearLoadoutGrants()`로는 잡히지 않는다.** 무기 GE는 로드아웃이 아니라 컴뱃 컴포넌트가 `ApplyGameplayEffectSpecToSelf()`로 직접 적용하므로 `LoadoutEffectHandles`에 기록되지 않는다. 적용한 주체가 회수해야 한다
- **무기 저장소는 목록(`TArray<FCBRegisteredWeaponData> EquippedWeapons`, 복제)**. 단일 무기는 1개, **쌍수 무기는 2개를 별개로 등록**(최대 `MaxWeaponCount = 2`). 장착/해제(`OnEnter/ExitCombatMode`)는 목록을 순회해 모든 무기를 Hand/Sheath에 부착.
  - 무기가 어느 소켓(주무기/부무기·전투/비전투)에 붙는지는 `ECBWeaponSocketType`(무기 카테고리·무브셋 아님, **소켓 조회 전용 키**)으로 결정. `UCBWeaponSocketData`가 타입별 소켓 이름을 보유. 쌍수는 `Dagger_L`/`Dagger_R` 두 소켓 타입을 각각 사용.
  - **등록 중복 검사 = 소켓 타입 점유 기준** ("한 소켓엔 무기 하나"). 무기 식별 태그(`WeaponTag`)는 제거됨 — `UCBWeaponData`·`FCBRegisteredWeaponData` 모두 `WeaponSocketType`을 보유. `None`은 소켓 미점유(소환형 무기)라 중복 검사에서 제외되며 `HasValidData()`/`IsValid()` 유효 조건에도 포함하지 않는다(인스턴스 존재만 검사).
  - 소켓 타입은 **무기 BP(`ACBBaseWeapon`, 소켓 오버라이드 베이킹용)와 무기 데이터(`UCBWeaponData`, 슬롯 선언용) 두 곳에 존재** — 등록 시 둘이 다르면 경고 로그로 설정 실수를 감지한다(등록은 계속 진행).
  - **외부 조회는 `GetEquippedWeaponInstances()`**(BlueprintPure) — 유효한 무기 액터만 담아 돌려준다. 목록이 복제되므로 서버·클라 어디서나 쓸 수 있고, 클라에서 목록이 무기 액터보다 먼저 도착해 참조가 비어 있는 항목은 거른다. 게임플레이 큐 같은 연출이 캐릭터에서 `Get Component by Class(CBCombatComponent)`로 컴포넌트를 찾아 호출한다(→ [Burst.md](Burst.md) "연출").
- 무기 트레이스: `StartWeaponTrace()` → `TickWeaponTrace()` → `StopWeaponTrace()`
  - 트레이스는 노티파이 스테이트 `CBANS_GameplayEventWindow`(기본 태그 `Event.Combat.TraceStart/End`, 옛 이름 `CBANS_WeaponTraceWindow`)가 구간을 제어
  - **채널은 전용 `Weapon`**(`CBCollisionChannels::Weapon`, 아래 별도 절 참조). 캐릭터는 캡슐=`Ignore` / 메시=`Overlap`이라 판정이 피직스 애셋 실루엣으로 이뤄지고, 일직선의 여러 적을 관통해서 벤다
  - **무기별 root/tip을 순회 트레이스**(쌍수는 양손 블레이드 모두). `AlreadyHitActors`는 무기 간 **공유**하여 한 스윙에 같은 대상이 두 블레이드로 이중 히트되는 것을 방지
  - **판정 형태는 구체가 아니라 세로로 선 캡슐이다** (아래 「세로 판정 밴드」 절 참조)
  - **중복 히트 방지는 `AlreadyHitActors` 하나가 전담한다** (배칭이 아니다 — 아래 별도 절)
  - ⚠️ **`CapsuleTraceMulti`의 반환값은 쓰지 않는다.** 이 함수는 블로킹 히트가 있을 때만 `true`를 돌려주는데 캐릭터는 `Overlap`이라, 반환값으로 거르면 **적을 맞혀도 전부 무시된다**. 오버랩 결과는 `bHit`와 무관하게 `HitResults`에 담기므로 배열을 그대로 순회한다
  - **종료 시 마지막 구간을 한 번 더 트레이스한다** (`StopWeaponTrace()`가 `TickWeaponTrace()`를 한 번 호출). 트레이스는 프레임 단위 샘플링이라 **마지막 Tick ~ `NotifyEnd` 사이의 휘두름이 통째로 빠지고**, 재생 속도(공격 속도 GE)가 빠를수록 누락되는 호가 커진다
    - `bIsTracing` 가드를 두는 이유: `EndAbility`의 안전장치 호출 등으로 이미 종료된 뒤 다시 불리면, stale한 `PrevRootLocs`/`PrevTipLocs`로 엉뚱한 구간을 중복 트레이스한다
  - **진영 필터**: 트레이스에 걸린 액터는 적대(`Hostile`)일 때만 배칭 목록에 담긴다(`IsHostileTarget()`). 아군·중립은 히트에서 제외 → 서버 RPC도 나가지 않는다
    - `AlreadyHitActors`에는 **적대 여부와 무관하게** 기록한다. 같은 대상을 트레이스 등분마다 다시 판정하지 않기 위함
  - **벽 검사**: 걸린 적대 대상은 **공격자 중심 → 대상 중심** 한 줄이 벽에 막혔는지 한 번 더 본다(`UCBAbilitySystemLibrary::IsBlockedByWall()`, 영역 공격과 같은 함수·같은 `Weapon` 채널 기준). 막혔으면 히트에서 뺀다 → 아래 "벽 너머 히트 — 스윕이 놓치는 경우"
- 히트 배칭: 첫 타는 즉시 전송하고, 뒤따라 걸리는 대상만 `HitBatchInterval`(0.1초) 창으로 묶어 RPC를 줄인다 (아래 별도 절)
- 히트 검증: 로컬에서 감지 → `Server_NotifyAttackHit()` RPC → 서버에서 **진영 → 거리** 순으로 재검증. 쌍수는 무기 중 **가장 먼 tip 거리**를 허용 거리 기준으로 사용 (`HitValidationTolerance`)
  - **왜 서버가 직접 훑지 않는가(= 왜 로컬 감지인가).** 몽타주 재생 위치는 머신마다 미세하게 어긋난다. 서버가 직접 트레이스하면 **레이턴시만큼 늦은 자세로 판정**해, 때린 쪽 화면에서 분명히 맞은 스윙이 빗나간다. 그래서 보이는 화면(`IsLocallyControlled`, AI는 서버가 곧 로컬)에서 판정하고 서버는 결과만 다시 잰다
  - **클라 필터만으로는 부족하다.** 트레이스는 로컬에서 돌고 결과만 RPC로 올라오므로, 조작된 클라가 아군 타겟을 실어 보낼 수 있다. 서버가 같은 기준으로 한 번 더 판정한다
  - **클라가 보내는 것은 "누구를 맞췄는가"뿐이다.** 데미지 값·계수는 전부 서버가 스펙을 만들어 적용하므로 RPC 페이로드를 조작해도 피해량은 바뀌지 않는다
  - **벽 검사는 서버 검증에 넣지 않았다.** AI 는 감지 자체가 서버에서 돌아 이미 권위 판정이고, 정직한 플레이어 클라는 벽 너머 히트를 보내지 않는다. 서버가 다시 재면 **지연만큼 다른 위치**로 판정해, 클라 화면에서 열려 있던 사이 AI 가 기둥 뒤로 들어간 경우 "맞았는데 거부"가 난다. 협동(PvE)에서는 이 손해가 부정행위 방지 이득보다 크다 — **조작된 클라는 벽 너머 히트를 보낼 수 있다(수용).** 경쟁 규칙이 되면 거리 검증처럼 허용 오차를 둔 서버 벽 검사를 추가한다
- **진영 판정 기준은 한 곳**: `FGenericTeamId::GetAttitude(공격자, 대상) == Hostile`. `ACBAIController::IsValidTarget()`·퍼셉션 소속 필터와 같은 전역 attitude solver를 탄다 → [Teams.md](../Foundation/Teams.md)
  - ⚠️ **중립은 때릴 수 없다.** solver는 한쪽이라도 Neutral이면 Neutral을 반환한다. `ACBBaseCharacter`의 팀 기본값이 Neutral이므로, **팀 지정이 빠진 캐릭터는 조용히 무적이 된다**
  - ⚠️ **`IGenericTeamAgentInterface`를 구현하지 않은 액터도 걸러진다**(엔진 구현상 Neutral). 파괴 가능한 오브젝트를 무기로 때리려면 그때 별도 경로가 필요하다 — 오브젝트가 `Weapon` 채널에 응답하게 만드는 것만으로는 진영 필터에서 걸린다
- **데미지 적용은 히트마다 타겟 하나씩.** 히트가 배칭되어 한 이벤트에 여러 피격자가 실려 오므로(`FGameplayAbilityTargetDataHandle`), 무기 트레이스 기능(`UCBFragment_WeaponTrace::OnAttackHit`)은 타겟을 하나씩 돌며 **그 회차의 HitResult 하나만 담은 타겟 데이터**(`FGameplayAbilityTargetData_SingleTargetHit`)로 적용한다. 공격 어빌리티(`UCBChaserAttackAbility`/`UCBAIAttackAbility`)는 이 기능을 소유해 호출만 한다 → [Abilities.md](Abilities.md) "기능 조각"
  - 타겟 하나에 대한 적용 코드는 `UCBAbilitySystemLibrary::Auth_ApplyDamageToTarget()`에 있고, 아래 영역 판정과 공유한다. 아래 두 주의 사항도 그 함수 안의 이야기다.
  - ⚠️ **핸들 전체를 넘기면 안 된다.** `ApplyGameplayEffectSpecToTarget`은 넘긴 핸들의 *모든* 타겟에 적용하므로, 타겟 수만큼 도는 루프에서 전체 핸들을 넘기면 N² 번 적용되어 **각자 N 배 데미지**를 받는다(한 명만 때리면 1×1이라 증상이 안 보인다).
  - **타겟은 오직 `TargetData`가 정한다.** 앞의 `CurrentSpecHandle`/`CurrentActorInfo`/`CurrentActivationInfo`는 `HasAuthorityOrPredictionKey()` 게이트(권한·예측키 검사)에만 쓰이며 적용 대상과 무관하다. 실제 대상은 `FGameplayAbilityTargetData_SingleTargetHit::GetActors()` = HitResult의 액터이고, 소스는 스펙 컨텍스트의 시전자 ASC다.
  - **HitResult는 스펙을 만든 뒤 컨텍스트에 붙인다** (`SpecHandle.Data->GetContext().AddHitResult()`). `MakeOutgoingGameplayEffectSpec()`이 컨텍스트를 자체 생성하므로, 미리 만든 `FGameplayEffectContextHandle`을 넘길 자리가 없다 — 따로 만들어두면 조용히 버려진다.
- 무기 등록 시 `Data.Weapon.AttackPower` GE를 ASC에 적용, 컴포넌트 `EndPlay`에서 회수. **무기별로 각자의 `WeaponData` 공격력 GE를 적용**(핸들도 무기별로 보관 후 회수 시 전부 제거) — 쌍수는 두 무기 데미지 값을 각각 설정
- 전투 상태(태그) 소유: `SetCombatMode(bool)`이 유일한 진입점. 내부에서 `IsCombatMode()`(= `Status.Combat.InCombat` 태그 검사)로 idempotent 가드 후 `OnEnterCombatMode()`/`OnExitCombatMode()` 호출 — 이 두 함수가 무기 부착(Hand/Sheath)과 상태 태그(서버·예측 클라가 동일하게 `TagOnly` 복제 상태로 1회 추가/제거 — 로컬 `None` 추가와 서버 복제 추가를 나누면 예측 경고·카운트 불일치 발생) 처리 부수효과를 함께 소유. 어빌리티(`UCBGAChaserEquipWeapon`/`UnequipWeapon`)는 `SetCombatMode()`만 호출하고 태그를 직접 건드리지 않는다 — 무기 부착 상태와 태그가 다른 곳에서 따로 갱신되어 어긋나는 것을 방지.
  - **전투 상태 태그도 `EndPlay`에서 정리한다.** 루스 태그는 GE와 달리 **넣은 쪽이 빼야만** 사라지는데, 플레이어 ASC는 PlayerState 소유라 폰보다 오래 산다. 정리하지 않으면 사망 리스폰·무기 변경으로 새로 스폰된 폰이 **무기를 칼집에 둔 채 전투 상태로 시작**하고, 장착 어빌리티는 `SetCombatMode(true)`의 idempotent 가드에 걸려 조기 반환하므로 **무기가 영영 손에 붙지 않는다.** 그런데도 공격 어빌리티의 `ActivationRequiredTags(Status.Combat.InCombat)`는 통과한다.
  - 정리 주체는 **직접 붙인 인스턴스뿐**이다(`bCombatTagApplied` 플래그). 시뮬레이티드 프록시는 `TagOnly` 복제로 태그를 받았을 뿐이라 제거 주체가 아니다. `UCBLocomotionProcessor`가 미러링 태그(`Idle`/`InAir`/`Run`)를 같은 방식(적용 플래그 + `EndPlay` 정리)으로 처리한다 (→ [Locomotion.md](Locomotion.md))
- 콤보 상태 보관: `AdvanceCombo(Tag, MaxCount)`(재생 인덱스 반환 + 내부 전진) / `ResetCombo()`. 컴포넌트는 **상태만 소유**하고 타이머도 판단도 갖지 않는다.
  - **호출 주체는 `UCBChaserAttackAbility` 하나다.** 전진은 `SelectActionMontageIndex()`에서, 리셋은 `CleanupActionState()`(노티파이 종료·블렌드 아웃 종료·캔슬 모든 경로 — 몽타주가 다른 몽타주에 끊긴 경우도 캔슬 경로로 리셋)에서 호출한다. 입력 윈도우로 다음 타를 이어갈 때는 `EndAbility`만 부르고 이 훅을 거치지 않으므로 콤보가 유지된다. 콤보 여부를 정하는 `IsCombo`도 이 어빌리티의 프로퍼티다 — **액션 어빌리티 베이스(`UCBActionAbility`)는 콤보를 알지 못한다.**
  - 콤보 인덱스는 **비복제**다. 서버·소유 클라가 각자 예측 전진하고, sim proxy는 자기 값을 쓰지 않고 큐 파라미터로 받은 서버 인덱스로 재생한다.
  - **복제하지 않는 이유**: 예측하는 값에 복제를 얹으면 지연되어 도착한 서버 값이 이미 앞서 나간 클라 값을 덮어써 **정상 연타에서도 콤보가 뒤로 밀린다**(GAS가 어트리뷰트를 절대값이 아니라 델타로 예측하는 것과 같은 Override 문제).
  - **거부 시 롤백이 예측의 짝이다.** 서버가 활성화를 거부하면 클라만 인덱스가 앞선 채 남는다 — 거부는 `bWasCancelled = false`로 종료되므로 `CleanupActionState()` 경로에도 걸리지 않는다. 그대로 두면 **체인이 정상 종료될 때까지 소유 클라와 나머지 화면이 서로 다른 콤보 단계를 재생**하고, 클립 길이가 달라 루트 모션 이동량까지 어긋난다.
    - 거부 감지는 **예측 키의 거부 델리게이트**(`FPredictionKey::NewRejectedDelegate()`)로 한다. `UCBChaserAttackAbility::SelectActionMontageIndex()`가 `AdvanceCombo()` 직후 등록하고, 콜백이 `RollbackCombo(키)`를 부른다. 소유 클라에서만 등록한다(서버는 발화하지 않고 전역 맵에 항목만 남는다).
    - ⚠️ **`EndAbility`에서 `ActivationMode == Rejected`를 보는 방식은 쓰면 안 된다.** 어빌리티가 `InstancedPerActor`(`UCBGameplayAbility` 생성자)라 콤보 다음 타가 **같은 인스턴스를 재사용하며 `ActivationInfo`를 덮어쓴다**. 엔진의 거부 처리는 인스턴스의 *현재* 키와 대조하므로(`AbilitySystemComponent_Abilities.cpp:2324`), 콤보 전환이 거부 통보보다 먼저 일어나면 `EndAbility` 자체가 호출되지 않는다. 반면 `FPredictionKeyDelegates`는 키 값으로만 관리되는 전역 맵이라 인스턴스 생사와 무관하게 발화하고, 거부 처리의 **첫 줄**에서 브로드캐스트된다.
    - **방향은 언제나 클라가 서버를 따라간다.** 서버가 거부한 활성화는 게임적으로 없던 일이므로, 되돌리는 쪽이 정확하다. 반대로 서버가 클라를 따라가면 검증이 무의미해지고, 이미 서버 인덱스로 재생을 마친 프록시와 또 어긋난다.
    - **`0`으로 리셋하면 안 된다.** 서버는 전진 직전 값에 머물러 있으므로 방향만 바뀐 채 계속 어긋난다. `AdvanceCombo()`가 **함수 맨 앞에서** 전진 직전 상태를 스냅샷해두고(내부 리셋/순환 분기보다 먼저) `RollbackCombo()`가 그 값으로 복원한다.
    - 스냅샷은 한 단계 깊이뿐이라, 여러 활성화가 동시에 응답을 기다리면 낡은 값이 될 수 있다. 그래서 **스냅샷과 거부 통보의 예측 키를 대조**해 일치할 때만 되돌린다(불일치 시 건너뛰고 체인 종료에서 수렴). 몽타주 길이가 왕복 지연보다 훨씬 길어 실제로 겹치는 일은 드물다.
    - 새 RPC도 새 복제 속성도 필요 없다 — 서버가 이미 보내는 `ClientActivateAbilityFailed`를 쓴다. **서버 코드는 변하지 않는다.**

## 세로 판정 밴드 — 수평은 소켓, 수직은 몸 높이

무기 트레이스는 **수평(XY)은 무기 소켓 구간을 그대로 따라가되, 수직(Z)은 오너 캡슐 높이까지 넓힌 캡슐**로 쏜다.

```
밴드 = [오너 캡슐 바닥, 오너 캡슐 머리끝] ∪ [이번 구간의 소켓 Z]
가로 = TraceRadius (변함 없음)
```

**왜 필요한가 — 블레이드가 높이 지나가면 키 작은 적이 스윙 평면 아래로 빠진다.** 판정이 `root→tip` 선분이 쓸고 간 얇은 면이라, 적의 키가 공격 모션의 높이와 어긋나면 정면으로 겨눠도 빗나간다. 덩치가 제각각인 적을 상대하는 한 모션만으로는 못 맞춘다.

- **가로가 아니라 세로만 넓히는 것이 요점이다.** `TraceRadius`를 키우면 모든 방향으로 뚱뚱해지는데, 그건 캡슐을 판정에서 뺀 이유(실루엣보다 뚱뚱해서 **눈에 띄게 빗나간 공격이 맞는다**)를 그대로 되살린다. 가로로 빗나간 것은 플레이어 눈에 바로 들통나지만 세로 오차는 거의 보이지 않으므로, **넓혀도 되는 축과 그렇지 않은 축이 다르다.**
- **높이를 고정 cm(예: `TraceHalfHeight = 40`)로 두지 않은 이유**: 캐릭터마다 맞는 값이 달라 매직 넘버가 하나 생긴다. 오너 캡슐에서 유도하면 큰 보스는 자동으로 커지고 새 캐릭터를 추가해도 맞출 값이 없다.
- **언리얼 캡슐 트레이스는 회전을 못 준다**(항상 월드 Z축). 여기서는 그게 정확히 원하는 성질이다 — 세로로만 넓어진다.
- **소켓 Z와 합집합인 이유**: 머리 위로 치켜든 공격이나 발밑을 훑는 공격에서 밴드가 몸 높이로 잘리면 **현행 사거리를 잃는다.** 소켓이 몸 밖으로 나가면 거기까지 늘린다.
- **시작·끝의 Z를 밴드 중심으로 눕힌다.** 캡슐 트레이스는 스윕 전체에 하나의 반높이만 쓸 수 있는데, 밴드가 이미 두 끝점의 Z를 모두 포함하므로 원래 스윕이 세로로 덮던 범위는 손실 없이 캡슐 안에 들어온다.
- **서버 검증 허용 거리도 같이 늘린다**(`Server_NotifyAttackHit()`). 판정 볼륨과 검증 기준이 다른 값을 보면 **맞았는데 서버가 거부**한다. 소켓이 몸 밖으로 벗어난 몫은 `AttackerToTipDistance`에 이미 포함되어 있어 따로 더하지 않는다.

**바닥에 여유(margin)를 두지 않는다.** 캡슐 바닥은 정의상 지형에 파묻혀 있지 않다 — 파묻혀 있으면 캐릭터 자신이 지형을 뚫은 상태다. 따라서 "시작부터 파묻혀 스윕이 죽는" 상황은 생기지 않는다.

**수용하는 대가 둘**
- **내려베기는 사거리가 늘어난다.** 캡슐이 항상 수직이라, 블레이드가 수직에 가까운 자세에서는 세로 확장이 곧 **칼날 방향 확장**이 된다. 가슴 높이에서 멈추는 내려찍기가 발밑의 적에게 닿는다.
- **지형이 올라오면(계단·턱·경사) 밴드가 걸려 스윕이 끊길 수 있다.** 밴드가 발밑까지 내려가 있어서, 가슴 높이 가로베기인데 앞의 턱에 걸리는 경우가 생긴다. 지형이 트레이스를 끊는 것 자체는 **의도된 동작**(벽 너머 히트 방지)이라, 이것이 버그인지 사양인지는 실제 레벨에서 판단할 문제다. 작은 상수 마진으로는 지형 기복을 못 막으므로 그런 파라미터를 미리 두지 않았다.

## 히트 판정 채널 — `Weapon`

무기 트레이스는 이동용 `ECC_Pawn`이 아니라 전용 트레이스 채널 `Weapon`을 쓴다. 채널 정의는 `Config/DefaultEngine.ini`의 `[/Script/Engine.CollisionProfile]`이 소유하고, 코드에서는 `Types/CBCollisionChannels.h`의 별칭으로만 참조한다(인덱스가 흩어지지 않게).

**왜 나눴나 — "맞을 수 있는가"와 "이동을 막는가"는 다른 질문이다.** 한 채널에 얹혀 있으면 둘을 따로 끌 수 없다.

| 상황 | 이동 차단 | 히트 판정 |
|---|---|---|
| 평소 | 막음 | 받음 |
| 회피 무적(i-frame) | **막음** | 제외 |
| 시체 | 통과 | 제외 |

한 채널이면 회피 무적이 곧 "관통 가능"이 되어버린다.

**응답 설정은 `ACBBaseCharacter` 생성자에 있다.** 캡슐=`Ignore`, 메시=`Overlap`. BP가 아니라 C++인 이유는 캐릭터 BP를 새로 만들 때마다 다시 세팅해야 하고, **한 번 빠뜨리면 그 캐릭터만 조용히 안 맞기 때문**이다.

- **캡슐을 판정에서 뺀 이유**: 캡슐 반지름이 실루엣보다 뚱뚱해 **눈에 띄게 빗나간 공격이 맞는다.** 메시는 피직스 애셋 바디로 판정되므로 실루엣에 가깝고, `FHitResult::BoneName`이 들어와 부위별 처리도 열린다.
  - 참고로 엔진 기본 `CharacterMesh` 프리셋은 `Pawn: Ignore`라, 전환 전에는 판정이 **전부 캡슐**에서 이뤄지고 있었다.
- **메시가 `Block`이 아니라 `Overlap`인 이유**: 블로킹 히트는 트레이스를 거기서 끊는다. `Overlap`이어야 일직선으로 선 여러 적을 관통해서 벨 수 있다. 채널 기본 응답이 `Block`이라 **벽·지형은 여전히 트레이스를 끊는다** — 다만 스윕이 벽을 **지날 때만**이다(아래 절).

### 벽 너머 히트 — 스윕이 놓치는 경우

트레이스는 칼날 위 샘플 지점이 **지난 프레임 → 이번 프레임**으로 움직인 경로를 스윕한다. 그래서 벽이 막는 것은 "칼날이 휘두르는 도중 벽을 가로지르는" 경우뿐이다.

| 상황 | 스윕만으로 |
|---|---|
| 휘두르는 칼날이 벽을 가로질러 들어감 | ✅ 스윕이 벽에서 끊김 |
| 칼날 일부가 **처음부터 벽 너머에 있는 채로** 휘둘러짐 (벽에 붙어 휘두름·얇은 벽·문·긴 무기) | ❌ 벽 너머 샘플은 벽 너머 공간 안에서만 움직여 스윕이 벽을 지나지 않음 → 뒤의 적이 맞음 |

그래서 `TickWeaponTrace()` 가 걸린 적대 대상마다 **공격자 중심 → 대상 중심** 한 줄을 추가로 긋는다(`IsBlockedByWall()`).

- **기준점이 손·칼날이 아니라 공격자 중심인 이유**: 손 기준이면 공격자의 팔이 벽을 뚫은 경우를 놓친다. 중심 기준은 몸 → 팔 → 칼날을 대략 한 번에 본다.
- **새 오판정이 생기지 않는다.** 판정 밴드가 이미 오너 캡슐 바닥~머리끝으로 서 있어, 공격자와 대상 사이의 낮은 턱·난간은 지금도 스윕을 끊는다. 중심선이 추가로 막는 것은 사실상 "칼날이 벽을 뚫은" 경우뿐이다.
- **막힌 대상은 `AlreadyHitActors` 에 기록하지 않는다.** 기록하면 같은 스윙 안에서 대상이 벽 밖으로 나와도 다시 맞을 수 없다. 대신 같은 틱 안에서는 위치가 같으므로 틱 단위 집합(`WallBlockedActors`)으로 바디·샘플마다 다시 긋지 않는다. 비용은 "벽 너머에 걸린 적대 대상 수 × 틱"이다.
- **영역 공격과 같은 한계** — 대상 중심 한 줄만 보므로 몸 일부가 벽 밖으로 나와 있어도 중심이 벽 뒤면 맞지 않는다.
- 디버그는 `bShowDebugTrace` 를 따른다 — 막힌 대상에게 공격자 중심에서 빨간 선을 그린다.
- 서버 검증에는 넣지 않았다 → 위 "히트 검증".
- ⚠️ **`CapsuleTraceMulti`의 반환값을 쓰면 안 된다.** 이 함수는 **블로킹 히트가 있을 때만** `true`를 돌려준다(`SweepMultiByChannel`). 대상이 `Overlap`이면 적을 맞혀도 `false`가 나오고, 오버랩 결과는 `bHit`와 무관하게 `HitResults`에만 담긴다. 그래서 `TickWeaponTrace()`는 반환값을 무시하고 배열을 그대로 순회한다. (`SphereTraceMulti`에서 바꿔 왔지만 주의 내용은 그대로 유효하다 — 스윕 계열 전부가 같다)
- ⚠️ **채널은 이름이 아니라 인덱스(`ECC_GameTraceChannelN`)로 에셋에 저장된다.** ini에서 순서를 바꾸거나 지우면 저장된 콜리전 설정이 다른 채널로 밀린다. 추가만 하고 제거·재배치는 하지 말 것.

**회피 무적을 얹을 때 주의** — 콜리전 응답은 복제되지 않는데 **트레이스는 공격자 머신에서 돈다.** 회피하는 쪽이 자기 클라에서만 응답을 꺼봐야 공격자 화면에서는 그대로 맞는다. 사망과 같은 패턴으로 가야 한다: 복제되는 태그를 진입점으로 두고 **각 인스턴스가 태그 콜백에서 자기 몫의 응답을 끄고**, 권위 판정은 `Server_NotifyAttackHit()`에서 타깃의 무적 태그를 확인한다 (→ [Abilities.md](Abilities.md)의 사망 라이프사이클, [Multiplayer.md](../Conventions/Multiplayer.md)).

## 중복 히트 방지와 히트 배칭 — 별개의 두 장치

이름이 비슷해 헷갈리기 쉬운데 **책임이 완전히 다르다.** 배칭을 꺼도 중복은 생기지 않는다.

| | 담당 | 목적 | 단위 |
|---|---|---|---|
| 중복 히트 방지 | `AlreadyHitActors` | 한 스윙에 같은 대상을 여러 번 때리지 않기 | 스윙 |
| 히트 배칭 | `PendingHits` + 배칭 창 | RPC 수 줄이기 | 0.1초 |

### 중복 히트 방지 — `AlreadyHitActors`

트레이스는 매 프레임 × 등분(`TraceSubdivisions`) × 무기 수만큼 돌기 때문에, 같은 적이 한 스윙 동안 수십 번 걸린다. `AlreadyHitActors`에 한 번 들어간 액터는 그 스윙이 끝날 때까지 다시 담기지 않는다(`StartWeaponTrace()`에서 비움).

- **무기 간 공유한다.** 쌍수가 양손 블레이드로 같은 대상을 훑어도 한 번만 히트된다.
- **적대 여부와 무관하게 기록한다.** 아군이라 걸러진 대상도 기록해야 등분마다 진영 판정을 다시 돌리지 않는다.
- ⚠️ **이게 곧 RPC 수의 상한이다.** 한 액터는 스윙당 최대 한 번만 `PendingHits`에 들어가므로, 배칭이 전혀 없어도 **한 스윙의 RPC 수는 "맞은 적의 수"를 넘지 않는다.** 프레임 수와는 무관하다.

### 히트 배칭 — 첫 타는 즉시, 뒤따르는 대상만 묶기

`UpdateHitBatch()`가 배칭 창을 진행한다. 창이 **닫혀 있으면 걸린 히트를 그 자리에서 전송**하고, 그 전송이 새 창을 연다. 창이 열려 있는 동안 걸린 대상들은 `PendingHits`에 쌓였다가 0.1초가 차면 RPC 하나로 나간다. 창이 찼는데 쌓인 게 없으면 창을 닫아, 다음 히트가 다시 지연 없이 나가게 한다.

- **스윙 첫 타는 항상 지연 0이다.** `StartWeaponTrace()`가 창을 닫힌 상태로 초기화하므로, 타격감에서 가장 중요한 첫 타에는 배칭 비용이 붙지 않는다.
  - ⚠️ 이 초기화를 빼면 **첫 타 지연이 직전 스윙의 명중 여부에 따라 달라진다.** 직전 스윙이 맞았으면 누적값이 0에서 시작해 최대 0.1초 늦고, 빗나갔으면 누적값이 남아 즉시 나간다. 하필 "연속으로 잘 맞히고 있을 때"만 느려져서 체감이 나쁘다
- **`StopWeaponTrace()`가 마지막에 flush한다.** 배칭이 스윙 경계를 넘어 이월되지 않는다.

**아끼는 양은 크지 않다.** 위의 상한 때문에, 배칭이 실제로 합치는 것은 **서로 다른 프레임에 걸린 서로 다른 적들**뿐이다. 같은 프레임에 5명이 걸리면 배칭과 무관하게 어차피 RPC 하나고, 1:1 교전에서는 아끼는 것이 없다. 이득은 넓은 호로 군중을 훑을 때만 나온다 — 캐릭터 메시를 `Overlap`으로 둬서 관통 베기가 가능해진 지금 그 값어치가 조금 올라갔다.

### 트레이드오프 — 협동이라서 넣는다

배칭의 대가는 **뒤따라 맞은 대상의 피해·피격 반응이 최대 0.1초 늦게 적용된다**는 것이다(첫 타는 해당 없음). 이걸 감수할 수 있는지는 **장르가 결정한다.**

- **경쟁(PvP)이라면 넣으면 안 된다.** 맞는 쪽이 사람이면 0.1초는 회피·패링·반격 입력이 들어가는 시간이다. 같은 스윙에 맞았는데 **첫 번째로 걸린 사람은 즉시, 두 번째는 0.1초 뒤에** 판정되는 것도 곧 불공정이다 — 서버 도착 순서가 바뀌면 트레이드에서 누가 먼저 죽는지가 달라진다.
- **협동(PvE)이라면 공정성 문제가 성립하지 않는다.** 맞는 쪽이 AI고 반응도 서버가 만든다. 0.1초는 몬스터의 피격 반응이 조금 늦게 시작되는 것으로 끝나며, 누구도 손해를 보지 않는다. ChainBurst는 여기에 해당하므로 **넣는다.**

경쟁 규칙으로 방향이 바뀌면 `HitBatchInterval`을 **0으로** 두면 된다. 창이 매 틱 차면서 프레임 단위 전송이 되어 배칭이 사실상 꺼지고, 나머지 코드는 손댈 것이 없다.

## 영역 판정 — 오버랩 쿼리

무기가 닿은 대상이 아니라 **범위 안의 대상**을 때리는 공격(내려찍기·충격파)은 무기 트레이스 대신 **판정 순간에 오버랩 쿼리를 한 번** 던진다. 구현은 기능 조각 `UCBFragment_AreaAttack` → [Abilities.md](Abilities.md) "기능 조각 — 영역 공격".

| | 무기 트레이스 | 영역 판정 |
|---|---|---|
| 쿼리 | 스윕 (칼날 구간이 프레임 사이에 쓸고 간 면) | 오버랩 (한 자리에 놓은 박스, 시전자 방향으로 회전) |
| 시점 | 구간 (`CBANS_GameplayEventWindow`) | 순간 (`CBAN_SendGameplayEventToOwner`, `Event.Combat.AreaAttack`) |
| 감지 머신 | 로컬 → 서버 검증 | 서버 (현재 AI 전용) |
| 결과 | `FHitResult` | `FOverlapResult` → `FHitResult`를 직접 구성 |
| 벽 | 채널이 Block 이라 스윕이 끊김 | 끊길 경로가 없어 **라인 트레이스로 따로 검사** |
| 적용 | `Auth_ApplyDamageToTarget()` 공유 | 〃 |

- **콜리전 컴포넌트를 스폰해 `BeginOverlap`을 받는 방식은 쓰지 않는다.** 결과가 다음 물리 갱신 이후에 오고, 생성 시점에 이미 겹쳐 있던 대상은 이벤트가 오지 않을 수 있어 따로 처리해야 한다. 한 순간 터지는 공격에는 호출 즉시 결과가 나오는 쿼리가 맞다. 일정 시간 남는 장판이 생겨도 타이머로 주기마다 쿼리하는 쪽을 먼저 검토한다.
- **채널은 무기 트레이스와 같은 `Weapon`이다.** 캐릭터 메시가 이 채널에 `Overlap`이라 같은 기준(피직스 애셋 바디)으로 걸리고, 회피 무적 등으로 판정 응답을 끄면 두 판정에 함께 적용된다(위 "히트 판정 채널").
- **진영 필터도 같다** — `FGenericTeamId::GetAttitude(시전자, 대상) == Hostile`. 컴뱃 컴포넌트의 `IsHostileTarget()`과 같은 한 줄을 조각이 직접 부른다(컴포넌트에 의존하지 않기 위해).
- **벽 차단 트레이스도 `Weapon` 채널이다.** 캐릭터가 이 채널을 막지 않으므로 대상 자신이나 사이에 선 다른 적은 트레이스를 끊지 않고, 벽·지형(채널 기본 Block)만 끊는다. 무기 메시는 콜리전이 꺼져 있다(`ACBBaseWeapon`). 무기 트레이스의 벽 검사와 같은 함수(`UCBAbilitySystemLibrary::IsBlockedByWall()`)를 쓴다.
  - **시작점은 공격마다 고른다** — 판정 중심(기본, 터진 자리에서 퍼지는 공격) 또는 시전자 소켓(`WallCheckSocketName`, 입·손에서 전방으로 뿜는 공격). 전방으로 길게 뻗은 박스는 중심이 벽 너머에 있을 수 있어, 중심에서 그으면 시전자와 대상 사이의 벽을 못 본다 → [Abilities.md](Abilities.md) "벽 검사 시작점".
  - 대가: 시작점→대상 **한 줄**만 보므로, 대상 몸의 일부가 벽 밖으로 나와 있어도 캡슐 중심이 벽 뒤면 맞지 않는다. 무기 트레이스와 달리 벽 모서리에서 관대하지 않다.

## 투사체 판정 — `ACBProjectile`

날아가는 물체가 맞히는 공격. 발사(몇 발·어디서·데미지)는 기능 조각 `UCBFragment_Projectile` → [Abilities.md](Abilities.md) "기능 조각 — 투사체", **비행과 명중은 투사체 액터**가 맡는다.

```
[서버] 조각이 스폰 → Auth_Launch(조준점, 유도 대상, 데미지 스펙, 피격 큐)
         │ 발사 속도·유도 대상만 최초 1회 복제 (COND_InitialOnly)
         ▼
[전 머신] ProjectileMovement 가 각자 시뮬레이션 ─ 서버 이동 복제로 보정 (PostNetReceiveVelocity)
[서버]   매 틱: 지난 위치 → 현재 위치를 Weapon 채널로 스윕
           ├ 적대 + ASC → 데미지 스펙 적용 + 피격 큐 → 소멸   (단일 명중)
           ├ 아군·중립   → 통과
           └ 벽·지면(Block) → 소멸
```

| | 무기 트레이스 | 영역 판정 | 투사체 |
|---|---|---|---|
| 쿼리 | 스윕 (칼날 구간) | 오버랩 (박스) | **스윕 (루트 모양, 지난 위치 → 현재 위치)** |
| 시점 | 구간 노티파이 | 순간 노티파이 | 발사 노티파이 이후 매 틱 |
| 감지 머신 | 로컬 → 서버 검증 | 서버 | 서버 |
| 벽 | 스윕이 끊김 | 라인 트레이스로 따로 | 스윕이 끊김 → 소멸 |
| 적용 | `Auth_ApplyDamageToTarget()` | 〃 | **발사 때 만든 스펙을 `Auth_ApplyDamageSpecToTarget()`** |

- **채널·진영 필터는 다른 판정과 같다** — `Weapon` 채널(캐릭터는 메시 실루엣으로 Overlap, 벽·지형 Block), `GetAttitude(시전자, 대상) == Hostile`. 회피 무적처럼 판정 응답을 끄는 처리가 생기면 투사체에도 함께 적용된다.
- **판정 모양 = 루트 컴포넌트의 모양.** BP 에서 루트를 Sphere/Box/Capsule 로 교체하면 `GetCollisionShape()` 로 모양·크기·스케일을 그대로 읽어 스윕한다. 회전은 액터 회전을 쓰므로 박스처럼 방향이 있는 모양은 `bRotationFollowsVelocity`(기본 켜짐)로 진행 방향을 향하게 한다. 캡슐은 루트 기준 세로축이라 **진행 방향으로 긴 판정은 박스로** 만든다. 루트가 모양 컴포넌트가 아니면 경고 후 굵기 없는 선으로 판정한다.
- **투사체의 모든 콜리전은 런타임에 끈다**(`PostInitializeComponents`). 판정은 위 스윕 하나뿐이다.
  - 루트 콜리전이 켜져 있으면 ProjectileMovement 가 그것으로 따로 부딪혀 멈추거나 튕긴다(엔진 이동 컴포넌트는 루트를 스윕 이동시킨다).
  - 연출 메시의 기본 프리셋 `BlockAllDynamic` 은 `Weapon` 채널도 막는다 — 날아가는 투사체가 **플레이어의 무기 트레이스·스프링암 카메라·영역 공격 벽 검사를 가로막는다.** BP 마다 NoCollision 으로 바꾸는 것을 잊기 쉬워 코드가 일괄로 끈다.
- **명중 판정은 이동이 끝난 뒤에 돈다.** 액터 틱을 ProjectileMovement 틱의 후행으로 걸어(`AddTickPrerequisiteComponent`), 이번 틱에 실제로 지나간 구간을 스윕한다. 빠른 투사체가 얇은 대상을 건너뛰지 않는다.
- **시전자가 먼저 파괴되면**: 진영 판정이 중립(시전자 null)이 되고 스펙의 시전자 ASC 도 사라지므로 아무도 맞지 않는다. 시전자가 죽었지만 시체가 남아 있으면 그대로 맞는다.
- **복제**: 서버가 스폰하는 복제 액터. 클라에는 **발사 조건(속도·유도 대상)만 최초 1회** 보내고 각자 시뮬레이션한다 — 직선·포물선은 결정적이라 서버와 같은 궤적이 나온다. 유도는 타겟의 복제 위치 차이로 갈라질 수 있어 이동 복제(`SetReplicatingMovement`)로 보정한다. 엔진 `AActor::PostNetReceiveVelocity` 는 비어 있어, 받은 속도를 이동 컴포넌트에 넣도록 오버라이드했다. 소멸은 액터 파괴 복제로 전 클라에 반영된다.
- **발사 방식**(`LaunchMode`, 투사체 BP)
  - `Direct` — 조준점을 향해 `InitialSpeed`(기본 1500) 로. 중력 기본 0.
  - `Arc` — 엔진 `SuggestProjectileVelocity_CustomArc` 로 조준점에 떨어지는 속도를 푼다. `ArcParam`(0.1 높게 ~ 0.9 낮게). **이 투사체에 걸리는 실제 중력**(`ProjectileMovement->GetGravityZ()`)으로 풀어야 조준점에 떨어진다.
    - ⚠️ 엔진 함수는 중력 0 을 "월드 중력 사용"으로 해석한다. 중력 스케일 0 인 투사체를 그대로 넘기면 월드 중력 기준 속도로 쏜 뒤 중력 없이 날아가 **하늘로 사라진다.** 그래서 중력이 없거나 해가 없으면(조준점이 너무 높음) 경고 후 직선으로 쏜다.
    - `MaxSpeed` 를 걸면 계산된 속도가 잘려 짧게 떨어진다 — Arc 는 0 권장.
  - **유도** — 엔진 `bIsHomingProjectile`·`HomingAccelerationMagnitude` 를 BP 에서 켠다. 유도 대상은 발사 때 받은 타겟. 유도 가속은 속도를 계속 키우므로 `MaxSpeed` 로 상한을 거는 편이 좋다.
- **수명** `InitialLifeSpan` 기본 5초 — 빗나간 직선 투사체가 끝없이 날아가지 않게. BP 에서 조정한다.
- **미구현**: 착탄(벽·지면) 이펙트, 소멸 시 트레일이 끊기지 않게 떼어내는 처리, 관통·폭발(범위 피해). 지금은 소멸과 함께 붙은 Niagara 도 즉시 사라진다.

## 본체 무기 (맨손 몬스터) — `ACBBodyWeapon`

무기를 들지 않는 몬스터(발톱·이빨)도 **무기로 등록해서** 기존 전투 파이프라인을 그대로 쓴다. 컴뱃 컴포넌트의 모든 기능(`HasValidWeapon()`·트레이스·히트 검증·AttackPower GE)이 `EquippedWeapons` 엔트리 하나에 걸려 있기 때문에, 무기 개념을 우회하는 것보다 **트레이스 소스만 바꾼 무기**를 하나 등록하는 쪽이 변경 범위가 훨씬 작다.

```
ACBBaseWeapon
├── ACBChaserWeapon / ACBRogueWeapon / ACBOutlawWeapon   ← 무기 메시의 소켓을 트레이스 소스로 사용
└── ACBBodyWeapon                                        ← 오너 캐릭터 메시의 소켓을 트레이스 소스로 사용
```

- **이 클래스가 하는 일은 위치 getter 재정의 하나뿐이다.** `GetWeaponRootLocation()`/`GetWeaponTipLocation()`이 `WeaponMesh` 대신 오너 캐릭터의 스켈레탈 메시에서 소켓을 조회한다. 등록·공격력 GE·트레이스 루프·히트 검증은 **한 줄도 바뀌지 않는다.**
- **메시 없는 일반 무기로는 대체할 수 없다.** `WeaponMesh`는 생성자에서 만들어지는 디폴트 서브오브젝트라 스태틱 메시를 비워도 포인터가 유효하다. 그래서 베이스 getter가 `if (WeaponMesh)` 분기를 타고, 소켓이 없는 빈 메시에서 `GetSocketLocation`은 **컴포넌트 자기 위치**를 돌려준다 → root == tip == 부착 지점이 되어 판정이 사라진다.
- **오너 메시 조회 경로**: `Auth_SpawnWeapon`이 `SpawnParams.Owner`에 캐릭터를 지정하고 `AActor::Owner`는 복제되므로, 서버·클라이언트 모두 `GetOwner()` → `ACharacter::GetMesh()`로 찾을 수 있다. 매 틱 조회를 피하려고 약참조로 지연 캐싱한다.
- **소켓은 반드시 떨어진 두 지점을 쓴다.** 트레이스는 `root→tip` 선분(`Alpha` 등분) × 프레임 이동이 만드는 면을 훑으므로, 두 지점이 같으면 등분마다 **동일한 트레이스를 중복 발사**하고 판정 면이 점으로 쪼그라든다. 발톱이면 손목/발톱 끝처럼 벌려 잡을 것.
- **본 이름을 그대로 써도 된다.** 스켈레탈 메시의 `GetSocketLocation`·`DoesSocketExist`는 소켓뿐 아니라 본 이름도 받는다. 다만 본은 관절 위치라 발톱 끝 같은 지점은 소켓을 따로 심는 편이 정확하다 → [SkeletonCompatibility.md](../Presentation/SkeletonCompatibility.md)
- **소켓 이름 오타는 조용히 실패한다.** 못 찾으면 캐릭터 원점이 반환되어 판정이 엉뚱한 곳에서 난다. 그래서 오너 메시를 처음 찾은 시점에 이름 존재를 1회 검증해 경고 로그를 남긴다(매 틱 로그 방지).
- `WeaponSocketType`은 `None`(소켓 미점유)이 기본값이라 등록 시 소켓 중복 검사에서 제외된다. 데미지·공격력 GE는 일반 무기와 똑같이 `UCBWeaponData` 데이터 에셋에서 온다 — 본체 무기도 데이터 에셋이 하나 필요하다.
- 최대 등록 수는 일반 무기와 같은 `MaxWeaponCount = 2`다(양 앞발 = 2개). 한 스윙에 같은 대상이 두 부위로 이중 히트되는 것은 기존 `AlreadyHitActors` 공유가 막는다.

## 무기 오라 — 무기가 소유하는 범용 연출

무기에 붙는 지속 이펙트(오라)는 **무기가 소유하고, 켜는 쪽은 요청만 한다.** 버스트는 켜는 출처 중 하나일 뿐이다(→ [Burst.md](Burst.md) "연출").

```
어떤 GE든 (GE_Burst, 이후 버프·아이템 등) ── Gameplay Cues: GameplayCue.Weapon.Aura
  └ GCN_FX_WeaponAura (범용 큐 하나, GameplayCueNotify_Actor)
       On Become Relevant → 장착 무기마다 ActivateAura()
       On Cease Relevant  → 장착 무기마다 DeactivateAura()
```

| 요소 | 위치 | 역할 |
|---|---|---|
| `AuraComponent` | `ACBBaseWeapon` 기본 컴포넌트 (`WeaponMesh` 자식, `bAutoActivate = false`) | **무기 BP 뷰포트에서 Niagara 에셋·위치·회전·크기를 무기에 맞춰 지정**한다. 에셋을 비우면 오라 없음(AI 무기 등). 루핑 시스템이어야 켜진 동안 유지된다 |
| `ActivateAura()` / `DeactivateAura()` | `ACBBaseWeapon` (BlueprintCallable) | 켜기 요청 / 요청 하나 거두기 |
| `GetEquippedWeaponInstances()` | `UCBCombatComponent` | 큐가 캐릭터에서 무기를 찾는 경로 (`Get Component by Class`로 컴포넌트 조회) |

- **오라는 무기 BP에 미리 붙어 있는 컴포넌트다.** 모양이 정해진 오라 이펙트는 무기마다 위치·회전·크기를 맞춰야 하는데, 컴포넌트로 두면 무기 BP 뷰포트에서 **기즈모로 보면서** 맞춘다. 켜고 끄는 것은 `Activate(true)`/`Deactivate()`뿐이고, 무기가 파괴되면 함께 사라진다. 큐 쪽은 켜고 끄는 호출만 하므로 배열 관리가 없다.
  - 이전에는 에셋 프로퍼티(`AuraEffect`)를 두고 켤 때 메시 원점에 스폰했으나, 이펙트 자체의 회전·크기를 무기에 맞출 수 없어 바꿨다. 무기마다 오프셋·회전·크기 프로퍼티를 두는 안은 숫자로만 맞춰야 해서 택하지 않았다.
- **요청 수를 센다.** 범용이라 켜는 출처가 겹칠 수 있다(버스트 + 버프). 한 출처가 끝나도 다른 출처가 켜 두고 있으면 유지되고, 처음 켜질 때(0→1)만 켜고 모두 거둘 때(1→0)만 끈다. 엔진이 같은 큐를 가진 GE 여럿에 대해 이벤트를 GE마다 보내든 합쳐 보내든 둘 다 맞게 동작한다. 짝 없는 끄기는 0에서 무시한다.
- **복제하지 않는다.** 큐가 전 머신에서 불리므로 각 머신이 자기 무기의 오라를 켠다. 늦게 관련성을 얻은 클라는 새 무기 액터(요청 수 0)에 On Become Relevant가 다시 불려 따라잡는다.
- 캐릭터 스켈레톤의 무기 뼈가 아니라 **무기 메시에 붙는다.** 뼈 이름은 캐릭터마다 달라 쓰지 않는다. 무기를 칼집에 넣으면 오라도 무기를 따라간다.
- 끌 때는 `Deactivate()`로 새 파티클 생성만 멈춰 남은 파티클이 자연스럽게 사라진다. 다시 켜면 `Activate(true)`로 처음부터 재생한다.
- 오라 Niagara 에셋은 팩 원본을 고치지 말고 `/Game/ChainBurst/` 아래로 복제해서 쓴다.

**메시 표면을 샘플링하는 이펙트일 때만** (Particle Spawn의 Static Mesh Location — 파티클이 무기 모양을 따라 퍼지는 형식. 모양이 정해진 오라면 해당 없음)

| 항목 | 값 | 이유 |
|---|---|---|
| Mesh → Source Mode | **`Default`** | `Attach Parent`는 부모만 봐서 에디터 미리보기(부모 없음)에서 메시를 못 찾는다(`StaticMesh data interface has no valid mesh` 로그). `Default`는 게임에서 부착된 `WeaponMesh`를 먼저 잡으므로 동작이 같다 |
| Preview Mesh | 대표 무기 메시 하나 | 에디터 미리보기 전용, 쿠킹 시 제거된다. **Default Mesh에는 넣지 않는다** — 쿠킹에 포함되는 고정 참조가 되고 "Potentially unused Default Mesh set!" 경고가 뜬다 |
| Sampling Mode | Random Triangle | 표면에 고르게 |
| Emitter Properties → Local Space | 켬 | 휘두를 때 칼날에 붙어 따라온다 (궤적에 남기는 연출이면 끔) |

- ⚠️ **오라를 쓰는 무기 스태틱 메시마다 `Allow CPU Access`를 켠다.** 표면·정점 샘플링은 메시 데이터를 읽어 CPU 접근이 필요하다(엔진 `NiagaraDataInterfaceStaticMesh.cpp` `FunctionNeedsCpuAccess` — 바운드·소켓 함수만 예외). 에디터 스택에 "CPU access error"로 뜨고 Fix 버튼이 있지만, **Fix는 Preview Mesh 하나만 고친다.** 게임에서 붙는 나머지 무기 메시에 빠지면 **파티클이 조용히 안 나오고** 로그에 `used by CPU emitter and does not allow CPU access`만 남는다. GPU 시뮬로 바꿔도 에디터 검사는 같은 기준이라 우회가 안 된다.
- 무기 메시가 서드파티 팩 원본이면 이 플래그 변경은 **팩 원본(서브모듈) 수정**이다. 의도된 수정으로 기록해 두고, 팩 갱신 시 다시 켠다. 원본을 건드리지 않으려면 소켓(`WeaponRoot`/`WeaponTip`)만 쓰는 방식(CPU 접근 불필요, 스크래치 패드로 두 소켓 사이에 스폰)이 있으나 표면 대신 중심선을 따라 퍼진다. **샘플링하지 않는 오라로 바꿨다면 이 플래그는 필요 없다.**

**큐 BP(`GCN_FX_WeaponAura`) 규칙**
- `/Game/ChainBurst/GameplayCues`에 저장(`DefaultGame.ini`의 `GameplayCueNotifyPaths`. 밖에 두면 스캔되지 않아 **조용히 안 뜬다**). `Auto Destroy On Remove` 켬.
- **BP에서는 이름이 다르다.** C++ 이름에 표시 이름이 따로 붙어 있고, `bool`을 반환하는 `BlueprintNativeEvent`라 이벤트 그래프의 이벤트가 아니라 **My Blueprint → Functions → Override**에 있다(엔진 `GameplayCueNotify_Actor.h`). 반환값은 엔진이 읽지 않는다(기본 구현과 같이 `false`).

  | C++ | BP 표시 이름 | 불리는 때 |
  |---|---|---|
  | `OnActive` | On Burst | GE가 실제로 적용된 순간 1회 |
  | `WhileActive` | **On Become Relevant** | 적용 순간 + 활성 중에 관련성을 얻은 클라에서 |
  | `OnRemove` | **On Cease Relevant** | GE 제거 |

- 오버라이드한 두 함수의 시작에 **부모 함수 호출**을 둔다. 엔진 기본 구현이 큐 액터의 숨김 상태를 관리한다(켤 때 보이기 / 끌 때 숨기기 — 큐 액터 재활용용).
- ⚠️ **On Burst(`OnActive`)에서 `ActivateAura`를 부르지 않는다.** 적용 순간 On Burst와 On Become Relevant가 둘 다 불려 요청 수가 2가 되고, 끄기는 한 번만 오므로 **오라가 영원히 꺼지지 않는다.**
- `My Target`이 캐릭터(ASC 아바타)다(`UAbilitySystemComponent::InvokeGameplayCueEvent`). `Parameters`의 `Instigator`는 플레이어면 PlayerState라 쓰지 않는다.

```
On Become Relevant (My Target) → 부모 호출 → My Target → Get Component by Class(CBCombatComponent)
    → Get Equipped Weapon Instances → For Each → Activate Aura
On Cease Relevant (My Target)  → 부모 호출 → (같은 경로) → For Each → Deactivate Aura
```

**검토했지만 채택하지 않은 것**

| 안 | 이유 |
|---|---|
| `GameplayCueNotify_Looping` | 그래프 없이 이펙트를 지정하고 정리도 자동이지만, 스폰 대상이 큐 파라미터의 `TargetAttachComponent`(GE로 켜지는 큐는 비어 있음) → **캐릭터 메시**로 고정된다(`GameplayCueNotifyTypes.cpp`). 무기 뼈에 붙여야 하는데 **캐릭터마다 무기 뼈 이름이 달라** 캐릭터별 큐가 필요해진다. 또 오라 Niagara가 **자체적으로 루프**하므로 큐는 시작에 켜고 끝에 끄기만 하면 되어, Looping의 반복·단계별 이펙트 기능이 필요 없다 |
| 큐 BP가 직접 스폰·배열 관리 | 컴포넌트가 무기에 붙어 큐 액터와 수명이 어긋나고, 출처가 겹치면 이펙트도 겹친다 |
| 버스트 어빌리티가 큐를 추가·제거 | 어빌리티 큐는 어빌리티 종료(=발동 몽타주 종료) 때 제거되고, ASC에 직접 추가하면 만료·사망·서버 거부 제거를 다시 짜야 한다. GE가 수명을 대신 관리한다 |
| 데이터 에셋(`UCBWeaponData`)에 이펙트 | 큐는 전 클라에서 도는데 클라에는 무기 → 데이터 에셋 경로가 없다. 만들려면 컴뱃 컴포넌트의 복제 구조체를 바꿔야 해서, 오라를 모르는 컴포넌트를 건드리지 않는 쪽을 택했다 |

## 관련 문서
- 콤보 인덱스를 소비하는 몽타주 재생 흐름: [Montage.md](Montage.md)
- 콤보 전진/리셋을 호출하는 어빌리티: [Abilities.md](Abilities.md)
- 로컬 감지 → 서버 검증 패턴: [Multiplayer.md](../Conventions/Multiplayer.md)
- 진영 값·attitude solver·팀 할당 위치: [Teams.md](../Foundation/Teams.md)
