// project
#include "Controllers/CBChaserController.h"
#include "Camera/CBLobbyCamera.h"
#include "Characters/CBBaseCharacter.h"
#include "Characters/CBChaserCharacter.h"
#include "Components/Input/CBInputManagerComponent.h"
#include "Components/Mesh/CBModularMeshComponent.h"
#include "Core/CBLocalReadySubsystem.h"
#include "CBGameplayTags.h"
#include "GameModes/CBLobbyGameMode.h"
#include "PlayerState/CBPlayerState.h"

// engine
#include "AbilitySystemComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

// [서버] 로비 등에서 클라이언트가 의상을 고르면 호출 (서버에서 실행)
void ACBChaserController::Server_RequestCosmeticPart_Implementation(ECBCosmeticSlot InSlot, FGameplayTag InPartId)
{
	// Player State 가져오기
	ACBPlayerState* CBPlayerState = GetPlayerState<ACBPlayerState>();
	if (!CBPlayerState) return;

	// 빈 태그는 선택 해제라 검증할 대상이 없음. 그 외에는 카탈로그에 등록된 파츠인지 확인
	if (InPartId.IsValid())
	{
		// 모듈러 메시 컴포넌트 가져오기
		const ACBChaserCharacter* ChaserCharacter = Cast<ACBChaserCharacter>(GetPawn());
		const UCBModularMeshComponent* ModularMeshComponent = ChaserCharacter ? ChaserCharacter->GetModularMeshComponent() : nullptr;

		// 폰이 없거나 카탈로그에 없는 파츠면 경고 로그를 남기고 요청 무시
		if (!ModularMeshComponent || !ModularMeshComponent->IsValidCosmeticPart(InSlot, InPartId))
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] 유효하지 않은 의상 파츠 요청: %s"), *GetNameSafe(this), *InPartId.ToString());
			return;
		}
	}

	// 검증을 통과했으면 PlayerState 에 반영 (서버에서만 실행)
	CBPlayerState->Auth_SetCosmeticPart(InSlot, InPartId);
}

// [서버] 로비에서 무기(캐릭터) 변경 버튼을 누르면 호출 (서버에서 실행)
void ACBChaserController::Server_RequestCharacterSelection_Implementation(FGameplayTag InCharacterId)
{
	const UWorld* World = GetWorld();
	if (!World) return;

	// 로비 레벨이 아니면 무시 (게임플레이 중에는 캐릭터를 바꿀 수 없음)
	ACBLobbyGameMode* LobbyGameMode = World->GetAuthGameMode<ACBLobbyGameMode>();
	if (!LobbyGameMode) return;

	ACBPlayerState* CBPlayerState = GetPlayerState<ACBPlayerState>();
	if (!CBPlayerState) return;

	// 준비를 마친 뒤에는 바꿀 수 없음. 무기 장착 어빌리티가 실행된 상태라 재스폰과 엉킴
	if (CBPlayerState->IsReady())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 준비 상태에서 캐릭터 변경을 요청해 무시함"), *GetNameSafe(this));
		return;
	}

	// 같은 캐릭터를 다시 고른 경우. 재스폰할 이유가 없음 (버튼 연타 방어)
	if (CBPlayerState->GetSelectedCharacterId() == InCharacterId) return;

	// 카탈로그에 등록된 캐릭터인지 확인 (게임모드가 게임 인스턴스의 카탈로그로 검증)
	if (!LobbyGameMode->Auth_IsValidCharacterId(InCharacterId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 유효하지 않은 캐릭터 요청: %s"), *GetNameSafe(this), *InCharacterId.ToString());
		return;
	}

	// 검증을 통과했으면 PlayerState 에 반영 (스폰할 클래스를 정하는 키)
	CBPlayerState->Auth_SetSelectedCharacterId(InCharacterId);

	// 고른 캐릭터로 폰을 다시 스폰
	LobbyGameMode->Auth_RespawnWithSelectedCharacter(this);
}

// [서버] 로비에서 준비 버튼을 누르면 호출 (서버에서 실행)
void ACBChaserController::Server_RequestToggleReady_Implementation()
{
	const UWorld* World = GetWorld();
	if (!World) return;

	// 로비 레벨이 아니면 무시
	ACBLobbyGameMode* LobbyGameMode = World->GetAuthGameMode<ACBLobbyGameMode>();
	if (!LobbyGameMode) return;

	ACBPlayerState* CBPlayerState = GetPlayerState<ACBPlayerState>();
	if (!CBPlayerState) return;

	// 캐릭터 로드가 끝나기 전의 준비 요청은 무시.
	const ACBBaseCharacter* CBCharacter = Cast<ACBBaseCharacter>(GetPawn());
	if (!CBCharacter || !CBCharacter->IsCharacterSystemReady())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 캐릭터 로드 중에 준비 요청을 받아 무시함"), *GetNameSafe(this));
		return;
	}

	// 현재 준비 상태를 뒤집음
	const bool bNewReady = !CBPlayerState->IsReady();

	// 플레이어의 준비 상태 설정
	CBPlayerState->Auth_SetReady(bNewReady);

	// 어빌리티 실행
	Auth_PlayReadyAbility(bNewReady);

	// 로비 전체 준비 상태 집계
	LobbyGameMode->Auth_RefreshReadyState();
}

// [서버] 로비에서 닉네임 입력을 확정하면 호출 (서버에서 실행)
void ACBChaserController::Server_RequestSetNickname_Implementation(const FString& InNickname)
{
	const UWorld* World = GetWorld();
	if (!World) return;

	// 로비 레벨이 아니면 무시 (게임플레이 중에는 닉네임을 바꿀 수 없음)
	ACBLobbyGameMode* LobbyGameMode = World->GetAuthGameMode<ACBLobbyGameMode>();
	if (!LobbyGameMode) return;

	ACBPlayerState* CBPlayerState = GetPlayerState<ACBPlayerState>();
	if (!CBPlayerState) return;

	// 준비를 마친 뒤에는 바꿀 수 없음 (무기·의상 변경과 같은 정책)
	if (CBPlayerState->IsReady())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 준비 상태에서 닉네임 변경을 요청해 무시함"), *GetNameSafe(this));
		return;
	}

	// 공백·제어문자·길이 검사와 중복 검사는 게임모드가 담당해서 수정 처리.
	// 쓸 수 있는 이름이 안 나오면(공백뿐인 입력 등) 거부.
	FString Nickname;
	if (!LobbyGameMode->Auth_SanitizeNickname(InNickname, CBPlayerState, Nickname))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 쓸 수 없는 닉네임 요청: %s"), *GetNameSafe(this), *InNickname);
		return;
	}

	// 요청한 결과가 지금 이름과 같으면 복제 갱신을 낼 이유가 없음.
	if (CBPlayerState->GetPlayerName().Equals(Nickname, ESearchCase::CaseSensitive)) return;

	// 엔진 경로로 반영 (SetPlayerName + K2_OnChangeName 훅).
	// 리슨 서버면 SetPlayerName 이 OnRep 을 직접 호출해 주므로 호스트 화면도 같은 경로로 갱신됨
	LobbyGameMode->ChangeName(this, Nickname, true);
}

// [서버] 로비에서 시작 버튼을 누르면 호출 (서버에서 실행)
void ACBChaserController::Server_RequestStartMatch_Implementation()
{
	const UWorld* World = GetWorld();
	if (!World) return;

	// 검증은 전부 게임모드가 함 (호스트 여부·전원 준비·최소 인원)
	if (ACBLobbyGameMode* LobbyGameMode = World->GetAuthGameMode<ACBLobbyGameMode>())
	{
		LobbyGameMode->Auth_TryStartMatch(this);
	}
}

// [서버] 준비 상태에 맞는 무기 장착·해제 어빌리티를 발동 (서버에서 실행)
void ACBChaserController::Auth_PlayReadyAbility(bool bInReady)
{
	// 플레이어 스테이트에서 ASC 가져오기
	const ACBPlayerState* CBPlayerState = GetPlayerState<ACBPlayerState>();
	UAbilitySystemComponent* ASC = CBPlayerState ? CBPlayerState->GetAbilitySystemComponent() : nullptr;
	if (!ASC) return;

	// 준비 상태에 맞는 어빌리티 태그를 선택
	const FGameplayTag AbilityTag = bInReady
		? CBGameplayTags::Ability_Combat_EquipWeapon
		: CBGameplayTags::Ability_Combat_UnequipWeapon;

	// 어빌리티 실행
	ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(AbilityTag));
}

// 뷰포트/넷 커넥션이 연결된 직후. 화면을 그리기 시작하는 시점. 빙의보다 확실히 앞서므로 여기서 화면을 검게 덮음
void ACBChaserController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	// 뷰 타겟이 정해지고 캐릭터가 준비될 때까지 화면을 가림. Local_RequestFadeIn 이 걷어냄
	if (IsLocalController() && PlayerCameraManager) 
	{
		PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, false);
	}
}

// 뷰타겟을 설정할 때 호출되는 함수. 로비 레벨이면 폰 대신 레벨에 배치된 로비 카메라를 뷰 타겟으로 삼음
void ACBChaserController::AutoManageActiveCameraTarget(AActor* SuggestedTarget)
{
	// 커스터마이징 뷰 중에는 뷰 타겟을 건드리지 않음.
	// 이 함수는 빙의마다 여러 번 불리므로, 게이트가 없으면 재빙의(직업 변경 등)에서 클로즈업이 풀림
	if (!bInCosmeticView)
	{
		ACBLobbyCamera* LobbyCamera = Local_ResolveLobbyCamera();

		if (!LobbyCamera)
		{
			// 로비가 아님. 엔진 기본 동작(폰을 뷰 타겟으로)에 맡김
			Super::AutoManageActiveCameraTarget(SuggestedTarget);
		}
		else if (GetViewTarget() != LobbyCamera)
		{
			// 이 함수는 빙의 과정에서 여러 번 불리므로, 이미 보고 있으면 다시 설정하지 않음
			SetViewTarget(LobbyCamera);

			// 즉시 컷이므로 렌더러에 알림.
			// 알리지 않으면 직전 뷰의 오클루전 쿼리·모션 벡터를 재사용해 이전 화면이 몇 프레임 비친다.
			if (PlayerCameraManager)
			{
				PlayerCameraManager->SetGameCameraCutThisFrame();
			}
		}
	}

	// 뷰 타겟이 정해졌으므로 화면을 걷어냄. 폰이 준비 중이면 준비 완료까지 기다림
	Local_RequestFadeIn();
}

// [로컬] 로비 UI의 커스터마이징 버튼에서 호출. 내 캐릭터 앞으로 카메라를 당김
void ACBChaserController::Local_EnterCosmeticView()
{
	// 로컬인지 확인
	if (!IsLocalController()) return;

	// 로비인지 확인
	if (!Local_ResolveLobbyCamera()) return;

	const APawn* MyPawn = GetPawn();
	if (!MyPawn) return;

	// 커스터마이징 카메라 가져오기
	ACameraActor* ViewCamera = Local_ResolveCosmeticViewCamera();
	if (!ViewCamera) return;

	// 카메라 위치 계산
	// 포커스 위치 : 액터 위치 + 높이 오프셋
	// 카메라 위치 : 포커스 위치 + 액터 전방 벡터 * 거리
	const FVector FocusPoint = MyPawn->GetActorLocation() + FVector(0.0f, 0.0f, CosmeticViewHeight);
	const FVector CameraLocation = FocusPoint + MyPawn->GetActorForwardVector() * CosmeticViewDistance;

	// 캐릭터를 바라본 뒤 Yaw 를 틀어 캐릭터를 화면 한쪽으로 밀어냄 (반대쪽이 의상 선택 위젯 자리)
	FRotator CameraRotation = (FocusPoint - CameraLocation).Rotation();
	CameraRotation.Yaw += CosmeticViewYawOffset;

	// 카메라 위치 설정
	ViewCamera->SetActorLocationAndRotation(CameraLocation, CameraRotation);

	// 플래그 설정
	bInCosmeticView = true;

	SetViewTargetWithBlend(ViewCamera, CosmeticViewBlendTime);
}

// [로컬] 커스터마이징을 닫을 때 호출. 공용 로비 뷰로 되돌림
void ACBChaserController::Local_ExitCosmeticView()
{
	if (!IsLocalController()) return;

	// 들어간 적이 없으면 되돌릴 것도 없음
	if (!bInCosmeticView) return;

	// 게이트를 먼저 내려야 복귀 이후 AutoManageActiveCameraTarget 이 정상 동작함
	bInCosmeticView = false;

	if (ACBLobbyCamera* LobbyCamera = Local_ResolveLobbyCamera())
	{
		SetViewTargetWithBlend(LobbyCamera, CosmeticViewBlendTime);
	}
}

// 커스터마이징 카메라 조회. 첫 호출 시 스폰하고 이후에는 위치만 갱신해 재사용
ACameraActor* ACBChaserController::Local_ResolveCosmeticViewCamera()
{
	// 커스터마이징 카메라가 있는지 조회
	if (IsValid(CosmeticViewCamera)) return CosmeticViewCamera;

	UWorld* World = GetWorld();
	if (!World) return nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.ObjectFlags |= RF_Transient;

	// 커스터마이징 카메라 생성
	CosmeticViewCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, SpawnParams);

	if (CosmeticViewCamera)
	{
		// 로컬에만 생성해야하므로 복제 끄기
		CosmeticViewCamera->SetReplicates(false);

		// 렌더 영역과 UI 영역을 일치시킴 (ACBLobbyCamera 와 동일)
		CosmeticViewCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
	}

	return CosmeticViewCamera;
}

// 레벨의 로비 카메라 조회. 찾은 결과만 캐시함 (못 찾은 것을 캐시하면 맵을 넘었을 때 영영 못 찾음)
ACBLobbyCamera* ACBChaserController::Local_ResolveLobbyCamera()
{
	if (ACBLobbyCamera* Cached = CachedLobbyCamera.Get()) return Cached;

	// 레벨에 배치된 로비 카메라 검색. 클래스 자체가 식별자임
	TArray<AActor*> LobbyCameras;
	UGameplayStatics::GetAllActorsOfClass(this, ACBLobbyCamera::StaticClass(), LobbyCameras);

	if (LobbyCameras.IsEmpty()) return nullptr;

	// 로비 카메라는 하나를 전제로 함. 여러 개면 경고 로그를 남기고 첫 번째 카메라만 사용
	if (LobbyCameras.Num() > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 로비 카메라가 %d 개 배치됨. 하나만 남길 것."), *GetNameSafe(this), LobbyCameras.Num());
	}

	CachedLobbyCamera = Cast<ACBLobbyCamera>(LobbyCameras[0]);

	return CachedLobbyCamera.Get();
}

// [로컬] 페이드 인 요청. 폰이 아직 준비 중이면 준비 완료까지 미룸
void ACBChaserController::Local_RequestFadeIn()
{
	if (!IsLocalController() || !PlayerCameraManager) return;

	// 이 함수는 빙의 해제 등으로도 들어오므로, 이미 밝은 상태면 넘어가기
	if (PlayerCameraManager->FadeAmount <= 0.0f) return;

	ACBBaseCharacter* CBCharacter = Cast<ACBBaseCharacter>(GetPawn());

	// 아직 캐릭터에 빙의하지 않음 (관전 폰 단계 등). 화면을 열지 않고 다음 호출을 기다림
	if (!CBCharacter) return;

	// 캐릭터가 아직 준비 중이면 준비 완료 신호를 기다림 (의상 등 비동기 에셋이 붙은 뒤에 화면을 염)
	if (!CBCharacter->IsCharacterSystemReady())
	{
		// 핸들이 유효하면 이미 대기 중이므로 중복 구독하지 않음
		if (!CharacterSystemReadyHandle.IsValid())
		{
			// 준비 완료를 기다릴 캐릭터의 델리게이트 구독.
			PendingReadyCharacter = CBCharacter;
			CharacterSystemReadyHandle = CBCharacter->OnCharacterSystemReadyDelegate.AddUObject(
				this, &ACBChaserController::Local_HandleCharacterSystemReady);
		}
		return;
	}

	// 캐릭터가 이미 준비된 경우. 준비 완료 작업 수행
	Local_ApplyReadyState();
}

// [로컬] 캐릭터 준비 완료 콜백. 구독을 해제하고 화면을 걷어냄
void ACBChaserController::Local_HandleCharacterSystemReady()
{
	if (ACBBaseCharacter* CBCharacter = PendingReadyCharacter.Get())
	{
		// 구독 해제
		CBCharacter->OnCharacterSystemReadyDelegate.Remove(CharacterSystemReadyHandle);
	}

	// 구독 핸들 초기화
	PendingReadyCharacter.Reset();
	CharacterSystemReadyHandle.Reset();

	// 캐릭터 준비 완료 작업 수행
	Local_ApplyReadyState();
}

// [로컬] 캐릭터 준비 완료가 끝나면 호출 (캐릭터 준비 완료 작업 수행).
void ACBChaserController::Local_ApplyReadyState()
{
	// 1회성. 여러 번 들어오면 페이드가 알파 1(완전 검정)에서 다시 시작되어 화면이 깜빡임
	if (bReadyStateApplied) return;
	bReadyStateApplied = true;

	// 게임플레이 입력 허용. 로비에서는 아예 붙이지 않음
	Local_TryAllowGameplayInput();

	// 준비 완료를 알림. 레벨의 위젯 컨트롤러 등이 이 신호로 UI 를 띄움
	if (UCBLocalReadySubsystem* ReadySubsystem = GetWorld()->GetSubsystem<UCBLocalReadySubsystem>())
	{
		ReadySubsystem->NotifyLocalPlayerReady();
	}

	// 화면 열기
	Local_FadeInFromBlack();
}

// [로컬] 게임플레이 레벨에서만 매핑 컨텍스트 등록을 허용. 로비에서는 아예 붙이지 않음
void ACBChaserController::Local_TryAllowGameplayInput()
{
	// 로비면 조작할 일이 없으므로 IMC 를 붙이지 않음.
	if (Local_ResolveLobbyCamera()) return;

	const ACBChaserCharacter* Chaser = Cast<ACBChaserCharacter>(GetPawn());
	if (!Chaser) return;

	if (UCBInputManagerComponent* InputManager = Chaser->GetInputManagerComponent())
	{
		// 게임플레이 입력 허용. 캐릭터의 IMC 등록이 여기서 수행됨
		InputManager->AllowGameplayInput();
	}
}

// [로컬] 검은 화면 걷어내기
void ACBChaserController::Local_FadeInFromBlack()
{
	if (!PlayerCameraManager) return;

	PlayerCameraManager->StartCameraFade(1.0f, 0.0f, ScreenFadeInDuration, FLinearColor::Black);
}
