// project
#include "Components/Mesh/CBModularMeshComponent.h"
#include "Characters/CBBaseCharacter.h"
#include "AssetManager/CBAssetManager.h"
#include "CBGameplayTags.h"
#include "DataAssets/Cosmetic/CBCosmeticCatalog.h"

// engine
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

UCBModularMeshComponent::UCBModularMeshComponent()
{
	// 슬롯을 배열 인덱스로 쓰므로 크기를 미리 고정 (값이 없는 슬롯은 nullptr = 벗은 상태)
	CosmeticMeshes.Init(nullptr, static_cast<int32>(ECBCosmeticSlot::MAX));

	// 슬롯별로 요청한 파츠 태그도 슬롯 인덱스를 쓰므로 같은 크기로 확보
	RequestedParts.Init(FGameplayTag(), static_cast<int32>(ECBCosmeticSlot::MAX));

	// 실제 착용 태그도 같은 크기로 확보 (기본 의상이 적용되기 전에는 빈 태그)
	CurrentPartIds.Init(FGameplayTag(), static_cast<int32>(ECBCosmeticSlot::MAX));
}

// [로드아웃] 로드아웃에서 로드아웃의 Skin Mesh를 적용할 때 호출
void UCBModularMeshComponent::SetSkinMeshes(const TArray<TObjectPtr<USkeletalMesh>>& InSkinMeshes)
{
	SkinMeshes = InSkinMeshes;
}

// [로드아웃] 부위별 기본 의상 파츠 태그를 주입할 때 호출. 메시 로드는 PreloadDefaultCosmetics 가 수행
void UCBModularMeshComponent::SetDefaultCosmeticIds(const TMap<ECBCosmeticSlot, FGameplayTag>& InDefaultCosmeticIds)
{
	DefaultCosmeticIds = InDefaultCosmeticIds;
}

// [로드아웃] 기본 의상 메시를 미리 로드. 캐릭터 준비 완료가 이 로드를 기다림
void UCBModularMeshComponent::PreloadDefaultCosmetics(TFunction<void()> OnComplete)
{
	// 로드할 것이 없어도 콜백은 반드시 호출해야 함 (호출부가 준비 완료를 기다리고 있음)
	if (!CosmeticCatalog || DefaultCosmeticIds.IsEmpty())
	{
		// 입힐 것이 없어도 착용 파츠는 "벗음"으로 적용 해야 함 (빈 태그로 두면 순회가 현재 착용 파츠를 찾지 못함)
		ApplyDefaultCosmetics();

		OnComplete();
		return;
	}

	// 부위별 기본 의상 파츠를 저장하기 위해 경로 배열을 준비.
	TArray<FSoftObjectPath> PathsToLoad;
	PathsToLoad.Reserve(DefaultCosmeticIds.Num());

	// 부위별 기본 의상 파츠를 카탈로그에서 찾아 경로를 저장. 없는 파츠는 경고만 남기고 무시
	for (const TPair<ECBCosmeticSlot, FGameplayTag>& Pair : DefaultCosmeticIds)
	{
		// 벗기 태그는 카탈로그에 없으므로 무시. 벗기 태그는 ApplyDefaultCosmetics()에서 처리됨
		if (Pair.Value == CBGameplayTags::Item_Cosmetic_None) continue;

		const FCBCosmeticPart* FoundPart = CosmeticCatalog->FindPart(Pair.Key, Pair.Value);
		if (!FoundPart)
		{
			// 로드아웃이 지정한 기본 의상이 카탈로그에 없음. 그 부위는 아무것도 입지 않은 상태가 됨
			UE_LOG(LogTemp, Warning, TEXT("[%s] 기본 의상 파츠를 카탈로그에서 찾지 못함: %s"),
				*GetNameSafe(GetOwner()), *Pair.Value.ToString());
			continue;
		}

		PathsToLoad.Add(FoundPart->Mesh.ToSoftObjectPath());
	}

	// 로드 중 소유 액터가 파괴될 수 있으므로 약한 참조로 캡처
	TWeakObjectPtr<UCBModularMeshComponent> WeakThis(this);

	// 부위별 기본 의상 파츠 메시를 비동기 로드. 로드가 끝나면 ApplyDefaultCosmetics() 호출
	UCBAssetManager::Get().LoadAssetsAsync(PathsToLoad, [WeakThis, OnComplete]()
	{
		// 로드가 끝나면 기본 의상을 각 슬롯에 반영. 컴포넌트가 사라졌으면 그냥 무시
		if (UCBModularMeshComponent* ModularMesh = WeakThis.Get())
		{
			ModularMesh->ApplyDefaultCosmetics();
		}

		// 컴포넌트가 사라졌어도 호출부가 영구히 잠기지 않도록 콜백은 항상 실행
		OnComplete();
	});
}

// 로드가 끝난 기본 의상을 각 슬롯에 반영
void UCBModularMeshComponent::ApplyDefaultCosmetics()
{
	// 기본 의상이 지정된 슬롯만이 아니라 슬롯 전체를 순회함.
	for (int32 SlotIndex = 0; SlotIndex < CurrentPartIds.Num(); ++SlotIndex)
	{
		// enum 을 int32 로 변환하여 배열 인덱스로 사용
		const ECBCosmeticSlot Slot = static_cast<ECBCosmeticSlot>(SlotIndex);

		// 로드아웃에서 설정한 기본 의상 파츠 태그를 가져옴. 없으면 nullptr
		const FGameplayTag* DefaultId = DefaultCosmeticIds.Find(Slot);

		// 카탈로그에 등록된 파츠 가져오기
		const FCBCosmeticPart* FoundPart =
			(DefaultId && CosmeticCatalog) ? CosmeticCatalog->FindPart(Slot, *DefaultId) : nullptr;

		// 동록된 파츠가 있다면
		if (FoundPart)
		{
			// 런타임 의상 교체
			SetCosmeticForSlot(Slot, FoundPart->Mesh.Get());
			// 현재 착용 태그 기록
			SetCurrentPartId(Slot, *DefaultId);
			continue;
		}

		// 등록된 파츠가 없으면 그 부위는 벗은 상태로 처리
		SetCurrentPartId(Slot, CBGameplayTags::Item_Cosmetic_None);
	}
}

// 슬롯의 현재 착용 태그 기록
void UCBModularMeshComponent::SetCurrentPartId(ECBCosmeticSlot InSlot, const FGameplayTag& InPartId)
{
	const int32 SlotIndex = static_cast<int32>(InSlot);
	if (!CurrentPartIds.IsValidIndex(SlotIndex)) return;

	CurrentPartIds[SlotIndex] = InPartId;
}

// 그 부위가 지금 입고 있는 파츠 태그
FGameplayTag UCBModularMeshComponent::GetCurrentCosmeticPartId(ECBCosmeticSlot InSlot) const
{
	const int32 SlotIndex = static_cast<int32>(InSlot);
	if (!CurrentPartIds.IsValidIndex(SlotIndex)) return FGameplayTag();

	return CurrentPartIds[SlotIndex];
}

// 현재 착용 파츠 기준 다음 파츠 (순회용)
FGameplayTag UCBModularMeshComponent::GetNextCosmeticPartId(ECBCosmeticSlot InSlot) const
{
	return GetCosmeticPartIdByStep(InSlot, 1);
}

// 현재 착용 파츠 기준 이전 파츠 (순회용)
FGameplayTag UCBModularMeshComponent::GetPreviousCosmeticPartId(ECBCosmeticSlot InSlot) const
{
	return GetCosmeticPartIdByStep(InSlot, -1);
}

// 다음/이전 공용 계산
FGameplayTag UCBModularMeshComponent::GetCosmeticPartIdByStep(ECBCosmeticSlot InSlot, int32 InStep) const
{
	if (!CosmeticCatalog) return FGameplayTag();

	// 그 부위에 등록된 파츠 태그를 모두 가져옴
	TArray<FGameplayTag> PartIds;
	CosmeticCatalog->GetPartIdsForSlot(InSlot, PartIds);

	// 등록된 파츠가 없으면 순회할 것이 없음 (벗기만 남으면 이동의 의미가 없음)
	if (PartIds.IsEmpty()) return FGameplayTag();

	// 순환 목록 = [벗기] + 등록 순서. 순회만으로 벗을 수 있도록 맨 앞에 벗기를 둠
	TArray<FGameplayTag> Cycle;
	Cycle.Reserve(PartIds.Num() + 1);
	Cycle.Add(CBGameplayTags::Item_Cosmetic_None);
	Cycle.Append(PartIds);

	// [예외 처리] 현재 착용 태그가 목록에 없으면(카탈로그에서 제거된 파츠 등) 맨 앞부터 시작
	const int32 CurrentIndex = Cycle.IndexOfByKey(GetCurrentCosmeticPartId(InSlot));
	if (CurrentIndex == INDEX_NONE) return Cycle[0];

	// 다음/이전 인덱스 계산. 음수 스텝도 가능하도록 모듈로 연산. (C++에서 음수 % 양수는 음수이므로 Cycle.Num()을 더해 양수로 만듦)
	const int32 NextIndex = (CurrentIndex + InStep + Cycle.Num()) % Cycle.Num();

	// 다음/이전 파츠 태그 반환.
	return Cycle[NextIndex];
}

// [런타임 교체] 로비 의상 교체 등 런타임에서 슬롯 하나의 메시를 교체할 때 호출
void UCBModularMeshComponent::SetCosmeticForSlot(ECBCosmeticSlot InSlot, USkeletalMesh* InMesh)
{
	// enum 을 int32 로 변환하여 배열 인덱스로 사용
	const int32 SlotIndex = static_cast<int32>(InSlot);

	// 팔로워 컴포넌트가 생성되기 전이라면 CosmeticMeshes 에 메시를 저장
	if (!CosmeticComponents.IsValidIndex(SlotIndex))
	{
		// MAX 같은 유효하지 않은 슬롯 값이 들어오면 무시
		if (!CosmeticMeshes.IsValidIndex(SlotIndex)) return;
		// 컴포넌트 생성 시 이 값으로 컴포넌트의 메시를 설정
		CosmeticMeshes[SlotIndex] = InMesh;
		return;
	}

	// 슬롯에 맞는 팔로워 컴포넌트 가져오기
	USkeletalMeshComponent* Follower = CosmeticComponents[SlotIndex];
	if (!Follower) return;

	// 팔로워 컴포넌트의 메시 설정
	// nullptr 을 넣으면 렌더링할 메시가 없어져 그 부위를 벗은 상태가 됨
	Follower->SetSkeletalMeshAsset(InMesh);

	// 메시가 바뀌면 본 구성도 바뀌므로 리더 포즈를 다시 연결함
	if (InMesh)
	{
		Follower->SetLeaderPoseComponent(CachedLeader);
	}
}

// [로드아웃] 로드아웃에서 카탈로그를 주입할 때 호출
void UCBModularMeshComponent::SetCosmeticCatalog(UCBCosmeticCatalog* InCosmeticCatalog)
{
	CosmeticCatalog = InCosmeticCatalog;
}

// [런타임 교체][비동기 로드] 파츠 태그로 의상을 교체할 때 호출
void UCBModularMeshComponent::RequestCosmeticPart(ECBCosmeticSlot InSlot, const FGameplayTag& InPartId)
{
	// enum 을 int32 로 변환하여 배열 인덱스로 사용
	const int32 SlotIndex = static_cast<int32>(InSlot);

	// MAX 같은 유효하지 않은 슬롯 값이 들어오면 무시
	if (!RequestedParts.IsValidIndex(SlotIndex)) return;

	// 로드가 끝났을 때 이 값과 다르면 그 사이 다른 요청이 온 것으로 판별함
	RequestedParts[SlotIndex] = InPartId;

	// 벗기 태그 = 로드할 것이 없으므로 즉시 반영
	if (InPartId == CBGameplayTags::Item_Cosmetic_None)
	{
		// 해당 슬롯의 메시를 비워 그 부위를 벗김
		SetCosmeticForSlot(InSlot, nullptr);
		SetCurrentPartId(InSlot, InPartId);
		return;
	}

	// 빈 태그 = 선택 안 함. 기본 의상을 그대로 둬야 하므로 아무것도 하지 않음
	if (!InPartId.IsValid()) return;

	if (!CosmeticCatalog)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 의상 카탈로그가 없어 파츠를 적용하지 못함"), *GetNameSafe(GetOwner()));
		return;
	}

	// 그 부위에 등록된 파츠인지 조회 (부위는 카탈로그의 맵 키가 정함)
	const FCBCosmeticPart* FoundPart = CosmeticCatalog->FindPart(InSlot, InPartId);

	// 그 부위에 등록되지 않은 파츠면 거부
	if (!FoundPart)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 카탈로그에 없거나 부위가 다른 파츠 요청: %s"), *GetNameSafe(GetOwner()), *InPartId.ToString());
		return;
	}

	// 로드가 끝나기 전에 소유 액터가 파괴될 수 있으므로 약한 참조로 캡처
	// 로드 중 캐릭터가 파괴되면 그냥 무시
	TWeakObjectPtr<UCBModularMeshComponent> WeakThis(this);
	
	// 파츠 메시를 비동기 로드. 로드가 끝나면 HandleCosmeticPartLoaded() 호출
	UCBAssetManager::Get().LoadAssetAsync<USkeletalMesh>(FoundPart->Mesh,
		[WeakThis, InSlot, InPartId](USkeletalMesh* LoadedMesh)
		{
			if (UCBModularMeshComponent* ModularMesh = WeakThis.Get())
			{
				ModularMesh->HandleCosmeticPartLoaded(InSlot, InPartId, LoadedMesh);
			}
		});
}

// 서버가 클라이언트의 의상 교체 요청을 검증할 때 호출
bool UCBModularMeshComponent::IsValidCosmeticPart(ECBCosmeticSlot InSlot, const FGameplayTag& InPartId) const
{
	// 벗기는 카탈로그와 무관하게 항상 허용 (어느 부위든 벗을 수 있음)
	if (InPartId == CBGameplayTags::Item_Cosmetic_None) return true;

	// 카탈로그가 주입되지 않았으면 판정할 근거가 없으므로 거부
	return CosmeticCatalog && CosmeticCatalog->IsValidPartForSlot(InSlot, InPartId);
}

// 파츠 메시 비동기 로드가 끝나면 호출
void UCBModularMeshComponent::HandleCosmeticPartLoaded(ECBCosmeticSlot InSlot, const FGameplayTag& InPartId, USkeletalMesh* InLoadedMesh)
{
	const int32 SlotIndex = static_cast<int32>(InSlot);
	if (!RequestedParts.IsValidIndex(SlotIndex)) return;

	// 로드 중에 다른 파츠를 요청했다면 종료 (이미 다른 파츠로 건너뛰었기에 메시 교체할 필요 없음)
	if (RequestedParts[SlotIndex] != InPartId) return;

	// 해당 슬롯의 메시를 교체
	SetCosmeticForSlot(InSlot, InLoadedMesh);
	SetCurrentPartId(InSlot, InPartId);
}

// 캐릭터 시스템 준비가 끝나면 호출되는 함수 (로드아웃 적용이 모두 끝난 후 호출)
// 로드아웃에서 주입된 메시를 기반으로 팔로워 컴포넌트를 생성하고 리더에 부착함
void UCBModularMeshComponent::OnCharacterSystemReady()
{
	ACBBaseCharacter* OwnerCharacter = GetOwningPawn<ACBBaseCharacter>();
	if (!OwnerCharacter) return;

	// 리더 = 캐릭터 본체 메시. 준비 완료 시점이므로 로드아웃의 스켈레탈 메시가 적용된 상태.
	USkeletalMeshComponent* Leader = OwnerCharacter->GetMesh();
	if (!Leader || !Leader->GetSkeletalMeshAsset())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 리더 메시가 유효하지 않아 팔로워 메시를 부착하지 않음"), *GetOwner()->GetName());
		return;
	}

	// 런타임 교체에서 리더 포즈를 다시 연결해야 하므로 캐싱해 둠
	CachedLeader = Leader;

	// 피부 → 의상 순으로 팔로워 컴포넌트 생성. 팔로워 컴포넌트는 리더에 부착됨.
	BuildFollowers(SkinMeshes, Leader, TEXT("CBSkinMesh"), false, SkinComponents);
	BuildFollowers(CosmeticMeshes, Leader, TEXT("CBCosmeticMesh"), true, CosmeticComponents);

	// 실제로 보이는 팔로워가 하나도 없는데 리더까지 숨기면 캐릭터가 통째로 안 보이므로, 그럴 때는 숨기지 않음
	auto HasAnyMesh = [](const TArray<TObjectPtr<USkeletalMesh>>& InMeshes)
	{
		return InMeshes.ContainsByPredicate([](const TObjectPtr<USkeletalMesh>& InMesh) { return InMesh != nullptr; });
	};

	// 리더 표시 여부 결정은 팔로워를 모두 부착한 뒤에 처리
	const bool bShouldHideLeader = bHideLeaderMesh && (HasAnyMesh(SkinMeshes) || HasAnyMesh(CosmeticMeshes));

	if (bShouldHideLeader)
	{
		// 리더 스켈레탈 메시 애님 틱을 활성화.
		// 숨김 스켈레탈 메시는 기본적으로 애님 틱이 꺼지므로 팔로워가 리더 포즈를 따라가지 못함.
		Leader->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	// 리더 메시 숨김 여부 적용.
	// 팔로워는 리더의 자식이므로 자식은 숨기지 않도록 설정.
	Leader->SetVisibility(!bShouldHideLeader, false);
}

// 피부·의상 조립에서 공통으로 호출
void UCBModularMeshComponent::BuildFollowers(const TArray<TObjectPtr<USkeletalMesh>>& InMeshes, USkeletalMeshComponent* InLeader, const FString& InNamePrefix, bool bInKeepEmptySlots, TArray<TObjectPtr<USkeletalMeshComponent>>& OutComponents)
{
	// 컴포넌트 배열 초기화
	OutComponents.Reset();
	// 컴포넌트 배열 메모리 예약 (실제로 원소를 만들지 않고 공간만 미리 잡아둘 뿐, 배열의 개수는 그대로 0임)
	OutComponents.Reserve(InMeshes.Num());

	for (int32 Index = 0; Index < InMeshes.Num(); ++Index)
	{
		// 메시가 nullptr 이고 bInKeepEmptySlots가 false 일 경우
		// 팔로워 컴포넌트를 만들지 않고 nullptr을 넣어 빈 슬롯으로 처리
		if (!InMeshes[Index] && !bInKeepEmptySlots)
		{
			OutComponents.Add(nullptr);
			continue;
		}

		// 디버깅 시 어느 자리인지 알 수 있도록 이름에 인덱스를 붙임
		const FName ComponentName = *FString::Printf(TEXT("%s_%d"), *InNamePrefix, Index);

		// 컴포넌트 배열에 팔로워 컴포넌트를 생성해 추가. 실패하면 nullptr이 들어감
		OutComponents.Add(CreateFollowerComponent(InMeshes[Index], InLeader, ComponentName));
	}
}

// 팔로워 컴포넌트를 생성하고 부착하는 함수
USkeletalMeshComponent* UCBModularMeshComponent::CreateFollowerComponent(USkeletalMesh* InMesh, USkeletalMeshComponent* InLeader, const FName& InComponentName)
{
	USkeletalMeshComponent* Follower = NewObject<USkeletalMeshComponent>(GetOwner(), InComponentName);
	if (!Follower) return nullptr;

	// 팔로워 메시 설정. 벗은 슬롯(nullptr)도 컴포넌트는 만들어 두고 이후 교체 때 메시만 갈아끼움.
	Follower->SetSkeletalMeshAsset(InMesh);

	// 리더에 부착.
	Follower->SetupAttachment(InLeader);

	// 월드에 등록. RegisterComponent를 호출해야 BeginPlay 이후에 팔로워가 렌더링됨.
	Follower->RegisterComponent();

	// 리더 포즈 연결. 메시가 없으면 연결할 본이 없으므로 건너뜀 (교체 시 다시 연결됨).
	// 등록(RegisterComponent) 이후에 호출해야 본 매핑이 올바르게 만들어짐.
	if (InMesh)
	{
		Follower->SetLeaderPoseComponent(InLeader);
	}

	return Follower;
}

void UCBModularMeshComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 런타임 생성한 팔로워 정리 (피부·의상 모두)
	DestroyFollowers(SkinComponents);
	DestroyFollowers(CosmeticComponents);

	Super::EndPlay(EndPlayReason);
}

// 팔로워 정리에서 공통으로 호출
void UCBModularMeshComponent::DestroyFollowers(TArray<TObjectPtr<USkeletalMeshComponent>>& InOutComponents)
{
	for (const TObjectPtr<USkeletalMeshComponent>& Follower : InOutComponents)
	{
		if (Follower)
		{
			Follower->DestroyComponent();
		}
	}
	InOutComponents.Reset();
}
