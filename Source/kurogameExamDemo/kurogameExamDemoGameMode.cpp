// Copyright Epic Games, Inc. All Rights Reserved.

#include "kurogameExamDemoGameMode.h"
#include "kurogameExamDemoPlayerController.h"
#include "kurogameExamDemoCharacter.h"

// Template game mode kept for reference only. The game runs on AHexSokobanGameMode
// (HexSokoban plugin), configured in Config/DefaultEngine.ini and each level's World Settings.
AkurogameExamDemoGameMode::AkurogameExamDemoGameMode()
{
	PlayerControllerClass = AkurogameExamDemoPlayerController::StaticClass();
	DefaultPawnClass = AkurogameExamDemoCharacter::StaticClass();
}
