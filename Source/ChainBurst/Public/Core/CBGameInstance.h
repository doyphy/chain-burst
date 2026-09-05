#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "CBGameInstance.generated.h"

class UCBCharacterCatalog;

/**
 * 프로젝트 공용 게임 인스턴스 클래스.
 * 맵을 넘어 유지돼야 하는 게임 전역 로직·데이터를 여기에 둠.
 * 접속 실패 시 엔진의 자동 기본 맵 복귀를 통제하기 위해 온라인 세션 클래스를 갈아끼우고,
 * 고를 수 있는 캐릭터(무기) 카탈로그를 보유해 서버와 전 클라이언트가 같은 목록을 보게 함.
 * 또한 진영 판정 규칙(attitude solver)을 엔진에 등록해 전 시스템이 같은 적/아군 판정 규칙을 쓰게 함.
 */
UCLASS()
class CHAINBURST_API UCBGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	//~ Begin UGameInstance Interface.
	/** 게임 인스턴스 초기화. 진영 판정 규칙(attitude solver)을 엔진에 등록한다. */
	virtual void Init() override;

	/** 게임 인스턴스 종료. 등록한 진영 판정 규칙을 엔진 기본값으로 되돌린다. */
	virtual void Shutdown() override;

	/** 게임 인스턴스 시작. */
	virtual void OnStart() override;
	//~ End UGameInstance Interface.

	/** UCBGameInstance 에서 사용할 온라인 세션 클래스를 지정함. */
	virtual TSubclassOf<UOnlineSession> GetOnlineSessionClass() override;

	/** [Getter] 고를 수 있는 캐릭터(무기) 카탈로그. 등록하지 않았으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Character")
	FORCEINLINE UCBCharacterCatalog* GetCharacterCatalog() const { return CharacterCatalog; }

protected:
	/**
	 * 고를 수 있는 캐릭터(무기) 목록.
	 * 서버는 스폰할 클래스를 결정할 때, 클라이언트는 선택 UI 를 만들 때 읽음.
	 * 태그와 표시 이름 정도라 가벼움, 무거운 캐릭터 클래스,아이콘은 소프트 참조임.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "ChainBurst|Character")
	TObjectPtr<UCBCharacterCatalog> CharacterCatalog = nullptr;
};
