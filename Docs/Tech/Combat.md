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
- 무기 트레이스: `StartWeaponTrace()` → `TickWeaponTrace()` → `StopWeaponTrace()`
  - 트레이스는 애님노티파이 `CBANS_WeaponTraceWindow`가 구간을 제어
  - **무기별 root/tip을 순회 트레이스**(쌍수는 양손 블레이드 모두). `AlreadyHitActors`는 무기 간 **공유**하여 한 스윙에 같은 대상이 두 블레이드로 이중 히트되는 것을 방지
  - 히트 배칭(0.1초 간격)으로 중복 히트 방지
  - **진영 필터**: 트레이스에 걸린 액터는 적대(`Hostile`)일 때만 배칭 목록에 담긴다(`IsHostileTarget()`). 아군·중립은 히트에서 제외 → 서버 RPC도 나가지 않는다
    - `AlreadyHitActors`에는 **적대 여부와 무관하게** 기록한다. 같은 대상을 트레이스 등분마다 다시 판정하지 않기 위함
- 히트 검증: 로컬에서 감지 → `Server_NotifyAttackHit()` RPC → 서버에서 **진영 → 거리** 순으로 재검증. 쌍수는 무기 중 **가장 먼 tip 거리**를 허용 거리 기준으로 사용 (`HitValidationTolerance`)
  - **클라 필터만으로는 부족하다.** 트레이스는 로컬(`IsLocallyControlled`)에서 돌고 결과만 RPC로 올라오므로, 조작된 클라가 아군 타겟을 실어 보낼 수 있다. 서버가 같은 기준으로 한 번 더 판정한다
- **진영 판정 기준은 한 곳**: `FGenericTeamId::GetAttitude(공격자, 대상) == Hostile`. `ACBAIController::IsValidTarget()`·퍼셉션 소속 필터와 같은 전역 attitude solver를 탄다 → [Teams.md](Teams.md)
  - ⚠️ **중립은 때릴 수 없다.** solver는 한쪽이라도 Neutral이면 Neutral을 반환한다. `ACBBaseCharacter`의 팀 기본값이 Neutral이므로, **팀 지정이 빠진 캐릭터는 조용히 무적이 된다**
  - ⚠️ **`IGenericTeamAgentInterface`를 구현하지 않은 액터도 걸러진다**(엔진 구현상 Neutral). 지금은 트레이스 채널이 `Pawn`이라 무해하지만, 파괴 가능한 오브젝트를 무기로 때리려면 그때 별도 경로가 필요하다
- **데미지 적용은 히트마다 타겟 하나씩.** 히트가 배칭되어 한 이벤트에 여러 피격자가 실려 오므로(`FGameplayAbilityTargetDataHandle`), 어빌리티(`UCBChaserAttackAbility`/`UCBAIAttackAbility`의 `OnAttackHit`)는 **그 회차의 HitResult 하나만 담은 핸들**을 만들어 적용한다.
  - ⚠️ **핸들 전체를 넘기면 안 된다.** `ApplyGameplayEffectSpecToTarget`은 넘긴 핸들의 *모든* 타겟에 적용하므로, 타겟 수만큼 도는 루프에서 전체 핸들을 넘기면 N² 번 적용되어 **각자 N 배 데미지**를 받는다(한 명만 때리면 1×1이라 증상이 안 보인다).
  - **타겟은 오직 `TargetData`가 정한다.** 앞의 `CurrentSpecHandle`/`CurrentActorInfo`/`CurrentActivationInfo`는 `HasAuthorityOrPredictionKey()` 게이트(권한·예측키 검사)에만 쓰이며 적용 대상과 무관하다. 실제 대상은 `FGameplayAbilityTargetData_SingleTargetHit::GetActors()` = HitResult의 액터이고, 소스는 스펙 컨텍스트의 시전자 ASC다.
  - **HitResult는 스펙을 만든 뒤 컨텍스트에 붙인다** (`SpecHandle.Data->GetContext().AddHitResult()`). `MakeOutgoingGameplayEffectSpec()`이 컨텍스트를 자체 생성하므로, 미리 만든 `FGameplayEffectContextHandle`을 넘길 자리가 없다 — 따로 만들어두면 조용히 버려진다.
- 무기 등록 시 `Data.Weapon.AttackPower` GE를 ASC에 적용, 컴포넌트 `EndPlay`에서 회수. **무기별로 각자의 `WeaponData` 공격력 GE를 적용**(핸들도 무기별로 보관 후 회수 시 전부 제거) — 쌍수는 두 무기 데미지 값을 각각 설정
- 전투 상태(태그) 소유: `SetCombatMode(bool)`이 유일한 진입점. 내부에서 `IsCombatMode()`(= `Status.Combat.InCombat` 태그 검사)로 idempotent 가드 후 `OnEnterCombatMode()`/`OnExitCombatMode()` 호출 — 이 두 함수가 무기 부착(Hand/Sheath)과 상태 태그(서버·예측 클라가 동일하게 `TagOnly` 복제 상태로 1회 추가/제거 — 로컬 `None` 추가와 서버 복제 추가를 나누면 예측 경고·카운트 불일치 발생) 처리 부수효과를 함께 소유. 어빌리티(`UCBGAChaserEquipWeapon`/`UnequipWeapon`)는 `SetCombatMode()`만 호출하고 태그를 직접 건드리지 않는다 — 무기 부착 상태와 태그가 다른 곳에서 따로 갱신되어 어긋나는 것을 방지.
  - **전투 상태 태그도 `EndPlay`에서 정리한다.** 루스 태그는 GE와 달리 **넣은 쪽이 빼야만** 사라지는데, 플레이어 ASC는 PlayerState 소유라 폰보다 오래 산다. 정리하지 않으면 사망 리스폰·무기 변경으로 새로 스폰된 폰이 **무기를 칼집에 둔 채 전투 상태로 시작**하고, 장착 어빌리티는 `SetCombatMode(true)`의 idempotent 가드에 걸려 조기 반환하므로 **무기가 영영 손에 붙지 않는다.** 그런데도 공격 어빌리티의 `ActivationRequiredTags(Status.Combat.InCombat)`는 통과한다.
  - 정리 주체는 **직접 붙인 인스턴스뿐**이다(`bCombatTagApplied` 플래그). 시뮬레이티드 프록시는 `TagOnly` 복제로 태그를 받았을 뿐이라 제거 주체가 아니다. `UCBLocomotionProcessor`가 미러링 태그(`Idle`/`InAir`/`Run`)를 같은 방식(적용 플래그 + `EndPlay` 정리)으로 처리한다 (→ [Locomotion.md](Locomotion.md))
- 콤보 상태 보관: `AdvanceCombo(Tag, MaxCount)`(재생 인덱스 반환 + 내부 전진) / `ResetCombo()`. 컴포넌트는 **상태만 소유**하고 타이머도 판단도 갖지 않는다.
  - **호출 주체는 `UCBChaserAttackAbility` 하나다.** 전진은 `SelectActionMontageIndex()`에서, 리셋은 `CleanupActionState()`(정상 종료·폴백·캔슬 세 경로)에서 호출한다. 콤보 여부를 정하는 `IsCombo`도 이 어빌리티의 프로퍼티다 — **액션 어빌리티 베이스(`UCBActionAbility`)는 콤보를 알지 못한다.**
  - 콤보 인덱스는 **비복제**다. 서버·소유 클라가 각자 예측 전진하고, sim proxy는 자기 값을 쓰지 않고 큐 파라미터로 받은 서버 인덱스로 재생한다.
  - **복제하지 않는 이유**: 예측하는 값에 복제를 얹으면 지연되어 도착한 서버 값이 이미 앞서 나간 클라 값을 덮어써 **정상 연타에서도 콤보가 뒤로 밀린다**(GAS가 어트리뷰트를 절대값이 아니라 델타로 예측하는 것과 같은 Override 문제).
  - **거부 시 롤백이 예측의 짝이다.** 서버가 활성화를 거부하면 클라만 인덱스가 앞선 채 남는다 — 거부는 `bWasCancelled = false`로 종료되므로 `CleanupActionState()` 경로에도 걸리지 않는다. 그대로 두면 **체인이 정상 종료될 때까지 소유 클라와 나머지 화면이 서로 다른 콤보 단계를 재생**하고, 클립 길이가 달라 루트 모션 이동량까지 어긋난다.
    - 거부 감지는 **예측 키의 거부 델리게이트**(`FPredictionKey::NewRejectedDelegate()`)로 한다. `UCBChaserAttackAbility::SelectActionMontageIndex()`가 `AdvanceCombo()` 직후 등록하고, 콜백이 `RollbackCombo(키)`를 부른다. 소유 클라에서만 등록한다(서버는 발화하지 않고 전역 맵에 항목만 남는다).
    - ⚠️ **`EndAbility`에서 `ActivationMode == Rejected`를 보는 방식은 쓰면 안 된다.** 어빌리티가 `InstancedPerActor`(`UCBGameplayAbility` 생성자)라 콤보 다음 타가 **같은 인스턴스를 재사용하며 `ActivationInfo`를 덮어쓴다**. 엔진의 거부 처리는 인스턴스의 *현재* 키와 대조하므로(`AbilitySystemComponent_Abilities.cpp:2324`), 콤보 전환이 거부 통보보다 먼저 일어나면 `EndAbility` 자체가 호출되지 않는다. 반면 `FPredictionKeyDelegates`는 키 값으로만 관리되는 전역 맵이라 인스턴스 생사와 무관하게 발화하고, 거부 처리의 **첫 줄**에서 브로드캐스트된다.
    - **방향은 언제나 클라가 서버를 따라간다.** 서버가 거부한 활성화는 게임적으로 없던 일이므로, 되돌리는 쪽이 정확하다. 반대로 서버가 클라를 따라가면 검증이 무의미해지고, 이미 서버 인덱스로 재생을 마친 프록시와 또 어긋난다.
    - **`0`으로 리셋하면 안 된다.** 서버는 전진 직전 값에 머물러 있으므로 방향만 바뀐 채 계속 어긋난다. `AdvanceCombo()`가 **함수 맨 앞에서** 전진 직전 상태를 스냅샷해두고(내부 리셋/순환 분기보다 먼저) `RollbackCombo()`가 그 값으로 복원한다.
    - 스냅샷은 한 단계 깊이뿐이라, 여러 활성화가 동시에 응답을 기다리면 낡은 값이 될 수 있다. 그래서 **스냅샷과 거부 통보의 예측 키를 대조**해 일치할 때만 되돌린다(불일치 시 건너뛰고 체인 종료에서 수렴). 몽타주 길이가 왕복 지연보다 훨씬 길어 실제로 겹치는 일은 드물다.
    - 새 RPC도 새 복제 속성도 필요 없다 — 서버가 이미 보내는 `ClientActivateAbilityFailed`를 쓴다. **서버 코드는 변하지 않는다.**

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
- **본 이름을 그대로 써도 된다.** 스켈레탈 메시의 `GetSocketLocation`·`DoesSocketExist`는 소켓뿐 아니라 본 이름도 받는다. 다만 본은 관절 위치라 발톱 끝 같은 지점은 소켓을 따로 심는 편이 정확하다 → [SkeletonCompatibility.md](SkeletonCompatibility.md)
- **소켓 이름 오타는 조용히 실패한다.** 못 찾으면 캐릭터 원점이 반환되어 판정이 엉뚱한 곳에서 난다. 그래서 오너 메시를 처음 찾은 시점에 이름 존재를 1회 검증해 경고 로그를 남긴다(매 틱 로그 방지).
- `WeaponSocketType`은 `None`(소켓 미점유)이 기본값이라 등록 시 소켓 중복 검사에서 제외된다. 데미지·공격력 GE는 일반 무기와 똑같이 `UCBWeaponData` 데이터 에셋에서 온다 — 본체 무기도 데이터 에셋이 하나 필요하다.
- 최대 등록 수는 일반 무기와 같은 `MaxWeaponCount = 2`다(양 앞발 = 2개). 한 스윙에 같은 대상이 두 부위로 이중 히트되는 것은 기존 `AlreadyHitActors` 공유가 막는다.

## 관련 문서
- 콤보 인덱스를 소비하는 몽타주 재생 흐름: [Montage.md](Montage.md)
- 콤보 전진/리셋을 호출하는 어빌리티: [Abilities.md](Abilities.md)
- 로컬 감지 → 서버 검증 패턴: [Multiplayer.md](Multiplayer.md)
- 진영 값·attitude solver·팀 할당 위치: [Teams.md](Teams.md)
