#include "DataAssets/Weapon/CBWeaponSocketData.h"

bool UCBWeaponSocketData::FindSocketConfig(ECBWeaponCombatType InType,
	FCBWeaponSocketConfig& OutConfig) const
{
	for (const FCBWeaponSocketConfig& Config : SocketConfigs)
	{
		if (Config.WeaponCombatType == InType)
		{
			OutConfig = Config;
			return true;
		}
	}
	return false;
}