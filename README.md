# ChainBurst

> **Unreal Engine 5.8 · C++ · GAS** 기반 멀티플레이어 3인칭 액션 게임
> (리슨 서버 구조의 co-op 액션)

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.8-0E1128?logo=unrealengine)
![GAS](https://img.shields.io/badge/GameplayAbilitySystem-사용-5A2D82)
![Network](https://img.shields.io/badge/Network-Listen%20Server%20·%20서버%20권위-2E7D32)

---

## 🎬 데모 영상

[![Watch the video](https://img.youtube.com/vi/4FiAOKTMSoU/maxresdefault.jpg)](https://youtu.be/4FiAOKTMSoU)

---

## 📖 프로젝트 상세 기획 및 개발 일지

[![Notion](https://img.shields.io/badge/Notion-Project_CB_개발문서-000000?style=for-the-badge&logo=notion&logoColor=white)](https://sprout-whitefish-298.notion.site/project-cb?source=copy_link)

---

## 🔎 프로젝트 개요

| 항목 | 내용 |
|---|---|
| **장르** | 멀티플레이어 협동(PvE) 3인칭 액션 |
| **엔진** | Unreal Engine 5.8 |
| **개발 기간** | 2026.03 ~ 2026.10 (개발 완료) |
| **인원** | 1인 개인 프로젝트 |
| **담당 범위** | 기획·설계·C++ 구현. 아트·애니메이션은 마켓플레이스 에셋 |
| **네트워크** | 리슨 서버(Listen Server), 서버 권위(Server-Authoritative) · 세션은 EOS(온라인)와 LAN 중 메인 메뉴에서 선택 |
| **주요 플러그인** | GameplayAbilities, MotionWarping, AnimationWarping, EnhancedInput, OnlineServices(EOS·Null), Niagara |
| **C++ / BP 경계** | 게임플레이 로직·규칙·네트워크는 전부 C++. 블루프린트는 데이터 에셋 구성과 연출 조립만 |

---

## 📁 폴더 구조

```
Source/ChainBurst/              ※ Public/ · Private/ 가 같은 하위 구조를 가짐
├── CBGameplayTags              네이티브 게임플레이 태그
├── CBAbilitySystemLibrary      ASC·태그 조회 공용 함수
├── AbilitySystem/              ASC, AttributeSet
│   ├── Abilities/              어빌리티 베이스 계층 (Gameplay → Action → Input / Event)
│   │   ├── Combat/             공격·사망·피격·무기 장착, 버스트·기절 게이지·적중 회복 패시브
│   │   ├── Movement/           대시·점프·이동 속도
│   │   └── Fragments/          여러 어빌리티가 소유하는 기능 조각 (무기 트레이스·영역 공격·투사체)
│   ├── GEExecCalc/             데미지 계산
│   └── Tasks/                  어빌리티 태스크 (몽타주 블렌드 아웃 대기)
├── Characters/                 캐릭터 5종 계층 (Base → Chaser / AI → Outlaw · Rogue)
├── Components/                 캐릭터 기능 컴포넌트
│   ├── Animation/              액션 몽타주 재생·콤보
│   ├── Camera/                 카메라 줌·제어
│   ├── Combat/                 무기 소유·히트 처리 (공통 → Chaser / Outlaw / Rogue)
│   ├── Input/                  입력 바인딩·피벗 감지
│   ├── Mesh/                   모듈러 외형(의상) 조립
│   ├── Movement/               가감속·이동 상태 태그, 캐릭터 회전
│   ├── Perception/             AI 청각용 소음 발생
│   └── UI/                     캐릭터 UI 관리 (HUD·머리 위 체력바)
├── AnimInstances/              애님 인스턴스 계층 (Base → Character → Player / AI → Chaser / Outlaw)
├── AnimNotify(State)/          소유자에게 게임플레이 이벤트 전송 (단발 / 구간)
├── MotionWarping/              루트모션 모디파이어 (거리 상한·일정 속도 돌진·고정 각도 회전)
├── Controllers/                플레이어 컨트롤러, AI 컨트롤러 계층 (Base → Outlaw / Rogue)
├── PlayerState/                플레이어 ASC·AttributeSet 소유, 로비 선택값·매치 데미지 기록
├── GameModes/                  Base → Lobby / Gameplay (경험치·레벨업·매치 종료 규칙)
├── GameStates/                 Base → Lobby / Gameplay (공유 경험치·경과 시간 복제)
├── Core/                       GameInstance, 서브시스템 (로그인·세션·BGM·로컬 준비 완료), 접속 끊김 처리
├── AssetManager/               비동기 에셋 로드
├── DataAssets/
│   ├── Loadout/                캐릭터별 로드아웃 (Character → Chaser / AI → Outlaw · Rogue)
│   ├── Character/              선택 가능한 캐릭터(무기) 카탈로그
│   ├── Weapon/                 무기 데이터·소켓 위치
│   ├── Animation/              액션 태그별 몽타주 테이블
│   ├── Movement/               개이트별 이동 수치
│   ├── Input/                  InputConfig
│   ├── Cosmetic/               의상 카탈로그
│   └── LevelUp/                경험치 곡선·카드·등급별 뽑기 가중치
├── GameplayCues/               몽타주 재생·정지·섹션 점프 (전 클라 동기화), 생성·소멸 연출
├── AI/
│   ├── BehaviorTree/           BT 태스크·서비스·데코레이터·컴포지트
│   └── EQS/                    EQS 컨텍스트
├── Spawn/                      적 웨이브 스포너, 보스 구역
├── Items/
│   ├── Weapons/                무기 액터 (Base → Chaser / Outlaw / Rogue, 맨손 몬스터용 본체 무기)
│   └── Projectiles/            투사체
├── Camera/                     로비 고정 카메라
├── UI/                         HUD
│   └── Widgets/                C++ 위젯 베이스 (체력·버스트·기절·경험치·보스 바·레벨업 카드·상태창 등)
├── Interfaces/                 전투·UI 인터페이스
├── Types/                      공용 enum·struct·델리게이트·콜리전 채널 별칭
└── Editor/                     에디터 전용 머티리얼 정리 명령
```

**프로젝트 기능 컴포넌트는 전부 `UCBExtensionComponent`를 상속**함. 이 베이스가 "캐릭터 초기화 완료 신호를 구독하거나, 이미 끝났으면 즉시 실행"하는 처리를 한 곳에서 담당하므로, 개별 컴포넌트는 `OnCharacterSystemReady()`만 오버라이드하면 초기화 순서를 신경 쓰지 않아도 됨.

---

## 💡 주요 시스템 요약

| 문서 | 내용 |
|---|---|
| [1. 캐릭터 아키텍처](Docs/SubReadme/01-character-architecture.md) | 캐릭터 계층 구조와 로드아웃 |
| [2. 어빌리티 아키텍처](Docs/SubReadme/02-ability-architecture.md) | 어빌리티 계층 구조와 종류별 실행 흐름 |
| [3. 액션 시스템](Docs/SubReadme/03-action-system.md) | 몽타주 재생·정지 경로와 액션 어빌리티 동기화 |
| [4. 이동/로코모션 시스템](Docs/SubReadme/04-locomotion-system.md) | 개이트 태그·회전·피벗 처리와 애님BP |
| [5. 입력 시스템](Docs/SubReadme/05-input-system.md) | 입력 시스템 구조와 역할 분리 |
| [6. 전투 시스템](Docs/SubReadme/06-combat-system.md) | 무기·본체 무기 구조, 등록과 수명, 트레이스 스윕과 히트 검증 |
| [7. 멀티플레이 구조](Docs/SubReadme/07-multiplayer-architecture.md) | 로그인·세션 생성·검색·참가 흐름과 접속 실패 처리 |
| [8. AI 전투](Docs/SubReadme/08-AI-combat.md) | 세 감각 감지, 타겟 점수·보스 집중 규칙, BT 커스텀 노드와 거리 구역 전투 사이클 |
| [9. 의상 시스템](Docs/SubReadme/09-cosmetic-system.md) | 의상 구조와 교체 및 복제 |
| [10. 로비 구조](Docs/SubReadme/10-lobby-system.md) | 로비의 요청·검증·상태 분리, 닉네임·캐릭터 변경·준비와 게임 시작 |

---

## 📄 기술 문서

설계 규칙과 시스템별 상세 구조를 담은 문서. `Docs/Tech/` 아래 폴더는 **언제 읽는 문서인가**로 나뉨.

### Claude

| 문서 | 내용 |
|---|---|
| [CLAUDE.md](CLAUDE.md) | 프로젝트 전역 규칙과 기술 문서 인덱스 |

### 설계 규칙 (`Conventions/`)

코드를 쓰기 전에 모든 작업에 공통으로 적용하는 규칙.

| 문서 | 내용 |
|---|---|
| [Multiplayer.md](Docs/Tech/Conventions/Multiplayer.md) | 서버 권위 구조, 함수 접두사(`Auth_`/`Local_`) 규칙, 로컬 감지 → 서버 검증, 어빌리티 네트워크 정책 |
| [CodingConventions.md](Docs/Tech/Conventions/CodingConventions.md) | 주석 스타일, `#pragma region` 구조, 컴포넌트 결합 최소화 규칙 |
| [DesignReview.md](Docs/Tech/Conventions/DesignReview.md) | 설계 승인 직전에 중복·도달 불가·관측 불가 항목을 덜어내는 검토 절차 |
| [AssetReference.md](Docs/Tech/Conventions/AssetReference.md) | 하드/약한/소프트 참조 선택 기준과 비동기 로드 경계 |

### 기반 (`Foundation/`)

캐릭터 골격·초기화·데이터.

| 문서 | 내용 |
|---|---|
| [ASC-Ownership.md](Docs/Tech/Foundation/ASC-Ownership.md) | ASC·AttributeSet 소유 주체(플레이어=PlayerState / AI=Character)와 캐싱 |
| [SystemReady.md](Docs/Tech/Foundation/SystemReady.md) | 비동기 초기화 완료 신호. 의존 로직을 준비 완료까지 미루는 게이트 패턴 |
| [Loadout.md](Docs/Tech/Foundation/Loadout.md) | 캐릭터 에셋을 로드아웃 하나로 일괄 등록·적용하는 데이터 에셋 패턴 |
| [GameplayTags.md](Docs/Tech/Foundation/GameplayTags.md) | 태그 4역할 분류(식별·속성·상태·이벤트), 상태 태그 소유권과 복제 규칙 |
| [Teams.md](Docs/Tech/Foundation/Teams.md) | 진영 enum, 캐릭터가 복제 소유하는 팀 데이터, 전역 attitude solver |
| [Components.md](Docs/Tech/Foundation/Components.md) | 캐릭터에 붙는 주요 컴포넌트와 각자의 책임 |

### 게임플레이 (`Gameplay/`)

전투·성장·이동·AI.

| 문서 | 내용 |
|---|---|
| [Abilities.md](Docs/Tech/Gameplay/Abilities.md) | 어빌리티 베이스 계층과 상속 선택 기준, `NetExecutionPolicy` 결정, 피격·사망·적중 회복 |
| [ActionFragment.md](Docs/Tech/Gameplay/ActionFragment.md) | 베이스에는 공용 로직만 두고 기능은 조각(무기 트레이스·영역 공격·투사체)으로 소유하는 구조와 그 이유 |
| [Combat.md](Docs/Tech/Gameplay/Combat.md) | 무기 스폰·수명 소유, 트레이스와 서버 히트 검증, 영역·투사체 판정, 본체 무기, 무기 오라 |
| [Montage.md](Docs/Tech/Gameplay/Montage.md) | 액션 태그+인덱스 몽타주 조회, GameplayCue 기반 재생·정지·섹션 점프 동기화, 모션 워핑 |
| [Burst.md](Docs/Tech/Gameplay/Burst.md) | 적중으로 쌓은 게이지로 발동하는 피버 타임. 적립 패시브·발동 어빌리티·버프 GE 분리 |
| [LevelUp.md](Docs/Tech/Gameplay/LevelUp.md) | 전 플레이어 공유 경험치, 인원 비례 필요량, 레벨업 시 월드 정지와 플레이어별 카드 선택 |
| [Stun.md](Docs/Tech/Gameplay/Stun.md) | 보스 기절(그로기) 게이지. 피격·스킬 사용으로 적립, 가득 차면 기절 어빌리티 발동 |
| [Locomotion.md](Docs/Tech/Gameplay/Locomotion.md) | 개이트·출발/정지 판정·피벗·대시/스프린트·점프, ABP 상태 머신 배선 |
| [Input.md](Docs/Tech/Gameplay/Input.md) | EnhancedInput 기반 입력 태그 바인딩, InputConfig 주입, UI가 게임 입력을 막는 방식 |
| [AI.md](Docs/Tech/Gameplay/AI.md) | AI 컨트롤러 계층과 두뇌 시작 게이트, 감지·타겟 선정, BT 커스텀 노드와 등급별 전투 사이클 |
| [Spawner.md](Docs/Tech/Gameplay/Spawner.md) | 범위 안 플레이어 수 기반 웨이브 소환, 내비메시 소환 지점 탐색 |
| [BossArena.md](Docs/Tech/Gameplay/BossArena.md) | 전원 진입 대기·카운트다운·등장 연출·입구 봉쇄, 인원 비례 보스 내구도, 플레이어별 졸개 소환 |

### 표현 (`Presentation/`)

애니메이션·UI·외형·연출.

| 문서 | 내용 |
|---|---|
| [AnimInstance.md](Docs/Tech/Presentation/AnimInstance.md) | 애님 인스턴스 계층(Base→Character→Player/AI)과 로직 배치 기준 |
| [UI.md](Docs/Tech/Presentation/UI.md) | 캐릭터 UI(HUD·머리 위 체력바·보스 바·상태창·클리어 화면) 구조. 값 동기화는 어트리뷰트 복제 전담 |
| [EasyGameUI.md](Docs/Tech/Presentation/EasyGameUI.md) | 시스템 UI(메인메뉴·일시정지·옵션) 에셋팩 채택 범위와 위젯 스택 규칙 |
| [Cosmetic.md](Docs/Tech/Presentation/Cosmetic.md) | 부위 슬롯×파츠 태그 의상 교체. 조합 태그만 복제하고 조립은 로컬 |
| [SpawnFX.md](Docs/Tech/Presentation/SpawnFX.md) | 캐릭터 생성·소멸 텔레포트 디졸브 연출. 캐릭터가 스스로 재생하고 로드아웃이 연출을 지정 |
| [Music.md](Docs/Tech/Presentation/Music.md) | 각 머신이 로컬로 재생하는 BGM, 기본 곡과 보스 곡 크로스페이드 |
| [Lighting.md](Docs/Tech/Presentation/Lighting.md) | 스카이박스 하늘과 SkyLight 실시간 캡처, 첫 로드에 맵이 어두워지는 원인 |
| [SkeletonCompatibility.md](Docs/Tech/Presentation/SkeletonCompatibility.md) | 새 메시와 애니메이션 스켈레톤 호환 진단, 커스텀 본 추가·리타게팅 |

### 게임 흐름 (`Flow/`)

레벨·접속·로비.

| 문서 | 내용 |
|---|---|
| [GameFlow.md](Docs/Tech/Flow/GameFlow.md) | 게임모드 계층과 레벨 전환, 맵을 넘어 살아남는 데이터, 폰 재스폰, 매치 종료와 클리어 화면 |
| [Session.md](Docs/Tech/Flow/Session.md) | 로그인(EOS·LAN), 세션 생성·검색·참가, 접속 실패 복귀와 나가기 |
| [Lobby.md](Docs/Tech/Flow/Lobby.md) | 로비 캐릭터 스폰, 닉네임, 준비 완료와 게임 시작, 고정 카메라·페이드, 로비 복귀 처리 |

---

## 🔧 주요 트러블슈팅

겪은 문제와 그 원인·해결. 각 항목은 노션 개발 문서의 상세 페이지.

| 주제 | 증상 | 원인 → 해결 |
|---|---|---|
| 트레이스 | [빠른 스윙이 적을 그대로 통과](https://sprout-whitefish-298.notion.site/v1-2_-3792a74e9c6f807e97a3e9c42c4720f2?pvs=143) | 몽타주가 빨라 프레임 간 무기 이동량이 커지면서 트레이스 사이에 빈 공간 발생 → 무기의 세로선 대신 **이전 프레임과 현재 프레임 위치 사이**를 분할 스윕 |
| 트레이스 | [2배속 재생 시 스윙 후반부가 판정되지 않음](https://sprout-whitefish-298.notion.site/3902a74e9c6f80d2b8c1eb5027a39905?pvs=143) | 몽타주 카운터는 2배로 도는데 Blend In 0.25초가 노티파이 구간 전체를 덮어 실제 포즈가 못 따라옴 → 블렌드 인 · 아웃 시간을 재생 속도로 나눠 배속과 무관하게 같은 비율만 블렌드 |
| 애니메이션 | [출발·정지 도중 개이트가 바뀌면 동작이 끊김](https://sprout-whitefish-298.notion.site/v1-9-39b2a74e9c6f80f2a119fedcd56840d5?pvs=143) | Select 노드가 매 프레임 재평가되어 재생 중 클립이 스왑됨 → 스테이트 진입 시점(On Become Relevant)에 개이트를 스냅샷해 그 값으로 고정 출력 |
| 애니메이션 | [모션 워핑이 멀어지는 타겟을 끝까지 따라감](https://sprout-whitefish-298.notion.site/AI-v1-3-3ce2a74e9c6f80ad84e4fa63a07b81eb?pvs=143) | 엔진 SkewWarp의 속도 상한은 루트모션 이동이 있는 클립에만 걸려 제자리 클립엔 제한이 없음 → SkewWarp를 상속해 워프 시작점 기준 최대 거리 초과분만 잘라내는 모디파이어 작성(회전은 유지해 타겟은 계속 조준) |
| 애니메이션 | [급격한 방향 전환에서 상체가 뒤틀림](https://sprout-whitefish-298.notion.site/v1-6_-39a2a74e9c6f807b98d0f05389a31183?pvs=143) | 블렌드 타임 동안 좌·우 반대 방향 포즈가 그대로 섞임 → 트랜지션 블렌드를 전부 관성화로 교체해 직전 포즈 스냅샷에서 이어붙이도록 변경 |
| 애니메이션 | [무기 장착 몽타주가 끝난 뒤에야 전투 포즈로 바뀜](https://sprout-whitefish-298.notion.site/3c52a74e9c6f80288516fb62f5b60f1e?pvs=143) | FullBody 슬롯 재생 중 소스 포즈가 갱신되지 않아 하위 로코모션 전환이 멈춰 있음 → 슬롯의 소스 포즈 상시 업데이트 옵션을 켜서 재생 중에도 전환 진행 |
| 애니메이션 | [이동 중 루트모션을 재생하면 방향이 끌려감](https://sprout-whitefish-298.notion.site/3c52a74e9c6f808aab36d946d38cb144?pvs=143) | 회전 컴포넌트가 매 틱 액터 회전을 덮어써, 다음 틱 루트모션을 월드로 변환하는 기준 자체가 바뀜 → 루트모션 재생 중에는 회전 덮어쓰기를 차단 |
| 멀티플레이 | [원격 화면에서만 전투/비전투 전환이 한 박자 늦음](https://sprout-whitefish-298.notion.site/3ca2a74e9c6f800c99d7e2ca11afb3b9?pvs=143) | 태그 복제는 ASC를 소유한 PlayerState의 갱신 주기를 따르는데 그 기본값이 1Hz → PlayerState 갱신 주기 상향. **복제 컴포넌트는 자기 주기 없이 소유 액터를 따른다** |
| 멀티플레이 | [원격 화면에서 캐릭터 메시가 공중에 뜸](https://sprout-whitefish-298.notion.site/3a32a74e9c6f8005bb5ed2b06222a3bb?pvs=143) | 시뮬 프록시 스무딩이 기준으로 쓰는 오프셋은 스폰 시 1회만 캐싱되어, 로드아웃이 런타임에 적용한 메시 오프셋이 반영되지 않음 → 오프셋 적용과 함께 캐시도 재계산 |
| 멀티플레이 | [클라이언트에서 재생한 몽타주가 중간에 끊김](https://sprout-whitefish-298.notion.site/3c42a74e9c6f80dabefefbf3b84ab0fa?pvs=143) | 종료 이벤트를 먼저 받은 쪽이 어빌리티 종료를 복제해 반대쪽까지 정리 → 서버가 몽타주 중지 전에 끝남. 몽타주 재생·중지를 어빌리티에서 떼어내 GameplayCue로 이관 |
| 멀티플레이 | [이미 닫힌 방이 세션 목록에 계속 남음](https://sprout-whitefish-298.notion.site/3ca2a74e9c6f804b9f02dd80c6c56b11?pvs=143) | 엔진 LAN 비콘이 검색으로 발견한 세션까지 캐시에 영구 보관하고, 소유자 검사 없이 캐시 전체를 광고 → 방 개설 직전 온라인 서비스 인스턴스를 재생성해 캐시를 비움(LAN 한정) |
| 충돌 | [적과 부딪히면 멀리 튕겨나감](https://sprout-whitefish-298.notion.site/3d12a74e9c6f80299a41fb8452c610bb?pvs=143) | 적 캡슐 위에 착지하면 그 적이 무빙 베이스가 되어 루트모션·모션워프 속도가 그대로 더해짐 → 캡슐을 걸을 수 없는 면으로 선언하고 캡슐 겹침 시 밀어내기 거리 축소 |
