// Level data helpers shared by the game, the level editor and authoring scripts.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HexLevelTypes.h"
#include "HexDeckSolver.h"
#include "HexLevelTools.generated.h"

class FHexStateSpace;
struct FHexSolverInput;

/** What the solvers say about a definition (editor status pill, save cache, logs). */
USTRUCT(BlueprintType)
struct HEXSOKOBAN_API FHexLevelCheck
{
	GENERATED_BODY()

	/** Playable at all (tiles, start, goals, boxes)? */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Level")
	bool bPlayable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Level")
	FString Problem;

	/** Fewest steps ignoring cards (-1 = unsolvable / too big). */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Level")
	int32 GeometricSteps = -1;

	/** Fewest turns with the designed hand (-1 = no win found). */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Level")
	int32 DeckTurns = -1;

	/** The card search ran out of budget (DeckTurns unknown rather than impossible). */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Level")
	bool bBudgetHit = false;

	/** Tiles without a face (entered for free). */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Level")
	int32 FacelessTiles = 0;
};

UCLASS()
class HEXSOKOBAN_API UHexLevelTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Spawn tiles, goals and boxes of Definition (tile / box classes from Project Settings). */
	static void SpawnLevel(UWorld* World, const FHexLevelDefinition& Definition, TArray<AActor*>* OutActors = nullptr);

	/** Spawn one tile: the suit's terrain tile (a goal is the same terrain plus the goal glow), else the plain classes. */
	static AActor* SpawnTileActor(UWorld* World, const FHexTileDef& Tile);

	/** Solver input (tiles, goals, boxes, start) of a definition. */
	static void MakeSolverInput(const FHexLevelDefinition& Definition, FHexSolverInput& OutInput);

	/** Tile faces as a coordinate map (faceless tiles left out). */
	static TMap<FIntPoint, FHexCard> MakeLabelMap(const FHexLevelDefinition& Definition);

	/** Geometric optimum + card-aware optimum of a definition (synchronous; run it off the game thread in the editor). */
	static FHexLevelCheck CheckLevel(const FHexLevelDefinition& Definition, int32 MaxStates, int32 MaxNodes);

	/** "One-click deck": the faces of the tiles entered along a fewest-steps solution, in order. */
	static bool MakeRecommendedDeck(const FHexLevelDefinition& Definition, TArray<FHexCard>& OutDeck);

	/**
	 * Authoring: capture the board of the current world (tiles, goals, boxes, player, the faces the card system
	 * shows) as a designed level with the recommended deck, and save it as a built-in level (editor builds only).
	 * Returns the file written.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Levels", meta = (WorldContext = "WorldContext"))
	static FString ExportWorldAsBuiltInLevel(const UObject* WorldContext, const FString& DisplayName);
};
