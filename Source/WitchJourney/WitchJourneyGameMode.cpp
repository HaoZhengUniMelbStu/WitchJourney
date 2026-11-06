// Copyright Epic Games, Inc. All Rights Reserved.

#include "WitchJourneyGameMode.h"
#include "WitchJourneyPlayerController.h"
#include "WitchJourneyCharacter.h"

// Template game mode kept for reference only. The game runs on AHexSokobanGameMode
// (HexSokoban plugin), configured in Config/DefaultEngine.ini and each level's World Settings.
AWitchJourneyGameMode::AWitchJourneyGameMode()
{
	PlayerControllerClass = AWitchJourneyPlayerController::StaticClass();
	DefaultPawnClass = AWitchJourneyCharacter::StaticClass();
}
