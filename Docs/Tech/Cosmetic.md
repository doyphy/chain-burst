# 의상 커스터마이징 (Cosmetic)

> 캐릭터의 의상 파츠를 부위별로 교체하는 시스템. 로비에서 고른 조합이 게임플레이 레벨까지 이어진다.
> 클래스 6개를 가로지르므로 **의상 관련 작업은 이 문서를 먼저 읽는다.**
>
> 모듈러 메시의 조립 원리(리더/팔로워·리더 포즈)는 [Components.md](Components.md), 캐릭터 에셋 등록 원칙은 [Loadout.md](Loadout.md).

## 구성 요소

| 대상 | 역할 |
|---|---|
| `ECBCosmeticSlot` (`Types/CBEnumTypes.h`) | 부위 슬롯. `Helmet`/`Torso`/`Legs`/`Feet` + `MAX` |
| `UCBCosmeticCatalog` (`DataAssets/Cosmetic/`) | **교체 가능한 파츠 목록.** 조회·검증 제공 |
| `UCBChaserLoadout` | 캐릭터의 **기본 구성**을 컴포넌트에 주입 (피부·기본 의상·카탈로그) |
| `UCBModularMeshComponent` | **조립과 교체를 실제로 수행.** 카탈로그 조회·비동기 로드·검증 |
| `ACBPlayerState` | **확정된 조합을 복제.** 맵을 넘어 이관 |
| `ACBChaserController` | **요청 진입점.** 서버에서 검증 후 PlayerState 갱신 |

## 데이터 모델

```cpp
// 파츠 하나 — 부위 정보가 없다 (카탈로그의 맵 키가 부위다)
FCBCosmeticPart {
    FGameplayTag                  PartId;       // Item.Cosmetic.* (ini 등록)
    TSoftObjectPtr<USkeletalMesh> Mesh;         // 선택된 것만 로드
    FText                         DisplayName;  // UI 표시용
}

// 카탈로그 — 부위별로 나뉜다
UCBCosmeticCatalog::PartsBySlot : TMap<ECBCosmeticSlot, FCBCosmeticPartList>

// 확정 조합 (복제)
ACBPlayerState::Cosmetics : TArray<FGameplayTag>   // 인덱스 = ECBCosmeticSlot
```

**부위의 권위는 맵 키다.** 예전에는 평평한 배열에 `Slot` 필드를 두었는데, 태그 이름·`Slot` 필드가 어긋날 수 있어 "권위는 `Slot` 필드"라는 규칙이 필요했다. **키가 부위가 되면서 어긋날 대상 자체가 사라졌다.**

`FCBCosmeticPartList`는 `TMap`의 값에 배열을 직접 담을 수 없어 둔 래퍼다.

## 파츠의 원본은 카탈로그 하나다

**로드아웃은 기본 의상을 메시가 아니라 파츠 태그로 지정한다.**

```cpp
UCBChaserLoadout::DefaultCosmeticIds : TMap<ECBCosmeticSlot, FGameplayTag>
```

로드아웃이 메시를 직접 들고 있으면 **카탈로그를 거치지 않고 의상이 캐릭터에 붙는 경로**가 생긴다. 그러면 카탈로그에 없는 의상을 기본값으로 넣는 실수가 가능하고, 그 의상은 순회로 되돌아올 수도 없다. 태그로만 지정하면 **태그 픽커가 카탈로그 밖의 값을 애초에 막는다.**

여기서 따라 나오는 것: **컴포넌트는 슬롯별 "현재 파츠 태그"를 스폰 직후부터 알고 있다**(`GetCurrentCosmeticPartId`). 기본 의상도 태그를 갖기 때문이며, 덕분에 UI가 메시를 역참조하거나 "빈 태그 = 기본"을 특수 처리할 필요가 없다.

### 태그는 세 상태를 구분한다

| 값 | 의미 | 동작 |
|---|---|---|
| **빈 태그** | 선택 안 함 | 아무것도 하지 않음 → **로드아웃의 기본 의상 유지** |
| **`Item.Cosmetic.None`** | 명시적으로 벗음 | 슬롯 메시를 `nullptr`로 |
| 그 외 파츠 태그 | 착용 | 카탈로그 조회 → 비동기 로드 → 적용 |

**빈 태그를 "벗기"로 쓰면 안 된다.** `Cosmetics`의 초기값이 빈 태그 4개라, 그대로 적용하면 아무도 아무것도 고르지 않았는데 기본 의상이 전부 벗겨진다.

판정은 전부 `UCBModularMeshComponent::RequestCosmeticPart` 한 곳에 모여 있고, 상위 계층(PlayerState·컨트롤러)은 태그를 그대로 전달만 한다.

### 세 상태는 입력에만 있다 — `CurrentPartIds`는 빈 태그로 남지 않는다

위 표는 **`RequestCosmeticPart`가 받는 값**의 규칙이다. 반면 컴포넌트가 기록하는 **착용 상태(`CurrentPartIds`)에 빈 태그는 없다** — 아무것도 안 입은 슬롯도 `Item.Cosmetic.None`으로 기록한다.

`ApplyDefaultCosmetics`가 지정된 슬롯만이 아니라 **슬롯 전체를 순회**하는 이유다. 아래 셋은 전부 "안 입음"으로 수렴한다.

| 로드아웃의 `DefaultCosmeticIds` | 기록 |
|---|---|
| 슬롯이 아예 없음 | `None` |
| `Item.Cosmetic.None` 지정 | `None` |
| 카탈로그에 없는 파츠 지정 (설정 실수) | `None` + 경고 |

**빈 태그로 남기면 순회가 깨진다.** `GetCosmeticPartIdByStep`은 현재 착용 태그를 순환 목록에서 찾는데, 빈 태그는 목록에 없어 `INDEX_NONE`이 되고 맨 앞(`None`)을 반환한다. 이미 벗은 상태라 화면은 그대로고, **화살표 첫 입력이 통째로 낭비된다.** 두 번째부터 정상 동작해서 원인을 짚기 어렵다.

기본 의상이 하나도 없는 캐릭터도 마찬가지이므로, `PreloadDefaultCosmetics`는 **로드할 것이 없어 조기 반환하는 경로에서도** `ApplyDefaultCosmetics`를 부른다.

> `Item.Cosmetic.None`은 카탈로그에 없는 것이 정상이므로 **프리로드 단계에서 경고 없이 건너뛴다.** 이 예외가 없으면 정상 설정에서도 "카탈로그에서 찾지 못함" 경고가 뜬다.

파츠 태그는 데이터로 늘어나므로 **ini 등록**이고, 코드가 이름으로 비교하는 `None`만 네이티브다 → [GameplayTags.md](GameplayTags.md)

## 하드/소프트 참조 경계

```
캐릭터 ──(soft)──▶ 로드아웃 ──(hard)──▶ 피부 메시
                        └──(hard)──▶ 카탈로그 ──(soft)──▶ 파츠 메시 (기본 의상 포함)
```

| 대상 | 참조 | 이유 |
|---|---|---|
| 피부 | 하드 | 로드아웃과 함께 resolve |
| 카탈로그 자체 | 하드 | 태그·표시 이름 정도라 가볍다 |
| **카탈로그의 파츠 메시** | **소프트** | 4부위 × N종류의 스켈레탈 메시는 무겁다. 선택된 것만 로드 |

### 기본 의상도 소프트지만 준비 완료가 그 로드를 기다린다

기본 의상이 카탈로그에서 오므로 소프트다. 그런데 **"옷 없이 한 번 나타나는" 문제는 생기지 않는다** — 준비 완료 신호가 그 로드까지 기다리고, 화면은 준비 완료까지 검게 덮여 있기 때문이다 → [GameFlow.md](GameFlow.md)

```
로드아웃 async 로드
 └ ApplyToCharacter (카탈로그 + 기본 파츠 태그 주입)
 └ PreloadDefaultCosmetics — 기본 파츠 메시 async 로드   ← 2단계
     └ HandleCharacterSystemReady() 방송
```

**예전에는 기본 의상을 로드아웃의 하드 메시로 두어 이 문제를 피했다.** 그 방식은 파츠의 출처가 둘로 갈리는 대가가 있었고, 페이드가 준비 완료까지 화면을 덮게 되면서 하드로 묶을 이유가 사라졌다.

`SystemReady`의 "async 1개"가 **2단계**가 된 것이 이 변경의 대가다 → [SystemReady.md](SystemReady.md)

교체 파츠는 첫 착용 시 로드 시간만큼 반영이 늦다. 두 번째부터는 메모리에 남아 즉시 적용된다.

## 네트워크 — 복제하는 것은 조합뿐

```
UI 클릭
  → ACBChaserController::Server_RequestCosmeticPart   (RPC)
  → [서버] 폰의 카탈로그로 검증 (IsValidCosmeticPart)
  → ACBPlayerState::Auth_SetCosmeticPart              (확정)
  → 복제 → OnRep_Cosmetics → ApplyCosmeticsToPawn
  → UCBModularMeshComponent::RequestCosmeticPart      (각 인스턴스가 로컬로 조립)
```

**메시나 컴포넌트를 복제하지 않는다.** 복제되는 건 가벼운 태그 배열뿐이고, 조립은 서버·각 클라이언트가 각자 로컬로 수행한다. 덕분에 `UCBModularMeshComponent`에는 복제 코드가 없다 — 체력바가 어트리뷰트 복제만으로 동작하는 것과 같은 구조다 → [Multiplayer.md](Multiplayer.md)

- **서버는 `OnRep`이 불리지 않으므로** `Auth_SetCosmeticPart`가 직접 `OnRep_Cosmetics()`를 호출해 호스트 화면에도 반영한다.
- **검증은 폰의 컴포넌트를 거친다.** 카탈로그를 밖으로 노출하지 않고 `IsValidCosmeticPart`가 판정만 제공한다. `Item.Cosmetic.None`은 카탈로그와 무관하게 항상 통과한다(어느 부위든 벗을 수 있다).
- **맵을 넘는 이관**은 `ACBPlayerState::CopyProperties`가 담당한다. seamless travel이 꺼져 있거나 이 함수를 빠뜨리면 로비 선택이 조용히 사라진다 → [GameFlow.md](GameFlow.md)

### 적용 타이밍 — 진입점 셋, 관문 둘

PlayerState 복제 도착과 Pawn 생성은 순서가 보장되지 않는다. 그래서 **여러 곳에서 밀어넣되, 실제 적용은 두 조건이 모두 갖춰졌을 때만** 한다.

```
진입점                                   관문
 ├ BeginPlay (즉시 1회)                   ApplyCosmeticsWhenReady()
 ├ OnPawnSet 델리게이트                    ├ 폰이 없음        → 대기
 └ OnRep_Cosmetics()                      ├ 준비 전         → 준비 완료 구독하고 대기
                                          └ 준비 완료       → ApplyCosmeticsToPawn()
```

`APlayerState::OnPawnSet`은 엔진이 제공하므로 **캐릭터에는 코드를 넣지 않는다.** 캐릭터가 PlayerState를 몰라도 된다.

**`BeginPlay`의 즉시 1회 시도가 seamless travel을 살린다.** 맵을 넘으면 새 PlayerState가 만들어지는데, **폰이 붙은 뒤에 그 `BeginPlay`가 실행**되는 경우가 있다. 구독만 해두면 방송은 이미 끝난 뒤라 영영 적용되지 않는다 — 로비에서 고른 의상이 게임플레이 레벨에서 기본 의상으로 돌아가는 형태로 나타났고, **`RequestCosmeticPart`가 한 번도 불리지 않아 경고 한 줄 없이** 조용히 실패했다. [SystemReady.md](SystemReady.md)의 "구독 또는 즉시"와 같은 해법이다.

**준비 완료를 기다리는 이유는 덮어쓰기 때문이다.** 로드아웃의 기본 의상(`ApplyDefaultCosmetics`)은 준비 완료 **직전**에 적용된다. 그보다 먼저 조합을 넣으면 기본 의상이 그 위를 덮는다. 그래서 관문이 "폰이 있는가"만으로는 부족하고 "준비가 끝났는가"까지 봐야 한다.

> 캐릭터가 바뀌면(재스폰 등) 이전 구독을 해제하고 새로 구독한다. 같은 캐릭터를 이미 기다리는 중이면 중복 구독하지 않는다.

## 컴포넌트의 동작 규칙

### 의상 컴포넌트는 벗어도 유지한다

의상 슬롯은 **벗은 상태에도 컴포넌트를 남겨** 항상 슬롯 개수만큼 존재한다. 교체는 생성 없이 `SetSkeletalMeshAsset` + 리더 포즈 재연결로 끝난다.

- **이름 충돌 방지**: 파괴 후 같은 이름으로 다시 `NewObject`하면 GC 전 오브젝트와 부딪힌다. 로비에서 연타하면 실제로 만난다
- **교체 속도**: 로비에서 자주 갈아입는 시나리오에 유리하다

피부는 런타임 교체가 없으므로 **메시가 있는 것만** 만든다(빈 자리는 `nullptr`로 인덱스만 유지).

### 조립 전에는 캐시에 담긴다

`SetCosmeticForSlot`은 컴포넌트 존재 여부로 분기한다.

| 상태 | 보관처 |
|---|---|
| 조립 전 (컴포넌트 없음) | `CosmeticMeshes` 배열 |
| 조립 후 | **컴포넌트 자신** |

**같은 값을 두 곳에 쓰지 않는다.** 조립 후 `CosmeticMeshes`는 갱신되지 않으므로, **현재 착용 상태는 `CosmeticComponents[i]->GetSkeletalMeshAsset()`에서 조회**한다.

### 로드 경합 방어

비동기 로드라 연타하면 완료 순서가 뒤집힐 수 있다. `RequestedParts[Slot]`에 **마지막 요청 태그**를 기록해두고, 로드 완료 콜백에서 그 값과 다르면 버린다. 콜백은 `TWeakObjectPtr`로 캡처해 로드 중 파괴에 대비한다.

`RequestedParts`는 **요청 추적용이지 착용 상태가 아니다.**

### 카탈로그 조회는 컴포넌트가 한다

로드아웃은 `SetCosmeticCatalog()`로 주입만 하고 빠진다. `MontageData`를 받은 `UCBActionComponent`가 태그로 몽타주를 조회하는 것과 같은 구조다.

**컴포넌트에 두는 결정적 이유는 재사용이다** — 캐릭터가 아닌 액터(로비 프리뷰 등)도 컴포넌트만 붙이면 그대로 동작한다.

## 로비에서

로비는 **실제 `ACBChaserCharacter`를 스폰**한다. 프리뷰 전용 액터를 만들지 않는 이유와 대신 처리할 것(입력 차단·카메라)은 → [GameFlow.md](GameFlow.md)

**UI는 부위별 화살표 순회 방식이다.** 목록을 늘어놓지 않고 이전/다음 버튼으로 파츠를 넘긴다.

```
화살표 클릭
  → UCBModularMeshComponent::GetNextCosmeticPartId(Slot)          // 또는 GetPrevious…
  → ACBChaserController::Server_RequestCosmeticPart(Slot, 그 태그)  // 요청
```

**방향은 bool 파라미터가 아니라 함수 이름으로 가른다.** BP에 노출되는 API에서 bool 핀은 호출부만 봐서 의미를 알기 어렵고, 이 경우 화살표 버튼 두 개가 노드 두 개에 그대로 대응한다.

**인덱스를 저장하지 않는다.** 매번 현재 착용 태그(`GetCurrentCosmeticPartId`)에서 위치를 구하므로, 서버가 요청을 거부하거나 다른 경로로 값이 바뀌어도 UI가 어긋나지 않는다.

순환 목록은 **`[Item.Cosmetic.None]` + 카탈로그 등록 순서**다. 맨 앞에 벗기를 두어 순회만으로 벗을 수 있게 한다. 이 항목은 카탈로그에 등록하지 않아도 검증을 통과한다.

**카탈로그는 순서까지만 제공하고 순환 규칙은 컴포넌트가 갖는다.** 카탈로그가 "다음 파츠"를 직접 답하게 하면 *"없음을 순환에 포함한다"* 는 UI 정책이 데이터 에셋 계층으로 새고, 경계 처리(끝에서 벗기로 감기)가 두 계층에 쪼개진다.

**UI는 컴포넌트로 교체를 직접 요청하지 않는다.** 조회(`GetAdjacent…`)는 컴포넌트에서 하되, **적용은 반드시 `Server_RequestCosmeticPart`를 거쳐야** 다른 플레이어에게도 보이고 게임 레벨까지 유지된다.

> **직접 호출은 PIE 1인으로 검증되지 않는다.** 컴포넌트를 직접 부르면 그 인스턴스에서만 메시가 바뀌는데, 빈 태그는 `ApplyCosmeticsToPawn`이 건너뛰므로 로컬 변경을 되돌리지도 않는다. **화면상 아무 이상이 없고**, 다른 플레이어에게 안 보이는 것과 게임 레벨에서 사라지는 것은 나중에야 드러난다. 배선 검증은 2인 리슨 서버로 한다.

### 표시 이름은 쓰지 않는다 (현재)

`FCBCosmeticPart::DisplayName`은 정의만 되어 있고 UI가 읽지 않는다. 화살표만으로 순회하고 착용 중인 옷 이름을 띄우지 않는 선택이다.

띄우려면 함수를 하나 열어야 하는데, **`UCBCosmeticCatalog::FindPart`를 BP에 노출하는 것은 답이 아니다** — 반환 타입이 구조체 포인터라 `UFUNCTION`이 될 수 없고, 파츠 구조체를 통째로 넘기면 `Mesh` 소프트 포인터까지 딸려 나가 **컴포넌트를 우회해 메시를 적용하는 경로**가 생긴다. 필요해지면 컴포넌트에 `GetCosmeticPartDisplayName(Slot, PartId)`를 추가한다(카탈로그 조회는 컴포넌트가 한다는 위 규칙 그대로). `Item.Cosmetic.None`은 카탈로그에 항목이 없으므로 그 표시 이름도 컴포넌트가 갖는다.

**UI 갱신 신호도 같은 이유로 아직 없다.** 이름을 안 띄우면 갱신할 라벨이 없다. 도입하게 되면 `ACBPlayerState::OnRep_Cosmetics`가 아니라 **컴포넌트의 `SetCurrentPartId`** 에 다는 것이 맞다 — 순회 기준이 `GetCurrentCosmeticPartId`(컴포넌트)라 PlayerState 기준으로 갱신하면 **비동기 로드가 끝나기 전 구간에서 라벨과 화살표가 서로 다른 값을 본다.** 게다가 기본 의상은 PlayerState를 거치지 않으므로(`Cosmetics` 초기값이 빈 태그 4개) OnRep 기준이면 첫 화면의 이름이 비어 있다.

## 에셋 작성

1. 카탈로그 에셋(`UCBCosmeticCatalog`)의 **`PartsBySlot`에서 부위를 고르고** 그 목록에 항목 추가
2. `PartId`는 태그 픽커의 **Add New Gameplay Tag**로 그 자리에서 생성 (Source = `DefaultGameplayTags.ini`)
3. 명명 규칙 `Item.Cosmetic.<슬롯>.<테마><번호>` → [GameplayTags.md](GameplayTags.md)
4. 로드아웃의 `CosmeticCatalog`에 연결
5. **기본 의상은 로드아웃의 `DefaultCosmeticIds`에 파츠 태그로 지정** — 그 파츠가 카탈로그에 먼저 등록돼 있어야 한다

**배열 순서가 곧 UI 순회 순서다.** 항목을 중간에 끼워 넣으면 순회 순서가 바뀌지만, 저장·복제되는 값은 태그뿐이라 이미 고른 의상은 영향받지 않는다.

**슬롯을 추가할 때**는 `ECBCosmeticSlot`의 `MAX` 앞에 새 값을 이어 붙인다. **기존 값은 변경 금지** — 배열 인덱스이자 복제 대상이라 순서를 바꾸면 저장된 조합이 다른 부위를 가리킨다.

## 진행 상황

**완료**
- 슬롯 enum, 카탈로그, 피부/의상 분리, 런타임 교체(메시·태그), 비동기 로드·경합 방어
- PlayerState 복제, 서버 검증, `CopyProperties` 이관, 벗기 태그
- PIE 2인 검증 (양쪽 화면 반영 확인)

- 로비 레벨·GameMode BP·고정 카메라 → [GameFlow.md](GameFlow.md)
- **선택 UI** (`WBP_CB_CosmeticSelector`, `Menu` 레이어) — 화살표 순회 → `Server_RequestCosmeticPart`. 위젯 구성은 → [GameFlow.md](GameFlow.md)
- **로비 교체 2인 검증** — 서버·클라 양쪽 화면에 반영 확인

**미구현**
- 카탈로그 프리로드 (첫 착용 시 지연 제거)
- 표시 이름·UI 갱신 신호 (위 "표시 이름은 쓰지 않는다")

**추후 검토**
- **선택 조합의 영구 저장.** 지금은 세션 안에서만 유효하다. 저장이 생기면 파츠 태그는 그대로 쓸 수 있지만(이미 안정적 ID), 슬롯 enum 값 변경 금지 규칙이 더 중요해진다
- 색상·재질 변경 등 메시 교체가 아닌 커스터마이징

## 관련 문서

- 모듈러 메시 조립 원리(리더/팔로워): [Components.md](Components.md)
- 로드아웃이 담는 필드와 주입 경로: [Loadout.md](Loadout.md)
- 태그 네임스페이스·등록 경로: [GameplayTags.md](GameplayTags.md)
- 하드/소프트 참조 판단: [AssetReference.md](AssetReference.md)
- 복제 원칙·서버 권위: [Multiplayer.md](Multiplayer.md)
- 로비·맵 전환·PlayerState 이관: [GameFlow.md](GameFlow.md)
