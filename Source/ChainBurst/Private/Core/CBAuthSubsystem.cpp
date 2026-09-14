// project
#include "Core/CBAuthSubsystem.h"

// engine
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Online/Auth.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineResult.h"
#include "Online/OnlineServices.h"

// EOS SDK — Device ID 생성 경로를 OSSv2 가 감싸주지 않아 직접 호출함.
// 프로젝트에서 EOS 전용 코드는 이 파일에만 존재해야 함.
// eos_sdk.h 는 EOS_Platform_GetConnectInterface 용, eos_connect.h 는 Device ID 옵션·콜백 타입용
#include "IEOSSDKManager.h"
#include "eos_sdk.h"
#include "eos_connect.h"

using namespace UE::Online;

#define LOCTEXT_NAMESPACE "CBAuth"

namespace
{
	// EOS SDK 콜백은 void* 하나만 실어 나르므로, 서브시스템 포인터를 그대로 넘기면
	// 콜백이 오기 전에 서브시스템이 사라졌을 때(PIE 정지 등) 댕글링이 됨.
	// 약참조를 힙에 담아 넘기고 콜백이 회수하는 방식으로 그 구간을 막음
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

	// 제공자(Null/EOS)는 ini 의 [OnlineServices] DefaultServices 가 정함
	return GetServices(EOnlineServices::Default, ResolveInstanceName());
}

// [로컬] 로그인 시작, 게임 시작 시 게임 인스턴스에서 호출됨.
void UCBAuthSubsystem::RequestLogin()
{
	// 중복 요청 차단. 진행 중이거나 이미 끝났으면 할 일 없음
	if (LoginState == ECBLoginState::LoggingIn || LoginState == ECBLoginState::LoggedIn)
	{
		return;
	}

	// 로그인 진행 중으로 상태를 바꿈.
	Local_SetLoginState(ECBLoginState::LoggingIn);

	// Device ID 확보가 로그인의 전제. 완료되면 Local_LoginWithDeviceId 로 이어짐
	Local_CreateDeviceId();
}

// [로컬][EOS 전용] 기기 익명 계정 생성
void UCBAuthSubsystem::Local_CreateDeviceId()
{
	// 서비스를 먼저 확보해 온라인 서비스 쪽이 EOS 플랫폼을 만들게 함.
	// 이 순서를 지키지 않으면 아래 CreatePlatform 이 별개의 플랫폼을 하나 더 만들어,
	// 로그인한 플랫폼과 세션이 쓰는 플랫폼이 어긋남
	const IOnlineServicesPtr Services = ResolveServices();
	if (!Services)
	{
		UE_LOG(LogTemp, Error, TEXT("[Auth] 온라인 서비스를 찾을 수 없음. [OnlineServices] DefaultServices 설정과 플러그인 활성화를 확인할 것"));
		Local_SetLoginState(ECBLoginState::Failed, CBGetLoginFailureText());
		return;
	}

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

	// 콜백이 회수할 약참조 컨텍스트. 캡처가 없어야 C 함수 포인터로 넘어가므로 ClientData 로 전달함
	FCBDeviceIdCallbackContext* CallbackContext = new FCBDeviceIdCallbackContext{ this };

	EOS_Connect_CreateDeviceId(ConnectHandle, &Options, CallbackContext,
		[](const EOS_Connect_CreateDeviceIdCallbackInfo* InData)
		{
			// 넘겨준 컨텍스트를 여기서 회수함 (SDK 가 콜백을 반드시 한 번 호출함)
			const TUniquePtr<FCBDeviceIdCallbackContext> Context(static_cast<FCBDeviceIdCallbackContext*>(InData->ClientData));

			// 요청과 콜백 사이에 서브시스템이 사라졌으면(PIE 정지 등) 여기서 멈춤
			UCBAuthSubsystem* AuthSubsystem = Context.IsValid() ? Context->Owner.Get() : nullptr;
			if (!AuthSubsystem) return;

			// 이미 만들어 둔 기기 계정이 있으면 DuplicateNotAllowed 가 옴. 재사용하면 되므로 성공으로 봄
			const bool bDeviceIdReady =
				InData->ResultCode == EOS_EResult::EOS_Success ||
				InData->ResultCode == EOS_EResult::EOS_DuplicateNotAllowed;

			if (!bDeviceIdReady)
			{
				UE_LOG(LogTemp, Error, TEXT("[Auth] Device ID 생성 실패 (코드 %d)"), static_cast<int32>(InData->ResultCode));
				AuthSubsystem->Local_SetLoginState(ECBLoginState::Failed, CBGetLoginFailureText());
				return;
			}

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

	// 로컬 플레이어가 있어야 계정을 붙일 대상이 정해짐. 분할 화면은 아직 고려하지 않음
	const UGameInstance* OwningGameInstance = GetGameInstance();
	const ULocalPlayer* LocalPlayer = OwningGameInstance ? OwningGameInstance->GetFirstGamePlayer() : nullptr;
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("[Auth] 로컬 플레이어가 없어 로그인할 수 없음. RequestLogin 호출 시점을 확인할 것"));
		Local_SetLoginState(ECBLoginState::Failed, CBGetLoginFailureText());
		return;
	}

	// Device ID 방식은 넘길 토큰 문자열이 없음. 종류만 지정하면 SDK 가 기기에 저장된 자격증명을 씀
	FExternalAuthToken ExternalToken;
	ExternalToken.Type = ExternalLoginType::DeviceIdAccessToken;

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
