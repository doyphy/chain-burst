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

protected:
	//~ Begin AGameModeBase Interface.
	/** 
	 * [서버] 스폰할 폰 클래스를 결정함. PlayerState 에 고른 캐릭터가 있으면 그 클래스로, 없으면 게임모드 BP 의 기본값으로 스폰.
	 * 로비 재스폰·게임플레이 첫 스폰 모두 이 경로를 지남
	 */
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	//~ End AGameModeBase Interface.

	/** [서버] 고를 수 있는 캐릭터 카탈로그를 게임 인스턴스에서 가져옴. 등록하지 않았으면 nullptr. */
	UCBCharacterCatalog* GetCharacterCatalog() const;
};
