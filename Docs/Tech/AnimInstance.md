# 애님 인스턴스 계층 구조

> 애님 인스턴스를 새로 만들거나 로직을 어느 계층에 둘지 정할 때 읽는 문서. **애니메이션은 상태 머신으로 구현**하며(모션 매칭 미사용), 애님 인스턴스는 상태 전이·블렌드에 쓸 데이터를 제공한다.

## 계층도

```
UAnimInstance
└── UCBBaseAnimInstance ← 캐릭터 무관 최소 베이스 (공통 유틸용)
    │
    └── UCBCharacterAnimInstance ← 폰이 소유하는 애님 인스턴스의 공통 베이스
        │   • Player/AI가 모두 공유하는 로직만 담는다
        │   • 참조 캐싱 / 시스템 준비 초기화 / 로코모션 데이터 / 개이트 / 전투 모드 / 몽타주 재생
        │
        ├── UCBPlayerAnimInstance (Abstract 성격) ← 플레이어 조작 캐릭터 베이스
        │   │   • 컨트롤 회전/카메라·로컬 입력 종속 로직 (에임오프셋, 스트레이핑 lean 등)
        │   │
        │   └── UCBChaserAnimInstance (Chaser 전용)
        │
        └── UCBAIAnimInstance (Abstract 성격) ← AI 조작 캐릭터 베이스
            │   • 포커스/타겟 방향·AI 상태(순찰/전투/경계) 종속 로직
            │
            └── UCBOutlawAnimInstance (Outlaw 전용, 보스/페이즈)
                └─ (Rogue는 고유 코드가 생기기 전까지 UCBAIAnimInstance를 ABP 부모로 직접 사용 가능)
```

> **링크드 애님 레이어는 사용하지 않는다.** 스켈레톤(=스켈레탈 메시)당 애니메이션이 고정이고 전투/비전투 상태머신 2개만 존재하므로, 갈아끼울 변종이 없어 레이어의 모듈성이 무의미하다. 전투↔비전투 전환은 애님그래프 분기로 처리한다(아래 ④ 축 참조).

## 로직을 어느 계층에 둘지

분기 기준은 **진영(Chaser/Outlaw)이 아니라 "애니메이션 입력을 무엇이 구동하는가"** 다.

| 계층 | 담는 것 | 담지 않는 것 |
|---|---|---|
| **Base** | 캐릭터 무관 공통 유틸(오너 캐스팅 등) | 폰 종속 로직 |
| **Character** | 모든 캐릭터 공통 — 참조 캐싱, 시스템 준비 초기화, 로코모션 데이터(속도/가속), 개이트, 전투 모드, 몽타주 재생 | 시점/입력 종속, AI 상태 종속 |
| **Player** | 컨트롤 회전/카메라 기반 에임오프셋, 스트레이핑 lean, 로컬 입력에만 의미 있는 데이터 | AI 상태 |
| **AI** | 포커스/타겟 방향 기반 에임, AI 상태(순찰/전투/경계), PathFollowing 전제 처리 — 현재 **스트레이프 상태**(`Status.Movement.Strafe` 구독 → `IsStrafing()`) 보유 | 카메라/입력 |
| **Chaser/Outlaw/Rogue** | 캐릭터별 고유 데이터. ABP의 C++ 부모 훅 | — |

**스트레이프 상태를 AI 계층에 둔 이유:** 태그를 거는 주체가 BT 서비스(`UCBBTService_StrafeFocus`)뿐이고 플레이어에는 아직 경계 모드가 없다. 락온 등으로 Chaser 도 주시 이동을 하게 되면 그때 Character 계층으로 올린다 — 구독·해제 코드가 그대로 이동하면 된다.

**과설계 방지:** 빈 pass-through 계층은 냄새다. Player/AI 중간 계층은 **실제 공유 멤버가 생길 때** 채운다(현재는 골격만 존재). 잡몹처럼 단순하면 별도 구체 클래스 없이 상위 베이스를 ABP 부모로 직접 써도 된다.

## UCBCharacterAnimInstance 내부 구성 (`#pragma region`)

| region | 내용 |
|---|---|
| **References** | `CachedCharacter`, `CachedCMC` + 지연 캐싱 getter(`GetCachedCharacter`/`GetCachedCMC`) |
| **Lifecycle** | `OnCharacterSystemReady`(캐릭터 시스템 준비 델리게이트 바인딩) → `InitAnimData`(ASC 준비 후 전투 태그 이벤트 등록) → `NativeUninitializeAnimation`(ASC 태그 이벤트 구독 해제) |
| **Locomotion** | 데이터 계산 + 상태 쿼리(`IsMoving`/`IsStopping`, `GetMoveX`/`GetMoveY`), 개이트(`ECBLocomotionGait`) |
| **Combat** | `bIsCombatMode` — `Status.Combat.InCombat` 태그 변경(`OnCombatTagChanged`)으로 갱신 |
| **Montage** | `PlayMontage` — `UCBActionComponent`가 호출(공격 속도 배속을 블렌드 인에 반영) |

### 폰보다 오래 사는 발행자의 델리게이트는 반드시 해제한다

애님 인스턴스는 폰과 함께 사라지지만 **플레이어 ASC는 PlayerState 소유라 폰 재스폰을 넘어 살아남는다.** 그런데 이 프로젝트는 **무기 변경 = 캐릭터 변경 = 폰 재스폰**이다([GameFlow.md](GameFlow.md)).

해제하지 않으면 무기를 바꿀 때마다 파괴된 캐릭터의 애님 인스턴스가 ASC에 구독자로 남아 **GC 전까지 계속 콜백을 받는다**(로그에 `Actor=None`인 콜백이 찍히는 것으로 발견). 콜백이 불값 하나만 세우는 동안은 증상이 없지만, 부수효과가 생기는 순간 버그가 된다.

- 등록: `InitAnimData()`에서 `RegisterGameplayTagEvent(...).AddUObject(this, ...)`
- 해제: `NativeUninitializeAnimation()`에서 `RegisterGameplayTagEvent(...).RemoveAll(this)` — `RegisterGameplayTagEvent`는 내부적으로 `FindOrAdd`라 등록 때와 같은 델리게이트를 돌려준다
- 해제 시점에 `CachedCharacter`로 ASC를 찾아가도 된다. `UWorld::DestroyActor`가 **`UnregisterAllComponents()`(→ 애님 인스턴스 정리)를 `MarkAsGarbage()`보다 먼저** 호출하므로 캐릭터가 아직 유효하고, `GetCBAbilitySystemComponent()`는 PlayerState를 다시 조회하지 않고 캐릭터가 캐싱해둔 포인터를 반환한다(언포제스 여부와 무관)

반대로 발행자가 애님 인스턴스와 같이 죽는 경우(캐릭터의 `OnCharacterSystemReadyDelegate`)는 해제가 없어도 문제되지 않지만, 일관성을 위해 `UCBBaseAnimInstance`도 해제한다. **판단 기준은 "발행자가 구독자보다 오래 사는가"다.**

### 태그를 구독하면 그 자리에서 현재 값을 1회 읽는다

`RegisterGameplayTagEvent`는 **구독 이후의 변화만** 발화한다. 구독 시점에 이미 붙어 있던 태그는 아무 콜백도 내지 않으므로, 값을 이벤트로만 채우면 **늦게 붙은 애님 인스턴스가 항상 기본값(false)으로 시작한다.**

이게 실제로 갈리는 두 경우:

- **폰 교체** — 리스폰·무기 변경으로 새 폰이 스폰되는데 ASC(PlayerState 소유)에는 `Status.Combat.InCombat`이 그대로 남아 있으면, 새 애님은 비전투 모션으로 시작해 ASC 상태와 어긋난다
- **뒤늦은 관련성** — 이미 스트레이프 중인 AI에 나중에 관련성을 얻은 클라이언트는 `Status.Movement.Strafe` 변경을 놓쳐 주시하지 않는 모션으로 재생한다

그래서 `UCBCharacterAnimInstance::InitAnimData`(`bIsCombatMode`)와 `UCBAIAnimInstance::OnCharacterSystemReady`(`bIsStrafing`) 모두 **등록 직후 `HasMatchingGameplayTag()`로 현재 상태를 한 번 반영**한다. `ACBBaseCharacter::BindDeathStateEvent`가 같은 이유로 쓰는 패턴이다(→ [Abilities.md](Abilities.md)).

## Character가 상태 머신에 제공하는 데이터 (4축)

폰 공통 계층이 상태 머신·블렌드 스페이스에 넘기는 값은 **4가지 축**으로 정리된다. Player/AI가 갈리는 에임오프셋 등은 여기 포함되지 않는다(하위 계층 소관).

| 축 | 값 | 갱신 위치 | 용도 |
|---|---|---|---|
| **① 이동 방향** | `MoveX`(좌우), `MoveY`(앞뒤) | ThreadSafeUpdate | 방향 블렌드 스페이스 좌표 |
| **② 이동 상태** | `IsMoving()` / `IsStopping()` / `IsStarting()`, `bHasAcceleration`, `CurrentVelocitySize`, `GetSpeedRatio()` | Update + ThreadSafe | Idle↔Move↔Stop 전이, Move 내 출발(Start) 하위 구간 |
| **③ 보행 등급** | `CurrentLocomotionGait` (Walk/Run/Sprint), `LockedLocomotionGait` (Start/Stop용 스냅샷) | Update — **ASC 태그**(`Status.Movement.Gait.*`) / 스테이트 진입 시 `LockCurrentGait` | 속도 단계 선택 |
| **④ 전투 모드** | `bIsCombatMode` | 이벤트 — `Status.Combat.InCombat` 콜백 | 전투/비전투 포즈 전환 |
| **⑤ 공중 상태** | `IsInAir()` (CMC `IsFalling` 캐싱), `GetVerticalVelocity()` (＋상승/－하강) | Update | 지상↔공중 전이, 점프↔낙하 블렌드, 착지 판별 |

> ②·③·⑤축을 소비하는 **상태 머신 배선(지상/공중 전이 표), 출발/정지 판정식, 개이트 스냅샷, 피벗, 관성화 정책** 등 로코모션 기능 상세는 [Locomotion.md](Locomotion.md)로 분리했다. 이 문서는 데이터 계산·계층·스레딩까지만 다룬다.

**전투/비전투 전환 구현 (④):** 링크드 레이어가 아니라 **애님그래프 분기**로 처리한다.
- 전투용·비전투용 상태머신을 각각 별도로 두고 캐시(캐시드 포즈)한다.
- `Blend Poses by bool(bIsCombatMode)`로 두 캐시를 합친다 — 가중치 0 브랜치는 평가가 스킵되고, 캐시드 포즈는 소비될 때만 평가되므로 정착 시 한쪽만 돈다.
- 전환 블렌드는 최종 출력 직전 **Inertialization** 노드에 맡긴다(상태머신 개별 전이가 아니라 이 bool 전환에 블렌드를 집중해 발 미끄러짐/팝핑 방지).
- 두 상태머신은 같은 ABP 안에서 ①~③ 축(MoveX/Y·이동 상태·개이트)을 그대로 공유한다 — 애니메이션 에셋만 다르고 구동 데이터는 동일.

**MoveX/MoveY 계산 (①):**
1. 월드 속도 → `CachedActorRotation.UnrotateVector()`로 **캐릭터 로컬 속도** 변환 (로컬 X=앞뒤, Y=좌우).
2. **정규화** — 크기를 버리고 **방향만** 남긴다.
3. **축 스왑** — 블렌드 스페이스 규약(X=좌우, Y=앞뒤)에 맞춰 `MoveX ← 로컬Y`, `MoveY ← 로컬X`.
4. `FInterpTo`(속도 10)로 **스무딩** — 방향 급변 시 샘플 지점 팝핑 방지.

- 정규화하므로 **속도 크기는 담기지 않는다** → 걷기/질주 구분은 ②·③이 담당.
- 이름의 `X`/`Y`는 로컬 축이 아니라 **블렌드 스페이스 축**을 가리킨다(스왑 주의). 블렌드 스페이스 축 규약이 바뀌면 이 스왑도 함께 수정.

## 스레딩 (게임 스레드 / 워커 스레드)

- **`NativeUpdateAnimation` (게임 스레드, UObject 접근 가능)**: `UpdateBasicMovementData`(캐릭터에서 속도/가속/회전을 읽어 `Cached*`에 저장) + `UpdateCombatAndAbilityData`(ASC 태그로 개이트 결정 + `CachedGaitMaxSpeed` 캐싱).
- **`NativeThreadSafeUpdateAnimation` (워커 스레드, 매우 빠름, UObject 접근 불가)**: `Cached*` 복사본으로 크기 계산·`bHasAcceleration` 판정·로컬 속도 → `MoveX`/`MoveY` 보간·`CurrentSpeedRatio` 계산.
- 상태 쿼리 함수는 `meta = (BlueprintThreadSafe)`로 워커 스레드/상태 머신에서 안전하게 호출.

## 상태 머신 전제 (모션 매칭 미사용)

모션 매칭(PoseSearch/MotionTrajectory)은 사용하지 않는다. 관련 코드는 모두 제거됨:
- 트래젝토리 캐싱, 미래 속도 예측(`GetTrajectoryVelocity`), 예측 기반 `IsStarting`/`IsPivoting`, 개이트 속도 임계값
- `UCBCharacterTrajectoryComponent`(삭제), `MotionTrajectory`/`PoseSearch` 모듈·플러그인 의존성 제거

상태 전이 판단에는 남은 데이터(`IsMoving`/`IsStopping`/`IsStarting`, `GetSpeedRatio`, `MoveX`/`MoveY`, 개이트, 전투 모드, 공중 상태)를 사용한다 — 판정식·전이 표는 [Locomotion.md](Locomotion.md).

## 문서 유지 규칙

- 계층도에는 **베이스/추상 성격의 클래스**를 중심으로 나열하고, 구체 클래스는 대표 예시만 둔다.
- 애님 인스턴스 클래스를 추가·이동·삭제하거나 로직을 다른 계층으로 옮기면 **같은 작업에서 이 문서도 갱신**한다.

## 관련 문서
- **로코모션 기능 상세 (상태 머신 배선·출발/정지/피벗/대시/점프·관성화)**: [Locomotion.md](Locomotion.md)
- 캐릭터 시스템 준비까지 초기화를 미루는 패턴(Lifecycle region의 `OnCharacterSystemReady`): [SystemReady.md](SystemReady.md)
- 캐릭터가 소유하는 컴포넌트: [Components.md](Components.md)
- 몽타주 재생 흐름(`PlayMontage` 호출 경로): [Montage.md](Montage.md)
- 서버 권위·Simulated Proxy 반영: [Multiplayer.md](Multiplayer.md)
