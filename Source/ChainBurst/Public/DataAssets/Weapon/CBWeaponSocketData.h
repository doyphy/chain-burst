#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/CBStructTypes.h"
#include "CBWeaponSocketData.generated.h"

/**
 * 무기 종류별 소켓 부착 위치를 정의하는 데이터 에셋
 * 캐릭터마다 스켈레톤 소켓 이름이 다를 수 있으므로
 * Chaser / Outlaw / Rogue 각각 별도 에셋으로 관리
 */
UCLASS()
class CHAINBURST_API UCBWeaponSocketData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	/**
	 * WeaponSocketType 에 맞는 소켓 설정을 반환
	 * @param InType    조회할 무기 소켓 타입
	 * @param OutConfig 반환될 소켓 설정
	 * @return 찾으면 true, 못 찾으면 false
	 */
	bool FindSocketConfig(ECBWeaponSocketType InType, FCBWeaponSocketConfig& OutConfig) const;

private:
	/** 무기 소켓 타입별 소켓 설정 배열 */
	UPROPERTY(EditDefaultsOnly, Category = "Socket",
		meta = (TitleProperty = "WeaponSocketType"))
	TArray<FCBWeaponSocketConfig> SocketConfigs;
};
