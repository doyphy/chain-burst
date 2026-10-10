// 에디터 전용 머티리얼 정리 콘솔 명령 (CB.FixIgnoredOpacityMask).
// 마스크 입력을 연결했는데 컴파일에서 무시되는 Masked 머티리얼을 고침 (생성·소멸 연출 → SpawnFX.md). 옛 팩 머티리얼에 남아 있는 두 원인을 끔:
//  - "마스크여도 불투명으로 간주" 플래그(UMaterial::bCanMaskedBeAssumedOpaque) — GetBlendMode 가 Opaque 를 돌려줌
//  - 마스크 출력 핀의 인라인 상수 표시(OpacityMask.UseConstant) — 연결된 노드 대신 상수로 컴파일됨
// 두 값 모두 Details·Python 에 노출되지 않고 에디터는 set 콘솔 명령을 막아, C++ 명령으로만 바꿀 수 있음.

#if WITH_EDITOR

// engine
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/AssetManager.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"

namespace
{
	// 마스크 입력이 실제로 연결돼 있는지. 어트리뷰트 방식은 마스크가 어트리뷰트 안에 있으므로 그 출력의 연결 여부로 판단
	bool IsMaskConnected(const UMaterial* InMaterial, const UMaterialEditorOnlyData* InEditorOnly)
	{
		return InMaterial->bUseMaterialAttributes ? InEditorOnly->MaterialAttributes.IsConnected() : InEditorOnly->OpacityMask.IsConnected();
	}

	// 경로 하나(머티리얼 패키지 또는 폴더)에서 머티리얼 에셋을 모음. 폴더는 하위 폴더까지 포함
	void CollectMaterials(IAssetRegistry& InAssetRegistry, const FString& InPath, TArray<FAssetData>& OutAssets)
	{
		TArray<FAssetData> Found;
		InAssetRegistry.GetAssetsByPackageName(FName(*InPath), Found);
		if (Found.IsEmpty())
		{
			InAssetRegistry.GetAssetsByPath(FName(*InPath), Found, true /*bRecursive*/);
		}

		for (const FAssetData& Asset : Found)
		{
			if (Asset.AssetClassPath == UMaterial::StaticClass()->GetClassPathName())
			{
				OutAssets.Add(Asset);
			}
		}
	}

	void FixIgnoredOpacityMask(const TArray<FString>& InArgs)
	{
		if (InArgs.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[FixIgnoredOpacityMask] 사용법: CB.FixIgnoredOpacityMask <폴더 또는 머티리얼 패키지 경로> ..."));
			return;
		}

		IAssetRegistry& AssetRegistry = UAssetManager::Get().GetAssetRegistry();
		TArray<FAssetData> Assets;
		for (const FString& Path : InArgs)
		{
			CollectMaterials(AssetRegistry, Path, Assets);
		}

		int32 FixedCount = 0;
		for (const FAssetData& Asset : Assets)
		{
			UMaterial* Material = Cast<UMaterial>(Asset.GetAsset());
			if (!Material || Material->BlendMode != BLEND_Masked) continue;

			UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
			// 마스크 입력이 없는 머티리얼의 플래그는 엔진의 정상 최적화라 건드리지 않음
			if (!EditorOnly || !IsMaskConnected(Material, EditorOnly)) continue;

			const bool bAssumedOpaque = Material->bCanMaskedBeAssumedOpaque;
			// 어트리뷰트 방식은 마스크 출력 핀을 쓰지 않으므로 그 핀의 상수 표시와 무관
			const bool bConstantOverride = !Material->bUseMaterialAttributes && EditorOnly->OpacityMask.UseConstant;
			if (!bAssumedOpaque && !bConstantOverride) continue;

			// 변경 알림으로 셰이더를 다시 컴파일함. 저장은 하지 않음 (고친 에셋만 골라 저장할 것)
			Material->PreEditChange(nullptr);
			Material->bCanMaskedBeAssumedOpaque = false;
			if (bConstantOverride)
			{
				EditorOnly->OpacityMask.UseConstant = false;
			}
			Material->PostEditChange();
			Material->MarkPackageDirty();

			UE_LOG(LogTemp, Log, TEXT("[FixIgnoredOpacityMask] 고침: %s (불투명 간주 %d, 인라인 상수 %d)"),
				*Asset.PackageName.ToString(), bAssumedOpaque, bConstantOverride);
			++FixedCount;
		}

		UE_LOG(LogTemp, Log, TEXT("[FixIgnoredOpacityMask] 머티리얼 %d개 중 %d개 고침"), Assets.Num(), FixedCount);
	}

	FAutoConsoleCommand FixIgnoredOpacityMaskCommand(
		TEXT("CB.FixIgnoredOpacityMask"),
		TEXT("마스크 입력을 연결했는데 컴파일에서 무시되는 Masked 머티리얼을 고침 (에디터 전용, 저장은 안 함). 인자: 폴더 또는 머티리얼 패키지 경로 여러 개"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FixIgnoredOpacityMask));
}

#endif // WITH_EDITOR
