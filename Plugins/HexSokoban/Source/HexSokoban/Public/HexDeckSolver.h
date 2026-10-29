// Card-aware solver for designed levels (no reflection).
//
// A* over (board position from FHexStateSpace, cards still in hand as a multiset). Each step must be paid with
// a held card of the same suit or rank as the tile entered (unlabelled tiles are free); a played card is gone.
// h = the position's exact steps-to-win (ignores cards: admissible and consistent), so the first solved node
// popped is a solution with the fewest turns. When the hand can't win, the best partial line is returned
// (lowest estimated total, then deepest) - the hint still points somewhere sensible.

#pragma once

#include "CoreMinimal.h"
#include "HexCardTypes.h"

class FHexStateSpace;

struct HEXSOKOBAN_API FHexDeckSolveResult
{
	/** A winning line exists (Steps reaches a win). */
	bool bSolved = false;

	/** The search ran out of budget before deciding. */
	bool bBudgetHit = false;

	/** Fewest turns to win from the root (-1 = unsolved). */
	int32 Turns = -1;

	/** Cells entered, in order: the full solution, or the best partial line when unsolved. */
	TArray<FIntPoint> Steps;

	/** Card paid for each step (default card for free tiles). */
	TArray<FHexCard> Cards;

	/** Nodes expanded (for logging). */
	int32 Expanded = 0;
};

class HEXSOKOBAN_API FHexDeckSolver
{
public:
	static FHexDeckSolveResult Solve(const FHexStateSpace& Space, int32 RootState, const TMap<FIntPoint, FHexCard>& Labels,
		const TArray<FHexCard>& Hand, int32 MaxNodes);
};
