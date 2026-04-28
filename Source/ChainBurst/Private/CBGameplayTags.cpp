#include "CBGameplayTags.h"

namespace CBGameplayTags
{
	/** [Input] Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Move, "Input.Action.Move", "캐릭터 이동 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Look, "Input.Action.Look", "카메라 회전 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Sprint, "Input.Action.Sprint", "캐릭터 달리기 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Walk, "Input.Action.Walk", "캐릭터 걷기 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Combat_ToggleWeapon, "Input.Action.Combat.ToggleWeapon", "무기 장착/해제 토글 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Camera_Zoom, "Input.Action.Camera.Zoom", "카메라 줌 인/아웃")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Attack_Basic, "Input.Action.Attack.Basic", "기본 공격 입력")

	
	/** [Chaser] Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Chaser_Weapon_Sword, "Chaser.Weapon.Sword", "무기 태그 - 검")

	/** [Chaser] Ability Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Chaser_Ability_Combat_ToggleWeapon, "Chaser.Ability.Combat.ToggleWeapon", "무기 장착/해제 토글 어빌리티 태그")

	
	/** [Shared] Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shared_Status_Combat_InCombat, "Shared.Status.Combat.InCombat", "현재 전투 상태")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shared_Status_Movement_Overridden, "Shared.Status.Movement.Overridden", "GE에 의해 이동속도가 오버라이드 된 상태")

	/** [Shared] Event Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shared_Event_Combat_EquipWeapon, "Shared.Event.Combat.EquipWeapon", "무기 장착 이벤트")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shared_Event_Combat_UnequipWeapon, "Shared.Event.Combat.UnequipWeapon", "무기 해제 이벤트")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shared_Event_Combat_EndAbility, "Shared.Event.Combat.EndAbility", "전투 어빌리티 종료 이벤트")

	/** [Shared] Movement Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shared_Movement_Idle, "Shared.Movement.Idle", "캐릭터가 가만히 있는 상태")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shared_Movement_Walk, "Shared.Movement.Walk", "캐릭터가 걷는 상태")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shared_Movement_Run, "Shared.Movement.Run", "캐릭터가 달리는 상태")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shared_Movement_Sprint, "Shared.Movement.Sprint", "캐릭터가 전력 질주하는 상태")

	/** [Shared] Data Tags. (SetByCaller 전용) */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shared_Data_Movement_Speed, "Shared.Data.Movement.Speed", "이동 속도 (SetByCaller 전용)")

	/** [Shared] Ability Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Shared_Ability_Combat_Attack_Basic, "Shared.Ability.Combat.Attack.Basic", "기본 공격 어빌리티 태그")
}
