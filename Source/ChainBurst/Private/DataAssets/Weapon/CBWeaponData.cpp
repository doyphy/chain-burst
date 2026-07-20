#include "DataAssets/Weapon/CBWeaponData.h"
#include "Items/Weapons/CBBaseWeapon.h"

bool UCBWeaponData::HasValidData()
{
	// 무기 클래스가 유효하면 true 반환
	// (WeaponSocketType 은 None 이 소환형 무기의 정상 값이므로 유효 조건에서 제외)
	return WeaponClass != nullptr;
}
