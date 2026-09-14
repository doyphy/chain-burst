#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Online/CoreOnline.h"
#include "Types/CBDelegates.h"
#include "Types/CBEnumTypes.h"
#include "CBAuthSubsystem.generated.h"

namespace UE::Online
{
	class IOnlineServices;
	using IOnlineServicesPtr = TSharedPtr<class IOnlineServices>;

	struct FAuthLogin;

	template <typename OpType> class TOnlineResult;
}

/**
 * 온라인 로그인 창구 서브시스템.
 * 세션 작업에 필요한 계정 ID(FAccountId)를 확보하는 것이 유일한 책임임.
 *
 * 제공자별 차이를 여기 한 곳에 가둠 —
 *  · Null 제공자는 계정을 자동 생성하므로 로그인이 사실상 없음.
 *  · EOS 는 Connect 로그인을 거쳐야 계정 ID 가 생기고, 그 전제인 Device ID 생성은
 *    OSSv2 가 감싸주지 않아 EOS SDK 를 직접 호출해야 함. (프로젝트에서 EOS 전용 코드는 이 파일뿐)
 *
 * UCBSessionSubsystem 은 이 서브시스템이 넘겨주는 계정 ID 만 쓰고 제공자를 알지 못함.
 *
 * 맵을 넘어 로그인이 유지돼야 하므로 게임 인스턴스 서브시스템으로 둠.
 */
UCLASS()
class CHAINBURST_API UCBAuthSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

#pragma region Login
	/** 로그인 요청·상태 조회. 실제 시작은 UCBGameInstance::OnStart 가 호출함. */
public:
	/**
	 * [로컬] 로그인을 시작함. 게임 인스턴스 시작 시 한 번 호출할 것.
	 * 로컬 플레이어가 만들어진 뒤여야 하므로 서브시스템 Initialize 에서 부르면 안 됨.
	 * 이미 진행 중이거나 완료된 상태면 아무것도 하지 않음.
	 */
	void RequestLogin();

	/** [Getter] 세션 작업이 가능한 상태인지. 세션 진입점들이 이 값을 가드로 씀. */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Auth")
	FORCEINLINE bool IsLoggedIn() const { return LoginState == ECBLoginState::LoggedIn; }

	/** [Getter] 현재 로그인 상태. UI 가 버튼 활성화·스피너 표시를 가름. */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Auth")
	FORCEINLINE ECBLoginState GetLoginState() const { return LoginState; }

	/**
	 * [Getter] 로그인한 계정 ID. 미로그인이면 무효한 ID 를 돌려줌.
	 * 게임 인스턴스 종료 중에도 값이 남아 있어, 세션 이탈 시점에도 그대로 쓸 수 있음.
	 */
	FORCEINLINE UE::Online::FAccountId GetLocalAccountId() const { return CachedAccountId; }

	/**
	 * [로컬] 이 클라이언트에 붙을 수 있는 P2P 주소를 반환함. 로그인 전이면 빈 문자열.
	 *
	 * 형식은 "[EOS:<ProductUserId>]" 이며 **대괄호가 필수**임 —
	 * FURL 파서가 콜론을 보고 "EOS" 를 프로토콜로 잘라내 주소가 통째로 사라지는데,
	 * 대괄호가 있으면 IPv6 리터럴로 인식해 그 파싱을 건너뛰고 괄호만 벗겨 줌.
	 * (엔진의 OSSv1 경로 FOnlineSessionEOS 도 같은 형식을 씀)
	 */
	FString GetLocalEOSAddress() const;

	/**
	 * [로컬] 이 게임 인스턴스의 온라인 서비스를 반환함. 없으면 nullptr.
	 *
	 * 인스턴스 식별 규칙을 이 함수 하나에 모음 — 서비스는 (인스턴스 이름 × 설정 이름) 조합마다
	 * 별개로 생성되므로, 로그인과 세션이 서로 다른 조합을 쓰면 "로그인은 됐는데 세션은 계정을
	 * 못 찾는" 상태가 됨. 그래서 세션 서브시스템도 반드시 이 함수를 거쳐 서비스를 얻을 것.
	 */
	UE::Online::IOnlineServicesPtr ResolveServices() const;

	/** 로그인 상태 변화 신호. 메인 메뉴 위젯이 구독해 버튼 활성화를 갱신함. */
	UPROPERTY(BlueprintAssignable, Category = "ChainBurst|Auth")
	FCBOnLoginStateChanged OnLoginStateChanged;

private:
	/**
	 * [로컬][EOS 전용] 기기 익명 계정(Device ID)을 만듦.
	 * OSSv2 가 이 과정을 감싸주지 않아 EOS SDK 를 직접 호출함.
	 * 이미 있으면 EOS_DuplicateNotAllowed 가 오는데, 재사용하면 되므로 성공으로 보고 로그인으로 넘어감.
	 */
	void Local_CreateDeviceId();

	/** [로컬] Device ID 자격증명으로 로그인을 요청함. Device ID 확보 후 호출됨. */
	void Local_LoginWithDeviceId();

	/** [로컬] 로그인 완료 콜백. 성공하면 계정 ID 를 보관함. */
	void Local_HandleLoginComplete(const UE::Online::TOnlineResult<UE::Online::FAuthLogin>& InResult);

	/**
	 * [로컬] 상태를 바꾸고 방송함. 같은 상태로의 재진입은 무시함.
	 * @param InNewState 새 상태
	 * @param InFailureReason 실패 시 표시할 사유 (그 외에는 비워 둘 것)
	 */
	void Local_SetLoginState(ECBLoginState InNewState, const FText& InFailureReason = FText::GetEmpty());

	/**
	 * [로컬] 서비스 인스턴스를 구분하는 이름을 반환함.
	 * PIE 는 한 프로세스에 게임 인스턴스가 여럿이라 월드 컨텍스트 이름으로 갈라야 함.
	 */
	FName ResolveInstanceName() const;

	/** 현재 로그인 상태 */
	ECBLoginState LoginState = ECBLoginState::NotLoggedIn;

	/** 로그인 성공 시 보관하는 계정 ID. 이후 세션 작업이 이 값을 씀. */
	UE::Online::FAccountId CachedAccountId;
#pragma endregion
};
