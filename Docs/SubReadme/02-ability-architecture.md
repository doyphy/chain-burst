## 2 어빌리티 아키텍처

### 2.1 어빌리티 계층 구조
<img src="../img/Ability/어빌리티계층.png" width="750">

`Action Ability`
- 몽타주 재생, 종료 요청
- 몽타주 끝나면 어빌리티 종료함
- 태그 이벤트 대기 (애님 노티파이에서 전송 `Event_Action_EndAbility`)
  - 어빌리티 종료 태그

`Event Action Ability` (서버 전용)
- 게임플레이 이벤트로 트리거되는 액션 어빌리티.
- 이벤트는 서버에서 발행되므로 기본적으로 서버에서만 활성화함.
- 피격 어빌리티, 사망 어빌리티 등이 있음 (이벤트 받으면 실행됨)

`AI Attack Ability` (서버 전용)
- AI 공격 어빌리티는 BT 태스크 노드에서 활성화함
  - `UCBBTTask_ActivateAbility`, `UCBBTTask_ActivateAbilityAndWait`
- AI 관련 작업은 모두 서버에서 처리함.
- 데미지 GE 와 데미지 계수 설정 (어빌리티 마다 데미지 다르게 가능)
- **랜덤 공격 여부 (공격 액션 랜덤으로 재생)**
- 태그 이벤트 대기 (애님 노티파이에서 전송 `UCBAN_SendGameplayEventToOwner`)
  - `Event_Combat_TraceStart`, `Event_Combat_TraceEnd` 이벤트 대기
  - 몽타주의 노티파이 스테이트 구간에서 트레이스 활성화 (`UCBANS_WeaponTraceWindow`)

`Input Action Ability`
- 입력으로 활성화하는 액션 어빌리티
- 입력을 누르고 있으면 계속 활성화할지 설정
- 태그 이벤트 대기 (애님 노티파이에서 전송 `UCBAN_SendGameplayEventToOwner`)
  - `Event_Action_CheckInput` 이벤트 대기
  - 입력 감지 시작 (이때 입력을 누르거나 누르고 있으면 재활성화)

`Chaser Attack Ability`
- 플레이어 공격 어빌리티
- 데미지 GE 와 데미지 계수 설정 (어빌리티 마다 데미지 다르게 가능)
- **콤보 공격 여부 (재활성화할때마다 다음 콤보 시작)**
- 태그 이벤트 대기 (애님 노티파이에서 전송 `UCBAN_SendGameplayEventToOwner`)
  - `Event_Combat_TraceStart`, `Event_Combat_TraceEnd` 이벤트 대기
  - 몽타주의 노티파이 스테이트 구간에서 트레이스 활성화 (`UCBANS_WeaponTraceWindow`)