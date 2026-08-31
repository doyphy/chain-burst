#pragma once

#include "CoreMinimal.h"
#include "Items/Weapons/CBBaseWeapon.h"
#include "CBBodyWeapon.generated.h"

class USkeletalMeshComponent;

/**
 * 본체 무기 — 무기를 들지 않는 캐릭터(맨손 몬스터 등)의 발톱·이빨을 무기로 취급하는 무기.
 * 무기 메시 대신 오너 캐릭터의 스켈레탈 메시에서 트레이스 소켓을 조회.
 *
 * 설정 방법 (BP 자식):
 *  - WeaponRootSocketName / WeaponTipSocketName 을 오너 캐릭터 스켈레톤의 소켓 또는 본 이름으로 지정
 *    (예: 발톱 = claw_root / claw_tip, 물어뜯기 = jaw_root / jaw_tip)
 *  - WeaponSocketType 은 None (소켓 미점유) — 생성자 기본값
 */
UCLASS()
class CHAINBURST_API ACBBodyWeapon : public ACBBaseWeapon
{
	GENERATED_BODY()

public:
	ACBBodyWeapon();

	//~ Begin ACBBaseWeapon Interface
	/** 오너 캐릭터 메시의 뿌리 소켓 위치 */
	virtual FVector GetWeaponRootLocation() const override;

	/** 오너 캐릭터 메시의 끝 소켓 위치 */
	virtual FVector GetWeaponTipLocation() const override;
	//~ End ACBBaseWeapon Interface

protected:
	/**
	 * 트레이스 소스가 되는 오너 캐릭터의 스켈레탈 메시를 지연 캐싱해 반환.
	 * @return 오너 캐릭터의 메시. 오너가 캐릭터가 아니거나 메시가 없으면 nullptr
	 */
	USkeletalMeshComponent* GetOwnerMesh() const;

private:
	/**
	 * 트레이스 소켓 이름이 오너 메시에 실제로 있는지 최초 1회 검증.
	 * 이름이 틀리면 GetSocketLocation 이 조용히 컴포넌트 원점을 반환해 판정이 사라지므로 경고로 알린다.
	 */
	void ValidateTraceSockets(const USkeletalMeshComponent* InOwnerMesh) const;

	/** 오너 메시 지연 캐시 (const getter 에서 채우므로 mutable) */
	mutable TWeakObjectPtr<USkeletalMeshComponent> CachedOwnerMesh;

	/** 소켓 이름 검증을 이미 수행했는지 여부 (매 틱 경고 방지) */
	mutable bool bValidatedTraceSockets = false;
};
