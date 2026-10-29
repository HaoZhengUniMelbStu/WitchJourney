// The authoritative puzzle state: which tile is where, which box sits on which tile,
// move validation, undo history and restart snapshot. One per world.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HexSokobanTypes.h"
#include "HexBoardSubsystem.generated.h"

class AHexTile;
class AHexBox;
class UHexGridMovementComponent;
struct FHexSolverInput;
class FHexStateSpace;

UCLASS()
class HEXSOKOBAN_API UHexBoardSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Board")
	bool IsPerformanceMode() const;
	void EnablePerformanceMode() const { bRuntimePerformanceMode = true; }
	mutable bool bRuntimePerformanceMode = false;
	/** Scans the world for HexTile / HexBox actors and rebuilds the board. Clears history. */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Board")
	void RebuildBoard();

	void EnsureBoardBuilt();

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Board")
	bool IsBoardBuilt() const { return bBuilt; }

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Board")
	AHexTile* GetTileAt(FIntPoint Coord) const;

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Board")
	AHexBox* GetBoxAt(FIntPoint Coord) const;

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Board")
	AHexTile* FindNearestTile(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Board")
	TArray<AHexTile*> GetAllTiles() const;

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Board")
	TArray<AHexBox*> GetAllBoxes() const;

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Board")
	int32 GetNumGoals() const;

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Board")
	int32 GetNumCoveredGoals() const;

	/** True when there is at least one goal and every goal tile holds a box. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Board")
	bool AreAllGoalsCovered() const;

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Board")
	int32 GetMoveCount() const { return ScoredMoveCount; }
	int32 GetActionCount() const { return History.Num(); }

	/** Can every goal still be covered from the current position? Exact (see HexSokobanSolver.h). */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Board")
	FHexSolveResult AnalyzeBoard(int32 MaxStates = 200000) const;

	/**
	 * One optimal solution from the current position, as the tiles the player will enter in order: fewest
	 * player steps (state space), or fewest pushes if the level is too big for the state space.
	 * Returns false when unsolvable / unknown / already solved (OutSteps empty).
	 */
	bool GetSolutionPlayerSteps(TArray<FIntPoint>& OutSteps, int32 MaxStates = 200000) const;

	/** Every position reachable from the restart state with its exact steps-to-win (null if too big / not built). */
	const FHexStateSpace* GetStateSpace() const;

	/** State-space index of the current position (INDEX_NONE if unknown). */
	int32 GetCurrentStateIndex() const;

	/** Fewest player steps from the level start to a win: the optimal baseline (-1 = unknown). */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Board")
	int32 GetBaselineMoves() const;

	/** Fewest player steps from the current position to a win (-1 = unknown / lost). */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Board")
	int32 GetMovesToWin() const;

	/** Where the player stood, newest first: the current cell, then before each real step (passes skipped). */
	void GetRecentPlayerCells(int32 Count, TArray<FIntPoint>& OutCells) const;

	/** Called by the mover when a push into a box was refused. */
	void NotifyPushBlocked(EHexDirection Direction, EHexMoveResult Result);

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Board")
	UHexGridMovementComponent* GetPlayerMover() const { return Mover.Get(); }

	/** Pure rule check: can a player standing on From step in Direction? Fills OutRecord on success. */
	EHexMoveResult EvaluateMove(const FIntPoint& From, EHexDirection Direction, FHexMoveRecord& OutRecord) const;
	bool EvaluateSkill(FIntPoint From, FIntPoint Target, const struct FHexCard& Card, FHexMoveRecord& OutRecord) const;

	/** Applies a validated move: updates occupancy, hands the box to the pusher, records history. */
	void CommitMove(const FHexMoveRecord& Record, AActor* Pusher, float PusherRadius);

	/** True while any box is still being carried or sliding. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Board")
	bool IsAnyBoxMoving() const;

	/** Called by the mover when the player's step animation has finished. */
	void NotifyMoveFinished(const FHexMoveRecord& Record);

	void RegisterMover(UHexGridMovementComponent* InMover);
	void UnregisterMover(UHexGridMovementComponent* InMover);

	/** Remembers the current player + box positions as the restart state. */
	void CaptureInitialState();

	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Board")
	bool UndoLastMove();

	/**
	 * Record a turn in which the pawn stays put (card discarded). Counts as a step and is undone like one,
	 * so the board and card histories stay in lockstep.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Board")
	bool CommitPass();

	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Board")
	void RestoreInitialState();

	/** A move was committed (logical state already updated, animations starting). */
	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban|Board")
	FHexMoveSignature OnMoveStarted;

	/** The player's step animation finished. Win checks happen here. */
	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban|Board")
	FHexMoveSignature OnMoveFinished;

	/** The player tried to push a box that cannot move. */
	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban|Board")
	FHexMoveBlockedSignature OnPushBlocked;

	/** Board state jumped (undo or restart). */
	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban|Board")
	FHexBoardSignature OnBoardReset;

private:
	void BuildSolverInput(FHexSolverInput& Input) const;

	/** Occupancy + tile events. Pusher == nullptr places the box instantly. */
	void MoveBoxOnBoard(AHexBox* Box, const FIntPoint& From, const FIntPoint& To, EHexDirection Direction, AActor* Pusher, float PusherRadius);

	TMap<FIntPoint, TWeakObjectPtr<AHexTile>> Tiles;
	TMap<FIntPoint, TWeakObjectPtr<AHexBox>> Boxes;

	UPROPERTY(Transient)
	TArray<FHexMoveRecord> History;
	int32 ScoredMoveCount = 0;

	TWeakObjectPtr<UHexGridMovementComponent> Mover;

	bool bBuilt = false;
	bool bHasInitialState = false;
	FIntPoint InitialPlayerCoord = FIntPoint::ZeroValue;
	TArray<TWeakObjectPtr<AHexBox>> InitialBoxes;
	TArray<FIntPoint> InitialBoxCoords;

	/** Built from the restart state (CaptureInitialState); positions only ever come from that start. */
	mutable TSharedPtr<FHexStateSpace> StateSpace;
	mutable bool bStateSpaceTried = false;
};
