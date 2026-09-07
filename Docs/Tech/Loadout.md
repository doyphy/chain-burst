# Loadout 데이터 에셋 패턴

> 캐릭터에 필요한 에셋을 로드아웃 하나로 일괄 등록·적용하는 패턴.

**설계 원칙: 캐릭터에 필요한 에셋은 모두 로드아웃에 등록한다.**
- 캐릭터/컴포넌트에 에셋을 직접 등록하지 않는다. 직접 등록하면 캐릭터 블루프린트를 새로 만들 때마다 설정할 항목이 많아 번거롭다.
- **캐릭터는 로드아웃 데이터 에셋 하나만 참조**하고, 나머지 필요한 에셋(메쉬, 어빌리티, 무기, GE 등)은 전부 로드아웃에 등록·적용한다.
- 새 캐릭터/변형을 만들 때는 새 로드아웃 데이터 에셋만 구성하면 된다.

**설계 원칙: 비동기 로드 경계는 "캐릭터 → 로드아웃" 한 곳뿐이다.**
- 캐릭터는 로드아웃을 **소프트 참조**하고 비동기 로드한다(`LoadAssetAsync<Loadout>`). 이것이 유일한 async 경계다.
- **로드아웃 내부의 에셋(메쉬·무기 데이터 등)은 전부 하드 참조**로 등록한다. 하드 참조는 로드아웃 패키지와 함께 로드되므로, 로드아웃 한 번의 비동기 로드로 내용물이 전부 함께 resolve된다.
- 따라서 **로드아웃 내부에서 다시 비동기 로드(중첩 async)를 하지 않는다.** 캐릭터의 로드아웃 로드 콜백 시점이 곧 "모든 게 준비된 완료 시점"이며, 별도의 완료 신호 전파가 필요 없다.
- 로드아웃은 게임플레이 스폰 번들 전용이다. UI(캐릭터 선택/프리뷰)용 표시 데이터가 필요하면 로드아웃을 재사용하지 말고 **가벼운 별도 표시 데이터 에셋**으로 분리한다(무거운 게임플레이 에셋을 UI가 끌고 오지 않도록).

**설계 원칙: 로드아웃은 "이 캐릭터의 구성"이고, "고를 수 있는 목록"은 카탈로그가 담는다.**
- 의상은 `UCBCosmeticCatalog`, **캐릭터(무기)는 `UCBCharacterCatalog`** — 둘 다 로드아웃 밖에 있다. 목록을 로드아웃에 넣으면 캐릭터마다 같은 목록이 중복된다.
- **캐릭터 카탈로그의 등록소는 `UCBGameInstance`다.** 의상 카탈로그와 달리 **폰이 없는 시점에 읽혀야** 하기 때문이다 — 서버는 스폰할 폰 클래스를 정할 때, 클라이언트는 선택 UI를 만들 때 읽는다. 담는 것은 태그 → 캐릭터 클래스(소프트)·표시 이름·아이콘뿐이라 가볍다.
- **무기 변경 = 캐릭터 변경 = 폰 재스폰**이며, 로드아웃을 런타임에 갈아끼우는 경로는 만들지 않는다. 흐름 전체는 → [GameFlow.md](GameFlow.md) "무기 변경 = 캐릭터 변경".

## 로드아웃 계층

```
UCBCharacterLoadout
├── UCBChaserLoadout            ← 플레이어
└── UCBAILoadout (Abstract)     ← AI 공통 (AI 전용 데이터의 홈)
    ├── UCBRogueLoadout         ← BehaviorTree 등록 (GetBehaviorTree 오버라이드)
    └── UCBOutlawLoadout
```
캐릭터 계층(`ACBBaseCharacter → {Chaser, ACBAICharacter → {Outlaw, Rogue}}`)과 대칭. AI 전용 데이터(예: 비헤이비어 트리)는 `UCBAILoadout`에 두어 Chaser 로드아웃이 오염되지 않게 한다. AI 두뇌 에셋의 흐름은 [AI.md](AI.md) 참조.

캐릭터 초기화 시 `UCBCharacterLoadout` (및 하위 `UCBChaserLoadout`, `UCBAILoadout` 계열) 데이터 에셋이 일괄 처리:
- `Auth_GrantAbilitiesToASC()` — 어빌리티 부여 (Active / Passive / Reactive 3종 분류) — **서버 전용**
- `Auth_RegisterWeaponsToCombatComponent()` — 무기 등록 — **서버 전용**. 무기 데이터 **목록**(`WeaponDatas`)을 순서대로 등록(단일 무기 1개, 쌍수 무기는 좌/우 2개). 소켓 점유 중복·최대 개수 차단은 컴뱃 컴포넌트 책임 — [Combat.md](Combat.md)
- `Auth_ApplyEffectsToASC()` — 초기 스탯 GE 적용 — **서버 전용**
- `ApplyToCharacter()` — **전 인스턴스 공용**. 바디 셋업(`FCBBodySetup`), 스켈레탈 메쉬·애님BP, 이동 데이터(`MovementData` → `ACBBaseCharacter`), 액션 몽타주 데이터(`MontageData` → `UCBActionComponent`)를 적용. 모든 클라이언트(시뮬 프록시 포함)에서 필요하므로 서버 전용 경로(`Auth_`)가 아닌 공용 경로에서 호출한다. 로드아웃 내부 에셋은 모두 하드 참조라 로드아웃 로드 시점에 함께 resolve되므로 **이 함수는 동기 실행**된다(내부에 중첩 비동기 로드 없음).
  - Rogue/Outlaw: `BeginPlay`, Chaser: `InitializePlayerSystem` 공용 구간
  - **`virtual`이다.** 파생 로드아웃이 오버라이드해 역할별 공용 데이터를 추가한다(`Super` 호출 필수). 현재 `UCBChaserLoadout`이 **팔로워 메시**(`SkinMeshes` + `DefaultCosmetics` + `bHideLeaderMesh` → `UCBModularMeshComponent`)를 주입한다 — 본체 메시(리더)를 리더 포즈로 따라가는 외형 메시들이며, 모듈러 메시 컴포넌트를 가진 Chaser에만 해당하므로 공통 로드아웃이 아닌 Chaser 로드아웃에 둔다. 실제 컴포넌트 생성·부착은 준비 완료 후 컴포넌트가 수행 — [Components.md](Components.md), [SkeletonCompatibility.md](SkeletonCompatibility.md)
  - **피부와 의상을 나눠 담는다.** `SkinMeshes`는 교체 대상이 아닌 고정 파츠(MetaHuman 바디)로 **메시를 직접** 들고, `DefaultCosmeticIds`는 `TMap<ECBCosmeticSlot, FGameplayTag>`로 **부위 슬롯별 기본 의상을 파츠 태그로** 지정한다. 등록하지 않은 슬롯은 그 부위를 입히지 않으므로 **전부 비우면 피부만(알몸) 노출**된다. `bHideLeaderMesh`는 피부가 몸을 덮는 구성에서 쓰는 값이라 `SkinMeshes`와 한 세트다.
  - **기본 의상을 메시가 아니라 태그로 두는 이유**: 로드아웃이 메시를 직접 들면 **카탈로그를 거치지 않고 의상이 붙는 경로**가 생겨, 카탈로그에 없는 의상을 기본값으로 넣는 실수가 가능하다. 태그 픽커는 카탈로그 밖의 값을 애초에 막는다. 메시는 카탈로그의 소프트 참조라 **준비 완료가 그 로드를 기다린다**(2단계) → [Cosmetic.md](Cosmetic.md), [SystemReady.md](SystemReady.md)
  - **교체 목록은 `CosmeticCatalog`(`UCBCosmeticCatalog`)가 따로 담는다.** 로드아웃은 "이 캐릭터의 기본 구성"이지 "선택 가능한 전체 목록"이 아니므로, 의상 카탈로그를 로드아웃에 직접 넣으면 캐릭터마다 같은 목록이 중복된다. 카탈로그 자체는 하드 참조(태그·표시 이름 정도라 가볍다)이고 **그 안의 파츠 메시만 소프트 참조**라, 선택된 파츠만 로드된다. 배치는 `MontageData`와 같은 성격 — 로드아웃이 참조하고 소비자에게 넘긴다.
  - 파츠 식별은 `FGameplayTag`(`Item.Cosmetic.*`, ini 등록)이며 **부위의 권위는 카탈로그 `PartsBySlot`의 맵 키**다 → [GameplayTags.md](GameplayTags.md)
  - **슬롯 체계·태그 3상태·교체 흐름·복제 등 의상 시스템 전반은 → [Cosmetic.md](Cosmetic.md).** 로드아웃은 그중 "이 캐릭터의 기본 구성을 주입한다"는 부분만 담당한다.
  - **캐릭터 BP 의 `Mesh` 는 비워두지 않는다.** 로드아웃이 리더 메시를 적용하기 전까지 메시가 없으면, 그 사이 도착한 **복제 어태치먼트가 무기 소켓을 찾지 못해** `GetSocketInfoByName: No SkeletalMesh` 경고가 난다(서버는 순서가 보장되지만 클라이언트는 로드아웃 로드와 무관하게 부착이 도착한다). **로드아웃과 같은 리더 메시를 BP 기본값으로 지정하고 `Visible`을 꺼둔다** — 같은 에셋이라 메모리 중복이 없고, 소켓 조회는 가시성과 무관하다. 리더를 다시 켜는 책임은 `UCBModularMeshComponent`가 준비 완료 시점에 진다.
    - **클라이언트에서만 생긴다.** 서버는 로드 완료 콜백 안에서 `ApplyToCharacter`(메시) → `Auth_InitServerData`(무기 스폰·부착)가 한 줄로 이어져 순서가 보장된다. 반면 클라는 **`OnRep_PlayerState`가 와야 초기화를 시작**하므로, 그 전에 서버가 만든 부착 결과가 먼저 도착할 수 있다.
    - 일반화하면 **서버가 만든 결과가 복제로 도착하는 시점과, 클라가 자기 로드아웃을 적용하는 시점은 서로 독립이다.** 복제 대상이 로드아웃 적용 결과(메시·소켓 등)에 의존하면 같은 증상이 재발한다 → [Multiplayer.md](Multiplayer.md)
  - **카탈로그 조회는 로드아웃이 아니라 `UCBModularMeshComponent`가 한다.** 로드아웃은 `SetCosmeticCatalog()`로 주입만 하고 빠진다 — `MontageData`를 받은 `UCBActionComponent`가 태그로 몽타주를 조회하는 것과 같은 구조다. 컴포넌트에 두면 **캐릭터가 아닌 로비 프리뷰 액터도 컴포넌트만 붙여 그대로 재사용**할 수 있다 → [Components.md](Components.md)
  - `MovementData`·`MontageData`는 로드아웃에 하드 참조로 등록되며, 캐릭터/컴포넌트 쪽 멤버는 `EditDefaultsOnly`가 아닌 런타임 캐시(세터 주입)
  - **바디 셋업(`FCBBodySetup`: 캡슐 반지름/절반높이 + 메시 상대 위치/회전/스케일)**: 사용하는 메시 모델에 종속되는 값이라 메시와 한 세트로 로드아웃이 관리. 캐릭터 생성자의 `InitCapsuleSize`는 스폰 직후 fallback이고 로드아웃 로드 후 오버라이드됨. 정렬 공식: `MeshRelativeLocation.Z = -CapsuleHalfHeight`, `MeshRelativeRotation.Yaw = -90`. 게임플레이상 크기를 키우려면 캡슐과 메시 스케일을 같은 비율로 올릴 것(스케일만 키우면 충돌은 그대로).
    - **함정 — 메시 오프셋 적용 직후 `CacheInitialMeshOffset()`를 반드시 호출한다.** `ACharacter`는 `PostInitializeComponents` 시점(로드아웃 적용 **전**, BP 디폴트 상태)의 메시 상대 트랜스폼을 `BaseTranslationOffset`/`BaseRotationOffset`에 1회 캐싱하고, 이 캐시는 런타임에 자동 갱신되지 않는다. 시뮬 프록시(및 리슨 서버가 보는 원격 플레이어)의 CMC 네트워크 스무딩은 매 틱 메시 상대 트랜스폼을 `BaseTranslationOffset + MeshTranslationOffset(스냅 감쇠분)`으로 **재생성**하므로, 캐시가 스테일하면 로드아웃으로 적용한 오프셋(`SetRelativeLocationAndRotation`)이 다음 틱에 덮여 메시가 캡슐 중앙으로 되돌아가 **공중에 떠 보인다**(권위 캐릭터는 스무딩이 안 돌아 정상). 그래서 `ApplyToCharacter()`가 오프셋 적용 직후 `CacheInitialMeshOffset(BodySetup.MeshRelativeLocation, BodySetup.MeshRelativeRotation)`로 캐시를 갱신한다. 근거: 엔진 `UCharacterMovementComponent::SmoothClientPosition_UpdateVisuals()`(합산식), `SmoothCorrection()`(스냅량 누적), `ACharacter::CacheInitialMeshOffset()`.
- `Local_ApplyToCharacter()` (`UCBChaserLoadout`) — **소유 클라이언트 전용**. 입력 설정(`InputConfig` → `ACBChaserCharacter`)을 주입. Chaser의 `Local_InitClientData()`(= `InitializePlayerSystem`의 `IsLocallyControlled()` 구간)에서 로드아웃 로드 후 호출. 주입 후 지연 바인딩 처리는 [Input.md](Input.md) 참고.

## 관련 문서
- **새 메시를 캐릭터 애니메이션 스켈레톤에 맞추는 절차**(호환 진단·커스텀 본 추가): [SkeletonCompatibility.md](SkeletonCompatibility.md)
- 함수 접두사 규칙(`Auth_` / `Local_`)·초기화 흐름: [Multiplayer.md](Multiplayer.md)
- 어빌리티 3종 분류: [Abilities.md](Abilities.md)
- 입력 설정(`InputConfig`) 주입·지연 바인딩: [Input.md](Input.md)
- 에셋 참조 방식(하드/약한/소프트): [AssetReference.md](AssetReference.md)
