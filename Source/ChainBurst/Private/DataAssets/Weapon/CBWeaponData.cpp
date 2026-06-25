#include "DataAssets/Weapon/CBWeaponData.h"
#include "Items/Weapons/CBBaseWeapon.h"

bool UCBWeaponData::HasValidData()
{
	// 태그가 유효하고, 무기 클래스가 유효하면 true 반환
	if (WeaponTag.IsValid() && WeaponClass)
	{
		return true;
	}
	
	return false;
}
