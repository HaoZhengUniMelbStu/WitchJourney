// Exact deadlock detection / solution search for hex Sokoban (no reflection).
//
//  1. Live cells: a cell is live if a box standing there can still be pushed onto some goal
//     (ignoring other boxes). Computed backwards from the goals: a box reaches X from Y = X - d
//     only if Y and Y - d (where the pusher stands) are tiles. Fewer boxes on live cells than goals
//     -> unsolvable, instantly.
//  2. Breadth-first search over push states (sorted box cells + the player's reachable region,
//     normalised to its lowest cell), never pushing a box onto a dead cell. No state with every goal
//     covered -> unsolvable. Running out of budget returns Unknown, which callers treat as "not stuck".
//  3. FindSolutionSteps additionally rebuilds one shortest-push solution as the exact list of tiles the
//     player walks onto (walking steps + pushing steps), used by the card system's fairness / pity logic.

#pragma once

#include "CoreMinimal.h"
#include "HexSokobanTypes.h"

struct HEXSOKOBAN_API FHexSolverInput
{
	TArray<FIntPoint> Tiles;
	TArray<FIntPoint> Goals;
	TArray<FIntPoint> Boxes;
	FIntPoint Player = FIntPoint::ZeroValue;
};

class HEXSOKOBAN_API FHexSokobanSolver
{
public:
	static FHexSolveResult Solve(const FHexSolverInput& Input, int32 MaxStates);

	/**
	 * Like Solve, and on success fills OutPlayerSteps with every tile the player enters, in order
	 * (empty when already solved). Returns the solve status.
	 */
	static FHexSolveResult FindSolutionSteps(const FHexSolverInput& Input, int32 MaxStates, TArray<FIntPoint>& OutPlayerSteps);
};
