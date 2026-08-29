// project
#include "DataAssets/Loadout/CBChaserLoadout.h"
#include "AbilitySystem/Abilities/CBChaserGameplayAbility.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Characters/CBChaserCharacter.h"
#include "Components/UI/CBUIComponent.h"
#include "Components/Mesh/CBModularMeshComponent.h"

bool FCBInputAbilitySet::IsValid() const
{
	return InputTag.IsValid() && AbilityToGrant;
}

void UCBChaserLoadout::Auth_GrantAbilitiesToASC(UCBAbilitySystemComponent* InASCToGive, int32 ApplyLevel)
{
	// 액티브, 패시브, 반응형 어빌리티 부여 (부모 함수 호출)
	Super::Auth_GrantAbilitiesToASC(InASCToGive, ApplyLevel);
	
	// InputAbilitySets 배열을 순회하며 각 어빌리티 세트를 처리
	for(const FCBInputAbilitySet& AbilitySet : InputAbilitySets)
	{
		// 입력 태그와 어빌리티 클래스가 모두 유효한지 검사, 유효하지 않으면 건너뜀
		if (!AbilitySet.IsValid()) continue;
		
		// 어빌리티 스펙 생성 (부여할 어빌리티 클래스 기반)
		FGameplayAbilitySpec AbilitySpec(AbilitySet.AbilityToGrant);
		// 어빌리티의 소스 오브젝트를 현재 어빌리티 시스템 컴포넌트의 아바타로 지정
		AbilitySpec.SourceObject = InASCToGive->GetAvatarActor();
		// 어빌리티 레벨 설정
		AbilitySpec.Level = ApplyLevel;
		// 입력 태그를 동적 어빌리티 태그에 추가 (입력 매핑 등에서 활용)
		AbilitySpec.GetDynamicSpecSourceTags().AddTag(AbilitySet.InputTag);

		// 어빌리티 시스템 컴포넌트에 어빌리티 부여.
		InASCToGive->Auth_GiveLoadoutAbility(AbilitySpec);
	}
}

void UCBChaserLoadout::ApplyToCharacter(ACBBaseCharacter* InCharacter)
{
	// 공용 데이터 적용 (바디 셋업·스켈레탈 메시·애님BP·이동/몽타주 데이터)
	Super::ApplyToCharacter(InCharacter);

	// 피부·의상 메시는 모듈러 메시 컴포넌트를 가진 추격자에만 해당
	ACBChaserCharacter* ChaserCharacter = Cast<ACBChaserCharacter>(InCharacter);
	if (!ChaserCharacter) return;

	// 부착·리더 포즈 연결은 준비 완료 후 컴포넌트가 수행하므로 여기서는 데이터만 넘김.
	// 기본 의상은 태그만 넘기고, 메시 로드는 준비 완료 직전에 PreloadDefaultCosmetics 가 수행함.
	if (UCBModularMeshComponent* ModularMeshComponent = ChaserCharacter->GetModularMeshComponent())
	{
		ModularMeshComponent->SetSkinMeshes(SkinMeshes);
		ModularMeshComponent->SetCosmeticCatalog(CosmeticCatalog);
		ModularMeshComponent->SetDefaultCosmeticIds(DefaultCosmeticIds);
		ModularMeshComponent->SetHideLeaderMesh(bHideLeaderMesh);
	}
}

// [공용] 기본 의상 파츠 메시 로드. 카탈로그의 소프트 참조라 로드아웃 로드만으로는 resolve되지 않음
void UCBChaserLoadout::ApplyAsyncToCharacter(ACBBaseCharacter* InCharacter, TFunction<void()> OnComplete)
{
	// 모듈러 메시 컴포넌트 가져오기
	const ACBChaserCharacter* ChaserCharacter = Cast<ACBChaserCharacter>(InCharacter);
	UCBModularMeshComponent* ModularMeshComponent = ChaserCharacter ? ChaserCharacter->GetModularMeshComponent() : nullptr;

	// 모듈러 메시 컴포넌트 유효성 검사
	if (!ModularMeshComponent)
	{
		// 유효하지 않으면 바로 완료 콜백 호출 (로드아웃 적용이 끝난 것으로 간주)
		OnComplete();
		return;
	}

	// 유효하면 기본 의상 파츠 메시를 비동기 로드하고 완료 시 OnComplete 호출
	ModularMeshComponent->PreloadDefaultCosmetics(MoveTemp(OnComplete));
}

void UCBChaserLoadout::Local_ApplyToCharacter(ACBChaserCharacter* InCharacter)
{
	// 캐릭터 유효성 검사
	if (!InCharacter) return;

	// 입력 설정 주입 (세터 내부에서 입력 컴포넌트 준비 여부를 확인해 지연 바인딩 수행)
	InCharacter->SetInputConfig(InputConfig);

	// HUD 위젯 클래스 주입 (위젯 생성은 준비 완료 후 UI 컴포넌트가 수행)
	if (UCBUIComponent* UIComponent = InCharacter->GetCBUIComponent())
	{
		UIComponent->SetHUDWidgetClass(HUDWidgetClass);
	}
}
