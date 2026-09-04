// project
#include "GameModes/CBGameModeBase.h"
#include "Controllers/CBChaserController.h"
#include "GameStates/CBGameStateBase.h"
#include "Core/CBGameInstance.h"
#include "DataAssets/Character/CBCharacterCatalog.h"
#include "PlayerState/CBPlayerState.h"

// engine
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

ACBGameModeBase::ACBGameModeBase()
{
	// 맵을 넘어갈 때 PlayerState 이관 경로(CopyProperties)를 타려면 필수.
	// 꺼져 있으면 로비에서 고른 값이 에러 없이 조용히 사라짐.
	bUseSeamlessTravel = true;

	// BP 마다 지정하면 누락 위험이 있으므로 베이스에서 고정함
	PlayerControllerClass = ACBChaserController::StaticClass();
	PlayerStateClass = ACBPlayerState::StaticClass();

	// 기본 게임 스테이트는 ACBGameStateBase 로 설정 (로비는 자기 것으로 다시 덮음)
	GameStateClass = ACBGameStateBase::StaticClass();

	// 기본 닉네임의 앞부분. BP 에서 덮어쓸 수 있음.
	DefaultPlayerName = NSLOCTEXT("ChainBurst", "DefaultPlayerName", "Player");
}

// [서버] 플레이어가 접속할 때 호출. 엔진 기본 처리를 마친 뒤 닉네임만 우리 규칙으로 다시 정함
FString ACBGameModeBase::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	// 세션 등록·스폰 지점 배정·이름 부여까지 엔진 기본 처리를 먼저 수행
	const FString ErrorMessage = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	// 플레이어 스테이트 가져오기.
	APlayerState* NewPlayerState = NewPlayerController ? NewPlayerController->PlayerState : nullptr;
	if (!NewPlayerState) return ErrorMessage;

	// 접속 URL 이 이름을 실어 보냈으면(?Name=...) 그 값을 존중하고 중복만 처리.
	// 옵션 이름 "Name" 은 엔진이 InitNewPlayer 에서 읽는 키라 바꿀 수 없음.
	const FString RequestedName = UGameplayStatics::ParseOption(Options, TEXT("Name"));

	// 옵션이 없거나 공백뿐이면 들어온 순서대로 기본 닉네임을 부여
	FString Nickname;
	
	// 닉네임 공백 및 제어문자 제거 및 길이 제한, 중복 해소까지 Auth_SanitizeNickname 에서 처리
	// true : 규칙에 맞게 수정됨, false : 규칙에 맞는 닉네임을 만들지 못함(공백뿐인 입력 등)
	if (!Auth_SanitizeNickname(RequestedName, NewPlayerState, Nickname))
	{
		// 규칙에 맞는 닉네임을 만들지 못한 경우, 기본 닉네임을 사용
		Nickname = Auth_MakeDefaultNickname(NewPlayerState);
	}

	// 엔진 경로로 닉네임 반영 (SetPlayerName + K2_OnChangeName 훅)
	ChangeName(NewPlayerController, Nickname, false);

	return ErrorMessage;
}

// [서버] 스폰할 폰 클래스 결정. 로비 재스폰·게임플레이 첫 스폰 모두 이 경로를 지남
UClass* ACBGameModeBase::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// 플레이어 스테이트 가져오기 (플레이어가 고른 캐릭터를 확인하기 위함)
	const ACBPlayerState* CBPlayerState = InController ? InController->GetPlayerState<ACBPlayerState>() : nullptr;
	
	// 플레이어가 고른 캐릭터 태그 가져오기
	const FGameplayTag SelectedCharacterId = CBPlayerState ? CBPlayerState->GetSelectedCharacterId() : FGameplayTag();

	// 플레이어가 고른 캐릭터가 있으면 카탈로그에서 클래스를 로드하고, 없으면 기본 폰 클래스로 이어감
	if (SelectedCharacterId.IsValid())
	{
		// 캐릭터 카탈로그 가져오기
		const UCBCharacterCatalog* CharacterCatalog = GetCharacterCatalog();

		// 카탈로그에서 고른 캐릭터의 클래스를 로드
		if (UClass* SelectedPawnClass = CharacterCatalog ? CharacterCatalog->LoadCharacterClass(SelectedCharacterId) : nullptr)
		{
			return SelectedPawnClass;
		}

		// 고른 값이 카탈로그에서 사라졌거나 클래스 로드에 실패한 경우.
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] 고른 캐릭터의 클래스를 얻지 못해 기본 폰으로 스폰함: %s"),
			*SelectedCharacterId.ToString());
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

// [서버] 클라이언트가 보낸 캐릭터 변경 요청 검증 (ACBChaserController::Server_RequestCharacterSelection 에서 호출됨)
bool ACBGameModeBase::Auth_IsValidCharacterId(const FGameplayTag& InCharacterId) const
{
	const UCBCharacterCatalog* CharacterCatalog = GetCharacterCatalog();

	// 카탈로그가 없으면 고를 수 있는 캐릭터도 없음
	return CharacterCatalog && CharacterCatalog->IsValidCharacterId(InCharacterId);
}

// [서버] 캐릭터 카탈로그 조회
UCBCharacterCatalog* ACBGameModeBase::GetCharacterCatalog() const
{
	const UCBGameInstance* CBGameInstance = GetGameInstance<UCBGameInstance>();
	return CBGameInstance ? CBGameInstance->GetCharacterCatalog() : nullptr;
}

// [서버] 컨트롤러가 닉네임 변경 요청을 받으면 호출 (ACBChaserController::Server_RequestSetNickname 에서 호출됨)
bool ACBGameModeBase::Auth_SanitizeNickname(const FString& InRaw, const APlayerState* InIgnorePlayerState, FString& OutNickname) const
{
	FString Cleaned;
	Cleaned.Reserve(InRaw.Len()); // 공간 예약.

	// 직전 글자가 공백이었는지. 연속 공백을 한 칸으로 줄이기 위함
	bool bPrevWasSpace = false;

	for (const TCHAR Char : InRaw)
	{
		// 줄바꿈·탭 같은 제어문자는 버림.
		if (FChar::IsControl(Char)) continue;

		// 공백 여부 검사. FChar::IsWhitespace 는 일반 스페이스뿐 아니라 줄바꿈·탭·넓은 공백 등도 true 를 반환함.
		const bool bIsSpace = FChar::IsWhitespace(Char);

		// 연속 공백은 한 칸으로 줄임
		if (bIsSpace && bPrevWasSpace) continue;

		// 어떤 종류의 공백이든 일반 스페이스로 통일 (보이지 않는 특수 공백으로 같은 이름을 만드는 것을 막음)
		Cleaned.AppendChar(bIsSpace ? TEXT(' ') : Char);
		bPrevWasSpace = bIsSpace;
	}

	// 앞뒤 공백 제거
	Cleaned.TrimStartAndEndInline();

	// 길이 제한. 자른 자리에 공백이 남을 수 있어 한 번 더 다듬음
	if (Cleaned.Len() > MaxNicknameLength)
	{
		Cleaned.LeftInline(MaxNicknameLength);
		Cleaned.TrimEndInline();
	}

	// 남는 글자가 없으면 거절. 호출한 쪽이 지금 이름을 그대로 두게 함
	if (Cleaned.IsEmpty()) return false;

	// 중복이면 뒤에 번호를 붙여 유일하게 만듦
	OutNickname = Auth_MakeUniqueNickname(Cleaned, InIgnorePlayerState);

	return true;
}

// [서버] 비어 있는 가장 작은 번호로 기본 닉네임 생성
FString ACBGameModeBase::Auth_MakeDefaultNickname(const APlayerState* InIgnorePlayerState) const
{
	// 남이 쥔 번호는 많아야 접속 인원 수만큼이므로, 하나 더 큰 번호까지 훑으면 반드시 빈 번호가 나옴
	const int32 MaxNumber = GameState ? GameState->PlayerArray.Num() + 1 : 1;

	// 1번부터 훑어 비어 있는 가장 작은 번호를 씀.
	// 단순 증가 카운터로 매기면 누가 나갔다 들어올 때 3인 로비에 Player5 같은 번호가 생김.
	for (int32 Number = 1; Number <= MaxNumber; ++Number)
	{
		// 기본 닉네임 + 번호 접미사 후보
		FString Candidate = FString::Printf(TEXT("%s%d"), *DefaultPlayerName.ToString(), Number);

		if (!Auth_IsNicknameTaken(Candidate, InIgnorePlayerState)) return Candidate;
	}

	// 모든 번호가 차 있으면 마지막 번호를 씀. (이론상 여기까지 오지 않음)
	return FString::Printf(TEXT("%s%d"), *DefaultPlayerName.ToString(), MaxNumber);
}

// [서버] 중복 닉네임에 번호 접미사를 붙여 유일하게 만듦
FString ACBGameModeBase::Auth_MakeUniqueNickname(const FString& InBase, const APlayerState* InIgnorePlayerState) const
{
	// 겹치지 않으면 그대로 씀
	if (!Auth_IsNicknameTaken(InBase, InIgnorePlayerState)) return InBase;
	
	// 붙일 수 있는 최대 접미사 번호.
	const int32 LastSuffix = (GameState ? GameState->PlayerArray.Num() : 0) + 1;

	// 2번부터 붙여 봄 (이름 → 이름2 → 이름3 ...)
	for (int32 Suffix = 2; Suffix <= LastSuffix; ++Suffix)
	{
		const FString SuffixText = FString::FromInt(Suffix);

		// 접미사까지 포함해 길이 제한에 들어가도록 앞부분을 잘라냄. 자른 꼬리의 공백도 제거
		FString Candidate = InBase.Left(FMath::Max(1, MaxNicknameLength - SuffixText.Len()));
		Candidate.TrimEndInline();
		Candidate += SuffixText;

		// 겹치지 않으면 그 후보를 씀
		if (!Auth_IsNicknameTaken(Candidate, InIgnorePlayerState)) return Candidate;
	}

	// 여기까지 오는 경우는 길이 제한 때문에 잘린 후보들이 서로 겹쳤을 때뿐임 (숫자로 끝나는 긴 이름).
	// 그때는 엔진이 유일하게 보장하는 PlayerId 를 붙여 확실히 구분함
	if (InIgnorePlayerState)
	{
		// PlayerId는 엔진에서 접속 순서대로 0부터 부여하는 정수. 접속한 다른 플레이어와 겹칠 수 없으므로 유일함.
		const FString IdText = FString::FromInt(InIgnorePlayerState->GetPlayerId());

		// 접미사와 같은 방식으로 앞부분을 잘라 길이 제한을 지킴
		FString Candidate = InBase.Left(FMath::Max(1, MaxNicknameLength - IdText.Len()));
		Candidate.TrimEndInline();
		Candidate += IdText;

		// 남이 우연히 같은 이름을 쓰고 있지 않을 때만 채택 (PlayerId 를 붙여도 남의 이름과 같을 수는 있음)
		if (!Auth_IsNicknameTaken(Candidate, InIgnorePlayerState)) return Candidate;
	}

	// 붙일 PlayerId 조차 없는 경우. 더 나은 후보가 없으므로 원본을 그대로 돌려줌
	return InBase;
}

// [서버] 접속한 다른 플레이어가 쓰는 이름인지 검사
bool ACBGameModeBase::Auth_IsNicknameTaken(const FString& InNickname, const APlayerState* InIgnorePlayerState) const
{
	if (!GameState) return false;

	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		// 요청자 자신은 검사 대상이 아님 (지금 자기 이름과 부딪혀 계속 번호가 붙는 것을 막음)
		if (!PlayerState || PlayerState == InIgnorePlayerState) continue;

		// 대소문자만 다른 이름도 같은 이름으로 봄 (목록에서 서로 구분되지 않음)
		if (PlayerState->GetPlayerName().Equals(InNickname, ESearchCase::IgnoreCase)) return true;
	}

	return false;
}
