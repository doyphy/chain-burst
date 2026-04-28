// project
#include "AnimInstances/CBAnimLayerBase.h"
#include "AnimInstances/CBCharacterAnimInstance.h"

void UCBAnimLayerBase::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	MainAnimInstance = Cast<UCBCharacterAnimInstance>(GetSkelMeshComponent()->GetAnimInstance());
}
