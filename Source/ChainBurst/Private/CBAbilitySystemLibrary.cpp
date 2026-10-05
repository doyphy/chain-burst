// project
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"
#include "Types/CBCollisionChannels.h"

// engine
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

// ASC 소유 액터를 폰으로 변환. 이미 폰이면 그대로 반환.
const AActor* UCBAbilitySystemLibrary::ResolveOwningPawn(const AActor* InActor)
{
	if (!InActor) return nullptr;

	// AI 는 캐릭터가 ASC 를 소유하므로 바로 반환.
	if (const APawn* Pawn = Cast<APawn>(InActor)) return Pawn;

	// 컨트롤러·PlayerState 는 조종 중인 폰으로 변환 (플레이어는 PlayerState 가 ASC 소유자)
	if (const AController* Controller = Cast<AController>(InActor)) return Controller->GetPawn();
	if (const APlayerState* PlayerState = Cast<APlayerState>(InActor)) return PlayerState->GetPawn();

	return InActor;
}

// 캐릭터 메시의 소켓(또는 본) 월드 위치 조회
bool UCBAbilitySystemLibrary::FindMeshSocketLocation(const AActor* InActor, FName InSocketName, FVector& OutLocation)
{
	const ACharacter* Character = Cast<ACharacter>(InActor);
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;

	// 소켓·본 이름 모두 허용. 없는 이름이면 엔진이 컴포넌트 위치를 돌려주므로 먼저 거름.
	if (!Mesh || InSocketName.IsNone() || !Mesh->DoesSocketExist(InSocketName)) return false;

	OutLocation = Mesh->GetSocketLocation(InSocketName);
	return true;
}

// 두 지점 사이를 벽이 가로막는지 검사 (Weapon 채널)
bool UCBAbilitySystemLibrary::IsBlockedByWall(const UWorld& InWorld, const FVector& InFrom, const FVector& InTo, const FCollisionQueryParams& InQueryParams)
{
	// 캐릭터 메시는 이 채널에 Overlap 이라 단일 트레이스의 블로킹 결과에 걸리지 않음 → 걸리는 것은 벽·지형뿐
	FHitResult WallHit;
	return InWorld.LineTraceSingleByChannel(WallHit, InFrom, InTo, CBCollisionChannels::Weapon, InQueryParams);
}

UAbilitySystemComponent* UCBAbilitySystemLibrary::GetASC(const AActor* InActor)
{
	if (!InActor) return nullptr;

	// IAbilitySystemInterface 를 구현한 액터라면 ASC 반환
	const IAbilitySystemInterface* ASCInterface = Cast<IAbilitySystemInterface>(InActor);
	if (!ASCInterface) return nullptr;

	return ASCInterface->GetAbilitySystemComponent();
}

bool UCBAbilitySystemLibrary::HasGameplayTag(const AActor* InActor, const FGameplayTag& InTag)
{
	if (!InTag.IsValid()) return false;

	UAbilitySystemComponent* ASC = GetASC(InActor);
	if (!ASC) return false;

	return ASC->HasMatchingGameplayTag(InTag);
}

UCBAbilitySystemComponent* UCBAbilitySystemLibrary::GetSafeCBASC(const AActor* InActor)
{
	if (!InActor) return nullptr;

	UCBAbilitySystemComponent* CBASC = Cast<UCBAbilitySystemComponent>(GetASC(InActor));
	if (!CBASC) return nullptr;

	return CBASC;
}

FGameplayTag UCBAbilitySystemLibrary::GetCurrentGaitTag(const UAbilitySystemComponent* InASC)
{
	// ASC 가 없으면 기본 개이트 (Run) 반환
	if (!InASC) return CBGameplayTags::Status_Movement_Gait_Run;

	// Sprint > Walk > 기본 Run 우선순위로 판별
	if (InASC->HasMatchingGameplayTag(CBGameplayTags::Status_Movement_Gait_Sprint))
	{
		return CBGameplayTags::Status_Movement_Gait_Sprint;
	}
	if (InASC->HasMatchingGameplayTag(CBGameplayTags::Status_Movement_Gait_Walk))
	{
		return CBGameplayTags::Status_Movement_Gait_Walk;
	}
	return CBGameplayTags::Status_Movement_Gait_Run;
}

int32 UCBAbilitySystemLibrary::GetGaitMontageIndex(const AActor* InActor)
{
	UAbilitySystemComponent* ASC = GetASC(InActor);
	if (!ASC) return 0;

	// Idle 파생 상태 태그 우선 (LocomotionProcessor가 로컬 미러링)
	if (ASC->HasMatchingGameplayTag(CBGameplayTags::Status_Movement_Idle))
	{
		return 0;
	}

	// 이동 중이면 개이트 태그로 분기 — Walk=1, Run/Sprint=2 (Sprint 몽타주는 Run 변형과 공유)
	return GetCurrentGaitTag(ASC) == CBGameplayTags::Status_Movement_Gait_Walk ? 1 : 2;
}

bool UCBAbilitySystemLibrary::GetCBCachedASC(const AActor* InActor, TWeakObjectPtr<UCBAbilitySystemComponent>& OutASC)
{
	// 이미 캐싱된 포인터가 유효한지 확인
	if (OutASC.IsValid())
	{
		return true;
	}

	// 액터 유효성 검사
	if (!IsValid(InActor))
	{
		return false;
	}

	// ASC 가져오기 시도
	if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InActor))
	{
		OutASC = Cast<UCBAbilitySystemComponent>(GetASC(InActor));
	}
	
	return OutASC.IsValid();
}

FActiveGameplayEffectHandle UCBAbilitySystemLibrary::NativeApplyEffectSpecHandleToTarget(AActor* TargetActor,
	const FGameplayEffectSpecHandle& InSpecHandle)
{
	if (!IsValid(TargetActor) || !InSpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);

	if (!TargetASC)
	{
		return FActiveGameplayEffectHandle();
	}
	
	return TargetASC->ApplyGameplayEffectSpecToSelf(*InSpecHandle.Data.Get());
}

FActiveGameplayEffectHandle UCBAbilitySystemLibrary::BP_ApplyEffectSpecHandleToTarget(AActor* TargetActor,
	const FGameplayEffectSpecHandle& InSpecHandle, ECBSuccessType& OutSuccessType)
{
	FActiveGameplayEffectHandle Handle = NativeApplyEffectSpecHandleToTarget(TargetActor, InSpecHandle);
    
	OutSuccessType = Handle.IsValid() ? ECBSuccessType::Success : ECBSuccessType::Failure;
    
	return Handle;
}

FGameplayEffectSpecHandle UCBAbilitySystemLibrary::NativeMakeEffectSpecHandle(TSubclassOf<UGameplayEffect> GEClass,
	AActor* SourceActor, float Level)
{
	// 유효성 검사
	if (!GEClass || !IsValid(SourceActor))
	{
		return FGameplayEffectSpecHandle();
	}

	// SourceActor의 ASC 가져오기
	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);
	if (!SourceASC)
	{
		return FGameplayEffectSpecHandle();
	}

	// 이펙트 컨텍스트 생성 및 Instigator 정보 추가
	FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
	ContextHandle.AddInstigator(SourceActor, SourceActor);

	// SpecHandle 생성하여 반환
	return SourceASC->MakeOutgoingSpec(GEClass, Level, ContextHandle);
}

FGameplayEffectSpecHandle UCBAbilitySystemLibrary::BP_MakeEffectSpecHandle(TSubclassOf<UGameplayEffect> GEClass,
	AActor* SourceActor, float Level, ECBSuccessType& OutSuccessType)
{
	FGameplayEffectSpecHandle Handle = NativeMakeEffectSpecHandle(GEClass, SourceActor, Level);
    
	OutSuccessType = Handle.IsValid() ? ECBSuccessType::Success : ECBSuccessType::Failure;
    
	return Handle;
}

void UCBAbilitySystemLibrary::DrawTagDebugMessage(const AActor* InActor, const FGameplayTag& InTag)
{
	if (!GEngine || !IsValid(InActor)) return;

	// ASC 가져오기 및 태그 보유 여부 확인
	UAbilitySystemComponent* ASC = GetASC(InActor);
	bool bHasTag = ASC ? ASC->HasMatchingGameplayTag(InTag) : false;

	// 실행 중인 환경 판별 (Server / Client)
	FString NetSide = InActor->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");
    
	// 현재 캐릭터의 역할 판별 (나 / 남 / 서버객체)
	FString Role;
	switch (InActor->GetLocalRole())
	{
		case ROLE_Authority: Role = TEXT("Auth"); break;
		case ROLE_AutonomousProxy: Role = TEXT("Auto(Me)"); break;
		case ROLE_SimulatedProxy: Role = TEXT("Sim(Other)"); break;
		default: Role = TEXT("None"); break;
	}

	// 디버그 메시지 구성
	// [SERVER] [Auth] CharacterName : TagName is [TRUE/FALSE]
	FString DebugMessage = FString::Printf(TEXT("[%s] [%s] %s : %s is %s"),
		*NetSide, 
		*Role, 
		*InActor->GetName(), 
		*InTag.ToString(), 
		bHasTag ? TEXT("True") : TEXT("False"));

	// 색상 설정 (서버는 빨강/분홍 계열, 클라이언트는 하늘/파랑 계열)
	FColor MsgColor = InActor->HasAuthority() ? FColor::Orange : FColor::Cyan;
	if (bHasTag) MsgColor = bHasTag ? FColor::Green : MsgColor; // 태그가 있으면 초록색으로 강조

	// 화면 출력
	// Key를 Actor의 고유 ID로 설정하면, 매 프레임 같은 위치에 갱신. (메시지 쌓임 방지)
	uint64 Key = (uint64)(InActor->GetUniqueID());
	GEngine->AddOnScreenDebugMessage(Key, 0.1f, MsgColor, DebugMessage);
}

bool UCBAbilitySystemLibrary::IsCombatMode(const AActor* InActor)
{
	return HasGameplayTag(InActor, CBGameplayTags::Status_Combat_InCombat);
}


// [서버] 피격 연출 큐를 피격자에게 실행
void UCBAbilitySystemLibrary::Auth_ExecuteHitCue(AActor* InTargetActor, AActor* InInstigator, const FGameplayTag& InCueTag, const FHitResult& InHitResult)
{
	// 태그를 안 채운 무기는 연출이 없는 것으로 보고 조용히 통과.
	if (!InCueTag.IsValid()) return;

	// 타겟의 ASC 가져오기.
	UAbilitySystemComponent* TargetASC = GetASC(InTargetActor);
	if (!TargetASC) return;

	// 타격 지점을 실어 보냄. 큐가 이 값을 우선 보고 스폰 위치를 잡음
	FGameplayCueParameters CueParams;
	CueParams.Location = InHitResult.ImpactPoint;
	CueParams.Normal = InHitResult.ImpactNormal;

	// 표면 재질. 큐의 Allowed Surface Types 조건이 이 값으로 갈라짐 (살/금속별 연출)
	CueParams.PhysicalMaterial = InHitResult.PhysMaterial;

	// 큐의 스폰 조건이 이 필드를 직접 읽음. ("시전자가 로컬인가" 조건 검사)
	CueParams.Instigator = InInstigator;

	// 서버에서 실행하면 전 클라이언트로 멀티캐스트됨
	TargetASC->ExecuteGameplayCue(InCueTag, CueParams);
}

// [서버] 데미지 GE 를 타겟 하나에게 적용
void UCBAbilitySystemLibrary::Auth_ApplyDamageToTarget(UGameplayAbility& InAbility, TSubclassOf<UGameplayEffect> InDamageEffectClass, float InDamageCoefficient, const FHitResult& InHitResult)
{
	UAbilitySystemComponent* SourceASC = InAbility.GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC) return;

	// 데미지 GE 스펙 만들기 (GE 미지정·생성 실패면 무효 핸들)
	const FGameplayEffectSpecHandle SpecHandle = MakeDamageSpec(InAbility, InDamageEffectClass, InDamageCoefficient);
	if (!SpecHandle.IsValid()) return;

	// 적용 중 어빌리티가 끝나거나 제거되지 않게 잠금 (엔진 TARGETLIST_SCOPE_LOCK 과 같음)
	FScopedTargetListLock TargetListLock(*SourceASC, InAbility);

	// 타겟 하나에게 적용
	Auth_ApplyDamageSpecToTarget(SpecHandle, InHitResult, SourceASC->GetPredictionKeyForNewAction());
}

// 데미지 GE 스펙 생성 (타겟·타격 지점은 적용 시점에 붙음)
FGameplayEffectSpecHandle UCBAbilitySystemLibrary::MakeDamageSpec(const UGameplayAbility& InAbility, TSubclassOf<UGameplayEffect> InDamageEffectClass, float InDamageCoefficient)
{
	if (!InDamageEffectClass) return FGameplayEffectSpecHandle();

	// Spec 만들기 (소스는 시전자 ASC 로 자동 설정됨)
	FGameplayEffectSpecHandle SpecHandle = InAbility.MakeOutgoingGameplayEffectSpec(InDamageEffectClass, InAbility.GetAbilityLevel());
	if (!SpecHandle.IsValid()) return SpecHandle;

	// 데미지 계수 설정 (SetByCaller 등록)
	SpecHandle.Data->SetSetByCallerMagnitude(CBGameplayTags::Data_Damage_Coefficient, InDamageCoefficient);

	return SpecHandle;
}

// [서버] 만들어 둔 데미지 스펙을 타겟 하나에게 적용
void UCBAbilitySystemLibrary::Auth_ApplyDamageSpecToTarget(const FGameplayEffectSpecHandle& InSpecHandle, const FHitResult& InHitResult, FPredictionKey InPredictionKey /* = FPredictionKey() */)
{
	if (!InSpecHandle.IsValid()) return;

	// 시전자가 파괴돼 ASC 가 사라졌으면 적용 불가 (엔진 적용 함수가 ensure 로 막는 상태라 먼저 거름)
	if (!InSpecHandle.Data->GetContext().GetInstigatorAbilitySystemComponent()) return;

	// 이 타겟 하나에게만 GE 적용.
	// 여러 대상을 담은 핸들을 쓰면 대상 수만큼 도는 호출자 루프에서 N² 번 적용됨.
	// 엔진이 적용마다 스펙·컨텍스트를 복사하고 히트 정보(피격 방향 등)를 붙이므로 원본 스펙은 그대로 남음.
	FGameplayAbilityTargetData_SingleTargetHit SingleTarget(InHitResult);
	SingleTarget.ApplyGameplayEffectSpec(*InSpecHandle.Data.Get(), InPredictionKey);
}
