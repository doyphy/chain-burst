// project
#include "Items/Weapons/CBBaseWeapon.h"

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

	// 기본값 설정
	SheathSocket = ECBWeaponSheathSocket::None;
	SheathSocketOverride = NAME_None;
}

void ACBBaseWeapon::AttachToHand(USceneComponent* TargetMesh)
{
	if (!TargetMesh) return;

	// 부착 규칙 (위치/회전/크기 Snap)
	FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);

	// 무기 부착
	AttachToComponent(TargetMesh, AttachRules, FName("Socket_Combat_Hand_R"));
}

void ACBBaseWeapon::AttachToSheath(USceneComponent* TargetMesh)
{
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
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACBBaseWeapon, SheathSocket))
	{
		switch (SheathSocket)
		{
		case ECBWeaponSheathSocket::Hip:
			SheathSocketOverride = FName("Socket_Sheath_Hip_L");
			break;

		case ECBWeaponSheathSocket::Back:
			SheathSocketOverride = FName("Socket_Sheath_Back_L");
			break;

		case ECBWeaponSheathSocket::None:
			SheathSocketOverride = NAME_None;
			break;
		}
	}
}
