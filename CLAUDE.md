# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

이 파일은 **항상 로드되는 핵심 규칙과 인덱스**만 담는다. 특정 시스템의 상세 설계는 아래 인덱스가 가리키는 `Docs/Tech/*.md` 문서를 **작업 전에 읽고** 따른다.

---

## 절대 규칙

- **코드 추가/수정 전 반드시 사용자 허락을 받아야 한다.** 변경 계획을 먼저 설명하고 승인 후 편집할 것.
- **엔진 소스 및 플러그인 코드는 절대 편집하지 않는다.** 읽기 전용 참조만 허용.
  - 엔진 루트: `D:\UE_5.8\Engine`
  - 소스: `D:\UE_5.8\Engine\Source`
  - 플러그인: `D:\UE_5.8\Engine\Plugins`
- **모든 기능은 멀티플레이(리슨 서버, 서버 권위)를 전제로 설계한다.** → [Docs/Tech/Multiplayer.md](Docs/Tech/Multiplayer.md)
- **시스템을 수정·확장하면 해당 `Docs/Tech/*.md` 문서도 함께 갱신한다.** 코드와 문서가 어긋나지 않게 유지할 것.

---

## 빌드

UnrealBuildTool(UBT) 기반 프로젝트. 평소 작업은 Rider 또는 언리얼 에디터에서 빌드하고, **코드를 수정한 뒤 컴파일 검증이 필요하면 아래 UBT 명령을 쓴다.**

- **에디터 실행**: `ChainBurst.uproject`를 언리얼 에디터에서 열기
- **C++ 빌드**: Rider에서 `Build > Build Solution` 또는 에디터 툴바의 컴파일 버튼
- **모듈**: `Source/ChainBurst/ChainBurst.Build.cs` — 의존 모듈: `EnhancedInput`, `GameplayAbilities`, `GameplayTasks`, `EngineSettings`

### 컴파일 검증 (UBT 직접 호출)

```bash
"D:/UE_5.8/Engine/Build/BatchFiles/Build.bat" ChainBurstEditor Win64 Development -Project="D:/UnrealProjects/ChainBurst/ChainBurst.uproject" -WaitMutex
```

증분 빌드라 보통 20~30초면 끝난다. **코드를 수정했으면 사용자에게 넘기기 전에 이걸로 검증한다.**

- **에디터가 열려 있으면 실패한다** — `Unable to build while Live Coding is active`. 에디터를 닫아달라고 요청할 것. (Rider만 열려 있는 건 무관)
- **`UPROPERTY`·`UFUNCTION`을 새로 추가한 변경은 Live Coding(Ctrl+Alt+F11)으로 반영되지 않는다.** 클래스 레이아웃이 바뀌므로 성공 메시지가 떠도 BP에서 새 핀·RPC가 보이지 않는다. 에디터 재시작이 필요하다고 안내할 것
- 실패해도 UHT 단계까지는 실행되므로, 리플렉션 매크로 문법 오류는 에디터를 닫지 않아도 걸러진다

---

## 프로젝트 개요

**ChainBurst** — 멀티플레이어 액션 게임. 추격자(Chaser)와 무법자(Outlaw) 진영이 대립하는 구조.

### 캐릭터 종류
| 클래스 | 역할 |
|---|---|
| `ACBChaserCharacter` | 플레이어 조작 추격자. PlayerState에 ASC 보유. 카메라/입력/회전 컴포넌트 포함. `ACBBaseCharacter` 직속. |
| `ACBOutlawCharacter` | AI 적. 보스 또는 엘리트 등급으로, 복잡한 로직과 많은 컴포넌트를 가짐. 컴뱃 컴포넌트 보유. `ACBAICharacter` 상속. |
| `ACBRogueCharacter` | AI 적. 일반 잡몹 등급으로, 단순한 기능과 최소한의 컴포넌트로 구성. `ACBAICharacter` 상속(로드아웃만 지정). |
| `ACBAICharacter` | AI 공통 베이스(추상). Character에 ASC·AttributeSet 생성, AI 빙의·초기화 흐름(`InitializeAISystem`) 소유. |
| `ACBBaseCharacter` | 전 캐릭터 공통 베이스. ASC·AttributeSet 캐싱, 공통 컴포넌트(Locomotion, Action) 소유, 준비 완료(Ready) 신호. |

---

## 문서 인덱스

### 기술 문서 (`Docs/Tech/`) — 해당 시스템 작업 전 읽을 것
| 문서 | 언제 읽나 |
|---|---|
| [ASC-Ownership.md](Docs/Tech/ASC-Ownership.md) | ASC/AttributeSet 소유·캐싱, 초기화 진입점을 다룰 때 + 공용 함수를 어디에 둘지(`UCBAbilitySystemLibrary` 배치 기준) 정할 때 |
| [AssetReference.md](Docs/Tech/AssetReference.md) | 멤버로 에셋/오브젝트를 참조할 때 (하드/약한/소프트 선택, 비동기 로드) |
| [Loadout.md](Docs/Tech/Loadout.md) | 캐릭터에 필요한 에셋(메쉬/어빌리티/무기/GE/입력) 등록·적용을 다룰 때 |
| [SkeletonCompatibility.md](Docs/Tech/SkeletonCompatibility.md) | **새 캐릭터 메시를 적용할 때** — 애니메이션 스켈레톤과의 호환 진단, 커스텀 본 추가, 리타게팅 모드 |
| [Cosmetic.md](Docs/Tech/Cosmetic.md) | **의상 커스터마이징을 다룰 때** — 슬롯·카탈로그·태그 3상태, 하드/소프트 경계, 조합 복제와 로컬 조립, 런타임 교체 규칙 |
| [Abilities.md](Docs/Tech/Abilities.md) | 새 어빌리티를 만들 때 (베이스 선택, NetExecutionPolicy) |
| [Combat.md](Docs/Tech/Combat.md) | 무기 스폰/트레이스/히트 검증, 콤보 상태를 다룰 때 |
| [Montage.md](Docs/Tech/Montage.md) | 액션 몽타주 조회·재생·동기화를 다룰 때 |
| [AnimInstance.md](Docs/Tech/AnimInstance.md) | 애님 인스턴스 계층·로직 배치(Base/Character/Player/AI), 상태 머신 전제를 다룰 때 |
| [Spawner.md](Docs/Tech/Spawner.md) | **적을 레벨에 소환할 때** — 범위 안 플레이어 수 기반 웨이브 규칙, 내비메시 소환 지점, 폴링을 쓰는 이유 |
| [AI.md](Docs/Tech/AI.md) | AI 컨트롤러 계층(Base/Outlaw/Rogue), 두뇌 시작 게이트(SystemReady), BT/StateTree·NavMesh·CMC 이동 책임 분리를 다룰 때 |
| [Teams.md](Docs/Tech/Teams.md) | **적/아군/중립 판정을 다룰 때** — 진영 enum, 팀 데이터 소유(캐릭터), 전역 attitude solver, 퍼셉션 소속 필터 |
| [Locomotion.md](Docs/Tech/Locomotion.md) | 이동 기능을 다룰 때 — 개이트, 출발/정지 판정, ABP 상태 머신 배선(지상/공중), 피벗, 대시/Sprint, 점프, 관성화 |
| [Multiplayer.md](Docs/Tech/Multiplayer.md) | **모든 기능 설계 시** — 서버 권위, 함수 접두사(`Auth_`/`Local_`), 초기화 흐름, 네트워크 정책 |
| [GameFlow.md](Docs/Tech/GameFlow.md) | 레벨·게임모드를 추가하거나 **맵을 넘어 데이터를 넘겨야 할 때** — 메인메뉴/로비/게임플레이 흐름, 게임모드 계층, seamless travel, 맵 전환 시 생존 범위 |
| [SystemReady.md](Docs/Tech/SystemReady.md) | 캐릭터의 ASC·비동기 로드 에셋에 접근/의존할 때 — 준비 완료 신호까지 초기화를 미루는 패턴 |
| [UI.md](Docs/Tech/UI.md) | 캐릭터 UI(HUD 체력·머리 위 체력바)를 다룰 때 — 위젯/UI 컴포넌트/인터페이스 구조, 로컬 UI 원칙 |
| [EasyGameUI.md](Docs/Tech/EasyGameUI.md) | **시스템 UI(메인메뉴·일시정지·옵션)를 다룰 때 + 위젯을 새로 만들 때** — 에셋팩 채택 범위, 팩 에셋 수정 정책, UI 내비게이션 스택 규칙, 필수 등록 체크리스트 |
| [Input.md](Docs/Tech/Input.md) | 입력 바인딩·EnhancedInput·InputConfig를 다룰 때 |
| [GameplayTags.md](Docs/Tech/GameplayTags.md) | 게임플레이 태그를 추가·사용할 때 |
| [Components.md](Docs/Tech/Components.md) | 캐릭터 컴포넌트의 역할을 확인할 때 |
| [CodingConventions.md](Docs/Tech/CodingConventions.md) | **코드를 작성할 때** — 주석 스타일, `#pragma region`, 디폴트 매개변수 주석, 클래스 주석 동기화, 컴포넌트 결합 최소화 |

---

## 아키텍처 한눈에 보기 (상세는 위 문서 참조)

- **ASC 소유**: 플레이어(Chaser)는 `ACBPlayerState`, AI는 Character 자체가 ASC·AttributeSet 소유. → [ASC-Ownership.md](Docs/Tech/ASC-Ownership.md)
- **에셋 등록**: 캐릭터는 로드아웃 데이터 에셋 하나만 참조, 나머지 에셋은 전부 로드아웃이 등록·적용. → [Loadout.md](Docs/Tech/Loadout.md)
- **어빌리티**: 몽타주 액션은 `UCBInputActionAbility`(입력) / `UCBEventActionAbility`(이벤트) 중 상속, 엣지에서 `NetExecutionPolicy` 명시. → [Abilities.md](Docs/Tech/Abilities.md)
- **몽타주 동기화**: 액션 몽타주는 GameplayCue(`GameplayCue.PlayAction`)로 전 클라 동기화, 콤보 인덱스는 큐 파라미터로 전달. → [Montage.md](Docs/Tech/Montage.md)
- **애님 인스턴스**: `Base → Character(공통) → Player/AI → Chaser/Outlaw` 계층, 애니메이션은 상태 머신(모션 매칭 미사용). → [AnimInstance.md](Docs/Tech/AnimInstance.md)
- **로코모션**: 개이트=ASC 태그(`Status.Movement.Gait.*`), 출발/정지=속도 비율 판정, 피벗=입력 잠금, Sprint=전방 대시에 종속, 점프=GAS 어빌리티(몽타주 없음), 전이 블렌드=관성화. → [Locomotion.md](Docs/Tech/Locomotion.md)
- **캐릭터 UI**: `UCBUIComponent`가 준비 완료 후 로컬 플레이어=HUD/그 외=머리 위 바 생성, 위젯 클래스는 로드아웃 등록, 값 동기화는 어트리뷰트 복제(UI용 네트워크 코드 없음), 외부 접근은 `ICBUIInterface`. → [UI.md](Docs/Tech/UI.md)
- **시스템 UI**: 에셋팩 `Easy Game UI 3.0` 도입(세이브·포토모드 등은 미채택). 팩 원본 수정 금지·복제 사용, 모든 위젯은 HUD 스택 경유(`AddToViewport` 금지), 시스템 UI 에셋은 Global Config에 등록. → [EasyGameUI.md](Docs/Tech/EasyGameUI.md)
- **멀티플레이**: 리슨 서버·서버 권위, 로컬 감지 → 서버 검증, Simulated Proxy는 GameplayCue/GE로 반영. → [Multiplayer.md](Docs/Tech/Multiplayer.md)
- **의상 커스터마이징**: 부위 슬롯(`ECBCosmeticSlot`) × 파츠 태그(`Item.Cosmetic.*`). 기본 의상은 로드아웃(하드), 교체 목록은 카탈로그(파츠 메시만 소프트). **복제하는 건 조합 태그뿐이고 조립은 각 인스턴스 로컬**. → [Cosmetic.md](Docs/Tech/Cosmetic.md)
- **진영(팀)**: `ECBTeam`(Chaser/Outlaw/Neutral, Outlaw·Rogue는 같은 팀). 팀 데이터는 **컨트롤러가 아니라 캐릭터가 복제 소유**(폰은 전 클라에 존재 → AI 판정과 UI 색을 한 번에), AI 컨트롤러는 폰에 위임. 중립은 엔진 기본 solver로 표현 불가라 `UCBGameInstance`가 전역 attitude solver를 등록. 퍼셉션은 적만 감지. → [Teams.md](Docs/Tech/Teams.md)
- **게임 플로우**: 메인메뉴 → 로비 → 게임플레이. 게임모드는 `ACBGameModeBase` 아래 로비/게임플레이로 분기하고, 공통 기본값(seamless travel·PC·PlayerState 클래스)은 베이스가 고정. 맵을 넘는 값은 `UGameInstance` 또는 PlayerState의 `CopyProperties`. 접속(호스트=`listen` 오픈 / 참가=주소 `ClientTravel`)과 접속 실패 복귀는 전부 `UCBSessionSubsystem` 하나. **무기 변경 = 캐릭터 변경 = 폰 재스폰**이며, 고를 목록은 `UCBCharacterCatalog`(게임 인스턴스 등록), 스폰 클래스는 `ACBGameModeBase`가 PlayerState의 선택 태그로 결정. 닉네임은 새 변수 없이 엔진 `PlayerName`을 그대로 쓰고(복제·맵 이관 내장), 기본값 `PlayerN` 부여와 다듬기·중복 접미사는 게임모드 담당. **사망 후 리스폰도 폰 재스폰**이며, 지연·스폰 지점 규칙은 `ACBGameplayGameMode`가 소유하고 스폰 전에 사망 GE·로드아웃 부여분을 회수한다(ASC가 PlayerState 소유라 폰을 갈아도 남기 때문). → [GameFlow.md](Docs/Tech/GameFlow.md)
- **적 소환**: 레벨 배치 스포너 `ACBEnemySpawner`가 구 범위 안 살아있는 플레이어 수 × N마리를 소환. **전멸 전에는 새 웨이브가 없고**(인원이 늘어난 만큼만 증원), 전멸하면 대기 후 반복. 인원 집계·전멸 감지는 오버랩이 아니라 스캔 폴링 하나(폰 재스폰에 영향받지 않음). 소환 지점은 범위 안 내비메시의 도달 가능 지점. → [Spawner.md](Docs/Tech/Spawner.md)
- **시스템 준비(Ready)**: 캐릭터 비동기 초기화 완료까지 의존 로직을 미룸. 컴포넌트/애님은 `OnCharacterSystemReady()` 훅 + `IsSystemLocked()` 게이트로 소비. → [SystemReady.md](Docs/Tech/SystemReady.md)
