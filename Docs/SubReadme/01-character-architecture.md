## 1 캐릭터 아키텍처

### 1.1 캐릭터 계층 구조
<img src="../img/Character/캐릭터클래스구조.png" width="750">

공통 베이스(`ACBBaseCharacter`)에서 플레이어(`ACBChaserCharacter`)와 AI(`ACBAICharacter`)로 나뉘고, AI는 다시 등급에 따라 보스·엘리트(`ACBOutlawCharacter`)와 일반 잡몹(`ACBRogueCharacter`)으로 구분됨.

| 클래스 | 역할 | ASC · AttributeSet 소유 | 복제 모드 |
|---|---|---|---|
| `ACBBaseCharacter` | 전 캐릭터 공통 베이스. 공통 컴포넌트, 진영, 사망, 생성/소멸 연출, 준비 완료 신호 담당 | 포인터 캐싱만 | — |
| `ACBChaserCharacter` | 플레이어 캐릭터. 카메라·회전·입력·모듈러 의상·전용 컴뱃 컴포넌트 보유 | **`ACBPlayerState`** | `Mixed` |
| `ACBAICharacter` | AI 공통 추상 클래스. ASC·AttributeSet 직접 소유, 사망 시 두뇌 정지 및 처치 경험치 전달 | **캐릭터 자신** | `Minimal` |
| `ACBOutlawCharacter` | 보스·엘리트 AI. 전용 컴뱃 컴포넌트 보유, 보스 BGM 전환 담당 | 캐릭터 자신 (`ACBAICharacter`) | `Minimal` |
| `ACBRogueCharacter` | 일반 잡몹 AI. 전용 컴뱃 컴포넌트와 로드아웃만 가지는 경량 구성 | 캐릭터 자신 (`ACBAICharacter`) | `Minimal` |

- **ASC 소유 주체 분리**
  - **플레이어 (`ACBChaserCharacter`)**: 로비 무기 변경이나 인게임 리스폰 시 폰이 파괴·재스폰되므로, 매치 동안 획득한 성장 상태(레벨업 카드 버프 등)가 유지되도록 `ACBPlayerState`가 ASC와 AttributeSet을 소유함.
  - **AI (`ACBAICharacter`)**: 리스폰이 없고 월드에 다수 배치되므로 캐릭터 자신이 직접 소유하고, GE 복제를 생략하는 `Minimal` 모드로 네트워크 비용을 줄임.

---

### 1.2 베이스 캐릭터 (`ACBBaseCharacter`)
<img src="../img/Character/베이스캐릭터구조.png" width="750">

플레이어와 AI가 공통으로 사용하는 컴포넌트와 생애주기 기능, 기본 충돌 설정을 담당함.

- **보유 컴포넌트**
  - `UCBLocomotionProcessor`: 개이트별 이동 속도·가감속 반영 및 이동 상태 태그 동기화
  - `UCBActionComponent`: 액션 태그+인덱스 기반 몽타주 조회·재생·정지 처리
  - `UCBUIComponent`: 캐릭터 부착형 UI(머리 위 체력바·이름표·보스 바, 로컬 HUD) 관리
  - `UCBNoiseEmitterComponent`: 서버에서 이동 속도에 따라 AI 청각 감지용 소음 발생
  - `UMotionWarpingComponent`: 몽타주 루트모션의 타겟 방향·거리 보정
- **공통 기능**
  - **ASC · AttributeSet 캐싱**: 자식 클래스가 초기화 시점에 세팅한 포인터를 보관하고 `IAbilitySystemInterface`로 외부에 제공
  - **진영(`ECBTeam`) 복제 및 판정**: 캐릭터가 직접 진영(`Chaser` / `Outlaw` / `Neutral`)을 복제 소유해 서버(AI 감지·피격 판정)와 클라이언트(UI 표현)가 동일하게 참조
  - **사망 처리 (`Status.Dead` 태그 구독)**: 서버는 이동 정지·자식 사망 훅(`Auth_OnDeath`)·소멸 연출 및 시체 파괴 타이머를 실행하고, 전 인스턴스는 시체 충돌 해제 및 사망 델리게이트 방송을 수행
  - **생성·소멸 디졸브 연출**: 준비 완료 시 생성 연출(`SpawnCueTag`)을 로컬 실행하고, 사망 시 시체 소멸 시점에 맞춰 소멸 GE(`DespawnEffectClass`)를 적용
- **초기 설정 (생성자)**
  - 캐릭터 액터 Tick 비활성화, 액터 및 이동 복제(`bReplicates`, `ReplicateMovement`) 활성화, 메시 데칼 수신 비활성화
  - **캐릭터 간 충돌 방지**: 다른 캐릭터 캡슐 위에 올라타거나 겹쳐서 튕겨 나가지 않도록 걸을 수 없는 면(`WalkableSlope_Unwalkable`, `CanCharacterStepUpOn = ECB_No`)으로 설정하고 캡슐 겹침 밀어내기 거리(`MaxDepenetrationWithPawn = 20.f`)를 축소
  - **무기 히트(`Weapon`) 채널 분리**: 캡슐은 `Ignore`, 스켈레탈 메시는 `Overlap`으로 설정해 정교한 메시 단위 타격 판정 및 다중 관통 타격 허용
  - **카메라(`Camera`) 채널 무시**: 다른 캐릭터에 가려 스프링암이 당겨지지 않도록 캡슐·메시 모두 `ECC_Camera` 무시

---

### 1.3 플레이어 캐릭터와 AI 캐릭터
<img src="../img/Character/플레이어캐릭터구조.png" width="750">

#### 1) 플레이어 캐릭터 (`ACBChaserCharacter`)
- **보유 컴포넌트**
  - `USpringArmComponent` / `UCameraComponent` / `UCBCameraControlComponent`: 3인칭 카메라 구성 및 마우스 휠 줌 인·아웃 보간 제어
  - `UCBCharacterRotationComponent`: 개이트별 회전 방식(Walk·Run=카메라 방향, Sprint=이동 방향) 계산 및 회전 복제
  - `UCBInputManagerComponent`: `InputConfig` 기반 입력 바인딩, 급반전(피벗) 감지, 이동 입력 잠금
  - `UCBChaserCombatComponent`: 플레이어 무기 스폰·장착·트레이스 및 콤보 상태 관리
  - `UCBModularMeshComponent`: 본체(리더) 메시를 따라가는 피부(`SkinMeshes`) 및 부위별 의상 파츠(`CosmeticMeshes`) 런타임 조립
- **주요 기능**
  - `ACBPlayerState`가 소유한 ASC·AttributeSet을 약참조(`TWeakObjectPtr`)로 캐싱해 사용
  - 사용하는 무기(검·쌍검·대검 등)에 따라 이동 수치·몽타주·애님BP가 모두 달라지므로 무기별 파생 캐릭터 BP와 로드아웃으로 구분
  - 사망 시 시체를 스스로 파괴하지 않고(`DespawnDelay = 0.f`), 게임모드에 리스폰을 요청해 새 폰으로 교체될 때 정리
- **초기 설정 (생성자)**
  - 기본 진영을 `ECBTeam::Chaser`로 설정하고, 캡슐 기본 크기 초기화(로드아웃 로드 후 `BodySetup` 값으로 덮어씀)
  - 컨트롤러 회전이 캐릭터 몸체에 직접 적용되지 않도록 끄고(`bUseControllerRotation* = false`), 스프링암만 컨트롤 회전을 따르도록 설정
  - CMC의 기본 회전 옵션(`bOrientRotationToMovement`, `bUseControllerDesiredRotation`)을 비활성화하고 회전 제어를 `UCBCharacterRotationComponent`에 일임

#### 2) AI 공통 베이스 (`ACBAICharacter`) 및 파생 클래스 (`Outlaw` / `Rogue`)
- **`ACBAICharacter` (AI 공통 추상 클래스)**
  - **보유 컴포넌트**: 캐릭터 생성자에서 `UCBAbilitySystemComponent`와 `UCBAttributeSet`을 직접 생성·소유
  - **주요 기능**: AI 공통 초기화(`InitializeAISystem`) 수행, 사망 시(`Auth_OnDeath`) AI 컨트롤러의 비헤이비어 트리를 정지하고 게임모드에 처치 공유 경험치(`ExperienceReward`) 전달
  - **초기 설정 (생성자)**:
    - ASC 복제 활성화 및 복제 모드를 `Minimal`로 설정
    - 월드 배치·런타임 스폰 시 AI 컨트롤러 자동 빙의(`PlacedInWorldOrSpawned`), 기본 진영 `ECBTeam::Outlaw` 설정
    - 이동 방향 회전(`bOrientRotationToMovement = true`) 및 경로 추종 시 가속 기반 이동(`bUseAccelerationForPaths = true`) 활성화
    - 리슨 서버 화면 밖에 있는 AI도 공격 몽타주 재생 중에는 모션 워핑·소켓 위치가 정상 갱신되도록 `VisibilityBasedAnimTickOption = OnlyTickMontagesAndRefreshBonesWhenPlayingMontages` 설정
- **`ACBOutlawCharacter` (보스 · 엘리트 AI)**
  - 보스·엘리트 전용 컴뱃 컴포넌트(`UCBOutlawCombatComponent`) 생성 및 컨트롤러를 `ACBOutlawController`로 지정
  - 캐릭터 준비 완료 시 보스 BGM(`BossMusic`)으로 전환하고, 사망 또는 액터 파괴 시 원래 레벨 BGM으로 복귀
- **`ACBRogueCharacter` (일반 잡몹 AI)**
  - 일반 몬스터 전용 컴뱃 컴포넌트(`UCBRogueCombatComponent`) 생성 및 컨트롤러를 `ACBRogueController`로 지정

---

### 1.4 로드아웃 데이터 에셋 (`Loadout DataAsset`)
<img src="../img/Loadout/로드아웃클래스.png" width="750">

#### 1) 로드아웃을 도입한 이유
- 캐릭터에 붙는 컴포넌트와 필요 데이터가 늘어날수록, 캐릭터 BP마다 각 컴포넌트의 프로퍼티를 일일이 찾아 세팅하면 누락이 발생하고 관리가 어려워짐.
- **캐릭터가 사용하는 모든 에셋과 데이터를 하나의 로드아웃 데이터 에셋에 모아 등록**하고, 캐릭터 BP는 로드아웃 소프트 참조 단 하나만 갖도록 단순화함.
- 새로운 캐릭터나 몬스터를 추가할 때 코드 수정 없이 로드아웃 데이터 에셋만 구성하면 되며, 캐릭터 계층과 1:1 대칭 구조로 설계해 플레이어 전용 데이터와 AI 전용 데이터가 섞이지 않게 함.

#### 2) 클래스별 보유 데이터

| 로드아웃 클래스 | 대상 캐릭터 | 주요 보유 데이터 |
|---|---|---|
| `UCBCharacterLoadout` | 전 캐릭터 공통 | 무기 데이터 배열(`WeaponDatas`), 액티브·패시브·반응형 어빌리티, 스켈레탈 메시·애님BP·바디 셋업(`FCBBodySetup`), 생성(`SpawnCueTag`)·소멸(`DespawnEffectClass`) 연출, 이동·몽타주 데이터 에셋, 체력바·이름표 위젯, 초기 스탯 GE |
| `UCBChaserLoadout` | `ACBChaserCharacter` | 입력 태그 어빌리티 세트(`InputAbilitySet`), 입력 설정(`InputConfig`), 로컬 HUD 위젯, 피부 메시(`SkinMeshes`)·기본 의상 태그(`DefaultCosmeticIds`)·의상 카탈로그(`CosmeticCatalog`) |
| `UCBAILoadout` | `ACBAICharacter` | AI 비헤이비어 트리(`BehaviorTree`), 처치 공유 경험치 보상(`ExperienceReward`) |
| `UCBOutlawLoadout` | `ACBOutlawCharacter` | 화면 상단 보스 체력바 위젯 클래스(`BossBarWidgetClass`), 보스 전용 BGM(`BossMusic`) |
| `UCBRogueLoadout` | `ACBRogueCharacter` | 일반 잡몹용 로드아웃 (공통 AI 로드아웃 데이터 사용) |

---

### 1.5 로드아웃 적용 과정
<img src="../img/Loadout/로드아웃적용.png" width="750">

비동기 로드된 로드아웃 데이터는 멀티플레이 환경에서의 역할에 따라 **서버 전용 · 서버/로컬 공용 · 로컬 전용** 세 갈래로 나누어 캐릭터에 적용함.

| 구분 | 호출 함수 | 적용 내용 |
|---|---|---|
| **서버에서만 적용** | `Auth_GrantAbilitiesToASC()`<br>`Auth_RegisterWeaponsToCombatComponent()`<br>`Auth_ApplyEffectsToASC()`<br>`Auth_ApplyBehaviorTreeToController()` | • 어빌리티(Active / Passive / Reactive / InputAbilitySet) 부여<br>• 무기 액터 스폰 및 컴뱃 컴포넌트 등록<br>• 초기 스탯 GE 적용<br>• AI 컨트롤러에 비헤이비어 트리 주입 및 처치 경험치 캐싱<br>*(엔진 복제 및 GAS 동기화 경로를 통해 클라이언트로 자동 전파됨)* |
| **서버·로컬 각자 적용** | `ApplyToCharacter()` (동기)<br>`ApplyAsyncToCharacter()` (비동기) | • 스켈레탈 메시, 애님BP, 캡슐·메시 오프셋(`BodySetup`), 생성·소멸 연출 세팅<br>• 이동 데이터(`MovementData`), 몽타주 데이터(`MontageData`), 보스 BGM 주입<br>• 체력바·이름표·보스 바 UI 위젯 설정<br>• 피부 메시 조립 및 기본 의상 파츠 소프트 참조 비동기 로드·적용 |
| **로컬에서만 적용** | `Local_ApplyToCharacter()` | • 직접 조종하는 로컬 플레이어에게 `InputConfig` 바인딩 및 HUD 위젯 생성 |

- **플레이어 로드아웃 부여분 추적 및 선택적 회수 (`Auth_ClearLoadoutGrants`)**: 플레이어 ASC는 `PlayerState`에 유지되므로, 캐릭터를 변경하거나 리스폰할 때 이전 로드아웃이 부여한 어빌리티·초기 스탯 GE 핸들만 정확히 회수하고 인게임에서 획득한 레벨업 카드 버프 GE는 그대로 보존함.

---

### 1.6 캐릭터 초기화 흐름
<img src="../img/Character/캐릭터초기화.png" width="750">

비동기 로드와 네트워크 복제 타이밍 차이로 인해 준비되지 않은 상태에서 컴포넌트나 애니메이션이 먼저 동작하는 문제를 막기 위해, **초기화가 모두 끝난 시점에 `OnCharacterSystemReadyDelegate`를 방송**하는 구조로 통일함.

1. **캐릭터 생성**
   - 캐릭터 스폰 직후 시스템 준비 상태는 `Uninitialized`이며, 모든 확장 컴포넌트(`UCBExtensionComponent`)와 애님 인스턴스는 잠금(`IsSystemLocked`) 상태로 대기함.
2. **서버 · 클라이언트 진입점 분기**
   - **서버**: 컨트롤러 빙의 시점인 `PossessedBy()`에서 초기화 진입
   - **클라이언트**: 플레이어는 `PlayerState` 복제가 보장되는 `OnRep_PlayerState()`에서, AI(시뮬레이티드 프록시)는 `BeginPlay()`에서 초기화 진입
   - `StartSystemInitialization()`을 통해 중복 진입을 방지함.
3. **초기화 작업 (로드아웃 비동기 로드)**
   - **공용 초기화**: ASC·AttributeSet 캐싱 및 `InitAbilityActorInfo` 호출 후 로드아웃 에셋을 비동기 로드(`LoadAssetAsync`)하여 외형·애님BP·데이터 에셋·UI·기본 의상 등 공용 데이터를 적용함.
   - **서버 전용 초기화**: 어빌리티 부여, 초기 스탯 GE 적용, 무기 등록, AI 비헤이비어 트리 주입을 수행함.
   - **클라 전용 초기화**: 로컬 플레이어 캐릭터에 입력 데이터(`InputConfig`)와 HUD를 적용함.
4. **캐릭터 준비 완료 방송 (`HandleCharacterSystemReady`)**
   - 모든 비동기 로드와 데이터 적용이 끝나면 상태를 `Ready`로 전환하고 `OnCharacterSystemReadyDelegate`를 방송함.
   - 이 신호를 받아 **확장 컴포넌트(`OnCharacterSystemReady`), 애님 인스턴스, AI 비헤이비어 트리 구동, 카메라 페이드 해제 및 입력 잠금 해제, 생성 디졸브 연출**이 일제히 시작됨.
