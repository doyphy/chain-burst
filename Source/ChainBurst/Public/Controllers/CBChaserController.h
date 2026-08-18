#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Types/CBEnumTypes.h"
#include "CBChaserController.generated.h"

class ACameraActor;
class ACBBaseCharacter;
class ACBLobbyCamera;

/**
 * 플레이어(추격자)의 컨트롤러.
 * 의상 교체 요청을 서버로 중계하고, 뷰 타겟이 정해지고 캐릭터가 준비될 때까지 화면을 검게 덮음.
 * 로비 레벨에서는 폰 대신 레벨의 고정 카메라를 뷰 타겟으로 삼으며, 커스터마이징 중에는 내 캐릭터 앞으로 당긴 카메라로 전환함.
 */
UCLASS()
class CHAINBURST_API ACBChaserController : public APlayerController
{
	GENERATED_BODY()

#pragma region Cosmetic
public:
	/**
	 * [클라 → 서버] 의상 파츠 교체 요청. (서버에서만 실행)
	 * 서버가 폰의 카탈로그로 검증한 뒤 PlayerState 에 반영함. 외형 적용은 복제된 값을 받은 쪽이 각자 로컬로 수행함.
	 * @param InSlot 교체할 부위 슬롯
	 * @param InPartId 입힐 파츠 태그 (빈 태그 = 선택 해제, 로드아웃의 기본 의상 유지)
	 */
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "ChainBurst|Cosmetic")
	void Server_RequestCosmeticPart(ECBCosmeticSlot InSlot, FGameplayTag InPartId);
#pragma endregion

#pragma region Camera
public:
	/**
	 * [로컬] 커스터마이징 뷰로 전환하는 함수. 내 캐릭터 앞에 카메라를 놓고 뷰 타겟을 옮김.
	 * 로비 카메라가 없는 레벨(게임플레이)에서는 아무것도 하지 않음.
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Lobby")
	void Local_EnterCosmeticView();

	/** [로컬] 커스터마이징 뷰에서 공용 로비 뷰로 되돌리는 함수. */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Lobby")
	void Local_ExitCosmeticView();

	/** 커스터마이징 뷰 상태인지 여부 (위젯이 표시 분기에 사용). */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Lobby")
	FORCEINLINE bool IsInCosmeticView() const { return bInCosmeticView; }

protected:
	/**
	 * 뷰포트/넷 커넥션이 이 컨트롤러에 연결된 직후 호출됨.
	 * 빙의보다 앞서 호출되므로 여기서 화면을 검게 덮음.
	 */
	virtual void ReceivedPlayer() override;

	/** 빙의·빙의 해제 등으로 뷰타겟을 다시 설정해야 할 때 호출. 로비면 폰 대신 로비 카메라를 선택 */
	virtual void AutoManageActiveCameraTarget(AActor* SuggestedTarget) override;

private:
	/**
	 * 레벨의 로비 카메라를 반환함. 없으면 nullptr (= 로비가 아님).
	 * 이 함수는 빙의 과정에서 여러 번 불리므로 찾은 결과를 캐시함. 못 찾은 경우는 캐시하지 않음
	 * (만약 맵을 넘어 컨트롤러가 살아남는 경우 다시 안 불릴 수 있으니 맵 전환 시 예외 처리 해주기.)
	 */
	ACBLobbyCamera* Local_ResolveLobbyCamera();

	/**
	 * [로컬] 검은 화면을 걷어내는 요청.
	 * 폰이 아직 준비 중이면 준비 완료 신호를 구독하고 대기.
	 */
	void Local_RequestFadeIn();

	/** [로컬] 캐릭터 시스템 준비 완료 콜백. 구독을 해제하고 준비 완료 처리로 넘김. */
	void Local_HandleCharacterSystemReady();

	/**
	 * [로컬] 캐릭터 준비 완료 시 처리.
	 * 게임플레이 입력 허용 → 준비 완료 방송 → 화면 열기 순서.
	 */
	void Local_ApplyReadyState();

	/**
	 * [로컬] 게임플레이 레벨이면 캐릭터의 입력 매니저에 매핑 컨텍스트 등록을 허용함.
	 * 로비에서는 호출하지 않아 IMC 가 아예 붙지 않음.
	 */
	void Local_TryAllowGameplayInput();

	/** [로컬] 검은 화면을 ScreenFadeInDuration 에 걸쳐 걷어냄. */
	void Local_FadeInFromBlack();

	/**
	 * [로컬] 커스터마이징 카메라를 반환함. 첫 호출 시 스폰하고 이후 재사용.
	 * 매번 스폰·파괴하면 블렌드가 끝나기 전에 대상이 사라지므로 하나를 계속 옮겨 씀.
	 */
	ACameraActor* Local_ResolveCosmeticViewCamera();

	/** 찾아둔 로비 카메라. 유효하면 재검색하지 않음. */
	TWeakObjectPtr<ACBLobbyCamera> CachedLobbyCamera;

	/**
	 * 로컬 스폰한 커스터마이징 카메라. 복제하지 않으므로 내 화면에만 영향을 줌.
	 * 맵을 넘으면 이전 월드와 함께 사라지므로, 유효하지 않으면 다시 스폰함.
	 */
	UPROPERTY()
	TObjectPtr<ACameraActor> CosmeticViewCamera = nullptr;

	/**
	 * 커스터마이징 뷰 상태인지.
	 * AutoManageActiveCameraTarget 이 뷰 타겟을 되돌리지 않게 막는 게이트를 겸함
	 */
	bool bInCosmeticView = false;

	/**
	 * 준비 완료 처리를 이미 수행했는지.
	 * PlayerCameraManager 의 FadeAmount 는 DoUpdateCamera 에서만 갱신되므로 프레임 안에서는 신뢰할 수 없다.
	 * AutoManageActiveCameraTarget 이 빙의 과정에서 여러 번 불리는 만큼, 자체 플래그로 1회만 수행하게 함.
	 */
	bool bReadyStateApplied = false;

	/** 준비 완료를 기다리는 중인 캐릭터. 구독 해제 대상 추적용. */
	TWeakObjectPtr<ACBBaseCharacter> PendingReadyCharacter;

	/** PendingReadyCharacter 의 준비 완료 델리게이트 구독 핸들. 유효하면 이미 대기 중이라는 뜻. */
	FDelegateHandle CharacterSystemReadyHandle;

	/** 검은 화면에서 밝아지는 데 걸리는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Camera")
	float ScreenFadeInDuration = 0.3f;

	/** [커스터마이징 뷰] 캐릭터 정면으로 얼마나 떨어져 볼지 (언리얼 단위). */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Camera|CosmeticView")
	float CosmeticViewDistance = 250.0f;

	/** [커스터마이징 뷰] 캐릭터의 어느 높이를 볼지 (액터 원점 기준 오프셋). */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Camera|CosmeticView")
	float CosmeticViewHeight = 0.0f;

	/**
	 * [커스터마이징 뷰] 캐릭터를 화면 한쪽으로 밀어내는 Yaw 오프셋(도).
	 * 반대쪽 공간이 커스터마이징 위젯 자리가 됨. 카메라를 옆으로 옮기면 캐릭터가 다시 중앙으로 오므로 회전으로 처리함.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Camera|CosmeticView")
	float CosmeticViewYawOffset = -20.0f;

	/** [커스터마이징 뷰] 뷰 전환에 걸리는 시간(초). 진입·복귀 공용. */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Camera|CosmeticView")
	float CosmeticViewBlendTime = 0.4f;
#pragma endregion
};
