# 진영(팀) 시스템

> 적/아군/중립 판정을 다룰 때 읽는 문서. 엔진의 `IGenericTeamAgentInterface`를 그대로 쓰고, 팀 데이터는 캐릭터가 소유한다.

## 팀 값

```cpp
enum class ECBTeam : uint8 { Chaser = 0, Outlaw = 1, Neutral = 255 };
```

- **Outlaw와 Rogue는 같은 팀**(`Outlaw`)이다. 등급만 다르고 진영은 하나.
- `Neutral = 255`는 엔진 `FGenericTeamId::NoTeam`과 값이 같다. **enum 값 자체가 팀 번호**이므로 순서를 바꾸면 저장된 BP 기본값이 어긋난다 — 추가는 반드시 뒤에.
- 변환은 `CBTeamToGenericId()` / `GenericIdToCBTeam()` (`Types/CBEnumTypes.h`). 모르는 ID는 Neutral로 떨어진다.

## 데이터 소유 — 컨트롤러가 아니라 캐릭터

| 요소 | 위치 |
|---|---|
| 팀 데이터 (`Team`, 복제) | `ACBBaseCharacter` |
| `IGenericTeamAgentInterface` | `ACBBaseCharacter` + `ACBAIController` |
| 팀 할당 | 캐릭터 BP 기본값 (런타임 코드 없음) |

**왜 폰인가:** `APlayerController`는 서버와 그 본인 클라이언트에만 존재한다. 팀을 컨트롤러에 두면 다른 클라이언트에서 리모트 플레이어의 진영을 알 수 없어, 아군/적 체력바 색 구분 같은 로컬 표현에 별도 복제를 또 만들게 된다. 폰은 시뮬 프록시 포함 전 인스턴스에 존재하므로 **복제 하나로 AI 판정(서버)과 UI 표현(클라)을 모두 덮는다.**

**컨트롤러는 위임만 한다:** 퍼셉션은 "감지하는 쪽"의 팀을 자기 소유자(= AIController)에게 묻는다(`UAIPerceptionComponent::GetTeamIdentifier` → `GetOwner()`). 그래서 `ACBAIController`도 인터페이스를 구현하되, `AAIController`의 자체 `TeamID` 멤버를 쓰지 않고 **빙의한 폰의 진영을 반환**한다. 소스를 하나로 묶어 폰과 컨트롤러의 진영이 어긋날 여지를 없앤다.

- `GetGenericTeamId()`는 `CachedAICharacter`(`OnPossess`에서 세팅), 없으면 `GetPawn()`으로 폴백 — 퍼셉션이 빙의보다 먼저 물어올 수 있다.
- `ACBBaseCharacter::SetGenericTeamId()`는 `Auth_SetTeam()`으로 위임하며, 권위가 아니면 무시한다.

## 판정 규칙 — 전역 attitude solver

**엔진 기본 solver는 중립을 모른다:**
```cpp
// 엔진 AIInterfaces.cpp
DefaultTeamAttitudeSolver(A, B) { return A != B ? Hostile : Friendly; }
```
NoTeam을 특별 취급하지 않으므로, 그대로 두면 **중립이 모두에게 적대**가 된다.

그래서 `UCBGameInstance::Init()`에서 커스텀 solver를 1회 등록한다:
- 한쪽이라도 `Neutral`이면 → `Neutral`
- 같은 팀이면 → `Friendly`
- 다르면 → `Hostile`

`Shutdown()`에서 `ResetAttitudeSolver()`로 되돌린다(에디터 PIE 반복 대비).

### 클래스별 오버라이드가 아니라 전역 solver인 이유

**시각과 청각이 서로 다른 경로로 팀을 비교하기 때문이다.**

| 감각 | 호출 | 경유 |
|---|---|---|
| Sight | `ShouldSenseTeam(TeamAgent, TargetActor, flags)` | `TeamAgent->GetTeamAttitudeTowards()` — **인터페이스** |
| Hearing | `ShouldSenseTeam(Listener.TeamIdentifier, Event.TeamIdentifier, flags)` | `FGenericTeamId::GetAttitude(ID, ID)` — **전역 solver 직행** |

(엔진 `AISense_Sight.cpp` / `AISense_Hearing.cpp`)

클래스에서 `GetTeamAttitudeTowards()`를 오버라이드해 중립을 처리하면 **시각만 반영되고 청각은 기본 solver를 따라** 두 감각의 결론이 갈린다. 전역 solver는 두 경로를 동시에 덮는 유일한 지점이다.

### `GetTeamAttitudeTowards()`는 오버라이드하지 않는다

`AAIController`는 이 함수를 오버라이드하지 않으며, 인터페이스 기본 구현이 **상대 액터를 곧바로 인터페이스로 캐스팅**해 팀을 읽는다:
```cpp
// 엔진 IGenericTeamAgentInterface 기본 구현
const IGenericTeamAgentInterface* OtherTeamAgent = Cast<const IGenericTeamAgentInterface>(&Other);
```
캐릭터가 인터페이스를 직접 구현하므로 컨트롤러 홉 없이 해결된다. 별도 오버라이드는 불필요.

## 소비처

- **퍼셉션 소속 필터**: Sight·Hearing 모두 `DetectionByAffiliation`이 **적만 감지**(`bDetectEnemies`만 true). 아군·중립은 자극조차 오지 않는다.
- **타겟 선별 seam**: `ACBAIController::IsValidTarget()` = `GetAttitude(this, InActor) == Hostile`. 퍼셉션과 같은 기준이라 두 단계가 어긋나지 않는다. `virtual`이므로 자식이 좁힐 수 있다.
- **무기 트레이스 히트 필터**: `UCBCombatComponent::IsHostileTarget()` = `GetAttitude(GetOwner(), InActor) == Hostile`. **로컬 트레이스와 서버 검증 양쪽**에서 호출한다 — 클라가 보낸 히트를 서버가 그대로 믿지 않기 위해. 중립·팀 인터페이스 없는 액터는 피격되지 않는다. → [Combat.md](Combat.md)
- **팀 변경 통지**: `OnTeamChangedDelegate`(서버는 `Auth_SetTeam`에서, 클라는 `OnRep_Team`에서 방송). 현재 구독자는 없고 향후 UI 색 구분용 훅. → [UI.md](UI.md)

## 팀 할당 — C++ 생성자

진영은 **클래스 정체성**(`ACBChaserCharacter`는 정의상 추격자)이지 캐릭터별 에셋 구성이 아니므로, 로드아웃이나 BP가 아니라 **생성자에서 지정**한다. 새 몬스터 BP를 만들 때 설정을 빠뜨릴 여지가 없다 — 로드아웃의 "BP마다 설정할 항목을 늘리지 않는다" 원칙과 같은 이유([Loadout.md](Loadout.md)).

| 클래스 | 팀 | 지정 위치 |
|---|---|---|
| `ACBChaserCharacter` | `Chaser` | 자기 생성자 |
| `ACBOutlawCharacter` / `ACBRogueCharacter` | `Outlaw` | **`ACBAICharacter` 생성자** (둘이 같은 팀이라 공통 베이스에 1회) |
| `ACBBaseCharacter` | `Neutral` | 멤버 기본값 |

- **베이스 기본값이 Neutral**이라, 어느 계층에도 걸리지 않은 캐릭터는 오작동이 아니라 **조용히 무시**된다(안전한 기본).
- `Team`은 `EditAnywhere`이므로 **BP나 자식 생성자에서 오버라이드 가능**. 중립 야생동물 같은 예외는 그쪽에서 처리한다.
- ⚠️ UE는 CDO와 다른 값만 BP에 저장한다(델타 시리얼라이제이션). BP에서 `Team`을 한 번이라도 명시 지정하면 **C++ 기본값을 바꿔도 BP 값이 이긴다** — 프로퍼티 옆 되돌리기 화살표로 리셋해야 상속받는다.
- PvP 등으로 플레이어별 진영이 갈리면 그때 `ACBGameModeBase`가 스폰 시 `Auth_SetTeam()`으로 부여한다(훅은 열려 있고, 런타임 할당 코드는 아직 없다).

## 알려진 한계

- **런타임 팀 변경 시 퍼셉션이 자동 재평가되지 않는다.** 엔진 `AAIController::SetGenericTeamId`에도 `@todo notify perception system` 주석이 남아 있는 부분이다. 팀을 바꾼 뒤 즉시 반영이 필요하면 관련 AI 컨트롤러에서 `RequestStimuliListenerUpdate()`를 호출해야 한다. 현재는 BP 기본값 고정이라 발생하지 않는다.
- **중립은 "무관심"이지 "평화주의"가 아니다.** 공격받으면 반격하는 중립이 필요해지면 solver가 아니라 별도 어그로 로직으로 다뤄야 한다.

## 관련 문서
- 무기 트레이스 히트 판정에서의 소비: [Combat.md](Combat.md)
- 퍼셉션·타겟 선별·BT 데이터 흐름: [AI.md](AI.md)
- 서버 권위·`Auth_` 접두사 규칙: [Multiplayer.md](Multiplayer.md)
- 팀 색 구분이 붙을 UI 계층: [UI.md](UI.md)
