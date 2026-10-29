// Card data for the "play a card to move" rule.
// A deck holds 40 unique number cards: rank 1-10 (1 = "A") x 4 suits. Ranks 11-13 and jokers are
// reserved for future skill cards (Kind = Skill) and never generated yet.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HexCardTypes.generated.h"

class AHexTile;

UENUM(BlueprintType)
enum class EHexSuit : uint8
{
	Spade,
	Heart,
	Club,
	Diamond
};

UENUM(BlueprintType)
enum class EHexCardKind : uint8
{
	Number,
	/** Reserved: J / Q / K / jokers as skill cards (not implemented yet). */
	Skill
};

UENUM(BlueprintType)
enum class EHexCardReturnPolicy : uint8
{
	/** Played cards go to the bottom of the draw pile (FIFO rotation). */
	Bottom,
	/** Played cards are shuffled back into the draw pile. */
	ShuffleIntoPile
};

UENUM(BlueprintType)
enum class EHexCardVisualState : uint8
{
	Normal,
	Hovered,
	Selected,
	/** Cannot enter any neighbouring tile right now. */
	Unplayable
};

USTRUCT(BlueprintType)
struct HEXSOKOBAN_API FHexCard
{
	GENERATED_BODY()

	/** 1 = A ... 10. 11-13 reserved for J/Q/K skill cards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card", meta = (ClampMin = "1", ClampMax = "13"))
	int32 Rank = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	EHexSuit Suit = EHexSuit::Spade;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
	EHexCardKind Kind = EHexCardKind::Number;

	/** Handed out by the lucky-draw assist (UI may glow it). Not part of the card's identity. */
	UPROPERTY(BlueprintReadOnly, Category = "Card")
	bool bLucky = false;

	FHexCard() = default;
	FHexCard(int32 InRank, EHexSuit InSuit) : Rank(InRank), Suit(InSuit) {}

	/** Can this card be played to enter a tile showing Label? Same rank OR same suit. */
	bool Matches(const FHexCard& Label) const
	{
		return Kind == EHexCardKind::Number && (Rank == Label.Rank || Suit == Label.Suit);
	}

	/** Same printed face (ignores the lucky flag). */
	bool SameFace(const FHexCard& Other) const
	{
		return Rank == Other.Rank && Suit == Other.Suit && Kind == Other.Kind;
	}

	bool IsRed() const { return Suit == EHexSuit::Heart || Suit == EHexSuit::Diamond; }

	FString GetRankText() const;
	FString GetSuitSymbol() const;
	FString ToString() const { return GetRankText() + GetSuitSymbol(); }
};

/** Everything needed to undo / restart the card state. */
USTRUCT()
struct HEXSOKOBAN_API FHexCardState
{
	GENERATED_BODY()
	/** Remaining current-round skills, visible top is index zero. Played cards wait in SkillDiscard. */
	UPROPERTY() TArray<FHexCard> SkillPile;
	UPROPERTY() TArray<FHexCard> SkillDiscard;
	UPROPERTY() FRandomStream SkillRandom;
	UPROPERTY() bool bRewardSkill = false;

	UPROPERTY()
	TArray<FHexCard> Hand;

	/** Index 0 = top. */
	UPROPERTY()
	TArray<FHexCard> DrawPile;

	UPROPERTY()
	int32 PityMeter = 0;

	UPROPERTY()
	int32 MulliganCount = 0;

	/** Discards left (Project Settings > Cards > Max Discards). Part of the state so undo / restart restore it. */
	UPROPERTY()
	int32 DiscardCharges = 0;

	/** Cards on top of the pile placed by the anti-loop feed; the director / dud leave them alone. */
	UPROPERTY()
	int32 ProtectedDraws = 0;

	/** Board move count when the anti-loop feed last fired (-1 = never): one feed per bouncing streak. */
	UPROPERTY()
	int32 AntiLoopMoveStamp = -1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHexHandSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHexCardSignature, const FHexCard&, Card);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHexCardPlayedSignature, const FHexCard&, Card, AHexTile*, TargetTile);
/** Player guidance: why a move was refused (Tile may be null). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHexGuideMessageSignature, const FText&, Message, AHexTile*, Tile);

UCLASS()
class HEXSOKOBAN_API UHexCardLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** "A", "2" ... "10". */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	static FString GetRankText(const FHexCard& Card) { return Card.GetRankText(); }

	/** Unicode suit symbol. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	static FString GetSuitSymbol(const FHexCard& Card) { return Card.GetSuitSymbol(); }

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	static bool IsRedCard(const FHexCard& Card) { return Card.IsRed(); }

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	static bool CardMatches(const FHexCard& Card, const FHexCard& TileLabel) { return Card.Matches(TileLabel); }

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	static FString CardToString(const FHexCard& Card) { return Card.ToString(); }

	/** Glyph atlas cell: ranks 1-10 -> 0-9, suits -> 10-13 (4x4 atlas). */
	static int32 GetRankAtlasIndex(int32 Rank) { return FMath::Clamp(Rank, 1, 10) - 1; }
	static int32 GetSuitAtlasIndex(EHexSuit Suit) { return 10 + static_cast<int32>(Suit); }
};
