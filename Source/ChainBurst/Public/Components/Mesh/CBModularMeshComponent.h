#pragma once

#include "CoreMinimal.h"
#include "Components/CBExtensionComponent.h"
#include "CBModularMeshComponent.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * 모듈러 캐릭터 외형을 조립하는 공용 컴포넌트.
 * 캐릭터 본체 메시(ACharacter::Mesh)를 '리더'로 삼아, 팔로워 스켈레탈 메시 컴포넌트를
 * 런타임 생성·부착하고 SetLeaderPoseComponent로 리더의 포즈를 그대로 따라가게 함.
 * 생성·부착은 전 인스턴스(서버·클라)가 각자 로컬로 수행하므로 복제 코드가 없음.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CHAINBURST_API UCBModularMeshComponent : public UCBExtensionComponent
{
	GENERATED_BODY()

public:
	/**
	 * 팔로워 메시 목록을 주입하는 세터 (전 인스턴스 공용 경로).
	 * 준비 완료(OnCharacterSystemReady) 이전에 호출해야 반영. 로드아웃 적용 경로에서 사용.
	 */
	void SetFollowerMeshes(const TArray<TObjectPtr<USkeletalMesh>>& InFollowerMeshes);

	/**
	 * 리더 메시 숨김 여부를 주입하는 세터 (전 인스턴스 공용 경로).
	 * 팔로워 목록에 종속되는 값이라 메시와 한 세트로 로드아웃이 관리.
	 */
	FORCEINLINE void SetHideLeaderMesh(bool bInHideLeaderMesh) { bHideLeaderMesh = bInHideLeaderMesh; }

	/** 런타임 생성된 팔로워 컴포넌트 목록 (인덱스는 FollowerMeshes와 대응, 생성 실패분은 nullptr) */
	FORCEINLINE const TArray<TObjectPtr<USkeletalMeshComponent>>& GetFollowerComponents() const { return FollowerComponents; }

protected:
	//~ Begin UActorComponent Interface.
	/** 런타임 생성한 팔로워 컴포넌트를 정리. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface.

	//~ Begin UCBExtensionComponent Interface.
	/** 준비 완료 시 팔로워를 생성해 리더 메시에 부착 (리더 메시 에셋이 적용된 뒤 시점을 보장받기 위함). */
	virtual void OnCharacterSystemReady() override;
	//~ End UCBExtensionComponent Interface.

	/** 리더 메시를 따라갈 팔로워 스켈레탈 메시 목록. 로드아웃에서 주입되는 런타임 캐시 */
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMesh>> FollowerMeshes;

	/**
	 * 리더 메시의 렌더링을 숨길지 여부. 로드아웃에서 주입되는 런타임 캐시.
	 * 켜면 리더 메시를 숨긴 뒤에도 리더 메시의 포즈가 갱신되도록 VisibilityBasedAnimTickOption을 함께 조정.
	 */
	UPROPERTY()
	bool bHideLeaderMesh = false;

private:
	/** 팔로워 컴포넌트 하나를 생성·부착하고 리더 포즈에 연결하는 내부 함수 */
	USkeletalMeshComponent* CreateFollowerComponent(USkeletalMesh* InMesh, USkeletalMeshComponent* InLeader, int32 InIndex);

	/** 런타임 생성한 팔로워 컴포넌트 목록 */
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> FollowerComponents;
};
