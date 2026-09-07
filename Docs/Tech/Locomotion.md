# 로코모션 시스템 (출발/정지 · 피벗 · 대시/스프린트 · 점프)

> 캐릭터 이동의 고급 기능 전체를 다루는 문서. 상태 머신 기반(모션 매칭 미사용)이며, 애님 인스턴스는 판정 데이터만 제공하고 실제 애니메이션 전환은 ABP 상태 머신이 수행한다. 이동 기능(개이트·피벗·대시·점프)을 추가·수정할 때 읽을 것.

## 역할 분담

| 담당 | 역할 |
|---|---|
| `UCBCharacterAnimInstance` | 판정 데이터 제공 (5축 — [AnimInstance.md](AnimInstance.md)) : 이동 방향, 이동 상태(출발/정지 판정), 개이트(+스냅샷), 전투 모드, 공중 상태 |
| `UCBLocomotionProcessor` | 매 Tick CMC 가속·감속 적용 (개이트 태그 판별 + 대시 전용 감속) |
| `UCBCharacterRotationComponent` | 캐릭터 회전 (Walk/Run=aim-facing, Sprint=orient-to-movement) |
| `UCBInputManagerComponent` | 이동 입력 적용 + **피벗 감지·입력 잠금** |
| GAS 어빌리티 | `GA_Walk`/`GA_Sprint`(속도 GE+개이트 태그), `UCBGADash`(전방 대시), `UCBGAJump`(점프) |
| ABP 상태 머신 | 전투/비전투 각 1개, 지상 4-스테이트 + 공중 3-스테이트 (아래 배선 표) |

## 개이트 (Walk / Run / Sprint)

- 개이트 = **ASC의 `Status.Movement.Gait.*` 태그** — 항상 정확히 1개 유지. Walk/Sprint(의도)는 `GA_Walk`/`GA_Sprint`가 홀드 동안 GE로 태그+속도(SetByCaller)를 부여하고 릴리즈 시 제거(`UCBGAChangeSpeed`), Run은 `UCBLocomotionProcessor`가 "Walk/Sprint 태그 둘 다 없음"에서 파생해 명시 부여(태그 이벤트 기반). 태그 분류·복제 규칙은 [GameplayTags.md](GameplayTags.md) 참고.
- 판별 우선순위 Sprint > Walk > 기본 Run — **`UCBAbilitySystemLibrary::GetCurrentGaitTag()`** 공용 헬퍼 사용 (AnimInstance·RotationComponent·InputManager·`GetGaitMontageIndex()` 공유, 중복 구현 금지).
- 개이트별 몽타주 변형을 가진 액션(무기 장착/해제 등)은 **`UCBAbilitySystemLibrary::GetGaitMontageIndex()`** 로 재생 인덱스 결정 — Idle=0, Walk=1, Run/Sprint=2 (순수 태그 조회 — Idle은 아래 파생 상태 태그, [Montage.md](Montage.md) 참고).

## 파생 상태 태그 미러링 (Idle / InAir / Gait.Run)

어빌리티가 진입점이 될 수 없는 물리 파생 상태는 **`UCBLocomotionProcessor`가 단일 소유자**로서 각 머신에서 비복제 루스 태그를 로컬 부여/제거한다 (CMC가 복제되므로 서버/오너/프록시가 각자 같은 결론 — [GameplayTags.md](GameplayTags.md) 규칙 ④).

| 태그 | 갱신 방식 | 판정 |
|---|---|---|
| `Status.Movement.Idle` | 매 틱 (속도 기반 연속 판정) | 무가속 && 수평 속도 ≤ 진입 임계(10) — 이탈 임계(15)를 따로 둔 **히스테리시스**로 경계 플래핑 방지. ABP `IsMoving()` 기준과 일치 |
| `Status.Movement.InAir` | `MovementModeChangedDelegate` (이벤트) | CMC `IsFalling()` |
| `Status.Movement.Gait.Run` | Walk/Sprint 태그 이벤트(`RegisterGameplayTagEvent`) | 두 의도 태그가 모두 없으면 부여 (파생-배타) |

- **전이 시점에만 Add/Remove** (엣지 검출) — 루스 태그는 카운트 방식이라 반복 Add는 카운트가 누적된다.
- **비복제 API만 사용** — 복제 API(`TagAndCountToAll`)와 섞으면 서버 복제분과 로컬 부여분이 겹쳐 카운트가 꼬인다.
- `EndPlay`에서 태그·구독 정리 — 플레이어 ASC는 PlayerState 소유라 캐릭터보다 오래 살아 태그가 잔류할 수 있다.
- 이 컴포넌트는 파생 상태 태그만 **쓰고**, 의도 태그(Walk/Sprint)·Dashing은 **읽기만** 한다 (읽기/쓰기 태그 분리 — 피드백 루프 차단).
- 개이트별 파라미터는 **`UCBCharacterMovementData`의 `FCBGaitMovementData`** (태그 → {`MaxSpeed`, `RotationInterpSpeed`, `PivotAngleThreshold`, `PivotInputLockDuration`}) — 로드아웃에서 캐릭터에 주입.
- 가속·감속은 `UCBLocomotionProcessor`가 매 Tick 태그 판별로 CMC에 적용. **대시 중(`Status.Movement.Dashing`)은 개이트보다 우선**해 `DashBrakingDeceleration` 적용하고, 태그 소멸 후에도 `DashBrakingLingerTime`(기본 1초) 동안 유지(linger — 루트모션 잔여 고속 구간 처리, GE·복제 개입 없는 컴포넌트 로컬 방식).

## 출발/정지 판정 — 비예측, 속도 비율 기준

모션 매칭의 예측 데이터 없이 **현재 프레임 데이터만으로** 판정한다. 기준은 절대 속도가 아니라 **목표 개이트 최대 속도 대비 비율**(`CurrentSpeedRatio = CurrentVelocitySize / CachedGaitMaxSpeed`).

- **`IsStarting()`** = `bHasAcceleration && CurrentSpeedRatio < StartSpeedRatioThreshold`(기본 0.3) — 가속 중인데 아직 목표 속도에 크게 못 미치는 "출발" 구간.
- **`IsStopping()`** = `!bHasAcceleration && CurrentSpeedRatio > StopSpeedRatioThreshold`(기본 0.3) — 무입력인데 아직 속도가 남은 "정지" 구간. 절대 속도 기준이면 살짝 탭한 잔여 속도에도 Stop이 재생돼버려 비율 기준으로 설계. 비율 미만이면 Stop 생략하고 `Move → Idle` 직행.
- **`GetSpeedRatio()`**는 출발/정지 클립의 재생 속도·블렌드 선택에도 재사용.

**`IsMoving()` vs `bHasAcceleration` (전이 조건 선택 기준):**
- `IsMoving()`(가속 또는 잔여 속도)은 "이동 애니메이션을 틀어야 하는 상태" 판정용 — Idle↔Move 같은 상위 전이에 적합.
- `bHasAcceleration`은 "그 프레임에 입력이 살아있는지"만 보는 순수 신호 — **Stop 도중 재입력 감지는 반드시 이걸로** (Stop 중엔 잔여 속도 때문에 `IsMoving()`이 내내 참이라 오판). 피벗 판정도 동일.

## ABP 상태 머신 — 지상 (Idle/Start/Move/Stop)

전투/비전투 상태머신 각각 동일 구조. 4개 플랫 스테이트.

| From → To | 조건 | Priority |
|---|---|---|
| Idle → Start | `IsStarting()` | 1 |
| Idle → Move | `bHasAcceleration && !IsStarting()` | 1 |
| Idle → Stop | `IsStopping()` | 1 |
| Start → Idle | `!bHasAcceleration` | 1 |
| Start → Move | Automatic Rule (Start 클립 재생 완료) | 2 |
| Move → Idle | `!bHasAcceleration && !IsStopping()` | 1 |
| Move → Stop | `IsStopping()` | 1 |
| Stop → Idle | Automatic Rule (Stop 클립 재생 완료) | 2 |
| Stop → Start | `IsStarting() && bHasAcceleration` | 1 |
| Stop → Move | `!IsStarting() && bHasAcceleration` | 1 |

**설계 원칙:**
- Priority Order는 **같은 소스 스테이트의 전이끼리만** 경쟁. 같은 값(1)을 공유하는 전이들은 조건이 상호 배타라 충돌 없음.
- **인터럽트(재입력)는 항상 자연 완료(Automatic Rule)보다 Priority를 높게** — 애니메이션이 덜 끝나도 재입력이 이긴다.
- **Idle에는 "이미 고속인 상태" 출구가 필수** (`Idle → Move`/`Idle → Stop`) — 루트모션(대시)·넉백 등 외부 속도 소스로 고속이 된 채 Idle에 있으면 `IsStarting()`(저속 전용)만으론 영영 못 빠져나가 미끄러진다. 몽타주 재생 중에도 하위 상태 머신은 계속 평가되므로 대시 중 `Idle → Move`가 미리 발동해 블렌드 아웃 시 루프로 자연 인계.

## ABP 상태 머신 — 공중 (JumpStart/JumpLoop/JumpLand)

**Ground = State Alias** (Idle·Start·Move·Stop·**JumpLand** 5개 묶음). 별칭은 **소스 전용** — 타겟으로는 단일 스테이트 별칭만 허용되므로 복귀 전이는 구체 스테이트(Move/Idle)로 직접 연결한다. JumpLand를 별칭에 포함해야 착지 모션 중 재점프/재낙하가 가능.

| From → To | 조건 | Priority | 비고 |
|---|---|---|---|
| Ground(별칭) → JumpStart | `IsInAir() && GetVerticalVelocity() > 0` | 1 | 점프로 이륙 |
| Ground(별칭) → JumpLoop | `IsInAir() && GetVerticalVelocity() <= 0` | 1 | 절벽 낙하 (도약 모션 생략) |
| JumpStart → JumpLoop | Automatic Rule (또는 `Vz < 0` = 정점 통과) | 2 | 중력을 높이면 Vz 조건이 더 안전 |
| JumpStart → JumpLand | `!IsInAir()` | 1 | 짧은 점프 — 도약 클립 중 착지 인터럽트 |
| JumpLoop → JumpLand | `!IsInAir()` | 1 | |
| JumpLand → Move | `bHasAcceleration` | 1 | 달리며 착지 → 착지 모션 끊고 이동 |
| JumpLand → Idle | Automatic Rule (착지 클립 재생 완료) | 2 | |

## 전이 블렌드 = 관성화 (Inertialization)

- 지상·공중 **모든 전이의 Blend Logic은 Inertialization** — 크로스페이드는 인터럽트가 중첩되면 블렌드가 쌓여 뭉개지지만, 관성화는 전환 순간 포즈+속도를 스냅샷 떠 오프셋만 감쇠하므로 빠른 끊어치기 입력에도 매끄럽다 (동시 평가가 없어 비용도 쌈).
- 전이 설정은 "요청"일 뿐 — **최종 Output Pose 직전의 Inertialization 노드 1개**가 상류의 모든 요청(로코모션 전이 + 전투/비전투 bool 전환)을 실제 처리한다. 노드가 없으면 경고와 함께 팝핑.
- 포즈 차이가 큰 저빈도 전환(전투 진입/해제 등)은 관성화가 아니라 **전환 전용 애니메이션/몽타주**가 정답 (관성화는 잔차만 흡수).

## 개이트 스냅샷 — Start/Stop 재생 중 클립 스왑 방지

- Start/Stop의 개이트별 클립 선택에 실시간 `CurrentLocomotionGait`를 꽂으면 재생 중 개이트 변경 시(특히 **Sprint 자동 해제** — 정지 진입 0.2초 뒤 태그 제거로 매번 발생) 클립이 스왑되어 끊긴다.
- 셀렉터류 노드는 무엇이든 입력 핀을 매 프레임 재평가하므로 **값을 고정**해야 한다: Start/Stop 내 포즈 노드의 **On Become Relevant**(Anim Node Function)에 `LockCurrentGait` 바인딩(진입 시 1회) → Blend Poses 핀은 **`LockedLocomotionGait`** 사용. Move 루프는 실시간 개이트 유지.

## 피벗 — ABP 스테이트가 아니라 입력 잠금 방식

- 급반전 시 전용 스테이트/애니메이션 없이, `UCBInputManagerComponent::TryDetectPivot`이 **이동 입력 방향 vs 속도 방향 각도**가 개이트별 임계값 이상이면 이동 입력을 개이트별 시간만큼 잠근다 → 무입력이 되며 기존 Stop→Start 흐름이 비주얼 담당. **ABP 변경 없음.**
- 게이트: 최소 속도 비율(`PivotMinSpeedRatio`) + **지상 전용**(공중 스킵). 잠금은 이동 입력만 차단(Look/줌/어빌리티 유지), 시스템 준비 잠금과 별개 플래그. 로컬 전용 — 감속은 CMC 복제로 전 클라 반영.
- 개이트별 임계값·잠금 시간은 `FCBGaitMovementData` (예: Sprint 90도/길게, Walk 180도 반전만/짧게).

## 대시 & Sprint (Sprint는 대시에 종속)

- **Sprint 키 = 전방 대시 트리거.** `UCBGADash`가 전방 대시 몽타주(루트모션, `Action.Movement.Dash`)를 기존 GameplayCue 경로로 재생. 재생 인덱스는 전투 상태(`Status.Combat.InCombat`)로 분기 — 비전투 인덱스 0, 전투 인덱스 1 (`SelectActionMontageIndex()` 재정의). 8방향은 Sprint 루프가 전방뿐이라 폐기.
- **방향/거리는 몽타주의 baked 이동량이 아니라 모션 워핑으로 결정.** `UCBGADash::BuildActionCueParameters()`가 재생 직전 CMC `GetCurrentAcceleration()`(입력 없으면 전방 폴백)으로 방향을 구하고, 그 방향으로 `DashDistance`(에디터에서 무기별 조정 가능, 기본 700)만큼 떨어진 지점을 `FGameplayCueParameters`의 `Location`/`Normal`에 실어 큐로 보낸다.
  - **워프 타겟 등록은 액션 전용이 아니라 `UCBActionAbility` 공용 경로다.** `UCBGCN_PlayAction`이 몽타주 재생 **직전**(같은 프레임)에, `Parameters.Location`이 0이 아니면 대상의 `UMotionWarpingComponent`에 `ActionTag.GetTagName()`(예: `Action.Movement.Dash`)을 워프 타겟 이름으로 등록한다 — 몽타주 인덱스와 같은 큐 경로라 서버·소유 클라·Simulated Proxy가 전부 동일한 워프 타겟을 받는다. 대시 전용 상수가 아니라 **액션 태그를 그대로 재사용**하므로, 앞으로 다른 액션(공격 등)이 모션 워핑을 쓰고 싶으면 해당 어빌리티가 `BuildActionCueParameters()`에서 `Location`/`Normal`만 채우면 되고 `UCBGCN_PlayAction`은 손댈 필요 없다. `Location`을 안 채우는 액션은 그냥 스킵된다(무해).
  - 방향 소스로 `CBCharacterRotationComponent`의 `CachedMoveInputDir`(로컬 InputManager 전용) 대신 **CMC 가속도**를 쓰는 이유: 가속도는 이동 입력 리플리케이션/예측으로 서버 인스턴스에서도 항상 정확하지만, `CachedMoveInputDir`은 로컬 컨트롤 폰에서만 채워져 서버 쪽 리모트 클라 폰에서는 비어있을 수 있다.
  - **대시 몽타주(`AM_Chaser_*_Dash`/`*_Combat_Dash`)에는 이동 구간 전체를 덮는 Motion Warping AnimNotifyState가 배치되어 있어야 한다** (Root Motion Modifier: Skew Warp, Translation+Rotation 워프 켜짐, Warp Target Name = **`"Action.Movement.Dash"`** — 액션 태그 문자열과 정확히 일치해야 함). 이 노티파이가 없으면 워프 타겟만 등록되고 아무 효과가 없다(무해하게 무시됨). 다른 액션에 워프를 추가할 때는 그 액션의 태그 문자열을 그대로 Warp Target Name에 쓰면 된다.
  - [루트모션 재생 중 회전 잠금](#루트모션-재생-중-회전-잠금)과 공존한다 — 회전 잠금은 액터 회전을 안 건드리는 쪽으로 막을 뿐이고, 모션 워핑은 루트모션 델타 자체를 보정하는 별개 파이프라인(`ProcessRootMotionPreConvertToWorld`)이라 서로 간섭하지 않는다.
- **종속 체인**: ① `GA_Sprint`는 `ActivationRequiredTags = Status.Movement.Dashing`으로 대시 없인 활성화 불가 (대시 쿨다운이면 Sprint도 발동 불가) ② 대시 성공 시 `UCBGADash`가 `TryActivateAbilitiesByTag(Ability.Movement.Sprint)`로 Sprint를 직접 활성화(부여 순서 무관) ③ Sprint는 릴리즈 또는 **가속 소실 자동 종료**(`bEndWhenNoAcceleration`, 유예 0.2초 — 정지·피벗 잠금 포함)로 해제.
- 대시 쿨다운은 GAS 표준(`CooldownGameplayEffectClass`), 활성 중 `Status.Movement.Dashing` 부여(ActivationOwnedTags) — 대시 감속·점프 차단·Sprint 활성화 조건에 사용. 상세: [Abilities.md](Abilities.md)
- 대시 종료 후 상태 머신 인계는 Idle 고속 출구(`Idle → Move`)가 담당 (위 지상 표).

## 루트모션 재생 중 회전 잠금

- `UCBCharacterRotationComponent`는 매 틱 카메라(Walk/Run) 또는 이동 입력(Sprint) 방향으로 `SetActorRotation`을 호출한다. 엔진은 로컬 루트모션을 **"그 프레임의 액터 회전"** 기준으로 월드 변환하므로, 이 컴포넌트가 손대지 않으면 루트모션 몽타주(대시·공격 등)는 시작 시점 방향 그대로 재생되지만, 계속 회전을 갱신하면 재생 중간에 궤적/방향이 라이브 입력을 따라 꺾여버린다.
- 그래서 `TickComponent` 최상단에서 **`Character->IsPlayingRootMotion()`이면 그 틱의 회전 갱신 전체를 스킵**한다 — 특정 어빌리티 태그(`Status.Movement.Dashing` 등)에 의존하지 않고 엔진의 루트모션 재생 여부 자체로 게이팅하므로, 대시뿐 아니라 앞으로 추가될 모든 루트모션 몽타주(공격 등)에 동일하게 적용된다.
- 몽타주 내에서 캐릭터를 특정 타겟(적 위치 등)으로 정밀하게 회전/이동시키는 것은 이 잠금과 별개인 **모션 워핑**이 담당한다 — 루트모션 델타를 월드 변환 직전에 보정하는 정식 시스템이라 액터 회전을 직접 덮어쓰는 방식보다 안전하다. 전제 조건·제자리 클립 처리·배선은 → [Montage.md](Montage.md)

## 점프 & 공중

- `UCBGAJump`(C++ 최종 엣지)는 **CMC `Jump()`/`StopJumping()`만 트리거** — 몽타주 없음. `CanActivateAbility`에서 `CanJump()` 검사, 릴리즈 시 `StopJumping()`(가변 높이), 대시 중 `ActivationBlockedTags` 차단. `bPressedJump`는 CMC 압축 플래그로 자동 복제.
- 공중 애니메이션은 어빌리티와 무관하게 상태 머신이 ⑤축(`IsInAir()`/`GetVerticalVelocity()`)으로 처리 — **절벽 낙하 등 비점프 공중 상태와 공용** (위 공중 표).
- 점프 감 튜닝은 CMC/Character 프로퍼티: `JumpZVelocity`(높이), `GravityScale`(속도감), `AirControl`(공중 조작), `JumpMaxHoldTime`(가변 높이), `JumpMaxCount`(다단 점프 — `CanJump()`가 자동 반영). 높이 = `JumpZVelocity² ÷ (2 × 980 × GravityScale)`.

## 캐릭터 간 충돌 — 밟기 금지 + 밀림 상한

`ACBBaseCharacter` 생성자에서 세 값을 고정한다. **캐릭터끼리는 서로 밀되, 튕겨나가거나 올라타지 못한다**는 규칙.

| 설정 | 값 | 막는 것 |
|---|---|---|
| 캡슐 `WalkableSlopeOverride` | `WalkableSlope_Unwalkable` | 캐릭터 위에 착지·정지 |
| 캡슐 `CanCharacterStepUpOn` | `ECB_No` | 옆에서 부딪혀 기어오르기 |
| CMC `MaxDepenetrationWithPawn` | `20.f` (엔진 기본 100) | 프레임당 튕겨나가는 거리 |

### 왜 필요한가 — 루트모션 공격에 밀려 날아가던 문제

AI 가 모션 워핑 루트모션으로 접근할 때, 플레이어가 점프해 적 캡슐 위에 닿으면 **그 적이 무빙 베이스가 된다.** 캡슐 윗면이 둥글어 곧 미끄러져 떨어지는데, 그 전환 순간 엔진이 베이스 속도를 그대로 얹는다.

```cpp
// UCharacterMovementComponent::OnMovementModeChanged
if (MovementMode == MOVE_Falling) {
    if (bStayBasedInAir == false) { ApplyImpartedMovementBaseVelocity(); }
}
// ApplyImpartedMovementBaseVelocity
Velocity += GetImpartedMovementBaseVelocity();   // 베이스(= 돌진 중인 적)의 속도
```

루트모션 공격 속도가 그대로 더해지므로 관성까지 남아 멀리 날아간다.

- **`CanCharacterStepUpOn` 만으로는 못 막는다.** 착지 판정 `IsValidLandingSpot()` 은 이 값을 보지 않고 `IsWalkable()` 만 본다. 캡슐 꼭대기는 법선이 위를 향해 "걸을 수 있는 면"으로 통과된다. **걸을 수 없는 면으로 선언해야** 착지·`FindFloor` 가 모두 실패해 베이스 자체가 성립하지 않는다(`WalkableSlope_Unwalkable` → `ModifyWalkableFloorZ` 가 2.0 을 반환, 법선 Z 는 최대 1).
- **`bImpartBaseVelocityX/Y/Z` 를 끄지 않는 이유**: 그건 움직이는 발판까지 함께 죽인다. 캡슐만 걸을 수 없게 만들면 캐릭터만 정확히 제외된다.

### 밀림은 힘이 아니라 스스로 빠져나오는 것

캐릭터끼리는 **물리력이 오가지 않는다.** `ApplyImpactPhysicsForces`·`ApplyRepulsionForce` 둘 다 `IsInstanceSimulatingPhysics()` 가드가 있어 시뮬레이션 중인 바디에만 작용하고, 캡슐은 해당하지 않는다. 공격자는 sweep 으로 이동하다 막히면 **자기가** 멈추거나 미끄러진다.

실제로 일어나는 일은 **밀리는 쪽이 자기 프레임에 침투를 감지하고 스스로 빠져나오는 것**(`GetPenetrationAdjustment` → `ResolvePenetration`)이고, 이때 `MaxDepenetrationWithPawn` 이 프레임당 상한이다. 기본 100cm 면 상대가 계속 밀고 들어올 때 초당 수십 m 로 튄다.

- **이 이동은 속도를 만들지 않는다.** `ResolvePenetrationImpl` 이 `bJustTeleported` 를 세우고, `PhysWalking`/`PhysFalling` 의 "실제 이동량으로 속도 재계산" 코드가 전부 이 플래그로 막혀 있다. 그래서 값만 낮추면 **관성 없이 밀리기만** 한다.
- **진단**: `p.VisualizeMovement 1` 로 머리 위 `Velocity`/`Speed` 를 본다. 위치만 튀고 Speed 가 그대로면 침투 해소, Speed 가 뛰면 무빙 베이스 임파트. `showdebug physics` 의 `Based On <액터>` 로 베이스 여부를 직접 확인할 수 있다.

⚠️ **부작용**: 어떤 캐릭터도 다른 캐릭터 위에 설 수 없다. "적을 밟고 점프" 같은 기믹이 필요해지면 해당 캐릭터만 예외를 둬야 한다.

## 관련 문서
- 애님 데이터 5축·스레딩·계층: [AnimInstance.md](AnimInstance.md)
- 루트모션 접근을 만드는 모션 워핑: [Montage.md](Montage.md)
- 어빌리티 계층·Sprint 종속 체인·AssetTags 규칙: [Abilities.md](Abilities.md)
- 컴포넌트 역할 (LocomotionProcessor / RotationComponent / InputManager): [Components.md](Components.md)
- 개이트별 이동 데이터 주입 (로드아웃): [Loadout.md](Loadout.md)
- 몽타주(대시) 재생·동기화: [Montage.md](Montage.md)
- 서버 권위·복제 원칙: [Multiplayer.md](Multiplayer.md)
