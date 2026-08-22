// project
#include "GameModes/CBGameModeBase.h"
#include "Controllers/CBChaserController.h"
#include "Core/CBGameInstance.h"
#include "DataAssets/Character/CBCharacterCatalog.h"
#include "PlayerState/CBPlayerState.h"

// engine
#include "GameFramework/Controller.h"

ACBGameModeBase::ACBGameModeBase()
{
	// 맵을 넘어갈 때 PlayerState 이관 경로(CopyProperties)를 타려면 필수.
	// 꺼져 있으면 로비에서 고른 값이 에러 없이 조용히 사라짐.
	bUseSeamlessTravel = true;

	// BP 마다 지정하면 누락 위험이 있으므로 베이스에서 고정함
	PlayerControllerClass = ACBChaserController::StaticClass();
	PlayerStateClass = ACBPlayerState::StaticClass();
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
