#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types/CBEnumTypes.h"
#include "CBAbilitySystemLibrary.generated.h"

class UAbilitySystemComponent;

/**
 * 어빌리티 관련 공용 함수 모음.
 * 액터의 ASC에서 태그 검사, 상태 검사 등 자주 쓰이는 기능들을 구현.
 * 컴포넌트, 애님 인스턴스 등 ASC에 자주 접근해야 하는 곳에서 이 라이브러리를 통해 간편하게 ASC 기능을 사용할 수 있도록 함.
 */
UCLASS()
class CHAINBURST_API UCBAbilitySystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * 대상 액터의 ASC 에서 전투 상태 태그를 검사해 반환
	 * @param InActor 검사할 액터 (ACBBaseCharacter 또는 그 자식 클래스 권장)
	 * @return 전투 상태면 true
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|Combat", meta = (DefaultToSelf = "InActor"))
	static bool IsCombatMode(const AActor* InActor);

	/**
	 * 대상 액터의 ASC 에서 특정 GameplayTag 보유 여부를 반환
	 * @param InActor    검사할 액터
	 * @param InTag      검사할 태그
	 * @return 태그 보유 시 true
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|AbilitySystem", meta = (DefaultToSelf = "InActor"))
	static bool HasGameplayTag(const AActor* InActor, const FGameplayTag& InTag);

	/**
	 * 대상 액터의 CB ASC를 안전하게 가져오는 함수
	 * @param InActor ASC를 가져올 액터 (ACBBaseCharacter 또는 ACBPlayerState 권장)
	 * @return InActor의 ASC를 UCBAbilitySystemComponent 타입으로 안전하게 반환. 실패 시 nullptr 반환.
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|AbilitySystem", meta = (DefaultToSelf = "InActor"))
	static UCBAbilitySystemComponent* GetSafeCBASC(const AActor* InActor);

	/**
	 * ASC 를 지연 캐싱하여 가져오는 함수 (C++ 전용)
	 * @param InActor ASC를 가져올 액터 (ACBBaseCharacter 또는 ACBPlayerState 권장)
	 * @param OutASC 캐싱할 포인터 변수 (참조)
	 * @return 성공적으로 가져왔거나 이미 유효하면 true 반환.
	 */
	static bool GetCBCachedASC(const AActor* InActor, TWeakObjectPtr<UCBAbilitySystemComponent>& OutASC);
	
	/**
	 * 타겟 액터에게 이펙트 적용하는 함수 (C++ 전용)
	 * @param TargetActor 적용할 타겟 액터
	 * @param InSpecHandle 적용할 이펙트 핸들
	 * @return 적용된 이펙트의 핸들 반환. 적용 실패 시 유효하지 않은 핸들 반환.
	 */
	static FActiveGameplayEffectHandle NativeApplyEffectSpecHandleToTarget(AActor* TargetActor, const FGameplayEffectSpecHandle& InSpecHandle);

	/**
	 * 타겟 액터에 이펙트 적용하는 함수 (블루프린트 전용)
	 * @param TargetActor 적용할 타겟 액터
	 * @param InSpecHandle 적용할 이펙트 핸들
	 * @param OutSuccess 적용 성공 여부 반환 (참조)
	 * @return 적용된 이펙트의 핸들 반환. 적용 실패 시 유효하지 않은 핸들 반환.
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|AbilitySystem", meta = (DefaultToSelf = "TargetActor", DisplayName = "Apply Effect Spec Handle To Target", ExpandEnumAsExecs = "OutSuccessType"))
	static FActiveGameplayEffectHandle BP_ApplyEffectSpecHandleToTarget(AActor* TargetActor, const FGameplayEffectSpecHandle& InSpecHandle, ECBSuccessType& OutSuccessType);

	/**
	 * 이펙트 스펙 핸들을 생성하는 함수 (C++ 전용)
	 * @param GEClass 생성할 GameplayEffect 클래스
	 * @param SourceActor 소스 액터 (이펙트의 가해자 정보로 설정됨)
	 * @param Level 생성할 이펙트 레벨 (기본값 1.0f)
	 * @return 생성한 이펙트 스펙 핸들 반환. 생성 실패 시 유효하지 않은 스펙 핸들 반환. 
	 */
	static FGameplayEffectSpecHandle NativeMakeEffectSpecHandle(TSubclassOf<UGameplayEffect> GEClass, AActor* SourceActor, float Level = 1.0f);

	/**
	 * 이펙트 스펙 핸들을 생성하는 함수 (블루프린트 전용)
	 * @param GEClass 생성할 GameplayEffect 클래스
	 * @param SourceActor 소스 액터 (이펙트의 가해자 정보로 설정됨)
	 * @param Level 생성할 이펙트 레벨 (기본값 1.0f)
	 * @param OutSuccessType 생성 성공 여부 반환 (참조)
	 * @return 생성된 이펙트 스펙 핸들 반환. 생성 실패 시 유효하지 않은 스펙 핸들 반환
	 */
	UFUNCTION(BlueprintCallable, Category = "ChainBurst|AbilitySystem", meta = (DisplayName = "Make Effect Spec Handle", DefaultToSelf = "SourceActor", ExpandEnumAsExecs = "OutSuccessType"))
	static FGameplayEffectSpecHandle BP_MakeEffectSpecHandle(TSubclassOf<UGameplayEffect> GEClass, AActor* SourceActor, float Level, ECBSuccessType& OutSuccessType);

	/**
	 * 태그 상태를 디버그 메시지로 출력하는 함수
	 * @param InActor 검사할 액터
	 * @param InTag 검사할 태그
	 */
	static void DrawTagDebugMessage(const AActor* InActor, const FGameplayTag& InTag);
	
	/**
	 * 액터로부터 ASC 를 가져오는 함수 (C++ 전용)
	 * @param InActor ASC를 가져올 액터
	 * @return ASC 반환. 실패 시 nullptr 반환.
	 */
	static UAbilitySystemComponent* GetASC(const AActor* InActor);
};
