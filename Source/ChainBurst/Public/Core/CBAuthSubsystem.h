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
 * 세션 작업에 필요한 계정 ID(FAccountId)를 확보함.
 *
 * 제공자별 차이를 여기 한 곳에 처리
 * - Null 제공자는 계정을 자동 생성하므로 로그인이 사실상 없음. 레지스트리에서 꺼내 씀.
 * - EOS 는 Connect 로그인을 거쳐야 계정 ID 가 생기고, 그 전제인 Device ID 를 생성.
 *
 * 제공자는 ini 가 아니라 런타임 선택임. 메인 메뉴에서 RequestLogin(모드) 로 고르고,
 * 메뉴로 돌아오면 다시 고를 수 있음. 세션의 LAN 여부도 이 선택에서 파생되므로 둘이 어긋날 수 없음.
 *
 * 맵을 넘어 로그인이 유지돼야 하므로 게임 인스턴스 서브시스템으로 둠.
 */
UCLASS()
class CHAINBURST_API UCBAuthSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

#pragma region Login
	/** 로그인 요청·상태 조회. 실제 시작은 메뉴 위젯의 LAN/EOS 버튼이 호출함. */
public:
	/**
	 * [로컬] 고른 모드로 로그인을 시작함. LAN/EOS 버튼이 호출함.
	 * 로컬 플레이어가 만들어진 뒤여야 하므로 서브시스템 Initialize 에서 부르면 안 됨.
	 * 같은 모드로 이미 로그인했으면 아무것도 하지 않고, 다른 모드면 버리고 새로 로그인함.
	 * 진행 중이면 무시함 - 앞선 요청의 비동기 콜백이 뒤늦게 도착해 새 상태를 덮어쓰기 때문.
	 * @param InMode 쓸 온라인 제공자
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|Auth")
	void RequestLogin(ECBOnlineMode InMode);

	/** [Getter] 현재 고른 온라인 모드. 아직 고르지 않았으면 기본값(LAN). */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Auth")
	FORCEINLINE ECBOnlineMode GetSelectedMode() const { return SelectedMode; }

	/** [Getter] 세션을 LAN 비콘으로 다룰지. 세션 서브시스템이 생성·검색·주소·NetDriver 분기에 씀. */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Auth")
	FORCEINLINE bool IsLANMode() const { return SelectedMode == ECBOnlineMode::LAN; }

	/** [Getter] 세션 작업이 가능한 상태인지. 세션 진입점들이 이 값을 가드로 씀. */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Auth")
	FORCEINLINE bool IsLoggedIn() const { return LoginState == ECBLoginState::LoggedIn; }

	/** [Getter] 현재 로그인 상태. */
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
	 * 형식은 "[EOS:<ProductUserId>]" 이며 대괄호가 필수임.
	 * - FURL 파서가 콜론을 보고 "EOS" 를 프로토콜로 잘라내 주소가 통째로 사라지는데,
	 * - 대괄호가 있으면 IPv6 리터럴로 인식해 그 파싱을 건너뛰고 괄호만 벗겨 줌.
	 */
	FString GetLocalEOSAddress() const;

	/** [로컬] 이 게임 인스턴스의 온라인 서비스를 반환함. 없으면 nullptr. */
	UE::Online::IOnlineServicesPtr ResolveServices() const;

	/** 로그인 상태 변화 신호. (UI 에서 구독) */
	UPROPERTY(BlueprintAssignable, Category = "ChainBurst|Auth")
	FCBOnLoginStateChanged OnLoginStateChanged;

private:
	/** [로컬][Null 전용] 자동 등록된 계정을 레지스트리에서 꺼내 씀. */
	void Local_AdoptLocalAccount();

	/** [로컬] 로그인 상태와 계정 ID 를 비움. 모드를 바꿔 다시 로그인하기 전에 호출함. */
	void Local_ResetLogin();

	/** [로컬][EOS 전용] 기기 익명 계정(Device ID)을 만들고, 로그인을 요청함. (Local_LoginWithDeviceId 호출) */
	void Local_CreateDeviceId();

	/** [로컬] Device ID 자격증명으로 로그인을 요청함. Device ID 확보 후 호출됨. */
	void Local_LoginWithDeviceId();

	/** [로컬] 로그인 완료 콜백. 성공하면 계정 ID 를 보관함. */
	void Local_HandleLoginComplete(const UE::Online::TOnlineResult<UE::Online::FAuthLogin>& InResult);

	/**
	 * [로컬] 상태를 바꾸고 방송함. 같은 상태로의 재진입은 무시함.
	 * @param InNewState 새 상태
	 * @param InFailureReason 실패 시 표시할 사유 (그 외에는 빈 문자열)
	 */
	void Local_SetLoginState(ECBLoginState InNewState, const FText& InFailureReason = FText::GetEmpty());

	/** [로컬] 선택 모드에 대응하는 온라인 서비스 제공자를 반환함. */
	UE::Online::EOnlineServices Local_ResolveServicesProvider() const;

	/**
	 * [로컬] 서비스 인스턴스를 구분하는 이름을 반환함.
	 * PIE 는 한 프로세스에 게임 인스턴스가 여럿이라 월드 컨텍스트 이름으로 갈라야 함.
	 */
	FName ResolveInstanceName() const;

	/**
	 * 고른 온라인 제공자. RequestLogin 이 정하며 서비스 조회·세션 분기가 전부 이 값을 따름.
	 * 기본값은 LAN - 고르기 전에 세션 작업이 새어 나가도 외부 서비스를 건드리지 않게 함.
	 */
	ECBOnlineMode SelectedMode = ECBOnlineMode::LAN;

	/** 현재 로그인 상태 */
	ECBLoginState LoginState = ECBLoginState::NotLoggedIn;

	/** 로그인 성공 시 보관하는 계정 ID. 이후 세션 작업이 이 값을 씀. */
	UE::Online::FAccountId CachedAccountId;
#pragma endregion
};
