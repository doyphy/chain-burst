#pragma once

#include "NativeGameplayTags.h"

namespace  CBGameplayTags
{
	/** [Input] Tags. */
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_Move)
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_Look)
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_Sprint)
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_Walk)
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_Combat_ToggleWeapon)
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_Camera_Zoom)
	

	/** [Chaser] Tags. */
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Chaser_Weapon_Sword)
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Chaser_Status_WeaponEquipped)

	
	/** [Shared] Status Tags. */
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Status_Combat_InCombat)
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Status_Movement_Overridden)
	
	/** [Shared] Event Tags. */
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Event_Combat_EquipWeapon)
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Event_Combat_UnequipWeapon)
	
	/** [Shared] Movement Tags. */
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Movement_Idle)
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Movement_Walk)
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Movement_Run)
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Movement_Sprint)

	/** [Shared] Data Tags. (SetByCaller 전용) */
	CHAINBURST_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Shared_Data_Movement_Speed)
}
