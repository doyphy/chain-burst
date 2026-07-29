# -*- coding: utf-8 -*-
"""스켈레탈 메시 본 구조 비교 도구.

호환 스켈레톤(Compatible Skeletons)을 적용하기 전에, 두 메시의 본 구조가
실제로 호환되는지 검사한다. 호환 스켈레톤은 구조를 검증하지 않는
화이트리스트이므로, 어긋난 부분은 에러 없이 조용히 잘못된 결과를 낸다.

사용법:
    1. 콘텐츠 브라우저에서 검사할 스켈레탈 메시(B)를 선택한다.
       선택하지 않으려면 아래 B 상수에 경로를 직접 적는다.
    2. 아래 방법 중 하나로 실행한다.
       - Tools > Execute Python Script 로 이 파일을 지정
       - 파이썬 콘솔에서:
         exec(open(r'D:\\UnrealProjects\\ChainBurst\\Tools\\skeleton_diff.py',
                   encoding='utf-8').read())
    3. Output Log 에서 결과를 확인한다. (LogPython 카테고리)

진단 항목:
    (1) 이름 차집합 - B에 없는 본의 애니메이션 트랙은 조용히 버려진다.
                      [!] 표시는 게임플레이가 의존할 가능성이 높은 본.
    (2) 부모 비교   - 이름은 같은데 부모가 다르면 로컬 트랜스폼이 잘못된
                      부모 공간에 적용되어 에러 없이 망가진다.
                      하나라도 나오면 호환 스켈레톤 대신 IK 리타게터가 필요하다.
    (3) 회전 차이   - 미구현. get_socket_transform 의 실제 동작을 검증한 뒤 추가한다.
                      레퍼런스 포즈 로컬 회전이 수십 도 이상 어긋나면 축 규약이
                      다른 것이므로 호환 스켈레톤을 쓸 수 없다.

참고: UE4/UE5 스켈레톤 판별은 spine_05, neck_02, *_metacarpal_* 유무로 갈린다.
      셋 다 없으면 UE4 계열이고, 이 경우 (2)에서 부모 불일치가 대량 발생한다.
"""

import unreal

# 소스: 애니메이션이 묶여 있는 스켈레톤의 메시
A = '/Game/_ThirdParty/Sword_Animations/Character/Mannequins/Character/Meshes/SKM_Manny_Sword'

# 타겟: 그 애니메이션을 재생시키려는 새 메시.
# 비워두면 콘텐츠 브라우저에서 선택한 메시를 사용한다(권장).
B = ''

# 순회 시작점. Epic 계열 스켈레톤은 모두 'root'.
ROOT_BONE = 'root'

# 게임플레이가 의존할 가능성이 높은 본 이름 키워드.
# 치마/머리카락 같은 장식 본은 없어도 되지만, 이런 본이 빠지면 기능이 죽는다.
RISKY_KEYWORDS = ('weapon', 'ik_', 'holder', 'prop', 'attach')

SEPARATOR = '=' * 60


def collect_bones(mesh, root=ROOT_BONE):
    """root 부터 자식을 따라 내려가며 모든 본 이름을 수집한다.

    SkeletalMesh 에는 본 목록을 통째로 주는 API 가 없어서 직접 순회한다.
    get_bone_children 는 직계 자식만 반환하므로 스택으로 깊이 우선 탐색한다.
    """
    found, stack = [], [root]
    while stack:
        bone = str(stack.pop())
        found.append(bone)
        stack.extend(str(child) for child in mesh.get_bone_children(bone))

    if len(found) <= 1:
        raise RuntimeError("루트 본 '%s' 을 찾을 수 없음 - ROOT_BONE 확인" % root)
    return found


def is_risky(bone):
    """게임플레이가 의존할 가능성이 높은 본인지 이름으로 추정한다.

    어디까지나 휴리스틱이다. 확정 판정이 아니라 우선 살펴볼 대상을 좁히는 용도.
    """
    low = bone.lower()
    return any(keyword in low for keyword in RISKY_KEYWORDS)


def compare_parents(a, b, common):
    """이름은 같은데 부모가 다른 본을 찾는다.

    하나라도 나오면 호환 스켈레톤은 위험하다. 애니메이션 트랙의 로컬
    트랜스폼이 잘못된 부모 공간에 적용되어 에러 없이 망가진다.

    get_bone_parent 는 루트일 때와 본이 없을 때 모두 NAME_None 을 반환하지만,
    여기서는 양쪽에 모두 존재하는 공통 본만 조회하므로 안전하다.
    """
    mismatched = []
    for bone in common:
        parent_a = str(a.get_bone_parent(bone))
        parent_b = str(b.get_bone_parent(bone))
        if parent_a != parent_b:
            mismatched.append((bone, parent_a, parent_b))
    return mismatched


def asset_name(path):
    """에셋 경로에서 이름만 뽑는다."""
    return path.rsplit('/', 1)[1]


def pick_target_from_selection():
    """콘텐츠 브라우저에서 선택한 스켈레탈 메시를 반환한다. 없으면 None."""
    selected = unreal.EditorUtilityLibrary.get_selected_assets()
    meshes = [x for x in selected if isinstance(x, unreal.SkeletalMesh)]
    if len(meshes) == 1:
        return meshes[0]
    if len(meshes) > 1:
        unreal.log_warning('스켈레탈 메시를 하나만 선택하세요 (%d개 선택됨)' % len(meshes))
    return None


def run():
    a = unreal.EditorAssetLibrary.load_asset(A)
    b = pick_target_from_selection()
    if b is None:
        if not B:
            unreal.log_error('콘텐츠 브라우저에서 검사할 스켈레탈 메시를 선택하세요. '
                             '(또는 스크립트의 B 상수에 경로를 지정)')
            return
        b = unreal.EditorAssetLibrary.load_asset(B)   # 선택 없으면 상수로 폴백
        unreal.log('선택된 메시가 없어 기본 경로 사용: %s' % asset_name(B))
    if not a or not b:
        unreal.log_error('에셋 로드 실패')
        return

    try:
        bones_a = collect_bones(a)
        bones_b = collect_bones(b)
    except RuntimeError as e:
        unreal.log_error(str(e))
        return

    set_a, set_b = set(bones_a), set(bones_b)
    only_a = sorted(set_a - set_b)
    only_b = sorted(set_b - set_a)
    common = sorted(set_a & set_b)

    unreal.log(SEPARATOR)
    unreal.log('A (소스) : %s  -  %d bones' % (a.get_name(), len(bones_a)))
    unreal.log('B (타겟) : %s  -  %d bones' % (b.get_name(), len(bones_b)))

    # --- 진단 1. 이름 차집합 ---
    risky = [x for x in only_a if is_risky(x)]
    unreal.log(SEPARATOR)
    unreal.log('[1] B에 없는 본 : %d개 (그 중 주의 대상 %d개)' % (len(only_a), len(risky)))
    unreal.log('    이 본들의 애니메이션 트랙은 경고 없이 버려집니다.')
    for bone in only_a:
        unreal.log('    %s %s' % ('[!]' if is_risky(bone) else '   ', bone))

    unreal.log('')
    unreal.log('[1] A에 없는 본 : %d개 (무해)' % len(only_b))
    for bone in only_b:
        unreal.log('        %s' % bone)

    # --- 진단 2. 부모 비교 ---
    mismatched = compare_parents(a, b, common)
    unreal.log(SEPARATOR)
    unreal.log('[2] 부모 불일치 : %d개 (공통 본 %d개 중)' % (len(mismatched), len(common)))
    for bone, parent_a, parent_b in mismatched:
        unreal.log('    %s : A의 부모=%s / B의 부모=%s' % (bone, parent_a, parent_b))

    # --- 판정 ---
    unreal.log(SEPARATOR)
    if mismatched:
        unreal.log_error(
            '판정: 호환 스켈레톤 위험 - 부모 불일치 %d개. IK 리타게터를 검토하세요.'
            % len(mismatched))
    elif risky:
        unreal.log_warning(
            '판정: 조건부 사용 가능 - 주의 대상 본 %d개가 B에 없습니다. '
            '해당 본에 의존하는 기능(무기 부착 등)은 동작하지 않습니다.' % len(risky))
    elif only_a:
        unreal.log_warning(
            '판정: 사용 가능 - B에 없는 본 %d개는 장식용으로 보입니다.' % len(only_a))
    else:
        unreal.log('판정: 본 구조 일치 - 호환 스켈레톤 안전')
    unreal.log(SEPARATOR)


run()
