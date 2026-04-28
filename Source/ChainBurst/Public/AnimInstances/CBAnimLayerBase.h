#pragma once

#include "CoreMinimal.h"
#include "AnimInstances/CBBaseAnimInstance.h"
#include "CBAnimLayerBase.generated.h"

class UCBCharacterAnimInstance;

UCLASS()
class CHAINBURST_API UCBAnimLayerBase : public UCBBaseAnimInstance
{
	GENERATED_BODY()
	
protected:
	virtual void NativeInitializeAnimation() override;
	
	UPROPERTY(BlueprintReadOnly, Transient, Category = "ChainBurst|References")
	TObjectPtr<UCBCharacterAnimInstance> MainAnimInstance;
	
public:
	UFUNCTION(BlueprintPure, Category = "ChainBurst|References", meta = (BlueprintThreadSafe))
	UCBCharacterAnimInstance* GetCharacterAnimInstance() const {return MainAnimInstance;}
};
