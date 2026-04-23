#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "CBPlayerState.generated.h"

class UCBAbilitySystemComponent;
class UCBAttributeSet;

/**
 * 플레이어의 상태를 관리하는 클래스. 주로 어빌리티 시스템과 관련된 데이터를 저장하고 관리하는 역할을 함.
 * 플레이어가 소유한 [ASC]와 [AttributeSet]을 보유하며, IAbilitySystemInterface를 구현하여 다른 클래스에서 ASC에 접근할 수 있도록 함.
 * 플레이어의 상태는 PlayerState에서 관리하는 것이 일반적이므로, 플레이어 캐릭터가 소유하는 ASC는 PlayerState에 두고, 캐릭터에서는 PlayerState의 ASC를 참조하는 방식으로 구현.
 * 이렇게 하면 플레이어가 캐릭터를 변경하더라도 상태가 유지되고, 서버와 클라이언트 간의 상태 동기화가 용이해짐
 */
UCLASS()
class CHAINBURST_API ACBPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ACBPlayerState();
	
	//~ Begin IAbilitySystemInterface Interface.
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	//~ End IAbilitySystemInterface Interface.

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|AbilitySystem")
	TObjectPtr<UCBAbilitySystemComponent> CBAbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ChainBurst|Components|AbilitySystem")
	TObjectPtr<UCBAttributeSet> CBAttributeSet;

public:
	FORCEINLINE UCBAbilitySystemComponent* GetCBAbilitySystemComponent() const { return CBAbilitySystemComponent.Get(); }
	FORCEINLINE UCBAttributeSet* GetCBAttributeSet() const { return CBAttributeSet.Get(); }
};
