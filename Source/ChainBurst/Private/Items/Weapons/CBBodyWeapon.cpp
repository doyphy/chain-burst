// project
#include "Items/Weapons/CBBodyWeapon.h"

// engine
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

ACBBodyWeapon::ACBBodyWeapon()
{
	// 본체 무기는 손·칼집 소켓을 점유하지 않는다 (등록 시 소켓 중복 검사에서 제외됨).
	WeaponSocketType = ECBWeaponSocketType::None;
}

// 오너 캐릭터 메시의 뿌리 소켓 위치 (무기 메시가 아니라 캐릭터 본을 트레이스 소스로 사용)
FVector ACBBodyWeapon::GetWeaponRootLocation() const
{
	if (const USkeletalMeshComponent* OwnerMesh = GetOwnerMesh())
	{
		return OwnerMesh->GetSocketLocation(WeaponRootSocketName);
	}

	// 오너 메시를 찾지 못하면 액터 위치 (안전장치)
	return GetActorLocation();
}

// 오너 캐릭터 메시의 끝 소켓 위치
FVector ACBBodyWeapon::GetWeaponTipLocation() const
{
	if (const USkeletalMeshComponent* OwnerMesh = GetOwnerMesh())
	{
		return OwnerMesh->GetSocketLocation(WeaponTipSocketName);
	}

	// 오너 메시를 찾지 못하면 액터 위치 (안전장치)
	return GetActorLocation();
}

// 트레이스 소스가 되는 오너 캐릭터의 메시를 지연 캐싱해 반환
USkeletalMeshComponent* ACBBodyWeapon::GetOwnerMesh() const
{
	// 이미 캐싱되어 있으면 그대로 사용 (트레이스는 매 틱 호출되므로 조회를 반복하지 않는다)
	if (CachedOwnerMesh.IsValid())
	{
		return CachedOwnerMesh.Get();
	}

	// 스폰 시 SpawnParams.Owner 에 캐릭터가 지정되며, AActor::Owner 는 복제되므로 클라이언트에서도 찾을 수 있다.
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		return nullptr;
	}

	USkeletalMeshComponent* OwnerMesh = OwnerCharacter->GetMesh();
	if (!OwnerMesh)
	{
		return nullptr;
	}

	CachedOwnerMesh = OwnerMesh;

	// 메시를 처음 찾은 시점에 소켓 이름을 1회 검증
	ValidateTraceSockets(OwnerMesh);

	return OwnerMesh;
}

// 트레이스 소켓 이름이 오너 메시에 실제로 있는지 최초 1회 검증
void ACBBodyWeapon::ValidateTraceSockets(const USkeletalMeshComponent* InOwnerMesh) const
{
	if (bValidatedTraceSockets || !InOwnerMesh)
	{
		return;
	}
	bValidatedTraceSockets = true;

	// 스켈레탈 메시의 DoesSocketExist 는 소켓뿐 아니라 본 이름도 참으로 판정함 (본을 직접 써도 됨).
	if (!InOwnerMesh->DoesSocketExist(WeaponRootSocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 의 뿌리 소켓 [%s] 을 오너 메시에서 찾을 수 없음. 공격 판정이 캐릭터 원점에서 발생함."),
			*GetName(), *WeaponRootSocketName.ToString());
	}

	if (!InOwnerMesh->DoesSocketExist(WeaponTipSocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 의 끝 소켓 [%s] 을 오너 메시에서 찾을 수 없음. 공격 판정이 캐릭터 원점에서 발생함."),
			*GetName(), *WeaponTipSocketName.ToString());
	}
}
