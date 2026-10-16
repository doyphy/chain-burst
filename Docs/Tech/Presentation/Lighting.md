# 맵 조명 (SkyLight·하늘)

> 레벨의 환경광이 어떻게 만들어지고, 왜 SkyLight 를 **실시간 캡처**로 두는가. 게임플레이 맵(`L_CB_Gameplay_Sci`)의 하늘·SkyLight 를 바꾸거나, 새 맵에 하늘을 깔 때 이 문서를 따른다.

## 구성 (게임플레이 맵)

| 액터 | 설정 | 역할 |
|---|---|---|
| `SM_Skybox_Mesh_0~3` | Chaotic Skies 스카이박스 메시, 재질 `MI_Flowmap_Inst_05` (부모 `M_Flowmap_Main` — Unlit, **`Is Sky` 켬**) | 눈에 보이는 하늘. 흐르는 구름(Flowmap) |
| `SkyLight_1` | `Source Type = Captured Scene`, **`Real Time Capture` 켬**, `Movable`, `Intensity 5`, `CubemapResolution 128`, `Lower Hemisphere Is Black` | 하늘을 찍어 환경광(Lumen 하늘 조명 포함)으로 씀 |
| `PostProcessVolume_1` | Unbound, 색 보정 + **노출 고정**(Min·Max Brightness 1) | 노출이 고정이라 환경광이 어두우면 화면도 그대로 어둡다 |
| `AtmosphericFog_1` · `ExponentialHeightFog_1` · `DirectionalLight_1` | — | 아래 문제와는 무관 |

## 왜 실시간 캡처인가 — 첫 로드에만 맵이 어두웠던 문제

**증상**: 프로그램을 새로 실행하고 **처음** 게임플레이 레벨에 들어가면 맵 전체가 어둡다. 같은 프로세스에서 메인 메뉴·로비로 나갔다 다시 들어오면 정상으로 밝다. PIE·메인 메뉴 경유 모두 같다. 어두운 상태에서 `r.SkylightUpdateEveryFrame 1` 을 넣으면 즉시 밝아진다 → SkyLight 캡처가 어두웠던 것.

**원인 — PSO 프리캐싱 중에는 메시가 그려지지 않는데, SkyLight 는 그 순간 한 번만 찍었다.**

```
r.PSOPrecaching = 1
r.PSOPrecache.ProxyCreationWhenPSOReady = 1
r.PSOPrecache.ProxyCreationStrategy = 1    ← PSO 컴파일이 끝날 때까지 프록시 생성을 미룸 (= 안 그려짐)
```

1. 새 프로세스에서 처음 레벨을 열면 스카이박스 재질의 PSO 가 아직 컴파일 중이라 **스카이박스가 그려지지 않는다.**
2. 실시간 캡처가 꺼진 SkyLight(Captured Scene)는 레벨이 열릴 때 **한 번만** 캡처하는데, 하늘이 빈 상태(아래 반구는 원래 검정)로 찍혀 환경광이 거의 0 이 된다. 다시 찍지 않으므로 그 판 내내 어둡다.
3. 같은 프로세스에서 다시 들어오면 PSO 가 메모리에 남아 있어(`r.PSOPrecache.KeepInMemoryForActiveMaterials = 1`) 스카이박스가 곧바로 그려지고 캡처도 정상이다.

엔진의 캡처는 에디터에서 **셰이더 컴파일**·에셋 컴파일은 기다리지만 **PSO 프리캐싱은 기다리지 않는다**(`SkyLightComponent.cpp` `UpdateSkyCaptureContentsArray`). 패키징 빌드는 아무것도 기다리지 않는다.

**해결 — 실시간 캡처**: 하늘을 매 프레임(시간 분할) 다시 찍으므로 스카이박스가 늦게 나타나도 곧 따라잡는다. 구름 흐름도 환경광에 반영된다.

- **하늘 재질에 `Is Sky` 가 필수다.** 실시간 캡처는 일반 메시를 그리지 않고 `Is Sky` 재질·SkyAtmosphere·볼류메트릭 구름만 찍는다. 꺼 두면 캡처에 하늘이 아예 없어 **항상** 어둡다.
- `M_Flowmap_Main` 은 서드파티(`_ThirdParty/Sky/Chaotic_Skies`) 원본을 직접 고쳤다(`Is Sky` 켬) — SpawnFX 의 서드파티 마스터 수정과 같은 방식이다. **팩을 갱신하면 다시 켠다.** 이 재질을 쓰는 다른 스카이박스 MI 도 함께 `Is Sky` 가 된다.
- `Is Sky` 재질에는 높이 안개가 화면에서 씌워지지 않는다(캡처에는 깊이 버퍼 기준으로 적용 — `r.SkyLight.RealTimeReflectionCapture.DepthBuffer = 1`). 지평선 안개 느낌이 바뀌면 이 때문이다.
- 실시간 캡처는 하늘만 찍으므로 1.5km(`SkyDistanceThreshold`) 밖 일반 지형은 더 이상 환경광에 들어가지 않는다. 지금 맵은 스카이박스뿐이라 차이가 없다.

### 비용

| 요소 | 이 맵 |
|---|---|
| 그리는 것 | `Is Sky` 재질·대기만 (씬 전체 아님) |
| 해상도 | 큐브맵 128 (해상도 제곱에 비례 — **올리지 말 것**) |
| 시간 분할 | `r.SkyLight.RealTimeReflectionCapture.TimeSlice = 1`(기본) — 하늘 그리기·밉·컨볼루션을 여러 프레임에 나눔 |
| 볼류메트릭 구름 | 없음 (실시간 캡처에서 가장 비싼 부분) |

측정: 플레이 중 `stat gpu` 의 **`CaptureConvolveSkyEnvMap`** 줄, 자세히는 `ProfileGPU`.

### 검토했지만 채택하지 않은 것

| 안 | 이유 |
|---|---|
| PSO 준비 뒤 C++ 에서 `RecaptureSky` | 코드가 늘고 컴파일이 끝나기 전까지는 어둡다. 실시간 캡처가 엔진 정식 방식 |
| 레벨 BP 에서 몇 초 뒤 `Recapture Sky` | PSO 가 그보다 늦게 끝나면 다시 어두워짐 — 시간 추측 |
| `r.PSOPrecache.ProxyCreationWhenPSOReady = 0` | 모든 메시가 PSO 없이 바로 그려져 첫 렌더 끊김이 생김 (원인 확인용으로만 씀) |
| Specified Cubemap 으로 전환 | 하늘 텍스처가 큐브맵이 아니고(2D), 흐르는 구름이 환경광에 반영되지 않음 |

## 새 맵에 하늘을 깔 때

- 스카이박스·스카이돔 메시로 하늘을 만들면 **재질 `Is Sky` + SkyLight `Real Time Capture`** 를 같이 켠다. 한 번만 찍는 SkyLight 는 위 문제를 그대로 겪는다.
- 노출을 고정(PostProcess Min·Max Brightness 같게)해 두면 환경광이 틀렸을 때 바로 화면이 어두워져 드러난다 — 자동 노출이면 어두운 캡처가 노출로 가려져 늦게 발견된다.

## 관련 문서

- 서드파티 재질을 직접 고친 다른 사례(디졸브): [SpawnFX.md](SpawnFX.md)
- 시스템 UI 의 그래픽 설정(스케일러빌리티 등): [EasyGameUI.md](EasyGameUI.md)
