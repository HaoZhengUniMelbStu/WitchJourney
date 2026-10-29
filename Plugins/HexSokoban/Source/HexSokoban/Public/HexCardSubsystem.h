// "Play a card to move": every tile carries a card label; stepping onto a tile costs one hand card of
// the same suit or the same rank. Played cards rotate back into the draw pile, one card is drawn after
// every step, so the hand size never changes.
//
// Comeback assists (Project Settings > Hex Sokoban > Cards|Assist):
//  1. Fair Deal     - labels stay random, but the draw order is repaired (cards swapped deeper in the pile)
//                     until the solver's winning line can be walked with no mulligan at all; if a deal cannot
//                     be repaired the seed is re-rolled (Max Fair Deal Attempts).
//  2. Lucky Draw    - each draw checks the next tile of the current optimal line; dry draws and refused
//                     moves fill a pity meter; when full, the next draw is the first pile card that fits.
//  Draw Director  - (difficulty) each draw picks, from the top few pile cards, the one that keeps the hand's
//                     reach near a target share of the board, so not every direction is open every turn.
//  Dud Draw       - (difficulty) sometimes the drawn card fits none of the tiles around the player.
//  3. Lucky Mulligan- if no neighbouring tile can be entered, the whole hand is swapped and the new hand
//                     is guaranteed to fit the next tile of the optimal line.
//  Anti-loop Feed  - when the player bounces A-B-A-B, cards that fit the ring of tiles around A and B are put
//                     on top of the pile (tiles on the optimal line first).
//
// The next draw is decided right after the previous one (PrepareNextDraw), so the face-up top of the pile
// the player sees (PeekNextDraw) is always the card they will really get. The optimal line comes from the
// board's state space (fewest steps); the hint line is planned with the actual hand (PlanHandHint, A*).
// The deck always stays 40 unique cards (hand + pile); assists only change the draw order.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HexCardTypes.h"
#include "HexSokobanTypes.h"
#include "HexCardSubsystem.generated.h"

class AHexTile;
class UHexBoardSubsystem;
class UHexGridMovementComponent;

/** Outcome of the hand-aware hint planner. */
enum class EHexHintStatus : uint8
{
	/** No plan (no state space, solved, or the level is lost). */
	None,
	/** The hand can follow an optimal line for these steps. */
	OnOptimal,
	/** The hand can't stay on an optimal line: best detour with these cards. */
	Detour,
	/** No card in hand can move the player (discard to cycle). */
	NoMove,
	/** Designed hand: these cards can no longer win; the line shown is the best that is left. */
	Lost
};

UENUM()
enum class EHexCardPayment : uint8
{
	/** A card was played. */
	Paid,
	/** Tile has no label: enter for free (no card, no draw). */
	Free,
	/** No (or the wrong selected) card fits. */
	Refused
};

UCLASS()
class HEXSOKOBAN_API UHexCardSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	friend class FHexSkillIntegrationTest;

public:
	/** Labels + deck + opening hand for the current level. Called by the game mode once board and player exist. */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Cards")
	void InitializeForLevel();

	/** Card rule active in this level. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	bool IsActive() const { return bActive; }

	/**
	 * Designed level: fixed tile faces and the designed hand (all cards held from the start, a played card
	 * is gone, no draws, no assists, no discard zone). The random deal + assists stay for EHexDealMode::Random.
	 */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	bool IsDesigned() const { return bActive && bDesigned; }

	/** Is there a draw pile (random mode)? The hand widget shows the deck only then. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	bool HasDrawPile() const { return bActive && !bDesigned; }

	/** Fewest turns that win with the designed hand from the start (-1 = unknown / no win). */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	int32 GetDesignedOptimalTurns() const { return DesignedOptimalTurns; }

	/**
	 * Designed hand ran dry: the hand is empty, or no held card fits any tile the player could step onto.
	 * (The level isn't failed just because it can no longer be won with these cards.)
	 */
	bool IsOutOfMoves(FText& OutReason) const;

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	TArray<FHexCard> GetHand() const { return State.Hand; }

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	int32 GetDrawPileCount() const { return State.DrawPile.Num(); }

	/** The card the next draw will give (already decided; false when the pile is empty). */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	bool PeekNextDraw(FHexCard& OutCard) const;
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	bool PeekNextSkill(FHexCard& OutCard) const;
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	int32 GetSkillPileCount() const { return State.SkillPile.Num(); }
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	int32 GetSkillDiscardCount() const { return State.SkillDiscard.Num(); }

	/**
	 * Hint planned with the cards in hand (A* over board positions x used hand cards; exact steps-to-win as
	 * the heuristic). Picks the payable sequence with the fewest estimated total steps, the longest among
	 * equals. OutSteps = tiles to enter, in order.
	 */
	EHexHintStatus PlanHandHint(TArray<FIntPoint>& OutSteps) const;

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	int32 GetMulliganCount() const { return State.MulliganCount; }

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	int32 GetPityMeter() const { return State.PityMeter; }

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	int32 GetSeed() const { return DealSeed; }

	/** 0 at the start of a level, 1 once Difficulty Ramp Steps cards were played (Project Settings > Cards|Draw). */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	float GetDifficultyProgress() const;

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	bool GetTileLabel(FIntPoint Coord, FHexCard& OutLabel) const;

	// ---- Selection (UI) ----

	/** Select a hand card for the next move. -1 clears; selecting the selected card toggles it off. */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Cards")
	void SelectCard(int32 HandIndex);

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	int32 GetSelectedCardIndex() const;

	// ---- Queries ----

	/** Could this hand card pay for entering Tile? (true for unlabelled tiles) */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	bool CanCardEnterTile(int32 HandIndex, const AHexTile* Tile) const;

	/** Could any hand card pay for entering Tile? */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	bool CanEnterTile(const AHexTile* Tile) const;

	/** Neighbouring tiles the player could move to right now with this card (-1 = with any card). */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	TArray<AHexTile*> GetEnterableNeighbors(int32 HandIndex = -1) const;

	/** Does this card fit at least one neighbouring tile right now? */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	bool IsCardPlayableNow(int32 HandIndex) const;

	// ---- Movement gate (called by UHexGridMovementComponent) ----

	EHexCardPayment PayForStep(AHexTile* TargetTile);
	void NotifyStepCompleted();
	void NotifyPaymentRefused(AHexTile* TargetTile);

	/** The step itself is impossible (box can't move): red feedback + message. */
	void ReportBlockedMove(AHexTile* TargetTile, EHexMoveResult Result);

	/** A card was dropped on a tile that is not next to the player. */
	void ReportNotNeighbor(AHexTile* Tile);

	// ---- Player guidance ----

	/**
	 * Preview what HandIndex can do: reachable neighbours lift and glow; with bFocus (dragging / selected)
	 * every other tile darkens and HoverTile shows green-ready or red. Call every frame while previewing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Cards|Guide")
	void UpdateGuide(int32 HandIndex, bool bFocus, AHexTile* HoverTile);

	/** Stop previewing (back to the plain "enterable with any card" glow). */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Cards|Guide")
	void ClearGuide(bool bSnap = false);

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards|Guide")
	bool IsGuideActive() const { return bGuideActive; }

	/** Is Tile one the previewed card can reach right now? */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards|Guide")
	bool IsGuideTarget(const AHexTile* Tile) const;

	/** Hand index being previewed (hovered / dragged / selected card), -1 when none. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards|Guide")
	int32 GetGuidePreviewIndex() const { return bGuideActive ? GuidePreviewIndex : INDEX_NONE; }

	/** True while dragging or with a card selected (not just hovering it). */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards|Guide")
	bool IsGuideFocused() const { return bGuideActive && bGuideFocus; }

	/** Tile under the cursor while focused (may be unreachable). */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards|Guide")
	AHexTile* GetGuideHoverTile() const { return bGuideActive ? GuideHoverTile.Get() : nullptr; }

	/** Why HandIndex (-1 = whole hand) can't pay for Tile, as a player-facing sentence. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards|Guide")
	FText DescribeCardRefusal(int32 HandIndex, const AHexTile* Tile) const;

	/** Player guidance message (refused move and the reason). */
	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban|Cards|Guide")
	FHexGuideMessageSignature OnGuideMessage;

	// ---- Undo / restart (called by the game mode) ----

	void UndoLastPlay();
	void RestoreInitialState();

	// ---- Discard zone ----

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards|Discard")
	int32 GetDiscardCharges() const { return State.DiscardCharges; }

	/** Max Discards from the project settings (0 = the discard zone is off). */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards|Discard")
	int32 GetMaxDiscardCharges() const;

	/** Can a card be discarded right now? OutReason explains a "no" to the player. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards|Discard")
	bool CanDiscard(FText& OutReason) const;

	/**
	 * Throw HandIndex away and skip the turn: the pawn stays, one discard charge is spent, a new card is drawn.
	 * Undoable like a move. Returns false (and reports why) when not allowed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Cards|Discard")
	bool DiscardCard(int32 HandIndex);

	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban|Cards|Discard")
	FHexCardSignature OnCardDiscarded;

	/** Discard charges changed (spent, refunded, undo, restart). */
	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban|Cards|Discard")
	FHexHandSignature OnDiscardChargesChanged;

	/** Swap the whole hand (lucky). Runs automatically when the player has no way to move. */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Cards")
	void Mulligan();

	/** Re-evaluate which tiles glow as reachable. */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Cards")
	void RefreshHighlights();

	/** Remove all tile highlights (while the pawn walks, on win / game over). */
	void ClearHighlights();

	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban|Cards")
	FHexHandSignature OnHandDealt;

	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban|Cards")
	FHexCardPlayedSignature OnCardPlayed;

	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban|Cards")
	FHexCardSignature OnCardDrawn;

	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban|Cards")
	FHexHandSignature OnMulligan;

	/** Hand content / order changed (draw, play, undo, restart...). */
	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban|Cards")
	FHexHandSignature OnHandChanged;

	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban|Cards")
	FHexHandSignature OnSelectionChanged;

	/** A move was refused because no (selected) card fits. */
	UPROPERTY(BlueprintAssignable, Category = "Hex Sokoban|Cards")
	FHexHandSignature OnPlayRejected;

private:
	UHexBoardSubsystem* GetBoard() const;
	UHexGridMovementComponent* GetMover() const;
	const FHexCard* FindLabel(const FIntPoint& Coord) const;
	int32 ChooseCardFor(const FHexCard& Label) const;
	void ReturnToPile(FHexCard Card);
	/** Take the (already decided) top card of the pile into the hand. */
	void DrawOne();

	/**
	 * Decide the next draw now and put it on top of the pile: anti-loop feed > lucky draw > dud > director.
	 * bAllowDud = false after a discard: the replacement for it is never a deliberate dud.
	 */
	void PrepareNextDraw(bool bAllowDud = true);

	/** Anti-loop feed: bouncing between two tiles puts cards for the tiles around them on top. */
	bool TryAntiLoopFeed();

	bool GetNextLineLabel(FHexCard& OutLabel) const;

	/** Labels of the first Count labelled tiles along the optimal line from the current position. */
	void GetLineLabels(int32 Count, TArray<FHexCard>& OutLabels) const;
	bool PullMatchingToTop(const FHexCard& Label, int32 Depth);

	/** Labels of the tiles the player could step onto right now (geometrically). */
	void GetNeighborLabels(TArray<FHexCard>& OutLabels) const;

	/** Dud Draw: move a pile card that fits none of the neighbouring tiles to the top. */
	bool PullDudToTop();

	/** Share of labelled tiles that Cards can pay for (0..1). */
	float GetCoverage(const TArray<FHexCard>& Cards) const;

	/** Draw Director: move the best of the top cards (for the current hand) to the top of the pile. */
	void DirectNextDraw();

	/** Deal a fresh hand: first card fits the next step of the winning line, the rest go through the director. */
	void DealHand(int32 HandSize, bool bMarkLucky);
	bool HasAnyMove() const;
	bool HasAnyGeometricMove() const;
	void ApplyLabelsToBoard();

	/** Deck + labels for one seed. */
	void GenerateDeal(int32 Seed, TMap<FIntPoint, FHexCard>& OutLabels, TArray<FHexCard>& OutPile) const;

	/**
	 * Fair deal: greedy play-through of Steps (no mulligan). Whenever the hand cannot pay a step, the most
	 * recently drawn card is swapped (in the pile order) with a not-yet-drawn card that fits, and the line is
	 * replayed. Returns how many steps the (repaired) InOutPile can pay.
	 */
	int32 RepairDeal(const TArray<FIntPoint>& Steps, const TMap<FIntPoint, FHexCard>& InLabels, TArray<FHexCard>& InOutPile) const;

	static TArray<FHexCard> MakeDeck();

	bool bActive = false;
	bool bDesigned = false;
	int32 DesignedOptimalTurns = -1;

	/** Designed level set-up (labels + hand from the definition). */
	void InitializeDesigned(const struct FHexLevelDefinition& Definition);
	void InitializeSkillDeck();
	void PrepareNextSkill();
	void ReturnSkill(FHexCard Card);
	int32 SkillSuitWeights[4] = {0,0,0,0};
	void AwardSkill();
	int32 DealSeed = 0;

	UPROPERTY(Transient)
	FHexCardState State;

	UPROPERTY(Transient)
	FHexCardState InitialState;

	UPROPERTY(Transient)
	TArray<FHexCardState> History;

	TMap<FIntPoint, FHexCard> Labels;

	bool bHasSelection = false;
	FHexCard SelectedCard;

	FRandomStream ShuffleStream;
	bool bGuideActive = false;
	int32 GuidePreviewIndex = INDEX_NONE;
	bool bGuideFocus = false;
	TWeakObjectPtr<AHexTile> GuideHoverTile;
	TArray<TWeakObjectPtr<AHexTile>> GuideTargets;
	TArray<TWeakObjectPtr<AHexTile>> HighlightedTiles;
};
