# AI 컨트롤러 구조

> AI 캐릭터의 두뇌(결정 계층)를 담는 AIController 계층과 그 시작 타이밍.

## 계층

```
AAIController (엔진)
└── ACBAIController (Abstract)      ← 공통: 두뇌 시작 게이트
    ├── ACBOutlawController         ← 보스/엘리트: 복잡한 두뇌
    └── ACBRogueController          ← 일반 잡몹: 단순한 두뇌
```

캐릭터 계층(`ACBAICharacter → Outlaw/Rogue`)과 대칭. 각 AI 캐릭터가 생성자에서 `AIControllerClass`로 자기 컨트롤러를 지정하고, 자동 빙의(`AutoPossessAI = PlacedInWorldOrSpawned`)는 `ACBAICharacter` 베이스가 설정한다.

## AI 이동의 책임 분리

BehaviorTree/StateTree는 **결정만** 한다. 실제 이동은 아래 스택이 처리:

| 계층 | 역할 |
|---|---|
| BT / StateTree | "언제 어디로" 결정 → `MoveTo` 목표 |
| AIController | `MoveTo` → `UCrowdFollowingComponent`로 경로 추종 (+ 군중 회피) |
| NavMesh | 목표까지 경로 계산 (레벨에 NavMeshBoundsVolume 필요) |
| CharacterMovementComponent | 캡슐을 실제로 이동·복제 |

즉 두뇌는 목표만 던지고, 좌표 이동·중력·회전은 CMC가 처리한다. 이동 로직을 직접 코딩하지 않는다.

### 함정 — 경로 추종은 기본적으로 가속을 쓰지 않는다

`PathFollowingComponent`는 `bUseAccelerationForPaths` 값에 따라 **두 가지 경로**로 CMC를 구동한다 (엔진 `UPathFollowingComponent::FollowPathSegment`):

| 값 | 호출 | 결과 |
|---|---|---|
| `false` (**엔진 기본값**) | `RequestDirectMove(MoveVelocity)` | `RequestedVelocity`만 세팅 → **CMC의 `Acceleration` 멤버는 0으로 남음**, 목적지에서 감속 없이 급정지 |
| `true` | `RequestPathMove(MoveInput)` | 입력 벡터 경유 → `Acceleration` 멤버 채워짐 + `CachedBrakingDistance` 기반 점진 감속 |

기본값(`false`)이면 **애님 인스턴스의 `bHasAcceleration`이 항상 false**가 되고(애님이 읽는 `GetCurrentAcceleration()`은 `Acceleration` 멤버이며, 경로 추종이 쓰는 내부 `RequestedAcceleration`과는 별개), 정지 애님 판정도 무너진다.

→ 그래서 `ACBAICharacter` 생성자에서 **`NavMovementProperties.bUseAccelerationForPaths = true`** 로 켠다. 이렇게 해야 `UCBLocomotionProcessor`가 세팅하는 `MaxAcceleration`·`BrakingDecelerationWalking`이 AI에도 의미를 갖고, 출발/정지 애님 판정(`StartSpeedRatioThreshold`/`StopSpeedRatioThreshold`)이 플레이어와 동일한 전제로 동작한다. → [Locomotion.md](Locomotion.md), [AnimInstance.md](AnimInstance.md)

(UE 5.5부터 `UNavMovementComponent::bUseAccelerationForPaths`는 deprecated이고 `NavMovementProperties` 구조체로 옮겨졌다)

### 군중 회피 — Detour Crowd

여러 AI 가 같은 타겟으로 몰릴 때 서로 밀고 캡슐이 겹치는 것을 막는다. `ACBAIController` 생성자가 경로 추종 컴포넌트를 `UCrowdFollowingComponent` 로 교체한다 — 엔진 `ADetourCrowdAIController` 와 같은 방식이며, 베이스에서 하므로 Rogue·Outlaw 가 함께 받는다.

```cpp
ACBAIController::ACBAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
```

- **CMC 의 `bUseRVOAvoidance` 를 쓰지 않는 이유**: AI 이동이 전부 NavMesh 경로 추종이라, 속도만 밀어내는 RVO 는 벽 모서리에서 경로 밖으로 밀린다. Crowd 는 경로 단계에서 함께 푼다. **둘을 같이 켜지 말 것** — 서로 다른 해를 매 틱 덮어써 흔들린다.
- **위 가속 함정과 충돌하지 않는다.** `UCrowdFollowingComponent::ApplyCrowdAgentVelocity()` 가 `UseAccelerationForPathFollowing()` 을 보고 분기해 `RequestPathMove()` 를 부르므로, `bUseAccelerationForPaths = true` 가 그대로 살아 있고 출발/정지 애님 판정도 유지된다.
- 파라미터는 `ApplyCrowdAvoidanceSettings()` 가 `OnPossess` 에서 1회 적용. 분리(Separation)는 엔진 기본이 꺼짐이라 켜 주고, 회피 품질은 기본 `Low` 로는 겹침이 남아 `Medium` 으로 올린다.
  - `ECrowdAvoidanceQuality` 는 `UENUM` 이 아니라 **`UPROPERTY` 로 노출할 수 없어** 코드에 둔다. 나머지(분리 on/off·강도·탐색 반경)는 `EditDefaultsOnly`.
- **회피는 분산이 아니다.** "서로 겹치지 않게" 할 뿐, "타겟 주위로 골고루 퍼지게" 하지는 않는다. 전원이 같은 목적지(`MoveTo(TargetActor)`)로 수렴하는 구조는 그대로라 **타겟 앞에 뭉친 채 서로 비켜서는** 그림이 된다. 분산은 목적지를 다르게 주는 문제이고 EQS 의 몫이다 — 경계 모드는 `Random Best 25%` 로 이미 흩어지므로, 남은 것은 공격 브랜치의 접근 지점이다.

## 두뇌 시작 게이트 (핵심)

AI 두뇌는 로드아웃 비동기 로드가 끝나(어빌리티·이동 데이터 준비) **준비 완료(SystemReady) 후**에 시작해야 한다. → [SystemReady.md](SystemReady.md)

`ACBAIController::OnPossess()`가 이를 게이트한다:
- 빙의한 폰을 `ACBAICharacter`로 캐싱.
- 이미 `IsCharacterSystemReady()`면 `StartAILogic()` 즉시 호출.
- 아직이면 `OnCharacterSystemReadyDelegate`에 바인딩해 대기(핸들은 `OnUnPossess`에서 해제).
- `StartAILogic()`은 **베이스는 비어 있는 가상 함수**. 자식이 오버라이드해 실제 두뇌(BT/StateTree)를 구동한다 — 두뇌 방식은 아직 미결정이라 훅만 열어둔 상태.

**서버 권위**: AIController는 서버에만 존재하므로 `StartAILogic()` 이하 AI 결정은 자연히 서버 권위다. → [Multiplayer.md](Multiplayer.md)

## 두뇌 방식 (등급별)

컨트롤러 계층이 자식별로 `StartAILogic()`을 따로 오버라이드하므로 등급별로 다른 두뇌를 쓸 수 있다.

- **Rogue = BehaviorTree** (확정). 단순 잡몹.
- **Outlaw = 미정** (BT vs StateTree). 페이즈 전투를 강하게 요구하면 StateTree가 유리. StateTree로 가면 `ACBOutlawController::StartAILogic()`에서 StateTree를 구동하고, BT 계층은 건드리지 않는다.

## BT 데이터 흐름 (Rogue)

BT 애셋은 "AI를 정의하는 에셋"이므로 로드아웃이 관리한다 (에셋 등록 원칙 → [Loadout.md](Loadout.md)). BT를 **쓰는 주체는 컨트롤러**(두뇌)이므로, 로드아웃은 BT를 **컨트롤러에 하드 참조로 주입(inject)**한다 — 캐릭터가 MontageData를 ActionComponent에 넘기는 것과 동일한 "에셋을 사용처에 주입" 패턴. 캐릭터는 자기 컨트롤러를 로드아웃 함수에 넘겨 **호출만** 한다.

```
UCBRogueLoadout::BehaviorTree (하드 참조)
   │  UCBAILoadout::GetBehaviorTree() (virtual, 베이스는 null)
   ▼
ACBAICharacter::InitializeAISystem() 로드 콜백 [서버 분기]
   → LoadedLoadout->Auth_ApplyBehaviorTreeToController(GetController())   ← 로드아웃이 적용
   ▼
ACBAIController::BehaviorTree (하드 참조, SetBehaviorTree로 주입)
   ▼
ACBRogueController::StartAILogic()  (SystemReady 후)
   → RunBehaviorTree(BehaviorTree)   ← BT에 지정된 Blackboard 자동 세팅
```

- **적용 함수는 로드아웃이 소유**: `UCBAILoadout::Auth_ApplyBehaviorTreeToController(AController*)`가 내부에서 `ACBAIController`로 캐스팅 후 `SetBehaviorTree(GetBehaviorTree())` 주입. 캐릭터는 오케스트레이터로서 호출만 하고 두뇌 애셋을 보관하지 않는다(자기가 쓰지 않는 애셋을 들지 않음).
- 로드아웃 계층: `UCBCharacterLoadout → {UCBChaserLoadout, UCBAILoadout → {Rogue, Outlaw}}` — 캐릭터 계층과 대칭. BT getter는 `UCBAILoadout`에만 있어 Chaser 로드아웃은 오염되지 않는다.
- BT 멤버는 `ACBAIController`(베이스)가 하드 참조로 보유(생존 보장). 실행 방식(`RunBehaviorTree`)은 자식 `StartAILogic()`이 결정 — StateTree 쓰는 Outlaw는 이 멤버를 쓰지 않고 자체 두뇌를 구동.
- **하드 참조 트레이드오프**: BT가 로드아웃 하드 참조라 클라이언트에도 로드된다(AI는 서버 전용인데). BT/BB는 가벼워 감수. 로드아웃 원칙(중첩 async 금지)을 지키기 위한 선택.

## 인식 → 추격 (Perception)

적 감지와 타겟 결정은 컨트롤러(두뇌)가, 추격 행동은 BT가 담당한다.

```
[인식] UAIPerceptionComponent (Sight + Hearing)  →  적 플레이어 감지
   │  ACBAIController::HandleTargetPerceptionUpdated  (감각 종류를 가리지 않음)
   │  UCBBTService_UpdateTarget                       (주기적 재평가)
   ▼
[선정] ACBAIController::UpdateTarget  →  후보 점수화 → 전환 판정
   ▼
[기억] Blackboard::TargetActor  ←  ACBAIController::UpdateTargetInBlackboard
   ▼
[결정] BehaviorTree  →  TargetActor 있으면 MoveTo, 없으면 Idle/Patrol
   ▼
[실행] MoveTo → PathFollowing → NavMesh → CMC
```

**연속 추격의 주체는 `MoveTo`다.** 블랙보드에 위치(Vector)가 아닌 **액터 참조**(`SetValueAsObject`)를 저장하므로, `MoveTo`가 그 액터의 실시간 위치를 추적하며 경로를 갱신한다(`bTrackMovingGoal`). 퍼셉션은 "타겟 획득/상실" 두 이벤트만 담당하고, 매 프레임 위치를 먹여줄 필요가 없다.

### 두 감각의 역할 분리

| 감각 | 범위 | 시야 차단(LOS) | 감지 조건 |
|---|---|---|---|
| **Sight** | 전방 부채꼴 (반각 60° = 전체 120°), 반경 1500 | **적용** (벽 뒤 못 봄) | 시야 안 + 가림 없음 + **진영이 적** |
| **Hearing** | **전방위**, 기준 반경 1500 | 무관 | 대상이 **소음을 낼 때만** + **진영이 적** |

두 감각 모두 `DetectionByAffiliation`이 적만 감지로 설정돼 있어 아군·중립은 자극조차 오지 않는다. 진영 판정 규칙은 → [Teams.md](Teams.md)

- **시각은 순수하게 시각으로 둔다** — "근접이면 360° 시야" 같은 인위적 처리를 하지 않는다(AI가 뒤통수로 보는 셈이라 부자연스러움).
- **등 뒤 사각지대는 청각이 보완한다.** 대신 **가만히 서 있으면 소음이 없어 감지되지 않는다** — 버그가 아니라 의도된 스텔스 규칙.
- `UAISense_Hearing`은 **이벤트 기반**이라 폰을 수동 감지하지 않는다. 반드시 `ReportNoiseEvent`로 소음을 보고해야 한다(Sight의 `bAutoRegisterAllPawnsAsSources` 같은 자동 등록이 없음).

### 소음 발생 — `UCBNoiseEmitterComponent`

`ACBBaseCharacter`가 소유하는 컴포넌트가 이동 중 주기적으로 소음을 보고한다.

- **[서버 전용]** `BeginPlay`에서 `HasAuthority()`일 때만 타이머 시작(기본 0.35초 간격). AI 퍼셉션이 서버에만 있으므로 클라 보고는 낭비.
- **애님 노티파이가 아닌 서버 타이머 방식** — 서버에서 애님 틱이 스킵되어도(`VisibilityBasedAnimTickOption`) 소음이 누락되지 않는다.
- 속도가 `MinSpeedToMakeNoise` 미만이면 소음 없음(정지 = 미감지).
- **개이트별 Loudness**로 들리는 거리를 조절. **유효 청취 거리 = 청취자의 `HearingRange` × `Loudness`** (엔진 `UAISense_Hearing::Update` 기준):

| 개이트 | Loudness | 유효 거리(HearingRange 1500 기준) |
|---|---|---|
| 정지 | 소음 없음 | — |
| Walk | 0.3 | 450 |
| Run | 0.65 | ~975 |
| Sprint | 1.0 | 1500 |

개이트 판별은 `UCBAbilitySystemLibrary::GetCurrentGaitTag(ASC)` 재사용. → [GameplayTags.md](GameplayTags.md)

- 퍼셉션은 **`ACBAIController`(베이스)**가 소유 (Rogue·Outlaw 공통 두뇌 기능). 생성자에서 `UAISenseConfig_Sight`·`UAISenseConfig_Hearing`을 구성 후 `OnTargetPerceptionUpdated`에 바인딩. 콜백은 감각 종류를 가리지 않으며, 감지·상실 어느 쪽이든 `UpdateTarget()` 재선정으로 합류시킨다(아래).
- **함정 — 한 감각의 만료를 "상실"로 읽지 말 것.** 콜백은 감각을 가리지 않고 한 덩어리로 오는데, 청각 자극은 `MaxAge`(3초)가 지나면 `UAIPerceptionComponent::AgeStimuli()` 가 만료 표시 후 **자극을 다시 등록**해 `WasSuccessfullySensed() == false` 인 업데이트를 보낸다. 타겟이 멈춰 소음이 끊기면 이 만료가 반드시 오고, 실패 분기가 무조건 지우면 **눈앞에 보이는 타겟이 3초마다 None 으로 초기화**된다(움직일 땐 소음이 갱신돼 증상이 안 보임). 그래서 만료를 그 자리에서 "상실"로 처리하지 않고 재선정에 넘긴다 — 후보 수집(`GetCurrentlyPerceivedActors`)과 `IsTargetStillValid()` 의 `HasAnyCurrentStimulus(Actor)` 검사가 **다른 감각이 아직 인지 중인지**를 대신 판단한다.
  - 시야 자극은 만료되지 않는다 — `UAISense_Sight` 는 보이는 동안 매 업데이트마다 성공 자극을 재등록하므로 `MaxAge` 에 걸릴 일이 없다. 그래서 "청각 만료 + 시야 유효"가 정상 상태로 존재한다.
- **적 판정 seam — `IsValidTarget(AActor*)`**: 이 한 함수가 "적이냐"를 격리한다. 현재는 `FGenericTeamId::GetAttitude(this, InActor) == Hostile`(진영 판정)만 본다 — 퍼셉션 소속 필터와 **같은 기준**이라 두 단계의 결론이 어긋나지 않는다. `virtual`이므로 자식이 추가 조건(생존·등급 등)으로 좁힐 수 있다. → [Teams.md](Teams.md)
- **컨트롤러는 팀을 보유하지 않는다.** `ACBAIController::GetGenericTeamId()`는 빙의한 폰의 진영을 **위임 반환**한다(`CachedAICharacter`, 없으면 `GetPawn()` 폴백). 퍼셉션 등록이 빙의보다 먼저일 수 있으므로 `OnPossess`에서 `RequestStimuliListenerUpdate()`로 소속 필터를 1회 재평가시킨다.
- **시드**: `OnTargetPerceptionUpdated`는 상태 변화 시에만 발화하므로, BT 시작(`RunBehaviorTree`) 직후 `UpdateTarget()`을 1회 불러 이미 시야에 있던 정지 타겟을 시드한다. 별도 시드 함수를 두지 않는 이유는 "현재 인지 중인 후보에서 고른다"가 재선정과 완전히 같은 일이기 때문.
- 블랙보드 키 이름은 `ACBAIController::TargetActorKey`(= `"TargetActor"`) 상수. **에디터 BB 키 이름과 반드시 일치.**
- 서버 권위: 컨트롤러가 서버 전용이라 인식·판단이 전부 서버에서 돌고, 이동 결과만 CMC가 복제. → [Multiplayer.md](Multiplayer.md)

### 타겟 선정 — `ACBAIController::UpdateTarget()`

퍼셉션 자극이 곧 타겟이던 시절에는 **마지막으로 자극을 보낸 적이 이겨서**, 플레이어가 둘 이상이면 바로 옆의 적을 두고 멀리 있는 적으로 튀었다. 지금은 후보 전원을 점수로 비교한다.

**블랙보드에 쓰는 경로는 `UpdateTarget()` 하나뿐이다.** 퍼셉션 콜백도 여기로 합류하므로, 점수 규칙을 우회하는 두 번째 쓰기 지점이 생기지 않는다. 소비자(EQS 컨텍스트·BT·모션 워핑)는 예전과 똑같이 키를 읽기만 한다.

| 나누는 축 | 소유 | 이유 |
|---|---|---|
| **언제** 평가하나 | `UCBBTService_UpdateTarget` (주기) + 퍼셉션 콜백 (이벤트) | 실행 시점은 두뇌의 관심사 |
| **누구를** 고르나 | `ACBAIController` | BT 노드에 넣으면 StateTree·BT 밖에서 재사용 불가 |

- **주기 평가가 반드시 필요하다.** 점수에 거리가 들어가는 순간, 아무 자극이 없어도 순위가 바뀐다(적이 조용히 다가오기만 해도 역전). 자극 콜백만으로는 그 변화를 못 잡는다.
- 등급별 차이는 `ScoreTarget()`·`CanSwitchTarget()` 오버라이드로. 2차 위협도 테이블(누적·감쇠·도발)도 `ScoreTarget()` 한 곳으로 들어온다.

#### 점수

```
Score = BaseTargetScore(1.0)
      + DistanceWeight(1.0)    × (1 - Clamp(거리 / MaxScoreDistance(2000), 0, 1))
      + SightBonus(0.5)        × (시야 감각이 현재 인지 중이면 1)
      + RecentDamageBonus(1.0) × (RecentDamageMemoryTime(5초) 안에 나를 때린 대상이면 1)
```

- **바닥값(`BaseTargetScore`)이 있는 이유**: 히스테리시스가 **배수 비교**라 점수가 0이 되면 비교가 무너진다(0 × 1.25 = 0). 모든 항이 0 이상이고 바닥이 1.0 이면 안전하다. `ScoreTarget()` 을 오버라이드할 때도 이 전제(항상 0보다 큼)를 지킬 것.
- 시야 확보 판정은 `HasActiveStimulus(Actor, UAISense::GetSenseID<UAISense_Sight>())` — "소리만 들리는 적"보다 "보이는 적"을 우선.
- 가중치는 전부 컨트롤러의 `EditDefaultsOnly`. 캐릭터별로 갈리면 그때 로드아웃 데이터로 옮긴다.

#### 전환 안정화 세 가지

| | 값 | 무엇을 막나 |
|---|---|---|
| **재평가 주기** | `UBTService::Interval` 0.5초 ± 0.1 | 매 틱 순회. 편차는 AI 여럿의 순회가 한 프레임에 몰리는 것 |
| **히스테리시스** | `SwitchScoreRatio` 1.25 | 점수가 엎치락뒤치락할 때 매 평가마다 타겟이 뒤바뀌는 것 |
| **전환 금지 구간** | `TargetLockAbilityTags` (기본 `Ability.Combat.Attack`) | 공격 몽타주 도중 결정이 바뀌어, 끝나자마자 반대쪽으로 튀는 것 |

- **잠금은 활성 어빌리티를 조회한다.** ASC 의 `GetActivatableAbilities()` 를 돌며 `Spec.IsActive() && GetAssetTags().HasAny(TargetLockAbilityTags)` 이면 교체 금지. 상태 태그를 새로 만들지 않은 이유는 잠금 범위를 **컨트롤러 클래스마다 데이터로** 정하기 위함(잡몹은 공격만, 보스는 스킬·돌진까지). `Ability.Combat.Attack` 은 하위 공격을 한 번에 매칭하려고 추가한 부모 태그다(`HasAny` 는 부모 매칭).
- **잠금이 막는 것은 교체뿐이다.** 현재 타겟이 죽거나·인지에서 사라지거나·파괴되면(`IsTargetStillValid()` 실패) 잠금과 무관하게 즉시 버린다. 아니면 이미 없는 대상을 향해 몽타주가 끝날 때까지 서 있게 된다.
- **모션 워핑은 몽타주 중간에 꺾이지 않는다.** `UCBAIAttackAbility::BuildActionCueParameters()` 가 활성화 시점에 타겟 컴포넌트를 큐 파라미터로 고정하므로, 잠금이 없어도 워프 대상은 안 바뀐다. 잠금이 실제로 막는 것은 **판단과 연출의 불일치**(때리는 대상과 쫓기로 정한 대상이 다름)와 BT 브랜치 흔들림이다.
- 사망 판정은 아직 사망 시스템이 없어 `CurrentHealth > 0`(`IsTargetAlive()`)으로 대신한다. 체력 어트리뷰트가 없는 대상은 생존으로 간주. → 사망 어빌리티가 들어오면 이 자리를 상태 태그 검사로 교체할 것.

#### 위협 입력 — 피격 이벤트 재사용

```
UCBAttributeSet::PostGameplayEffectExecute [서버]
  └ Event.Combat.HitReact  (Payload.Instigator = 가해자의 ASC 소유 액터)
        ▼
ACBAIController::HandleHitReactEvent  →  ResolveThreatPawn() → LastDamageInstigator / LastDamageTime
        ▼
ScoreTarget() 의 최근 피격 가산점
```

- **새 배관을 만들지 않고 피격 반응 이벤트를 구독한다.** 구독은 베이스 `StartAILogic()`(SystemReady 이후라 ASC 확정), 해제는 `OnUnPossess()`. **자식 컨트롤러는 `StartAILogic()` 에서 반드시 `Super` 를 호출할 것** — 안 부르면 위협 가산점만 조용히 죽는다.
- **함정 — `Payload.Instigator` 는 폰이 아니다.** `UAbilitySystemComponent::MakeEffectContext()` 가 `AddInstigator(OwnerActor, AvatarActor)` 로 채우므로 `GetInstigator()` 는 **ASC 소유 액터**다. 플레이어는 ASC 가 PlayerState 에 있어(→ [ASC-Ownership.md](ASC-Ownership.md)) 여기로 `ACBPlayerState` 가 들어온다. 퍼셉션 후보(폰)와 그대로 비교하면 **영원히 일치하지 않으므로**, `ResolveThreatPawn()` 이 컨트롤러·PlayerState 를 폰으로 환원한다.
- **한계(수용)**: 이 이벤트는 GE 의 `Effect.HitReact` **옵트인**이라, 태그가 없는 지속(DoT)·환경 데미지는 위협으로 잡히지 않는다. 체력이 0 이 되는 타격도 스킵되지만 죽는 순간이라 무관. 모든 데미지를 위협으로 삼으려면 어트리뷰트셋에 별도 이벤트를 하나 더 발행해야 한다.
- 기록만 하고 **타겟을 즉시 바꾸지는 않는다.** 피격은 점수 항목일 뿐이고 전환 여부는 잠금·문턱을 거친다. (도발은 2차에서 즉시 전환 예외로 들어올 자리)

## BT 커스텀 노드

접근·회전·조건 판정은 **엔진 내장 노드로 충분하다.** 엔진에 없는 것만 커스텀으로 채운다 — GAS 와 BehaviorTree 를 잇는 다리, 그리고 타겟 기준 지점 계산.

| 하고 싶은 것 | 노드 |
|---|---|
| 적에게 다가가기 | `BTTask_MoveTo` (`AcceptableRadius` = 공격 사거리) — 내장 |
| 사거리 판정 | `BTDecorator_IsAtLocation` — 내장 |
| 타겟 바라보기 | `BTTask_RotateToFaceBBEntry` — 내장 |
| 타겟 보유 여부 | `BTDecorator_Blackboard` (`Is Set`) — 내장 |
| 어빌리티 재생 중 다른 행동 차단 | `BTDecorator_CheckGameplayTagsOnActor` — 내장 |
| **어빌리티 활성화** | **커스텀** — `UCBBTTask_ActivateAbility(AndWait)` (아래) |
| **후퇴·경계 지점 계산** | **EQS** — `BTTask_RunEQSQuery`(내장) + 쿼리 에셋 (아래) |
| **경계 중 타겟 주시 + 스트레이프 회전** | **커스텀** — `UCBBTService_StrafeFocus` (아래) |
| **타겟 재선정 주기** | **커스텀** — `UCBBTService_UpdateTarget` (아래) |

### 둘로 나누는 기준은 등급이 아니라 "기다릴 것이 있느냐"

```
UCBBTTask_ActivateAbility          ← 활성화하고 즉시 반환
└── UCBBTTask_ActivateAbilityAndWait  ← 종료까지 대기, 결과로 분기
```

활성화 로직(`TryActivateAbilityByTag`)은 베이스가 소유하고, 파생은 **대기·종료 판정만** 얹는다. 이 함수는 상태를 남기지 않는 `const` 라 공유 노드에서 호출해도 안전하다.

| | 단순 (베이스) | 대기형 (파생) |
|---|---|---|
| 어빌리티 재생 중 | `Failed` → BT 가 다른 분기로 | `InProgress` → 그 자리에서 대기 |
| 캔슬/정상 종료 구분 | ❌ 알 수 없음 | ✅ `Failed` / `Succeeded` |
| 노드 인스턴스화 | ❌ 불필요 (무상태) | ✅ 필요 |
| 용도 | 즉시 끝나는 어빌리티(버프·상태 전환), 재생 중에도 이동해야 하는 공격(돌진 등) | **몽타주 액션 전반 — 잡몹 공격 포함** |

**잡몹이라고 단순 버전을 쓰지 않는다.** 몽타주가 도는 동안 태스크에 머물러야 BT 가 다른 가지로 새지 않기 때문이다(안 그러면 휘두르면서 걸어간다). 잡몹은 캔슬 여부를 안 쓸 뿐이지, 대기 자체는 필요하다.

> **대안이었던 폴링 방식**: `TickTask` 는 `NodeMemory` 를 인자로 받으므로, 델리게이트 대신 매 틱 `IsActive()` 를 확인하면 인스턴스화 없이도 대기할 수 있다. 다만 아끼는 것이 AI 당 UObject 하나인 데 비해 매 틱 스펙 조회가 들고 캔슬 구분을 잃어 **손해가 더 크다.** AI 가 수백 마리 규모가 되면 재검토할 것.

**대기형만 인스턴스화가 필요한 이유**: 어빌리티 종료는 ASC 델리게이트로 나중에 도착하는데, 그 콜백은 노드 메모리 포인터를 받을 수 없다. 콜백이 자기 AI 를 특정할 단서는 바인딩된 `this` 뿐이라 노드가 AI 마다 따로 있어야 한다. 단순 버전은 `ExecuteTask` 안에서 모든 것이 끝나므로 노드를 공유해도 무방하다.

```
Selector
├─ [Deco: TargetActor Is Set]  Sequence
│   ├─ MoveTo (TargetActor, AcceptableRadius = 사거리)
│   ├─ RotateToFaceBBEntry (TargetActor)
│   └─ Activate Ability And Wait (AbilityTag = Ability.Combat.Attack.*)
└─ Wait / Patrol
```

### 공통 규칙

- **어느 어빌리티가 켜졌는지 알아야 하므로** `TryActivateAbilitiesByTag` 대신 `GetActivatableGameplayAbilitySpecsByAllMatchingTags` 로 스펙을 찾아 `TryActivateAbility(Handle)` 를 부른다. 대기형은 종료 콜백에서 `FAbilityEndedData::AbilitySpecHandle` 을 이 핸들과 대조해 **같은 ASC 의 다른 어빌리티(피격 반응 등) 종료를 걸러낸다.**
- **쿨다운 데코레이터가 필요 없다.** 쿨다운은 어빌리티의 GAS 표준 쿨다운(`CommitAbility`)이 담당하고, 쿨다운 중에는 활성화가 실패해 `Failed` 가 반환된다. → [Abilities.md](Abilities.md)
  - ⚠️ 다만 **그 `Failed` 를 받아줄 형제가 있어야** 한다. Rogue 의 전투 브랜치처럼 Sequence 안에 있으면 시퀀스가 통째로 실패하므로, 쿨다운으로 활성화가 막히는 상황 자체를 만들지 않는 편이 안전하다(아래 [전투 사이클](#전투-사이클-rogue)).
- **대기형의 반환값 규칙**: 활성화 실패·캔슬 종료 = `Failed` / 정상 종료 = `Succeeded`. **"활성화 직후 즉시 종료"는 `Succeeded`** 다 — 기다릴 것이 없었을 뿐이므로, 즉시 완료되는 어빌리티에 이 태스크를 써도 결과가 올바르다.
  - 이것이 성립하려면 **어빌리티가 발동 전제 조건을 `CanActivateAbility` 에서 검사**해야 한다. `ActivateAbility` 안에서 `EndAbility` 로 튕기면 "즉시 끝남"이 실패인지 정상 완료인지 구분할 수 없다. 안전망으로 남기는 검사는 `bWasCancelled = true` 로 끝내 실패로 식별되게 한다. → [Abilities.md](Abilities.md)
- **이미 재생 중이면 재활성화를 시도하지 않는다** (`FindActiveAbilityHandle` 가드). 어빌리티가 `InstancedPerActor`(`UCBGameplayAbility` 생성자)이고 `bRetriggerInstancedAbility` 가 기본 false 라 엔진도 재활성화를 막지만, 그냥 두면 매 틱 실패 시도가 반복되며 엔진 로그만 쌓인다. 가드가 그 앞에서 걸러낸다.
  - **단순 버전**: 이미 재생 중 → `Succeeded`. 의도(이 어빌리티가 돌아가는 것)는 이미 충족됐다. 여기서 `Failed` 를 반환하면 BT 가 다른 분기로 흘러 **공격 몽타주 중에 `MoveTo` 가 끼어든다**(휘두르며 걸어가는 그림).
  - **대기형**: 이미 재생 중 → 그 어빌리티의 종료를 구독하고 대기. 다른 가지나 이벤트가 먼저 발동시킨 경우에도 자기가 켠 것처럼 동작한다.
  - 이 가드로 "재생 중 다른 행동 차단"이 대체로 해결되지만, **다른 가지 전체를 막아야 하면** 어빌리티에 `ActivationOwnedTags` 로 상태 태그를 걸고 그 가지에 `BTDecorator_CheckGameplayTagsOnActor` 를 붙인다(커스텀 코드 불필요).
- **대기형은 중단·파괴 경로에서 반드시 구독을 끊는다.** `AbortTask` 는 진행 중인 어빌리티를 `CancelAbilityHandle` 로 캔슬하고, `OnInstanceDestroyed` 는 BT 정지·AI 파괴 시 남은 구독을 정리한다. 캔슬이 종료 콜백을 다시 부르므로 **구독 해제를 캔슬보다 먼저** 한다.
- **노드 인스턴스의 멤버는 실행이 끝나도 유지된다.** 자동 리셋이 없으므로 `ExecuteTask` 첫머리에서 직접 정리한다.

### 경계 중 타겟 주시 — `UCBBTService_StrafeFocus`

경계 모드는 **이동 중에도 타겟을 계속 바라봐야** 한다(스트레이프). 엔진 노드로는 두 군데가 막힌다.

**① 포커스 우선순위** — 이동이 시작되면 `UPathFollowingComponent::UpdateMoveFocus()` 가 진행 방향으로 Move 포커스를 계속 세팅한다. 내장 `UBTService_DefaultFocus` 는 우선순위가 Default 라 여기에 밀려 **이동하는 순간 주시가 풀린다.**

| 우선순위 | 값 | 세팅 주체 |
|---|---|---|
| Default | 0 | `UBTService_DefaultFocus` |
| Move | 1 | PathFollowing (이동 중 자동) |
| Gameplay | 2 | `SetFocus`, `BTTask_RotateToFaceBBEntry`, **이 서비스** |

베이스가 `FocusPriority` 를 protected 로 열어둔 것이 이 목적("reusing focus-setting mechanics by derived classes")이라, 파생해서 우선순위만 Gameplay 로 올린다.

**② 회전 모드** — AI 기본값은 `bOrientRotationToMovement = true` 라 몸이 진행 방향으로 돈다. 주시하려면 진입/이탈에 맞춰 뒤집어야 한다.

```cpp
// 진입: 컨트롤러 포커스 회전을 몸에 반영
bOrientRotationToMovement = false;
bUseControllerDesiredRotation = true;
```

**두 값은 짝으로 뒤집는다.** `bOrientRotationToMovement` 만 끄면 CMC 의 `PhysicsRotation` 이 아무것도 회전시키지 않아 몸이 굳는다.

- **복원값은 캐릭터 클래스의 CDO 에서 읽는다.** 노드가 기본값을 하드코딩해 들고 있으면 BP 에서 설정을 바꿨을 때 조용히 어긋난다.
- `bUseStrafeRotation` 을 끄면 포커스만 걸고 회전 모드·태그는 건드리지 않는다.

**③ 애님 전환** — 회전 모드와 함께 `Status.Movement.Strafe` 태그를 `TagOnly` 로 걸고 뺀다. 회전 설정은 서버에서만 바뀌므로(BT 가 서버 전용) **애님이 읽을 상태는 복제되어야** 시뮬 프록시에서도 옆걸음 모션이 나온다. 소비자는 `UCBAIAnimInstance::IsStrafing()`. → [AnimInstance.md](AnimInstance.md), [GameplayTags.md](GameplayTags.md)

**③ 이동 속도** — 경계 중에는 걷기(`SpeedAbilityTag`, 기본 `Ability.Movement.Walk`)로 낮춘다. **속도 값을 CMC 에 직접 쓰지 않고 어빌리티를 켠다.**

- 정식 경로는 `GA_Walk → GE_MovementModifier → MovementSpeed 어트리뷰트 → ACBBaseCharacter::OnMovementSpeedChanged → CMC.MaxWalkSpeed` 다. `OnMovementSpeedChanged` 는 **종착점**이라 여기만 부르면 어트리뷰트와 CMC 가 어긋나고, 이후 아무 GE 나 어트리뷰트를 갱신하면 값이 조용히 덮어써진다.
- 더 큰 이유는 **개이트 태그**다. 속도만 낮추면 `UCBLocomotionProcessor` 가 여전히 `Gait.Run` 으로 판정해서 → 애님은 Run 기준 속도 비율로 출발/정지를 판정하고, `UCBNoiseEmitterComponent` 는 Run Loudness(0.65)로 소음을 보고하며(살금살금 걷는데 뛰는 만큼 시끄러움), 가속·제동도 Run 값이 된다.
- 이탈 시에는 **태그로 취소**한다(`CancelAbilities`). 어빌리티의 `EndAbility` 가 속도 GE 를 제거하므로 원래 개이트로 복구된다 — 핸들을 보관할 필요가 없다(활성화·취소 모두 애셋 태그로 매칭).
- **로드아웃에 해당 어빌리티가 부여돼 있어야 한다.** 없으면 활성화가 조용히 실패하고 속도만 안 바뀐다.
- BP 자식의 `bEndWhenNoAcceleration` 은 꺼져 있어야 한다(GA_Sprint 전용). 켜져 있으면 경계 중 잠깐 멈출 때마다 걷기가 풀린다.

**태스크 두 개(진입/종료)로 나누지 않는 이유**: 브랜치는 여러 경로로 끝난다 — 경계 시퀀스 정상 완료(공격으로 넘어감), 타겟 상실, BT 정지, 사망. 종료 태스크는 그때 실행되지 않아 **회전 모드가 켜진 채 남고**, 이후 추격·순찰이 계속 이상하게 돈다. 서비스의 `OnCeaseRelevant` 는 중단 경로에서도 호출이 보장되므로 짝이 어긋날 수 없다.

### 타겟 재선정 주기 — `UCBBTService_UpdateTarget`

`ACBAIController::UpdateTarget()` 을 부르는 것이 전부다. **판정은 한 줄도 들고 있지 않다** — 노드에 넣으면 StateTree 나 BT 밖에서 재사용할 수 없기 때문(→ 위 [타겟 선정](#타겟-선정--acbaicontrollerupdatetarget)).

- **재평가 주기는 엔진 `UBTService` 의 `Interval`/`RandomDeviation` 을 그대로 쓴다** — 커스텀 타이머가 필요 없다. 기본 0.5초 ± 0.1.
- `bCallTickOnSearchStart = true` 로 브랜치 진입 즉시 1회 평가. 주기를 기다리며 타겟 없이 서 있는 구간이 사라진다.
- **BT 루트에 붙인다.** 전투든 순찰이든 타겟 평가는 항상 돌아야 한다. 특정 가지에만 붙이면 그 가지 밖에서는 재선정이 퍼셉션 이벤트에만 의존하게 된다.

## EQS — 위치 판단

후퇴·경계처럼 **"타겟을 기준으로 어디에 설 것인가"** 는 EQS 가 담당한다. BT 는 지점을 계산하지 않는다.

- **역할 분리**: 내장 `Run EQS Query` 가 지점을 블랙보드 Vector 키에 쓰고, 이어지는 내장 `MoveTo` 가 그 키로 이동한다. 판단(어디)과 이동(어떻게)이 갈려 있어 판단 기준을 바꿔도 이동 쪽은 그대로다.
- **왜 커스텀 태스크가 아니라 EQS 인가**: 후퇴 지점 조건은 "타겟에서 N 만큼 떨어지고 + 갈 수 있고 + 타겟이 보이고 + 지금 위치에서 가깝고" 처럼 계속 붙는다. C++ 로 짜면 조건마다 코드가 붙지만, EQS 는 테스트를 얹기만 하면 되고 점수 분포를 에디터에서 눈으로 보며 조율할 수 있다.

### 구성 요소

| 구성 요소 | 역할 | 우리 선택 |
|---|---|---|
| 컨텍스트 | 기준이 될 액터/위치 | **`UCBEnvQueryContext_Target`** (유일한 커스텀) |
| 제네레이터 | 후보 뿌리기 | `Points: Donut` (중심 = 타겟) |
| 아이템 | 후보 하나 (Point / Actor) | Point → 결과는 **Vector 키** |
| 테스트 | 거르기·점수 | `Pathfinding`·`Distance`·`Trace`·`Dot` (전부 내장) |
| 옵션 | 제네레이터 1 + 테스트 N 묶음 | 앞 옵션이 아이템을 못 내면 다음 옵션으로 (폴백 단위) |

거리 같은 값은 쿼리의 Query Config 파라미터로 노출해 **BT 노드마다 다르게 준다.** 후퇴·경계·거리 유지가 쿼리 에셋 하나를 공유한다.

### 커스텀은 컨텍스트 하나뿐 — `UCBEnvQueryContext_Target`

엔진 기본 컨텍스트는 `Querier`·`Item`·`NavigationData` 뿐이라 **"블랙보드에 든 그 액터"를 가리킬 수단이 없다.** 그 한 칸만 채운다.

- 쿼리 오너(폰) → 컨트롤러 → 블랙보드 순으로 거슬러 올라가 `TargetActor` 를 읽는다. 오너가 컨트롤러가 아니라 폰인 것은 `bAllowControllersAsEQSQuerier` 가 기본 false 이기 때문.
- 키 이름은 `ACBAIController::TargetActorKey` 상수를 공유한다 (에디터 BB 키와 일치해야 하는 값이 두 곳에 생기지 않도록 public 으로 열어둠).
- **타겟을 고르지 않는다.** 누구를 타겟으로 삼을지는 퍼셉션·컨트롤러가 정해 블랙보드에 쓴 결과이고, 컨텍스트는 읽기만 한다. 여기서 따로 판정하면 **BT 가 쫓는 대상과 EQS 가 기준 삼는 대상이 어긋난다**(A 에게 달려가며 B 에게서 물러나는 그림).
- 타겟이 없으면 컨텍스트가 빈 채로 남아 쿼리가 실패하고, `bClearBBEntryOnBTEQSFail`(기본 true)이 결과 키를 비워 이어지는 `MoveTo` 도 실패한다 — 실패 분기를 따로 짜지 않아도 된다.

### 알아둘 것

- **실행 주체는 에셋이 아니라 `UEnvQueryManager`** 다. `UEnvQuery` 는 "제네레이터·테스트를 어떤 설정으로 조합했는가"만 담은 데이터 에셋이고, BT 노드는 요청(`FEnvQueryRequest`: 에셋 + 오너 + 파라미터 + RunMode)을 만들어 매니저에 넘긴다. 매니저는 이를 인스턴스로 컴파일해(`InstanceCache` 재사용) 큐에 넣고, 매 틱 3ms 예산 안에서 제네레이터·테스트를 한 스텝씩 돌린다.
- 그래서 **결과는 즉시 오지 않는다.** BT 태스크는 `InProgress` 로 잠들었다 완료 메시지로 깨어난다. 무거운 쿼리도 프레임을 잡아먹지 않는 대신 결과가 몇 프레임 뒤에 온다.
- **테스트 실행 순서는 에디터에 놓은 순서가 아니다.** 제네레이터의 `bAutoSortTests`(기본 켜짐)가 실행 직전에 재배치한다 — ① 필터(`TestPurpose != Score`)가 점수 전용보다 앞 ② 같은 부류끼리는 `Cost` 싼 것 먼저(`Distance`=Low, `Trace`·`Pathfinding`=High) ③ **RunMode 가 SingleResult 면 가장 비싼 필터 하나를 빼내 맨 마지막에 붙인다** — 앞선 점수로 정렬한 뒤 위에서부터 검사해 첫 통과에서 멈추기 위해서(`CanRunAsFinalCondition`). 덕분에 비싼 트레이스·경로 계산이 아이템 수만큼이 아니라 보통 한두 번만 돈다.
  - 그래서 **필터에 걸린 아이템에도 점수 테스트 값이 찍혀 보인다.** 점수 테스트가 먼저 돌았기 때문이며, 걸린 아이템은 무효화되어 최종 선택에서는 제외된다. 디버깅 중 이걸 막고 싶으면 `bAutoSortTests` 를 끄면 에디터 순서 그대로 실행된다.
  - 같은 Cost 끼리는 StableSort 라 에디터 순서가 유지된다. 순서가 의미를 갖는 건 동급 비용 테스트들 사이뿐이다.
- **`Trace` 테스트의 Bool Match 는 "충돌 여부"가 아니라 "기대값과 일치하는가"다.** 원시 값은 `bHit`(막혔는가)이고 Bool Match 가 기대값이라, **시야 판정은 Bool Match = false**(막히지 않아야 통과)로 둔다. 그리고 Item·Context Height Offset 을 100~150 으로 올릴 것 — 0 이면 바닥끼리 선을 그어 지면에 막히거나 스쳐서 결과가 전부 한쪽으로 쏠린다.
- **아이템 타입과 블랙보드 키 타입이 맞아야 한다.** Point 계열 제네레이터 → Vector 키, Actor 계열 → Object 키. 어긋나면 경고 로그만 남고 실패한다.
- 한 번 실행이 25ms 를 넘으면 경고 로그가 뜬다. 비용 대부분은 도넛 반경·링 수·포인트 수와 `Pathfinding` 테스트이니 조일 곳도 거기다.
- **테스트는 블루프린트로 못 만든다**(C++ 전용). 컨텍스트·제네레이터는 BP 베이스가 있다.
- **디버깅**: 인게임은 게임플레이 디버거 — `'`(Apostrophe)로 켜고 콘솔에서 `gdt.EnableCategoryName EQS 1`. 카테고리 토글 키는 **넘패드** 숫자라 넘패드 없는 키보드에서는 콘솔 명령이 사실상 유일한 경로다. 카테고리 안에서 넘패드 `*`=최근 쿼리 순환, `/`=아이템 점수 상세. 수집 주기는 `ai.debug.EQS.RefreshInterval`(기본 2초).
- 에디터에서는 레벨에 `EQS Testing Pawn` 을 놓고 쿼리를 지정하면 플레이 없이 점수 분포를 본다. 단 블랙보드가 없어 타겟 컨텍스트가 비므로, 형태 확인용으로만 쓰고 실제 검증은 인게임 디버거로 한다.
- 서버 전용이다. AI 컨트롤러·BT 가 서버에만 있으므로 EQS 도 서버에서만 돈다. 쿼리 에셋의 `bStripFromClientBuilds` 로 클라 빌드에서 뺄 수 있다.

## 전투 사이클 (Rogue)

**멀면 추격 → 가까우면 경계(주시하며 원호 이동) → 잠깐 대기 → 공격 → 반복.** 분기 조건으로만 표현하고 상태 변수를 두지 않는다.

```
Root
└─ Selector
   ├─ Sequence "추격"        [Deco: IsAtLocation(TargetActor, 근접 반경) ❌]   ← 멀 때
   │   └─ MoveTo (TargetActor)
   │
   └─ Sequence "경계/공격"   [Deco: IsAtLocation(TargetActor, 근접 반경) ✅]   ← 가까울 때
       ├─ Sequence "경계"     + Service: Strafe Focus (TargetActor)
       │   ├─ Run EQS Query (EQS_CombatPosition, Random Best 25% → MoveLocation)
       │   ├─ MoveTo (MoveLocation, Allow Strafe ✅)      ← 주시하며 원호 이동
       │   └─ Wait (WaitTime ± RandomDeviation)           ← 공격까지의 템포
       └─ Sequence "공격"
           ├─ MoveTo (TargetActor, AcceptableRadius = 공격 사거리, Allow Strafe ❌)
           └─ Activate Ability And Wait (Ability.Combat.Attack.Basic)
```

- **순서는 Sequence 가 보장한다.** 경계 → 공격이 한 시퀀스 안에 묶여 있어 **공격 전에 반드시 경계를 거친다.** "방금 공격했다"를 나타내는 상태 변수도, 재공격을 막는 쿨다운 게이트도 필요 없다 — 구조가 그 역할을 한다.
  - 그래서 **공격 어빌리티에 쿨다운 GE 가 없어도 된다.** 실제로 `GA_Rogue_Attack_Basic` 에는 없다. GAS 쿨다운은 어빌리티 개별 제약용으로 남겨두고, 전투 리듬은 BT 가 소유한다. → [Abilities.md](Abilities.md)
- **공격 사이 템포 = 경계 브랜치 끝의 `Wait` 노드.** 엔진 `BTTask_Wait` 가 `FRandRange(max(0, WaitTime − RandomDeviation), WaitTime + RandomDeviation)` 로 매 반복 새로 뽑으므로, **`RandomDeviation` 하나로 템포가 랜덤해진다**(예: 1.0 ± 0.5 → 0.5~1.5초). 커스텀 노드도 코드도 필요 없다.
- ⚠️ **공격을 확률로 건너뛰게 만들면 안 된다.** 공격은 Sequence 의 자식이라 여기서 실패하면 **시퀀스 전체가 Failed** 가 되고, 부모 Selector 에는 남은 형제가 없어 **적을 눈앞에 두고 전투 브랜치 자체가 실패로 빠진다.** 리듬을 흔들 자리는 `Wait` 노드이지 분기 조건이 아니다.
  - 같은 이유로 `Activate Ability And Wait` 가 실패하는 상황(무기 없음 등)도 시퀀스를 통째로 떨어뜨린다. 발동 전제는 `CanActivateAbility` 에서 걸러 **활성화 자체가 시도되지 않게** 하는 것이 안전하다. → [Abilities.md](Abilities.md)
- **반복은 Selector 재진입이 만든다.** 공격까지 끝나 시퀀스가 성공하면 루트부터 다시 평가되어, 거리가 여전히 가까우면 새 EQS 지점으로 또 경계를 돈다. Loop 데코레이터가 필요 없다.
- **Strafe Focus 서비스는 바깥 시퀀스가 아니라 경계 서브 시퀀스에 붙인다.** 서비스는 자기가 붙은 서브트리가 relevant 한 동안만 살아 있으므로, 공격으로 넘어가는 순간 `OnCeaseRelevant` 가 회전 모드와 `Status.Movement.Strafe` 태그를 복원한다. 바깥에 붙이면 **공격 중에도 스트레이프 회전이 유지된다.**
- **경계는 RunMode 를 `Random Best 25%` 로 둔다.** Single Best Item 이면 "가장 가까운 지점"이 늘 현재 위치 옆이라 제자리에 붙어 안 움직인다. RunMode 는 쿼리 에셋이 아니라 BT 노드에 있으므로 같은 에셋을 용도별로 나눠 쓸 수 있다.
- **경계 이동에 스트레이프를 쓰는 이유는 애니메이션이다.** 반경을 유지하는 경계 이동은 방향이 접선(좌/우)이라 기존 클립으로 커버되지만, **후진 클립이 없다.** 그래서 **경계용 반경은 좁게**(예: 240~260) 두고, `EQS_CombatPosition` 에 `Dot` 필터(LineA = Querier forward, LineB = Querier→Item, Min ≈ −0.3)를 걸어 뒤로 걸어야 하는 후보를 걸러낸다. 공격 접근(`MoveTo(TargetActor)`)은 정면 이동이라 스트레이프를 끈다.
- **후퇴(공격 후 물러나기) 브랜치는 미구현이다.** 넣는다면 `EQS_CombatPosition` 을 `Single Best Item` 으로 돌려 쓰고, 몸을 돌려 전방 애님으로 처리하도록 `Allow Strafe` 를 끄며, `MoveTo` 의 `ReachTestIncludesAgentRadius` 를 꺼야 한다(기본값이면 절반쯤에서 도착 판정이 나 제자리에 가깝게 멈춘다).

## 다음 단계

- **[에디터]** `EQS_CombatPosition` 에 `Dot` 필터 추가(후진 후보 제거) → 경계용 반경 파라미터 조정.
- **[에디터]** `BT_Rogue` 루트에 `Update Target` 서비스 배치(안 붙이면 재선정이 퍼셉션 이벤트에만 의존한다).
- **[C++]** 여러 마리 스폰 테스트 후, 타겟 앞에 뭉치면 공격 브랜치의 접근 지점도 EQS 로 전환(아군 회피 테스트 추가).
- **[C++]** 위협도 테이블(누적·감쇠·도발 — `ScoreTarget()` 오버라이드), 수색(LastKnownLocation).
- **[미정]** 공격 후 후퇴 브랜치 (위 전투 사이클의 미구현 항목).
- **[애님]** 후진 클립이 없어 경계 이동을 접선(좌/우)으로 제한하고 있다. 2D 블렌드스페이스에 후진이 들어오면 반경 제약과 `Dot` 필터를 완화할 수 있다.

## 관련 문서
- **진영(팀) 판정 규칙·팀 데이터 소유**: [Teams.md](Teams.md)
- 준비 완료 신호까지 초기화를 미루는 패턴: [SystemReady.md](SystemReady.md)
- 서버 권위·초기화 흐름: [Multiplayer.md](Multiplayer.md)
- AI 캐릭터 계층·컴포넌트: [Components.md](Components.md)
