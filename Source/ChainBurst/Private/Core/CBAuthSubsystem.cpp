// project
#include "Core/CBAuthSubsystem.h"

// engine
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Online/Auth.h"
#include "Online/OnlineAsyncOpHandle.h"
#include "Online/OnlineServices.h"
#include "Online/OnlineUtilsCommon.h"

// EOS SDK — Device ID 생성 경로가 OSSv2 에 없어 직접 호출함.
// 프로젝트에서 EOS 전용 코드는 이 파일에만 존재해야 함
#include "IEOSSDKManager.h"
#include "eos_connect.h"
#include <eos_sdk.h>

using namespace UE::Online;

#define LOCTEXT_NAMESPACE "CBAuth"

#pragma region Core

// 서브시스템 생성 시 호출됨.
// 로그인은 여기서 시작하지 않음 — 이 시점엔 로컬 플레이어가 아직 없어 계정 ID 를 만들 수 없음.
// 실제 시작은 UCBGameInstance::OnStart 가 RequestLogin() 을 부르는 것으로 이뤄짐
void UCBAuthSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

// 서브시스템 해제 시 호출됨
void UCBAuthSubsystem::Deinitialize()
{
	// 계정 ID 는 값 타입이라 서비스가 내려간 뒤에도 보관 자체는 무해함.
	// 세션 서브시스템이 종료 중 세션 이탈에 이 값을 쓰므로 비우지 않는다
	Super::Deinitialize();
}

#pragma endregion

#pragma region Login

// [로컬] 온라인 서비스 조회 (인스턴스 식별 규칙의 단일 창구)
IOnlineServicesPtr UCBAuthSubsystem::ResolveServices() const
{
	const UGameInstance* OwningGameInstance = GetGameInstance();
	if (!OwningGameInstance) return nullptr;

	// PIE 는 한 프로세스에 게임 인스턴스가 여럿이라, 월드 컨텍스트 이름으로 서비스 인스턴스를 구분함
	const FWorldContext* WorldContext = OwningGameInstance->GetWorldContext();
	const FName InstanceName = WorldContext ? WorldContext->ContextHandle : NAME_None;

	// 제공자(Null/EOS)는 ini 의 [OnlineServices] DefaultServices 가 정함
	return GetServices(EOnlineServices::Default, InstanceName);
}

// [로컬] 로그인 시작
void UCBAuthSubsystem::RequestLogin()
{
	// 중복 요청 차단. 진행 중이거나 이미 끝났으면 할 일 없음
	if (LoginState == ECBLoginState::LoggingIn || LoginState == ECBLoginState::LoggedIn)
	{
		return;
	}

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
		Local_SetLoginState(ECBLoginState::Failed, LOCTEXT("NoServices", "온라인 서비스를 사용할 수 없습니다."));
		return;
	}

	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	if (!SDKManager)
	{
		Local_SetLoginState(ECBLoginState::Failed, LOCTEXT("NoSDK", "온라인 서비스를 사용할 수 없습니다."));
		return;
	}

	const UGameInstance* OwningGameInstance = GetGameInstance();
	const FWorldContext* WorldContext = OwningGameInstance ? OwningGameInstance->GetWorldContext() : nullptr;
	const FName InstanceName = WorldContext ? WorldContext->ContextHandle : NAME_None;

	// ini 의 [EOSSDK.Platform.<PlatformConfigName>] 을 이름으로 조회함
	const IEOSPlatformHandlePtr Platform = SDKManager->CreatePlatform(PlatformConfigName, InstanceName);
	if (!Platform.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[Auth] EOS 플랫폼을 얻지 못함. ini 의 [EOSSDK.Platform.%s] 을 확인할 것"), *PlatformConfigName);
		Local_SetLoginState(ECBLoginState::Failed, LOCTEXT("NoPlatform", "온라인 서비스를 사용할 수 없습니다."));
		return;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(*Platform);
	if (!ConnectHandle)
	{
		Local_SetLoginState(ECBLoginState::Failed, LOCTEXT("NoConnect", "온라인 서비스를 사용할 수 없습니다."));
		return;
	}

	EOS_Connect_CreateDeviceIdOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
	// 표시용 문자열. EOS 가 기기를 구분하는 데 참고만 하며 식별자는 아님
	Options.DeviceModel = "PC";

	UE_LOG(LogTemp, Log, TEXT("[Auth] Device ID 생성 요청"));

	EOS_Connect_CreateDeviceId(ConnectHandle, &Options, this,
		[](const EOS_Connect_CreateDeviceIdCallbackInfo* InData)
		{
			UCBAuthSubsystem* Self = static_cast<UCBAuthSubsystem*>(InData->ClientData);
			if (!Self) return;

			// 이미 만들어 둔 기기 계정이 있으면 DuplicateNotAllowed 가 옴. 재사용하면 되므로 성공으로 봄
			const bool bReady =
				InData->ResultCode == EOS_EResult::EOS_Success ||
				InData->ResultCode == EOS_EResult::EOS_DuplicateNotAllowed;

			if (!bReady)
			{
				UE_LOG(LogTemp, Error, TEXT("[Auth] Device ID 생성 실패 (코드 %d)"), static_cast<int32>(InData->ResultCode));
				Self->Local_SetLoginState(ECBLoginState::Failed, LOCTEXT("DeviceIdFailed", "로그인에 실패했습니다."));
				return;
			}

			Self->Local_LoginWithDeviceId();
		});
}

// [로컬] Device ID 자격증명으로 로그인 요청
void UCBAuthSubsystem::Local_LoginWithDeviceId()
{
	const IOnlineServicesPtr Services = ResolveServices();
	const IAuthPtr Auth = Services ? Services->GetAuthInterface() : nullptr;
	if (!Auth)
	{
		Local_SetLoginState(ECBLoginState::Failed, LOCTEXT("NoAuth", "온라인 서비스를 사용할 수 없습니다."));
		return;
	}

	// 로컬 플레이어가 있어야 계정을 붙일 대상이 정해짐. 분할 화면은 아직 고려하지 않음
	const UGameInstance* OwningGameInstance = GetGameInstance();
	const ULocalPlayer* LocalPlayer = OwningGameInstance ? OwningGameInstance->GetFirstGamePlayer() : nullptr;
	if (!LocalPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("[Auth] 로컬 플레이어가 없어 로그인할 수 없음. RequestLogin 호출 시점을 확인할 것"));
		Local_SetLoginState(ECBLoginState::Failed, LOCTEXT("NoLocalPlayer", "로그인에 실패했습니다."));
		return;
	}

	// Device ID 방식은 토큰 문자열이 없음. 종류만 지정하면 SDK 가 기기에 저장된 자격증명을 씀
	FExternalAuthToken ExternalToken;
	ExternalToken.Type = ExternalLoginType::DeviceIdAccessToken;
	ExternalToken.Data = FString();

	FAuthLogin::Params Params;
	Params.PlatformUserId = LocalPlayer->GetPlatformUserId();
	// Epic 계정(EAS)을 쓰지 않으므로 외부 인증 경로로 로그인함.
	// ini 의 EASAuthEnabled 가 켜져 있으면 엔진이 이 경로 대신 Epic 계정 로그인을 시도하므로 꺼 둘 것
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
		Local_SetLoginState(ECBLoginState::Failed, LOCTEXT("LoginFailed", "로그인에 실패했습니다."));
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

	// 실패로 끝났으면 계정 ID 도 비워, 뒤늦게 무효한 ID 가 쓰이지 않게 함
	if (InNewState == ECBLoginState::Failed || InNewState == ECBLoginState::NotLoggedIn)
	{
		CachedAccountId = FAccountId();
	}

	OnLoginStateChanged.Broadcast(InNewState, InFailureReason);
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
