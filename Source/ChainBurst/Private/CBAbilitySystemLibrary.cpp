// project
#include "CBAbilitySystemLibrary.h"
#include "CBGameplayTags.h"
#include "AbilitySystem/CBAbilitySystemComponent.h"

// engine
#include "AbilitySystemInterface.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

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
	if (!InASC) return CBGameplayTags::Movement_Run;

	// Sprint > Walk > 기본 Run 우선순위로 판별
	if (InASC->HasMatchingGameplayTag(CBGameplayTags::Movement_Sprint))
	{
		return CBGameplayTags::Movement_Sprint;
	}
	if (InASC->HasMatchingGameplayTag(CBGameplayTags::Movement_Walk))
	{
		return CBGameplayTags::Movement_Walk;
	}
	return CBGameplayTags::Movement_Run;
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
