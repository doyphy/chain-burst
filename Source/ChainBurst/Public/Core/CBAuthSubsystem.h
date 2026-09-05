#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Online/CoreOnline.h"
#include "CBAuthSubsystem.generated.h"

namespace UE::Online
{
	class IOnlineServices;
	using IOnlineServicesPtr = TSharedPtr<class IOnlineServices>;

	struct FAuthLogin;

	template <typename OpType> class TOnlineResult;
}

/** 로그인 진행 상태. UI 가 버튼 활성화·스피너 표시를 결정하는 데 씀. */
UENUM(BlueprintType)
enum class ECBLoginState : uint8
{
	/** 아직 시도하지 않음 */
	NotLoggedIn,
	/** 진행 중 (세션 작업 불가) */
	LoggingIn,
	/** 완료 (세션 작업 가능) */
	LoggedIn,
	/** 실패 (사유는 방송에 실려 옴) */
	Failed
};

/** 로그인 상태 변화 신호. (새 상태, 실패 사유 — 실패가 아니면 비어 있음) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCBOnLoginStateChanged, ECBLoginState, NewState, const FText&, FailureReason);

/**
 * 온라인 로그인 창구 서브시스템.
 * 세션 작업에 필요한 계정 ID(FAccountId)를 확보하는 것이 유일한 책임임.
 *
 * 제공자별 차이를 여기 한 곳에 가둠 —
 *  · Null 제공자는 계정을 자동 생성하므로 로그인이 사실상 없음.
 *  · EOS 는 Connect 로그인을 거쳐야 계정 ID 가 생기고, 그 전제인 Device ID 생성은
 *    OSSv2 가 제공하지 않아 EOS SDK 를 직접 호출해야 함. (이 파일이 유일한 EOS 전용 코드)
 *
 * UCBSessionSubsystem 은 이 서브시스템이 넘겨주는 계정 ID 만 쓰고 제공자를 알지 못함.
 *
 * 맵을 넘어 로그인이 유지돼야 하므로 게임 인스턴스 서브시스템으로 둠.
 */
UCLASS(Config = Engine)
class CHAINBURST_API UCBAuthSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

#pragma region Core
	/** 서브시스템 수명 주기 */
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
#pragma endregion

#pragma region Login
	/** 로그인 요청·상태 조회. 실제 호출은 UCBGameInstance::OnStart 가 시작함. */
public:
	/**
	 * [로컬] 로그인을 시작함. 게임 인스턴스 시작 시 한 번 호출할 것.
	 * 로컬 플레이어가 만들어진 뒤여야 하므로 서브시스템 Initialize 에서 부르면 안 됨.
	 * 이미 진행 중이거나 완료된 상태면 아무것도 하지 않음.
	 */
	void RequestLogin();

	/** [Getter] 세션 작업이 가능한 상태인지. 세션 진입점들이 이 값을 가드로 씀. */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Auth")
	bool IsLoggedIn() const { return LoginState == ECBLoginState::LoggedIn; }

	/** [Getter] 현재 로그인 상태. UI 가 표시를 가름. */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Auth")
	ECBLoginState GetLoginState() const { return LoginState; }

	/**
	 * [Getter] 로그인한 계정 ID. 미로그인이면 무효한 ID 를 돌려줌.
	 * 게임 인스턴스 종료 중에도 유효하므로, 세션 이탈 시점에도 그대로 쓸 수 있음.
	 */
	UE::Online::FAccountId GetLocalAccountId() const { return CachedAccountId; }

	/**
	 * [로컬] 이 게임 인스턴스의 온라인 서비스를 반환함. 없으면 nullptr.
	 *
	 * 인스턴스 식별 규칙을 이 함수 하나에 모음 — 서비스는 (인스턴스 이름 x 설정 이름) 조합마다
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
	 * OSSv2 는 이 과정을 제공하지 않아 EOS SDK 를 직접 호출함.
	 * 이미 있으면 EOS_DuplicateNotAllowed 가 오는데 그것도 정상으로 보고 로그인으로 넘어감.
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
	 * EOS 플랫폼 설정 이름. ini 의 [EOSSDK.Platform.<이름>] 섹션과 일치해야 함.
	 * Device ID 생성에 쓸 플랫폼 핸들을 이 이름으로 조회함.
	 */
	UPROPERTY(Config)
	FString PlatformConfigName = TEXT("ChainBurst");

	/** 현재 로그인 상태 */
	ECBLoginState LoginState = ECBLoginState::NotLoggedIn;

	/** 로그인 성공 시 보관하는 계정 ID. 세션 작업이 이 값을 씀. */
	UE::Online::FAccountId CachedAccountId;
#pragma endregion
};
