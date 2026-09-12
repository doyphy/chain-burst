# ChainBurst

> **Unreal Engine 5.8 · C++ · GAS** 기반 멀티플레이어 3인칭 액션 게임
> (리슨 서버 구조의 co-op 액션)

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.8-0E1128?logo=unrealengine)
![GAS](https://img.shields.io/badge/GameplayAbilitySystem-사용-5A2D82)
![Network](https://img.shields.io/badge/Network-Listen%20Server%20·%20서버%20권위-2E7D32)

---

## 🎬 데모 영상 (v0.14.2 기준)

[![Watch the video](https://img.youtube.com/vi/Nx3biih8VoA/maxresdefault.jpg)](https://www.youtube.com/watch?v=Nx3biih8VoA)

---

## 📖 프로젝트 상세 기획 및 개발 일지

[![Notion](https://img.shields.io/badge/Notion-Project_CB_개발문서-000000?style=for-the-badge&logo=notion&logoColor=white)](https://sprout-whitefish-298.notion.site/project-cb?source=copy_link)

---

## 🔎 프로젝트 개요

| 항목 | 내용 |
|---|---|
| **장르** | 멀티플레이어 3인칭 액션 |
| **엔진** | Unreal Engine 5.8 |
| **개발 기간** | 2026.03 ~ 진행 중 |
| **인원** | 1인 개인 프로젝트 |
| **담당 범위** | 기획·설계·C++ 구현. 아트·애니메이션은 마켓플레이스 에셋 |
| **네트워크** | 리슨 서버(Listen Server), 서버 권위(Server-Authoritative) |
| **주요 플러그인** | GameplayAbilities, MotionWarping, AnimationWarping, EnhancedInput, OnlineServices |
| **C++ / BP 경계** | 게임플레이 로직·규칙·네트워크는 전부 C++. 블루프린트는 데이터 에셋 구성과 연출 조립만 |

---

## 📁 폴더 구조

```
Source/ChainBurst/
├── AbilitySystem/        ASC, AttributeSet, 어빌리티 계층, 데미지 ExecCalc
│   └── Abilities/        Combat(공격·사망·피격·장착) / Movement(대시·점프·속도)
├── Characters/           캐릭터 5종 계층
├── Components/           캐릭터 기능 단위 (Combat / Movement / Animation / Camera / Input / Mesh / Perception / UI)
├── AnimInstances/        애님 인스턴스 계층 (Base → Character → Player/AI)
├── Controllers/          플레이어·AI 컨트롤러 계층
├── PlayerState/          플레이어 ASC·AttributeSet 소유
├── Core/                 GameInstance, 세션 서브시스템
├── DataAssets/           로드아웃·무기·몽타주·의상·이동·입력 데이터
├── GameModes/            Base → Lobby / Gameplay
├── GameplayCues/         PlayAction / StopAction (몽타주 전 클라 동기화)
├── AI/                   BehaviorTree 태스크·서비스, EQS
├── Spawn/                적 스포너
├── Items/Weapons/        무기 액터
└── UI/                   HUD·머리 위 위젯
```

**캐릭터에 붙는 기능 컴포넌트는 전부 `UCBExtensionComponent`를 상속**함. 이 베이스가 "캐릭터 초기화 완료 신호를 구독하거나, 이미 끝났으면 즉시 실행"하는 처리를 한 곳에서 담당하므로, 개별 컴포넌트는 `OnCharacterSystemReady()`만 오버라이드하면 초기화 순서를 신경 쓰지 않아도 됨.

---

## 💡 주요 시스템 요약

| 문서 | 내용 |
|---|---|
| [1. 캐릭터 아키텍처](Docs/SubReadme/01-character-architecture.md) | 캐릭터 계층 구조와 로드아웃 |
| [2. 어빌리티 아키텍처](Docs/SubReadme/02-ability-architecture.md) | 어빌리티 계층 구조 |
| [3. 액션 시스템](Docs/SubReadme/03-action-system.md) | 액션 시스템과 액션 어빌리티 동기화 |
| [4. 이동/로코모션 시스템](Docs/SubReadme/04-locomotion-system.md) | 이동/로코모션 동작 구조와 애님BP |
| [5. 입력 시스템](Docs/SubReadme/05-input-system.md) | 입력 시스템 구조와 역할 분리 |
| [6. 전투 시스템](Docs/SubReadme/06-combat-system.md) | 무기 클래스 및 생성과 등록, 트레이스 처리 |
| [7. 멀티플레이 구조](Docs/SubReadme/07-multiplayer-architecture.md) | 리슨 서버 설계 및 세션 시스템 |
| [8. AI 전투](Docs/SubReadme/08-AI-combat.md) | AI BT 배선과 타겟팅 계산 |
| [9. 의상 시스템](Docs/SubReadme/09-cosmetic-system.md) | 의상 구조와 교체 및 복제 |

---

## 📄 기술 문서

설계 규칙과 시스템별 상세 구조를 담은 문서.

### Claude

| 문서 | 내용 |
|---|---|
| [CLAUDE.md](CLAUDE.md) | 프로젝트 전역 규칙 |

### 설계 원칙

| 문서 | 내용 |
|---|---|
| [Multiplayer.md](Docs/Tech/Multiplayer.md) | 서버 권위 구조, 함수 접두사(`Auth_`/`Local_`) 규칙, 어빌리티 네트워크 정책 |
| [CodingConventions.md](Docs/Tech/CodingConventions.md) | 주석 스타일, `#pragma region` 구조, 컴포넌트 결합 최소화 규칙 |
| [GameplayTags.md](Docs/Tech/GameplayTags.md) | 태그 4역할 분류(식별·속성·상태·이벤트), 상태 태그 소유권과 복제 규칙 |
| [AssetReference.md](Docs/Tech/AssetReference.md) | 하드/약한/소프트 참조 선택 기준과 비동기 로드 경계 |

### 캐릭터 · 초기화

| 문서 | 내용 |
|---|---|
| [ASC-Ownership.md](Docs/Tech/ASC-Ownership.md) | ASC·AttributeSet 소유 주체(플레이어=PlayerState / AI=Character)와 복제 모드 |
| [Loadout.md](Docs/Tech/Loadout.md) | 캐릭터 에셋을 로드아웃 하나로 일괄 등록·적용하는 데이터 에셋 패턴 |
| [SystemReady.md](Docs/Tech/SystemReady.md) | 비동기 초기화 완료 신호. 의존 로직을 준비 완료까지 미루는 게이트 패턴 |
| [Components.md](Docs/Tech/Components.md) | 캐릭터에 붙는 주요 컴포넌트와 각자의 책임 |

### 어빌리티 · 전투

| 문서 | 내용 |
|---|---|
| [Abilities.md](Docs/Tech/Abilities.md) | 어빌리티 베이스 계층과 상속 선택 기준, `NetExecutionPolicy` 결정 |
| [Combat.md](Docs/Tech/Combat.md) | 무기 스폰·수명 소유, 트레이스와 서버 히트 검증, 콤보 상태 |
| [Montage.md](Docs/Tech/Montage.md) | 액션 태그+인덱스 몽타주 조회, GameplayCue 기반 전 클라 동기화 |

### 이동 · 애니메이션

| 문서 | 내용 |
|---|---|
| [Locomotion.md](Docs/Tech/Locomotion.md) | 개이트·출발/정지 판정·피벗·대시/스프린트·점프, ABP 상태 머신 배선 |
| [AnimInstance.md](Docs/Tech/AnimInstance.md) | 애님 인스턴스 계층(Base→Character→Player/AI)과 로직 배치 기준 |
| [Input.md](Docs/Tech/Input.md) | EnhancedInput 기반 입력 태그 바인딩과 InputConfig 주입 |

### AI · 적

| 문서 | 내용 |
|---|---|
| [AI.md](Docs/Tech/AI.md) | AI 컨트롤러 계층과 두뇌 시작 게이트, BT/StateTree·이동 책임 분리 |
| [Teams.md](Docs/Tech/Teams.md) | 진영 enum, 캐릭터가 복제 소유하는 팀 데이터, 전역 attitude solver |
| [Spawner.md](Docs/Tech/Spawner.md) | 범위 안 플레이어 수 기반 웨이브 소환, 내비메시 소환 지점 탐색 |

### 게임 흐름

| 문서 | 내용 |
|---|---|
| [GameFlow.md](Docs/Tech/GameFlow.md) | 메인메뉴→로비→게임플레이 전환, 세션 접속, 맵을 넘어 살아남는 데이터 |

### UI

| 문서 | 내용 |
|---|---|
| [UI.md](Docs/Tech/UI.md) | 캐릭터 UI(HUD 체력·머리 위 체력바) 구조. 값 동기화는 어트리뷰트 복제 전담 |
| [EasyGameUI.md](Docs/Tech/EasyGameUI.md) | 시스템 UI(메인메뉴·일시정지·옵션) 에셋팩 채택 범위와 위젯 스택 규칙 |

### 리소스

| 문서 | 내용 |
|---|---|
| [Cosmetic.md](Docs/Tech/Cosmetic.md) | 부위 슬롯×파츠 태그 의상 교체. 조합 태그만 복제하고 조립은 로컬 |
| [SkeletonCompatibility.md](Docs/Tech/SkeletonCompatibility.md) | 새 메시와 애니메이션 스켈레톤 호환 진단, 커스텀 본 추가·리타게팅 |

---

## 🔧 주요 트러블슈팅

겪은 문제와 그 원인·해결. 각 항목은 노션 개발 문서의 상세 페이지.

| 주제 | 증상 | 원인 → 해결 |
|---|---|---|
| 트레이스 | [빠른 스윙이 적을 그대로 통과](https://sprout-whitefish-298.notion.site/v1-2_-3792a74e9c6f807e97a3e9c42c4720f2?pvs=143) | 몽타주가 빨라 프레임 간 무기 이동량이 커지면서 트레이스 사이에 빈 공간 발생 → 무기의 세로선 대신 **이전 프레임과 현재 프레임 위치 사이**를 분할 스윕 |
| 트레이스 | [2배속 재생 시 스윙 후반부가 판정되지 않음](https://sprout-whitefish-298.notion.site/3902a74e9c6f80d2b8c1eb5027a39905?pvs=143) | 몽타주 카운터는 2배로 도는데 Blend In 0.25초가 노티파이 구간 전체를 덮어 실제 포즈가 못 따라옴 → 재생 속도에 맞춰 블렌드 인 시간 축소 |
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
