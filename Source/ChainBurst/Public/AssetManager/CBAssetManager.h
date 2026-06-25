#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "CBAssetManager.generated.h"

UCLASS()
class CHAINBURST_API UCBAssetManager : public UAssetManager
{
	GENERATED_BODY()
public:
	static UCBAssetManager& Get();

	/** 비동기 로드 함수 */
	template <typename AssetType>
	void LoadAssetAsync(const TSoftObjectPtr<AssetType>& AssetPointer, TFunction<void(AssetType*)> OnLoadedCallback);
	
protected:
	virtual void StartInitialLoading() override;
};

template <typename AssetType>
void UCBAssetManager::LoadAssetAsync(const TSoftObjectPtr<AssetType>& AssetPointer, TFunction<void(AssetType*)> OnLoadedCallback)
{
	// 에셋 경로 (소프트 참조) 가져오기
	FSoftObjectPath AssetPath = AssetPointer.ToSoftObjectPath();
	if (AssetPath.IsNull())
	{
		// 경로가 비었으면, nullptr 반환
		OnLoadedCallback(nullptr);
		return;
	}

	// 이미 메모리에 로드되어 있다면, 에셋 포인터 반환
	if (AssetPointer.IsValid())
	{
		// 콜백 함수에 로드된 에셋 포인터 전달
		OnLoadedCallback(AssetPointer.Get());
		return;
	}

	// 비동기 로드 요청, 로딩이 끝나면 람다 함수 호출
	GetStreamableManager().RequestAsyncLoad(
		AssetPath,
		FStreamableDelegate::CreateLambda([AssetPointer, OnLoadedCallback]()
		{
			// 로딩이 끝나면 콜백 함수에 로드된 에셋 포인터 전달
			OnLoadedCallback(AssetPointer.Get());
		})
	);
}
