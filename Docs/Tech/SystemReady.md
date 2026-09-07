# 캐릭터 시스템 준비(Ready) 시스템

> 캐릭터의 비동기 초기화가 끝날 때까지 의존 로직을 미루는 신호 체계. **캐릭터의 ASC나 비동기 로드 에셋(로드아웃 파생 데이터)에 접근하는 모든 코드는 준비 완료까지 기다린 뒤 초기화·동작하도록 설계한다.**

## 왜 필요한가

캐릭터 초기화는 **비동기**다 — 로드아웃을 async 로드하고 그 콜백에서 메쉬·애님BP·이동 데이터·몽타주 데이터·어빌리티·무기를 적용한다(→ [Loadout.md](Loadout.md)). 또 플레이어 ASC는 PlayerState 복제를 기다린다(→ [ASC-Ownership.md](ASC-Ownership.md)).

따라서 스폰 직후에는 다음이 **아직 준비 안 된** 상태일 수 있다:
- `CBASC`가 null이거나 `InitAbilityActorInfo` 미완료
- `MovementDataAsset`·몽타주 데이터 미주입, 스켈레탈 메쉬 미로드

이 시점에 ASC/에셋에 의존하는 로직이 돌면 크래시하거나 잘못 동작한다. 그래서 **"준비 완료" 신호가 올 때까지 초기화를 미루는** 체계를 둔다.

## 상태와 신호 (`ACBBaseCharacter`)

| 요소 | 내용 |
|---|---|
| `ECBSystemState` | `Uninitialized → Initializing → Ready` 3상태 |
| `IsCharacterSystemReady()` | 현재 Ready 여부 |
| `OnCharacterSystemReadyDelegate` | 준비 완료 시 방송되는 멀티캐스트 델리게이트 |
| `HandleCharacterSystemReady()` | 공용 데이터(로드아웃) 적용 완료 시 **1회** 호출 → 상태 Ready 전환 + 델리게이트 방송 + `InitializeAttributes()`. **멱등**(중복 방송 방지) |

- 방송 시점 = **로드아웃 로드 + 기본 의상 로드, 2단계가 모두 끝난 뒤**. 로드아웃 내부 에셋은 하드 참조라 1단계에서 resolve되지만, **기본 의상은 카탈로그의 소프트 메시**라 한 단계가 더 필요하다(→ [Cosmetic.md](Cosmetic.md), [AssetReference.md](AssetReference.md)).

```
LoadAssetAsync<Loadout>
 └ ApplyToCharacter / Auth_ / Local_          (동기)
 └ Loadout->ApplyAsyncToCharacter(OnComplete) ← 2단계. 로드아웃의 소프트 참조를 여기서 로드
     └ HandleCharacterSystemReady()
```

**소프트 참조 로드는 로드아웃이 소유한다.** `ApplyAsyncToCharacter`는 `UCBCharacterLoadout`의 가상 함수이며 기본 구현은 즉시 완료한다. `UCBChaserLoadout`이 이를 오버라이드해 기본 의상 파츠 메시를 로드한다.

- **"무엇을 더 로드해야 하는지"는 로드아웃**이 알고, **"언제 준비 완료인지"는 캐릭터**가 정한다. 캐릭터는 완료 콜백만 기다린다
- 로드아웃에 소프트 참조가 늘어나면 **이 함수 안에 모은다.** 캐릭터의 초기화 콜백이 에셋 사정을 알게 되는 것을 막기 위함
- **AI 캐릭터도 같은 형태로 호출한다.** 지금은 즉시 완료지만, 이 호출이 없으면 나중에 AI 로드아웃이 오버라이드해도 **조용히 무시된다**

**각 단계는 실패해도 반드시 다음으로 넘어간다.** 로드아웃 로드 실패·카탈로그 없음·기본 의상 없음 어느 경우에도 콜백이 호출되어 방송에 도달한다 — 빠지면 캐릭터가 영구히 잠기고, 지금은 **화면이 검은 채로 멈춘다**(페이드가 준비 완료를 기다리므로).

### 단계가 늘면 게이트로 바꾼다

지금 2단계는 **콜백 체인**이다 — 읽으면 순서가 보이고, 방송 지점이 배타적 경로마다 하나씩이라 눈으로 검증된다.

| 단계 수 | 적절한 방식 |
|---|---|
| **1~2 (지금)** | 콜백 체인 |
| 3~4 | **완료 게이트** — 남은 작업 수를 세고 0이 되면 방송 |
| 그 이상 | 초기화 파이프라인 (단계 목록 + 순차 실행) |

**지금 체인을 택한 근거는 "2개라서"다.** 이 기준을 적어두지 않으면 5단계가 되어도 관성으로 체인을 이어붙이게 되고, 그때부터는 "이 경로에서도 방송을 빠뜨리지 않았나"를 사람이 매번 검사해야 한다.
- 진입점·서버/클라 흐름 상세는 [Multiplayer.md](Multiplayer.md)의 "캐릭터 시스템 준비(Ready) 신호" 참고.

## 소비 패턴 — "구독 또는 즉시" 1회성 훅

베이스 클래스가 이 패턴을 **내장**하므로, 자식은 `OnCharacterSystemReady()` 오버라이드만 하면 된다.

| 소비자 유형 | 베이스 | 준비 후 훅 |
|---|---|---|
| 컴포넌트 | `UCBExtensionComponent` (`BeginPlay`에서 구독) | `virtual OnCharacterSystemReady()` |
| 애님 인스턴스 | `UCBBaseAnimInstance` (`NativeInitializeAnimation`에서 구독) | `virtual OnCharacterSystemReady()` |

**동작:**
1. 초기화 시점에 오너 캐릭터를 얻어 `IsCharacterSystemReady()`를 확인한다.
2. **이미 Ready면 즉시** 훅 호출. **아니면** `OnCharacterSystemReadyDelegate`에 구독.
3. 방송이 오면 내부 콜백이 **구독 해제 → 잠금 해제(`bIsSystemLocked=false`) → `OnCharacterSystemReady()` 1회 호출**. (잠금 플래그로 중복 실행 방지)

**게이트:** 준비 전 매 프레임 로직은 `IsSystemLocked()`로 막는다.
```cpp
void UpdateX()
{
    if (IsSystemLocked()) return;   // 준비 전엔 아무 것도 하지 않음
    // ASC/에셋 의존 로직 ...
}
```

**예시:**
- `UCBCharacterAnimInstance`: `OnCharacterSystemReady()` → `InitAnimData()`에서 **ASC 준비 후** 전투 태그 이벤트를 등록. 그전까지 업데이트는 `IsSystemLocked()`로 게이트.
- `UCBAttributeSet`: `HandleCharacterSystemReady()` → `InitializeAttributes()` → `OnCharacterSystemReady()`로 어트리뷰트 초기화.
- `ACBChaserController`: 베이스를 못 쓰는 외부 액터 사례. 델리게이트를 직접 구독해 **캐릭터가 준비될 때까지 화면을 검게 유지**한다(의상이 늦게 붙는 것을 가림). 핸들 유효성으로 중복 구독을 막고 콜백에서 해제한다 → [GameFlow.md](GameFlow.md)

## 준비 완료를 BP로 중계할 때 — `UCBLocalReadySubsystem`

캐릭터의 `OnCharacterSystemReadyDelegate`는 **non-dynamic** 이라 블루프린트에서 구독할 수 없다. BP 쪽(레벨의 위젯 컨트롤러 등)이 준비 시점을 알아야 하면, 컨트롤러가 준비를 확인한 뒤 **월드 서브시스템의 dynamic 델리게이트로 중계**한다.

```
캐릭터 준비 완료 (non-dynamic)
  → ACBChaserController::Local_ApplyReadyState()
      → UCBLocalReadySubsystem::NotifyLocalPlayerReady()  (BlueprintAssignable)
          → BP 구독자
```

중계 지점을 컨트롤러로 둔 이유는 **거기가 로컬·로비 여부·폰 유효성을 이미 판별한 자리**이기 때문이다. 소유자를 왜 서브시스템으로 했는지, 구독자가 왜 "구독 + 즉시 확인" 두 갈래를 모두 처리해야 하는지는 → [GameFlow.md](GameFlow.md)

## 설계 규칙 (필수)

**외부에서 캐릭터의 ASC나 비동기 로드 에셋에 접근·의존하는 코드는, 반드시 준비 완료까지 대기한 뒤 초기화·동작하도록 설계한다.**

- **준비 전 접근 금지.** 스폰 직후 ASC/에셋은 미완성일 수 있으므로, 의존 초기화는 `OnCharacterSystemReady()`(또는 델리게이트 콜백)에서 수행한다.
- **새 컴포넌트/애님 인스턴스**는 위 베이스(`UCBExtensionComponent`/`UCBBaseAnimInstance`)를 상속하고 `OnCharacterSystemReady()`에 의존 초기화를 넣는다. 매 프레임 로직은 `IsSystemLocked()`로 게이트.
- **베이스를 못 쓰는 외부 액터/서브시스템**은 같은 패턴을 직접 구현한다: `IsCharacterSystemReady()` 확인 → 아니면 `OnCharacterSystemReadyDelegate` 구독. 처리 후 구독 해제 + 1회성 가드.
- **멱등·1회성 유지.** 델리게이트가 여러 번 불릴 가능성에 대비해 잠금 플래그로 훅이 1회만 실행되게 한다.

## 관련 문서
- 초기화 흐름(서버/클라 진입점, 단일 신호): [Multiplayer.md](Multiplayer.md)
- ASC 소유·복제(플레이어=PlayerState, AI=Character): [ASC-Ownership.md](ASC-Ownership.md)
- 로드아웃 비동기 로드·공용 데이터 적용: [Loadout.md](Loadout.md), [AssetReference.md](AssetReference.md)
- 애님 인스턴스 계층·Lifecycle region: [AnimInstance.md](AnimInstance.md)
- 컴포넌트 베이스(`UCBExtensionComponent`): [Components.md](Components.md)
