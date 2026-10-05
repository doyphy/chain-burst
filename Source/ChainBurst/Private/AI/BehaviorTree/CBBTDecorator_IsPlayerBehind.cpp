// project
#include "AI/BehaviorTree/CBBTDecorator_IsPlayerBehind.h"
#include "Characters/CBBaseCharacter.h"

// engine
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UCBBTDecorator_IsPlayerBehind::UCBBTDecorator_IsPlayerBehind(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = TEXT("Is Player Behind");

	// 위치는 블랙보드 값 변화로 알 수 없으므로 중단 옵션을 두지 않음 (HasLineOfSight 와 같은 처리)
	bAllowAbortLowerPri = false;
	bAllowAbortNone = false;
	bAllowAbortChildNodes = false;
	FlowAbortMode = EBTFlowAbortMode::None;
}

// 등 뒤 반원·Radius 안에 살아있는 플레이어가 있는지 판정
bool UCBBTDecorator_IsPlayerBehind::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	// AI 폰과 월드 가져오기
	const AAIController* AIOwner = OwnerComp.GetAIOwner();
	const APawn* Pawn = AIOwner ? AIOwner->GetPawn() : nullptr;
	const UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!World) return false;

	// AI 폰 위치와 전방 벡터, 반경 제곱값
	const FVector PawnLocation = Pawn->GetActorLocation();
	const FVector Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
	const float RadiusSquared = FMath::Square(Radius);

	// 서버에는 접속한 모든 플레이어 컨트롤러가 있으므로 현재 폰을 그때그때 읽음 (스포너의 인원 집계와 같은 방식)
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (!PC) continue;

		// 죽은 플레이어는 제외
		const ACBBaseCharacter* PlayerCharacter = Cast<ACBBaseCharacter>(PC->GetPawn());
		if (!PlayerCharacter || PlayerCharacter->IsDead()) continue;

		// AI와 플레이어 사이의 수평 거리(X,Y축)만 고려, 높이(Z축)는 무시
		const FVector ToPlayer = (PlayerCharacter->GetActorLocation() - PawnLocation) * FVector(1.f, 1.f, 0.f);
		
		// 반경 안 + 정면 기준 뒤쪽 반원 (내적이 음수)
		if (ToPlayer.SizeSquared() <= RadiusSquared && FVector::DotProduct(Forward, ToPlayer) < 0.f)
		{
			return true;
		}
	}

	return false;
}

FString UCBBTDecorator_IsPlayerBehind::GetStaticDescription() const
{
	// 베이스가 출력하는 반전 여부를 유지하고 판정 기준 한 줄만 덧붙임
	return FString::Printf(TEXT("%s\n등 뒤 반원 %.0fcm 안에 살아있는 플레이어가 있는가"), *Super::GetStaticDescription(), Radius);
}
