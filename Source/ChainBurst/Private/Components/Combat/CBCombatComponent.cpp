// project
#include "Components/Combat/CBCombatComponent.h"
#include "Items/Weapons/CBBaseWeapon.h"
#include "Characters/CBBaseCharacter.h"
#include "CBAbilitySystemLibrary.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "CBGameplayTags.h"

// engine
#include "Net/UnrealNetwork.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameplayEffect.h"

UCBCombatComponent::UCBCombatComponent()
{
	SetIsReplicatedByDefault(true);

	// Tick 사용
	PrimaryComponentTick.bCanEverTick = true;

	// Tick은 필요할 때만 켜도록 설정 (예: 무기 트레이스 활성화 시)
	PrimaryComponentTick.bStartWithTickEnabled = false;
}


void UCBCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCBCombatComponent, EquippedWeapon);
}

void UCBCombatComponent::TickComponent(float DeltaTime, enum ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsTracing)
	{
		TickWeaponTrace();

		// 히트 배칭 타이머 업데이트
		HitBatchAccumulator += DeltaTime;

		// 일정 시간마다 배칭된 히트 처리 (기본 값 : 0.1초)
		if (HitBatchAccumulator >= HitBatchInterval)
		{
			FlushPendingHits();
		}
	}
}

/**
 * 서버에서만 호출 (PossessedBy 함수에서 호출)
 * UCBCharacterLoadout::RegisterWeaponsToCombatComponent 에서 호출되어 캐릭터의 무기를 등록하는데 사용됨.
 */
void UCBCombatComponent::Auth_RegisterWeapon(UCBWeaponData* InWeaponToRegister)
{
	// 이미 무기가 등록되어 있는지 확인
	if (EquippedWeapon.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 에 이미 무기가 등록되어 있습니다."), *GetOwner()->GetName());
		return;
	}
	
	// 등록할 무기 데이터가 유효한지 확인
	if (!InWeaponToRegister->HasValidData())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 에 등록할 무기 데이터가 유효하지 않습니다."), *GetOwner()->GetName());
		return;
	}
	
	// 동일한 무기 이미 등록되어 있는지 확인
	// operator== 연산자 중복 정의 필요
	if (EquippedWeapon == InWeaponToRegister)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 에 [%s] 무기가 이미 등록되어 있음."), *GetOwner()->GetName(), *InWeaponToRegister->WeaponTag.ToString());
		return;
	}

	// 무기 생성
	ACBBaseWeapon* NewWeapon = Auth_SpawnWeapon(InWeaponToRegister->WeaponClass);

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

	// 무기 AttackPower GE 적용
	Auth_ApplyWeaponAttackPowerEffect(InWeaponToRegister);
}

bool UCBCombatComponent::IsCombatMode()
{
	// Status_Combat_InCombat 태그를 검사해서 반환
	return UCBAbilitySystemLibrary::IsCombatMode(GetOwner());
}

void UCBCombatComponent::SetCombatMode(bool bInCombat)
{
	// 이미 같은 상태라면 무시
	if (IsCombatMode() == bInCombat)
	{
		return;
	}
	
	// 전투 상태로 전환 시 (무기 부착)
	if (bInCombat)
	{
		OnEnterCombatMode();
	}
	// 비전투 상태로 전환 시 (무기 부착)
	else
	{
		OnExitCombatMode();
	}
}

void UCBCombatComponent::StartWeaponTrace()
{
	// 내 캐릭터인지 확인
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled()) return;
	
	// 새로운 트레이스 시작. 기존 충돌 기록 초기화
	AlreadyHitActors.Empty();

	if (!EquippedWeapon.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 에는 트레이스할 무기가 유효하지 않음."), *GetOwner()->GetName());
		return;
	}
	
	PrevRootLoc = EquippedWeapon.WeaponInstance->GetWeaponRootLocation();
	PrevTipLoc = EquippedWeapon.WeaponInstance->GetWeaponTipLocation();
	
	// Trace 및 Tick 활성화
	bIsTracing = true;
	SetComponentTickEnabled(true);
}

void UCBCombatComponent::TickWeaponTrace()
{
	// 내 캐릭터인지 확인
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled()) return;

	// 무기 유효성 검사
	if (!EquippedWeapon.IsValid()) return;

	// 현재 무기의 뿌리와 끝 위치 가져오기
	FVector CurrRootLoc = EquippedWeapon.WeaponInstance->GetWeaponRootLocation();
	FVector CurrTipLoc = EquippedWeapon.WeaponInstance->GetWeaponTipLocation();

	// 무시할 액터 목록 생성 (나 자신과 무기 자체는 때리지 않음)
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(OwnerPawn);
	ActorsToIgnore.Add(EquippedWeapon.WeaponInstance);

	// 트레이스 결과를 담을 배열
	TArray<FHitResult> HitResults;
	
	// 디버그 드로잉 설정
	EDrawDebugTrace::Type DebugTraceType = bShowDebugTrace ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None;

	// 트레이스 분할 계산 (과거 위치와 현재 위치 사이를 TraceSubdivisions 만큼 나눔)
	for (int32 i = 0; i <= TraceSubdivisions; i++)
	{
		// 0.0 ~ 1.0 사이의 비율 (예: 3등분이면 0.0, 0.33, 0.66, 1.0)
		float Alpha = (float)i / TraceSubdivisions;

		// 과거의 위치와 현재의 위치를 계산
		FVector PrevPoint = FMath::Lerp(PrevRootLoc, PrevTipLoc, Alpha);
		FVector CurrPoint = FMath::Lerp(CurrRootLoc, CurrTipLoc, Alpha);

		// 트레이스 결과 배열 초기화
		HitResults.Reset();
		
		// 구형 트레이스 발사
		bool bHit = UKismetSystemLibrary::SphereTraceMulti(
			this,
			PrevPoint,			// 시작 위치
			CurrPoint,			// 끝 위치
			TraceRadius,		// 트레이스 두께
			WeaponTraceChannel,	// 트레이스 채널
			false,				// 복잡한 콜리전 검사 여부
			ActorsToIgnore,		// 무시할 액터들
			DebugTraceType,		// 디버그 선 그리기 여부
			HitResults,		// 결과를 담을 곳
			true,				// 자신 무시 (안전장치)
			FLinearColor::Red,	// 트레이스 색상
			FLinearColor::Green,// 타격 성공 시 색상
			2.0f				// 디버그 선 유지 시간
		);

		// 타격 성공 시
		if (bHit)
		{
			for (const FHitResult& Hit : HitResults)
			{
				AActor* HitActor = Hit.GetActor();

				// 충돌한 액터가 유효하고, 충돌한 기록이 없다면 배칭 목록에 추가
				// 배칭 목록에 담아두고 일정 간격 마다 한 번에 처리함.
				if (HitActor && !AlreadyHitActors.Contains(HitActor))
				{
					AlreadyHitActors.Add(HitActor);
					PendingHits.Add(Hit);
				}
			}
		}
	}
	// 다음 프레임(Tick)을 위해 현재 위치를 과거 위치로 저장.
	PrevRootLoc = CurrRootLoc;
	PrevTipLoc = CurrTipLoc;
}

void UCBCombatComponent::StopWeaponTrace()
{
	// 내 캐릭터인지 확인
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled()) return;

	// 남은 히트 즉시 처리
	FlushPendingHits();

	// 트레이스 종료. 충돌 기록 비우기
	AlreadyHitActors.Empty();

	// Trace 및 Tick 비활성화
	bIsTracing = false;
	SetComponentTickEnabled(false);
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
		// 충돌 끄기 (충돌은 필요한 경우 켜기)
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

void UCBCombatComponent::Auth_ApplyWeaponAttackPowerEffect(UCBWeaponData* InWeaponData)
{
	// ASC 가져오기
	UCBAbilitySystemComponent* ASC = GetCachedOwnerASC();
	if (!ASC || !InWeaponData || !InWeaponData->WeaponAttackPowerEffect)
	{
		return;
	}

	// Context 생성
	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(InWeaponData->WeaponAttackPowerEffect, 1.f, ContextHandle);
	if (!SpecHandle.IsValid())
	{
		return;
	}

	// SetByCallerMagnitude 설정 (무기 공격력)
	SpecHandle.Data->SetSetByCallerMagnitude(CBGameplayTags::Data_Weapon_AttackPower, InWeaponData->WeaponDamage);

	// GE 적용 및 핸들 저장
	WeaponAttackPowerEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
}

void UCBCombatComponent::Auth_RemoveWeaponAttackPowerEffect()
{
	// ASC 가져오기
	UCBAbilitySystemComponent* ASC = GetCachedOwnerASC();
	if (!ASC || !WeaponAttackPowerEffectHandle.IsValid())
	{
		return;
	}

	// GE 제거
	ASC->RemoveActiveGameplayEffect(WeaponAttackPowerEffectHandle);

	// 무기 공격력 GE 핸들 초기화
	WeaponAttackPowerEffectHandle.Invalidate();
}

USkeletalMeshComponent* UCBCombatComponent::GetCachedOwnerMesh()
{
	if (!CachedOwnerMesh.IsValid())
	{
		if (ACBBaseCharacter* CharacterOwner = GetOwningPawn<ACBBaseCharacter>())
		{
			CachedOwnerMesh = CharacterOwner->GetMesh();
		}
	}
	return CachedOwnerMesh.Get();
}

UCBAbilitySystemComponent* UCBCombatComponent::GetCachedOwnerASC()
{
	if (!CachedOwnerASC.IsValid())
	{
		if (ACBBaseCharacter* CharacterOwner = GetOwningPawn<ACBBaseCharacter>())
		{
			CachedOwnerASC = CharacterOwner->GetCBAbilitySystemComponent();
		}
	}
	return CachedOwnerASC.Get();
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

void UCBCombatComponent::ProcessHit(const FGameplayAbilityTargetDataHandle& TargetDataHandle)
{
	// 로컬에서 감지한 히트 정보를 서버로 전달
	Server_NotifyAttackHit(TargetDataHandle);
}

void UCBCombatComponent::Server_NotifyAttackHit_Implementation(const FGameplayAbilityTargetDataHandle& TargetDataHandle)
{
	UCBAbilitySystemComponent* ASC = GetCachedOwnerASC();
	if (!ASC || !EquippedWeapon.IsValid()) return;

	// 허용 거리 = 공격자 위치 ~ WeaponTip 거리 + 트레이스 반지름 + 레이턴시 보정값
	// WeaponTip 이 가장 멀리 히트 판정이 가능한 지점이므로 기준으로 사용
	const float AttackerToTipDistance = FVector::Distance(
		GetOwner()->GetActorLocation(),
		EquippedWeapon.WeaponInstance->GetWeaponTipLocation()
	);
	const float AllowedDistance = AttackerToTipDistance + TraceRadius + HitValidationTolerance;

	// 유효한 히트만 담을 핸들
	FGameplayAbilityTargetDataHandle ValidatedHandle;

	for (int32 i = 0; i < TargetDataHandle.Num(); i++)
	{
		const FGameplayAbilityTargetData* TargetData = TargetDataHandle.Get(i);
		if (!TargetData) continue;

		const FHitResult* HitResult = TargetData->GetHitResult();
		if (!HitResult) continue;

		AActor* HitActor = HitResult->GetActor();
		if (!HitActor) continue;

		// 히트한 액터와 내 캐릭터 사이의 거리를 계산해서 허용 거리보다 멀면 유효하지 않은 히트로 간주
		const float ActualDistance = FVector::Distance(GetOwner()->GetActorLocation(), HitActor->GetActorLocation());
		if (ActualDistance > AllowedDistance)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] 히트 검증 실패: 거리 %.1f > 허용 %.1f (타겟: %s)"),
				*GetOwner()->GetName(), ActualDistance, AllowedDistance, *HitActor->GetName());
			continue;
		}

		// 검증된 히트는 핸들에 추가
		ValidatedHandle.Add(new FGameplayAbilityTargetData_SingleTargetHit(*HitResult));
	}

	if (ValidatedHandle.Num() == 0) return;

	// 검증된 히트 정보를 가지고 이벤트 처리
	FGameplayEventData EventData;
	EventData.TargetData = ValidatedHandle;
	ASC->HandleGameplayEvent(CBGameplayTags::Event_Combat_Attack_Hit, &EventData);
}

void UCBCombatComponent::FlushPendingHits()
{
	// 배칭된 히트가 없으면 처리하지 않음
	if (PendingHits.IsEmpty()) return;

	// 히트 정보 담기
	FGameplayAbilityTargetDataHandle TargetDataHandle;
	for (const FHitResult& Hit : PendingHits)
	{
		TargetDataHandle.Add(new FGameplayAbilityTargetData_SingleTargetHit(Hit));
	}

	// 히트 정보와 함께 이벤트 처리
	ProcessHit(TargetDataHandle);

	// 배칭 목록 초기화
	PendingHits.Reset();
	HitBatchAccumulator = 0.0f;
}


