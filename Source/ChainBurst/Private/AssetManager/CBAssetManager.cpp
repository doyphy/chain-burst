#include "AssetManager/CBAssetManager.h"

UCBAssetManager& UCBAssetManager::Get()
{
	// 엔진 객체가 유효한지 확인
	check(GEngine);

	// 에셋 매니저 가져오기
	UCBAssetManager* MyAssetManager = Cast<UCBAssetManager>(GEngine->AssetManager);

	// 캐스팅 검사
	checkf(MyAssetManager, TEXT("Asset Manager Class 가 UCBAssetManager 로 설정되어 있지 않음."));

	// 반환
	return *MyAssetManager;
}

void UCBAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	UE_LOG(LogTemp, Warning, TEXT("Starting initial loading in CBAssetManager"));
}
