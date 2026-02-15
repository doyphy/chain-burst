// project
#include "Types/CBStructTypes.h"
#include "Items/Weapons/CBBaseWeapon.h"

bool FCBWeaponData::IsValid() const
{
		return WeaponTag.IsValid() && WeaponClass != nullptr;
}
