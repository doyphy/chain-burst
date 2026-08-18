// project
#include "Camera/CBLobbyCamera.h"

// engine
#include "Camera/CameraComponent.h"

ACBLobbyCamera::ACBLobbyCamera()
{
	// ACameraActor 는 생성자에서 화면비를 16:9 로 고정하므로, 창 비율이 다르면 남는 영역이 검게 칠해짐.
	// UI 는 카메라의 제약과 무관하게 뷰포트 전체에 그려져 그 검은 영역 위에도 나타나므로,
	// 고정을 해제해 렌더 영역과 UI 영역을 일치시킴 (게임플레이의 폰 카메라는 원래 고정이 없어 동작도 통일됨)
	GetCameraComponent()->bConstrainAspectRatio = false;
}
