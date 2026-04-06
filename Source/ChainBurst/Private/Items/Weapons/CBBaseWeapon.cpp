// project
#include "Items/Weapons/CBBaseWeapon.h"
#include "DataAssets/Weapon/CBWeaponSocketData.h"

// engine
#include "Components/BoxComponent.h"

ACBBaseWeapon::ACBBaseWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	// 컴포넌트 생성
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	
	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetupAttachment(SceneRoot);

	WeaponCollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WeaponCollisionBox"));
	WeaponCollisionBox->SetupAttachment(RootComponent);
	WeaponCollisionBox->SetBoxExtent(FVector(20.f));
	WeaponCollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACBBaseWeapon::AttachToHand(USceneComponent* TargetMesh)
{
	if (!TargetMesh) return;

	// 이미 장착된 상태면 무시 (전투 상태면)
	if (bIsEquipped) return;
	bIsEquipped = true;
	
	// 부착 규칙 (위치/회전/크기 Snap)
	FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);

	// 무기 부착
	AttachToComponent(TargetMesh, AttachRules, CombatSocketOverride);
}

void ACBBaseWeapon::AttachToSheath(USceneComponent* TargetMesh)
{
	if (!bRequiresSheathSocketAttachment) return;

	if (!TargetMesh) return;

	// 이미 장착된 상태면 무시 (비전투 상태면)
	if (!bIsEquipped) return;
	bIsEquipped = false;
	
	// 부착 규칙 (위치/회전/크기 Snap)
	FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);

	// 무기 부착
	AttachToComponent(TargetMesh, AttachRules, SheathSocketOverride);
}

void ACBBaseWeapon::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	// 변경된 속성의 이름을 가져옴
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// 변경된 속성이 'SheathSocket'인 경우에만 실행
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACBBaseWeapon, WeaponCombatType))
	{
		// 무기 종류에 맞는 소켓 설정을 조회
		if (WeaponSocketData)
		{
			FCBWeaponSocketConfig SocketConfig;
			if (WeaponSocketData->FindSocketConfig(WeaponCombatType, SocketConfig))
			{
				CombatSocketOverride = SocketConfig.CombatSocket;
				SheathSocketOverride = SocketConfig.SheathSocket;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] 의 무기 종류 [%d]에 대한 소켓 설정을 찾을 수 없습니다. 데이터 에셋을 확인하세요."), *GetName(), static_cast<int32>(WeaponCombatType));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] 의 WeaponSocketData가 설정되어 있지 않습니다. 소켓 설정을 적용할 수 없습니다."), *GetName());
		}
	}
}
