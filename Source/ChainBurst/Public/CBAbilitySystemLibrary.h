#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayPrediction.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Types/CBEnumTypes.h"
#include "CBAbilitySystemLibrary.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
class UWorld;
struct FCollisionQueryParams;

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
	 * ASC의 Status.Movement.Gait.* 태그로 현재 개이트 태그를 판별하는 함수 (Sprint > Walk > 기본 Run 우선순위)
	 * @param InASC 검사할 ASC (nullptr 허용 — 기본 Run 반환)
	 * @return 현재 개이트 태그 (Status.Movement.Gait.Sprint / .Walk / .Run)
	 */
	static FGameplayTag GetCurrentGaitTag(const UAbilitySystemComponent* InASC);

	/**
	 * 현재 이동 상태 기준 개이트별 몽타주 변형 인덱스를 반환하는 함수 (개이트별 몽타주를 가진 액션 공용 — 무기 장착/해제 등)
	 * Idle은 파생 상태 태그(Status.Movement.Idle — LocomotionProcessor가 로컬 미러링)로, 개이트는 Gait.* 태그로 판별.
	 * @param InActor 검사할 캐릭터 액터
	 * @return 개이트 몽타주 인덱스 (0=Idle, 1=Walk, 2=Run/Sprint)
	 */
	static int32 GetGaitMontageIndex(const AActor* InActor);

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

	/**
	 * ASC 소유 액터를 그 ASC 를 쓰는 폰으로 변환하는 함수.
	 * 플레이어는 폰이 아니라 PlayerState 가 들어오기 때문에 소유 액터로 변환해야 함.
	 * @param InActor 환원할 액터 (폰 / 컨트롤러 / PlayerState)
	 * @return 대응하는 폰. 환원할 수 없으면 입력 그대로 반환.
	 */
	static const AActor* ResolveOwningPawn(const AActor* InActor);

	/**
	 * 캐릭터 메시의 소켓(또는 본) 월드 위치를 조회하는 함수. (C++ 전용)
	 * 엔진 GetSocketLocation 은 이름을 못 찾으면 컴포넌트 위치를 반환하므로, 못 찾으면 위치 반환하지 말고 실패로 처리.
	 * @param InActor 조회할 액터 (ACharacter 의 메시에서 찾음)
	 * @param InSocketName 소켓 또는 본 이름
	 * @param OutLocation 찾은 위치 (실패 시 변경하지 않음)
	 * @return 찾았으면 true. 캐릭터가 아니거나 메시가 없거나 이름이 없으면 false
	 */
	static bool FindMeshSocketLocation(const AActor* InActor, FName InSocketName, FVector& OutLocation);

	/**
	 * 두 지점 사이를 벽이 가로막는지 검사하는 함수. (C++ 전용)
	 * Weapon 채널 라인 트레이스 - 캐릭터는 이 채널을 막지 않으므로(캡슐 Ignore·메시 Overlap) 벽·지형(채널 기본 Block)만 막음.
	 * 무기 트레이스·영역 공격이 "공격자 쪽 기준점 → 대상"이 벽에 막혔는지 같은 기준으로 판정할 때 사용.
	 * @param InWorld 트레이스할 월드
	 * @param InFrom 시작점 (공격자 쪽 기준점)
	 * @param InTo 끝점 (대상 위치)
	 * @param InQueryParams 쿼리 파라미터 (공격자 자신 등 제외할 액터)
	 * @return 막혀 있으면 true
	 */
	static bool IsBlockedByWall(const UWorld& InWorld, const FVector& InFrom, const FVector& InTo, const FCollisionQueryParams& InQueryParams);

	/**
	 * [서버] 피격 연출 GameplayCue 를 피격자에게 실행하는 함수. (C++ 전용)
	 * 큐의 대상이 피격자여야 연출이 맞은 쪽에 붙으므로 시전자가 아니라 타겟 ASC 로 실행함.
	 * 타격 지점을 큐 파라미터에 실어, 큐가 액터 원점이 아니라 실제 맞은 자리에서 연출하게 함
	 * @param InTargetActor 피격자
	 * @param InInstigator 때린 액터. 큐의 "시전자가 로컬인가" 스폰 조건에 쓰임
	 * @param InCueTag 실행할 큐 태그. 비어 있으면 아무것도 하지 않음
	 * @param InHitResult 타격 지점·법선·표면 재질의 출처
	 */
	static void Auth_ExecuteHitCue(AActor* InTargetActor, AActor* InInstigator, const FGameplayTag& InCueTag, const FHitResult& InHitResult);

	/**
	 * [서버] 공격 어빌리티의 데미지 GE 를 타겟 하나에게 적용하는 함수. (C++ 전용)
	 * 스펙 생성(MakeDamageSpec) + 타겟 하나에 적용(Auth_ApplyDamageSpecToTarget)을 어빌리티 잠금 안에서 한 번에 수행.
	 * 판정과 적용이 같은 순간인 무기 트레이스·영역 공격용.
	 * @param InAbility 데미지를 주는 어빌리티 (스펙의 소스·레벨 출처)
	 * @param InDamageEffectClass 적용할 데미지 GE. 비어 있으면 아무것도 하지 않음
	 * @param InDamageCoefficient 데미지 계수 (SetByCaller Data.Damage.Coefficient)
	 * @param InHitResult 타겟과 타격 지점. 스펙 컨텍스트에 실려 피격 방향 등에 쓰임
	 */
	static void Auth_ApplyDamageToTarget(UGameplayAbility& InAbility, TSubclassOf<UGameplayEffect> InDamageEffectClass, float InDamageCoefficient, const FHitResult& InHitResult);

	/**
	 * 공격 어빌리티의 데미지 GE 스펙을 만드는 함수. (C++ 전용)
	 * 소스·레벨은 어빌리티에서, 계수는 SetByCaller 로 실음. 타겟·타격 지점은 적용할 때 붙음.
	 * 투사체처럼 적용 시점에 어빌리티가 살아 있다는 보장이 없을 때, 발사 시점에 미리 만들어 넘기는 용도.
	 * @param InAbility 데미지를 주는 어빌리티 (스펙의 소스·레벨 출처)
	 * @param InDamageEffectClass 적용할 데미지 GE. 비어 있으면 무효 핸들 반환
	 * @param InDamageCoefficient 데미지 계수 (SetByCaller Data.Damage.Coefficient)
	 * @return 생성한 스펙 핸들. 실패 시 무효 핸들
	 */
	static FGameplayEffectSpecHandle MakeDamageSpec(const UGameplayAbility& InAbility, TSubclassOf<UGameplayEffect> InDamageEffectClass, float InDamageCoefficient);

	/**
	 * [서버] 만들어 둔 데미지 스펙을 타겟 하나에게 적용하는 함수. (C++ 전용)
	 * 타겟 하나만 담은 타겟 데이터로 적용하므로, 여러 대상을 도는 루프에서 불러도 N² 적용이 생기지 않음.
	 * 엔진이 적용마다 스펙·컨텍스트를 복사해 타격 지점을 붙이므로 원본 스펙은 여러 번 재사용 가능.
	 * 시전자 ASC 가 사라졌으면(시전자 파괴) 적용하지 않음.
	 * @param InSpecHandle 적용할 데미지 스펙 (MakeDamageSpec 결과)
	 * @param InHitResult 타겟과 타격 지점. 스펙 컨텍스트에 실려 피격 방향 등에 쓰임
	 * @param InPredictionKey 예측 키. 어빌리티 밖(투사체 등)에서는 기본값
	 */
	static void Auth_ApplyDamageSpecToTarget(const FGameplayEffectSpecHandle& InSpecHandle, const FHitResult& InHitResult, FPredictionKey InPredictionKey = FPredictionKey());
};
