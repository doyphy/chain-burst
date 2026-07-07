#pragma once

#include "CoreMinimal.h"
#include "Characters/CBBaseCharacter.h"
#include "CBAICharacter.generated.h"

class UCBCharacterLoadout;

/**
 * AI 캐릭터 공통 베이스 (Outlaw·Rogue 등).
 * ASC·AttributeSet을 캐릭터 자체가 소유하며, AI 공통 초기화 흐름을 담는다.
 * 직접 스폰하지 않는 추상 클래스.
 */
UCLASS(Abstract)
class CHAINBURST_API ACBAICharacter : public ACBBaseCharacter
{
	GENERATED_BODY()

public:
	ACBAICharacter();

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	//~ End AActor Interface

	//~ Begin APawn Interface
	virtual void PossessedBy(AController* NewController) override;
	//~ End APawn Interface

	/**
	 * AI 캐릭터 초기화 진입점. 로드아웃을 1회 로드해 공용 데이터(전 인스턴스) + 서버 권위 데이터(서버)를 적용하고 준비 완료를 통지한다.
	 * 서버는 PossessedBy(InitAbilityActorInfo 완료 후), 비권위(시뮬 프록시)는 BeginPlay에서 호출한다.
	 */
	void InitializeAISystem();

	/** 이 AI가 사용할 로드아웃 소프트 참조를 반환한다. 서브클래스가 자신의 타입 멤버를 반환하도록 구현한다. */
	virtual TSoftObjectPtr<UCBCharacterLoadout> GetAILoadout() const PURE_VIRTUAL(ACBAICharacter::GetAILoadout, return nullptr;);
};
