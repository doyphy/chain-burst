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
| [Abilities.md](Docs/Tech/Gameplay/Abilities.md) | 새 어빌리티를 만들 때 — 몽타주 액션은 `UCBInputActionAbility`(입력)/`UCBEventActionAbility`(이벤트) 중 상속, 엣지에서 `NetExecutionPolicy` 명시. 여러 어빌리티가 쓰는 기능은 기능 조각(예: `UCBFragment_WeaponTrace`)을 소유해 호출. 적중 시 회복은 어트리뷰트(`LifeOnHit`) + 상시 패시브(`UCBLifeOnHitAbility`) — 출처는 GE 모디파이어로 더함 |
| [ActionFragment.md](Docs/Tech/Gameplay/ActionFragment.md) | **어빌리티 구조를 바꾸거나 여러 어빌리티가 쓸 기능을 만들 때** — 베이스(`UCBGameplayAbility`)에는 공용 로직만, 기능은 새 베이스 클래스 대신 조각으로 빼서 필요한 어빌리티가 멤버로 소유·호출. 조각 크기는 "늘 함께 쓰이는 기능 묶음" 단위 |
| [Combat.md](Docs/Tech/Gameplay/Combat.md) | 무기 스폰/트레이스/히트 검증, 콤보 상태를 다룰 때. **영역 판정**(내려찍기·충격파 — 순간 오버랩 쿼리, 무기 트레이스와 적용 코드 공유)도 여기. **투사체 판정**(`ACBProjectile` — 루트 모양으로 매 틱 Weapon 채널 스윕, 발사 때 만든 데미지 스펙을 명중 때 적용)도 여기. **무기 오라**(무기가 이펙트 소유·요청 수 관리, 켜는 쪽은 `GameplayCue.Weapon.Aura` 큐)도 여기 |
| [Burst.md](Docs/Tech/Gameplay/Burst.md) | **버스트(피버 타임)를 다룰 때** — 적립은 패시브(`UCBBurstGaugeAbility`, 서버·적중당), 발동은 입력 액션(`UCBBurstAbility`, 게이지 가득 참), 버프는 `GE_Burst` 에셋만. 사망·캐릭터 변경 초기화는 패시브 종료 한 곳 |
| [LevelUp.md](Docs/Tech/Gameplay/LevelUp.md) | **경험치·레벨업을 다룰 때** — 경험치는 전 플레이어 공유(게임 스테이트 복제, 어트리뷰트 아님), 규칙은 `ACBGameplayGameMode`. **필요량 = 곡선(1인 기준) × 접속 인원**(`PlayerArray` — 복제 없이 게임 스테이트 `GetRequiredExperience` 하나를 서버·바가 공용, 바는 `OnPlayerListChanged`도 구독). 레벨업마다 **월드 정지**(호스트 `PC::SetPause` — 정지 중엔 속성 복제가 멈춰 카드는 RPC), 플레이어별 카드 3장(등급별 뽑기 가중치·겉모습은 데이터), 최대 레벨은 데이터(0 = 무제한), 타임아웃은 실제 시간 티커(`FTSTicker`). 카드 효과는 Infinite GE + SetByCaller(`Data.LevelUp`), 로드아웃 부여 기록 밖이라 매치 끝까지 유지 |
| [Stun.md](Docs/Tech/Gameplay/Stun.md) | **보스 기절(그로기)을 다룰 때** — 적립은 패시브(`UCBStunGaugeAbility`, 서버 — 피격마다 고정값 + 자기 스킬 **정상 종료** 시 태그별 값), 가득 차면 `Event.Combat.Stunned` → `GA_Stun`(이벤트 액션 BP, 코드 없음). 기절 어빌리티가 `Staggered` 를 함께 소유해 BT 경직 분기를 그대로 탐. 최대값 `MaxStunGauge` 는 어트리뷰트(기본 100 = 1인 기준, 보스전 인원만큼 곱해짐) |
| [Montage.md](Docs/Tech/Gameplay/Montage.md) | 액션 몽타주 조회·재생·동기화 — GameplayCue(`GameplayCue.PlayAction`)로 전 클라 동기화, 콤보 인덱스는 큐 파라미터로 전달. 정지·**섹션 점프**도 큐 경유(요청 머신은 요청자가 정함). **모션 워핑**(거리 상한 `ClampedSkewWarp`, 일정 속도 돌진 `ConstantSpeedWarp`, 고정 각도 회전 `RotateBy` — 애님 루트모션이 있으면 루트모션 소스는 무시됨)과 서버 본 갱신(애님 틱) 전제도 여기 |
| [Locomotion.md](Docs/Tech/Gameplay/Locomotion.md) | 이동 기능을 다룰 때 — 개이트=ASC 태그(`Status.Movement.Gait.*`), 출발/정지=속도 비율 판정, 피벗=입력 잠금, Sprint는 전방 대시에 종속, 점프=GAS 어빌리티, 전이 블렌드=관성화 |
| [AI.md](Docs/Tech/Gameplay/AI.md) | AI 컨트롤러 계층(Base/Outlaw/Rogue), 두뇌 시작 게이트(SystemReady), BT/StateTree·NavMesh·CMC 이동 책임 분리를 다룰 때. **감지**(시야·청각·피해 — 맞으면 가해자 인지)와 **타겟 선정**(점수 + 등급별 교체 규칙 `ReevaluateTarget` — 보스는 집중 시간: 조용하면 교체·최대 시간이면 순환, 소환한 쪽이 정하는 **지정 타겟** `Auth_AssignTarget` — 살아 있는 동안 선정 규칙을 건너뜀)도 여기. BT 커스텀 노드(어빌리티 활성화·랜덤 셀렉터·시야 데코레이터·등 뒤 데코레이터·타겟 거리 서비스·**노드 실행 중에만 이동 속도 어빌리티를 켜는 서비스**(`Movement Speed` — 보스가 멀 때 `MoveTo` 동안 전력질주, AI 용 질주 GA 는 대시 종속 없이 따로) 등)와 등급별 전투 사이클 배선(이동만 끊고 공격은 끊지 않는 거리 판정, 구역보다 먼저 고르는 등 뒤 대응)도 여기 |
| [Spawner.md](Docs/Tech/Gameplay/Spawner.md) | **적을 레벨에 소환할 때** — 범위 안 살아있는 플레이어 수 × N마리, 전멸 전에는 새 웨이브 없음, 집계는 스캔 폴링, 소환 지점은 내비메시 도달 가능 지점. 소환만 하고 생성 연출은 캐릭터가 스스로 재생 |
| [BossArena.md](Docs/Tech/Gameplay/BossArena.md) | **보스 구역(보스 등장·입구 봉쇄)을 다룰 때** — 접속 인원 전원이 살아서 구역 안에 들어오면 5초 뒤 보스 소환 + 입구 차단(복제 액터 소환·파괴, 구역 액터는 서버 전용·비복제). 다 안 들어왔으면 들어온 플레이어 ASC 에 `Status.Arena.Waiting` 태그 → HUD 대기 표시. 모이면 카운트다운 기간 GE(`Status.Arena.Countdown`, 기간 = `BossAppearDelay`) → 같은 위젯이 GE 남은 시간으로 숫자 표시, 보스 등장은 1회 타이머. 카운트다운 중 이탈하면 취소. **등장 연출**: 소환 순간 서버가 모든 PC 뷰 타겟을 구역의 `IntroCamera` 로 + 시네마틱 모드(엔진 RPC가 클라로 전달, 이동·시점만) + 행동 차단 GE(`Block Abilities with Tags` — 공격·대시·점프 등, 소유 클라 예측도 막음), 보스는 `Event.Combat.Intro` → 등장 어빌리티(BP, `Staggered` 로 BT 대기) 몽타주, 보스 ASC 의 `Status.Combat.Intro` 가 빠지면 카메라 복귀. **인원 비례 내구도**: 보스 준비 완료(서버, 로드아웃 시작 GE 다음)에서 최대 체력·최대 기절 게이지 기본값 × 접속 인원 + 체력 가득. 보스 처치 시 게임모드에 매치 클리어 통지(졸개 사망 처리보다 먼저) + 입구 개방, 보스가 사라지면(파괴) 소멸 통지 → 클리어 화면, 전멸 시 보스 제거·입구 개방 후 대기로. 보스전 중 사망자는 **사망 시점에** 정한 구역 밖 부활 지점에서 리스폰. **졸개**: 연출이 끝나면 구역 안 생존 플레이어마다 따로 주변에 N마리 소환(구역 스캔이 처리, 다 잡히면 지연 후 그 플레이어에게만 재소환), 소환 직후 AI 컨트롤러 `Auth_AssignTarget`으로 그 플레이어만 쫓게 함. 처치 시 졸개 사망 처리, 전멸 시 파괴 |
| [Input.md](Docs/Tech/Gameplay/Input.md) | 입력 바인딩·EnhancedInput·InputConfig를 다룰 때 — 매핑 컨텍스트 등록, UI가 게임 입력을 막는 방식(IMC 제거) |

### `Presentation/` — 애니메이션·UI·외형
| 문서 | 언제 읽나 |
|---|---|
| [AnimInstance.md](Docs/Tech/Presentation/AnimInstance.md) | 애님 인스턴스 계층·로직 배치 — `Base → Character(공통) → Player/AI → Chaser/Outlaw`, 애니메이션은 상태 머신(모션 매칭 미사용) |
| [UI.md](Docs/Tech/Presentation/UI.md) | 캐릭터 UI(HUD 체력·스킬 쿨다운·버스트 게이지·머리 위 체력바·**화면 상단 보스 바**)를 다룰 때 — `UCBUIComponent`가 준비 완료 후 생성, 위젯 클래스는 로드아웃 등록, 값 동기화는 어트리뷰트 복제(UI용 네트워크 코드 없음), 보스 바는 교전 개시 태그(`Status.Combat.Engaged`) 구독, 외부 접근은 `ICBUIInterface`. **키로 여닫는 상태창**은 HUD 자식 — 표시할 어트리뷰트는 WBP에 텍스트(`UCBAttributeTextBlock`) 배치로 정하고, 열림은 LocalOnly 어빌리티가 붙이는 `Status.UI.StatusWindow` 태그 구독. **UI 머티리얼·UI 텍스처를 만들 때**도 여기 — 위젯 색·투명도는 엔진이 자동으로 곱함(VertexColor로 또 곱하지 말 것), 감마 공간 블렌딩, 움직이는 UI 텍스처는 밉맵 + Trilinear |
| [EasyGameUI.md](Docs/Tech/Presentation/EasyGameUI.md) | **시스템 UI(메인메뉴·일시정지·옵션)를 다룰 때 + 위젯을 새로 만들 때** — 팩 원본 수정 금지·복제 사용, 모든 위젯은 HUD 스택 경유(`AddToViewport` 금지), 시스템 UI 에셋은 Global Config에 등록 |
| [Cosmetic.md](Docs/Tech/Presentation/Cosmetic.md) | **의상 커스터마이징을 다룰 때** — 부위 슬롯(`ECBCosmeticSlot`) × 파츠 태그(`Item.Cosmetic.*`), 기본 의상은 로드아웃(하드)·교체 목록은 카탈로그(소프트), **복제하는 건 조합 태그뿐이고 조립은 각 인스턴스 로컬** |
| [SpawnFX.md](Docs/Tech/Presentation/SpawnFX.md) | **캐릭터 생성·소멸 연출을 다룰 때 (플레이어·AI 공통)** — 텔레포트 디졸브: 연출을 쓰는 메시의 **원본 마스터에 `MF_CB_TeleportDissolve`를 직접 연결**(평소 값이면 무효과, 엔진·플러그인 마스터는 복사본에 연결 후 MI 부모만 교체, 수정한 원본 목록은 문서에). **캐릭터가 스스로 재생**하고 로드아웃이 정함(`SpawnCueTag` / `DespawnEffectClass`). **생성**은 각 머신의 준비 완료에서 로컬 큐 실행 — 레벨에 생성될 때만(서버가 생성 직후 여는 "생성 창"을 첫 복제로만 보내 관련 거리 진입·배치 캐릭터와 구분). **소멸**은 서버가 사망 시 예약한 기간 GE(큐 상태 복제)로, 시체가 사라지는 시점(AI `DespawnDelay`, 플레이어 리스폰 지연)에 끝남. 큐 `ACBGCN_SpawnFX`는 캐릭터 전체 메시 + 붙은 무기에 값을 넣음 |
| [SkeletonCompatibility.md](Docs/Tech/Presentation/SkeletonCompatibility.md) | **새 캐릭터 메시를 적용할 때** — 애니메이션 스켈레톤과의 호환 진단, 커스텀 본 추가, 리타게팅 모드 |
| [Music.md](Docs/Tech/Presentation/Music.md) | **배경 음악(BGM)을 다룰 때** — 각 머신 로컬 재생(네트워크 없음), `UCBMusicSubsystem`(월드 서브시스템)이 기본 곡(레벨 BP `SetBaseMusic`)·덮어쓰기 곡 두 자리를 크로스페이드. 보스 BGM 은 `UCBOutlawLoadout::BossMusic` → 보스가 각 머신 준비 완료(소환·등장 연출)에 걸고 사망·파괴(`Destroyed`)에 해제. 레벨업 정지 중에도 흐르게 UI 사운드로 재생. 반복은 에셋 Looping. 옵션 Music 슬라이더는 Global Config Game Audio Classes 에 팩 계층(`SC_EGUI_*`)을 등록해야 먹음 — 엔진 Master 밑 새 클래스는 엔진 에셋 수정이라 금지 |

### `Flow/` — 레벨·접속·로비
| 문서 | 언제 읽나 |
|---|---|
| [GameFlow.md](Docs/Tech/Flow/GameFlow.md) | 레벨·게임모드를 추가하거나 **맵을 넘어 데이터를 넘겨야 할 때** — 게임모드 계층, seamless travel(5.8 은 PC 를 항상 새로 만들고, 도착은 `PostLogin` 을 타지 않음), `CopyProperties`, 그리고 **폰 재스폰 두 갈래**(무기 변경 = 캐릭터 변경 / 사망 후 리스폰). **매치 종료**(보스 처치 → 보스 시체가 사라진 뒤(구역 스캔이 파괴를 기다림 — 별도 지연 값 없음) 컨트롤러가 전원에게 클리어 위젯(Menu 레이어, **클리어 기록** = 처치 순간 월드 시간(레벨업 정지 제외) + 내 데미지(서버의 `UCBAttributeSet`이 실제로 깎인 체력을 공격한 플레이어의 `ACBPlayerState`에 누적, 끝에 RPC 로 한 번)) → **호스트만** 세션 유지한 채 전원 로비 복귀 / **각자** 세션 나가 메인 메뉴, 위젯은 `PreClientTravel` 에서 내림, 클리어 뒤 경험치 무시)도 여기 |
| [Session.md](Docs/Tech/Flow/Session.md) | **접속을 다룰 때** — 호스트(`listen` 오픈)/참가(주소 `ClientTravel`), 세션 생성·검색·참가, 접속 실패 복귀, 나가기. 전부 `UCBSessionSubsystem` 하나 |
| [Lobby.md](Docs/Tech/Flow/Lobby.md) | 로비 레벨을 다룰 때 — 실제 Chaser를 스폰하는 이유, 닉네임(엔진 `PlayerName` 사용), 준비 완료·게임 시작, 고정 카메라·페이드, 로비 UI 월드 투영. 게임플레이에서 돌아오면 인원 재집계(`HandleSeamlessTravelPlayer`)·세션 재개방(`BeginPlay`) |
