# 주요 컴포넌트 역할

> 캐릭터에 붙는 주요 컴포넌트와 각자의 책임 요약.

| 컴포넌트 | 역할 |
|---|---|
| `UCBActionComponent` | 몽타주 재생 요청, 콤보 단계 관리, 콤보 타임아웃 |
| `UCBLocomotionProcessor` | ① 매 Tick CMC의 가속(`MaxAcceleration`)·감속(`BrakingDecelerationWalking`)을 ASC 태그로 판별해 적용. 우선순위: **대시 중(`Status.Movement.Dashing`, GA_Dash 활성 동안 ActivationOwnedTags로 부여) > Sprint > Walk > 기본 Run** — 대시는 루트모션 잔여 속도가 높아 전용 감속(`DashBrakingDeceleration`)을 쓰고, 태그가 사라진 뒤에도 `DashBrakingLingerTime`(기본 1초) 동안 대시 감속을 유지(잔여 고속 구간 처리 — GE 연장 대신 컴포넌트 로컬 linger 방식이라 예측·복제 이슈 없음) ② **파생 상태 태그(`Status.Movement.Idle`/`InAir`/`Gait.Run`)의 단일 소유자** — 각 머신에서 CMC 데이터를 읽어 비복제 루스 태그를 로컬 미러링 (Idle=틱+히스테리시스, InAir=무브먼트 모드 델리게이트, Run=Walk/Sprint 태그 이벤트 — [Locomotion.md](Locomotion.md) 파생 상태 절 참고) |
| `UCBCharacterRotationComponent` | 캐릭터 회전 처리 (Chaser 전용). 회전 보간 속도는 ASC 개이트 태그(`Status.Movement.Gait.Sprint`/`Walk`/`Run`)로 판별해 이동 데이터 에셋(`UCBCharacterMovementData`)에서 조회 — 질주 시 느리게 회전. **회전 타겟이 개이트별로 다름**: Walk/Run은 카메라 방향(aim-facing, 3인칭 슈터 — 카메라=전방 공격 방향), Sprint는 이동 입력 방향(orient-to-movement — 전방 클립 하나로 8방향). **이동도 보간된 facing 기준**으로 적용(`Input_Move`)돼 속도 방향이 회전을 따라 지연됨 — Walk/Run은 전/후·좌/우 분해(스트레이핑, 코너 관성), Sprint는 방향을 회전이 전담하므로 facing 정면으로 입력 크기만큼 직진(분해 시 방향 이중 적용되어 엉뚱하게 감). Sprint 회전 타겟용 카메라 기준 입력 방향은 `Input_Move` → `SetMoveInputDirection`으로 전달 |
| `UCBCameraControlComponent` | 카메라 줌/제어 |
| `UCBInputManagerComponent` | 입력 처리 (Chaser 전용, 소유 클라이언트 전용). InputConfig 캐시·매핑 컨텍스트 등록·액션 바인딩 + **피벗 감지·이동 입력 잠금** — `Input_Move`에서 카메라 기준 입력 방향과 현재 속도 방향의 각도가 개이트별 임계값(`FCBGaitMovementData::PivotAngleThreshold`) 이상이면 이동 입력을 개이트별 시간(`PivotInputLockDuration`)만큼 잠가 자연 감속 → Stop → 재출발(Start)을 유도. **지상 전용**(공중에서는 감지 스킵 — 공중 방향 전환에 잠금이 걸리는 오동작 방지). 피벗 잠금은 이동 입력만 차단(Look/줌/어빌리티 입력 유지)하며 시스템 준비 잠금과 별개 플래그. 로컬 전용 — 감속은 CMC 리플리케이션으로 서버/타 클라에 자동 반영 |
| `UCBUIComponent` | 캐릭터 부착형 UI의 공용 관리자 (전 캐릭터 공통). 준비 완료 후 로컬 플레이어에게는 HUD 체력 위젯을 뷰포트에, 그 외(AI·원격 캐릭터)에게는 머리 위 체력바(`UWidgetComponent`)를 생성. 위젯 클래스는 로드아웃에서 주입. 외부 접근은 `ICBUIInterface` 경유 — [UI.md](UI.md) |
| `UCBModularMeshComponent` | 모듈러 캐릭터 외형 조립 (클래스는 공통, 현재 `ACBChaserCharacter`만 인스턴스 보유). 준비 완료 후 등록된 팔로워 스켈레탈 메시들을 런타임 생성해 캐릭터 본체 메시(**리더**)에 **평면 부착**하고 `SetLeaderPoseComponent`로 리더 포즈를 따라가게 한다 (MetaHuman 바디·의상 파츠처럼 자기 애니메이션을 재생하지 않는 외형 메시가 대상). 팔로워는 애님BP를 돌리지 않고 본을 **이름으로** 매칭하므로 리더에만 있는 커스텀 본(`weapon_r1`·`ik_*`)이 없어도 되고 스켈레톤 호환 등록도 불필요. `bHideLeaderMesh`를 켜면 리더를 숨기되 **`VisibilityBasedAnimTickOption = AlwaysTickPoseAndRefreshBones`를 함께 설정**(안 하면 숨긴 리더가 애님 틱을 건너뛰어 팔로워 전체가 얼어붙음). 생성·부착은 전 인스턴스가 로컬로 수행 — 복제 코드 없음. 리더는 무기 소켓 조회 때문에 **BP에서 메시를 지정하되 `Visible`을 끈 채로 스폰**되며, 표시 여부는 이 컴포넌트가 준비 완료 시점에 확정한다.<br>**팔로워는 피부(`SkinMeshes`, 교체 불가)와 부위 슬롯별 의상(`CosmeticMeshes`)으로 나뉜다.** 목록·카탈로그·리더 숨김 여부는 **`UCBChaserLoadout`에서 주입**(`SetSkinMeshes`/`SetCosmeticMeshes`/`SetCosmeticCatalog`/`SetHideLeaderMesh`)되는 런타임 캐시라 컴포넌트·BP에 직접 등록하지 않는다. **의상 슬롯·태그·비동기 교체·경합 방어 등 커스터마이징 규칙 전반은 → [Cosmetic.md](Cosmetic.md)** — [Loadout.md](Loadout.md), [SkeletonCompatibility.md](SkeletonCompatibility.md) |
| `UCBNoiseEmitterComponent` | **[서버 전용 동작]** 이동 중 주기적으로 AI 청각용 소음(`UAISense_Hearing::ReportNoiseEvent`)을 발생 (전 캐릭터 공통). 개이트(`UCBAbilitySystemLibrary::GetCurrentGaitTag`)별 Loudness로 들리는 거리를 조절 — **유효 청취 거리 = 청취자 `HearingRange` × Loudness**. 속도가 임계값 미만(정지)이면 소음 없음 = 청각으로 감지 안 됨(의도된 스텔스 규칙). 애님 노티파이가 아닌 서버 타이머 방식이라 서버 애님 틱 스킵에도 누락 없음 — [AI.md](AI.md) |
| `UMotionWarpingComponent`(엔진) | 루트모션 몽타주(대시 등)의 방향/거리를 보정 (전 캐릭터 공통 — 플레이어/AI 모두 사용할 대비로 `ACBBaseCharacter` 소유). 워프 타겟은 어빌리티가 직접 등록하지 않고 `UCBGCN_PlayAction`이 몽타주 재생 큐 경로로 등록 — `BuildActionCueParameters()`에서 `FGameplayCueParameters::Location`/`Normal`을 채우면, 큐가 실행될 때 액션 태그 이름(`ActionTag.GetTagName()`)을 워프 타겟 이름으로 써서 등록된다. 큐 경로라 서버·소유 클라·Simulated Proxy가 전부 동일한 타겟을 받음 — [Locomotion.md](Locomotion.md) 대시 절 참고 |
| `UCBExtensionComponent` | 컴포넌트 베이스 (오너 캐스팅 등 공통 유틸 + 캐릭터 시스템 준비 소비 패턴 내장) |

## 관련 문서
- 로코모션 기능 전체 (LocomotionProcessor·RotationComponent·InputManager가 협력하는 이동 시스템): [Locomotion.md](Locomotion.md)
- `UCBActionComponent` 상세(몽타주 재생기): [Montage.md](Montage.md)
- CombatComponent(`UCBExtensionComponent` 상속): [Combat.md](Combat.md)
- 컴포넌트가 준비 완료까지 초기화를 미루는 패턴(`OnCharacterSystemReady`/`IsSystemLocked`): [SystemReady.md](SystemReady.md)
