// Move-exact state space of one level (no reflection).
//
// Every position (player cell + box cells) reachable from the level start is explored once with a forward BFS
// over single player steps (walks and pushes, never pushing a box onto a dead cell), then a reverse BFS from the
// solved positions gives each one its exact "cost to go": the fewest player steps still needed to win.
//
// Uses:
//  - the level's optimal baseline (fewest steps from the start) for the star rating;
//  - the optimal line from any position (the card system's macro reference for dealing / feeding);
//  - an exact, consistent heuristic for the hand-aware hint planner (A* in UHexCardSubsystem).
//
// Small hex levels have a few thousand positions; Build gives up (IsValid() == false) past MaxStates.

#pragma once

#include "CoreMinimal.h"

struct FHexSolverInput;

/** Position key: [0] = player cell, [1..] = sorted box cells. */
struct HEXSOKOBAN_API FHexStateKey
{
	TArray<int16> Cells;

	bool operator==(const FHexStateKey& Other) const { return Cells == Other.Cells; }

	friend uint32 GetTypeHash(const FHexStateKey& Key)
	{
		uint32 Hash = 0;
		for (const int16 Cell : Key.Cells)
		{
			Hash = HashCombine(Hash, static_cast<uint32>(static_cast<uint16>(Cell)));
		}
		return Hash;
	}
};

class HEXSOKOBAN_API FHexStateSpace
{
public:
	static constexpr int32 NumDirs = 6;

	/** Explore from Input. Returns false (and stays invalid) when the board is malformed or too big. */
	bool Build(const FHexSolverInput& Input, int32 MaxStates);

	bool IsValid() const { return bValid; }
	bool WasBudgetExceeded() const { return bBudgetExceeded; }
	int32 GetNumStates() const { return Keys.Num(); }
	int32 GetStartState() const { return StartState; }

	/** Every board cell (tile coordinate) of the level. */
	const TArray<FIntPoint>& GetCells() const { return CoordOf; }

	/** Fewest player steps from the level start to a win (-1 = unsolvable / unknown). */
	int32 GetStartCost() const { return bValid ? GetCost(StartState) : -1; }

	/** State of a board position, INDEX_NONE if never reached (or only reachable through a dead push). */
	int32 FindState(const FIntPoint& Player, const TArray<FIntPoint>& Boxes) const;

	/** Fewest steps to win from State; 0 = solved, -1 = the level can no longer be won from here. */
	int32 GetCost(int32 State) const { return Cost.IsValidIndex(State) ? Cost[State] : -1; }

	/**
	 * One step in Dir (EHexDirection order). Returns the child state, or INDEX_NONE when the step is blocked
	 * or pushes a box onto a dead cell. OutEntered = the cell the player walks onto.
	 */
	int32 Step(int32 State, int32 Dir, FIntPoint& OutEntered) const;

	/** One optimal walk from State to a win: the cells the player enters, in order (empty if solved / lost). */
	bool GetOptimalSteps(int32 State, TArray<FIntPoint>& OutSteps) const;

private:
	int32 Next(int32 Cell, int32 Dir) const { return Neighbors[Cell * NumDirs + Dir]; }

	bool bValid = false;
	bool bBudgetExceeded = false;
	int32 NumCells = 0;
	int32 NumGoals = 0;
	int32 StartState = INDEX_NONE;
	TArray<int32> Neighbors;		// NumCells * 6, -1 = no tile
	TArray<bool> IsGoal;
	TArray<bool> Live;
	TArray<FIntPoint> CoordOf;
	TMap<FIntPoint, int32> CellOf;

	TArray<FHexStateKey> Keys;
	TMap<FHexStateKey, int32> IndexOf;
	TArray<int32> Children;			// NumStates * 6, INDEX_NONE = blocked / dead
	TArray<int32> Cost;
};
