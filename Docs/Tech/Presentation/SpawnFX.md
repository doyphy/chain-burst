# 생성·소멸 연출 (텔레포트 디졸브)

> 캐릭터가 레벨에 **생성될 때 빛 줄무늬로 녹아들듯 나타나고**, 시체가 **같은 방식으로 녹아 사라지는** 연출. **플레이어·AI 공통이며 캐릭터가 스스로 재생한다** — 스포너는 소환만, 게임모드는 리스폰만 하고 연출은 모른다. 어떤 연출을 쓸지는 로드아웃이 정한다(`SpawnCueTag` / `DespawnEffectClass`, 비우면 그 연출이 없는 캐릭터).
>
> 캐릭터 머티리얼의 디졸브(텔레포트 블록) + 발밑 나이아가라를 GameplayCue 하나로 묶는다. 서드파티 팩 **TeleportationFX**(`_ThirdParty/VFX/TeleportationFX`)의 방식(캐릭터 머티리얼에 텔레포트 블록을 넣고 파라미터를 시간에 따라 바꿈)을 따른다.

```
[생성] 각 머신에서 캐릭터 준비 완료 (HandleCharacterSystemReady — 메시·머티리얼이 처음 붙는 순간)
   └ 방금 생성된 캐릭터인가?
        서버(호스트) : 런타임 생성(!IsNetStartupActor)이면 예
        클라이언트   : 첫 복제로 받은 '생성 창'(bInSpawnEffectWindow)이 열려 있었으면 예
   └ 예 → 로드아웃 SpawnCueTag 를 이 머신에서만 실행 (ExecuteGameplayCue_NonReplicated)
          → GCN_SpawnReveal (ACBGCN_SpawnFX) OnExecute: 발밑 나이아가라 + 머티리얼 값을 커브대로 → 끝나면 스스로 정리

[소멸] 서버 — 사망(Status.Dead) → Auth_HandleDeath
   └ 시체가 사라지는 시점(GetCorpseLifetime: AI = DespawnDelay, 플레이어 = 리스폰 지연) − 연출 길이 에 예약
        → 로드아웃 DespawnEffectClass(GE_DespawnReveal, 기간 1.6초) 적용
             ▼ 큐 GameplayCue.Character.DespawnReveal — 기간 GE 의 큐는 '상태'로 복제
[각 클라·호스트] GCN_DespawnReveal (ACBGCN_SpawnFX 자식) WhileActive → 사라짐 커브
   └ 시체가 사라지는 순간 끝남 (AI = DespawnDelay 에 스스로 파괴, 플레이어 = 리스폰 때 게임모드가 폰 교체)

[캐릭터 머티리얼] 원본 마스터에 MF_CB_TeleportDissolve — 평소 Teleport_Dissolve=1 이라 원래 모습 그대로
```

## 시작점 — 엔진 호출 순서에서 고른 자리

| 머신 | 생성 순서 |
|---|---|
| 서버 · AI | `SpawnActor` → 생성자 → `PostInitializeComponents`(자동 빙의 → `PossessedBy` → 로드아웃 로드 시작) → `BeginPlay` → … 로드 완료: 메시 적용 → **준비 완료** |
| 서버 · 플레이어 | `RestartPlayer` → `SpawnActor`(생성자 → `PostInitializeComponents` → `BeginPlay`) → `Possess` → `PossessedBy` → 로드 → **준비 완료** |
| 클라이언트 | 복제로 액터 생성 → `PostInitializeComponents` → (첫 복제 값 적용) → `BeginPlay`(AI) / `OnRep_PlayerState`(플레이어)에서 로드 시작 → **준비 완료** |

- **캐릭터가 처음 보이는 순간은 머신마다 각자의 준비 완료**다. 메시·머티리얼을 로드아웃이 그때 붙이므로(→ [SystemReady.md](../Foundation/SystemReady.md)) 그 전에는 보일 것이 없다. 그래서 생성 연출은 **각 머신이 자기 준비 완료에서 로컬로** 시작한다 — 네트워크로 시작 신호를 보내면 서버 로드가 늦을 때 신호가 클라이언트의 준비 완료보다 늦게 도착해 원래 모습이 먼저 보인다.
- **파괴는 서버 `Destroy` → `EndPlay` 순이고 클라이언트는 복제로 뒤따른다.** `EndPlay` 는 이미 사라지는 중이라 연출을 걸 수 없으므로 **사망 시점에 미리 예약**한다. 시점은 서버만 알므로 소멸 연출은 서버가 기간 GE 로 보낸다.

## 구성 요소

| 대상 | 위치 | 역할 |
|---|---|---|
| `MF_CB_TeleportDissolve` | `/Game/ChainBurst/VFX/Teleport/` | 팩 텔레포트 블록을 머티리얼 함수로 재구성. 입력 Emissive(원래 발광 통과), 출력 OpacityMask · Emissive · WorldPositionOffset |
| 원본 마스터 | 각 팩 폴더 (아래 "수정한 원본 마스터") | 함수를 직접 연결. 그 마스터를 쓰는 MI·메시는 자동으로 연출 가능 |
| `M_CB_GltfDefault` | `/Game/ChainBurst/Items/Weapons/Materials/` | 무기용. 원래 부모가 엔진 플러그인(`/InterchangeAssets/gltf/M_Default`)이라 수정할 수 없어, 프로젝트로 복사해 함수를 연결하고 무기 MI 의 부모만 바꿈 |
| `UCBCharacterLoadout::SpawnCueTag` / `DespawnEffectClass` | 로드아웃(공통) | 생성 큐 태그 / 소멸 GE. `ApplyToCharacter` 가 `SetLifecycleEffects` 로 캐릭터에 주입 |
| `ACBBaseCharacter` | 캐릭터 | 재생 시점을 정함 — 생성 창, 준비 완료 때 생성 큐 실행, 사망 시 소멸 GE 예약, `EndPlay` 에서 남은 소멸 GE 회수 |
| `ACBGCN_SpawnFX` (C++) / `GCN_SpawnReveal` · `GCN_DespawnReveal` (BP) | 큐 | 나이아가라 → 커브로 머티리얼 값 진행. 생성은 실행형(OnExecute), 소멸은 상태형(WhileActive). 둘은 커브만 다름. 옛 이름 `ACBGCN_TeleportDissolve` 에서 바꿀 때 쓴 클래스 리다이렉트는 두 BP 재저장 후 지움 |
| `GE_DespawnReveal` | `/Game/ChainBurst/GameEffects/Shared/` | 기간 1.6초, 큐 태그 `GameplayCue.Character.DespawnReveal` |

## 머티리얼 — 팩 블록을 함수로

팩 예제(`M_Mannequin_Teleport`)의 블록을 그대로 다시 만들었다. **엔진 에셋만 쓴다**(노이즈 `OffsetNoiseDistanceFields_ForNormals`, `LocalPosition` 함수) — 팩이 없어도 머티리얼은 깨지지 않는다.

```
노이즈 UV = LocalPosition × 0.001 의 YZ × (TilingU, TilingV), Panner(0.176, 0.808)
OpacityMask = saturate( saturate((1 − 노이즈.R)^Pow) + Dissolve )
Emissive    = 원래 발광 + Glow × 노이즈.R² × Color
WPO         = |(1 − 노이즈.R)^Pow × VertexNoise × (0,0,1) × VertexNormalWS|
```

| 파라미터 | 기본값 | 의미 |
|---|---|---|
| `Teleport_Dissolve` | 1 | **1 = 완전히 보임.** 마스크 기준값 0.333 보다 크면 노이즈와 무관하게 전부 보이고, 그 아래로 내려가면 노이즈가 밝은 곳만 남아 세로 줄무늬로 사라짐. 0 이면 거의 안 보임 |
| `Teleport_Glow` | 0 | 노이즈 줄무늬 발광 세기 |
| `Teleport_VertexNoise` | 0 | 노이즈만큼 정점을 위로 밀어 올림(cm) |
| `Teleport_Color` | (0, 2, 40) | 발광색(HDR). 나이아가라(`NS_Teleport_Start_Sci` 의 `User.Color`)와 맞춤 |
| `Teleport_Pow` | 15 | 줄무늬 날카로움 |
| `Teleport_TilingU/V` | 18 / 2 | 줄무늬 개수(가로/세로) |

- **노이즈 투영은 로컬 YZ 면으로 고정**했다. 팩 머티리얼의 `UseRG` 스위치(XZ/YZ 선택)를 빼고, 데모 캐릭터 MI(`MI_Quinn_Teleport_2/3`)가 둘 다 쓰는 `UseRG`=true(YZ)로 맞췄다. 처음에 XZ 로 고정했더니 줄무늬가 감기는 방향이 데모와 달랐다. 다른 축이 필요한 캐릭터가 생기면 그때 스위치를 되살린다.
- **이름에 `Teleport_` 접두사**를 붙였다. 기존 마스터에 `Color` 같은 같은 이름 파라미터가 있으면 함수 파라미터와 섞인다.
- 평소 값(`Dissolve`=1, `Glow`=0, `VertexNoise`=0)이면 원래 모습 그대로다. 연출이 없는 캐릭터·연출이 끝난 캐릭터 모두 이 값으로 렌더링된다.

### 원본 마스터에 직접 연결한다

**연출을 쓰는 메시의 원본 마스터에 함수를 직접 연결한다.** 함수는 평소 값이면 아무 효과가 없고, 연출을 할지는 로드아웃의 큐 태그·GE 가 정하므로 머티리얼에 함수가 늘 들어 있어도 된다. 마스터 한 곳만 고치면 그 마스터를 쓰는 MI·메시(LOD 별 MI 포함)가 전부 따라온다.

| 원래 출력 | 연결 |
|---|---|
| Emissive | 원래 발광 → 함수 입력 `Emissive`, 함수 출력 `Emissive` → 머티리얼 Emissive |
| OpacityMask (Masked) | 원래 마스크 × 함수 `OpacityMask` (없으면 함수 출력 직결). **Opaque 마스터는 Masked 로 바꾸고, 마스크 기준값(`OpacityMaskClipValue`)이 0.3333 인지 확인한다** — 함수가 이 기준값을 전제로 하고, 불투명일 땐 쓰이지 않던 값이라 0 으로 저장된 마스터가 있다(glTF `M_Default` → `M_CB_GltfDefault`). 0 이면 마스크가 0 이어도 지워지지 않아 연출 내내 그대로 보인다 |
| Opacity (Translucent) | 원래 Opacity × 함수 `OpacityMask` — 반투명은 이진 마스크가 아니라 흐려지며 사라짐 |
| WorldPositionOffset | 원래 WPO + 함수 `WorldPositionOffset` (없으면 직결) |

- **머티리얼 어트리뷰트 방식 마스터**(MetaHuman 등)는 출력이 하나라, 마지막 어트리뷰트 뒤에 `Set Material Attributes`(Emissive Color · Opacity Mask 또는 Opacity · World Position Offset 핀)를 끼우고 `Break Material Attributes` 로 원래 값을 꺼내 위 표대로 잇는다. **Set 노드의 핀 추가는 에디터에서 한다** — MCP 로 핀 목록(`attributeSetTypes`)을 쓰면 핀 배열이 함께 늘지 않아 에디터가 크래시한다.
- **Opaque → Masked 로 바꾼 뒤 마스크가 무시되면 `CB.FixIgnoredOpacityMask` 를 돌린다.** 증상은 **마스크에 상수 0 을 꽂아도 그대로 보이는 것**이고, 옛 팩 머티리얼에 남아 있던 두 가지 원인이 있다.
  | 원인 | 무엇이 일어나나 | 겪은 곳 |
  |---|---|---|
  | "마스크여도 불투명으로 간주" 플래그(`UMaterial::bCanMaskedBeAssumedOpaque`) | 불투명일 땐 효과가 없다가 Masked 로 바꾸면 `GetBlendMode()` 가 Opaque 를 돌려줌. UE 5.8 은 이 값을 다시 계산하지 않음 | Dog·의상 원본 |
  | 마스크 출력 핀의 인라인 상수 표시(`OpacityMask.UseConstant`) | 켜져 있으면 **핀에 노드가 연결돼 있어도 무시하고 그 상수(1)로 컴파일**한다(`FScalarMaterialInput::CompileWithDefault`). 그래프에는 선이 보여 알아채기 어렵다. 발광·WPO 핀은 표시가 꺼져 있어 줄무늬 발광은 보이는데 잘리지만 않는 상태가 된다 | 의상 원본 13개 전부 |

  둘 다 Details·Python·MCP 로 못 바꾸고 에디터는 `set` 콘솔 명령을 막아, 에디터 전용 콘솔 명령을 두었다(`Private/Editor/CBMaterialFixCommand.cpp`).
  ```
  CB.FixIgnoredOpacityMask <폴더 또는 머티리얼 패키지 경로> ...
  ```
  마스크 입력이 연결된 Masked 머티리얼에서 위 두 값을 끄고(마스크 입력이 없는 머티리얼의 플래그는 엔진의 정상 최적화라 둠), 재컴파일 후 저장 대기 상태로 둔다. 저장은 직접 한다.
  - **원인 확인**은 숨은 값을 직접 본다. 에디터 콘솔에 `obj dump <머티리얼 경로>` 로 플래그를, `obj dump <머티리얼 경로>:<이름>EditorOnlyData` 로 핀별 `UseConstant` 를 로그에 찍는다(MCP `ObjectTools` 로는 안 읽힘). 정상 마스터를 복제해 같은 변경을 한 것과 나란히 비교하면 빠르다.
  - 같은 표시가 Emissive·Opacity 핀에 있어도 같은 식으로 무시된다. 지금 연결한 마스터 중엔 없어 명령은 마스크 핀만 본다. WPO 핀은 현재 컴파일러가 이 표시를 쓰지 않아 영향이 없다(무기 `M_CB_GltfDefault` 의 WPO 핀에 켜져 있지만 무해).
- **수정한 원본은 팩을 다시 임포트·업데이트하면 사라진다.** 아래 목록을 보고 다시 연결한다. 에셋 서브모듈(`chain-burst-assets`)·메인 저장소(`Content/MetaHumans`) 커밋에 들어간다.
- **같은 마스터를 쓰는 다른 에셋도 함께 바뀐다.** 함수 자체는 평소 효과가 없지만 Opaque → Masked 의 렌더링 비용은 모든 사용처에 붙는다. 연결 전에 참조자를 확인했다 — 아래 마스터는 캐릭터 전용이고, Robot 마스터만 팩 데모 맵 소품 2개와 공유한다(이미 Masked 라 비용 변화 없음).
- **엔진·플러그인 소속 마스터는 고치지 않는다.** 프로젝트로 복사해 연결하고 MI 의 부모만 바꾼다(무기 `M_CB_GltfDefault`). 부모를 바꿔도 같은 이름의 파라미터 오버라이드는 그대로 유지된다(4개 무기 MI 전후 값 일치 확인).

**수정한 원본 마스터**

| 대상 | 마스터 | 비고 |
|---|---|---|
| Robot | `RadicalMike/Materials/Body/M_MasterMaterial`, `Lens/M_Glass_parent` | Masked · Translucent (원래 값 유지) |
| Dog | `SciFi_Dog/Materials/M_SciFi_Dog_{Armor,Body}_Skin1~4` (8) | Opaque → Masked, 불투명 간주 플래그 끔 |
| 의상 | `SciFi_Bundle_03` 의 `M_Jacket_Helmet_*` · `M_Pants_Sneakers_*` · `M_Armor_Helmet_*` (13, 색 변형마다 마스터가 따로 있음) | Opaque → Masked, 불투명 간주 플래그·마스크 핀 상수 표시 끔 |
| 무기 | `M_CB_GltfDefault` (복사본) ← MI `aiStandardSurface1SG/4SG` 4개 | Opaque → Masked |
| 피부(MetaHuman) | `MetaHumans/Common` 의 `M_skin_unified_baked`, `M_teeth_unified`, `M_eye_eyeball_unified`, `M_eye_occlusion_unified`, `M_eye_lacrimal_fluid_unified`, `M_Eyelashes_Cards` (6) | 피부·치아·눈동자 Opaque → Masked, 눈 가림·눈물막 Translucent(어트리뷰트 방식 — Set 노드 경유), 속눈썹 Translucent |

### 대상 메시 — 캐릭터 전체 + 붙은 액터

큐는 **캐릭터의 모든 메시 컴포넌트와 붙어 있는 액터(무기)의 메시**에 값을 넣는다. 플레이어는 의상 파츠가 별도 메시 컴포넌트라(→ [Cosmetic.md](Cosmetic.md)) 본체 메시만 바꾸면 일부만 사라진다. 무기는 별도 액터로 늦게 붙을 수 있어 **매 틱 다시 모은다.** 파라미터가 없는 머티리얼은 엔진이 건너뛰므로(`SetScalarParameterValueOnMaterials`) 텔레포트 함수가 없는 메시는 그대로 보인다.

### 적용 현황

| 캐릭터 | 머티리얼 | 생성(`SpawnCueTag`) | 소멸(`DespawnEffectClass`) |
|---|---|---|---|
| Robot | ✅ 원본 마스터 2 | ✅ | ✅ |
| Dog | ✅ 원본 마스터 8 | ✅ (Dog1~4) | ✅ (Dog1~4) |
| Hulk | 예정 (`M_MASTER_BlackSite` · 유리 · 콘) | 예정 | 예정 |
| Chaser(플레이어) | ✅ 의상 13 · 무기(복사본 1) · 피부(MetaHuman) 6. 리더 메시(`SKM_Manny_*`)는 숨김이라 제외 | ✅ (Sword·Dagger·GreatSword·Spear) | ✅ (4개 모두) |

새 캐릭터를 추가하면 메시의 원본 마스터에 함수를 연결(위 "원본 마스터에 직접 연결한다")하고 로드아웃에 `SpawnCueTag` · `DespawnEffectClass` 를 지정한다. 텔레포트 함수가 없는 머티리얼은 연출 중에도 그대로 보이고 나이아가라만 뜬다.

## 생성 연출 — 레벨에 생성될 때만

클라이언트 입장에서는 방금 생성된 캐릭터를 받든, 오래전부터 있던 캐릭터가 관련 거리 안으로 들어와 받든 **똑같이 "액터가 도착"** 한다. 그래서 **서버가 알려줘야** 구분된다.

- **생성 창** — 서버는 런타임에 생성된 캐릭터(`!IsNetStartupActor()`)의 `BeginPlay` 에서 `bInSpawnEffectWindow` 를 켜고 **1초 뒤 끈다.** 생성 프레임 안이라 이 값이 첫 복제와 함께 나간다.
- 이 값은 **첫 복제에만 실린다**(`COND_InitialOnly`). 이미 받은 클라이언트에는 이후 변경을 보내지 않으므로, **창이 닫힌 뒤 이 캐릭터를 처음 받는 클라이언트만 "닫힘"을 받는다.**
- 준비 완료 때 판정: **서버는 런타임 생성이면 항상**(스스로 생성했으므로), **클라이언트는 받은 창 값**으로. 클라이언트의 준비 완료는 첫 복제 이후라 값이 항상 먼저 와 있다.

| 상황 | 연출 |
|---|---|
| 스포너가 소환 / 플레이어 리스폰 | O (그 순간 관련 있던 모든 머신) |
| 매치 시작 때 스폰되는 플레이어, 로비 첫 생성·무기 변경 | O (전부 런타임 생성) |
| 레벨에 배치된 캐릭터 | X (창을 열지 않음) |
| 이미 있던 캐릭터가 관련 거리 안으로 들어옴 | X (창이 닫힌 뒤 첫 복제) |

- **창이 1초인 이유.** 첫 복제는 보통 생성한 그 프레임에 나가지만 대역폭 사정으로 몇 프레임 밀릴 수 있어 여유를 둔다. 1초 사이에 관련 거리(기본 150m) 밖에서 안으로 들어오는 경우는 이동 속도상 사실상 없다. 튜닝 대상이 아니라 코드 상수(`SpawnEffectWindowDuration`)다.
- **판정이 로드 시간과 무관하다.** "첫 복제 때 창이 열려 있었나"만 보므로 클라이언트 로드가 오래 걸려도 생성으로 인정되고, 연출은 그 클라이언트의 준비 완료에서 처음부터 재생된다.
- **복제하지 않는 실행형 큐다**(`UGameplayCueManager::ExecuteGameplayCue_NonReplicated`). 실행형은 제거 이벤트가 오지 않으므로 큐가 **연출이 끝나면 스스로 풀로 돌아간다**(`GameplayCueFinishedCallback`).
- 시작하는 즉시 첫 값(`Dissolve`=−0.5)을 넣는다. 준비 완료 방송과 같은 호출 스택이라 원래 모습이 한 프레임도 렌더링되지 않는다.
- **연출 중 캐릭터를 멈추지 않는다.** 1.6초로 짧아 준비 완료 직후 움직여도 문제없다. 예전 방식의 연출 중 BT 대기(`Status.Spawning` 태그 + 이를 붙이던 `GE_SpawnReveal`)는 BT 분기와 함께 지웠다.

## 소멸 연출 — 사망 후 시체

- **시체가 사라지는 시점에 끝나도록 연출 길이만큼 먼저 시작한다.** 시점은 캐릭터가 `GetCorpseLifetime()` 으로 정한다.
  - AI: `DespawnDelay`(시체 유지 시간, 5초). 그 시점에 **스스로 파괴**한다. 5 − 1.6 = 3.4초에 연출 시작.
  - 플레이어: 게임모드의 `RespawnDelay`. 플레이어는 `DespawnDelay = 0` 이라 스스로 파괴하지 않고, **리스폰 때 게임모드가 폰을 교체하며** 사라진다(→ [GameFlow.md](../Flow/GameFlow.md) "사망과 리스폰"). 새 폰은 런타임 생성이라 생성 연출로 나타난다.
  - 연출이 시체 유지 시간보다 길면 사망 직후 바로 시작한다.
- **파괴는 `DespawnDelay` 타이머가 한다.** 연출 GE 가 끝나는 순간과 같은 시점이다. 연출이 없는 캐릭터도 같은 시점에 사라진다.
- **고정 기간형(`HasDuration` + 고정 값) GE 만 받는다.** 기간만큼 시작을 앞당겨야 하므로 기간을 미리 알아야 한다. 아니면 경고 로그를 남기고 연출 없이 사라진다. 검증·캐싱은 서버가 로드아웃 적용 때 한다.
- **GE 기간 = 큐 `Duration` 으로 맞춘다.** 큐(1.6초)와 GE(1.6초)는 다른 에셋에 있고, 클라이언트는 Minimal 모드라 GE 기간을 알 수 없어 하나로 합칠 수 없다. GE 가 짧으면 연출이 잘린다.
- **플레이어는 `EndPlay` 에서 남은 소멸 GE 를 걷어낸다.** ASC 가 PlayerState 소유라 폰보다 오래 살아남고(→ [ASC-Ownership.md](../Foundation/ASC-Ownership.md)), 리스폰과 GE 만료가 같은 순간이라 타이머 순서에 따라 GE 가 새 폰으로 넘어갈 수 있다.
- 클라이언트는 연출 시작(큐)과 파괴를 **같은 지연만큼** 늦게 받으므로 끝까지 재생된다. 연출 도중에 관련성을 얻은 클라이언트는 중간부터 받는다 — 그 클라이언트의 준비 완료가 늦으면 큐가 **준비 완료를 기다렸다** 시작한다. 이 대기의 델리게이트가 네이티브(`DECLARE_MULTICAST_DELEGATE`)라 BP 에서 구독할 수 없어 큐를 C++ 베이스로 두었다.

### 동기화 — 기간 GE 의 큐는 '상태'로 복제된다

- 일회성 큐(`ExecuteGameplayCue`)는 신뢰성 없는 멀티캐스트 RPC 라 받지 못한 클라이언트가 생길 수 있다.
- **기간 GE 에 붙인 큐는 Minimal/Mixed 모드에서 `MinimalReplicationGameplayCues`(복제 프로퍼티)에 들어간다**(`GameplayEffect.cpp` — `AddGameplayCue_MinimalReplication`). 그래서 늦게 받은 클라이언트도 연출 중이면 받고, 끝난 뒤 받은 클라이언트에는 아무것도 오지 않는다. 플레이어 ASC(Mixed)도 같은 경로로 다른 플레이어에게 간다.
- 상태 복제로 받으면 `OnActive` 없이 **`WhileActive` 만 온다**(`FActiveGameplayCue::PostReplicatedAdd`). 그래서 소멸은 `WhileActive` 에서 시작한다.
- 같은 클라이언트에 RPC 와 상태 복제 양쪽으로 `WhileActive` 가 올 수 있다. `AGameplayCueNotify_Actor::bAllowMultipleWhileActiveEvents` 기본값이 **true** 라 그대로 두면 연출이 두 번 시작된다 → `false` 로 둔다.

### 커브 — 데모 사라짐 구간(0.9–2.5초, 정확히 1.6초)

| 커브 | `GCN_DespawnReveal` 값 (진행률) | 데모 원본 (초, 값) |
|---|---|---|
| Dissolve | 1 →(큐빅) −0.5(1.0) | (0.9, 1) → (2.5, −0.5) |
| VertexNoise | 12 → **593**(0.6875) → 500(1.0) | (0.8, 0) → (2.0, 2171.5) → … 2.5초에 약 1830 |
| Glow | 0 → 1(0.6875) → 0.84(1.0) | (0.9, 0) → (2.0, 1) → … 2.5초에 0.84 |

VertexNoise 는 생성 연출과 **같은 비율(×0.273, 1832 → 500)** 로 줄였고, 기울기도 같은 비율로 줄여 곡선 모양을 유지했다(처음 1000 기준으로 맞췄다가 생성 기본값과 함께 절반으로 낮춤). 끝 값(Dissolve −0.5)은 완전히 안 보이는 상태라, 파괴 직전 큐가 제거되며 끝 값으로 고정돼도 튀지 않는다.

**나이아가라는 `GCN_DespawnReveal` 의 `NiagaraSystem` 에서 직접 지정한다**(비우면 머티리얼 연출만).

## 생성 커브 — 데모 타임라인에서 옮김 (진행률 0~1, 1.6초)

팩 데모(`BP_Teleport_8`)의 5초 타임라인은 **사라짐(0.9–2.5초) → 나타남(2.5–4.0초)** 이다. 생성 연출은 나타남 구간을 1.6초 진행률로 옮겼다(기울기도 시간 비율만큼 환산).

| 커브 | 기본값 (`ACBGCN_SpawnFX` 생성자) | 데모 원본 (초, 값) |
|---|---|---|
| Dissolve | −0.5 →(큐빅) 1(0.625) → 1 | (2.5, −0.5) → (3.5, 1) |
| VertexNoise | **500** →(큐빅) 0(0.9375) → 0 | (2.0, **2171.5**) → (4.0, 0) 의 2.5초 지점(약 1830)부터 |
| Glow | 0.84 →(큐빅) 0(0.9375) → 0 | (2.0, 1) → (4.0, 0) 의 2.5초 지점부터 |

- **VertexNoise 는 수백 단위 이상이어야 보인다.** 일렁임 항 `(1−노이즈)^Pow` 가 대부분 0 에 가까워 작은 배수(처음 기본값 30)로는 효과가 사실상 없다 — 데모와 달라 보였던 가장 큰 원인이었다. 시작값은 데모(약 1830)보다 낮춰 1000 으로 정했다가 **500** 으로 다시 낮췄다. 시작 기울기도 값과 같은 비율로 줄여(−1066.7 → −533.35) 곡선 모양(`값 × (1−s)²(1+s)`, s = 0.9375 까지의 진행)을 유지한다.
- Dissolve 를 0 이 아니라 −0.5 에서 시작해야 첫 프레임에 아무것도 보이지 않는다(0 이면 가는 줄무늬가 남음).
- 기울기는 엔진 자동 계산 대신 **고정값**(`RCTM_User`)으로 넣는다 — 데모 커브의 0 기울기 구간을 진행률로 환산한 값이라 자동 계산이 모양을 바꾸면 안 된다.
- 데모 커브 값은 MCP 로 노출되지 않아(`UCurveFloat::FloatCurve` 비노출) **BP 파일의 키 레코드에서 읽었다.**
- **값의 정본은 C++ 생성자다.** `GCN_SpawnReveal` 은 세 커브를 "기본값으로 되돌리기" 상태로 두어 이 값을 물려받는다. 캐릭터·연출별로 다른 값이 필요할 때만 BP 에서 덮어쓴다.

## 확인할 것 (PIE)

- **2인 리슨 서버로 생성 창을 확인한다.** 소환·리스폰 때 양쪽 화면에서 연출이 나오는지, 레벨 배치 캐릭터는 연출 없이 보이는지. 관련 거리 진입(창이 닫힌 뒤 첫 복제)은 PIE 맵 크기로 재현이 어려우면 `NetCullDistanceSquared` 를 줄여 확인한다.
- 플레이어 사망 → 리스폰 지연 끝에 시체가 녹아 사라지고 새 폰이 나타나는지 (플레이어 머티리얼 작업 후).
- **Dog BP 는 기본 메시가 보이는 상태다.** 로드아웃 적용 전 한두 프레임 원래 모습이 번쩍일 수 있다. 보이면 BP 메시를 숨기고 준비 완료 때 켜는 처리를 추가한다. Robot·Hulk 는 BP 기본 메시가 비어 있어 해당 없음.
- 마스터가 Masked 가 되면서 생기는 렌더링 비용(Early-Z·그림자).

## 검토했지만 채택하지 않은 것

| 안 | 이유 |
|---|---|
| 스포너가 소환 프레임에 생성 연출 GE 적용 (처음 방식) | 스포너가 연출까지 맡고, 플레이어 리스폰에는 별도 경로가 필요하다. 연출은 캐릭터 자신의 것으로 옮겼다 |
| 캐릭터가 로드아웃 로드 후 생성 연출 GE 적용 | GE 클래스가 비동기 로드아웃에 있어 서버 로드가 늦으면 큐가 첫 복제보다 늦게 간다 → 클라이언트에서 원래 모습이 먼저 보였다가 사라진다 |
| 준비 완료마다 무조건 로컬 재생 (창 없이) | 관련 거리 진입 등으로 이미 있던 캐릭터를 처음 받을 때도 재생된다 |
| 생성 시각을 복제해 클라이언트가 서버 시간과 비교 | 시계 오차와 로드 시간이 섞여 기준을 정하기 어렵다. 창은 첫 복제 시점만 보므로 로드 시간과 무관하다 |
| 소멸도 각 머신이 로컬로 예약 | 플레이어 리스폰 지연은 서버의 게임모드에만 있어 클라이언트가 시점을 모른다 |
| 생성 연출 중 AI 정지 (`Status.Spawning`) | 1.6초로 짧아 준비 완료 직후 움직여도 문제없다 |
| 원본은 두고 복제 마스터·MI 를 로드아웃 `MaterialOverrides` 로 끼우기 (처음 방식) | 덮어쓰기가 본체 메시에만 적용돼 플레이어의 피부·의상 메시(별도 컴포넌트)에는 쓸 수 없고, 캐릭터마다 복제가 늘어난다. 함수는 평소 효과가 없으니 원본에 직접 넣는 쪽을 택했다 |
| 메시의 머티리얼 슬롯을 복제 머티리얼로 교체 | 무거운 메시 패키지(MetaHuman 등)가 다시 저장돼 LFS 용량을 먹고, 복제 머티리얼도 그대로 필요하다 |
| (B) 연출 동안만 전 슬롯을 텔레포트 머티리얼 하나로 교체 | 서드파티 수정이 없고 새 캐릭터에 자동 적용되지만, 연출 중 원래 텍스처 없이 빛 실루엣만 보인다. 질감이 녹아드는 팩 연출을 택했다 |
| 오버레이 머티리얼 | 메시 위에 한 겹 더 그릴 뿐 원래 모습을 잘라낼 수 없다 |
| 연출 중 무적 | 소환 지점이 플레이어에게서 `MinSpawnDistanceFromPlayers`(800) 밖이라 맞을 일이 드물다 |
| 팩의 `UseRG` 스위치(노이즈 투영 축) | 데모가 쓰는 YZ 로 고정. 다른 축이 필요한 캐릭터가 생기면 추가 |
| 큐에 색 파라미터 | 색은 MI(`Teleport_Color`)와 나이아가라 에셋에 이미 있다 |

## 관련 문서

- 준비 완료 신호: [SystemReady.md](../Foundation/SystemReady.md)
- 로드아웃(머티리얼 덮어쓰기·연출 등록): [Loadout.md](../Foundation/Loadout.md)
- 플레이어 사망과 리스폰: [GameFlow.md](../Flow/GameFlow.md)
- 스포너: [Spawner.md](../Gameplay/Spawner.md)
- 의상 파츠 메시: [Cosmetic.md](Cosmetic.md)
- GameplayCue 로 시뮬 프록시 반영: [Multiplayer.md](../Conventions/Multiplayer.md)
