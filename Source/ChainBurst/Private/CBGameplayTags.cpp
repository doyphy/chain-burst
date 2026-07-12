#include "CBGameplayTags.h"

namespace CBGameplayTags
{
	/** Input Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Move, "Input.Action.Move", "캐릭터 이동 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Look, "Input.Action.Look", "카메라 회전 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Sprint, "Input.Action.Sprint", "캐릭터 달리기 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Walk, "Input.Action.Walk", "캐릭터 걷기 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Jump, "Input.Action.Jump", "캐릭터 점프 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Camera_Zoom, "Input.Action.Camera.Zoom", "카메라 줌 인/아웃")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Combat_Attack_Basic, "Input.Action.Combat.Attack.Basic", "기본 공격 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Combat_EquipWeapon, "Input.Action.Combat.EquipWeapon", "무기 장착 입력")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Combat_UnequipWeapon, "Input.Action.Combat.UnequipWeapon", "무기 해제 입력")

	/** Item Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Weapon_Sword, "Item.Weapon.Sword", "무기 태그 - 검")
	
	/** Status Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Combat_InCombat, "Status.Combat.InCombat", "현재 전투 상태")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Movement_Overridden, "Status.Movement.Overridden", "GE에 의해 이동속도가 오버라이드 된 상태")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Status_Movement_Dashing, "Status.Movement.Dashing", "대시 중 상태 (GA_Dash 활성 동안 ActivationOwnedTags로 부여 — 대시 전용 감속 판별에 사용)")

	/** Event Tags. (애님노티파이 등 태그 이벤트) */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_EquipWeapon, "Event.Combat.EquipWeapon", "무기 장착 이벤트")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_UnequipWeapon, "Event.Combat.UnequipWeapon", "무기 해제 이벤트")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_TraceStart, "Event.Combat.TraceStart", "트레이스 시작 이벤트")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_TraceEnd, "Event.Combat.TraceEnd", "트레이스 종료 이벤트")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Attack_Hit, "Event.Combat.Attack.Hit", "공격 적중 이벤트")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_HitReact, "Event.Combat.HitReact", "피격 반응 트리거 이벤트")
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
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_Dash, "Ability.Movement.Dash", "대시 어빌리티 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_Sprint, "Ability.Movement.Sprint", "전력 질주 어빌리티 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_Walk, "Ability.Movement.Walk", "걷기 어빌리티 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Movement_Jump, "Ability.Movement.Jump", "점프 어빌리티 태그")

	/** Action Tags. (액션에 맞는 몽타주 재생에 사용) */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action, "Action", "액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat, "Action.Combat", "전투 액션 태그")
	
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat_Attack, "Action.Combat.Attack", "공격 액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat_Attack_Basic, "Action.Combat.Attack.Basic", "기본 공격 액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat_Block, "Action.Combat.Block", "방어 액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat_HitReact, "Action.Combat.HitReact", "피격 반응 액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat_Skill, "Action.Combat.Skill", "스킬 액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat_EquipWeapon, "Action.Combat.EquipWeapon", "무기 장착 액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Combat_UnequipWeapon, "Action.Combat.UnequipWeapon", "무기 해제 액션 태그")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Action_Movement_Dash, "Action.Movement.Dash", "대시 액션 태그 (몽타주 인덱스 = 8방향 섹터)")

	/** Cooldown Tags. (어빌리티 쿨다운 GE의 GrantedTags에 사용) */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Movement_Dash, "Cooldown.Movement.Dash", "대시 쿨다운 상태 (GE_Cooldown_Dash가 부여)")

	/** Effect Tags. (GE의 동작 의도를 선언하는 태그) */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_HitReact, "Effect.HitReact", "이 GE가 피격 반응을 유발함을 선언하는 태그 (Opt-in)")

	/** GameplayCue Tags. */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_PlayAction, "GameplayCue.PlayAction", "액션 재생 GameplayCue 태그")

	/** Test Tags */
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Test_Tag_1, "Test.Tag.1", "테스트 태그 1")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Test_Tag_2, "Test.Tag.2", "테스트 태그 2")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Test_Tag_3, "Test.Tag.3", "테스트 태그 3")
}
