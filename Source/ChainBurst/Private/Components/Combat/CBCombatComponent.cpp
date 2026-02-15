// project
#include "Components/Combat/CBCombatComponent.h"
#include "Items/Weapons/CBBaseWeapon.h"
#include "Characters/CBBaseCharacter.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "CBGameplayTags.h"

void UCBCombatComponent::RegisterWeapon(FCBWeaponData InWeaponToRegister)
{
	// 등록할 무기 데이터가 유효한지 확인
	if (!InWeaponToRegister.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 에 등록할 무기 데이터가 유효하지 않습니다."), *GetOwner()->GetName());
		return;
	}
	
	// 동일한 무기 이미 등록되어 있는지 확인
	// operator== 연산자 중복 정의 필요
	if (WeaponSlots.Contains(InWeaponToRegister))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 에 [%s] 무기가 이미 등록되어 있음."), *GetOwner()->GetName(), *InWeaponToRegister.WeaponTag.ToString());
		return;
	}

	// 무기 생성
	ACBBaseWeapon* NewWeapon = SpawnWeapon(InWeaponToRegister.WeaponClass);

	if (!NewWeapon || !GetCachedOwnerMesh())
	{
		return;
	}
	
	// 무기 부착
	NewWeapon->AttachToSheath(GetCachedOwnerMesh());
	
	// 무기 데이터 생성
	FCBRegisteredWeaponData NewWeaponData(InWeaponToRegister, NewWeapon);

	// 생성한 무기 데이터를 무기 슬롯에 등록
	WeaponSlots.Emplace(NewWeaponData);

	// 처음 등록한 무기이면 0번 슬롯 무기로 자동 장착
	if (WeaponSlots.Num() == 1)
	{
		SetCurrentWeaponSlot(0);
		NewWeapon->SetActorHiddenInGame(false);
	}
}

bool UCBCombatComponent::IsCombatMode()
{
	// Shared_Status_Combat_InCombat 태그를 검사해서 반환
	return GetCachedOwnerASC()->HasMatchingGameplayTag(CBGameplayTags::Shared_Status_Combat_InCombat);
}

void UCBCombatComponent::SetCombatMode(bool bInCombat)
{
	// 이미 같은 상태라면 무시
	if (IsCombatMode() == bInCombat) return;
	
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

ACBBaseWeapon* UCBCombatComponent::SpawnWeapon(TSubclassOf<ACBBaseWeapon> WeaponClass)
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

		// 숨기기 (장착할 때만 보이게 하기)
		NewWeapon->SetActorHiddenInGame(true);
	}
	
	return NewWeapon;
}

void UCBCombatComponent::DestroyWeapon(ACBBaseWeapon* WeaponToDestroy)
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

int32 UCBCombatComponent::GetCurrentWeaponSlot()
{
	if (WeaponSlots.IsEmpty())
	{
		CurrentWeaponSlot = -1;
	}
	return CurrentWeaponSlot;
}

void UCBCombatComponent::SetCurrentWeaponSlot(int32 NewSlot)
{
	if (WeaponSlots.IsEmpty())
	{
		CurrentWeaponSlot = -1;
	}
	else
	{
		CurrentWeaponSlot = FMath::Clamp(NewSlot, 0, WeaponSlots.Num() - 1);
	}
}

void UCBCombatComponent::OnEnterCombatMode()
{
	// 현재 무기 슬롯이 유효한지 확인
	if (!WeaponSlots.IsValidIndex(GetCurrentWeaponSlot())) return;

	if (GetCachedOwnerASC() == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 의 ASC를 가져올 수 없습니다."), *GetOwner()->GetName());
		return;
	}
	
	// 태그 추가
	GetCachedOwnerASC()->AddLooseGameplayTag(CBGameplayTags::Shared_Status_Combat_InCombat);
	
	// 현재 장착중인 무기 Hand 에 부착
	if (ACBBaseWeapon* CurrentWeapon = WeaponSlots[GetCurrentWeaponSlot()].WeaponInstance)
	{
		CurrentWeapon->AttachToHand(GetCachedOwnerMesh());
	}
	else
	{
		ResetWeaponState(); // 초기화 및 복구 진행
		return;
	}
}

void UCBCombatComponent::OnExitCombatMode()
{
	// 현재 무기 슬롯이 유효한지 확인
	if (!WeaponSlots.IsValidIndex(GetCurrentWeaponSlot())) return;

	if (GetCachedOwnerASC() == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 의 ASC를 가져올 수 없습니다."), *GetOwner()->GetName());
		return;
	}

	// 태그 제거
	GetCachedOwnerASC()->RemoveLooseGameplayTag(CBGameplayTags::Shared_Status_Combat_InCombat);
	
	// 현재 장착중인 무기 Sheath 에 부착
	if (ACBBaseWeapon* CurrentWeapon = WeaponSlots[GetCurrentWeaponSlot()].WeaponInstance)
	{
		CurrentWeapon->AttachToSheath(GetCachedOwnerMesh());
	}
	else
	{
		ResetWeaponState(); // 초기화 및 복구 진행
		return;
	}
}

void UCBCombatComponent::SwapWeapon(int32 InSwapWeaponSlot)
{
	// 무기 슬롯이 비어있다면 무시
	if (WeaponSlots.IsEmpty()) return;
	
	// 현재 무기 슬롯 인덱스가 유효한지 확인
	if (!WeaponSlots.IsValidIndex(GetCurrentWeaponSlot()))
	{
		ResetWeaponState(); // 초기화 및 복구 진행
		return;
	}
	
	// InSwapWeaponSlot 값 범위 제한
	InSwapWeaponSlot = FMath::Clamp(InSwapWeaponSlot, 0, WeaponSlots.Num() - 1);
	
	// 교체할 무기 슬롯이 현재 슬롯과 같다면 무시
	if (GetCurrentWeaponSlot() == InSwapWeaponSlot) return;
	
	if (GetCachedOwnerMesh() == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 의 캐릭터 메쉬를 가져올 수 없음."), *GetOwner()->GetName());
		return;
	}
	
	// 현재 무기 해제
	if (ACBBaseWeapon* CurrentWeapon = WeaponSlots[GetCurrentWeaponSlot()].WeaponInstance)
	{
		// 전투 상태면 (무기 넣기)
		if (!IsCombatMode())
		{
			CurrentWeapon->AttachToSheath(GetCachedOwnerMesh());
		}
		// 액터 숨기기
		CurrentWeapon->SetActorHiddenInGame(true);
	}
	else
	{
		ResetWeaponState(); // 초기화 및 복구 진행
		return;
	}
	
	// 교체할 무기로 교체
	if (ACBBaseWeapon* SwapWeapon = WeaponSlots[InSwapWeaponSlot].WeaponInstance)
	{
		// 전투 상태면 (무기 꺼내기)
		if (IsCombatMode())
		{
			// Hand에 장착
			SwapWeapon->AttachToHand(GetCachedOwnerMesh());
		}
		// 액터 보이기
		SwapWeapon->SetActorHiddenInGame(true);
		// 현재 무기 슬롯 업데이트
		SetCurrentWeaponSlot(InSwapWeaponSlot);
	}
	else
	{
		ResetWeaponState(); // 초기화 및 복구 진행
		return;
	}
}

void UCBCombatComponent::ResetWeaponState()
{
	// 모든 무기 순회 (역순)
	for (int32 i = WeaponSlots.Num() - 1; i >= 0; --i)
	{
		FCBRegisteredWeaponData& SlotItem = WeaponSlots[i];
		ACBBaseWeapon* Weapon = SlotItem.WeaponInstance;

		// 슬롯의 무기가 유효하다면 초기화 작업
		if (Weapon && IsValid(Weapon))
		{
			if (USkeletalMeshComponent* CharacterMesh = GetCachedOwnerMesh())
			{
				// 강제 해제 (Sheath로 이동)
				Weapon->AttachToSheath(CharacterMesh);
				// 충돌 끄기
				Weapon->SetActorEnableCollision(false);
				// 액터 숨기기
				Weapon->SetActorHiddenInGame(true); 
			}
		}
		// 슬롯의 무기가 유효하지 않다면 제거 처리
		else
		{
			// 유효하지 않은 무기: 배열에서 제거
			WeaponSlots.RemoveAt(i);
			UE_LOG(LogTemp, Warning, TEXT("[%s] 의 무기 슬롯 [%d]의 무기가 유효하지 않습니다. 슬롯에서 제거 처리함."), *GetName(), i);
		}
	}

	// 비전투 상태로 전환
	if (GetCachedOwnerASC() != nullptr)
	{
		// 태그 제거
		GetCachedOwnerASC()->RemoveLooseGameplayTag(CBGameplayTags::Shared_Status_Combat_InCombat);
	}
	
	// 슬롯 초기화
	if (WeaponSlots.IsEmpty())
	{
		SetCurrentWeaponSlot(-1);
	}
	else
	{
		// 변수 초기화
		SetCurrentWeaponSlot(0); // 0번 무기 슬롯으로

		// 0번 무기 보이기 (장착)
		WeaponSlots[0].WeaponInstance->SetActorHiddenInGame(false);
	}
	
	UE_LOG(LogTemp, Log, TEXT("[%s] 무기 상태 초기화 완료."), *GetName());
}


