// project
#include "Components/Mesh/CBModularMeshComponent.h"
#include "Characters/CBBaseCharacter.h"

// engine
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

// 로드아웃 적용 과정에서 호출
void UCBModularMeshComponent::SetFollowerMeshes(const TArray<TObjectPtr<USkeletalMesh>>& InFollowerMeshes)
{
	FollowerMeshes = InFollowerMeshes;
}

// 캐릭터 시스템 준비가 끝나면 호출되는 함수 (로드아웃 적용이 모두 끝난 후 호출)
void UCBModularMeshComponent::OnCharacterSystemReady()
{
	// 붙일 메시가 없으면 리더 숨김도 하지 않는다 (숨기기만 하면 캐릭터가 통째로 안 보이게 됨)
	if (FollowerMeshes.IsEmpty()) return;

	ACBBaseCharacter* OwnerCharacter = GetOwningPawn<ACBBaseCharacter>();

	// 리더 = 캐릭터 본체 메시. 준비 완료 시점이므로 로드아웃의 스켈레탈 메시가 적용된 상태다.
	USkeletalMeshComponent* Leader = OwnerCharacter->GetMesh();
	if (!Leader || !Leader->GetSkeletalMeshAsset())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 리더 메시가 유효하지 않아 팔로워 메시를 부착하지 않음"), *GetOwner()->GetName());
		return;
	}

	// 인덱스를 유지한 채로 생성해 어느 슬롯이 비었는지 추적할 수 있게 한다
	FollowerComponents.Reserve(FollowerMeshes.Num());
	for (int32 Index = 0; Index < FollowerMeshes.Num(); ++Index)
	{
		FollowerComponents.Add(CreateFollowerComponent(FollowerMeshes[Index], Leader, Index));
	}

	// 리더 숨김은 팔로워를 모두 부착한 뒤에 처리
	if (bHideLeaderMesh)
	{
		// 숨긴 스켈레탈 메시는 기본적으로 애님 틱을 건너뛴다. 그러면 팔로워가 참조할 포즈가 갱신되지 않아
		// 전부 레퍼런스 포즈로 얼어붙으므로, 숨기기 전에 항상 포즈·본을 갱신하도록 바꿔둔다.
		Leader->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		// 팔로워는 리더의 자식이므로 전파를 끄지 않으면 같이 숨겨진다
		Leader->SetVisibility(false, false);
	}
}

USkeletalMeshComponent* UCBModularMeshComponent::CreateFollowerComponent(USkeletalMesh* InMesh, USkeletalMeshComponent* InLeader, int32 InIndex)
{
	// 배열에 빈 슬롯이 있어도 나머지는 정상 부착되도록 건너뛰기만 한다
	if (!InMesh) return nullptr;

	// 디버깅 시 어느 슬롯인지 알 수 있도록 이름에 인덱스를 붙인다
	const FName ComponentName = *FString::Printf(TEXT("CBFollowerMesh_%d"), InIndex);
	USkeletalMeshComponent* Follower = NewObject<USkeletalMeshComponent>(GetOwner(), ComponentName);
	if (!Follower) return nullptr;

	Follower->SetSkeletalMeshAsset(InMesh);
	// 리더에 평면 부착. 팔로워끼리 중첩하지 않는다 (리더 포즈 연쇄는 엔진이 지원하지 않음)
	Follower->SetupAttachment(InLeader);
	Follower->RegisterComponent();

	// 리더 포즈 연결. 본 이름으로 매칭하므로 스켈레톤 호환 등록이 필요 없다.
	// 등록(RegisterComponent) 이후에 호출해야 본 매핑이 올바르게 만들어진다.
	Follower->SetLeaderPoseComponent(InLeader);

	return Follower;
}

void UCBModularMeshComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 런타임 생성한 팔로워 정리
	for (const TObjectPtr<USkeletalMeshComponent>& Follower : FollowerComponents)
	{
		if (Follower)
		{
			Follower->DestroyComponent();
		}
	}
	FollowerComponents.Reset();

	Super::EndPlay(EndPlayReason);
}
