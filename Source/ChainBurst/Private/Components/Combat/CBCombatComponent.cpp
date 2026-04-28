// project
#include "Components/Combat/CBCombatComponent.h"
#include "Items/Weapons/CBBaseWeapon.h"
#include "Characters/CBBaseCharacter.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "CBGameplayTags.h"
#include "CBAbilitySystemLibrary.h"

// engine
#include "Net/UnrealNetwork.h"

UCBCombatComponent::UCBCombatComponent()
{
	SetIsReplicatedByDefault(true);
}


void UCBCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCBCombatComponent, EquippedWeapon);
}

/**
 * 서버에서만 호출 (PossessedBy 함수에서 호출)
 * UCBCharacterLoadout::RegisterWeaponsToCombatComponent 에서 호출되어 캐릭터의 무기를 등록하는데 사용됨.
 */
void UCBCombatComponent::Auth_RegisterWeapon(FCBWeaponData InWeaponToRegister)
{
	// 이미 무기가 등록되어 있는지 확인
	if (EquippedWeapon.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 에 이미 무기가 등록되어 있습니다."), *GetOwner()->GetName());
		return;
	}
	
	// 등록할 무기 데이터가 유효한지 확인
	if (!InWeaponToRegister.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 에 등록할 무기 데이터가 유효하지 않습니다."), *GetOwner()->GetName());
		return;
	}
	
	// 동일한 무기 이미 등록되어 있는지 확인
	// operator== 연산자 중복 정의 필요
	if (EquippedWeapon == InWeaponToRegister)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 에 [%s] 무기가 이미 등록되어 있음."), *GetOwner()->GetName(), *InWeaponToRegister.WeaponTag.ToString());
		return;
	}

	// 무기 생성
	ACBBaseWeapon* NewWeapon = Auth_SpawnWeapon(InWeaponToRegister.WeaponClass);

	if (!NewWeapon || !GetCachedOwnerMesh())
	{
		return;
	}
	
	// 무기 부착
	NewWeapon->AttachToSheath(GetCachedOwnerMesh());
	
	// 무기 데이터 생성
	FCBRegisteredWeaponData NewWeaponData(InWeaponToRegister, NewWeapon);

	// 생성한 무기 데이터를 장착된 무기로 설정
	EquippedWeapon = NewWeaponData;
}

bool UCBCombatComponent::IsCombatMode()
{
	// Shared_Status_Combat_InCombat 태그를 검사해서 반환
	return UCBAbilitySystemLibrary::IsCombatMode(GetOwner());
}

UAnimMontage* UCBCombatComponent::GetCurrentEquipMontage() const
{
	if (!EquippedWeapon.IsValid()) return nullptr;
	return EquippedWeapon.WeaponInstance->GetEquipMontage();
}

UAnimMontage* UCBCombatComponent::GetCurrentUnequipMontage() const
{
	if (!EquippedWeapon.IsValid()) return nullptr;
	return EquippedWeapon.WeaponInstance->GetUnequipMontage();
}

void UCBCombatComponent::SetCombatMode(bool bInCombat)
{
	// 이미 같은 상태라면 무시
	if (IsCombatMode() == bInCombat)
	{
		return;
	}
	
	// 전투 상태로 전환 시
	if (bInCombat)
	{
		OnEnterCombatMode();
	}
	// 비전투 상태로 전환 시
	else
	{
		OnExitCombatMode();
	}
}

/**
 * 서버에서만 호출
 * UCBCombatComponent::RegisterWeapon 함수에서 무기를 생성하는 데 사용됨.
 */
ACBBaseWeapon* UCBCombatComponent::Auth_SpawnWeapon(TSubclassOf<ACBBaseWeapon> WeaponClass)
{
	// 유효성 검사
	if (!WeaponClass || !GetWorld())
	{
		return nullptr;
	}

	// 스폰 파라미터 설정
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner(); // 무기의 주인은 캐릭터
	SpawnParams.Instigator = GetOwningPawn(); // 가해자도 캐릭터
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; // 충돌 무시하고 스폰

	// 액터 스폰 (위치는 캐릭터의 위치(0,0,0)으로 설정, 어차피 바로 부착할 것이므로 크게 중요하지 않음)
	FVector SpawnLocation = GetOwner()->GetActorLocation();
	FRotator SpawnRotation = FRotator::ZeroRotator;
	ACBBaseWeapon* NewWeapon = GetWorld()->SpawnActor<ACBBaseWeapon>(WeaponClass, SpawnLocation, SpawnRotation, SpawnParams);
	
	if (NewWeapon)
	{
		// 충돌 끄기 (충돌은 데미지 로직에서 처리)
		NewWeapon->SetActorEnableCollision(false);

		// 틱 끄기 (필요할 때만 켜기)
		NewWeapon->SetActorTickEnabled(false);

		// 보이기 (필요할 때는 숨기기)
		NewWeapon->SetActorHiddenInGame(false);
	}
	
	return NewWeapon;
}

void UCBCombatComponent::Auth_DestroyWeapon(ACBBaseWeapon* WeaponToDestroy)
{
}

USkeletalMeshComponent* UCBCombatComponent::GetCachedOwnerMesh()
{
	if (CachedOwnerMesh == nullptr)
	{
		if (ACBBaseCharacter* CharacterOwner = GetOwningPawn<ACBBaseCharacter>())
		{
			CachedOwnerMesh = CharacterOwner->GetMesh();
		}
	}
	return CachedOwnerMesh;
}

UCBAbilitySystemComponent* UCBCombatComponent::GetCachedOwnerASC()
{
	if (CachedOwnerASC == nullptr)
	{
		if (ACBBaseCharacter* CharacterOwner = GetOwningPawn<ACBBaseCharacter>())
		{
			CachedOwnerASC = CharacterOwner->GetCBAbilitySystemComponent();
		}
	}
	return CachedOwnerASC;
}

void UCBCombatComponent::OnEnterCombatMode()
{
	// 현재 무기 유효 검사
	if (!EquippedWeapon.IsValid()) return;

	// ASC 유효 검사
	if (GetCachedOwnerASC() == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 의 ASC를 가져올 수 없습니다."), *GetOwner()->GetName());
		return;
	}

	// 현재 장착중인 무기 Hand 에 부착
	EquippedWeapon.WeaponInstance->AttachToHand(GetCachedOwnerMesh());
}

void UCBCombatComponent::OnExitCombatMode()
{
	// 현재 무기 유효 검사
	if (!EquippedWeapon.IsValid()) return;

	// ASC 유효 검사
	if (GetCachedOwnerASC() == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 의 ASC를 가져올 수 없습니다."), *GetOwner()->GetName());
		return;
	}
	
	// 현재 장착중인 무기 Sheath 에 부착
	EquippedWeapon.WeaponInstance->AttachToSheath(GetCachedOwnerMesh());
}


