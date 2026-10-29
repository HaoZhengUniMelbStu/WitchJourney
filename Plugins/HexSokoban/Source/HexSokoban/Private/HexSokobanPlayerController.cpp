#include "HexSokobanPlayerController.h"
#include "HexAudioSubsystem.h"
#include "HexCameraBounds.h"
#include "HexGameFlowSubsystem.h"
#include "HexSokobanGameMode.h"
#include "HexGridMovementComponent.h"
#include "HexBoardSubsystem.h"
#include "HexTile.h"
#include "HexGridLibrary.h"
#include "HexCardSubsystem.h"
#include "HexHandWidget.h"
#include "HexPathFXSubsystem.h"
#include "HexMenuWidgets.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "HexSokobanSettings.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

AHexSokobanPlayerController::AHexSokobanPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	bAutoManageActiveCameraTarget = !UHexSokobanSettings::Get().bUseBoardCamera;
}

void AHexSokobanPlayerController::BeginPlay()
{
	Super::BeginPlay();

	BuildRuntimeInput();
	if (IsLocalController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(RuntimeMapping, MappingPriority);
		}
		if (UHexSokobanSettings::Get().bEnableCardMovement)
		{
			CreateHandWidget();
		}
		if (UHexSokobanSettings::Get().bUseBoardCamera)
		{
			bAutoManageActiveCameraTarget = false;
			GetWorldTimerManager().SetTimerForNextTick(this, &AHexSokobanPlayerController::SetupBoardCamera);
		}
	}
}

void AHexSokobanPlayerController::SetupBoardCamera()
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	UWorld* World = GetWorld();
	UHexBoardSubsystem* Board = World ? World->GetSubsystem<UHexBoardSubsystem>() : nullptr;
	if (!Settings.bUseBoardCamera || !Board)
	{
		return;
	}
	Board->EnsureBoardBuilt();

	FBox Bounds(ForceInit);
	TArray<FVector2D> TileCenters;
	for (const AHexTile* Tile : Board->GetAllTiles())
	{
		Bounds += Tile->GetComponentsBoundingBox(true);
		TileCenters.Add(FVector2D(UHexGridLibrary::CoordToWorld(Tile->GetCoord())));
	}
	if (const UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this))
	{
		if (const FHexLevelDefinition* Definition = Flow->GetCurrentDefinition())
		{
			Bounds = FBox(ForceInit);
			TileCenters.Reset();
			for (const FHexTileDef& Tile : Definition->Tiles)
			{
				const FVector Center = UHexGridLibrary::CoordToWorld(Tile.Coord);
				TileCenters.Add(FVector2D(Center));
				const double Radius = UHexGridLibrary::GetTileSpacing()*.6;
				Bounds += Center-FVector(Radius,Radius,0);
				Bounds += Center+FVector(Radius,Radius,80);
			}
		}
	}
	if (!Bounds.IsValid)
	{
		return;
	}

	int32 ViewX = 16, ViewY = 9;
	GetViewportSize(ViewX, ViewY);
	const float Aspect = (ViewX > 0 && ViewY > 0) ? static_cast<float>(ViewX) / static_cast<float>(ViewY) : 16.f / 9.f;

	const float HalfHorizontal = FMath::DegreesToRadians(Settings.BoardCameraFOV * 0.5f);
	const float HalfVertical = FMath::Atan(FMath::Tan(HalfHorizontal) / Aspect);
	const FVector Extent = Bounds.GetExtent();
	const float Pitch = FMath::DegreesToRadians(Settings.BoardCameraPitch);

	// Board footprint as seen by the camera: width across the view, depth foreshortened by the pitch.
	const FRotator Rotation(-Settings.BoardCameraPitch, Settings.BoardCameraYaw, 0.f);
	const FVector Right = FRotationMatrix(FRotator(0.f, Settings.BoardCameraYaw, 0.f)).GetUnitAxis(EAxis::Y);
	const FVector Forward = FRotationMatrix(FRotator(0.f, Settings.BoardCameraYaw, 0.f)).GetUnitAxis(EAxis::X);
	// Small boards (2-4 tiles) would otherwise fill the screen: frame at least ~5 tiles across.
	const float MinHalf = static_cast<float>(UHexGridLibrary::GetTileSpacing()) * 2.6f;
	const float HalfWidth = FMath::Max(MinHalf, static_cast<float>(FMath::Abs(Right.X) * Extent.X + FMath::Abs(Right.Y) * Extent.Y));
	const float HalfDepth = FMath::Max(MinHalf * 0.8f, static_cast<float>(FMath::Abs(Forward.X) * Extent.X + FMath::Abs(Forward.Y) * Extent.Y));
	const float ProjectedDepth = HalfDepth * FMath::Sin(Pitch) + static_cast<float>(Extent.Z) * FMath::Cos(Pitch);

	const float DistanceForWidth = HalfWidth / FMath::Tan(HalfHorizontal);
	const float DistanceForDepth = ProjectedDepth / FMath::Tan(HalfVertical);
	const float Distance = FMath::Max(DistanceForWidth, DistanceForDepth) * Settings.BoardCameraMargin + static_cast<float>(Extent.Z);

	CameraTargetFocus = CameraFocus = Bounds.GetCenter();
	CameraPanHull = HexCameraBounds::Hull(TileCenters);
	// Same close-up scale on every map: roughly eight tile widths across a 16:9 view.
	CameraMinDistance = UHexGridLibrary::GetTileSpacing()*4.f/FMath::Tan(HalfHorizontal)*FMath::Max(1.f,Aspect/(16.f/9.f));
	CameraMaxDistance = FMath::Max(CameraMinDistance, Distance);
	CameraBoardCenter = CameraFocus;
	CameraEdgeAllowance = UHexGridLibrary::GetTileSpacing()*.35f;
	CameraPanVelocity = FVector::ZeroVector;
	CameraTargetDistance = CameraDistance = CameraMaxDistance;
	if (Board->IsPerformanceMode())
	{
		if (const UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this)) if (const FHexLevelDefinition* Def = Flow->GetCurrentDefinition())
			CameraTargetFocus = CameraFocus = UHexGridLibrary::CoordToWorld(Def->PlayerStart);
		CameraTargetDistance = CameraDistance = CameraMinDistance;
	}
	CameraTargetFocus = CameraFocus = ConstrainBoardFocus(CameraFocus);
	const FVector Location = CameraFocus - Rotation.Vector() * CameraDistance;

	if (!BoardCamera)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		BoardCamera = World->SpawnActor<ACameraActor>(Location, Rotation, Params);
	}
	if (!BoardCamera)
	{
		return;
	}
	BoardCamera->SetActorLocationAndRotation(Location, Rotation);
	if (UCameraComponent* Camera = BoardCamera->GetCameraComponent())
	{
		Camera->SetFieldOfView(Settings.BoardCameraFOV);
		Camera->SetConstraintAspectRatio(false);
	}
	SetViewTargetWithBlend(BoardCamera, 0.f);
}

void AHexSokobanPlayerController::CreateHandWidget()
{
	if (HandWidget || !IsLocalController())
	{
		return;
	}
	TSubclassOf<UHexHandWidget> Class = UHexSokobanSettings::Get().HandWidgetClass.TryLoadClass<UHexHandWidget>();
	if (!Class)
	{
		Class = UHexHandWidget::StaticClass();
	}
	HandWidget = CreateWidget<UHexHandWidget>(this, Class);
	if (!HandWidget)
	{
		return;
	}
	HandWidget->AddToViewport(HandWidgetZOrder);

	// Keep game input + the cursor; UI gets the clicks that land on cards.
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

UInputAction* AHexSokobanPlayerController::MakeAction(const TCHAR* Name)
{
	UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
	Action->ValueType = EInputActionValueType::Boolean;
	return Action;
}

void AHexSokobanPlayerController::BuildRuntimeInput()
{
	if (RuntimeMapping)
	{
		return;
	}

	RuntimeMapping = NewObject<UInputMappingContext>(this, TEXT("IMC_HexSokoban_Runtime"));

	ClickAction = MakeAction(TEXT("IA_Hex_Click"));
	RuntimeMapping->MapKey(ClickAction, EKeys::LeftMouseButton);

	UndoAction = MakeAction(TEXT("IA_Hex_Undo"));
	RuntimeMapping->MapKey(UndoAction, EKeys::Z);
	RuntimeMapping->MapKey(UndoAction, EKeys::BackSpace);

	RestartAction = MakeAction(TEXT("IA_Hex_Restart"));
	RuntimeMapping->MapKey(RestartAction, EKeys::R);

	ConfirmAction = MakeAction(TEXT("IA_Hex_Confirm"));
	RuntimeMapping->MapKey(ConfirmAction, EKeys::Enter);
	RuntimeMapping->MapKey(ConfirmAction, EKeys::SpaceBar);

	// Screen-relative hex directions: up, up-right, down-right, down, down-left, up-left.
	const FKey DirectionKeys[6] = { EKeys::W, EKeys::E, EKeys::D, EKeys::S, EKeys::A, EKeys::Q };
	const TCHAR* DirectionNames[6] = { TEXT("IA_Hex_Up"), TEXT("IA_Hex_UpRight"), TEXT("IA_Hex_DownRight"), TEXT("IA_Hex_Down"), TEXT("IA_Hex_DownLeft"), TEXT("IA_Hex_UpLeft") };
	HintAction = MakeAction(TEXT("IA_Hex_Hint"));
	RuntimeMapping->MapKey(HintAction, EKeys::H);

	DeselectAction = MakeAction(TEXT("IA_Hex_Deselect"));
	RuntimeMapping->MapKey(DeselectAction, EKeys::RightMouseButton);

	PauseAction = MakeAction(TEXT("IA_Hex_Pause"));
	RuntimeMapping->MapKey(PauseAction, EKeys::Escape);
	RuntimeMapping->MapKey(PauseAction, EKeys::P);	// in Play-In-Editor, Esc stops the session: P opens the menu there

	const FKey NumberKeys[7] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven };
	CardSelectActions.Reset();
	for (int32 Index = 0; Index < 7; ++Index)
	{
		UInputAction* Action = MakeAction(*FString::Printf(TEXT("IA_Hex_Card%d"), Index + 1));
		RuntimeMapping->MapKey(Action, NumberKeys[Index]);
		CardSelectActions.Add(Action);
	}

	DirectionActions.Reset();
	for (int32 Index = 0; Index < 6; ++Index)
	{
		UInputAction* Action = MakeAction(DirectionNames[Index]);
		RuntimeMapping->MapKey(Action, DirectionKeys[Index]);
		DirectionActions.Add(Action);
	}
}

void AHexSokobanPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BuildRuntimeInput();
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AHexSokobanPlayerController::ZoomIn);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AHexSokobanPlayerController::ZoomOut);
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input)
	{
		UE_LOG(LogHexSokoban, Error, TEXT("HexSokobanPlayerController needs EnhancedInputComponent as the default input component class."));
		return;
	}

	Input->BindAction(ClickAction, ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnClick);
	Input->BindAction(UndoAction, ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnUndo);
	Input->BindAction(RestartAction, ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnRestart);
	Input->BindAction(ConfirmAction, ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnConfirm);

	Input->BindAction(DirectionActions[0], ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnDirUp);
	Input->BindAction(DirectionActions[1], ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnDirUpRight);
	Input->BindAction(DirectionActions[2], ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnDirDownRight);
	Input->BindAction(DirectionActions[3], ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnDirDown);
	Input->BindAction(DirectionActions[4], ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnDirDownLeft);
	Input->BindAction(DirectionActions[5], ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnDirUpLeft);

	Input->BindAction(DeselectAction, ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnDeselect);
	Input->BindAction(PauseAction, ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnPause);
	Input->BindAction(HintAction, ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnToggleHint);
	Input->BindAction(CardSelectActions[0], ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnSelect1);
	Input->BindAction(CardSelectActions[1], ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnSelect2);
	Input->BindAction(CardSelectActions[2], ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnSelect3);
	Input->BindAction(CardSelectActions[3], ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnSelect4);
	Input->BindAction(CardSelectActions[4], ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnSelect5);
	Input->BindAction(CardSelectActions[5], ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnSelect6);
	Input->BindAction(CardSelectActions[6], ETriggerEvent::Started, this, &AHexSokobanPlayerController::OnSelect7);
}

void AHexSokobanPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (InPawn && !InPawn->FindComponentByClass<UHexGridMovementComponent>())
	{
		UHexGridMovementComponent* Mover = NewObject<UHexGridMovementComponent>(InPawn, TEXT("HexGridMovement"));
		InPawn->AddInstanceComponent(Mover);
		Mover->RegisterComponent();
	}

	if (UHexSokobanSettings::Get().bUseBoardCamera && HasActorBegunPlay())
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &AHexSokobanPlayerController::SetupBoardCamera);
	}
}

UHexGridMovementComponent* AHexSokobanPlayerController::GetHexMover() const
{
	const APawn* ControlledPawn = GetPawn();
	return ControlledPawn ? ControlledPawn->FindComponentByClass<UHexGridMovementComponent>() : nullptr;
}

AHexSokobanGameMode* AHexSokobanPlayerController::GetHexGameMode() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<AHexSokobanGameMode>() : nullptr;
}

bool AHexSokobanPlayerController::IsLevelCompleted() const
{
	const AHexSokobanGameMode* GameMode = GetHexGameMode();
	return GameMode && GameMode->GetLevelState() != EHexLevelState::Playing; // completed or failed
}

void AHexSokobanPlayerController::MoveTowardsCursor()
{
	// Terrain clicks never auto-select or consume a card; movement requires card drag/drop.
}

void AHexSokobanPlayerController::MoveInScreenDirection(float ScreenAngle)
{
	// Movement requires card drag/drop, including when legacy keyboard settings are enabled.
}

void AHexSokobanPlayerController::OnClick()
{
	if (IsLevelCompleted())
	{
		OnConfirm();
		return;
	}
	if (HandWidget && (HandWidget->IsInteracting() || HandWidget->IsMouseOverHand()))
	{
		return;	// the click belongs to the hand
	}
	// Clicking terrain never moves the pawn. Only the hand's drag/drop commits moves.
}

void AHexSokobanPlayerController::OnUndo()
{
	if (AHexSokobanGameMode* GameMode = GetHexGameMode())
	{
		if (GameMode->UndoMove()) UHexAudioSubsystem::Play(this, TEXT("draw_card"));
        else UHexAudioSubsystem::PlayExact(this, TEXT("Error_Buzz_01"));
	}
	else if (UHexBoardSubsystem* Board = GetWorld()->GetSubsystem<UHexBoardSubsystem>())
	{
		if (Board->UndoLastMove()) UHexAudioSubsystem::Play(this, TEXT("draw_card"));
        else UHexAudioSubsystem::PlayExact(this, TEXT("Error_Buzz_01"));
	}
}

void AHexSokobanPlayerController::OnRestart()
{
	if (AHexSokobanGameMode* GameMode = GetHexGameMode())
	{
		GameMode->RestartPuzzle();
	}
	else if (UHexBoardSubsystem* Board = GetWorld()->GetSubsystem<UHexBoardSubsystem>())
	{
		Board->RestoreInitialState();
	}
}

void AHexSokobanPlayerController::OnConfirm()
{
	if (AHexSokobanGameMode* GameMode = GetHexGameMode())
	{
		GameMode->HandleConfirm();
	}
}

void AHexSokobanPlayerController::OnDirUp()			{ if (bEnableKeyboardMovement) { MoveInScreenDirection(0.f); } }
void AHexSokobanPlayerController::OnDirUpRight()	{ if (bEnableKeyboardMovement) { MoveInScreenDirection(60.f); } }
void AHexSokobanPlayerController::OnDirDownRight()	{ if (bEnableKeyboardMovement) { MoveInScreenDirection(120.f); } }
void AHexSokobanPlayerController::OnDirDown()		{ if (bEnableKeyboardMovement) { MoveInScreenDirection(180.f); } }
void AHexSokobanPlayerController::OnDirDownLeft()	{ if (bEnableKeyboardMovement) { MoveInScreenDirection(240.f); } }
void AHexSokobanPlayerController::OnDirUpLeft()		{ if (bEnableKeyboardMovement) { MoveInScreenDirection(300.f); } }

void AHexSokobanPlayerController::OnToggleHint()
{
	if (const UHexBoardSubsystem* Board = GetWorld()->GetSubsystem<UHexBoardSubsystem>()) if (Board->IsPerformanceMode()) return;
    UHexAudioSubsystem::Play(this, TEXT("Flick_Switch"));
	if (UHexPathFXSubsystem* PathFX = GetWorld()->GetSubsystem<UHexPathFXSubsystem>())
	{
		PathFX->ToggleHint();
	}
}

void AHexSokobanPlayerController::OnDeselect()
{
	if (UHexCardSubsystem* Cards = GetWorld()->GetSubsystem<UHexCardSubsystem>())
	{
		if (Cards->IsActive() && Cards->GetSelectedCardIndex() != INDEX_NONE)
		{
			Cards->SelectCard(INDEX_NONE);
		}
	}
}

void AHexSokobanPlayerController::OnPause()
{
	if (PauseMenu)
	{
		ClosePauseMenu();
		return;
	}
	if (UHexCardSubsystem* Cards = GetWorld()->GetSubsystem<UHexCardSubsystem>())
	{
		if (Cards->IsActive() && Cards->GetSelectedCardIndex() != INDEX_NONE)
		{
			Cards->SelectCard(INDEX_NONE);	// first Esc only drops the selected card
			return;
		}
	}
	PauseMenu = CreateWidget<UHexPauseMenuWidget>(this, UHexPauseMenuWidget::StaticClass());
	if (!PauseMenu)
	{
		return;
	}
	PauseMenu->OnResume = [this]() { ClosePauseMenu(); };
	PauseMenu->AddToViewport(500);
	UGameplayStatics::SetGamePaused(this, true);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(PauseMenu->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

void AHexSokobanPlayerController::ClosePauseMenu()
{
	// UIOnly consumes key-up events while paused. Clear the original Escape/P
	// press before gameplay input resumes, otherwise Enhanced Input starts it again.
	FlushPressedKeys();
	if (PauseMenu)
	{
		PauseMenu->RemoveFromParent();
		PauseMenu = nullptr;
	}
	UGameplayStatics::SetGamePaused(this, false);
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	UWidgetBlueprintLibrary::SetFocusToGameViewport();
	FlushPressedKeys();
}

void AHexSokobanPlayerController::SelectCardSlot(int32 HandIndex)
{
	UHexCardSubsystem* Cards = GetWorld()->GetSubsystem<UHexCardSubsystem>();
	// Cards no neighbouring tile accepts can't be selected (same rule as the hand widget).
	if (Cards && Cards->IsActive() && Cards->GetHand().IsValidIndex(HandIndex) && !IsLevelCompleted()
		&& (Cards->IsCardPlayableNow(HandIndex) || Cards->GetSelectedCardIndex() == HandIndex))
	{
		Cards->SelectCard(HandIndex);
	}
}
