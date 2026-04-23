#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CBExtensionComponent.generated.h"

/**
 * 컴포넌트의 베이스 클래스
 * 컴포넌트 공용 헬퍼 함수 모음.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CHAINBURST_API UCBExtensionComponent : public UActorComponent
{
	GENERATED_BODY()
	
protected:
	/**
	 * 소유 Pawn을 원하는 타입(T)으로 안전하게 반환하는 템플릿 함수.
	 * T는 APawn 또는 그 자식 타입이어야 하며, 그렇지 않으면 컴파일 에러 발생.
	 * 즉, 소유하고 있는 폰의 참조를 특정 타입으로 캐스팅이 필요할 때 사용.
	 * @tparam T 캐스팅 타입 (APawn 또는 그 자식 타입).
	 * @return APawn 또는 그 자식 타입의 소유 Pawn 객체.
	 */
	template<class T>
	T* GetOwningPawn() const
	{
		// T가 APawn에서 파생된 타입인지 컴파일 타임에 체크
		static_assert(TPointerIsConvertibleFromTo<T, APawn>::Value, "GetOwningPawn 함수의 템플릿 매개 변수 'T'는 APawn 또는 그 자식 타입이어야 합니다.");
		// 소유 액터를 T 타입으로 캐스팅하여 반환 (실패 시 에디터에서 에러 발생)
		return CastChecked<T>(GetOwner());
	}

	/**
	 * 소유 Pawn을 APawn 타입으로 반환하는 함수 (템플릿 함수의 기본 버전).
	 * 즉, 소유하고 있는 폰의 참조를 특정 타입으로 캐스팅이 필요 없을 때 사용.
	 * @return APawn 타입의 소유 Pawn 객체.
	 */
	APawn* GetOwningPawn() const
	{
		return GetOwningPawn<APawn>();
	}

	template<class T>
	T* GetOwningController() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, AController>::Value, "GetOwningController 함수의 템플릿 매개 변수 'T'는 AController 또는 그 자식 타입이어야 합니다.");
		// GetController<T>()는 소유하고 있는 컨트롤러를 특정 타입(T)으로 안전하게 캐스팅해서 반환
		return GetOwningPawn<APawn>()->GetController<T>();
	}
};
