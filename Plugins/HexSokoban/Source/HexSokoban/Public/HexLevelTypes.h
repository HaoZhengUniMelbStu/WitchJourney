// Data-driven levels: a level is plain data (tiles with their card faces, boxes, player start, the designed
// hand), saved as JSON. Built-in levels ship in Content/HexSokoban/BuiltInLevels, player levels go to
// Saved/HexLevels. The play map builds the board from the definition at start (see AHexSokobanGameMode).

#pragma once

#include "CoreMinimal.h"
#include "HexCardTypes.h"
#include "HexLocalization.h"
#include "HexLevelTypes.generated.h"

/** How the cards of a level are dealt. */
UENUM(BlueprintType)
enum class EHexDealMode : uint8
{
	/** Fixed tile faces and a designed hand: every card is in hand from the start, played cards are gone. */
	Designed,
	/** Legacy: random tile faces, 40-card rotating pile with assists (kept for future modes). */
	Random
};

USTRUCT(BlueprintType)
struct HEXSOKOBAN_API FHexTileDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	FIntPoint Coord = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	EHexSuit Suit = EHexSuit::Spade;

	/** 1 = A ... 10 (11-13 reserved for J/Q/K). 0 = no face: entered for free. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level", meta = (ClampMin = "0", ClampMax = "13"))
	int32 Rank = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	bool bGoal = false;

	/** 0 = playable terrain; positive values = decorative, impassable scenery variants. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	int32 SceneryVariant = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SceneryRotationSteps = 0;
	bool CanRotateScenery() const { return IsScenery() && (Suit==EHexSuit::Diamond || Suit==EHexSuit::Club || (Suit==EHexSuit::Heart && SceneryVariant==2)); }
	bool IsScenery() const { return SceneryVariant > 0; }
	int32 GetSceneryVariantCount() const { return Suit==EHexSuit::Club?6:(Suit==EHexSuit::Heart?3:2); }
	FString GetSceneryName() const
	{
		const bool Alt=SceneryVariant==2;
		switch(Suit)
		{
		case EHexSuit::Heart: return SceneryVariant==3?HEXT("普通森林"):(Alt?HEXT("林间小屋"):HEXT("巨木森林"));
		case EHexSuit::Spade: return Alt?HEXT("高原山洞"):HEXT("高原山地");
		case EHexSuit::Diamond: return Alt?HEXT("沉船"):HEXT("码头轮船");
		default: return SceneryVariant>2 ? FHexLocalization::Printf(HEXT("城镇建筑 %02d"),SceneryVariant+1) : (Alt?HEXT("城市"):HEXT("领主城堡"));
		}
	}
	bool HasFace() const { return Rank > 0 && !IsScenery(); }
	FHexCard GetFace() const { return FHexCard(Rank, Suit); }
};

USTRUCT(BlueprintType)
struct HEXSOKOBAN_API FHexLevelDefinition
{
	GENERATED_BODY()

	/** Stable id (file name without extension for built-ins, a GUID for player levels). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	FString LevelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	FString Author;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	int32 Version = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	TArray<FHexTileDef> Tiles;

	/** Box start cells (each must be on a tile). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	TArray<FIntPoint> Boxes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	FIntPoint PlayerStart = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	bool bHasPlayerStart = false;

	/** The designed hand: every card is held from the start (duplicates allowed); a played card is gone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	TArray<FHexCard> Deck;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	EHexDealMode DealMode = EHexDealMode::Designed;

	/** Fewest turns that win with the designed hand, cached when saved (-1 = unknown / unsolved). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	int32 CachedOptimalTurns = -1;

	/** Persisted opt-out of all route/progress prediction for world-sized levels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	bool bPerformanceMode = false;

	bool NeedsPerformanceMode() const
	{
		if(bPerformanceMode || Boxes.Num()>=8) return true;
		int32 PlayableTiles=0;
		for(const FHexTileDef& Tile:Tiles) if(!Tile.IsScenery() && ++PlayableTiles>=80) return true;
		return false;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Level")
	FString CreatedUtc;

	const FHexTileDef* FindTile(const FIntPoint& Coord) const
	{
		return Tiles.FindByPredicate([&Coord](const FHexTileDef& Tile) { return Tile.Coord == Coord; });
	}
	int32 GetNumGoals() const
	{
		int32 Count = 0;
		for (const FHexTileDef& Tile : Tiles)
		{
			Count += Tile.bGoal && !Tile.IsScenery() ? 1 : 0;
		}
		return Count;
	}
	/** Minimum to be playable (not to be solvable): tiles, a start on a tile, boxes >= goals > 0, boxes on tiles. */
	bool IsPlayable(FString* OutReason = nullptr) const;
};

/** One entry of the level list (built-in first, then player levels by creation time). */
USTRUCT(BlueprintType)
struct HEXSOKOBAN_API FHexLevelEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Hex Level")
	FHexLevelDefinition Definition;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Level")
	bool bBuiltIn = false;

	/** JSON file it came from. */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Level")
	FString SourceFile;
};

/** Best result of a level (save game). */
USTRUCT(BlueprintType)
struct HEXSOKOBAN_API FHexLevelRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Hex Level")
	bool bCompleted = false;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Hex Level")
	int32 BestStars = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Hex Level")
	int32 BestTurns = 0;
};
