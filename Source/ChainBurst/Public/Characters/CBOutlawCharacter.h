#pragma once

#include "CoreMinimal.h"
#include "Characters/CBBaseCharacter.h"
#include "CBOutlawCharacter.generated.h"

UCLASS()
class CHAINBURST_API ACBOutlawCharacter : public ACBBaseCharacter
{
	GENERATED_BODY()

public:
	ACBOutlawCharacter();

protected:
	/** [서버] AI 컨트롤러가 빙의될 때 */
	virtual void PossessedBy(AController* NewController) override;
	/** [클라이언트] 클라이언트에 액터가 복제된 직후 (BeginPlay 직전)*/
	virtual void PostNetInit() override;

	/**
	 * 서버와 클라이언트 모두에서 호출되는 초기화 진입 함수
	 * PossessedBy 또는 PostNetInit 에서 호출됨. 
	 */
	void InitializeAISystem();

	/** 서버 전용 초기화 함수 */
	void Auth_InitServerData();

	/** 클라이언트 전용 초기화 함수 */
	void Local_InitClientData();
};
