// Hex Sokoban - shared types.
//
// Grid convention (axial coordinates, FIntPoint X = q, Y = r):
//   world offset = q * (S, 0) + r * (S/2, S*sqrt(3)/2), S = TileSpacing (center-to-center).
// The six neighbours therefore sit at world yaw 0/60/120/180/240/300 degrees.
// With the TopDown template camera (yaw 0) "Forward" (+X) is screen-up.

#pragma once

#include "CoreMinimal.h"
#include "HexSokobanTypes.generated.h"

class AHexBox;

HEXSOKOBAN_API DECLARE_LOG_CATEGORY_EXTERN(LogHexSokoban, Log, All);

/** The six hex directions, ordered counter-clockwise by world yaw in 60 degree steps. */
UENUM(BlueprintType)
enum class EHexDirection : uint8
{
	Forward			UMETA(DisplayName = "Forward (+X, yaw 0)"),
	ForwardRight	UMETA(DisplayName = "Forward Right (yaw 60)"),
	BackRight		UMETA(DisplayName = "Back Right (yaw 120)"),
	Back			UMETA(DisplayName = "Back (-X, yaw 180)"),
	BackLeft		UMETA(DisplayName = "Back Left (yaw 240)"),
	ForwardLeft		UMETA(DisplayName = "Forward Left (yaw 300)"),
	Invalid			UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EHexTileType : uint8
{
	Normal,
	Goal
};

UENUM(BlueprintType)
enum class EHexMoveResult : uint8
{
	/** Player stepped onto an empty neighbour tile. */
	Moved,
	/** Player stepped and pushed a box one tile further. */
	Pushed,
	/** No tile in that direction (or no tile behind the box). Player stays. */
	BlockedNoTile,
	/** The box would be pushed into another box. Player stays. */
	BlockedByBox,
	/** A step is in progress (the request may have been buffered). */
	Busy,
	/** Movement is locked, e.g. the level is complete. */
	Locked,
	/** Bad input (no direction / not on the board). */
	Invalid,
	/** Card rule: no card in hand (or the selected card) matches the target tile. Player stays. */
	BlockedNoCard
};

/** Player-guidance look of a tile while a card is dragged / selected / hovered. */
UENUM(BlueprintType)
enum class EHexTileGuide : uint8
{
	None,
	/** The previewed card can move the player here: lifts and glows. */
	Valid,
	/** Valid and under the cursor: lifts higher, glows brighter. */
	ValidHovered,
	/** Not reachable with the previewed card: darkened. */
	Dimmed,
	/** Under the cursor but not reachable: faint red. */
	InvalidHovered
};

UENUM(BlueprintType)
enum class EHexLevelState : uint8
{
	Playing,
	Completed,
	/** Stuck -> game over; the level restarts on its own. */
	Failed
};

UENUM(BlueprintType)
enum class EHexSolveStatus : uint8
{
	/** Every goal can still be covered from here. */
	Solvable,
	/** Proven: no sequence of moves can cover every goal any more. */
	Unsolvable,
	/** Search budget ran out (treated as "not stuck"). */
	Unknown
};

/** Result of analysing the current board with the solver. */
USTRUCT(BlueprintType)
struct HEXSOKOBAN_API FHexSolveResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Hex Sokoban")
	EHexSolveStatus Status = EHexSolveStatus::Unknown;

	/** Fewest pushes still needed (Solvable only, else -1). */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Sokoban")
	int32 MinPushes = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Sokoban")
	int32 StatesExplored = 0;

	/** True when a box sits on a cell from which no goal can ever be reached (instant proof). */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Sokoban")
	bool bDeadBox = false;
};

/** Result of a won level: turns used vs. the optimal baseline, and the star rating. */
USTRUCT(BlueprintType)
struct HEXSOKOBAN_API FHexLevelResult
{
	GENERATED_BODY()

	/** Turns used: steps + discards (undone turns don't count). */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Sokoban")
	int32 Turns = 0;

	/** Fewest steps that win the level from its start (-1 = unknown, no stars then). */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Sokoban")
	int32 Baseline = -1;

	/** 1..3 (3 when the baseline is unknown). */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Sokoban")
	int32 Stars = 0;

	/** Most turns that still give 3 / 2 stars. */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Sokoban")
	int32 ThreeStarTurns = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Sokoban")
	int32 TwoStarTurns = 0;
};

USTRUCT()
struct FHexSkillBoxMove
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<AHexBox> Box = nullptr;
	UPROPERTY() FIntPoint From = FIntPoint::ZeroValue;
	UPROPERTY() FIntPoint To = FIntPoint::ZeroValue;
};

/** One logical move on the board. Stored in the undo history. */
USTRUCT(BlueprintType)
struct HEXSOKOBAN_API FHexMoveRecord
{
	GENERATED_BODY()
	UPROPERTY() int32 SkillRank = 0;
	int32 GetStepCost() const { return SkillRank==12?6:SkillRank==11?4:SkillRank==13?2:1; }
	UPROPERTY() TArray<FHexSkillBoxMove> SkillBoxes;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Sokoban")
	EHexDirection Direction = EHexDirection::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Sokoban")
	FIntPoint PlayerFrom = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Sokoban")
	FIntPoint PlayerTo = FIntPoint::ZeroValue;

	/** Box pushed by this move, or null. */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Sokoban")
	TObjectPtr<AHexBox> PushedBox = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Sokoban")
	FIntPoint BoxFrom = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Sokoban")
	FIntPoint BoxTo = FIntPoint::ZeroValue;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHexMoveSignature, const FHexMoveRecord&, Move);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHexMoveBlockedSignature, EHexDirection, Direction, EHexMoveResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHexBoardSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHexLevelCompletedSignature, int32, MoveCount);
