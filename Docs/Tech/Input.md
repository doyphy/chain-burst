# 입력 시스템

> EnhancedInput 기반. 모든 입력은 입력 태그(`Input.*`)에 바인딩하며, InputConfig는 로드아웃에서 주입된다.

EnhancedInput 기반이며, 모든 입력은 **입력 태그(`Input.*`)에 바인딩**하여 관리한다. (문서에서 "입력 태그" = 입력 바인딩 태그)

**구성 요소:**
- `UCBInputConfig` (데이터 에셋): 캐릭터가 사용하는 입력을 정의. `DefaultMappingContext` + 두 개의 `FCBInputActionConfig` 배열(`InputTag`↔`InputAction` 쌍)을 보유.
  - `NativeInputActions` : 이동/시점/줌 등 일반 입력
  - `AbilityInputActions` : 어빌리티 발동용 입력
- `UCBInputComponent` (`UEnhancedInputComponent` 상속): 바인딩 템플릿 함수 제공.
  - `BindNativeInputAction()` : 특정 입력 태그 → 콜백 함수 1:1 바인딩
  - `BindAbilityInputAction()` : `AbilityInputActions` 전체를 순회하며 `Pressed`/`Released` 콜백에 일괄 바인딩 (콜백에 `InputTag`를 함께 전달)

**`InputConfig` 출처 (로드아웃 주입):** `UCBInputConfig`는 캐릭터에 직접 등록하지 않고 **`UCBChaserLoadout`에 등록**한다. 소유 클라이언트에서 로드아웃 로드 시 `Local_ApplyToCharacter()` → `ACBChaserCharacter::SetInputConfig()`로 주입된다.

**지연 바인딩 (필수 이해):** `InputConfig`는 로드아웃에서 **비동기**로 주입되고, `SetupPlayerInputComponent`(입력 컴포넌트 준비)는 완료 시점이 다르다. 둘 중 어느 쪽이 먼저 끝날지 보장되지 않으므로, 두 경로 모두 `TrySetupInput()`을 호출하고 준비된 것부터 순서에 상관없이 처리한다.

## 매핑 컨텍스트 등록은 명시적 허용이 필요하다

**액션 바인딩과 매핑 컨텍스트 등록은 전제조건이 다르다.**

| | 전제조건 | 가드 |
|---|---|---|
| `TryBindInputActions()` | `InputConfig` + `InputComponent` | `bInputBindingsSetup` |
| `TryRegisterMappingContexts()` | `InputConfig` + **`AllowGameplayInput()` 호출됨** | `bMappingContextsRegistered` |

**IMC 는 `AllowGameplayInput()` 이 불리기 전에는 붙지 않는다.** 허용 주체는 `ACBChaserController`이며, **로비가 아닐 때만** 호출한다.

**왜 캐릭터 스폰만으로 등록하지 않는가:** 캐릭터는 게임플레이 레벨에서만 조작된다. 예전에는 스폰과 동시에 IMC를 붙이고 **로비 UI가 그것을 걷어내주기를 기대**했는데, 그러면 두 가지가 깨진다.

- **순서 경합** — UI가 먼저 뜨면 팩의 제거 스냅샷에 IMC가 없고, 그 뒤 로드아웃이 붙인 IMC는 제거 사정권 밖이라 **로비에서 캐릭터가 움직인다**
- **UI 의존** — 클라이언트에서 UI 삽입이 실패하면 조작이 그대로 살아난다

지금은 로비에서 **IMC가 아예 없으므로** 걷어낼 필요도, UI가 성공할 필요도 없다.

**바인딩은 게이트하지 않는다.** IMC가 없으면 트리거 자체가 발생하지 않아 바인딩만으로는 아무 일도 일어나지 않는다. 둘 다 미루면 허용 시점에 바인딩까지 챙겨야 해서 조건만 복잡해진다.

**실패 모드가 뒤집힌 것에 유의한다.** 예전에는 "막혀야 하는데 움직인다"(조용함)였고, 지금은 "움직여야 하는데 안 움직인다"(즉시 발견됨)다. 게임플레이 레벨에서 캐릭터가 안 움직이면 **`AllowGameplayInput()` 호출 경로부터** 확인한다.

**바인딩 흐름:**
1. `BindNativeInputAction()`으로 일반 입력을 개별 콜백(`Input_Move` 등)에 바인딩
2. `BindAbilityInputAction()`으로 어빌리티 입력을 일괄 바인딩
3. (허용된 뒤) `RegisterMappingContexts()`로 `MappingContexts`를 EnhancedInput 서브시스템에 등록

대부분의 캐릭터 클래스는 이 `UCBInputConfig` + `UCBInputComponent` 조합으로 입력을 처리한다.

**입력 → 어빌리티 흐름:**
`UCBInputComponent` → `Input_AbilityInputPressed(Tag)` → `UCBAbilitySystemComponent::OnAbilityInputPressed(Tag)` → 해당 태그로 등록된 어빌리티 활성화 (어빌리티마다 별도 콜백 불필요, 태그로 매칭)

## UI가 게임 입력을 막는 방식 — IMC 제거

메뉴가 떠 있는 동안 캐릭터가 움직이면 안 되는데, **입력 모드로는 막을 수 없다.** UI 팩에는 `UI Only` 모드가 없어서, `Game and UI`를 유지한 채 **게임플레이 IMC만 걷어내는 것이 유일한 수단**이다.

그래서 IMC 하나를 추가할 때마다 **두 곳**을 손봐야 한다:

| 곳 | 역할 |
|---|---|
| `UCBInputConfig::MappingContexts` (로드아웃) | **추가 주체.** 언제 켤지 (`AllowGameplayInput()` 이후) |
| Global Config `Gameplay Input Mapping Contexts` | **제거 권한.** UI가 걷어낼 수 있게 등록 (`Add Mapping Context on Game Load?` = False) |

**등록을 빠뜨리면 에러 없이 메뉴 중에도 캐릭터가 움직인다.** 반대로 **메뉴가 떠도 살아 있어야 하는 IMC는 일부러 등록하지 않는다** — 상황별 조작(로비 프리뷰 회전 등)을 넣게 되면 이 경로를 쓴다. 상세는 → [EasyGameUI.md](EasyGameUI.md)

**이 경로는 게임플레이 레벨 안에서만 의미가 있다.** 로비는 IMC를 애초에 등록하지 않으므로(위 절) 일시정지·옵션 메뉴가 IMC를 걷어내는 것과는 별개 문제다.

`FCBMappingContextEntry`의 주석대로 **캐릭터 조작에 본질적인 IMC만 `UCBInputConfig`에 둔다.** 상황·전역 컨텍스트는 그 상황과 가장 밀접한 클래스(위젯 등)가 소유한다.

## 관련 문서
- 로드아웃의 `InputConfig` 주입(`Local_ApplyToCharacter`): [Loadout.md](Loadout.md)
- UI가 IMC를 걷어내는 규칙·등록 체크리스트: [EasyGameUI.md](EasyGameUI.md)
- 입력으로 발동되는 어빌리티 베이스(`UCBInputActionAbility`): [Abilities.md](Abilities.md)
- `Input.*` 태그 네임스페이스: [GameplayTags.md](GameplayTags.md)
