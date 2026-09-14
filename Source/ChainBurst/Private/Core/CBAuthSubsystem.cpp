// project
#include "Core/CBAuthSubsystem.h"

// engine
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Online/Auth.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineResult.h"
#include "Online/OnlineServices.h"
#include "IEOSSDKManager.h"
#include "eos_sdk.h"
#include "eos_connect.h"

using namespace UE::Online;

#define LOCTEXT_NAMESPACE "CBAuth"

namespace
{
	// EOS SDK (EOS_Connect_CreateDeviceId) 콜백으로 전달할 컨텍스트 구조체.
	// 콜백에서 Owner 를 통해 서브시스템에 접근해서 로그인 시도.
	struct FCBDeviceIdCallbackContext
	{
		TWeakObjectPtr<UCBAuthSubsystem> Owner;
	};

	// 실패 사유는 한 문구로 통일함. 원인 구분은 로그가 하고, 사용자에게는 조치가 같으므로 나누지 않음
	FText CBGetLoginFailureText()
	{
		return LOCTEXT("LoginFailed", "로그인에 실패했습니다.");
	}
}

#pragma region Login

// [로컬] 서비스 인스턴스 구분 이름 조회
FName UCBAuthSubsystem::ResolveInstanceName() const
{
	const UGameInstance* OwningGameInstance = GetGameInstance();
	if (!OwningGameInstance) return NAME_None;

	// PIE 는 한 프로세스에 게임 인스턴스가 여럿이라, 월드 컨텍스트 이름으로 서비스 인스턴스를 구분해야 함
	const FWorldContext* WorldContext = OwningGameInstance->GetWorldContext();
	
	return WorldContext ? WorldContext->ContextHandle : NAME_None;
}

// [로컬] 온라인 서비스 조회 (인스턴스 식별 규칙의 단일 창구)
IOnlineServicesPtr UCBAuthSubsystem::ResolveServices() const
{
	if (!GetGameInstance()) return nullptr;

	// 제공자는 ini 가 아니라 선택 모드가 정함. 서비스 인스턴스는 제공자별로 따로 캐시되므로
	// 모드를 바꾸면 그쪽 인스턴스가 새로 만들어지고 이전 것과 섞이지 않음
	return GetServices(Local_ResolveServicesProvider(), ResolveInstanceName());
}

// [로컬] EOS P2P 주소 조회
FString UCBAuthSubsystem::GetLocalEOSAddress() const
{
	// LAN 모드의 계정 ID 는 Null 레지스트리 값이라 ProductUserId 가 아님.
	if (SelectedMode != ECBOnlineMode::EOS) return FString();

	// 로그인해야 ProductUserId 가 생김. 그 전에는 광고할 주소가 없음
	if (!CachedAccountId.IsValid()) return FString();

	// 계정 ID 를 문자열로 바꾸면 EOSGS 레지스트리가 ProductUserId 를 그대로 돌려줌.
	// 이 경로는 CoreOnline 에 있어 EOS 모듈에 의존하지 않음
	const FString ProductUserId = ToString(CachedAccountId);
	if (ProductUserId.IsEmpty()) return FString();

	// 대괄호 중요. 없으면 FURL 이 "EOS" 를 프로토콜로 잘라내 주소가 통째로 사라짐
	return FString::Printf(TEXT("[%s%s%s]"), TEXT("EOS"), TEXT(":"), *ProductUserId);
}

// [로컬] 선택 모드에 대응하는 온라인 서비스 제공자
EOnlineServices UCBAuthSubsystem::Local_ResolveServicesProvider() const
{
	return SelectedMode == ECBOnlineMode::LAN ? EOnlineServices::Null : EOnlineServices::Epic;
}

// [로컬] 고른 모드로 로그인 시작, 메인 메뉴의 LAN/EOS 버튼이 호출함.
void UCBAuthSubsystem::RequestLogin(ECBOnlineMode InMode)
{
	// 진행 중이면 무시함. 앞선 요청의 비동기 콜백이 뒤늦게 도착해 새 상태를 덮어쓰기 때문에,
	// 끝날 때까지 기다렸다 다시 누르도록 함. (UI 는 OnLoginStateChanged 로 버튼을 잠금)
	if (LoginState == ECBLoginState::LoggingIn)
	{
		UE_LOG(LogTemp, Log, TEXT("[Auth] 로그인 진행 중이라 모드 변경 요청을 무시함"));
		return;
	}

	// 같은 모드로 이미 로그인했으면 할 일 없음
	if (LoginState == ECBLoginState::LoggedIn && SelectedMode == InMode)
	{
		return;
	}

	// 모드가 바뀌었거나 이전 시도가 실패했으면
	// 로그인 상태 초기화 (ECBLoginState::NotLoggedIn, 계정 ID 비움)
	Local_ResetLogin();
	
	// 요청한 모드로 바꿈. 이후 로그인 절차는 이 모드에 맞춰 진행됨
	SelectedMode = InMode;

	// 로그인 상태를 LoggingIn(로그인중) 으로 바꾸고 방송.
	Local_SetLoginState(ECBLoginState::LoggingIn);

	// Null 은 로그인 절차가 없음. 자동 등록된 계정을 꺼내 쓰고 EOS 경로를 타지 않음
	if (SelectedMode == ECBOnlineMode::LAN)
	{
		Local_AdoptLocalAccount();
		return;
	}

	// Device ID 확보가 EOS 로그인의 전제. 완료되면 Local_LoginWithDeviceId 로 이어짐
	Local_CreateDeviceId();
}

// [로컬][Null 전용] 자동 등록된 계정 조회
void UCBAuthSubsystem::Local_AdoptLocalAccount()
{
	const IOnlineServicesPtr Services = ResolveServices();
	const IAuthPtr Auth = Services ? Services->GetAuthInterface() : nullptr;
	if (!Auth)
	{
		UE_LOG(LogTemp, Error, TEXT("[Auth] 인증 인터페이스를 얻지 못함"));
		Local_SetLoginState(ECBLoginState::Failed, CBGetLoginFailureText());
		return;
	}

	// 로컬 플레이어가 있어야 어느 계정인지 정해짐. 분할 화면은 아직 고려하지 않음
	const UGameInstance* OwningGameInstance = GetGameInstance();
	const ULocalPlayer* LocalPlayer = OwningGameInstance ? OwningGameInstance->GetFirstGamePlayer() : nullptr;
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("[Auth] 로컬 플레이어가 없어 계정을 얻을 수 없음. RequestLogin 호출 시점을 확인할 것"));
		Local_SetLoginState(ECBLoginState::Failed, CBGetLoginFailureText());
		return;
	}

	// Null 은 플랫폼 유저가 생길 때 계정을 등록해 두므로 조회만 하면 됨.
	// Login() 을 부르면 FAuthNull 에 구현이 없어 NotImplemented 로 실패함
	const TOnlineResult<FAuthGetLocalOnlineUserByPlatformUserId> Result =
		Auth->GetLocalOnlineUserByPlatformUserId({ LocalPlayer->GetPlatformUserId() });

	if (!Result.IsOk())
	{
		UE_LOG(LogTemp, Error, TEXT("[Auth] 로컬 계정을 얻지 못함: %s"), *Result.GetErrorValue().GetLogString());
		Local_SetLoginState(ECBLoginState::Failed, CBGetLoginFailureText());
		return;
	}

	// 계정 ID 를 보관. 세션 작업은 이 ID 가 유효해야 함
	CachedAccountId = Result.GetOkValue().AccountInfo->AccountId;

	UE_LOG(LogTemp, Log, TEXT("[Auth] 로그인 성공 (LAN)"));

	// 로그인 상태를 LoggedIn(로그인 완료) 으로 바꾸고 방송
	Local_SetLoginState(ECBLoginState::LoggedIn);
}

// [로컬] 로그인 상태 초기화
void UCBAuthSubsystem::Local_ResetLogin()
{
	// 계정 ID 는 Local_SetLoginState 가 NotLoggedIn 으로 갈 때 함께 비움
	Local_SetLoginState(ECBLoginState::NotLoggedIn);
}

// [로컬][EOS 전용] 기기 익명 계정 생성 (DeviceId)
void UCBAuthSubsystem::Local_CreateDeviceId()
{
	// 서비스를 먼저 확보해 온라인 서비스 쪽이 EOS 플랫폼을 만들게 함.
	// SDK 매니저의 플랫폼 캐시는 약참조라, 강참조를 드는 쪽이 없으면 플랫폼이 바로 파괴됨.
	// 이 순서를 지키지 않으면 아래에서 만든 플랫폼을 지역 변수만 붙들게 되고,
	// 함수가 반환되는 순간 파괴돼 CreateDeviceId 콜백이 오지 않음
	const IOnlineServicesPtr Services = ResolveServices();
	if (!Services)
	{
		UE_LOG(LogTemp, Error, TEXT("[Auth] 온라인 서비스를 찾을 수 없음. [OnlineServices] DefaultServices 설정과 플러그인 활성화를 확인할 것"));
		Local_SetLoginState(ECBLoginState::Failed, CBGetLoginFailureText());
		return;
	}

	// EOS SDK 매니저를 통해 플랫폼을 얻어야 Device ID 를 만들 수 있음
	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	if (!SDKManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[Auth] EOS SDK 매니저가 없음. EOSShared 플러그인을 확인할 것"));
		Local_SetLoginState(ECBLoginState::Failed, CBGetLoginFailureText());
		return;
	}

	// 플랫폼 설정 이름은 ini 의 [EOSSDK] DefaultPlatformConfigName 이 정함.
	// 코드에 이름을 적어두면 ini 와 이중 관리가 되므로 SDK 매니저가 해석한 값을 그대로 씀
	const FString& PlatformConfigName = SDKManager->GetDefaultPlatformConfigName();
	if (PlatformConfigName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[Auth] 플랫폼 설정 이름이 비어 있음. ini 의 [EOSSDK] DefaultPlatformConfigName 을 확인할 것"));
		Local_SetLoginState(ECBLoginState::Failed, CBGetLoginFailureText());
		return;
	}

	// 같은 (설정 이름 × 인스턴스 이름) 이면 SDK 매니저가 기존 핸들을 돌려주므로,
	// 위에서 온라인 서비스가 만든 것과 같은 플랫폼을 얻게 됨
	const IEOSPlatformHandlePtr Platform = SDKManager->CreatePlatform(PlatformConfigName, ResolveInstanceName());
	if (!Platform.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[Auth] EOS 플랫폼을 얻지 못함. ini 의 [EOSSDK.Platform.%s] 자격증명을 확인할 것"), *PlatformConfigName);
		Local_SetLoginState(ECBLoginState::Failed, CBGetLoginFailureText());
		return;
	}

	// EOS Connect 인터페이스를 얻어야 Device ID 를 만들 수 있음
	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(*Platform);
	if (!ConnectHandle)
	{
		UE_LOG(LogTemp, Error, TEXT("[Auth] EOS Connect 인터페이스를 얻지 못함"));
		Local_SetLoginState(ECBLoginState::Failed, CBGetLoginFailureText());
		return;
	}

	// Device ID 생성 옵션.
	EOS_Connect_CreateDeviceIdOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
	// 표시용 문자열. EOS 가 기기를 구분하는 데 참고만 하며 식별자는 아님
	Options.DeviceModel = "PC";

	UE_LOG(LogTemp, Log, TEXT("[Auth] Device ID 생성 요청"));

	// 서브시스템을 담은 컨텍스트. (new 로 만들었으니 반드시 콜백에서 delete 해 회수해야 함)
	FCBDeviceIdCallbackContext* CallbackContext = new FCBDeviceIdCallbackContext{ this };
	
	// EOS SDK 호출. Device ID 생성 완료 시 콜백이 호출됨
	// CallbackContext 를 콜백에서 InData->ClientData 로 전달됨.
	EOS_Connect_CreateDeviceId(ConnectHandle, &Options, CallbackContext,
		[](const EOS_Connect_CreateDeviceIdCallbackInfo* InData)
		{
			// 콜백함수에서 생성된 Device ID를 담아 주는 필드가 없음. SDK가 만들어서 로컬 기기에 저장해 두므로, 다음 로그인 시 SDK가 알아서 읽어 씀.
			
			// InData->ClientData 는 위에서 new 한 FCBDeviceIdCallbackContext* (CallbackContext 에게 전달받음)
			// UniquePtr 을 통해 동적 생성된 컨텍스트를 스코프 끝나면 자동 회수하도록 함. (RAII)
			// (결과에 무관하게 SDK 가 콜백을 반드시 한 번 호출함)
			const TUniquePtr<FCBDeviceIdCallbackContext> Context(static_cast<FCBDeviceIdCallbackContext*>(InData->ClientData));

			// 요청과 콜백 사이에 서브시스템이 사라졌는지 확인. (PIE 정지 등) 
			UCBAuthSubsystem* AuthSubsystem = Context.IsValid() ? Context->Owner.Get() : nullptr;
			if (!AuthSubsystem) return;

			// 이미 만들어 둔 기기 계정이 있으면 DuplicateNotAllowed 가 옴.
			// 생성에 성공했거나 이미 존재하면 로그인으로 넘어감. 실패면 로그인 시도하지 않고 실패 상태로 바꿈
			const bool bDeviceIdReady =
				InData->ResultCode == EOS_EResult::EOS_Success ||
				InData->ResultCode == EOS_EResult::EOS_DuplicateNotAllowed;

			// Device ID 생성 실패 시 로그인 시도하지 않고 실패 상태로 바꿈
			if (!bDeviceIdReady)
			{
				UE_LOG(LogTemp, Error, TEXT("[Auth] Device ID 생성 실패 (코드 %d)"), static_cast<int32>(InData->ResultCode));
				AuthSubsystem->Local_SetLoginState(ECBLoginState::Failed, CBGetLoginFailureText());
				return;
			}

			// Device ID 자격증명으로 로그인 시도
			AuthSubsystem->Local_LoginWithDeviceId();
		});
}

// [로컬] Device ID 자격증명으로 로그인 요청
void UCBAuthSubsystem::Local_LoginWithDeviceId()
{
	const IOnlineServicesPtr Services = ResolveServices();
	const IAuthPtr Auth = Services ? Services->GetAuthInterface() : nullptr;
	if (!Auth)
	{
		UE_LOG(LogTemp, Error, TEXT("[Auth] 인증 인터페이스를 얻지 못함"));
		Local_SetLoginState(ECBLoginState::Failed, CBGetLoginFailureText());
		return;
	}

	// 로컬 플레이어가 있어야 계정을 붙일 대상이 정해짐. (분할 화면은 아직 고려하지 않음)
	const UGameInstance* OwningGameInstance = GetGameInstance();
	const ULocalPlayer* LocalPlayer = OwningGameInstance ? OwningGameInstance->GetFirstGamePlayer() : nullptr;
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("[Auth] 로컬 플레이어가 없어 로그인할 수 없음. RequestLogin 호출 시점을 확인할 것"));
		Local_SetLoginState(ECBLoginState::Failed, CBGetLoginFailureText());
		return;
	}

	// Device ID 방식은 넘길 토큰 문자열이 없음.
	// 종류만 지정하면 SDK 가 기기에 저장된 자격증명을 씀
	FExternalAuthToken ExternalToken;
	ExternalToken.Type = ExternalLoginType::DeviceIdAccessToken; // EOS SDK 가 기기에 저장한 Device ID 를 읽어 씀

	FAuthLogin::Params Params;
	Params.PlatformUserId = LocalPlayer->GetPlatformUserId();
	// Epic 계정을 쓰지 않고 기기 익명 계정으로 붙으므로 외부 인증 경로로 로그인함
	Params.CredentialsType = LoginCredentialsType::ExternalAuth;
	Params.CredentialsToken.Set<FExternalAuthToken>(MoveTemp(ExternalToken));

	UE_LOG(LogTemp, Log, TEXT("[Auth] 로그인 요청 (Device ID)"));

	Auth->Login(MoveTemp(Params))
		.OnComplete(this, &UCBAuthSubsystem::Local_HandleLoginComplete);
}

// [로컬] 로그인 완료 콜백
void UCBAuthSubsystem::Local_HandleLoginComplete(const TOnlineResult<FAuthLogin>& InResult)
{
	if (!InResult.IsOk())
	{
		UE_LOG(LogTemp, Error, TEXT("[Auth] 로그인 실패: %s"), *InResult.GetErrorValue().GetLogString());
		Local_SetLoginState(ECBLoginState::Failed, CBGetLoginFailureText());
		return;
	}

	// 계정 ID 를 보관함. 이후 세션 작업은 전부 이 값을 씀
	CachedAccountId = InResult.GetOkValue().AccountInfo->AccountId;

	UE_LOG(LogTemp, Log, TEXT("[Auth] 로그인 성공"));

	Local_SetLoginState(ECBLoginState::LoggedIn);
}

// [로컬] 상태 변경 및 방송
void UCBAuthSubsystem::Local_SetLoginState(ECBLoginState InNewState, const FText& InFailureReason /* = FText::GetEmpty() */)
{
	// 같은 상태로의 재진입은 구독자에게 의미가 없으므로 걸러냄
	if (LoginState == InNewState) return;

	LoginState = InNewState;

	// 로그인이 실패하거나 로그아웃 상태면 계정 ID 도 비워, 뒤늦게 무효한 ID 가 쓰이지 않게 함
	if (InNewState == ECBLoginState::Failed || InNewState == ECBLoginState::NotLoggedIn)
	{
		CachedAccountId = FAccountId();
	}

	// 로그인 상태 변화 방송.
	OnLoginStateChanged.Broadcast(InNewState, InFailureReason);
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
