#include "HexSokobanGameMode.h"
#include "HexLocalization.h"
#include "HexAudioSubsystem.h"
#include "HexBoardSubsystem.h"
#include "HexGridMovementComponent.h"
#include "HexSokobanHUD.h"
#include "HexSokobanPlayerController.h"
#include "HexSokobanSettings.h"
#include "HexSokobanTypes.h"
#include "HexCardSubsystem.h"
#include "HexGameFlowSubsystem.h"
#include "HexLevelTools.h"
#include "HexTile.h"
#include "HexBox.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HexGridLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AHexSokobanGameMode::AHexSokobanGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PlayerControllerClass = AHexSokobanPlayerController::StaticClass();
	HUDClass = AHexSokobanHUD::StaticClass();
}

void AHexSokobanGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Use the pawn from Project Settings unless a Blueprint subclass picked one.
	if (DefaultPawnClass == ADefaultPawn::StaticClass() || !DefaultPawnClass)
	{
		const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
		if (!Settings.DefaultPlayerPawnClass.IsNull())
		{
			if (UClass* PawnClass = Settings.DefaultPlayerPawnClass.LoadSynchronous())
			{
				DefaultPawnClass = PawnClass;
			}
		}
	}
}

void AHexSokobanGameMode::StartPlay()
{
	BuildDataDrivenLevel();	// before any BeginPlay: the board and the pawn's snap see the spawned tiles
	Super::StartPlay();
}

void AHexSokobanGameMode::BuildDataDrivenLevel()
{
	UWorld* World = GetWorld();
	UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this);
	const TSoftObjectPtr<UWorld>& PlayMap = UHexSokobanSettings::Get().PlayMap;
	if (!World || !Flow || PlayMap.IsNull()
		|| UWorld::RemovePIEPrefix(World->GetPackage()->GetName()) != PlayMap.ToSoftObjectPath().GetLongPackageName())
	{
		return;	// a legacy map with placed tiles
	}
	if (!Flow->GetCurrentDefinition() && Flow->GetNumLevels() > 0)
	{
		Flow->SetCurrentLevel(Flow->GetLevelEntry(0)->Definition.LevelId);	// play map opened directly (PIE): first level
	}
	const FHexLevelDefinition* Definition = Flow->GetCurrentDefinition();
	if (!Definition)
	{
		UE_LOG(LogHexSokoban, Warning, TEXT("Play map opened without a level (no JSON levels found in %s)."), *UHexGameFlowSubsystem::GetBuiltInDir());
		return;
	}
	bDataDriven = true;
	EntranceDefinition = *Definition;
	const FIntPoint Start = Definition->PlayerStart;
	EntranceDefinition.Tiles.StableSort([Start](const FHexTileDef& A, const FHexTileDef& B)
	{
		auto Radius = [Start](FIntPoint C) { C -= Start; return FMath::Max3(FMath::Abs(C.X), FMath::Abs(C.Y), FMath::Abs(C.X+C.Y)); };
		const int32 DA=Radius(A.Coord), DB=Radius(B.Coord);
		if (DA != DB) return DA < DB;
		return A.Coord.X < B.Coord.X;
	});
	bBuildingLevel = true;
	EntranceIndex = 0;
	// One stable starting tile lets the pawn register without falling while the rest streams in.
	if (!EntranceDefinition.Tiles.IsEmpty())
	{
		UHexLevelTools::SpawnTileActor(World, EntranceDefinition.Tiles[EntranceIndex++]);
	}
	if (APlayerController* PC = World->GetFirstPlayerController()) if (APawn* Pawn = PC->GetPawn())
		Pawn->SetActorLocation(UHexGridLibrary::CoordToWorld(Start)+FVector(0,0,120));

	UE_LOG(LogHexSokoban, Log, TEXT("Built level '%s' (%s): %d tiles, %d boxes, %d cards."), *Definition->DisplayName, *Definition->LevelId,
		Definition->Tiles.Num(), Definition->Boxes.Num(), Definition->Deck.Num());
}

float AHexSokobanGameMode::GetAutoAdvanceRemaining() const
{
	return GetWorldTimerManager().IsTimerActive(AutoAdvanceTimer) ? GetWorldTimerManager().GetTimerRemaining(AutoAdvanceTimer) : -1.f;
}

FString AHexSokobanGameMode::GetLevelDisplayName() const
{
	const UHexGameFlowSubsystem* Flow = bDataDriven ? UHexGameFlowSubsystem::Get(this) : nullptr;
	const FHexLevelDefinition* Definition = Flow ? Flow->GetCurrentDefinition() : nullptr;
	if(!Definition) return FString();
	const FHexLevelEntry* Entry=Flow?Flow->GetLevelEntry(Flow->FindLevelIndex(Definition->LevelId)):nullptr;
	return FHexLocalization::LevelName(Definition->LevelId,Definition->DisplayName,Entry && Entry->bBuiltIn);
}

int32 AHexSokobanGameMode::GetNumLevels() const
{
	const UHexGameFlowSubsystem* Flow = bDataDriven ? UHexGameFlowSubsystem::Get(this) : nullptr;
	return Flow ? Flow->GetNumLevels() : GetLevels().Num();
}

void AHexSokobanGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (UHexBoardSubsystem* Board = GetBoard())
	{
		Board->EnsureBoardBuilt();
		Board->OnMoveFinished.AddDynamic(this, &AHexSokobanGameMode::HandleMoveFinished);
		Board->OnBoardReset.AddDynamic(this, &AHexSokobanGameMode::HandleBoardReset);
		Board->OnPushBlocked.AddDynamic(this, &AHexSokobanGameMode::HandlePushBlocked);
	}
	SetLevelState(EHexLevelState::Playing);
	GetWorldTimerManager().SetTimerForNextTick(this, &AHexSokobanGameMode::InitializeCards);
}

void AHexSokobanGameMode::InitializeCards()
{
	if (bBuildingLevel) return;
	// The pawn may have snapped to another tile before the board existed: put it on the designed start.
	const UHexGameFlowSubsystem* Flow = bDataDriven ? UHexGameFlowSubsystem::Get(this) : nullptr;
	const FHexLevelDefinition* Definition = Flow ? Flow->GetCurrentDefinition() : nullptr;
	UHexGridMovementComponent* Mover = GetMover();
	if (Definition && Mover && Mover->GetCoord() != Definition->PlayerStart && GetBoard() && GetBoard()->GetTileAt(Definition->PlayerStart))
	{
		Mover->TeleportToCoord(Definition->PlayerStart);
		GetBoard()->CaptureInitialState();
	}
	if (UHexCardSubsystem* Cards = GetWorld()->GetSubsystem<UHexCardSubsystem>())
	{
		Cards->InitializeForLevel();
	}
}

UHexBoardSubsystem* AHexSokobanGameMode::GetBoard() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UHexBoardSubsystem>() : nullptr;
}

UHexGridMovementComponent* AHexSokobanGameMode::GetMover() const
{
	const UHexBoardSubsystem* Board = GetBoard();
	return Board ? Board->GetPlayerMover() : nullptr;
}

int32 AHexSokobanGameMode::GetMoveCount() const
{
	const UHexBoardSubsystem* Board = GetBoard();
	return Board ? Board->GetMoveCount() : 0;
}

void AHexSokobanGameMode::SetLevelState(EHexLevelState NewState)
{
	LevelState = NewState;
	if (UHexGridMovementComponent* Mover = GetMover())
	{
		Mover->SetMovementLocked(bBuildingLevel || LevelState != EHexLevelState::Playing);
	}
	if (UHexCardSubsystem* Cards = GetWorld() ? GetWorld()->GetSubsystem<UHexCardSubsystem>() : nullptr)
	{
		if (LevelState == EHexLevelState::Playing)
		{
			Cards->RefreshHighlights();
		}
		else
		{
			Cards->ClearHighlights();
		}
	}
}

void AHexSokobanGameMode::HandleMoveFinished(const FHexMoveRecord& Move)
{
	if (LevelState != EHexLevelState::Playing)
	{
		return;
	}
	UHexBoardSubsystem* Board = GetBoard();
	if (bWinPending || !Board)
	{
		return;
	}
	if (!Board->AreAllGoalsCovered())
	{
		if (Board->IsPerformanceMode()) return;
		EvaluateStuck();
		// Designed hands: lose only when no card can move the player any more (not merely "can't win").
		const UHexCardSubsystem* Cards = GetWorld()->GetSubsystem<UHexCardSubsystem>();
		FText Reason;
		if (!bStuck && Cards && Cards->IsOutOfMoves(Reason))
		{
			FailLevel(Cards->GetHand().Num() == 0 ? EHexFailReason::OutOfCards : EHexFailReason::NoPlayableCard);
		}
		return;
	}

	// Solved: freeze input now, but celebrate only once every box has landed.
	bWinPending = true;
	if (UHexGridMovementComponent* Mover = GetMover())
	{
		Mover->SetMovementLocked(true);
	}
	if (UHexCardSubsystem* Cards = GetWorld()->GetSubsystem<UHexCardSubsystem>())
	{
		Cards->ClearHighlights();
	}
	WaitForBoxesThenComplete();
}

FHexLevelResult AHexSokobanGameMode::RateLevel(int32 Turns, int32 Baseline)
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	FHexLevelResult Result;
	Result.Turns = Turns;
	Result.Baseline = Baseline;
	if (Baseline < 0)
	{
		Result.Stars = 3;
		return Result;
	}
	const auto Allowance = [Baseline](int32 Steps, float Ratio)
	{
		return Baseline + FMath::Max(FMath::Max(0, Steps), FMath::CeilToInt(static_cast<float>(Baseline) * FMath::Max(0.f, Ratio)));
	};
	Result.ThreeStarTurns = Allowance(Settings.ThreeStarExtraSteps, Settings.ThreeStarExtraRatio);
	Result.TwoStarTurns = FMath::Max(Result.ThreeStarTurns + 1, Allowance(Settings.TwoStarExtraSteps, Settings.TwoStarExtraRatio));
	Result.Stars = Turns <= Result.ThreeStarTurns ? 3 : (Turns <= Result.TwoStarTurns ? 2 : 1);
	return Result;
}

void AHexSokobanGameMode::WaitForBoxesThenComplete()
{
	const UHexBoardSubsystem* Board = GetBoard();
	if (Board && Board->IsAnyBoxMoving())
	{
		GetWorldTimerManager().SetTimer(WinTimer, this, &AHexSokobanGameMode::WaitForBoxesThenComplete, 0.05f, false);
		return;
	}
	if (WinCelebrationDelay > 0.f)
	{
		GetWorldTimerManager().SetTimer(WinTimer, this, &AHexSokobanGameMode::CompleteLevel, WinCelebrationDelay, false);
	}
	else
	{
		CompleteLevel();
	}
}

void AHexSokobanGameMode::CompleteLevel()
{
	if (!bWinPending)
	{
		return;
	}
	bWinPending = false;
	const int32 Moves = GetMoveCount();
	const UHexBoardSubsystem* Board = GetBoard();
	const UHexCardSubsystem* Cards = GetWorld()->GetSubsystem<UHexCardSubsystem>();
	const int32 Baseline = (Cards && Cards->IsDesigned() && Cards->GetDesignedOptimalTurns() >= 0)
		? Cards->GetDesignedOptimalTurns()
		: (Board ? Board->GetBaselineMoves() : -1);
	LevelResult = RateLevel(Moves, Baseline);	// before anyone hears "completed"
	if (UHexGameFlowSubsystem* Flow = bDataDriven ? UHexGameFlowSubsystem::Get(this) : nullptr)
	{
		Flow->RecordResult(Flow->GetCurrentLevelId(), LevelResult.Stars, Moves);
	}
	SetLevelState(EHexLevelState::Completed);
	CompletedTime = GetWorld()->GetTimeSeconds();

	UE_LOG(LogHexSokoban, Log, TEXT("Level complete in %d turns (optimal %d): %d star(s)."), Moves, LevelResult.Baseline, LevelResult.Stars);

	OnLevelCompleted.Broadcast(Moves);
	ReceiveLevelCompleted(Moves);

	const float Advance = bDataDriven ? UHexSokobanSettings::Get().AutoAdvanceSeconds : AutoAdvanceDelay;
	if (Advance > 0.f && (HasNextLevel() || bDataDriven))
	{
		GetWorldTimerManager().SetTimer(AutoAdvanceTimer, this, &AHexSokobanGameMode::GoToNextLevel, Advance, false);
	}
}

const TArray<TSoftObjectPtr<UWorld>>& AHexSokobanGameMode::GetLevels() const
{
	return LevelSequence.Num() > 0 ? LevelSequence : UHexSokobanSettings::Get().LevelSequence;
}

void AHexSokobanGameMode::HandleBoardReset()
{
	ReceiveBoardReset();
	if (LevelState == EHexLevelState::Playing && !bWinPending)
	{
		EvaluateStuck(); // an undo may have got the player out of it
	}
}

void AHexSokobanGameMode::EvaluateStuck()
{
	// Geometric impossibility is useful for hints, never a loss condition:
	// skills can change reachability beyond the ordinary-card solver.
	ClearStuck();
}

void AHexSokobanGameMode::ClearStuck()
{
	bStuck = false;
	GetWorldTimerManager().ClearTimer(StuckTimer);
}

float AHexSokobanGameMode::GetStuckTimeRemaining() const
{
	return (bStuck && GetWorldTimerManager().IsTimerActive(StuckTimer)) ? GetWorldTimerManager().GetTimerRemaining(StuckTimer) : -1.f;
}

void AHexSokobanGameMode::HandlePushBlocked(EHexDirection Direction, EHexMoveResult Result)
{
	if (bStuck && LevelState == EHexLevelState::Playing && UHexSokobanSettings::Get().bGameOverOnBlockedPushWhenStuck)
	{
		TriggerGameOver();
	}
}

void AHexSokobanGameMode::TriggerGameOver()
{
	// Also guard legacy Blueprint/timer callers: only actual playable cards matter.
	const UHexCardSubsystem* Cards = GetWorld()->GetSubsystem<UHexCardSubsystem>();
	const UHexBoardSubsystem* Board = GetBoard();
	FText Reason;
	if (Board && !Board->AreAllGoalsCovered() && Cards && Cards->IsOutOfMoves(Reason))
	{
		FailLevel(Cards->GetHand().IsEmpty() ? EHexFailReason::OutOfCards : EHexFailReason::NoPlayableCard);
	}
}

void AHexSokobanGameMode::FailLevel(EHexFailReason Reason)
{
	if (LevelState != EHexLevelState::Playing || bWinPending)
	{
		return;
	}
	const UHexBoardSubsystem* Board = GetBoard();
	const UHexCardSubsystem* Cards = GetWorld()->GetSubsystem<UHexCardSubsystem>();
	FText UnplayableReason;
	if (!Board || Board->AreAllGoalsCovered() || !Cards || !Cards->IsOutOfMoves(UnplayableReason))
	{
		ClearStuck();
		return;
	}
	Reason = Cards->GetHand().IsEmpty() ? EHexFailReason::OutOfCards : EHexFailReason::NoPlayableCard;
	ClearStuck();
	FailReason = Reason;
	SetLevelState(EHexLevelState::Failed);
	FailedTime = GetWorld()->GetTimeSeconds();
	UE_LOG(LogHexSokoban, Log, TEXT("Game over (%s)."), Reason == EHexFailReason::Stuck ? TEXT("stuck") : (Reason == EHexFailReason::OutOfCards ? TEXT("out of cards") : TEXT("no playable card")));

	UHexAudioSubsystem::PlayExact(this, TEXT("Error_Buzz_02"));
	OnLevelFailed.Broadcast();
	ReceiveLevelFailed();
	if (Reason != EHexFailReason::Stuck)
	{
		return;	// wait for Z (undo) / R (restart)
	}

	const float Delay = UHexSokobanSettings::Get().GameOverRestartDelay;
	if (Delay > 0.f)
	{
		GetWorldTimerManager().SetTimer(RestartTimer, this, &AHexSokobanGameMode::RestartPuzzle, Delay, false);
	}
	else
	{
		RestartPuzzle();
	}
}

void AHexSokobanGameMode::RestartPuzzle()
{
	if (bBuildingLevel) return;
	GetWorldTimerManager().ClearTimer(AutoAdvanceTimer);
	GetWorldTimerManager().ClearTimer(WinTimer);
	GetWorldTimerManager().ClearTimer(RestartTimer);
	ClearStuck();
	bWinPending = false;
	CompletedTime = -1.f;
	FailedTime = -1.f;
	SetLevelState(EHexLevelState::Playing);
	if (UHexBoardSubsystem* Board = GetBoard())
	{
		Board->RestoreInitialState();
	}
	if (UHexCardSubsystem* Cards = GetWorld()->GetSubsystem<UHexCardSubsystem>())
	{
		Cards->RestoreInitialState();
	}
}

bool AHexSokobanGameMode::UndoMove()
{
	if (bBuildingLevel) return false;
	if (LevelState == EHexLevelState::Failed && FailReason != EHexFailReason::Stuck)
	{
		SetLevelState(EHexLevelState::Playing);	// out of cards: take the last step back and keep playing
		FailedTime = -1.f;
	}
	if (LevelState != EHexLevelState::Playing || bWinPending)
	{
		return false;
	}
	UHexBoardSubsystem* Board = GetBoard();
	if (!Board || !Board->UndoLastMove())
	{
		return false;
	}
	if (UHexCardSubsystem* Cards = GetWorld()->GetSubsystem<UHexCardSubsystem>())
	{
		Cards->UndoLastPlay();
	}
	return true;
}

int32 AHexSokobanGameMode::GetCurrentLevelIndex() const
{
	if (const UHexGameFlowSubsystem* Flow = bDataDriven ? UHexGameFlowSubsystem::Get(this) : nullptr)
	{
		return Flow->GetCurrentLevelIndex();
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return INDEX_NONE;
	}
	const FString CurrentPackage = UWorld::RemovePIEPrefix(World->GetPackage()->GetName());
	const TArray<TSoftObjectPtr<UWorld>>& Levels = GetLevels();
	for (int32 Index = 0; Index < Levels.Num(); ++Index)
	{
		if (Levels[Index].ToSoftObjectPath().GetLongPackageName() == CurrentPackage)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool AHexSokobanGameMode::HasNextLevel() const
{
	if (const UHexGameFlowSubsystem* Flow = bDataDriven ? UHexGameFlowSubsystem::Get(this) : nullptr)
	{
		return Flow->HasNextLevel();
	}
	const int32 NextIndex = GetCurrentLevelIndex() + 1; // INDEX_NONE (-1) -> 0
	const TArray<TSoftObjectPtr<UWorld>>& Levels = GetLevels();
	return Levels.IsValidIndex(NextIndex) && !Levels[NextIndex].IsNull();
}

void AHexSokobanGameMode::GoToNextLevel()
{
	GetWorldTimerManager().ClearTimer(AutoAdvanceTimer);
	if (UHexGameFlowSubsystem* Flow = bDataDriven ? UHexGameFlowSubsystem::Get(this) : nullptr)
	{
		Flow->PlayNextLevel();	// next level, the editor after a test play, or level select after the last one
		return;
	}
	const TArray<TSoftObjectPtr<UWorld>>& Levels = GetLevels();
	if (HasNextLevel())
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, Levels[GetCurrentLevelIndex() + 1]);
	}
	else if (GetCurrentLevelIndex() > 0 && !Levels[0].IsNull())
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, Levels[0]); // finished the run: back to level 1
	}
	else
	{
		RestartPuzzle();
	}
}

void AHexSokobanGameMode::HandleConfirm()
{
	if (LevelState == EHexLevelState::Completed)
	{
		GoToNextLevel();
	}
	else if (LevelState == EHexLevelState::Failed)
	{
		RestartPuzzle(); // skip the wait
	}
}
