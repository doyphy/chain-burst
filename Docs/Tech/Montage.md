# 몽타주 시스템

> 액션 몽타주를 태그+인덱스로 조회·재생하고, 멀티플레이 전 클라에 동기화하는 시스템.

**몽타주 관리·재생은 `UCBActionMontageData` + `UCBActionComponent` 두 클래스를 짝지어 사용한다.**

- `UCBActionMontageData` (데이터 에셋): 재생 가능한 모든 몽타주를 관리하는 **순수 조회 테이블**. **모든 몽타주는 액션 태그(`Action.*`)에 바인딩**되며, 태그 + 인덱스로 조회한다.
  - 싱글/콤보 구분 없이 **엔트리 하나(`FCBActionMontageEntry`)** 로 통합 — 태그당 몽타주 **배열(`Montages`)** 을 가진다. 1개면 단일 액션, 여러 개면 콤보(순서) 또는 랜덤(변형 풀)이며, **인덱스의 의미(콤보 단계/랜덤)는 데이터가 아니라 호출자가 결정**한다.
  - 대시(`Action.Movement.Dash`)는 전투 상태로 인덱스 분기 — 인덱스 0 = 비전투 대시, 인덱스 1 = 전투 대시 (`UCBGADash::SelectActionMontageIndex`). 방향은 전방 1방향만 사용 — Sprint 루프가 전방 질주뿐이라 대시도 전방으로 통일 (`UCBGADash` 주석 참고).
  - 무기 장착/해제(`Action.Combat.EquipWeapon`/`UnequipWeapon`)는 개이트로 인덱스 분기 — 인덱스 0 = Idle, 1 = Walk, 2 = Run/Sprint 공용 (`UCBAbilitySystemLibrary::GetGaitMontageIndex` 공용 헬퍼 — `Status.Movement.Idle` 파생 상태 태그 + `Gait.*` 태그 순수 조회).
  - 에디터 배열(`MontageEntries`) → 런타임 `TMap`(`MontageMap`) 변환(`UpdateRuntimeMap`).
  - 조회 API: `FindMontage(Tag, Index)`(인덱스 검사 내부 포함), `GetMontageCount(Tag)`.
- `UCBActionComponent` (컴포넌트): **순수 몽타주 재생·정지기.** `RequestPlayMontage(Tag, Index)`로 받은 몽타주를 애님 인스턴스에 재생하고, `StopMontage(BlendOutTime)`로 정지한다. **재생과 정지 모두 GameplayCue가 호출한다**(아래 "정지도 큐를 경유한다"). **콤보 상태·단계 관리는 하지 않는다**(콤보 소유는 `UCBCombatComponent` — [Combat.md](Combat.md)). 데이터 에셋 접근을 캡슐화(`GetMontageCount`)하므로 외부는 데이터 에셋을 직접 참조하지 않는다.

**재생 흐름 (인덱스 기반):**
1. 어빌리티(`UCBActionAbility`)가 재생 **인덱스 결정**: 콤보면 `UCBCombatComponent::AdvanceCombo(Tag, MaxCount)`로 다음 인덱스를 받고(MaxCount=`ActionComp->GetMontageCount(Tag)`), 아니면 `SelectActionMontageIndex()` 훅(기본 0 — 자식이 상태 분기로 재정의 가능, 예: 대시의 전투 상태 0/1 분기).
2. 인덱스를 **GameplayCue 파라미터(`RawMagnitude`)** 에 실어 `GameplayCue.PlayAction` 실행.
3. 각 클라의 `UCBGCN_PlayAction`이 태그·인덱스를 꺼내 `RequestPlayMontage(Tag, Index)` → `UCBActionComponent`가 재생.

**멀티플레이 동기화:** 모든 액션 몽타주는 **GameplayCue**(`GameplayCue.PlayAction` → `UCBGCN_PlayAction`)를 경유해 전 클라 동기화. 서버가 직접 재생하지 않고 큐 파라미터에 **액션 태그 + 콤보 인덱스**를 담아 전달하면, 각 클라의 `UCBActionComponent`가 해당 인덱스의 몽타주를 재생한다. 인덱스를 큐로 전달하므로 Simulated Proxy도 서버와 동일 클립을 재생. (어빌리티가 Simulated Proxy에서 실행되지 않는 문제를 GameplayCue로 해결 — [Multiplayer.md](Multiplayer.md) 참고)

> **이 선택은 2026-09-02에 `PlayMontageAndWait`와 재비교해 "유지"로 재확정했다.** 원래 동기 중 하나였던 "`PlayMontageAndWait`는 몽타주 위치 동기화 때문에 끊긴다"는 전제는 더 이상 성립하지 않는다. 갱신된 유지 근거와 전환을 재검토할 트리거는 아래 **검증 기록** 절에 있다.

**정지도 같은 경로다.** `GameplayCue.StopAction` → `UCBGCN_StopAction` → `ACBBaseCharacter::RequestStopMontage()` → `UCBActionComponent::StopMontage()`. 이유는 아래.

## 정지도 큐를 경유한다 — 루트 모션 때문

액션 몽타주는 애님 노티파이(`Event.Action.EndAbility`)로 **몽타주 중간에서 끊는 것을 전제**로 설계돼 있다. 긴 몽타주의 뒷부분(회수 동작)을 잘라내기 위한 것이다. 이 "끊는 시점"이 머신마다 어긋나면 **루트 모션 이동량이 달라져 캡슐 위치가 어긋나고, CMC가 이를 오차로 보고 클라를 되돌린다** — 화면에서는 캐릭터가 버벅이며 위치 보정되는 것으로 보인다.

**진단의 결정적 단서는 "끝까지 재생하면 오차가 없다"였다.** 즉 엔진의 네트워크 루트 모션 동기화(클라가 보고한 몽타주 트랙 위치로 서버가 리플레이)는 정상 작동하고 있었고, 문제는 **정지 시점**뿐이었다.

### 정지를 어빌리티가 하면 안 되는 이유

| 머신 | 몽타주 재생 | 어빌리티 인스턴스 | 어빌리티가 정지 가능? |
|---|:---:|:---:|:---:|
| 서버 (권위) | O (큐) | O | O |
| 소유 클라 (AutonomousProxy) | O (큐) | O | O |
| **Simulated Proxy** | O (큐) | **X** | **X** |

**몽타주는 큐로 전 머신에서 재생되는데 어빌리티는 서버·소유 클라에만 존재한다.** 정지를 어빌리티가 직접 하면 Simulated Proxy는 멈출 주체가 없어 **그 화면에서만 몽타주가 끝까지 재생된다.** 재생을 큐로 하는 이유가 정지에도 그대로 적용된다.

### 왜 타이밍이 맞나

지연 L(서버↔소유클라), L'(서버↔다른클라)일 때, 노티파이가 걸린 몽타주 위치를 P라 하면:

| 머신 | 몽타주 시작 | 정지 계기 | 정지 시점의 몽타주 위치 |
|---|---|---|---|
| 소유 클라 | `t=0` (예측 재생) | 자기 노티파이 → 예측 큐 | **P** |
| 서버 | `t=L` | 노티파이 이벤트 도달 | **≈P** |
| Simulated Proxy | `t=L+L'` (재생 큐) | 정지 큐 멀티캐스트 | **≈P** |

**재생 큐와 정지 큐가 같은 경로로 같은 지연을 겪으므로 `L'`이 상쇄된다.** 세 머신 모두 같은 몽타주 위치에서 멈추고, 루트 모션 총량이 일치한다.

### 지켜야 할 규칙 넷

**① 블렌드 아웃은 0으로 정지한다.** `Montage_Stop(BlendOutTime)`의 가중 블렌드 아웃 구간은 **실제 시간 기준**으로 루트 모션이 감쇠 추출되는데, 이 구간은 엔진의 몽타주 위치 동기화 밖이라 머신마다 독립 적분된다. 포즈 전환은 ABP의 **관성화**가 담당한다(→ [Locomotion.md](Locomotion.md)).

**② `EndAbility`는 복제하지 않는다 (`bReplicateEndAbility = false`).** 클라가 `true`로 종료하면 `ServerEndAbility` RPC가 나가서 서버 어빌리티를 죽이는데, **그 RPC가 노티파이 이벤트 RPC보다 먼저 발신된다.**

```cpp
// CBAN_SendGameplayEventToOwner::Notify
SendGameplayEventToActor(Owner, EventTag, Payload);   // 이 안에서 클라 OnActionEnded가 통째로 실행 → ServerEndAbility 예약 (1번)
if (!Owner->HasAuthority())
{
    Character->Server_SendGameplayEvent(...);         // 이벤트 RPC 예약 (2번)
}
```

둘 다 Reliable이고 같은 커넥션이라 서버는 이 순서로 처리한다. 서버 어빌리티는 이벤트를 받기 **전에** 죽고(`RemoteEndOrCancelAbility` — `bServerRespectsRemoteAbilityCancellation`이 기본 `true`라 원격 종료가 실제로 먹는다), 이벤트 대기 태스크도 함께 정리되어 **서버는 정지 큐를 영영 멀티캐스트하지 못한다.** 그 화면(시뮬 프록시)에서만 몽타주가 끝까지 재생된다. 경합이 아니라 **구조적으로 매번 서버가 진다.**

> 정지 로직을 GameplayCue로 옮겼으니 이제 `true`여도 되지 않나 — **아니다.** 큐를 쏘는 주체가 여전히 어빌리티이고, 프록시에 멀티캐스트할 수 있는 건 서버 인스턴스뿐이다. 서버 어빌리티가 살아서 자기 `OnActionEnded`에 도달해야 한다.

폴백 경로(`OnDelayFinished`)도 같은 이유로 `false`다. 여기는 정지 큐가 없지만, 클라 폴백이 먼저 터져 서버 어빌리티를 죽이면 **서버의 `TryResetComboOnEnd()`가 실행되지 않아** 콤보 인덱스가 서버에서만 리셋되지 않은 채 남는다.

`true`를 굳이 쓰려면 노티파이의 두 줄 순서를 뒤집어(서버 RPC 먼저, 로컬 이벤트 나중) 서버가 이벤트를 먼저 처리하게 만들어야 한다. 다만 서버는 자기 `OnActionEnded`에서 스스로 종료하므로 **`true`가 얻는 것이 없다** — 액션마다 RPC만 하나 늘어난다.

**③ 정지 큐 실행은 예측 윈도우 안에서 한다.** `OnActionEnded`는 비동기 태스크 콜백이라 예측 키가 보장되지 않는다. 윈도우가 없으면 소유 클라가 큐를 예측 실행하지 못하고 서버 멀티캐스트를 기다리게 되어, 그만큼 클라 몽타주가 더 재생된다. `UCBActionAbility::OnActionEnded`가 `FScopedPredictionWindow`를 명시적으로 연다.

**④ 종료 노티파이는 블렌드 겹침 구간 밖에 둔다.** `UCBAN_SendGameplayEventToOwner`는 `Animation != GetCurrentActiveMontage()`면 이벤트를 버리는데, `GetCurrentActiveMontage()`는 **"가장 최근에 시작된 활성 몽타주"** 다. 몽타주 A가 블렌드 아웃 중에 B가 시작되면 그 겹침 구간에서 **A의 노티파이가 조용히 삼켜진다.** 겹침은 실제 시간 기준이라 삼켜지느냐가 **머신마다 갈릴 수 있고**, 한쪽만 정지하면 위 오차가 그대로 재현된다. 에셋 작성 시 노티파이를 블렌드 범위 밖에 배치할 것.

> ④는 에셋 작성 규칙에 의존하는 방어라 콤보 블렌드 시간을 조정하면 다시 겹칠 수 있다. 재발하면 가드를 **"가장 최근 몽타주인가"가 아니라 "이 노티파이를 낸 몽타주가 중단되지 않았는가"**(`Montage_GetIsStopped`)로 바꾼다.

### 폴백

노티파이가 없거나 삼켜진 경우를 위해 `UCBActionAbility`가 몽타주 길이만큼의 `WaitDelay`를 안전망으로 건다. **단 이 폴백은 어빌리티에 붙어 있어 Simulated Proxy에는 없다** — 정지 큐가 유실되면(`ExecuteGameplayCue` 멀티캐스트는 unreliable) 그 화면에서만 몽타주가 끝까지 재생된다. 위치는 복제로 오므로 시각적 문제에 그친다.

> 폴백 딜레이는 `UAnimMontage::GetPlayLength()`를 쓰는데 이 값은 **배속 미반영**이다. `AttackSpeed`로 재생 속도가 바뀌면 실제 재생 시간과 어긋난다(현재는 항상 길게 잡히는 쪽이라 안전). 폴백이 실질 안전망으로 쓰이게 되면 `/ PlayRate`로 맞출 것.

### 캔슬도 몽타주를 정지시킨다 — 단 서버에서만

어빌리티가 캔슬되면 종료 노티파이(`Event.Action.EndAbility`)를 받지 못하므로 정지 큐를 쏠 기회가 없다. 그대로 두면 **어빌리티만 끝나고 몽타주는 끝까지 재생된다.** 그래서 `UCBActionAbility::EndAbility`가 `bWasCancelled`일 때 정지 큐를 쏜다. 조건 두 가지가 붙는다.

**① 자기가 재생을 시작한 경우에만.** `bActionMontageStarted` 표식으로 가른다. 무기 없음·쿨다운 실패처럼 `PlayActionMontage()` 전에 `EndAbility(..., bWasCancelled = true)`로 빠지는 경로가 있는데, 그때 정지 큐를 쏘면 **남이 재생 중인 몽타주(피격 반응 등)를 꺼버린다.**

**② 서버에서만 (`HasAuthority`).** 캔슬은 서버가 주도하고, "정지 큐 → 다음 몽타주 재생 큐"의 순서는 **서버 멀티캐스트 순서로만** 보장된다. 클라가 각자 캔슬 시점에 정지하면, 캔슬 복제(reliable)가 재생 큐(unreliable 멀티캐스트)보다 늦게 도착했을 때 **이미 시작된 다음 몽타주 — 사망 몽타주 등 — 를 꺼버린다.** 정지 큐 자체가 전 클라에 멀티캐스트되므로 서버만 쏴도 화면은 전부 맞는다.

정상 종료 경로(`OnActionEnded`)는 반대로 예측 윈도우를 열어 클라도 함께 정지한다(규칙 ③). 루트 모션 오차를 줄이는 게 우선이고, 그 시점엔 뒤이어 재생될 몽타주가 없기 때문이다.

### 예외 — 마지막 프레임을 유지하는 액션 (사망)

시체 포즈처럼 **몽타주가 끝난 뒤에도 마지막 프레임을 유지**해야 하는 액션이 있다. 이건 코드와 에셋 **양쪽**을 맞춰야 하고, 한쪽만 하면 조용히 원래대로 돌아간다.

| 막는 것 | 해제 방법 |
|---|---|
| `OnActionEnded`의 정지 큐 (`GameplayCue.StopAction` → `RequestStopMontage`) | 어빌리티가 **`ShouldStopActionOnEnd()`를 `false`로 오버라이드** (`UCBActionAbility`의 훅) |
| 몽타주가 끝에서 스스로 블렌드 아웃 | 몽타주 에셋의 **`Enable Auto Blend Out` 체크 해제** |

두 번째는 엔진 동작이다 — `UAnimMontage::bEnableAutoBlendOut`의 주석대로 *"끝에 도달하면 자동으로 블렌드 아웃하고, false면 블렌드 아웃하지 않고 명시적으로 정지될 때까지 마지막 포즈를 유지"* 한다. 기본값이 `true`라 코드만 고치면 여전히 풀린다.

현재 이 예외를 쓰는 건 `UCBDeathAbility` 하나다 (→ [Abilities.md](Abilities.md)).

> **폴백 경로(`OnDelayFinished`)에는 원래 정지 큐가 없다.** 그래서 사망 몽타주에 종료 노티파이를 안 붙이면 훅 없이도 우연히 동작하지만, 나중에 누가 노티파이를 붙이는 순간 조용히 깨진다. 의도를 훅으로 명시해 둔 이유다.

> **한계 — 늦게 합류한 클라이언트.** 몽타주는 재생 큐를 받은 머신에서만 재생되므로, 죽은 뒤에 관련성을 얻은(멀리서 접근·늦게 접속) 클라이언트는 **서 있는 시체**를 본다. 해결은 `GameplayCue.Death` 노티파이의 `WhileActive`에서 포즈를 잡거나, ABP가 `Status.Dead`를 보고 사망 상태로 전이하는 것이다. 둘 다 미구현.

## 검증 기록 — `PlayMontageAndWait` 재평가 (2026-09-02)

**결론: 큐 방식을 유지한다. 단 유지 근거가 바뀌었다.**

### 무너진 전제 — "PlayMontageAndWait는 끊긴다"

큐 방식을 택한 원래 동기 중 하나는 "`UAbilityTask_PlayMontageAndWait`는 서버와 클라의 몽타주 위치를 동기화하느라 재생이 끊겨 부자연스럽다"였다. **UE 5.8 업그레이드 + ASC 네트워크 업데이트 빈도 상향 이후 패킷 지연·손실을 건 상태에서 재테스트한 결과, 두 방식 모두 끊김 없이 재생됐다.**

엔진 코드상으로도 "항상 끊긴다"가 될 이유는 없다 (`UAbilitySystemComponent::OnRep_ReplicatedAnimMontage`):

- **소유 클라는 애초에 위치 보정 대상이 아니다** — `if (!AbilityActorInfo->IsLocallyControlled())` 가드가 보정 블록 전체를 건너뛴다. 예측 재생한 그대로 자유 재생된다.
- 보정이 걸리는 건 **Simulated Proxy뿐**이고 임계값은 0.1초(`MONTAGE_REP_POS_ERR_THRESH`). 첫 복제 수신 때 서버 위치로 맞춘 뒤에는 양쪽이 같은 속도로 진행하므로 **오차가 누적되지 않는다.** 하드 스냅(`Montage_SetPosition`)이 실제로 터지는 건 지터·패킷 손실·프레임 히칭으로 100ms 이상 어긋날 때다.

**"어빌리티가 Simulated Proxy에서 실행되지 않는다"도 큐의 단독 근거는 아니다.** ASC의 `RepAnimMontageInfo`가 복제되므로 어빌리티 실행 없이도 시뮬 프록시에서 몽타주는 재생된다.

### 그럼에도 큐를 유지하는 이유

| 근거 | 내용 |
|---|---|
| **정지 설계가 이미 검증됨** | 위 "지켜야 할 규칙 넷"은 실제 버그를 밟아가며 확정한 것이라 전환 시 전부 재검증 대상이다. 특히 `UAbilityTask_PlayMontageAndWait::StopPlayingMontage()`는 `ASC->CurrentMontageStop()`을 **인자 없이** 호출해 몽타주 기본 블렌드 아웃 시간을 쓴다 — **규칙 ①(블렌드 아웃 0) 위반.** |
| **다수 AI 대역폭** | 서버 ASC는 몽타주 재생 중 `GetShouldTick()`이 참이라 **매 틱** `AnimMontage_UpdateReplicatedData()`로 위치를 더티 마킹한다. 재생 중인 캐릭터마다 NetUpdateFrequency만큼 계속 구조체가 나간다. 큐는 **액션당 1회**다. |
| **AI는 전환 이득이 없다** | `UCBAIAttackAbility`는 `ServerOnly`라 예측 자체가 없다 → 예측 거부 롤백 이득 0. |

즉 유지 근거는 이제 **"부드러움"이 아니라 "검증된 정지 설계 + 대역폭"**이다.

### 큐 방식이 지고 있는 빚 (전환을 재검토할 트리거)

`ExecuteGameplayCue`의 멀티캐스트는 **unreliable**(`NetMulticast_InvokeGameplayCueExecuted_WithParams`)이고, 큐로 넘기는 건 몽타주 에셋이 아니라 **태그 + 인덱스**다. 여기서 오는 약점 셋 중 하나라도 실제로 관측되면 전환을 다시 검토한다.

1. **원격 클라에서 액션 모션이 통째로 빠진다** — 재생 큐 유실. 복구 경로가 없다(정지 큐 유실은 시각적 문제에 그치지만, 재생 큐 유실은 모션 자체가 증발한다).
2. **다른 클립이 재생된다** — 콤보 인덱스·데이터 에셋 순서 불일치. 몽타주 에셋을 직접 복제하는 `PlayMontageAndWait`에는 없는 문제.
3. **예측 거부가 잦아진다**(전용 서버 전환, 고핑 매칭) — `ExecuteGameplayCue`는 예측 거부 시 롤백되지 않아 소유 클라가 거부된 모션을 끝까지 재생한다. `PlayMontageAndWait`는 `OnPredictiveMontageRejected`로 자동 중단된다.

> 1번은 드문 확률 이벤트라 눈대중 테스트로는 안 잡힌다. 수치로 확인하려면 `UCBGCN_PlayAction`에 클라별 실행 카운터를 찍고 서버 실행 횟수와 대조할 것.

## 모션 워핑 — 몽타주 이동을 목표에 맞추기

몽타주가 도는 동안 캐릭터를 특정 지점/방향으로 정밀하게 이동·회전시키는 정식 경로. 액터 회전을 직접 덮어쓰는 방식과 달리 **루트모션 델타를 월드 변환 직전에 보정**하므로 회전 잠금 등 기존 처리와 간섭하지 않는다.

### 전제는 "이동량이 있는 클립"이 아니라 "루트 모션 활성화"

워핑은 이 경로로만 돈다.

```
CMC 루트모션 처리
  └─ UMotionWarpingCharacterAdapter::WarpLocalRootMotionOnCharacter
       ├─ Character->GetRootMotionAnimMontageInstance()   ← 루트모션 몽타주가 아니면 여기서 끝
       └─ UMotionWarpingComponent::ProcessRootMotionPreConvertToWorld
```

몽타주의 **`Enable Root Motion` 이 꺼져 있으면 훅 자체가 호출되지 않는다.** 워프 타겟을 등록해도 조용히 무시된다.

**반대로, 제자리 클립이어도 루트 모션만 켜져 있으면 워핑이 동작한다.** `URootMotionModifier_SkewWarp` 에 "애니메이션에 이동이 없으면 만들어 넣는" 분기가 따로 있기 때문이다.

| 클립 | 동작 | 관련 프로퍼티 |
|---|---|---|
| 이동량 있음 | 원본 궤적을 목표에 맞게 왜곡 | `MaxSpeedClampRatio` (원본 속도 대비 상한) |
| **이동량 없음(제자리)** | 시작 위치 → 타겟을 이징 보간해 델타를 **합성** | `Add Translation Easing Func` / `Easing Curve` |

즉 "제자리 공격 몽타주에 루트 모션만 켜서 타겟 앞으로 붙이기"가 정상 사용법이다.

### 배선 — 워프 타겟은 큐 경로로 전달된다

```
어빌리티: BuildActionCueParameters() 에서 Location / Normal 채움
   ▼ (GameplayCue.PlayAction)
UCBGCN_PlayAction: Location 이 0 이 아니면
   AddOrUpdateWarpTargetFromLocationAndRotation(ActionTag.GetTagName(), Location, Normal.Rotation())
   → 그 직후 같은 프레임에 몽타주 재생
   ▼
몽타주의 Motion Warping AnimNotifyState (Warp Target Name = 액션 태그 문자열)
```

- **워프 타겟 이름 = 액션 태그 문자열**이다. 노티파이의 Warp Target Name 이 태그와 다르면 타겟만 등록되고 아무 일도 안 일어난다.

**두 가지 등록 방식**이 있고 큐가 파라미터를 보고 고른다.

| 파라미터 | 등록 방식 | 쓰는 곳 |
|---|---|---|
| `TargetAttachComponent` | **추종** — `AddOrUpdateWarpTargetFromComponent(bFollowComponent = true)`. 워프가 도는 동안 매 프레임 타겟 트랜스폼을 다시 읽는다 | AI 공격 (움직이는 플레이어 추적) |
| `Location` / `Normal` | **스냅샷** — 재생 시점 좌표로 고정 | 대시 |

- 추종 방식이 **좌표를 다시 쏘는 것보다 나은 이유**: 서버가 주기적으로 좌표를 갱신하면 클라이언트는 그 사이를 못 따라가 어긋난다. 대상만 넘기면 각 머신이 복제된 타겟에서 직접 읽으므로 추가 통신 없이 일치한다.
- 추종 시 `NormalizedMagnitude` 에 **멈출 거리(cm)** 를 실어 보내고, 큐가 `VectorFromTargetToOwner` 오프셋으로 넘긴다 — 타겟을 관통하지 않고 사거리 앞에서 멈춘다. (`RawMagnitude` 는 콤보 인덱스가 이미 쓰고 있음)
- 엔진 주의: `bFollowComponent` 는 **오너가 타겟보다 먼저 틱하면 한 프레임 늦는다.** 정밀하게 붙여야 하면 tick prerequisite 를 걸 것.
- **워프 구간이 "얼마나 따라갈지"를 결정한다.** 몽타주 전 구간에 노티파이를 깔면 홈잉 미사일이 되어 회피가 불가능해진다. 선딜 구간에만 두면 타격 프레임 전에 워프가 끝나 살짝만 따라간다.

### 이동 거리 상한 — `UCBRootMotionModifier_ClampedSkewWarp`

추종 워프에는 함정이 하나 있다. 엔진 SkewWarp 의 `MaxSpeedClampRatio` 는 주석 그대로 **"애니메이션에 루트모션 이동이 있을 때만"** 적용된다. 제자리 클립은 이동을 합성하는 경로를 타는데 **거기엔 상한이 없어서**, 워프 도중 타겟이 순간이동하면(플레이어 대시 등) AI 가 그 거리만큼 그대로 끌려간다 — 피할 수 없는 추적기가 된다.

그래서 `URootMotionModifier_SkewWarp` 를 파생해 `Update()` 에서 목표 지점을 잘라낸다.

- 기준은 **워프가 시작된 지점**(`StartTransform`)이지 캐릭터의 현재 위치가 아니다. 현재 위치 기준으로 자르면 매 프레임 상한이 다시 늘어나 결국 끝까지 따라간다.
- 방향은 유지하고 거리만 자르므로 회전 워프가 바라보는 방향은 사실상 그대로다 — **타겟을 향한 채 갈 수 있는 만큼만 가고 헛친다.** 대시로 빠져나간 플레이어에게는 이게 올바른 결과다.
- `MaxWarpDistance`(기본 200cm)를 0 이하로 두면 엔진 기본 동작(무제한)으로 되돌아간다.
- 몽타주 노티파이의 Root Motion Modifier 를 이 클래스로 바꾸기만 하면 되고, 큐·어빌리티 배선은 손대지 않는다.

**대안이었던 방식**: 클립에 전진 루트모션을 조금 베이크하면 warp 경로로 전환돼 엔진의 `MaxSpeedClampRatio` 가 살아난다(코드 0). 다만 상한이 "원본 애님 속도의 배수"라 간접적이고 클립마다 작업이 필요해, cm 로 직접 제한하는 쪽을 택했다.
- 큐 경로라 서버·소유 클라·Simulated Proxy 가 **동일한 워프 타겟**을 받는다. 각자 계산하지 않는다.
- `Location` 을 안 채우는 액션은 그냥 건너뛴다(무해). 새 액션에 워프를 붙이려면 그 어빌리티가 `BuildActionCueParameters()` 만 채우면 되고 큐는 손댈 필요 없다.

### 주의

- **발 미끄러짐** — 제자리 클립 + 합성 이동은 거리가 길수록 미끄러져 보인다. 워프 구간을 짧게 잡거나 거리 상한을 둘 것. 사거리 보정 정도의 짧은 거리는 티가 안 난다.
- 루트모션 재생 중 회전 잠금과 **공존한다**. 잠금은 액터 회전을 안 건드리는 쪽으로 막을 뿐이고, 워핑은 루트모션 델타를 보정하는 별개 파이프라인이다. → [Locomotion.md](Locomotion.md)
- 컴포넌트는 `ACBBaseCharacter` 가 소유한다(`CBMotionWarpingComponent`).

## 관련 문서
- 콤보 인덱스 전진/리셋 소유: [Combat.md](Combat.md)
- 몽타주 재생을 트리거하는 어빌리티: [Abilities.md](Abilities.md)
- GameplayCue로 Simulated Proxy 동기화하는 이유: [Multiplayer.md](Multiplayer.md)
- 몽타주 정지 후 포즈 전환(관성화), 루트모션 접근 시 캐릭터 간 밀림 규칙: [Locomotion.md](Locomotion.md)
