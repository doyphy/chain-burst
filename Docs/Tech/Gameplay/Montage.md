# 몽타주 시스템

> 액션 몽타주를 태그+인덱스로 조회·재생하고, 멀티플레이 전 클라에 동기화하는 시스템.

**몽타주 관리·재생은 `UCBActionMontageData` + `UCBActionComponent` 두 클래스를 짝지어 사용한다.**

- `UCBActionMontageData` (데이터 에셋): 재생 가능한 모든 몽타주를 관리하는 **순수 조회 테이블**. **모든 몽타주는 액션 태그(`Action.*`)에 바인딩**되며, 태그 + 인덱스로 조회한다.
  - 싱글/콤보 구분 없이 **엔트리 하나(`FCBActionMontageEntry`)** 로 통합 — 태그당 몽타주 **배열(`Montages`)** 을 가진다. 1개면 단일 액션, 여러 개면 콤보(순서) 또는 랜덤(변형 풀)이며, **인덱스의 의미(콤보 단계/랜덤)는 데이터가 아니라 호출자가 결정**한다.
  - 대시(`Action.Movement.Dash`)는 전투 상태로 인덱스 분기 — 인덱스 0 = 비전투 대시, 인덱스 1 = 전투 대시 (`UCBGADash::SelectActionMontageIndex`). 몽타주는 전방 1방향만 사용 — Sprint 루프가 전방 질주뿐이라 대시도 전방으로 통일하고, 실제 이동 방향은 모션 워핑이 카메라 기준 입력 방향으로 돌린다 (→ [Locomotion.md](Locomotion.md)).
  - 무기 장착/해제(`Action.Combat.EquipWeapon`/`UnequipWeapon`)는 개이트로 인덱스 분기 — 인덱스 0 = Idle, 1 = Walk, 2 = Run/Sprint 공용 (`UCBAbilitySystemLibrary::GetGaitMontageIndex` 공용 헬퍼 — `Status.Movement.Idle` 파생 상태 태그 + `Gait.*` 태그 순수 조회).
  - 에디터 배열(`MontageEntries`) → 런타임 `TMap`(`MontageMap`) 변환(`UpdateRuntimeMap`).
  - 조회 API: `FindMontage(Tag, Index)`(인덱스 검사 내부 포함), `GetMontageCount(Tag)`.
- `UCBActionComponent` (컴포넌트): **순수 몽타주 재생·정지기.** `RequestPlayMontage(Tag, Index)`로 받은 몽타주를 애님 인스턴스에 재생하고, `StopMontage(BlendOutTime)`로 정지하며, `JumpToSection(Montage, SectionIndex)`로 섹션을 넘긴다. **재생·정지·섹션 점프 모두 GameplayCue가 호출한다**(아래 "정지도 큐를 경유한다"). **콤보 상태·단계 관리는 하지 않는다**(콤보 소유는 `UCBCombatComponent` — [Combat.md](Combat.md)). 데이터 에셋 접근을 캡슐화(`GetMontageCount`)하므로 외부는 데이터 에셋을 직접 참조하지 않는다. **방금 재생한 몽타주 인스턴스 ID**(`GetLastMontageInstanceID`)를 기록해 두고, 어빌리티는 이 ID로 자기 몽타주의 블렌드 아웃을 기다린다(아래 "어빌리티는 자기 몽타주의 블렌드 아웃 시작에 끝난다"). 이 ID는 각 머신이 재생할 때 직접 기록하는 로컬 값이라 복제하지 않는다.

**재생 흐름 (인덱스 기반):**
1. 어빌리티(`UCBActionAbility`)가 재생 **인덱스 결정**: 콤보면 `UCBCombatComponent::AdvanceCombo(Tag, MaxCount)`로 다음 인덱스를 받고(MaxCount=`ActionComp->GetMontageCount(Tag)`), 아니면 `SelectActionMontageIndex()` 훅(기본 0 — 자식이 상태 분기로 재정의 가능, 예: 대시의 전투 상태 0/1 분기).
2. 인덱스를 **GameplayCue 파라미터(`RawMagnitude`)** 에 실어 `GameplayCue.PlayAction` 실행.
3. 각 클라의 `UCBGCN_PlayAction`이 태그·인덱스를 꺼내 `RequestPlayMontage(Tag, Index)` → `UCBActionComponent`가 재생.

**멀티플레이 동기화:** 모든 액션 몽타주는 **GameplayCue**(`GameplayCue.PlayAction` → `UCBGCN_PlayAction`)를 경유해 전 클라 동기화. 서버가 직접 재생하지 않고 큐 파라미터에 **액션 태그 + 콤보 인덱스**를 담아 전달하면, 각 클라의 `UCBActionComponent`가 해당 인덱스의 몽타주를 재생한다. 인덱스를 큐로 전달하므로 Simulated Proxy도 서버와 동일 클립을 재생. (어빌리티가 Simulated Proxy에서 실행되지 않는 문제를 GameplayCue로 해결 — [Multiplayer.md](../Conventions/Multiplayer.md) 참고)

> **이 선택은 2026-09-02에 `PlayMontageAndWait`와 재비교해 "유지"로 재확정했다.** 원래 동기 중 하나였던 "`PlayMontageAndWait`는 몽타주 위치 동기화 때문에 끊긴다"는 전제는 더 이상 성립하지 않는다. 갱신된 유지 근거와 전환을 재검토할 트리거는 아래 **검증 기록** 절에 있다.

**정지도 같은 경로다.** `GameplayCue.StopAction` → `UCBGCN_StopAction` → `ACBBaseCharacter::RequestStopMontage()` → `UCBActionComponent::StopMontage()`. 이유는 아래. **섹션 점프도 같은 경로다** — `GameplayCue.JumpActionSection` → `UCBGCN_JumpActionSection` → `RequestJumpToSection()` → `UCBActionComponent::JumpToSection()` (아래 "섹션 점프").

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

**① 정지 시점을 맞춘다 — 블렌드 아웃 시간은 루트 모션과 무관하다.** 루트 모션 오차를 가르는 것은 **어느 몽타주 위치에서 멈추느냐**이지 블렌드 아웃 길이가 아니다. 현재 정지 큐는 0.1초로 블렌드 아웃한다(`UCBGCN_StopAction` → `RequestStopMontage(0.1f)`).

- **정지하는 순간 그 인스턴스는 루트 모션 몽타주에서 빠진다.** `FAnimMontageInstance::Stop` → `UAnimInstance::OnMontageInstanceStopped` → `ClearMontageInstanceReferences`가 `RootMotionMontageInstance`를 비우고, `Montage_Advance`는 `RootMotionFromMontagesOnly` 모드에서 **`GetRootMotionMontageInstance()`와 같은 인스턴스에서만** 루트 모션을 추출한다(`AnimInstance.cpp` `Montage_Advance`). 그래서 블렌드 아웃 중인 몽타주는 포즈만 섞일 뿐 이동량은 0이다.
- 따라서 블렌드 아웃 시간은 **포즈 연출만** 결정한다. 짧으면 전환이 딱딱하고 길면 부드럽다 — 게임플레이·네트워크 관점의 제약은 없다.
- 모든 ABP가 엔진 기본값 `RootMotionFromMontagesOnly`를 쓴다(2026-09-25 에셋 확인, `RootMotionMode`를 바꾼 ABP 없음). ⚠️ **`RootMotionFromEverything`으로 바꾸면 이 전제가 깨진다** — 그 모드는 블렌드 가중치를 곱한 루트 모션을 모든 인스턴스에서 추출하므로(`bBlendRootMotion`), 블렌드 아웃 구간의 이동량이 실제 시간 기준 가중치로 머신마다 따로 적분된다. 그때는 블렌드 아웃을 0에 가깝게 줄여야 한다.

> 예전 문서는 "블렌드 아웃은 0으로 정지한다"였고 근거가 "블렌드 아웃 구간에서 루트 모션이 감쇠 추출된다"였다. 위 엔진 동작상 현재 모드에서는 성립하지 않아 2026-09-25에 정정했다. 코드는 처음부터 0.1초였다.

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

블렌드 아웃 종료 경로(`OnActionMontageBlendingOut`)도 같은 이유로 `false`다. 여기는 정지 큐가 없지만, 클라가 먼저 끝나 서버 어빌리티를 죽이면 **서버의 `CleanupActionState()`(콤보 리셋)가 실행되지 않아** 콤보 인덱스가 서버에서만 리셋되지 않은 채 남는다. 각 머신은 자기 몽타주의 블렌드 아웃으로 스스로 끝난다.

`true`를 굳이 쓰려면 노티파이의 두 줄 순서를 뒤집어(서버 RPC 먼저, 로컬 이벤트 나중) 서버가 이벤트를 먼저 처리하게 만들어야 한다. 다만 서버는 자기 `OnActionEnded`에서 스스로 종료하므로 **`true`가 얻는 것이 없다** — 액션마다 RPC만 하나 늘어난다.

**③ 정지 큐 실행은 예측 윈도우 안에서 한다.** `OnActionEnded`는 비동기 태스크 콜백이라 예측 키가 보장되지 않는다. 윈도우가 없으면 소유 클라가 큐를 예측 실행하지 못하고 서버 멀티캐스트를 기다리게 되어, 그만큼 클라 몽타주가 더 재생된다. `UCBActionAbility::OnActionEnded`가 `FScopedPredictionWindow`를 명시적으로 연다.

**④ 종료 노티파이는 블렌드 겹침 구간 밖에 둔다.** `UCBAN_SendGameplayEventToOwner`는 `Animation != GetCurrentActiveMontage()`면 이벤트를 버리는데, `GetCurrentActiveMontage()`는 **"가장 최근에 시작된 활성 몽타주"** 다. 몽타주 A가 블렌드 아웃 중에 B가 시작되면 그 겹침 구간에서 **A의 노티파이가 조용히 삼켜진다.** 겹침은 실제 시간 기준이라 삼켜지느냐가 **머신마다 갈릴 수 있고**, 한쪽만 정지하면 위 오차가 그대로 재현된다. 에셋 작성 시 노티파이를 블렌드 범위 밖에 배치할 것.

> ④는 에셋 작성 규칙에 의존하는 방어라 콤보 블렌드 시간을 조정하면 다시 겹칠 수 있다. 재발하면 가드를 **"가장 최근 몽타주인가"가 아니라 "이 노티파이를 낸 몽타주가 중단되지 않았는가"**(`Montage_GetIsStopped`)로 바꾼다.

> ④가 깨져 한쪽 머신에서 노티파이가 삼켜져도 그 머신의 어빌리티는 아래 블렌드 아웃 경로로 끝나므로 **멈춰 서지는 않는다.** 다만 그 머신만 정지 큐 없이 끝까지 재생되므로 루트 모션 오차는 그대로 남는다 — ④는 여전히 지켜야 한다.

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

현재 이 예외를 쓰는 건 `UCBDeathAbility` 하나다 (→ [Abilities.md](Abilities.md)). 에셋은 사망 몽타주 전부(Chaser 무기별 8개 + `AM_Rogue_Dog_Death`)가 Auto Blend Out을 끈 상태다(2026-09-25 확인).

**어빌리티는 재생 직후 끝난다.** Auto Blend Out이 꺼진 몽타주는 블렌드 아웃도 완전 종료도 일어나지 않으므로, 이벤트를 기다리면 어빌리티가 영영 끝나지 않는다. 플레이어는 ASC가 PlayerState 소유라 어빌리티 인스턴스가 리스폰을 넘어 살아남아, **두 번째 사망 때 사망 어빌리티가 "이미 활성"이라 발동하지 않게 된다.** 그래서 `PlayActionMontage()`가 재생한 인스턴스의 `bEnableAutoBlendOut`을 보고 꺼져 있으면 곧바로 정상 종료한다. 몽타주는 큐로 재생되어 어빌리티와 무관하게 마지막 포즈를 유지한다.

- 판정 기준을 `ShouldStopActionOnEnd()`가 아니라 **에셋 플래그**로 둔 이유: 이벤트가 안 오는 원인이 에셋 플래그이기 때문이다. 훅만 켜고 에셋을 빠뜨린 경우는 몽타주가 블렌드 아웃되어 정상 경로로 끝나고, 에셋만 끈 경우도 어빌리티가 멈춰 서지 않는다.
- 정상 종료라 정지 큐는 원래 나가지 않는다. 훅(`ShouldStopActionOnEnd() == false`)은 **나중에 누가 종료 노티파이를 붙여도** 시체 포즈가 풀리지 않게 의도를 명시해 둔 것이다.

> **한계 — 늦게 합류한 클라이언트.** 몽타주는 재생 큐를 받은 머신에서만 재생되므로, 죽은 뒤에 관련성을 얻은(멀리서 접근·늦게 접속) 클라이언트는 **서 있는 시체**를 본다. 해결은 `GameplayCue.Death` 노티파이의 `WhileActive`에서 포즈를 잡거나, ABP가 `Status.Dead`를 보고 사망 상태로 전이하는 것이다. 둘 다 미구현.

## 섹션 점프 — 서버가 정하고 큐로 따라간다

재생 중인 액션 몽타주를 다른 섹션으로 넘긴다. 현재 쓰는 곳은 돌진 워프(`Constant Speed Warp`)의 "도착·막힘 시 정지 섹션으로"(`StopSectionName`)다.

```
[서버] 요청자 (예: Constant Speed Warp 가 멈춘 순간, 다음 틱)
   ▼ Event.Action.JumpSection   (OptionalObject = 몽타주, EventMagnitude = 섹션 인덱스)
[서버] UCBActionAbility::OnActionSectionJumpRequested   (PlayActionMontage 가 재생 중 대기 등록)
   ▼ GameplayCue.JumpActionSection   (SourceObject = 몽타주, RawMagnitude = 섹션 인덱스)
[전 머신] UCBGCN_JumpActionSection → ACBBaseCharacter::RequestJumpToSection → UCBActionComponent::JumpToSection
```

- **점프는 모든 머신에서, 요청은 한 머신에서.** 재생·정지와 같은 이유로 점프는 큐로 전 머신이 한다(Simulated Proxy 에는 어빌리티가 없다). 어느 머신이 요청하느냐는 **요청자가 정한다** — 돌진 워프는 도착·막힘 판정이 게임플레이 결과라 **서버 권위**로만 요청하고, 어빌리티는 예측 윈도우를 열지 않는다.
- **타이밍이 맞는 이유는 정지 큐와 같다.** 재생 큐와 점프 큐가 같은 경로·같은 지연을 겪으므로, Simulated Proxy 는 서버가 점프한 것과 거의 같은 몽타주 위치에서 점프한다.
- **섹션은 인덱스로 보낸다.** 게임플레이 이벤트·큐 파라미터에 `FName` 필드가 없다. 전 머신이 같은 몽타주 에셋을 재생하므로 인덱스가 같다. 이름 → 인덱스 변환은 요청자가 하고, 못 찾으면 경고 후 요청하지 않는다.
- **받는 쪽의 무시 조건 두 가지** (`JumpToSection`)
  - **그 몽타주가 이 머신에서 재생 중이 아니면** — 큐가 오기 전에 피격 등 다른 몽타주로 바뀐 머신에서 엉뚱한 몽타주를 같은 인덱스로 점프시키지 않도록. 그래서 큐가 몽타주를 함께 싣는다(`SourceObject` 는 복제된다).
  - **이미 그 섹션 시작을 지났으면** — 큐가 늦게 도착하는 사이 구간이 자연히 끝나 넘어간 머신을 되감지 않도록. 섹션 이름 비교가 아니라 **위치 비교**인 이유는, 이미 정지 섹션도 지나 다음 섹션에 있는 머신을 이름 비교로는 거를 수 없기 때문이다. 그래서 **점프 대상 섹션은 항상 요청 시점보다 뒤에 있어야 한다**(앞으로만 점프).
- **요청은 다음 틱에 보낸다.** 돌진 워프는 CMC 이동 처리 도중(`ProcessRootMotion`)에 멈춤을 감지한다. 그 자리에서 이벤트 → 어빌리티 → 큐 → 점프가 연쇄 실행되면 루트모션 적용 도중에 몽타주 위치가 바뀌므로, 타이머(`SetTimerForNextTick`)로 한 틱 미룬다. 이동은 감지 즉시 멈추므로 늦는 것은 섹션 전환 한 틱뿐이다.
- **같은 몽타주 인스턴스 안의 이동**이라 블렌드 아웃 대기·종료 노티파이 경로는 그대로다. 정지 섹션이 끝나면 평소처럼 블렌드 아웃되어 어빌리티가 정상 종료된다.
- **에셋**: 큐는 에셋으로 등록돼야 동작한다 — `Content/ChainBurst/GameplayCues/GCN_JumpActionSection` (부모 `UCBGCN_JumpActionSection`, Gameplay Cue Tag = `GameplayCue.JumpActionSection`). 없으면 에러 없이 점프만 안 된다.

**한계**
- **플레이어 몽타주에 쓰면 소유 클라가 지연만큼 늦게 점프한다.** 판정이 서버 권위라 소유 클라는 서버 큐를 기다린다. 그동안 돌진 섹션을 더 재생하므로(이동은 자기 모디파이어가 이미 멈춤) 루트모션 트랙 위치가 어긋나 보정이 날 수 있다. 예측 점프는 클라·서버의 막힘 판정이 갈릴 수 있어(상대 플레이어 위치가 지연만큼 다름) 택하지 않았다.
- 점프 큐도 재생·정지 큐처럼 **unreliable 멀티캐스트**다. 유실되면 그 화면에서만 섹션이 넘어가지 않는다. 위치는 서버 복제로 맞춰지므로 연출상의 문제에 그친다.

## 어빌리티는 자기 몽타주의 블렌드 아웃 시작에 끝난다

노티파이가 없는 액션, 노티파이가 삼켜진 경우, 다른 몽타주에 끊긴 경우를 모두 **몽타주 인스턴스의 블렌드 아웃 시작**으로 처리한다. 예전에는 몽타주 길이만큼의 `WaitDelay`를 폴백으로 걸었는데, `GetPlayLength()`가 배속(`AttackSpeed`)과 에셋 `RateScale`을 반영하지 않아 **공격속도가 빠를수록 몽타주가 끝난 뒤에도 어빌리티가 남아 다음 행동을 막았다.** 딜레이를 계산하는 대신 실제 재생을 따라가도록 바꿨다.

| 종료 경로 | 계기 | 정지 큐 | `EndAbility` |
|---|---|---|---|
| 종료 노티파이 | `Event.Action.EndAbility` | O (예측 윈도우) | 정상 |
| **자연 블렌드 아웃** | 자기 인스턴스 블렌드 아웃 시작, `bInterrupted = false` | **X** | 정상 |
| **끊김** | 자기 인스턴스 블렌드 아웃 시작, `bInterrupted = true` | **X** | 캔슬 |
| 캔슬 | 외부 캔슬 | 서버·자기 몽타주 생존 시 | 캔슬 |
| 재생 실패 | 인스턴스 ID 없음 (경고 로그) | X | 정상 |
| 마지막 프레임 유지 | 인스턴스의 `bEnableAutoBlendOut == false` | X | 재생 직후 정상 |

### 왜 "완전 종료"가 아니라 "블렌드 아웃 시작"인가

- **블렌드 아웃은 다음 동작이 섞여 들어오는 구간이다.** 어빌리티가 살아 있으면 `BlockAbilitiesWithTag`·재발동 불가로 그 구간 내내 다음 행동이 막혀 입력이 먹히지 않는 시간이 된다. 완전 종료를 기다리면 A → 로코모션 → B 로 한 번 원래 포즈로 돌아갔다가 이어진다.
- **끊긴 경우** 완전 종료(`OnMontageEnded`)는 새 몽타주의 블렌드 인이 끝난 뒤에야 오므로 두 어빌리티의 태그가 겹친다. 블렌드 아웃 이벤트는 끊기는 즉시 온다.
- 엔진 `PlayMontageAndWait`도 블렌드 아웃 시작에 ASC의 "애니메이션 중인 어빌리티"를 해제한다(`ClearAnimatingAbility`).

### 자기 몽타주만 구분하는 방법 — 인스턴스 ID + 태스크 수명

- **전역 델리게이트(`UAnimInstance::OnMontageBlendingOut`)는 쓰지 않는다.** 몽타주 에셋 포인터만 넘어와서 같은 몽타주를 연속 재생하면(피격 재발동, 콤보 없는 연타, AI 무작위 변형의 같은 인덱스) 구분이 안 된다. 종료 이벤트는 큐에 쌓였다 나중에 발송되므로(`AnimMontage.cpp` `QueueMontageBlendingOutEvent`) **새 재생이 시작된 뒤에 이전 재생의 이벤트가 도착**해 새 활성화가 곧바로 끝난다.
- **인스턴스별 델리게이트**(`FAnimMontageInstance::OnMontageBlendingOutStarted`)를 쓴다. 재생마다 새 인스턴스와 고유 ID가 생기므로(`Montage_PlayInternal`) 재생 1회에만 묶인다. `UCBActionComponent`가 기록한 ID로 `GetMontageInstanceForID()` 해서 바인딩한다.
- **바인딩은 어빌리티가 아니라 태스크(`UCBAbilityTask_WaitMontageBlendOut`)에 둔다.** 어빌리티는 `InstancedPerActor`라 콤보 다음 타가 같은 객체를 재사용하므로, 어빌리티에 바인딩하면 이전 인스턴스의 끊김 이벤트가 새 활성화로 들어온다. 태스크는 활성화마다 새로 생기고, 끝날 때 인스턴스에서 해제하며, 파괴되면 가비지로 표시되어(`UGameplayTask::OnDestroy`) 큐에 복사된 델리게이트도 호출되지 않는다. 엔진 `PlayMontageAndWait`와 같은 방식이다.
- **어빌리티가 재생 직후 ID를 읽을 수 있는 이유**: 어빌리티 활성화는 큐 전송 컨텍스트(`FScopedGameplayCueSendContext`) 밖이라 `ExecuteGameplayCue`가 그 자리에서 flush된다(`GameplayCueManager.cpp` `AddPendingCueExecuteInternal`). 서버는 멀티캐스트가 자기 머신에서도 즉시 실행되고, 소유 클라는 예측 실행된다.

### 지켜야 할 것

- **자기가 몽타주를 멈추기 전에 대기부터 끊는다.** 애님 업데이트 밖에서 몽타주가 정지되면 블렌드 아웃 이벤트가 큐를 거치지 않고 **즉시(동기) 호출된다**(`UAnimInstance::QueueMontageBlendingOutEvent`). 대기가 살아 있으면 노티파이 종료·캔슬 경로의 자기 정지가 "끊김"으로 되돌아와 정상 종료가 캔슬로 바뀐다. `StopActionMontage()`가 맨 앞에서 `BlendOutTask`를 끝낸다.
- **끊겼을 때는 정지 큐를 쏘지 않는다.** 끊김 이벤트는 **새 몽타주의 재생 호출 안에서** 들어온다. 이때 캔슬 경로가 정지 큐를 쏘면 `Montage_Stop`이 방금 시작된 새 몽타주를 끈다. 그래서 `bActionMontageStarted`를 먼저 내리고 캔슬로 끝낸다.
- **루프 섹션 몽타주는 블렌드 아웃이 오지 않는다.** 마지막 섹션에 도달하지 않으므로 어빌리티가 끝나지 않는다. 현재 액션 몽타주에는 루프 섹션이 없다(2026-09-25 에셋 확인). 루프 액션을 만들면 노티파이나 입력으로 끝내야 한다.
- **서버 애니메이션 틱에 의존한다.** 서버에서 몽타주가 진행돼야 이벤트가 온다. 트레이스·종료 노티파이도 같은 전제라 새 의존은 아니다. 메시의 `VisibilityBasedAnimTickOption` 이 몽타주를 멈추는 값(`OnlyTickPoseWhenRendered`)이면 어빌리티가 끝나지 않는다. 현재 설정은 아래와 같다.

  | 메시 | 설정 | 렌더링 안 될 때 |
  |---|---|---|
  | `ACharacter` 기본 (Chaser 본체) | `AlwaysTickPose` (엔진 `Character.cpp`) | 포즈·몽타주는 진행, **본 트랜스폼은 갱신 안 함** |
  | Chaser 리더 메시를 숨길 때 | `AlwaysTickPoseAndRefreshBones` (`UCBModularMeshComponent`) | 전부 갱신 |
  | AI (`ACBAICharacter`) | `OnlyTickMontagesAndRefreshBonesWhenPlayingMontages` | 몽타주 중에만 몽타주 진행 + 본 갱신, 그 외엔 애님 그래프 업데이트 생략 |

- **함정 — 노티파이는 오는데 소켓은 멈춰 있다.** 엔진 기본 `AlwaysTickPose` 는 본 트랜스폼을 **렌더링될 때만** 갱신한다(`USkinnedMeshComponent::ShouldUpdateTransform` — `bRecentlyRendered`). 서버가 그 캐릭터를 화면에 그리지 않으면(클라 창에서 테스트, 호스트 카메라가 다른 곳을 봄) 몽타주·노티파이·루트모션(모션 워핑 회전 포함)은 정상인데 **`GetSocketLocation` 은 마지막으로 그린 포즈를 돌려준다.** 몸이 도는 만큼은 따라가지만 몸 기준 위치는 고정이라, 투사체가 늘 첫 발 자리에서 나가고 근접 트레이스가 멈춘 팔로 훑는다.
  - AI 는 공격 판정(무기 트레이스·영역·투사체 발사)을 **서버에서** 하므로 `ACBAICharacter` 생성자에서 위 설정으로 바꿨다. 공격은 전부 몽타주라 몽타주 중 갱신이면 충분하고, `AlwaysTickPoseAndRefreshBones` 와 달리 대기·이동 중인 보이지 않는 잡몹에게는 비용을 쓰지 않는다.
  - Chaser 는 무기 트레이스를 **소유 클라**(자기 캐릭터가 렌더링됨)에서 하므로 해당 없다. 서버가 Chaser 소켓을 읽는 판정을 새로 만들면 같은 문제를 다시 볼 것.

> **Simulated Proxy에는 어빌리티가 없다** — 정지 큐가 유실되면(`ExecuteGameplayCue` 멀티캐스트는 unreliable) 그 화면에서만 몽타주가 끝까지 재생된다. 위치는 복제로 오므로 시각적 문제에 그친다.

## 블렌드 시간은 재생 속도로 보정한다

몽타주 블렌드 가중치는 **실제 시간(초) 기준**으로 움직인다. 재생 속도(`AttackSpeed` → `PlayRate`)가 빨라져도 블렌드 시간은 그대로라, 배속이 높을수록 **몽타주 내용 중 더 많은 부분이 블렌드 구간에 잠식된다.**

1초 몽타주, BlendOut 0.25초, 보정 없음:

| PlayRate | 실제 재생 | 블렌드 아웃 시작 (몽타주 위치) | 흐려지는 비율 |
|---|---|---|---|
| 1.0 | 1.0초 | 0.75 | 25% |
| 2.0 | 0.5초 | **0.50** | 50% |
| 4.0 | 0.25초 | **0.0** (첫 프레임부터) | 100% |

엔진은 남은 재생 시간을 `(구간 끝 − 위치) / PlayRate`(실제 초)로 계산해 `BlendOut × DefaultBlendTimeMultiplier` 이하가 되면 블렌드 아웃을 시작한다(`AnimMontage.cpp` `FAnimMontageInstance::Advance`). 보정이 없으면 **어빌리티 종료(= 블렌드 아웃 시작)가 몽타주 앞쪽으로 당겨져 뒤쪽 트레이스·입력 윈도우·노티파이가 잘리고**, 몽타주 길이 / BlendOut 이상의 배속에서는 재생 직후 어빌리티가 끝난다.

`UCBCharacterAnimInstance::PlayMontage`가 양쪽을 `÷ PlayRate`로 보정한다.

| 블렌드 | 방법 |
|---|---|
| 블렌드 인 | 재생 전에 `BlendInArgs.BlendTime /= PlayRate` |
| 자동 블렌드 아웃 | 재생 직후 새 인스턴스의 `DefaultBlendTimeMultiplier = 1 / PlayRate` — 엔진이 블렌드 아웃 시작 조건과 길이 양쪽에 곱한다 |

- 결과적으로 블렌드 아웃은 배속과 무관하게 **항상 몽타주의 `길이 − BlendOut` 지점**에서 시작한다. **트레이스 구간·입력 윈도우(`CheckInput`)·종료 노티파이는 이 지점보다 앞에 둔다.**
- 블렌드 인은 인스턴스 생성 시 이미 계산되어 `DefaultBlendTimeMultiplier`의 영향을 받지 않으므로 따로 나눈다.
- 에셋에 **커스텀 `BlendOutTriggerTime`**을 지정하면 시작 조건이 그 값을 그대로 써서 보정되지 않는다. 현재 쓰는 에셋은 없다(2026-09-25 확인).
- 다른 몽타주가 끊고 들어오는 전환은 **새 몽타주의 블렌드 인 설정**으로 이전 몽타주를 멈추므로(`StopAllMontagesByGroupName`) 이미 보정된 블렌드 인 값을 따른다.
- PlayRate는 모든 머신에서 같은 어트리뷰트로 계산되므로 머신마다 블렌드 아웃 시점이 같다.

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
| **정지 설계가 이미 검증됨** | 위 "지켜야 할 규칙 넷"은 실제 버그를 밟아가며 확정한 것이라 전환 시 전부 재검증 대상이다. (예전에는 "`PlayMontageAndWait`는 기본 블렌드 아웃 시간으로 정지해 규칙 ①(블렌드 아웃 0)을 어긴다"도 근거로 들었으나, 규칙 ①이 정정되어 더는 근거가 아니다 — 블렌드 아웃 길이는 루트 모션에 영향이 없다.) |
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

### 일정 속도 돌진 — `UCBRootMotionModifier_ConstantSpeedWarp`

Skew Warp 는 **"구간이 끝날 때 타겟에 도착"**하도록 매 프레임 속도를 정한다(남은 거리 ÷ 남은 시간). 시간이 고정이고 속도가 결과라 **타겟이 멀수록 빨라진다.** 거리 상한(`ClampedSkewWarp`)·속도 배수 상한(`MaxSpeedClampRatio`)을 걸어도 가까운 타겟엔 구간 끝에 맞춰 **느리게 기어가고**, 도착·충돌로 일찍 멈출 수도 없다. 돌진·전진 공격은 반대로 **속도가 고정이고 거리가 결과**여야 하므로 모디파이어를 따로 뒀다.

| | 동작 |
|---|---|
| 방향 | 구간이 활성화된 뒤 첫 프레임에 **워프 타겟 위치를 향해 고정** (A안). 이후 타겟이 움직여도 직진 — 옆으로 피할 수 있다 |
| 이동 | 매 프레임 `Speed`(cm/s) × 시간만큼 수평 이동. 애니메이션 자체 이동은 쓰지 않는다(제자리 클립 전제) |
| 정지 | ① 고정한 도착 지점 도달 ② **막힘** — 지난 프레임 요청 이동량 대비 실제 이동이 30% 미만이면(정면 충돌) 구간이 남아도 멈춘다. 비스듬히 스치는 벽·경사로는 그 이상 움직이므로 계속 간다 |
| 회전 | 고정한 방향을 바라본다. 엔진 `WarpMaxRotationRate`(도/초) > 0 이면 그 속도로, 0 이면 즉시 |
| 정지 거리 | 새 값 없음 — 도착 지점이 워프 타겟 위치 그대로라, AI 공격의 `WarpStopDistance`(큐의 `VectorFromTargetToOwner` 오프셋)가 이미 타겟 앞 정지 거리다 |
| 정지 섹션 | `StopSectionName` 이 있으면 멈춘 순간(구간당 1회) **서버에서** 그 섹션으로 점프 요청 → 위 "섹션 점프". None 이면 이동만 멈추고 몽타주는 그대로 진행 |

- **이동 파라미터는 `Speed` 하나**(+ 연출용 `StopSectionName`). 도착 거리는 워프 타겟이, 최대 거리는 "속도 × 구간 길이"가 정한다. 막힘 비율(0.3)은 튜닝 대상이 아니라 판정 기준이라 상수로 뒀다.
- **속도는 재생 속도(`PlayRate`)와 무관한 실제 시간 기준이다.** `ProcessRootMotion` 의 `DeltaSeconds` 는 CMC 이동 틱의 실제 시간이고(`ConvertLocalRootMotionToWorld` 가 그대로 넘김), 모디파이어는 `PlayRate` 를 곱하지 않는다. 회전 제한도 같다(엔진 워프 회전은 `DeltaSeconds * PlayRate` 라 여기만 다르다).
  - ⚠️ **공격 속도 적용 액션에는 쓰지 않는다.** 몽타주 데이터 항목의 `bAffectedByAttackSpeed` 가 켜져 있으면 `UCBActionComponent` 가 `PlayRate = AttackSpeed` 를 준다. 속도는 그대로인데 구간만 실제 시간으로 짧아져(1.5 배속이면 2/3) **버프를 받을 때만 돌진 거리가 조용히 줄어든다.** 돌진·전진 워프를 넣은 액션은 이 값을 끈 채로 둔다.
  - 몽타주 에셋의 `Rate Scale` 도 같은 원리로 구간을 줄이지만 저작 시점에 고정되는 값이라, 그 배속을 감안해 `Speed`·구간 길이를 맞추면 된다.
  - **`PlayRate` 를 곱하지 않은 이유**: 곱하면 배속과 무관하게 도달 거리가 유지되지만(루트모션을 구운 클립과 같은 동작), **돌진이 공격 속도로 빨라진다.** 돌진 속도는 회피 가능성을 정하는 값이라 공격 속도 버프에 끌려가면 안 된다는 설계 판단으로 택하지 않았다.
  - 런타임 경고(재생 속도 ≠ 1 이면 로그)도 검토했으나, `Rate Scale` 을 의도적으로 조정한 몽타주에서도 울려 넣지 않았다.
- **방향은 첫 `ProcessRootMotion` 에서 고정한다.** 활성화 콜백(`OnStateChanged`)은 베이스 `Update` 가 타겟 위치를 캐시하기 **전에** 불려 `CachedTargetTransform` 이 비어 있다. 그래서 활성화 시에는 플래그만 리셋한다.
- **막힘 판정은 각 머신이 따로 한다.** Simulated Proxy 는 복제 보정으로 위치가 튈 때 오판할 수 있지만, 이동의 권위는 서버(AI)라 다음 보정에서 맞춰진다 — 연출상 순간적인 차이뿐이다.
- 워프 타겟 배선은 기존 AI 공격과 같다(`bWarpToTarget` → 추종 타겟 등록). 몽타주 노티파이의 Root Motion Modifier 만 이 클래스로 고르고 `Warp Target Name` 을 액션 태그로 맞춘다.

**몽타주 구성 예 (돌진)**

```
[예비동작]  Clamped Skew Warp — Warp Translation 끔, 회전만 → 타겟을 향해 돈다
[돌진 구간] Constant Speed Warp — Speed, StopSectionName 지정 → 고정 방향 직진, 도착·막힘 시 정지 + 정지 섹션으로 점프
[정지 섹션] 워프 없음 (부딪히는 순간 피해는 영역 공격 노티파이 등)
```

이동 공격(한 걸음 베기)은 같은 모디파이어를 짧은 구간에 둔다.

**루트모션 소스(GAS)가 아니라 워프 모디파이어인 이유** — 엔진 CMC 는 **애님 루트모션이 있으면 루트모션 소스를 적용하지 않는다**(`UCharacterMovementComponent::ApplyRootMotionToVelocity` — "Animation root motion ... takes precedence"). 제자리 클립이라도 `Enable Root Motion` 이 켜져 있으면 루트모션이 "있는" 것으로 판정된다(이동량 0 이어도 `FRootMotionMovementParams::Set` 이 `bHasRootMotion = true`). 그런데 예비동작의 회전 워프는 루트모션이 켜져 있어야 동작하므로, 같은 몽타주에서 루트모션 소스 돌진은 **통째로 무시된다.** 루트모션을 끄면 회전 워프를 잃고 회전을 따로 만들어야 한다 — 그래서 이동까지 워프 파이프라인 안에서 처리했다. (피격 넉백이 반대로 루트모션 소스를 택하고 피격 몽타주의 루트모션을 끄게 한 것도 같은 우선순위 때문이다 → [Abilities.md](Abilities.md) "피격 넉백")

- **멈춘 뒤의 모션은 `StopSectionName` 이 정한다.** 비워 두면 모션은 구간 끝까지 계속되어, 달리는 루프 클립이면 제자리 뛰기가 남는다. 정지 섹션을 지정하면 멈추는 순간 그 섹션으로 넘어간다(위 "섹션 점프" — 서버 판정, 큐로 전 머신). 정지 섹션은 돌진 구간보다 **뒤에** 둔다.
- 아래 "같은 몽타주를 다시 재생하면 워프가 죽는다" 함정이 그대로 적용된다(고정 방향·정지 플래그가 첫 재생 값으로 남음). AI 공격은 몽타주가 끝난 뒤 다음 공격이 시작되므로 현재는 해당 없다.

### 고정 각도 회전 — `UCBRootMotionModifier_RotateBy`

구간 동안 **정해진 각도만큼 몸을 돌린다.** 전방 휩쓸기 몽타주에 회전을 얹어 등 뒤까지 덮는 회전 베기로 만드는 용도다(회전 공격 모션이 따로 없는 보스의 등 뒤 대응 → [AI.md](AI.md) "등 뒤 판정").

| | 동작 |
|---|---|
| 베이스 | `URootMotionModifier` — 워프 타겟이 필요 없어 `_Warp` 를 상속하지 않는다. `_Warp` 파생이면 노티파이가 "Warp Target Name 없음" 경고를 띄운다(`AnimNotifyState_MotionWarping.cpp`) |
| 파라미터 | `YawAngle`(도) 하나. + = 위에서 볼 때 시계 방향(오른쪽). 180° 이상·한 바퀴 이상도 된다 |
| 회전 | 구간 진행도(몽타주 위치)에 비례해 선형으로 돈다. 끝나는 방향 = 시작 방향 + `YawAngle` |
| 이동 | 애니메이션 원래 이동을 그대로 둔다 — 회전만 교체 |

**매 프레임 각도를 더하지 않고 목표 각을 계산한다.** `목표 = 활성화 시점 방향 + YawAngle × 진행도` 로 두고, 현재 방향과의 차이만큼만 돌린다.

- **구간 진입 첫 프레임이 처리되지 않는다.** 엔진은 이전 프레임 위치가 구간 안에 들어온 뒤에야 모디파이어를 활성화한다(`URootMotionModifier::Update` — `PreviousPosition >= StartTime`). 더하는 방식이면 그 프레임만큼(짧은 구간·낮은 프레임에선 수십 도) 모자라고 머신마다 모자란 양이 다르다. 목표 방식은 다음 프레임에 따라잡아 항상 정확한 각도에서 끝난다.
- **보정을 받아도 같은 목표로 수렴한다.** Simulated Proxy 가 서버 보정으로 방향이 튀거나, 플레이어에게 쓸 때 예측 이동이 재생(saved move replay)돼도 다음 프레임에 같은 목표를 다시 향한다. 더하는 방식은 재생 때 지난 진행분을 되돌리거나 빠뜨린다.
- **상태 변수가 없다.** 시작 방향은 엔진이 활성화 시점에 채우는 `StartTransform` 을 그대로 쓴다 — `OnStateChanged` 오버라이드가 필요 없다.

**네트워크**: 서버 전용이 아니다. 몽타주가 큐로 전 머신에서 재생되고 모디파이어는 CMC 루트모션 처리 안에서 돌므로(Simulated Proxy 는 `SimulatedTick` → `SimulateRootMotion` → `ConvertLocalRootMotionToWorld` 의 워핑 훅) **각 머신이 몽타주 위치만 보고 같은 회전을 계산**한다. 권위는 서버(AI)이고 어긋남은 `RepRootMotion` 보정이 맞춘다. 노티파이 스테이트로 액터 회전을 직접 덮어쓰지 않는 이유가 이것이다 — 이동 파이프라인 밖의 회전은 복제·보정이 모르는 값이라 서로 덮어쓴다.

- **배속과 무관하게 총 각도가 같다.** 몽타주 위치 기준이라 `PlayRate` 가 바뀌면 도는 속도만 바뀐다. `ConstantSpeedWarp` 의 "공격 속도 적용 액션에 쓰지 말 것" 제약이 없다.
- **구간 안에서는 애니메이션 자체의 회전을 덮어쓴다.** 다른 워프와 같은 제자리 클립 전제다.
- **같은 몽타주의 다른 회전 워프와 구간을 겹치지 말 것.** 둘 다 회전을 교체하므로 뒤에 처리된 쪽만 남는다. 조준 워프 → 회전 구간 순서로 둔다.
- 공격이 끊기면(블렌드 아웃) 루트모션이 0 이 되어(위 "정지하는 순간 그 인스턴스는 루트 모션 몽타주에서 빠진다") 회전도 그 자리에서 멈춘다.
- 한계: 회전량은 최단 각(`FindDeltaAngleDegrees`)으로 구하므로 **한 프레임에 180° 넘게 돌 만큼 짧은 구간**이면 반대로 돈다. 현실적인 값(0.3 초에 360° = 30fps 에서 프레임당 40°)에서는 해당 없다.
- 아래 "같은 몽타주를 다시 재생하면 워프가 죽는다" 함정도 적용된다(`StartTransform` 이 첫 재생 값으로 남아 그 방향 기준으로 돈다). AI 공격은 현재 해당 없다.

**몽타주 구성 예 (휩쓸기 → 회전 베기)**

```
[선딜]    Clamped Skew Warp — Warp Translation 끔, 회전만 → 타겟 조준
[꼬기]    Rotate By −30    (선택: 반대로 살짝 꼬는 예고)
[휘두름]  Rotate By +210   ← 무기 트레이스 구간과 겹치게
[후딜]    워프 없음
```

- **회전 방향은 스윙 방향과 같게** 둔다. 덮는 범위 ≈ 애니메이션이 휩쓰는 각 + `YawAngle`. 반대로 돌면 오히려 줄어든다.
- **회전 구간은 판정 구간과 겹치게** 둔다. 선딜·후딜에서 돌리면 판정 없이 돌기만 하고, 느리게 돌수록 발 미끄러짐이 드러난다.
- 무기 트레이스는 지난 프레임 → 이번 프레임 경로를 스윕하므로 빨리 돌아도 판정이 비지 않는다(→ [Combat.md](Combat.md)).

**채택하지 않은 것**

| 안 | 이유 |
|---|---|
| 노티파이 스테이트로 액터 회전 직접 설정 | 이동 파이프라인 밖이라 복제·보정과 충돌 (위 "네트워크") |
| 엔진 워프 + `AdditionalRotationOffset`(5.8) | 회전이 최단 경로 보간이라 180° 미만으로 제한되고, 기준이 될 워프 타겟이 필요하다 |
| 이징 커브 | 구간 위치·여러 구간 분할로 속도 변화를 줄 수 있다. 선형이 어색하다는 근거가 생기면 추가 |
| 최대 회전 속도 제한 | 속도는 각도 ÷ 구간 길이가 정한다. 상한을 걸면 목표 각에 못 미치고 끝난다 |
| 애니메이션 자체 회전 보존 | 제자리 클립 전제. 회전이 구워진 클립을 쓰게 되면 그때 구간 내 애님 회전을 추출해 더한다 |
| 뒤에 있는 대상 쪽으로 방향 자동 선택 | 타겟 데이터가 필요해 워프가 된다. 고정 각도로 충분히 덮는다 |

### ⚠️ `CueParams.Normal` 은 회전 워프로 들어간다

큐가 고정 워프를 등록할 때 `Parameters.Normal.Rotation()` 을 회전 목표로 쓴다.

```cpp
AddOrUpdateWarpTargetFromLocationAndRotation(ActionTag.GetTagName(), Parameters.Location, Parameters.Normal.Rotation());
```

`Location` 만 채우고 `Normal` 을 비워두면 `ZeroRotator` 가 들어가 **캐릭터가 월드 정면으로 홱 돌아간다.** 같은 필드를 액션마다 반대 의도로 쓰므로 주의:

고정 워프를 쓰는 액션은 `Location` 과 `Normal` 을 **반드시 함께** 채운다. 대시(`UCBGADash`)는 진행 방향을 실어 그 방향으로 돌면서 나가고, 방향을 유지해야 하는 액션이라면 현재 `GetActorForwardVector()` 를 실어야 한다.

### ⚠️ 같은 몽타주를 다시 재생하면 워프가 죽는다

`UMotionWarpingComponent::UpdateWithContext` 는 워프 윈도우를 스캔할 때 **Animation 포인터 + StartTime + EndTime** 이 같으면 기존 모디파이어를 재사용한다(`ContainsModifier`). 그래서 **워프 윈도우가 살아 있는 동안 같은 몽타주를 다시 재생하면** 지난 재생의 모디파이어가 그대로 쓰인다.

낡은 모디파이어가 제거되지도 않는다 — `URootMotionModifier::Update` 가 제거 조건을 검사하기 **전에** 멤버 `PreviousPosition` 을 컨텍스트 값으로 덮어쓰기 때문에, "재생 위치가 되감겼다"를 알아볼 수단이 사라진다.

결과는 `StartTransform` 이 첫 재생 시점에 고정된 채 남는 것이다. 거리 상한을 두는 `UCBRootMotionModifier_ClampedSkewWarp` 에서는 상한이 이미 소진된 상태가 되어 **이동량이 0으로 깎인다.**

- 실제로 피격 넉백을 워핑으로 만들었을 때 **연쇄 피격의 첫 타만 밀리는** 증상으로 드러났고, 그래서 넉백은 루트모션 소스로 갈아탔다 (→ [Abilities.md](Abilities.md) "피격 넉백").
- **콤보가 같은 클립을 연속 재생하면 같은 증상이 난다.** 워프를 쓰는 액션에 재생 반복이 생기면 이 함정을 먼저 의심할 것.
- 진단은 `log LogMotionWarping Verbose` — `RootMotionModifier added` / `removed` 가 재생마다 한 쌍씩 찍혀야 정상이다.
- 굳이 워핑을 유지해야 한다면 모디파이어가 `Update` 에서 되감김(`Context.PreviousPosition < PreviousPosition`)을 감지해 스스로 `MarkedForRemoval` 하면 된다. 그러면 다음 프레임에 새 인스턴스가 만들어진다.

### 주의

- **발 미끄러짐** — 제자리 클립 + 합성 이동은 거리가 길수록 미끄러져 보인다. 워프 구간을 짧게 잡거나 거리 상한을 둘 것. 사거리 보정 정도의 짧은 거리는 티가 안 난다.
- 루트모션 재생 중 회전 잠금과 **공존한다**. 잠금은 액터 회전을 안 건드리는 쪽으로 막을 뿐이고, 워핑은 루트모션 델타를 보정하는 별개 파이프라인이다. → [Locomotion.md](Locomotion.md)
- 컴포넌트는 `ACBBaseCharacter` 가 소유한다(`CBMotionWarpingComponent`).

## 관련 문서
- 콤보 인덱스 전진/리셋 소유: [Combat.md](Combat.md)
- 몽타주 재생을 트리거하는 어빌리티: [Abilities.md](Abilities.md)
- GameplayCue로 Simulated Proxy 동기화하는 이유: [Multiplayer.md](../Conventions/Multiplayer.md)
- 몽타주 정지 후 포즈 전환(관성화), 루트모션 접근 시 캐릭터 간 밀림 규칙: [Locomotion.md](Locomotion.md)
