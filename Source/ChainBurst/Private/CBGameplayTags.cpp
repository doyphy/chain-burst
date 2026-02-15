#include "CBGameplayTags.h"

namespace CBGameplayTags
{
	/** Input Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Move, "Input.Action.Move", "캐릭터 이동 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Look, "Input.Action.Look", "카메라 회전 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Sprint, "Input.Action.Sprint", "캐릭터 달리기 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Combat_ToggleWeapon, "Input.Action.Combat.ToggleWeapon", "무기 장착/해제 토글 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Camera_Zoom, "Input.Action.Camera.Zoom", "카메라 줌 인/아웃")
	

	/** Chaser Tags. */
	UE_DEFINE_GAMEPLAY_TAG(Chaser_Weapon_Sword, "Chaser.Weapon.Sword")

	/** Shared Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shared_Status_Combat_InCombat, "Shared.Status.Combat.InCombat", "현재 전투 상태")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shared_Event_Combat_EquipWeapon, "Shared.Event.Combat.EquipWeapon", "무기 장착 이벤트")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shared_Event_Combat_UnequipWeapon, "Shared.Event.Combat.UnequipWeapon", "무기 해제 이벤트")
}
