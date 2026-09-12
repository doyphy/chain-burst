## 4 이동/로코모션 시스템

### 4.1 이동/로코모션 구조
<img src="../img/Locomotion/로코모션구조.png" width="750">

**입력을 통해 ASC 와 CMC 에 태그 추가/제거 및 속도를 추가하면 이동/회전 컴포넌트에서 실시간으로 읽어서 상태 업데이트하고 관련 작업 처리함**
- 걷기, 대시, 전력질주 모두 어빌리티로 태그 추가함.
- 태그를 통해 캐릭터에 이동 속도 적용함.

`Input Manager Component`
- 사용자의 모든 입력을 받아 처리하는 컴포넌트
- 사용자의 입력 방향과 카메라 방향을 계산해 피벗 감지 및 입력 잠금 처리

`Locomotion Processor`
- 캐릭터의 이동 관련 데이터를 처리하는 컴포넌트
- 개이트별 가속/감속 처리, 개이트(Walk/Run/Sprint) 판정 및 처리, 개이트 태그 부여/제거
- Idle / InAir 판단 및 태그 부여/제거

`Character Rotation Component`
- 캐릭터의 회전 관련 데이터를 처리하는 컴포넌트
- 개이트별 회전 속도 및 회전 보간


**+) 무브먼트 데이터 에셋에서 이동,회전속도 및 피벗 각도, 잠금 시간 등 커스텀 가능**

**+) 애님 BP 에서는 매 프레임 ASC와 CMC를 읽어서 애니메이션 전환 및 재생에 사용**

### 4.2 캐릭터 애님
<img src="../img/Locomotion/애님BP템플릿.png" width="750">

**애님BP 관련 설정**

- 블렌드 설정 - [Notion - 블렌드 회전 규칙 변경](https://app.notion.com/p/39d2a74e9c6f8080abb7e82b08f6aefd?source=copy_link)

- 관성화 설정 - [Notion - 애니메이션 전환 개선](https://app.notion.com/p/v1-6_-39a2a74e9c6f807b98d0f05389a31183?source=copy_link)

- 싱크 설정 - [Notion - 애니메이션 싱크 맞추기](https://app.notion.com/p/v2-2-39f2a74e9c6f801f8d31d31705d246f5?source=copy_link)

- 슬롯 채널 설정 - [Notion - 소스 포즈 업데이트 설정](https://app.notion.com/p/3c52a74e9c6f80288516fb62f5b60f1e?source=copy_link)

- 스냅샷 설정 - [Notion - 직전 상태 저장](https://app.notion.com/p/v1-9-39b2a74e9c6f80f2a119fedcd56840d5?source=copy_link)