#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagContainer.h"
#include "CBGameModeBase.generated.h"

class UCBCharacterCatalog;

/**
 * 프로젝트 공용 게임모드 베이스.
 * 파생 게임모드가 공통으로 쓸 컨트롤러·플레이어 상태 클래스와 맵 전환 방식을 고정하고,
 * 플레이어가 고른 캐릭터(무기)로 스폰하도록 기본 폰 클래스를 결정함.
 * 접속한 플레이어에게 들어온 순서대로 기본 닉네임(Player1, Player2 ...)을 부여하고, 닉네임 변경 요청을 다듬어 중복을 막음.
 * 레벨별 규칙은 파생 클래스(로비·게임플레이)가 담당함.
 */
UCLASS()
class CHAINBURST_API ACBGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** 파생 게임모드가 공통으로 물려받을 클래스·맵 전환 기본값을 설정함 */
	ACBGameModeBase();

	/**
	 * [서버] 그 캐릭터로 스폰할 수 있는지 카탈로그로 검증하는 함수. (서버에서만 실행)
	 * 클라이언트가 보낸 캐릭터 변경 요청을 서버가 검증하는 지점.
	 * @param InCharacterId 검증할 캐릭터 태그
	 */
	bool Auth_IsValidCharacterId(const FGameplayTag& InCharacterId) const;

	/**
	 * [서버] 닉네임을 다듬고 중복이면 뒤에 번호를 붙여 유일하게 만드는 함수. (서버에서만 실행)
	 * 클라이언트가 보낸 닉네임 변경 요청을 서버가 검증하는 지점.
	 * @param InRaw 클라이언트가 보낸 원본 문자열
	 * @param InIgnorePlayerState 중복 검사에서 제외할 PlayerState (요청자 자신)
	 * @param OutNickname 다듬은 결과 (거절되면 건드리지 않음)
	 * @return 쓸 수 있는 닉네임을 만들었는지 여부 (false = 요청 무시)
	 */
	bool Auth_SanitizeNickname(const FString& InRaw, const APlayerState* InIgnorePlayerState, FString& OutNickname) const;

protected:
	//~ Begin AGameModeBase Interface.
	/** 
	 * [서버] 스폰할 폰 클래스를 결정함. PlayerState 에 고른 캐릭터가 있으면 그 클래스로, 없으면 게임모드 BP 의 기본값으로 스폰.
	 * 로비 재스폰·게임플레이 첫 스폰 모두 이 경로를 지남
	 */
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	/** [서버] 접속한 플레이어를 초기화함. 접속 옵션의 이름(?Name=)은 쓰지 않고 항상 기본 닉네임(Player1, Player2 ...)을 부여. */
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) override;
	//~ End AGameModeBase Interface.

	/** [서버] 고를 수 있는 캐릭터 카탈로그를 게임 인스턴스에서 가져옴. 등록하지 않았으면 nullptr. */
	UCBCharacterCatalog* GetCharacterCatalog() const;

	/**
	 * [서버] 아무도 쓰지 않는 가장 작은 번호로 기본 닉네임을 만드는 함수. (Player1, Player2 ...)
	 * @param InIgnorePlayerState 중복 검사에서 제외할 PlayerState (이름을 정해 줄 당사자)
	 */
	FString Auth_MakeDefaultNickname(const APlayerState* InIgnorePlayerState) const;

	/**
	 * [서버] 이미 쓰이는 닉네임이면 뒤에 번호를 붙여 유일하게 만드는 함수. (이름2, 이름3 ...)
	 * 길이 제한 탓에 잘린 후보들이 모두 겹치는 드문 경우에는 PlayerId 를 붙여 구분함.
	 * @param InBase 다듬기를 마친 닉네임
	 * @param InIgnorePlayerState 중복 검사에서 제외할 PlayerState (요청자 자신)
	 */
	FString Auth_MakeUniqueNickname(const FString& InBase, const APlayerState* InIgnorePlayerState) const;

	/**
	 * [서버] 다른 플레이어가 이미 쓰는 닉네임인지 검사하는 함수. 대소문자만 다른 이름도 같은 이름으로 봄.
	 * @param InNickname 검사할 닉네임
	 * @param InIgnorePlayerState 검사에서 제외할 PlayerState
	 */
	bool Auth_IsNicknameTaken(const FString& InNickname, const APlayerState* InIgnorePlayerState) const;

	/** 닉네임 최대 길이(글자) */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Player", meta = (ClampMin = "2"))
	int32 MaxNicknameLength = 10;
};
