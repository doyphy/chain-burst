#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "Types/CBEnumTypes.h"
#include "CBLevelUpData.generated.h"

class UCBLevelUpWidget;
class UGameplayEffect;
class UTexture2D;

/**
 * 레벨업 카드 한 장.
 * 어떤 어트리뷰트를 어떤 방식(Add·Multiply)으로 올릴지는 효과 GE 가 정하고, 얼마나 올릴지는 수치가 정함.
 * 수치는 SetByCaller(Data.LevelUp)로 GE 에 전달되므로 같은 GE 를 수치만 다른 여러 카드가 공유할 수 있음.
 * 겉모습은 등급으로 정함 — 같은 등급의 카드는 같은 프레임·샤인 색을 씀 (UCBLevelUpData::GradeStyles).
 */
USTRUCT(BlueprintType)
struct FCBLevelUpCard
{
	GENERATED_BODY()

	/**
	 * 선택 시 적용할 효과. Infinite 로 만들고 모디파이어 크기를 SetByCaller(Data.LevelUp)로 읽게 할 것.
	 * 비어 있으면 뽑히지 않음.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> Effect = nullptr;

	/** 올릴 수치. SetByCaller(Data.LevelUp)로 효과에 전달됨 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Magnitude = 0.f;

	/** 카드 등급. 카드 위젯이 등급별 겉모습을 고르는 키 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	ECBLevelUpCardGrade Grade = ECBLevelUpCardGrade::Common;

	/** UI 에 표시할 이름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText DisplayName;

	/** UI 에 표시할 설명 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (MultiLine = true))
	FText Description;

	/** UI 에 표시할 아이콘. 위젯이 필요할 때 로드하므로 소프트 참조 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon = nullptr;
};

/**
 * 카드 등급 하나의 겉모습.
 * 등급마다 위젯을 따로 만들지 않고, 카드 위젯 하나가 SetCard 에서 이 값으로 이미지·색만 갈아 끼움.
 */
USTRUCT(BlueprintType)
struct FCBLevelUpCardGradeStyle
{
	GENERATED_BODY()

	/** 카드 프레임(배경) 이미지. 위젯이 필요할 때 로드하므로 소프트 참조 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Frame = nullptr;

	/** 샤인 효과 색. M_UI_CardShine 의 ShineColor 파라미터로 넘김 (기본값은 머티리얼 기본값과 같음) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor ShineColor = FLinearColor(1.f, 0.95f, 0.8f, 1.f);
};

/**
 * 레벨업 시스템의 데이터 (경험치 곡선·카드 목록·선택 규칙·등급별 뽑기 가중치·카드 위젯·등급별 카드 겉모습).
 * 게임 인스턴스가 보유해 서버(카드 뽑기·검증·적용)와 전 클라이언트(카드 표시·경험치 바)가 같은 데이터를 봄.
 * 카드는 배열 인덱스로 주고받으므로 서버와 클라이언트가 같은 에셋을 써야 함.
 */
UCLASS()
class CHAINBURST_API UCBLevelUpData : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 지금 레벨에서 다음 레벨까지 필요한 경험치를 구하는 함수.
	 * 곡선이 비었거나 0 이하가 나오면 경고를 남기고 1 로 올림 (0 이면 레벨업이 끝없이 반복됨).
	 * @param InLevel 현재 레벨 (1부터)
	 * @return 다음 레벨까지 필요한 경험치 (최소 1)
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|LevelUp")
	int32 GetRequiredExperience(int32 InLevel) const;

	/**
	 * 최대 레벨에 도달했는지 검사하는 함수. (서버는 경험치 누적을 멈추고, 경험치 바는 가득 찬 상태로 표시)
	 * @param InLevel 검사할 레벨
	 * @return MaxLevel 이 지정돼 있고 그 레벨 이상이면 true. MaxLevel 이 0(무제한)이면 항상 false
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|LevelUp")
	FORCEINLINE bool IsMaxLevel(int32 InLevel) const { return MaxLevel > 0 && InLevel >= MaxLevel; }

	/**
	 * 인덱스로 카드를 찾는 함수.
	 * @param InCardIndex 카드 배열 인덱스
	 * @return 찾은 카드. 범위를 벗어나면 nullptr
	 */
	const FCBLevelUpCard* FindCard(int32 InCardIndex) const;

	/**
	 * 카드 목록에서 중복 없이 무작위로 CardsPerLevelUp 장을 뽑는 함수. (서버가 플레이어마다 따로 호출)
	 * GradeWeights 가 있으면 칸마다 등급을 가중치로 먼저 고르고 그 등급 안에서 균등하게, 없으면 전체에서 균등하게 뽑음.
	 * 효과가 비어 있는 카드는 뽑지 않으며, 뽑을 수 있는 후보가 그보다 적으면 있는 만큼만 뽑음.
	 * @param OutCardIndices 뽑은 카드 인덱스 (기존 내용은 비워짐)
	 */
	void RollCards(TArray<int32>& OutCardIndices) const;

	/**
	 * 등급에 맞는 카드 겉모습을 찾는 함수. (카드 위젯이 SetCard 에서 호출)
	 * 등록되지 않은 등급이면 경고를 남기고 false 를 반환함 — 위젯은 디자이너 기본 모습을 그대로 둠.
	 * @param InGrade 찾을 등급
	 * @param OutStyle 찾은 겉모습
	 * @return 등록된 등급이면 true
	 */
	UFUNCTION(BlueprintPure, Category = "ChainBurst|LevelUp")
	bool FindGradeStyle(ECBLevelUpCardGrade InGrade, FCBLevelUpCardGradeStyle& OutStyle) const;

	/** [Getter] 카드 선택 제한 시간(초, 실제 시간) */
	FORCEINLINE float GetSelectionTimeout() const { return SelectionTimeout; }

	/** [Getter] 카드 선택 위젯 클래스 */
	FORCEINLINE TSubclassOf<UCBLevelUpWidget> GetLevelUpWidgetClass() const { return LevelUpWidgetClass; }

protected:
	/**
	 * 레벨(X) → 다음 레벨까지 필요한 경험치(Y).
	 * 레벨 제한이 없으므로 마지막 키 뒤는 곡선의 외삽 설정을 따름 (Constant = 마지막 값 유지, Linear = 기울기 유지).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Experience")
	FRuntimeFloatCurve RequiredExperienceCurve;

	/**
	 * 최대 레벨 (0 = 무제한). 도달하면 경험치가 더 쌓이지 않고 레벨업도 없음.
	 * 한 번에 여러 레벨을 넘어도 이 레벨까지만 오르며, 넘친 경험치는 버림.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Experience", meta = (ClampMin = "0"))
	int32 MaxLevel = 0;

	/** 뽑힐 수 있는 카드. 배열 인덱스가 서버와 클라이언트가 주고받는 카드 식별자 */
	UPROPERTY(EditDefaultsOnly, Category = "Card", meta = (TitleProperty = "DisplayName"))
	TArray<FCBLevelUpCard> Cards;

	/** 레벨업마다 한 플레이어에게 보여줄 카드 수 */
	UPROPERTY(EditDefaultsOnly, Category = "Card", meta = (ClampMin = "1"))
	int32 CardsPerLevelUp = 3;

	/**
	 * 등급별 뽑기 가중치. 서버가 카드를 뽑을 때만 읽음.
	 * 칸마다 등급을 이 비율로 먼저 고른 뒤 그 등급 안에서 균등하게 한 장을 고르므로, 값의 비율이 곧 "한 칸이 그 등급일 확률"
	 * (등급별 카드 수와 무관). 다 뽑힌 등급은 빼고 남은 등급끼리 비율을 다시 나눔.
	 * 비어 있으면 등급과 무관하게 전체 카드에서 균등하게 뽑음. 맵에 없는 등급은 나오지 않음 (경고 로그).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Card")
	TMap<ECBLevelUpCardGrade, float> GradeWeights;

	/**
	 * 카드 선택 제한 시간(초). 월드가 정지된 동안 흐르므로 실제 시간 기준.
	 * 지나면 아직 고르지 않은 플레이어는 받은 카드 중 하나가 무작위로 적용됨.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Card", meta = (ClampMin = "1.0", Units = "s"))
	float SelectionTimeout = 20.f;

	/** 카드 선택 위젯 클래스. 각 플레이어의 컨트롤러가 HUD 스택의 Menu 레이어에 띄움 */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UCBLevelUpWidget> LevelUpWidgetClass = nullptr;

	/** 등급별 카드 겉모습. 카드에 쓰는 등급은 전부 등록할 것 (빠지면 그 등급 카드는 디자이너 기본 모습으로 뜸) */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TMap<ECBLevelUpCardGrade, FCBLevelUpCardGradeStyle> GradeStyles;
};
