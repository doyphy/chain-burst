# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

이 파일은 **항상 로드되는 핵심 규칙과 인덱스**만 담는다. 특정 시스템의 상세 설계는 아래 인덱스가 가리키는 `Docs/Tech/**/*.md` 문서를 **작업 전에 읽고** 따른다. 요약을 여기 중복해 두지 않는다 — 코드가 바뀌면 문서만 고치면 되게 한다.

---

## 절대 규칙

- **코드 추가/수정 전 반드시 사용자 허락을 받아야 한다.** 변경 계획을 먼저 설명하고 승인 후 편집할 것.
  - 승인을 요청하기 **직전에 [DesignReview.md](Docs/Tech/Conventions/DesignReview.md)의 검토 절차를 거친다.** 중복·도달 불가·관측 불가 항목을 덜어내고 나서 내놓을 것.
- **엔진 소스 및 플러그인 코드는 절대 편집하지 않는다.** 읽기 전용 참조만 허용.
  - 엔진 루트: `D:\UE_5.8\Engine`
  - 소스: `D:\UE_5.8\Engine\Source`
  - 플러그인: `D:\UE_5.8\Engine\Plugins`
- **모든 기능은 멀티플레이(리슨 서버, 서버 권위)를 전제로 설계한다.** → [Multiplayer.md](Docs/Tech/Conventions/Multiplayer.md)
- **시스템을 수정·확장하면 해당 `Docs/Tech/**/*.md` 문서도 함께 갱신한다.** 코드와 문서가 어긋나지 않게 유지할 것.

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

## 문서 인덱스 (`Docs/Tech/`)

폴더는 **언제 필요한가**로 나뉜다. 해당 시스템을 건드리기 전에 그 문서를 읽는다 — 인덱스의 한 줄만 보고 넘기지 않는다.

### `Conventions/` — 코드를 쓰기 전에 (모든 작업 공통)
| 문서 | 언제 읽나 |
|---|---|
| [Multiplayer.md](Docs/Tech/Conventions/Multiplayer.md) | **모든 기능 설계 시** — 리슨 서버·서버 권위, 함수 접두사(`Auth_`/`Local_`), 로컬 감지 → 서버 검증, Simulated Proxy는 GameplayCue/GE로 반영 |
| [CodingConventions.md](Docs/Tech/Conventions/CodingConventions.md) | **코드를 작성할 때** — 주석 스타일, `#pragma region`, 디폴트 매개변수 주석, 클래스 주석 동기화, 컴포넌트 결합 최소화 |
| [DesignReview.md](Docs/Tech/Conventions/DesignReview.md) | **설계를 마치고 승인을 요청하기 직전** — 중복·도달 불가·관측 불가 항목을 덜어내는 마지막 점검 |
| [AssetReference.md](Docs/Tech/Conventions/AssetReference.md) | 멤버로 에셋/오브젝트를 참조할 때 — 하드/약한/소프트 선택, 비동기 로드 |

### `Foundation/` — 캐릭터 골격·초기화·데이터
| 문서 | 언제 읽나 |
|---|---|
| [ASC-Ownership.md](Docs/Tech/Foundation/ASC-Ownership.md) | ASC/AttributeSet 소유·캐싱, 초기화 진입점 — 플레이어(Chaser)는 `ACBPlayerState`, AI는 Character 자체가 소유. 공용 함수를 어디에 둘지(`UCBAbilitySystemLibrary`)도 여기 |
| [SystemReady.md](Docs/Tech/Foundation/SystemReady.md) | 캐릭터의 ASC·비동기 로드 에셋에 접근/의존할 때 — 준비 완료 신호까지 미루기, `OnCharacterSystemReady()` 훅 + `IsSystemLocked()` 게이트 |
| [Loadout.md](Docs/Tech/Foundation/Loadout.md) | 캐릭터에 필요한 에셋(메쉬/어빌리티/무기/GE/입력) 등록·적용 — 캐릭터는 로드아웃 데이터 에셋 하나만 참조하고 나머지는 전부 로드아웃이 적용 |
| [GameplayTags.md](Docs/Tech/Foundation/GameplayTags.md) | 게임플레이 태그를 추가·사용할 때 — 태그의 4역할, 상태 태그 소유·복제 규칙, 네임스페이스 인덱스 |
| [Teams.md](Docs/Tech/Foundation/Teams.md) | **적/아군/중립 판정을 다룰 때** — `ECBTeam`(Outlaw·Rogue는 같은 팀), 팀 데이터는 컨트롤러가 아니라 **캐릭터가 복제 소유**, 중립 때문에 `UCBGameInstance`가 전역 attitude solver 등록 |
| [Components.md](Docs/Tech/Foundation/Components.md) | 캐릭터 컴포넌트의 역할을 확인할 때 |

### `Gameplay/` — 전투·이동·AI
| 문서 | 언제 읽나 |
|---|---|
| [Abilities.md](Docs/Tech/Gameplay/Abilities.md) | 새 어빌리티를 만들 때 — 몽타주 액션은 `UCBInputActionAbility`(입력)/`UCBEventActionAbility`(이벤트) 중 상속, 엣지에서 `NetExecutionPolicy` 명시. 여러 어빌리티가 쓰는 기능은 기능 조각(예: `UCBFragment_WeaponTrace`)을 소유해 호출 |
| [ActionFragment.md](Docs/Tech/Gameplay/ActionFragment.md) | **어빌리티 구조를 바꾸거나 여러 어빌리티가 쓸 기능을 만들 때** — 베이스(`UCBGameplayAbility`)에는 공용 로직만, 기능은 새 베이스 클래스 대신 조각으로 빼서 필요한 어빌리티가 멤버로 소유·호출. 조각 크기는 "늘 함께 쓰이는 기능 묶음" 단위 |
| [Combat.md](Docs/Tech/Gameplay/Combat.md) | 무기 스폰/트레이스/히트 검증, 콤보 상태를 다룰 때 |
| [Montage.md](Docs/Tech/Gameplay/Montage.md) | 액션 몽타주 조회·재생·동기화 — GameplayCue(`GameplayCue.PlayAction`)로 전 클라 동기화, 콤보 인덱스는 큐 파라미터로 전달 |
| [Locomotion.md](Docs/Tech/Gameplay/Locomotion.md) | 이동 기능을 다룰 때 — 개이트=ASC 태그(`Status.Movement.Gait.*`), 출발/정지=속도 비율 판정, 피벗=입력 잠금, Sprint는 전방 대시에 종속, 점프=GAS 어빌리티, 전이 블렌드=관성화 |
| [AI.md](Docs/Tech/Gameplay/AI.md) | AI 컨트롤러 계층(Base/Outlaw/Rogue), 두뇌 시작 게이트(SystemReady), BT/StateTree·NavMesh·CMC 이동 책임 분리를 다룰 때 |
| [Spawner.md](Docs/Tech/Gameplay/Spawner.md) | **적을 레벨에 소환할 때** — 범위 안 살아있는 플레이어 수 × N마리, 전멸 전에는 새 웨이브 없음, 집계는 스캔 폴링, 소환 지점은 내비메시 도달 가능 지점 |
| [Input.md](Docs/Tech/Gameplay/Input.md) | 입력 바인딩·EnhancedInput·InputConfig를 다룰 때 — 매핑 컨텍스트 등록, UI가 게임 입력을 막는 방식(IMC 제거) |

### `Presentation/` — 애니메이션·UI·외형
| 문서 | 언제 읽나 |
|---|---|
| [AnimInstance.md](Docs/Tech/Presentation/AnimInstance.md) | 애님 인스턴스 계층·로직 배치 — `Base → Character(공통) → Player/AI → Chaser/Outlaw`, 애니메이션은 상태 머신(모션 매칭 미사용) |
| [UI.md](Docs/Tech/Presentation/UI.md) | 캐릭터 UI(HUD 체력·머리 위 체력바)를 다룰 때 — `UCBUIComponent`가 준비 완료 후 생성, 위젯 클래스는 로드아웃 등록, 값 동기화는 어트리뷰트 복제(UI용 네트워크 코드 없음), 외부 접근은 `ICBUIInterface` |
| [EasyGameUI.md](Docs/Tech/Presentation/EasyGameUI.md) | **시스템 UI(메인메뉴·일시정지·옵션)를 다룰 때 + 위젯을 새로 만들 때** — 팩 원본 수정 금지·복제 사용, 모든 위젯은 HUD 스택 경유(`AddToViewport` 금지), 시스템 UI 에셋은 Global Config에 등록 |
| [Cosmetic.md](Docs/Tech/Presentation/Cosmetic.md) | **의상 커스터마이징을 다룰 때** — 부위 슬롯(`ECBCosmeticSlot`) × 파츠 태그(`Item.Cosmetic.*`), 기본 의상은 로드아웃(하드)·교체 목록은 카탈로그(소프트), **복제하는 건 조합 태그뿐이고 조립은 각 인스턴스 로컬** |
| [SkeletonCompatibility.md](Docs/Tech/Presentation/SkeletonCompatibility.md) | **새 캐릭터 메시를 적용할 때** — 애니메이션 스켈레톤과의 호환 진단, 커스텀 본 추가, 리타게팅 모드 |

### `Flow/` — 레벨·접속·로비
| 문서 | 언제 읽나 |
|---|---|
| [GameFlow.md](Docs/Tech/Flow/GameFlow.md) | 레벨·게임모드를 추가하거나 **맵을 넘어 데이터를 넘겨야 할 때** — 게임모드 계층, seamless travel, `CopyProperties`, 그리고 **폰 재스폰 두 갈래**(무기 변경 = 캐릭터 변경 / 사망 후 리스폰) |
| [Session.md](Docs/Tech/Flow/Session.md) | **접속을 다룰 때** — 호스트(`listen` 오픈)/참가(주소 `ClientTravel`), 세션 생성·검색·참가, 접속 실패 복귀, 나가기. 전부 `UCBSessionSubsystem` 하나 |
| [Lobby.md](Docs/Tech/Flow/Lobby.md) | 로비 레벨을 다룰 때 — 실제 Chaser를 스폰하는 이유, 닉네임(엔진 `PlayerName` 사용), 준비 완료·게임 시작, 고정 카메라·페이드, 로비 UI 월드 투영 |
