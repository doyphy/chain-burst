#include "DataAssets/Weapon/CBWeaponSocketData.h"

bool UCBWeaponSocketData::FindSocketConfig(ECBWeaponSocketType InType,
	FCBWeaponSocketConfig& OutConfig) const
{
	for (const FCBWeaponSocketConfig& Config : SocketConfigs)
	{
		if (Config.WeaponSocketType == InType)
		{
			OutConfig = Config;
			return true;
		}
	}
	return false;
}