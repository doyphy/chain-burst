#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "CBLobbyCamera.generated.h"

/**
 * 로비 대기실의 고정 카메라.
 * 레벨에 배치되어 있으면 ACBChaserController 가 폰 대신 이 카메라를 뷰 타겟으로 삼음.
 *
 * 동작은 화면비 고정 해제뿐이고 타입 자체가 식별자 역할을 함
 * 이 클래스가 배치되지 않은 레벨은 로비가 아닌 것으로 간주되어 게임플레이 레벨에는 배치하지 말 것.
 */
UCLASS()
class CHAINBURST_API ACBLobbyCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	/** ACameraActor 가 켜 두는 화면비 고정을 해제함 (창을 꽉 채우기 위함). */
	ACBLobbyCamera();
};
