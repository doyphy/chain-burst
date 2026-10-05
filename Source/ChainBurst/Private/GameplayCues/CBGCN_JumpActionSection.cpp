// project
#include "GameplayCues/CBGCN_JumpActionSection.h"

#include "Characters/CBBaseCharacter.h"

// engine
#include "Animation/AnimMontage.h"

bool UCBGCN_JumpActionSection::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (ACBBaseCharacter* Char = Cast<ACBBaseCharacter>(MyTarget))
	{
		// 몽타주 + 섹션 인덱스 (섹션 이름은 큐 파라미터에 실을 수 없어 인덱스로 받음)
		const UAnimMontage* Montage = Cast<UAnimMontage>(Parameters.SourceObject.Get());
		Char->RequestJumpToSection(Montage, FMath::RoundToInt(Parameters.RawMagnitude));
	}

	return Super::OnExecute_Implementation(MyTarget, Parameters);
}
