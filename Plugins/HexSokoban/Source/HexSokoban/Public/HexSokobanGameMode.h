// Game loop: Playing -> (all goals covered) -> Completed -> next level / replay.
// Set it as the GameMode Override of a level (or the project default).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HexSokobanTypes.h"
#include "HexLevelTypes.h"
#include "HexSokobanGameMode.generated.h"

class UHexBoardSubsystem;
class UHexGridMovementComponent;

/** Why the level ended in failure (the HUD words it). */
UENUM(BlueprintType)
enum class EHexFailReason : uint8
{
	/** A box can no longer reach a goal (geometrically lost): restarts by itself. */
	Stuck,
	/** Designed hand: no cards left. */
	OutOfCards,
	/** Designed hand: no held card fits any tile around the player. */
	NoPlayableCard
};

UCLASS()
class HEXSOKOBAN_API AHexSokobanGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHexSokobanGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	bool IsBuildingLevel() const { return bBuildingLevel; }

	/** Built from level data (play map) rather than placed actors (legacy maps)? */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	bool IsDataDrivenLevel() const { return bDataDriven; }

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	EHexFailReason GetFailReason() const { return FailReason; }

	/** Seconds until the results card moves on by itself (-1 = waiting for the player). */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	float GetAutoAdvanceRemaining() const;

	/** Fail with a reason (stuck restarts on its own; card fails wait for undo / restart). */
	void FailLevel(EHexFailReason Reason);

	/** Display name of the current level (data-driven), empty for legacy maps. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	FString GetLevelDisplayName() const;

	/** Puts player and boxes back to the level start. */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban")
	void RestartPuzzle();

	/** Takes back the last move. Not allowed once the level is complete. */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban")
	bool UndoMove();

	/** Opens the next map in LevelSequence; replays the current one if there is none. */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban")
	void GoToNextLevel();

	/** "Continue" input (click / Enter): next level when complete, restart now when failed. */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban")
	void HandleConfirm();

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	EHexLevelState GetLevelState() const { return LevelState; }

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	int32 GetMoveCount() const;

	/** Turns, optimal baseline and stars of the level just won (valid once the level is complete). */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	FHexLevelResult GetLevelResult() const { return LevelResult; }

	/** Star rating for Turns against Baseline (Project Settings > Hex Sokoban > Scoring). */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	static FHexLevelResult RateLevel(int32 Turns, int32 Baseline);

	/** Index of the current map in LevelSequence, or -1. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	int32 GetCurrentLevelIndex() const;

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	bool HasNextLevel() const;

	/** Number of levels in the active sequence. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	int32 GetNumLevels() const;

	/** True while the current position is proven unsolvable (game over follows). */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	bool IsStuck() const { return bStuck; }

	/** Seconds left before a stuck level ends in game over (-1 = no timed game over pending). */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	float GetStuckTimeRemaining() const;

	/** World time of the game over (for UI animation), or -1. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	float GetFailedTime() const { return FailedTime; }

	/** End the level as failed; it restarts after GameOverRestartDelay. */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban")
	void TriggerGameOver();

	/** The position just became unsolvable (warn the player). */
	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban")
	FHexBoardSignature OnStuckDetected;

	/** Stuck -> game over. */
	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban")
	FHexBoardSignature OnLevelFailed;

	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Sokoban", meta = (DisplayName = "On Stuck Detected"))
	void ReceiveStuckDetected();

	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Sokoban", meta = (DisplayName = "On Level Failed"))
	void ReceiveLevelFailed();

	/** World time the level was completed (for UI animation), or -1. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	float GetCompletedTime() const { return CompletedTime; }

	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban")
	FHexLevelCompletedSignature OnLevelCompleted;

	/** Blueprint hook for win UI / sound / VFX. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Sokoban", meta = (DisplayName = "On Level Completed"))
	void ReceiveLevelCompleted(int32 MoveCount);

	/** Blueprint hook after undo / restart. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Sokoban", meta = (DisplayName = "On Board Reset"))
	void ReceiveBoardReset();

protected:
	/** Maps played in order. Empty = use Project Settings > Hex Sokoban > Level Sequence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Sokoban|Levels")
	TArray<TSoftObjectPtr<UWorld>> LevelSequence;

	/** Pause between the last box landing and the "level complete" moment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Sokoban|Levels", meta = (ClampMin = "0.0"))
	float WinCelebrationDelay = 0.35f;

	/** If > 0 and a next level exists, advance automatically this many seconds after winning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Sokoban|Levels", meta = (ClampMin = "0.0"))
	float AutoAdvanceDelay = 0.f;

	UFUNCTION()
	void HandleMoveFinished(const FHexMoveRecord& Move);

	UFUNCTION()
	void HandleBoardReset();

	UFUNCTION()
	void HandlePushBlocked(EHexDirection Direction, EHexMoveResult Result);

	/** Deal cards once the board and the player exist (next tick after BeginPlay). */
	void InitializeCards();

	/** Run the solver and enter / leave the stuck state. */
	void EvaluateStuck();
	void ClearStuck();

	UHexBoardSubsystem* GetBoard() const;
	const TArray<TSoftObjectPtr<UWorld>>& GetLevels() const;
	void WaitForBoxesThenComplete();
	void CompleteLevel();
	UHexGridMovementComponent* GetMover() const;
	void SetLevelState(EHexLevelState NewState);

	EHexLevelState LevelState = EHexLevelState::Playing;
	FTimerHandle AutoAdvanceTimer;
	FTimerHandle WinTimer;
	bool bWinPending = false;
	float CompletedTime = -1.f;
	FHexLevelResult LevelResult;
	bool bDataDriven = false;
	EHexFailReason FailReason = EHexFailReason::Stuck;

	/** Spawn the board of the flow's current level (play map only). */
	void BuildDataDrivenLevel();
	void SpawnEntranceTile(const FHexTileDef& Tile);
	bool bBuildingLevel = false;
	FHexLevelDefinition EntranceDefinition;
	int32 EntranceIndex = 0;
	float EntranceDelay = 0.f;
	struct FEntrancePop
	{
		TWeakObjectPtr<AActor> Actor;
		FVector BaseScale = FVector::OneVector;
		float Value = .55f, Velocity = 6.f, Squash = -.25f, SquashVelocity = 0.f, Age = 0.f;
	};
	TArray<FEntrancePop> EntrancePops;

	bool bStuck = false;
	FTimerHandle StuckTimer;
	FTimerHandle RestartTimer;
	float FailedTime = -1.f;
};
