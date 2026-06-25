#include "CBGameplayTags.h"

namespace CBGameplayTags
{
	/** Input Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Move, "Input.Action.Move", "캐릭터 이동 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Look, "Input.Action.Look", "카메라 회전 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Sprint, "Input.Action.Sprint", "캐릭터 달리기 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Walk, "Input.Action.Walk", "캐릭터 걷기 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Camera_Zoom, "Input.Action.Camera.Zoom", "카메라 줌 인/아웃")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Combat_Attack_Basic, "Input.Action.Combat.Attack.Basic", "기본 공격 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Combat_EquipWeapon, "Input.Action.Combat.EquipWeapon", "무기 장착 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Combat_UnequipWeapon, "Input.Action.Combat.UnequipWeapon", "무기 해제 입력")

	/** Item Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Weapon_Sword, "Item.Weapon.Sword", "무기 태그 - 검")
	
	/** Status Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Combat_InCombat, "Status.Combat.InCombat", "현재 전투 상태")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Movement_Overridden, "Status.Movement.Overridden", "GE에 의해 이동속도가 오버라이드 된 상태")

	/** Event Tags. (애님노티파이 등 태그 이벤트) */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_EquipWeapon, "Event.Combat.EquipWeapon", "무기 장착 이벤트")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_UnequipWeapon, "Event.Combat.UnequipWeapon", "무기 해제 이벤트")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_TraceStart, "Event.Combat.TraceStart", "트레이스 시작 이벤트")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_TraceEnd, "Event.Combat.TraceEnd", "트레이스 종료 이벤트")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Attack_Hit, "Event.Combat.Attack.Hit", "공격 적중 이벤트")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Action_EndAbility, "Event.Action.EndAbility", "액션 어빌리티 종료 이벤트")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Action_CheckInput, "Event.Action.CheckInput", "액션 입력 확인 이벤트")

	/** Movement Tags. (이동 상태) */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Idle, "Movement.Idle", "캐릭터가 가만히 있는 상태")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Walk, "Movement.Walk", "캐릭터가 걷는 상태")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Run, "Movement.Run", "캐릭터가 달리는 상태")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Movement_Sprint, "Movement.Sprint", "캐릭터가 전력 질주하는 상태")

	/** Data Tags. (SetByCaller 전용) */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Movement_Speed, "Data.Movement.Speed", "이동 속도 (SetByCaller 전용)")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "데미지 (SetByCaller 전용)")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage_Coefficient, "Data.Damage.Coefficient", "데미지 계수 (SetByCaller 전용)")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Weapon_AttackPower, "Data.Weapon.AttackPower", "무기 공격력 (SetByCaller 전용)")

	/** Ability Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Combat_Attack_Basic, "Ability.Combat.Attack.Basic", "기본 공격 어빌리티 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Combat_EquipWeapon, "Ability.Combat.EquipWeapon", "무기 장착 어빌리티 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Combat_UnequipWeapon, "Ability.Combat.UnequipWeapon", "무기 해제 어빌리티 태그")

	/** Action Tags. (액션에 맞는 몽타주 재생에 사용) */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action, "Action", "액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat, "Action.Combat", "전투 액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat_Attack, "Action.Combat.Attack", "공격 액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat_Attack_Basic, "Action.Combat.Attack.Basic", "기본 공격 액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat_Block, "Action.Combat.Block", "방어 액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat_Hit, "Action.Combat.Hit", "피격 액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat_Skill, "Action.Combat.Skill", "스킬 액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat_EquipWeapon, "Action.Combat.EquipWeapon", "무기 장착 액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat_UnequipWeapon, "Action.Combat.UnequipWeapon", "무기 해제 액션 태그")

	/** Context Tags. (컨텍스트에 담는 태그) */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Context_Action_IsCombo, "Context.Action.IsCombo", "현재 액션이 콤보인지 여부 (컨텍스트에 담는 태그)")

	/** GameplayCue Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_PlayAction, "GameplayCue.PlayAction", "액션 재생 GameplayCue 태그")

	/** Test Tags */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Test_Tag_1, "Test.Tag.1", "테스트 태그 1")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Test_Tag_2, "Test.Tag.2", "테스트 태그 2")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Test_Tag_3, "Test.Tag.3", "테스트 태그 3")
}
