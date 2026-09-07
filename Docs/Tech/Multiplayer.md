# 멀티플레이 동기화 패턴

> 서버 권위 구조, 함수 접두사 규칙, 초기화 흐름, 어빌리티 네트워크 설정을 정의한다. **모든 기능은 멀티플레이를 전제로 설계한다.**

## 기본 원칙
- **리슨 서버(Listen Server)** 구조로 구현한다.
- **서버 권위적(Server-Authoritative).** 대부분의 처리는 서버에서 수행하며, 서버의 값이 절대적이다.
- 무거운 연산이나 복잡한 로직만 로컬(클라이언트)에서 먼저 처리하고, 서버는 간단한 연산으로 **검증**한 뒤 반영한다. (예: 무기 트레이스 히트 감지를 로컬에서 수행 → `Server_NotifyAttackHit()`로 서버 검증)

## 함수 접두사 규칙 (필수)
- `Auth_` : **서버에서만** 호출해야 하는 함수. (예: 무기 스폰, 어빌리티 부여, GE 적용)
- `Local_` : **로컬 클라이언트에서만** 호출해야 하는 함수.
- 위 두 접두사는 호출 위치를 명확히 구분·통제하기 위한 프로젝트 자체 규칙이다. 서버/로컬 전용 함수는 반드시 접두사를 붙인다.
- `Server_` / `Client_` 는 언리얼 RPC 시스템이 쓰는 접두사이므로 위 규칙과 별개. (RPC 정의에 그대로 사용)

## 초기화 흐름
`PossessedBy`(서버) / `OnRep_PlayerState`(클라이언트) → `InitializePlayerSystem()` → `Auth_InitServerData()` / `Local_InitClientData()`

### 캐릭터 시스템 준비(Ready) 신호
> 아래는 **초기화 흐름(방송하는 쪽)** 관점이다. 이 신호를 **소비**해 의존 로직을 미루는 규칙은 [SystemReady.md](SystemReady.md) 참고.

- 캐릭터 초기화는 **공용 데이터(로드아웃) 로드 async 1개로 수렴**한다. 로드아웃 내부 에셋은 모두 하드 참조라 이 로드 하나로 전부 resolve된다. → [Loadout.md](Loadout.md)
- 그 로드 콜백에서 **`HandleCharacterSystemReady()`를 한 번 호출**해 `OnCharacterSystemReadyDelegate`를 방송한다. 별도의 배리어 카운터를 두지 않는다(단일 완료 지점).
- 진입점은 캐릭터별로 다르지만 형태는 동일하다: `StartSystemInitialization()`(재진입 가드, `Initializing` 전환) → 공용 로드 → 콜백 끝에서 `HandleCharacterSystemReady()`.
  - **Chaser**: `InitializePlayerSystem()`(`PossessedBy`/`OnRep_PlayerState`)에서 구동. ASC는 PlayerState라 로드 전 동기 캐싱, 콜백에서 `HasAuthority()`/`IsLocallyControlled()`로 분기.
  - **Outlaw/Rogue(AI)**: 공통 베이스 `ACBAICharacter::InitializeAISystem()`으로 **로드아웃을 인스턴스당 1회만 로드**하고, 콜백에서 공용 데이터(전 인스턴스) + 서버 권위 데이터(`HasAuthority()` 분기)를 함께 적용한다. 진입점은 권위별로 나뉜다 — **서버는 `PossessedBy`**(빙의 완료 직후), **비권위(시뮬 프록시)는 `BeginPlay`**(공용만). 이 분리로 과거의 이중 로드(BeginPlay+PossessedBy)와 순서 해저드가 제거된다.
    - **ASC `InitAbilityActorInfo`는 `InitializeAISystem()` 안에서 호출**해 **서버·클라 모두** 세팅한다. character-owned ASC(owner=avatar=self)는 복제로 actor info가 자동 세팅되지 않으므로 각 인스턴스가 직접 호출해야 하며, 안 하면 시뮬 프록시에서 GameplayCue가 아바타(메쉬)를 못 찾아 AI 몽타주/큐가 재생되지 않는다. 서버는 `PossessedBy` 경유라 AIController까지 캡처된다. (Chaser는 PlayerState ASC라 서버=`PossessedBy`/클라=`OnRep_PlayerState`에서 세팅.)
- 로드아웃이 없으면 async 없이 즉시 `HandleCharacterSystemReady()`. `HandleCharacterSystemReady()`는 멱등(중복 방송 방지)이며, 컴포넌트/애님은 `UCBExtensionComponent`에서 이 델리게이트를 "구독 또는 즉시 처리"로 소비한다.

## 어빌리티 설계 시 네트워크 설정 (필수 확인)
- 어빌리티를 설계할 때는 **항상 멀티플레이를 전제로** 네트워크 관련 설정을 확인한다. 자세한 설정은 `UGameplayAbility` 클래스를 참고.
- 어빌리티의 멀티플레이 실행 규칙은 `UGameplayAbility::NetExecutionPolicy` (`EGameplayAbilityNetExecutionPolicy`)로 정한다. 이 어빌리티를 **어디서 실행할지**를 항상 고민하며 설계할 것.

  | 정책 | 실행 위치 |
  |---|---|
  | `LocalPredicted` | 클라이언트에서 호출 시 클라이언트·서버 **모두** 실행 (예측 실행) |
  | `LocalOnly` | 클라이언트(로컬)에서만 실행 |
  | `ServerOnly` | 서버에서만 실행 |
  | `ServerInitiated` | 서버에서 시작되어 실행 |

  예: **UI 띄우기**처럼 로컬에서만 처리하면 되는 기능은 `LocalOnly`로 설계. 게임플레이에 영향을 주는 액션은 보통 `LocalPredicted`/`ServerOnly` 등 권위 처리를 고려.

- **어빌리티는 로컬(소유 클라) 또는 서버에서만 실행되며, 제3자인 Simulated Proxy에서는 실행되지 않는다.** 따라서 어빌리티 로직만으로는 Simulated Proxy에 반영되지 않는다.
- 그래서 **몽타주 재생은 GameplayCue로, 데이터 처리는 GE(GameplayEffect)로** 처리한다. → GAS가 자체적으로 멀티플레이 동기화를 지원하므로, 서버에서 호출하면 Simulated Proxy까지 전부 적용된다.
- **Simulated Proxy까지 반영이 필요한데** 어빌리티에서 값을 수정하거나 별도 함수를 호출해야 한다면, 다음 중 하나를 선택한다:
  1. 서버 호출 시 전 클라(Simulated Proxy 포함) 동기화를 보장하는 GAS 기능/라이브러리/클래스 사용 (GameplayCue, GE 등)
  2. 직접 RPC(Multicast 등) 또는 프로퍼티 레플리케이션으로 수동 동기화

## ASC 복제 모드
- GameplayEffect 복제 비용을 최소화하기 위해 **AI는 `Minimal`(기본값), 플레이어(PlayerState 소유 ASC)는 `Mixed`** 로 설정한다.
- Attribute 값은 복제 모드와 무관하게 항상 복제되며, `Mixed`는 유효한 소유 커넥션이 필요하므로 AI에는 쓰지 않는다.
- 세 모드 비교표·주의사항 등 상세는 [ASC-Ownership.md](ASC-Ownership.md) 참고.

## 복제 갱신 주기 (ASC를 얹은 액터)

- **복제 컴포넌트는 자기 갱신 주기를 갖지 않는다.** 소유 액터가 복제 차례를 받을 때 그 채널에 얹혀 나가므로, 주기·관련성·휴면이 전부 액터를 따른다. GAS의 `UAbilitySystemComponent::ForceReplication()`도 컴포넌트가 아니라 `GetOwner()->ForceNetUpdate()`를 부른다.
- **`APlayerState`의 기본 갱신 주기는 1Hz**(이름·점수·핑 기준값)인데, 이 프로젝트는 플레이어 ASC·AttributeSet을 PlayerState에 둔다. 기본값 그대로면 **루스 태그와 어트리뷰트가 시뮬레이티드 프록시에 평균 0.5초 늦게 도착한다**(실측 500~517ms). 그래서 `ACBPlayerState` 생성자에서 `SetNetUpdateFrequency(30.f)`로 올린다 — 적용 후 실측 36ms.
  - 주기는 "초당 N번 보낸다"가 아니라 "초당 최대 N번 보낼 기회를 준다"이다. 값이 안 바뀌면 아무것도 나가지 않으므로(`FGameplayTagCountContainer::NetDeltaSerialize`가 변화 없으면 `false` 반환) 상향 비용은 크지 않다.
  - AI는 ASC가 Character 소유라 `APawn` 기본값 100Hz를 타므로 이 문제가 없다. **같은 코드인데 플레이어만 느린 증상**이 나오면 여기를 의심할 것.
- **RPC는 이 주기와 무관하다.** 호출 즉시 채널로 나간다. 그래서 게임플레이 큐(몽타주 재생·정지)는 즉시 도착하는데 같은 ASC의 루스 태그는 늦는 상황이 생긴다. **"왜 늦게 오나"를 볼 때는 그 값이 속성 복제인지 RPC인지 먼저 구분하고, 속성이면 컴포넌트가 아니라 소유 액터의 주기를 확인한다.**
- 전환 순간의 지연까지 없애야 하면 상태 변경 직후 `ASC->ForceReplication()`으로 다음 네트 틱에 강제 송출한다(현재는 30Hz만으로 충분해 미적용).

## 관련 문서
- 어빌리티 베이스별 NetExecutionPolicy 기본값: [Abilities.md](Abilities.md)
- GameplayCue로 몽타주를 동기화하는 구현: [Montage.md](Montage.md)
- ASC 소유·초기화 진입점: [ASC-Ownership.md](ASC-Ownership.md)
