#include "HexMenuGameMode.h"
#include "HexMenuWidgets.h"
#include "HexGameFlowSubsystem.h"
#include "HexLevelTools.h"
#include "HexBoardSubsystem.h"
#include "HexSokobanSettings.h"
#include "HexTile.h"
#include "HexGridLibrary.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "Blueprint/UserWidget.h"

AHexMenuGameMode::AHexMenuGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AHexMenuPlayerController::StaticClass();
	HUDClass = AHUD::StaticClass();
}

bool AHexMenuGameMode::FrameActors(const UWorld* World, const TArray<AActor*>& Actors, float ScreenShift, FVector& OutLocation, FRotator& OutRotation)
{
	FBox Bounds(ForceInit);
	for (const AActor* Actor : Actors)
	{
		if (Actor)
		{
			Bounds += Actor->GetComponentsBoundingBox(true);
		}
	}
	if (!Bounds.IsValid)
	{
		return false;
	}
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	const float HalfFov = FMath::DegreesToRadians(Settings.BoardCameraFOV * 0.5f);
	const FVector Extent = Bounds.GetExtent();
	OutRotation = FRotator(-Settings.BoardCameraPitch, Settings.BoardCameraYaw, 0.f);
	const FVector Right = FRotationMatrix(OutRotation).GetUnitAxis(EAxis::Y);
	const float Radius = static_cast<float>(FMath::Max(Extent.X, Extent.Y)) * 1.15f + 150.f;
	const float Distance = Radius / FMath::Tan(HalfFov) * Settings.BoardCameraMargin * 1.1f;
	const FVector Center = Bounds.GetCenter() - Right * (Radius * ScreenShift);
	OutLocation = Center - OutRotation.Vector() * Distance;
	return true;
}

void AHexMenuGameMode::StartPlay()
{
	Super::StartPlay();
	UWorld* World = GetWorld();
	if (!World) return;
	// The menu is portrait + UI over the shared flowing backdrop, with no puzzle actors.
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	MenuCamera = World->SpawnActor<ACameraActor>(Params);
	BaseLocation = FVector(0.f, 0.f, 2200.f);
	BaseRotation = FRotator(-90.f, 0.f, 0.f);
	if (MenuCamera)
	{
		MenuCamera->SetActorLocationAndRotation(BaseLocation, BaseRotation);
		MenuCamera->GetCameraComponent()->SetFieldOfView(60.f);
		MenuCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
	}

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		PC->SetViewTarget(MenuCamera);
	}
}

void AHexMenuGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

AHexMenuPlayerController::AHexMenuPlayerController()
{
	bShowMouseCursor = true;
	bAutoManageActiveCameraTarget = false;
}

void AHexMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	MenuWidget = CreateWidget<UHexMenuWidget>(this, UHexMenuWidget::StaticClass());
	if (MenuWidget)
	{
		MenuWidget->AddToViewport(10);
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(MenuWidget->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
	if (const AHexMenuGameMode* GameMode = GetWorld()->GetAuthGameMode<AHexMenuGameMode>())
	{
		if (GameMode->GetMenuCamera())
		{
			SetViewTarget(GameMode->GetMenuCamera());
		}
	}
}
