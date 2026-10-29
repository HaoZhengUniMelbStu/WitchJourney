#include "HexCardSubsystem.h"
#include "HexLocalization.h"
#include "HexAudioSubsystem.h"
#include "HexSokobanGameMode.h"
#include "HexSokobanMath.h"
#include "HexBoardSubsystem.h"
#include "HexGridLibrary.h"
#include "HexSokobanStateSpace.h"
#include "HexSokobanSolver.h"
#include "HexDeckSolver.h"
#include "HexLevelTools.h"
#include "HexGameFlowSubsystem.h"
#include "HexGridMovementComponent.h"
#include "HexSokobanSettings.h"
#include "HexTile.h"
#include "HexBox.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

namespace
{
	/** How many upcoming line labels the fair-deal simulation looks at when choosing which card to spend. */
	constexpr int32 SimulationLookahead = 3;

	void ShuffleCards(TArray<FHexCard>& Cards, FRandomStream& Stream)
	{
		for (int32 Index = Cards.Num() - 1; Index > 0; --Index)
		{
			const int32 Other = Stream.RandRange(0, Index);
			Cards.Swap(Index, Other);
		}
	}

	/** Pick the matching card that is least useful for the next few labels (keep flexible cards). */
	int32 PickForSimulation(const TArray<FHexCard>& Hand, const FHexCard& Label, const TArray<const FHexCard*>& Upcoming)
	{
		int32 Best = INDEX_NONE;
		int32 BestScore = TNumericLimits<int32>::Max();
		for (int32 Index = 0; Index < Hand.Num(); ++Index)
		{
			if (!Hand[Index].Matches(Label))
			{
				continue;
			}
			int32 Score = 0;
			for (const FHexCard* Next : Upcoming)
			{
				Score += (Next && Hand[Index].Matches(*Next)) ? 1 : 0;
			}
			if (Score < BestScore)
			{
				BestScore = Score;
				Best = Index;
			}
		}
		return Best;
	}
}

// ---------------------------------------------------------------- helpers

TArray<FHexCard> UHexCardSubsystem::MakeDeck()
{
	TArray<FHexCard> Deck;
	Deck.Reserve(40);
	for (int32 SuitIndex = 0; SuitIndex < 4; ++SuitIndex)
	{
		for (int32 Rank = 1; Rank <= 10; ++Rank)
		{
			Deck.Add(FHexCard(Rank, static_cast<EHexSuit>(SuitIndex)));
		}
	}
	return Deck;
}

UHexBoardSubsystem* UHexCardSubsystem::GetBoard() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UHexBoardSubsystem>() : nullptr;
}

UHexGridMovementComponent* UHexCardSubsystem::GetMover() const
{
	const UHexBoardSubsystem* Board = GetBoard();
	return Board ? Board->GetPlayerMover() : nullptr;
}

const FHexCard* UHexCardSubsystem::FindLabel(const FIntPoint& Coord) const
{
	return Labels.Find(Coord);
}

bool UHexCardSubsystem::GetTileLabel(FIntPoint Coord, FHexCard& OutLabel) const
{
	if (const FHexCard* Label = FindLabel(Coord))
	{
		OutLabel = *Label;
		return true;
	}
	return false;
}

// ---------------------------------------------------------------- dealing

void UHexCardSubsystem::GenerateDeal(int32 Seed, TMap<FIntPoint, FHexCard>& OutLabels, TArray<FHexCard>& OutPile) const
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	FRandomStream Stream(Seed);
	OutLabels.Reset();

	const UHexBoardSubsystem* Board = GetBoard();
	TArray<AHexTile*> Tiles = Board ? Board->GetAllTiles() : TArray<AHexTile*>();
	Tiles.Sort([](const AHexTile& A, const AHexTile& B)
	{
		return A.GetCoord().Y != B.GetCoord().Y ? A.GetCoord().Y < B.GetCoord().Y : A.GetCoord().X < B.GetCoord().X;
	});

	// Board labels: a separate 40-card face set, no repeats until it runs out.
	TArray<FHexCard> LabelSet;
	for (const AHexTile* Tile : Tiles)
	{
		if (Tile->IsGoal() && !Settings.bLabelGoalTiles)
		{
			continue;
		}
		if (LabelSet.Num() == 0)
		{
			LabelSet = MakeDeck();
			ShuffleCards(LabelSet, Stream);
		}
		OutLabels.Add(Tile->GetCoord(), LabelSet.Pop(EAllowShrinking::No));
	}

	OutPile = MakeDeck();
	ShuffleCards(OutPile, Stream);
}

namespace
{
	struct FSimCard
	{
		FHexCard Card;
		int32 Origin = INDEX_NONE;	// position in the dealt pile, INDEX_NONE once the card was played back
	};

	/** One greedy play-through. On a dead end reports the newest drawn card and the first undrawn card that fits. */
	int32 SimulateTagged(const TArray<FIntPoint>& Steps, const TMap<FIntPoint, FHexCard>& InLabels, const TArray<FHexCard>& Dealt,
		int32 HandSize, int32& OutNewestDrawn, int32& OutFittingUndrawn)
	{
		OutNewestDrawn = INDEX_NONE;
		OutFittingUndrawn = INDEX_NONE;

		TArray<FSimCard> Pile;
		Pile.Reserve(Dealt.Num());
		for (int32 Index = 0; Index < Dealt.Num(); ++Index)
		{
			Pile.Add({ Dealt[Index], Index });
		}
		TArray<FSimCard> Hand;
		for (int32 Index = 0; Index < HandSize && Pile.Num() > 0; ++Index)
		{
			Hand.Add(Pile[0]);
			Pile.RemoveAt(0);
		}

		TArray<FHexCard> HandCards;
		for (int32 StepIndex = 0; StepIndex < Steps.Num(); ++StepIndex)
		{
			const FHexCard* Label = InLabels.Find(Steps[StepIndex]);
			if (!Label)
			{
				continue; // free tile
			}
			TArray<const FHexCard*> Upcoming;
			for (int32 Ahead = StepIndex + 1; Ahead < Steps.Num() && Upcoming.Num() < SimulationLookahead; ++Ahead)
			{
				Upcoming.Add(InLabels.Find(Steps[Ahead]));
			}
			HandCards.Reset();
			for (const FSimCard& Held : Hand)
			{
				HandCards.Add(Held.Card);
			}
			const int32 Pick = PickForSimulation(HandCards, *Label, Upcoming);
			if (Pick == INDEX_NONE)
			{
				for (const FSimCard& Held : Hand)
				{
					OutNewestDrawn = FMath::Max(OutNewestDrawn, Held.Origin);
				}
				for (const FSimCard& Waiting : Pile)
				{
					if (Waiting.Origin != INDEX_NONE && Waiting.Card.Matches(*Label))
					{
						OutFittingUndrawn = Waiting.Origin;
						break;
					}
				}
				return StepIndex;
			}
			Pile.Add({ Hand[Pick].Card, INDEX_NONE });
			Hand.RemoveAt(Pick);
			if (Pile.Num() > 0)
			{
				Hand.Add(Pile[0]);
				Pile.RemoveAt(0);
			}
		}
		return Steps.Num();
	}
}

int32 UHexCardSubsystem::RepairDeal(const TArray<FIntPoint>& Steps, const TMap<FIntPoint, FHexCard>& InLabels, TArray<FHexCard>& InOutPile) const
{
	const int32 HandSize = FMath::Clamp(UHexSokobanSettings::Get().HandSize, 1, 10);
	int32 Best = 0;
	TArray<FHexCard> BestPile = InOutPile;
	const int32 MaxRepairs = 4 * Steps.Num() + 8;
	for (int32 Repair = 0; Repair <= MaxRepairs; ++Repair)
	{
		int32 NewestDrawn = INDEX_NONE;
		int32 FittingUndrawn = INDEX_NONE;
		const int32 Walked = SimulateTagged(Steps, InLabels, InOutPile, HandSize, NewestDrawn, FittingUndrawn);
		if (Walked > Best || Repair == 0)
		{
			Best = Walked;
			BestPile = InOutPile;
		}
		if (Walked >= Steps.Num() || NewestDrawn == INDEX_NONE || FittingUndrawn == INDEX_NONE)
		{
			break;
		}
		// Everything before the newest draw stays identical; from then on the fitting card is in hand.
		InOutPile.Swap(NewestDrawn, FittingUndrawn);
	}
	InOutPile = MoveTemp(BestPile);
	return Best;
}

void UHexCardSubsystem::InitializeForLevel()
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	UHexBoardSubsystem* Board = GetBoard();

	ClearHighlights();
	History.Reset();
	bHasSelection = false;
	bDesigned = false;
	DesignedOptimalTurns = -1;
	bActive = Settings.bEnableCardMovement && Board && GetMover();
	if (!bActive)
	{
		Labels.Reset();
		ApplyLabelsToBoard();
		return;
	}
	const AHexSokobanGameMode* LevelMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHexSokobanGameMode>() : nullptr;
	const UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this);
	const FHexLevelDefinition* Definition = (Flow && LevelMode && LevelMode->IsDataDrivenLevel()) ? Flow->GetCurrentDefinition() : nullptr;
	if (Definition && Definition->DealMode == EHexDealMode::Designed)
	{
		InitializeDesigned(*Definition);
		return;
	}

	const int32 BaseSeed = Settings.RandomSeed != 0
		? Settings.RandomSeed
		: static_cast<int32>(FPlatformTime::Cycles() & 0x7fffffff) | 1;

	TArray<FIntPoint> Line;
	Board->GetSolutionPlayerSteps(Line, Settings.SolverMaxStates);

	const int32 Attempts = (Settings.bFairDeal && Line.Num() > 0) ? FMath::Max(1, Settings.MaxFairDealAttempts) : 1;
	int32 BestSeed = BaseSeed;
	int32 BestSteps = -1;
	TArray<FHexCard> Pile;
	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		const int32 Seed = BaseSeed + Attempt * 7919;
		TMap<FIntPoint, FHexCard> TryLabels;
		TArray<FHexCard> TryPile;
		GenerateDeal(Seed, TryLabels, TryPile);
		const int32 Walked = (Settings.bFairDeal && Line.Num() > 0) ? RepairDeal(Line, TryLabels, TryPile) : 0;
		if (Walked > BestSteps)
		{
			BestSteps = Walked;
			BestSeed = Seed;
			Labels = MoveTemp(TryLabels);
			Pile = MoveTemp(TryPile);
		}
		if (Walked >= Line.Num())
		{
			break;
		}
	}

	DealSeed = BestSeed;
	ShuffleStream.Initialize(DealSeed ^ 0x5f3759df);

	State = FHexCardState();
	State.DrawPile = MoveTemp(Pile);
	State.DiscardCharges = GetMaxDiscardCharges();
	DealHand(FMath::Clamp(Settings.HandSize, 1, 10), false);

	if (Line.Num() > 0)
	{
		UE_LOG(LogHexSokoban, Log, TEXT("Cards dealt (seed %d): %s winning line of %d steps%s."),
			DealSeed, BestSteps >= Line.Num() ? TEXT("fair deal -") : TEXT("best effort -"), Line.Num(),
			BestSteps >= Line.Num() ? TEXT(" fair start") : *FString::Printf(TEXT(", %d steps covered (assists will help)"), BestSteps));
	}
	else
	{
		UE_LOG(LogHexSokoban, Log, TEXT("Cards dealt (seed %d)."), DealSeed);
	}

	ApplyLabelsToBoard();

	if (Settings.bAutoMulligan && !HasAnyMove() && HasAnyGeometricMove())
	{
		Mulligan();	// also decides the first draw
		State.MulliganCount = 0; // opening swap is free
	}
	else
	{
		PrepareNextDraw(true);
	}

	InitializeSkillDeck();
	InitialState = State;
	UHexAudioSubsystem::Deal(this, State.Hand.Num());
	OnHandDealt.Broadcast();
	OnDiscardChargesChanged.Broadcast();
	OnHandChanged.Broadcast();
	RefreshHighlights();
}

void UHexCardSubsystem::InitializeDesigned(const FHexLevelDefinition& Definition)
{
	bDesigned = true;
	Labels = UHexLevelTools::MakeLabelMap(Definition);
	State = FHexCardState();
	State.Hand = Definition.Deck;
	for (FHexCard& Card : State.Hand)
	{
		Card.bLucky = false;
	}
	ApplyLabelsToBoard();

	// The card-aware optimum is the star baseline (falls back to the geometric one if it can't be found).
	const UHexBoardSubsystem* Board = GetBoard();
	const FHexStateSpace* Space = Board ? Board->GetStateSpace() : nullptr;
	if (Space)
	{
		const double Start = FPlatformTime::Seconds();
		const FHexDeckSolveResult Solve = FHexDeckSolver::Solve(*Space, Space->GetStartState(), Labels, State.Hand, UHexSokobanSettings::Get().DeckSolverMaxNodes);
		DesignedOptimalTurns = Solve.bSolved ? Solve.Turns : -1;
		if (Solve.bBudgetHit) Board->EnablePerformanceMode();
		UE_LOG(LogHexSokoban, Log, TEXT("Designed level '%s': %d cards in hand, %s (%d nodes, %.1f ms)."), *Definition.LevelId, State.Hand.Num(),
			Solve.bSolved ? *FString::Printf(TEXT("fewest %d turns with this hand"), Solve.Turns)
				: (Solve.bBudgetHit ? TEXT("search budget hit") : TEXT("NOT winnable with this hand")),
			Solve.Expanded, (FPlatformTime::Seconds() - Start) * 1000.0);
	}
	InitializeSkillDeck();
	InitialState = State;
	UHexAudioSubsystem::Deal(this, State.Hand.Num());
	OnHandDealt.Broadcast();
	OnDiscardChargesChanged.Broadcast();
	OnHandChanged.Broadcast();
	RefreshHighlights();
}

bool UHexCardSubsystem::IsOutOfMoves(FText& OutReason) const
{
	if (GetBoard() && GetBoard()->IsPerformanceMode()) return false;
	if (!IsActive())
	{
		return false;
	}
	if (State.Hand.Num() == 0)
	{
		OutReason = FText::FromString(HEXT("牌用完了"));							// out of cards
		return true;
	}
	if (!HasAnyMove())
	{
		OutReason = FText::FromString(HEXT("手里没有能打出的牌了"));			// no card fits
		return true;
	}
	return false;
}

void UHexCardSubsystem::ApplyLabelsToBoard()
{
	UHexBoardSubsystem* Board = GetBoard();
	if (!Board)
	{
		return;
	}
	for (AHexTile* Tile : Board->GetAllTiles())
	{
		if (const FHexCard* Label = bActive ? FindLabel(Tile->GetCoord()) : nullptr)
		{
			Tile->SetCardLabel(*Label);
		}
		else
		{
			Tile->ClearCardLabel();
		}
	}
	for (AHexBox* Box : Board->GetAllBoxes())
	{
		Box->RefreshCardLabel(false);
	}
}

// ---------------------------------------------------------------- selection & queries

void UHexCardSubsystem::SelectCard(int32 HandIndex)
{
	const int32 Current = GetSelectedCardIndex();
	if (!State.Hand.IsValidIndex(HandIndex) || HandIndex == Current)
	{
		bHasSelection = false;
	}
	else
	{
		bHasSelection = true;
		SelectedCard = State.Hand[HandIndex];
        UHexAudioSubsystem::Play(this, TEXT("solid_card"), 0.4f);
	}
	OnSelectionChanged.Broadcast();
	RefreshHighlights();
}

int32 UHexCardSubsystem::GetSelectedCardIndex() const
{
	if (!bHasSelection)
	{
		return INDEX_NONE;
	}
	return State.Hand.IndexOfByPredicate([this](const FHexCard& Card) { return Card.SameFace(SelectedCard); });
}

bool UHexCardSubsystem::CanCardEnterTile(int32 HandIndex, const AHexTile* Tile) const
{
	if (Tile && State.Hand.IsValidIndex(HandIndex) && State.Hand[HandIndex].Kind == EHexCardKind::Skill)
	{
		FHexMoveRecord Record;
		return GetBoard() && GetMover() && GetBoard()->EvaluateSkill(GetMover()->GetCoord(),Tile->GetCoord(),State.Hand[HandIndex],Record);
	}
	if (!Tile)
	{
		return false;
	}
	const FHexCard* Label = FindLabel(Tile->GetCoord());
	if (!Label)
	{
		return true;
	}
	return State.Hand.IsValidIndex(HandIndex) && State.Hand[HandIndex].Matches(*Label);
}

bool UHexCardSubsystem::CanEnterTile(const AHexTile* Tile) const
{
	if (!Tile)
	{
		return false;
	}
	const FHexCard* Label = FindLabel(Tile->GetCoord());
	if (!Label)
	{
		return true;
	}
	return State.Hand.ContainsByPredicate([Label](const FHexCard& Card) { return Card.Matches(*Label); });
}

TArray<AHexTile*> UHexCardSubsystem::GetEnterableNeighbors(int32 HandIndex) const
{
	TArray<AHexTile*> Result;
	if (HandIndex == INDEX_NONE)
	{
		TArray<FHexCard> SeenFaces;
		TSet<AHexTile*> UniqueTargets;
		for (int Index=0;Index<State.Hand.Num();++Index)
		{
			const FHexCard& Card=State.Hand[Index];
			if (SeenFaces.ContainsByPredicate([&Card](const FHexCard& Seen){ return Seen.SameFace(Card); })) continue;
			SeenFaces.Add(Card);
			for (AHexTile* Tile : GetEnterableNeighbors(Index)) if (!UniqueTargets.Contains(Tile)) { UniqueTargets.Add(Tile); Result.Add(Tile); }
		}
		return Result;
	}
	const UHexBoardSubsystem* Board = GetBoard();
	const UHexGridMovementComponent* Mover = GetMover();
	if (!bActive || !Board || !Mover || !Mover->IsOnBoard())
	{
		return Result;
	}
	if (State.Hand.IsValidIndex(HandIndex) && State.Hand[HandIndex].Kind == EHexCardKind::Skill)
	{
		if (State.Hand[HandIndex].Rank==12)
		{
			for (AHexTile* Tile : Board->GetAllTiles()) if (CanCardEnterTile(HandIndex,Tile)) Result.Add(Tile);
		}
		else
		{
			for (uint8 Dir=0; Dir<6; ++Dir)
			{
				FIntPoint Cell=UHexGridLibrary::GetNeighbor(Mover->GetCoord(),static_cast<EHexDirection>(Dir));
				while (AHexTile* Tile=Board->GetTileAt(Cell))
				{
					if (CanCardEnterTile(HandIndex,Tile)) Result.Add(Tile);
					if (State.Hand[HandIndex].Rank==11 || Board->GetBoxAt(Cell)) break;
					Cell=UHexGridLibrary::GetNeighbor(Cell,static_cast<EHexDirection>(Dir));
				}
			}
		}
		return Result;
	}
	for (int32 Dir = 0; Dir < 6; ++Dir)
	{
		FHexMoveRecord Record;
		const EHexMoveResult Move = Board->EvaluateMove(Mover->GetCoord(), static_cast<EHexDirection>(Dir), Record);
		if (Move != EHexMoveResult::Moved && Move != EHexMoveResult::Pushed)
		{
			continue;
		}
		AHexTile* Tile = Board->GetTileAt(Record.PlayerTo);
		const bool bFits = HandIndex >= 0 ? CanCardEnterTile(HandIndex, Tile) : CanEnterTile(Tile);
		if (Tile && bFits)
		{
			Result.Add(Tile);
		}
	}
	return Result;
}

bool UHexCardSubsystem::IsCardPlayableNow(int32 HandIndex) const
{
	return State.Hand.IsValidIndex(HandIndex) && GetEnterableNeighbors(HandIndex).Num() > 0;
}

bool UHexCardSubsystem::HasAnyMove() const
{
	return GetEnterableNeighbors(INDEX_NONE).Num() > 0;
}

bool UHexCardSubsystem::HasAnyGeometricMove() const
{
	const UHexBoardSubsystem* Board = GetBoard();
	const UHexGridMovementComponent* Mover = GetMover();
	if (!Board || !Mover)
	{
		return false;
	}
	for (int32 Dir = 0; Dir < 6; ++Dir)
	{
		FHexMoveRecord Record;
		const EHexMoveResult Move = Board->EvaluateMove(Mover->GetCoord(), static_cast<EHexDirection>(Dir), Record);
		if (Move == EHexMoveResult::Moved || Move == EHexMoveResult::Pushed)
		{
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------- playing

int32 UHexCardSubsystem::ChooseCardFor(const FHexCard& Label) const
{
	// Exact face > same rank > same suit; ties go to the card held longest (lowest index).
	int32 Best = INDEX_NONE;
	int32 BestRank = 0;
	for (int32 Index = 0; Index < State.Hand.Num(); ++Index)
	{
		const FHexCard& Card = State.Hand[Index];
		if (Card.Kind != EHexCardKind::Number) continue;
		const int32 Quality = Card.SameFace(Label) ? 3 : (Card.Rank == Label.Rank ? 2 : (Card.Suit == Label.Suit ? 1 : 0));
		if (Quality > BestRank)
		{
			BestRank = Quality;
			Best = Index;
		}
	}
	return Best;
}

void UHexCardSubsystem::ReturnToPile(FHexCard Card)
{
	if (Card.Kind == EHexCardKind::Skill) { ReturnSkill(Card); return; }
	Card.bLucky = false;
	if (UHexSokobanSettings::Get().ReturnPolicy == EHexCardReturnPolicy::ShuffleIntoPile && State.DrawPile.Num() > 0)
	{
		State.DrawPile.Insert(Card, ShuffleStream.RandRange(1, State.DrawPile.Num()));	// never over the shown top
	}
	else
	{
		State.DrawPile.Add(Card);
	}
}

EHexCardPayment UHexCardSubsystem::PayForStep(AHexTile* TargetTile)
{
	if (!bActive || !TargetTile)
	{
		return EHexCardPayment::Free;
	}
	const FHexCard* Label = FindLabel(TargetTile->GetCoord());
	if (!Label)
	{
		return EHexCardPayment::Free;
	}

	// A selected card must fit; otherwise pick automatically.
	const int32 Selected = GetSelectedCardIndex();
	int32 Pick = INDEX_NONE;
	if (Selected != INDEX_NONE)
	{
		Pick = CanCardEnterTile(Selected, TargetTile) ? Selected : INDEX_NONE;
	}
	else
	{
		bHasSelection = false;
		Pick = ChooseCardFor(*Label);
	}
	if (Pick == INDEX_NONE)
	{
		return EHexCardPayment::Refused;
	}

	History.Add(State);
	State.DiscardCharges = FMath::Min(State.DiscardCharges + 1, GetMaxDiscardCharges());	// playing a card refunds a discard
	const FHexCard Played = State.Hand[Pick];
	State.bRewardSkill = Played.Kind == EHexCardKind::Number && Played.SameFace(*Label);
	State.Hand.RemoveAt(Pick);
	if (!bDesigned || Played.Kind == EHexCardKind::Skill)
	{
		ReturnToPile(Played);	// designed hands: a played card is gone
	}
	bHasSelection = false;
	ClearGuide(true);
	ClearHighlights();

	UE_LOG(LogHexSokoban, Log, TEXT("Played %s -> tile (%d,%d) [%s]"), *Played.ToString(), TargetTile->GetCoord().X, TargetTile->GetCoord().Y, *Label->ToString());
	UHexAudioSubsystem::Play(this, TEXT("play_a_card"));
	OnCardPlayed.Broadcast(Played, TargetTile);
	OnDiscardChargesChanged.Broadcast();
	OnSelectionChanged.Broadcast();
	OnHandChanged.Broadcast();
	return EHexCardPayment::Paid;
}

void UHexCardSubsystem::NotifyPaymentRefused(AHexTile* TargetTile)
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	State.PityMeter = FMath::Min(State.PityMeter + 1, FMath::Max(1, Settings.LuckyDrawThreshold));
	if (TargetTile)
	{
		TargetTile->PlayInvalidFeedback();
	}
	UHexAudioSubsystem::PlayExact(this, TEXT("Error_Buzz_01"));
	OnPlayRejected.Broadcast();
	OnGuideMessage.Broadcast(DescribeCardRefusal(GetSelectedCardIndex(), TargetTile), TargetTile);
}

void UHexCardSubsystem::ReportBlockedMove(AHexTile* TargetTile, EHexMoveResult Result)
{
	if (!bActive)
	{
		return;
	}
	if (TargetTile)
	{
		TargetTile->PlayInvalidFeedback();
	}
	FText Message;
	if (Result == EHexMoveResult::BlockedByBox)
	{
		Message = FText::FromString(HEXT("后面还有箱子，这次推不动。"));
	}
	else if (Result == EHexMoveResult::BlockedNoTile)
	{
		Message = FText::FromString(HEXT("箱子后面没有地台，不能往这边推。"));
	}
	else
	{
		return;
	}
	UHexAudioSubsystem::PlayExact(this, TEXT("Error_Buzz_01"));
	OnPlayRejected.Broadcast();
	OnGuideMessage.Broadcast(Message, TargetTile);
}

void UHexCardSubsystem::ReportNotNeighbor(AHexTile* Tile)
{
	if (Tile)
	{
		Tile->PlayInvalidFeedback();
	}
	UHexAudioSubsystem::PlayExact(this, TEXT("Error_Buzz_01"));
	OnPlayRejected.Broadcast();
	OnGuideMessage.Broadcast(FText::FromString(HEXT("普通牌只能走一格，请选择相邻地台。")), Tile);
}

FText UHexCardSubsystem::DescribeCardRefusal(int32 HandIndex, const AHexTile* Tile) const
{
	if (State.Hand.IsValidIndex(HandIndex) && State.Hand[HandIndex].Kind==EHexCardKind::Skill)
	{
		const FHexCard& Card=State.Hand[HandIndex];
		return FText::FromString(Card.Rank==12 ? HEXT("请选择同花色的空地台，脚下和有箱子的位置不能传送。")
			: Card.Rank==11 ? HEXT("请对相邻同花色地台上的箱子使用，箱子前面要留出空位。")
			: HEXT("请选择直线上同花色的空地台，沿途不能有箱子或缺口。"));
	}
	const FHexCard* Label = Tile ? FindLabel(Tile->GetCoord()) : nullptr;
	if (!Label)
	{
		return FText::FromString(HEXT("这里不能落脚，请换一个地台。"));
	}
	const FString Need = FHexLocalization::Printf(HEXT("请用 %s 花色或点数为 %s 的牌。"), *Label->GetSuitSymbol(), *Label->GetRankText());
	if (State.Hand.IsValidIndex(HandIndex))
	{
		return FText::FromString(FHexLocalization::Printf(HEXT("%s 不匹配。%s"), *State.Hand[HandIndex].ToString(), *Need));
	}
	return FText::FromString(FHexLocalization::Printf(HEXT("没有匹配的手牌。%s"), *Need));
}

// ---------------------------------------------------------------- guidance

void UHexCardSubsystem::UpdateGuide(int32 HandIndex, bool bFocus, AHexTile* HoverTile)
{
	UHexBoardSubsystem* Board = GetBoard();
	const UHexGridMovementComponent* Mover = GetMover();
	if (!bActive || !Board || !Mover || !Mover->IsOnBoard() || Mover->IsMoving() || Mover->IsMovementLocked()
		|| !State.Hand.IsValidIndex(HandIndex))
	{
		ClearGuide();
		return;
	}
	if (!bGuideActive)
	{
		ClearHighlights();
		bGuideActive = true;
	}

	GuidePreviewIndex = HandIndex;
	bGuideFocus = bFocus;
	GuideHoverTile = HoverTile;
	const TArray<AHexTile*> Targets = GetEnterableNeighbors(HandIndex);
	GuideTargets.Reset();
	for (AHexTile* Target : Targets)
	{
		GuideTargets.Add(Target);
	}
	const FIntPoint PlayerCoord = Mover->GetCoord();
	for (AHexTile* Tile : Board->GetAllTiles())
	{
		EHexTileGuide Guide = EHexTileGuide::None;
		if (Targets.Contains(Tile))
		{
			Guide = (Tile == HoverTile) ? EHexTileGuide::ValidHovered : EHexTileGuide::Valid;
		}
		else if (bFocus)
		{
			// Everything but the reachable tiles darkens (the player's own tile too, so only targets stand out).
			Guide = (Tile == HoverTile && Tile->GetCoord() != PlayerCoord) ? EHexTileGuide::InvalidHovered : EHexTileGuide::Dimmed;
		}
		Tile->SetGuideState(Guide);
	}
}

void UHexCardSubsystem::ClearGuide(bool bSnap)
{
	if (!bGuideActive)
	{
		return;
	}
	bGuideActive = false;
	GuidePreviewIndex = INDEX_NONE;
	bGuideFocus = false;
	GuideHoverTile.Reset();
	GuideTargets.Reset();
	if (UHexBoardSubsystem* Board = GetBoard())
	{
		for (AHexTile* Tile : Board->GetAllTiles())
		{
			Tile->SetGuideState(EHexTileGuide::None, bSnap);
		}
	}
	RefreshHighlights();
}

bool UHexCardSubsystem::IsGuideTarget(const AHexTile* Tile) const
{
	return Tile && GuideTargets.ContainsByPredicate([Tile](const TWeakObjectPtr<AHexTile>& Target) { return Target.Get() == Tile; });
}

bool UHexCardSubsystem::GetNextLineLabel(FHexCard& OutLabel) const
{
	const UHexBoardSubsystem* Board = GetBoard();
	if (!Board)
	{
		return false;
	}
	TArray<FIntPoint> Line;
	if (!Board->GetSolutionPlayerSteps(Line, UHexSokobanSettings::Get().SolverMaxStates))
	{
		return false;
	}
	for (const FIntPoint& Step : Line)
	{
		if (const FHexCard* Label = FindLabel(Step))
		{
			OutLabel = *Label;
			return true;
		}
	}
	return false;
}

bool UHexCardSubsystem::PullMatchingToTop(const FHexCard& Label, int32 Depth)
{
	// Already within the top Depth cards? Nothing to do.
	for (int32 Index = 0; Index < FMath::Min(Depth, State.DrawPile.Num()); ++Index)
	{
		if (State.DrawPile[Index].Matches(Label))
		{
			return true;
		}
	}
	const int32 Found = State.DrawPile.IndexOfByPredicate([&Label](const FHexCard& Card) { return Card.Matches(Label); });
	if (Found == INDEX_NONE)
	{
		return false;
	}
	FHexCard Lucky = State.DrawPile[Found];
	Lucky.bLucky = true;
	State.DrawPile.RemoveAt(Found);
	State.DrawPile.Insert(Lucky, 0);
	return true;
}

float UHexCardSubsystem::GetDifficultyProgress() const
{
	const int32 RampSteps = UHexSokobanSettings::Get().DifficultyRampSteps;
	if (RampSteps <= 0)
	{
		return 1.f;
	}
	// Cards played in this level (undo pops History, restart clears it).
	return FMath::Clamp(static_cast<float>(History.Num()) / static_cast<float>(RampSteps), 0.f, 1.f);
}

void UHexCardSubsystem::DealHand(int32 HandSize, bool bMarkLucky)
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	FHexCard NextLabel;
	const bool bHasLine = GetNextLineLabel(NextLabel);
	const float Ramp = GetDifficultyProgress();
	const bool bShape = Settings.bDrawDirector && Ramp > 0.f;
	for (int32 Index = 0; Index < HandSize && State.DrawPile.Num() > 0; ++Index)
	{
		if (Index == 0 && bHasLine && (bShape || bMarkLucky))
		{
			// The first card always opens the next step of a winning line (the opening deal is already fair).
			PullMatchingToTop(NextLabel, bShape ? 1 : HandSize);
			if (!bMarkLucky && State.DrawPile.Num() > 0)
			{
				State.DrawPile[0].bLucky = false;
			}
		}
		else if (bShape && ShuffleStream.FRand() < Ramp)
		{
			DirectNextDraw();
		}
		State.Hand.Add(State.DrawPile[0]);
		State.DrawPile.RemoveAt(0);
	}
}

void UHexCardSubsystem::GetNeighborLabels(TArray<FHexCard>& OutLabels) const
{
	OutLabels.Reset();
	const UHexBoardSubsystem* Board = GetBoard();
	const UHexGridMovementComponent* Mover = GetMover();
	if (!Board || !Mover || !Mover->IsOnBoard())
	{
		return;
	}
	for (int32 Dir = 0; Dir < 6; ++Dir)
	{
		FHexMoveRecord Record;
		const EHexMoveResult Move = Board->EvaluateMove(Mover->GetCoord(), static_cast<EHexDirection>(Dir), Record);
		if (Move != EHexMoveResult::Moved && Move != EHexMoveResult::Pushed)
		{
			continue;
		}
		if (const FHexCard* Label = FindLabel(Record.PlayerTo))
		{
			OutLabels.Add(*Label);
		}
	}
}

bool UHexCardSubsystem::PullDudToTop()
{
	TArray<FHexCard> Around;
	GetNeighborLabels(Around);
	if (Around.Num() == 0)
	{
		return false;
	}
	auto IsDud = [&Around](const FHexCard& Card)
	{
		return !Around.ContainsByPredicate([&Card](const FHexCard& Label) { return Card.Matches(Label); });
	};

	// With the director on, pick the dud that also keeps the hand's overall reach closest to the target.
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	const int32 MaxCandidates = Settings.bDrawDirector ? FMath::Max(1, Settings.DirectorLookahead) : 1;
	const float Target = FMath::Clamp(Settings.DirectorTargetCoverage, 0.f, 1.f);
	int32 Best = INDEX_NONE;
	float BestError = TNumericLimits<float>::Max();
	int32 Candidates = 0;
	TArray<FHexCard> Trial = State.Hand;
	Trial.AddDefaulted();
	for (int32 Index = 0; Index < State.DrawPile.Num() && Candidates < MaxCandidates; ++Index)
	{
		if (!IsDud(State.DrawPile[Index]))
		{
			continue;
		}
		++Candidates;
		Trial.Last() = State.DrawPile[Index];
		const float Error = FMath::Abs(GetCoverage(Trial) - Target);
		if (Error < BestError - KINDA_SMALL_NUMBER)
		{
			BestError = Error;
			Best = Index;
		}
	}
	if (Best == INDEX_NONE)
	{
		return false;
	}
	const FHexCard Dud = State.DrawPile[Best];
	State.DrawPile.RemoveAt(Best);
	State.DrawPile.Insert(Dud, 0);
	UE_LOG(LogHexSokoban, Log, TEXT("Dud draw: %s fits none of the %d tiles around the player."), *Dud.ToString(), Around.Num());
	return true;
}

float UHexCardSubsystem::GetCoverage(const TArray<FHexCard>& Cards) const
{
	if (Labels.Num() == 0)
	{
		return 1.f;
	}
	int32 Reachable = 0;
	for (const TPair<FIntPoint, FHexCard>& Pair : Labels)
	{
		const FHexCard& Label = Pair.Value;
		if (Cards.ContainsByPredicate([&Label](const FHexCard& Card) { return Card.Matches(Label); }))
		{
			++Reachable;
		}
	}
	return static_cast<float>(Reachable) / static_cast<float>(Labels.Num());
}

void UHexCardSubsystem::DirectNextDraw()
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	const int32 Lookahead = FMath::Min(FMath::Max(1, Settings.DirectorLookahead), State.DrawPile.Num());
	if (Lookahead <= 1)
	{
		return;
	}
	const float Target = FMath::Clamp(Settings.DirectorTargetCoverage, 0.f, 1.f);
	int32 Best = 0;
	float BestError = TNumericLimits<float>::Max();
	float BestCoverage = 0.f;
	TArray<FHexCard> Trial = State.Hand;
	Trial.AddDefaulted();
	for (int32 Index = 0; Index < Lookahead; ++Index)
	{
		Trial.Last() = State.DrawPile[Index];
		const float Coverage = GetCoverage(Trial);
		const float Error = FMath::Abs(Coverage - Target);
		if (Error < BestError - KINDA_SMALL_NUMBER)	// ties keep the shuffled order
		{
			BestError = Error;
			BestCoverage = Coverage;
			Best = Index;
		}
	}
	if (Best > 0)
	{
		const FHexCard Chosen = State.DrawPile[Best];
		State.DrawPile.RemoveAt(Best);
		State.DrawPile.Insert(Chosen, 0);
	}
	UE_LOG(LogHexSokoban, Verbose, TEXT("Draw director: %s (hand reach %.0f%%, target %.0f%%)"),
		*State.DrawPile[0].ToString(), BestCoverage * 100.f, Target * 100.f);
}

void UHexCardSubsystem::DrawOne()
{
	if (State.DrawPile.Num() == 0)
	{
		return;
	}
	const FHexCard Drawn = State.DrawPile[0];
	State.DrawPile.RemoveAt(0);
	State.ProtectedDraws = FMath::Max(0, State.ProtectedDraws - 1);
	State.Hand.Add(Drawn);
	UHexAudioSubsystem::Play(this, TEXT("draw_card"));
	OnCardDrawn.Broadcast(Drawn);
}

bool UHexCardSubsystem::PeekNextDraw(FHexCard& OutCard) const
{
	if (!bActive || State.DrawPile.Num() == 0)
	{
		return false;
	}
	OutCard = State.DrawPile[0];
	return true;
}

void UHexCardSubsystem::GetLineLabels(int32 Count, TArray<FHexCard>& OutLabels) const
{
	OutLabels.Reset();
	const UHexBoardSubsystem* Board = GetBoard();
	TArray<FIntPoint> Line;
	if (!Board || !Board->GetSolutionPlayerSteps(Line, UHexSokobanSettings::Get().SolverMaxStates))
	{
		return;
	}
	for (const FIntPoint& Step : Line)
	{
		if (OutLabels.Num() >= Count)
		{
			break;
		}
		if (const FHexCard* Label = FindLabel(Step))
		{
			OutLabels.Add(*Label);
		}
	}
}

void UHexCardSubsystem::PrepareNextDraw(bool bAllowDud)
{
	if (!bActive || State.DrawPile.Num() == 0)
	{
		return;
	}
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();

	// 1. Anti-loop feed (its cards stay on top until drawn).
	if (Settings.bAntiLoopFeed && TryAntiLoopFeed())
	{
		return;
	}
	if (State.ProtectedDraws > 0)
	{
		return;
	}

	// 2. Lucky draw. This card arrives after the next step, so it should pay the step after that:
	//    the hand must cover the next two labelled tiles of the optimal line with different cards.
	if (Settings.bLuckyDraw)
	{
		TArray<FHexCard> Upcoming;
		GetLineLabels(2, Upcoming);
		if (Upcoming.Num() > 0)
		{
			bool bCovered = false;
			for (int32 First = 0; First < State.Hand.Num() && !bCovered; ++First)
			{
				if (!State.Hand[First].Matches(Upcoming[0]))
				{
					continue;
				}
				if (Upcoming.Num() == 1)
				{
					bCovered = true;
					break;
				}
				for (int32 Second = 0; Second < State.Hand.Num(); ++Second)
				{
					if (Second != First && State.Hand[Second].Matches(Upcoming[1]))
					{
						bCovered = true;
						break;
					}
				}
			}
			if (bCovered)
			{
				State.PityMeter = 0;
			}
			else if (++State.PityMeter >= FMath::Max(1, Settings.LuckyDrawThreshold))
			{
				const FHexCard& Wanted = Upcoming.Last();
				if (PullMatchingToTop(Wanted, 1))
				{
					State.PityMeter = 0;
					UE_LOG(LogHexSokoban, Log, TEXT("Lucky draw: %s fits an upcoming tile of the optimal line."), *State.DrawPile[0].ToString());
					return;
				}
			}
		}
	}

	// 3. Dud: only while the player still has a move with the cards already in hand.
	const float Ramp = GetDifficultyProgress();
	const float DudChance = Settings.DudDrawChance * Ramp;
	if (bAllowDud && DudChance > 0.f && ShuffleStream.FRand() < DudChance && HasAnyMove() && PullDudToTop())
	{
		return;
	}

	// 4. Director.
	if (Settings.bDrawDirector && Ramp > 0.f && ShuffleStream.FRand() < Ramp)
	{
		DirectNextDraw();
	}
}

bool UHexCardSubsystem::TryAntiLoopFeed()
{
	const UHexBoardSubsystem* Board = GetBoard();
	if (!Board)
	{
		return false;
	}
	TArray<FIntPoint> Recent;
	Board->GetRecentPlayerCells(4, Recent);	// newest first: A, B, A, B = three steps back and forth
	if (Recent.Num() < 4 || Recent[0] == Recent[1] || Recent[2] != Recent[0] || Recent[3] != Recent[1])
	{
		return false;
	}
	const int32 Moves = Board->GetActionCount();
	if (State.AntiLoopMoveStamp >= 0 && Moves - State.AntiLoopMoveStamp < 3)
	{
		return false;	// already fed for this streak
	}
	const FIntPoint A = Recent[0];
	const FIntPoint B = Recent[1];

	// Ring of tiles around both, the optimal line's tiles first (in line order).
	TArray<FIntPoint> Ring;
	for (const FIntPoint& Center : { A, B })
	{
		for (int32 Dir = 0; Dir < 6; ++Dir)
		{
			const FIntPoint Around = UHexGridLibrary::GetNeighbor(Center, static_cast<EHexDirection>(Dir));
			if (Around != A && Around != B && Board->GetTileAt(Around) && FindLabel(Around))
			{
				Ring.AddUnique(Around);
			}
		}
	}
	TArray<FIntPoint> Line;
	Board->GetSolutionPlayerSteps(Line, UHexSokobanSettings::Get().SolverMaxStates);
	Ring.StableSort([&Line](const FIntPoint& X, const FIntPoint& Y)
	{
		const int32 IX = Line.Contains(X) ? Line.IndexOfByKey(X) : MAX_int32;
		const int32 IY = Line.Contains(Y) ? Line.IndexOfByKey(Y) : MAX_int32;
		return IX < IY;
	});

	// One pile card per ring tile the hand can't already pay, up to the feed size.
	const int32 MaxFeed = FMath::Max(1, UHexSokobanSettings::Get().AntiLoopFeedCards);
	TArray<int32> Picked;
	for (const FIntPoint& Cell : Ring)
	{
		if (Picked.Num() >= MaxFeed)
		{
			break;
		}
		const FHexCard& Label = *FindLabel(Cell);
		if (State.Hand.ContainsByPredicate([&Label](const FHexCard& Card) { return Card.Matches(Label); }))
		{
			continue;
		}
		for (int32 Index = 0; Index < State.DrawPile.Num(); ++Index)
		{
			if (!Picked.Contains(Index) && State.DrawPile[Index].Matches(Label))
			{
				Picked.Add(Index);
				break;
			}
		}
	}
	if (Picked.Num() == 0)
	{
		return false;
	}
	TArray<FHexCard> Feed;
	for (const int32 Index : Picked)
	{
		FHexCard Card = State.DrawPile[Index];
		Card.bLucky = true;
		Feed.Add(Card);
	}
	Picked.Sort([](int32 X, int32 Y) { return X > Y; });
	for (const int32 Index : Picked)
	{
		State.DrawPile.RemoveAt(Index);
	}
	State.DrawPile.Insert(Feed, 0);
	State.ProtectedDraws = Feed.Num();
	State.AntiLoopMoveStamp = Moves;
	UE_LOG(LogHexSokoban, Log, TEXT("Anti-loop feed: bouncing between (%d,%d) and (%d,%d) - %d card(s) for the tiles around them on top (next: %s)."),
		A.X, A.Y, B.X, B.Y, Feed.Num(), *State.DrawPile[0].ToString());
	return true;
}

void UHexCardSubsystem::NotifyStepCompleted()
{
	if (!bActive)
	{
		return;
	}
	if (State.bRewardSkill) { State.bRewardSkill=false; AwardSkill(); }
	else if (State.SkillPile.IsEmpty()) PrepareNextSkill();
	if (bDesigned)
	{
		OnHandChanged.Broadcast();	// no draws: the hand only shrinks
		RefreshHighlights();
		return;
	}
	DrawOne();

	if (UHexSokobanSettings::Get().bAutoMulligan && !HasAnyMove() && HasAnyGeometricMove())
	{
		Mulligan();	// decides the next draw too
	}
	else
	{
		PrepareNextDraw(true);
	}
	OnHandChanged.Broadcast();
	RefreshHighlights();
}

// ---------------------------------------------------------------- hand-aware hint (A*)

EHexHintStatus UHexCardSubsystem::PlanHandHint(TArray<FIntPoint>& OutSteps) const
{
	if (GetBoard() && GetBoard()->IsPerformanceMode()) { OutSteps.Reset(); return EHexHintStatus::None; }
	OutSteps.Reset();
	TArray<FHexCard> NormalHand;
	for (const FHexCard& Card : State.Hand) if (Card.Kind == EHexCardKind::Number) NormalHand.Add(Card);
	const UHexBoardSubsystem* Board = GetBoard();
	const FHexStateSpace* Space = Board ? Board->GetStateSpace() : nullptr;
	if (Board && Board->IsPerformanceMode()) return EHexHintStatus::None;
	int32 Root = Board ? Board->GetCurrentStateIndex() : INDEX_NONE;
	// Skills can reach positions outside the original ordinary-move graph. Build the SAME
	// ordinary rules from the current position for this hint only; never change the star baseline.
	FHexStateSpace CurrentSpace;
	if (Board && GetMover() && Root==INDEX_NONE)
	{
		FHexSolverInput Input; Input.Player=GetMover()->GetCoord();
		for(AHexTile* T:Board->GetAllTiles()) { Input.Tiles.Add(T->GetCoord()); if(T->IsGoal()) Input.Goals.Add(T->GetCoord()); }
		for(AHexBox* B:Board->GetAllBoxes()) Input.Boxes.Add(B->GetCoord());
		if(CurrentSpace.Build(Input,UHexSokobanSettings::Get().SolverMaxStates)) { Space=&CurrentSpace; Root=Space->GetStartState(); }
		else if (CurrentSpace.WasBudgetExceeded()) { Board->EnablePerformanceMode(); return EHexHintStatus::None; }
	}
	if (!bActive || !Space || Root == INDEX_NONE || Space->GetCost(Root) <= 0)
	{
		return EHexHintStatus::None;
	}
	if (bDesigned)
	{
		// The whole hand is known and nothing is drawn: plan all the way to the win.
		const FHexDeckSolveResult Solve = FHexDeckSolver::Solve(*Space, Root, Labels, NormalHand, UHexSokobanSettings::Get().DeckSolverMaxNodes);
		if (Solve.bBudgetHit) { Board->EnablePerformanceMode(); return EHexHintStatus::None; }
		OutSteps = Solve.Steps;
		if (Solve.bSolved)
		{
			return EHexHintStatus::OnOptimal;
		}
		return OutSteps.Num() > 0 ? EHexHintStatus::Lost : EHexHintStatus::NoMove;
	}
	const int32 HandCount = FMath::Min(NormalHand.Num(), 16);

	// Node = board position + which hand cards are spent. g = steps taken, h = exact steps-to-win of the
	// position (consistent, so f never drops along a path). Draws are unknown, so the plan ends where the
	// hand runs out; free (unlabelled) tiles cost no card.
	struct FNode
	{
		int32 Position = INDEX_NONE;
		uint32 Used = 0;
		int32 G = 0;
		int32 F = 0;
		int32 Parent = INDEX_NONE;
		FIntPoint Entered = FIntPoint::ZeroValue;
	};
	TArray<FNode> Nodes;
	TArray<int32> Open;
	TSet<uint64> Closed;
	const auto Better = [&Nodes](int32 X, int32 Y)	// heap order: lowest f, then deepest
	{
		return Nodes[X].F != Nodes[Y].F ? Nodes[X].F < Nodes[Y].F : Nodes[X].G > Nodes[Y].G;
	};
	const int32 MaxDepth = HandCount + 6;
	constexpr int32 MaxNodes = 20000;

	FNode Start;
	Start.Position = Root;
	Start.F = Space->GetCost(Root);
	Nodes.Add(Start);
	Open.HeapPush(0, Better);

	int32 Best = INDEX_NONE;
	while (Open.Num() > 0 && Nodes.Num() < MaxNodes)
	{
		int32 Current = INDEX_NONE;
		Open.HeapPop(Current, Better, EAllowShrinking::No);
		const FNode Node = Nodes[Current];	// copy: Nodes grows below
		const uint64 Key = (static_cast<uint64>(Node.Position) << 32) | Node.Used;
		if (Closed.Contains(Key))
		{
			continue;
		}
		Closed.Add(Key);

		if (Node.G > 0 && (Best == INDEX_NONE || Nodes[Best].F > Node.F || (Nodes[Best].F == Node.F && Nodes[Best].G < Node.G)))
		{
			Best = Current;
		}
		if (Space->GetCost(Node.Position) == 0 || Node.G >= MaxDepth)
		{
			continue;	// won, or far enough
		}
		for (int32 Dir = 0; Dir < FHexStateSpace::NumDirs; ++Dir)
		{
			FIntPoint Entered;
			const int32 Child = Space->Step(Node.Position, Dir, Entered);
			const int32 ChildCost = Space->GetCost(Child);
			if (Child == INDEX_NONE || ChildCost < 0)
			{
				continue;	// blocked, or the level could no longer be won
			}
			auto Push = [&](uint32 Used)
			{
				FNode Next;
				Next.Position = Child;
				Next.Used = Used;
				Next.G = Node.G + 1;
				Next.F = Next.G + ChildCost;
				Next.Parent = Current;
				Next.Entered = Entered;
				if (!Closed.Contains((static_cast<uint64>(Child) << 32) | Used))
				{
					Open.HeapPush(Nodes.Add(Next), Better);
				}
			};
			const FHexCard* Label = FindLabel(Entered);
			if (!Label)
			{
				Push(Node.Used);
				continue;
			}
			for (int32 Card = 0; Card < HandCount; ++Card)
			{
				if (!(Node.Used & (1u << Card)) && NormalHand[Card].Matches(*Label))
				{
					Push(Node.Used | (1u << Card));
				}
			}
		}
	}

	if (Best == INDEX_NONE)
	{
		return EHexHintStatus::NoMove;
	}
	for (int32 Index = Best; Index > 0; Index = Nodes[Index].Parent)
	{
		OutSteps.Insert(Nodes[Index].Entered, 0);
	}
	return Nodes[Best].F <= Space->GetCost(Root) ? EHexHintStatus::OnOptimal : EHexHintStatus::Detour;
}

// ---------------------------------------------------------------- discard zone

int32 UHexCardSubsystem::GetMaxDiscardCharges() const
{
	return bDesigned ? 0 : FMath::Max(0, UHexSokobanSettings::Get().MaxDiscards);	// the zone is off for designed hands (kept for random mode)
}

bool UHexCardSubsystem::CanDiscard(FText& OutReason) const
{
	OutReason = FText::GetEmpty();
	const UWorld* World = GetWorld();
	const AHexSokobanGameMode* GameMode = World ? World->GetAuthGameMode<AHexSokobanGameMode>() : nullptr;
	const UHexGridMovementComponent* Mover = GetMover();
	if (!bActive || GetMaxDiscardCharges() <= 0 || !Mover || !Mover->IsOnBoard()
		|| (GameMode && GameMode->GetLevelState() != EHexLevelState::Playing))
	{
		return false;
	}
	if (Mover->IsMoving() || Mover->IsMovementLocked())
	{
		return false;	// mid-step: silently ignore, the card just returns to the hand
	}
	if (State.DiscardCharges <= 0)
	{
		// Out of discards - moving (playing a card) refunds one.
		OutReason = FText::FromString(HEXT("暂时不能换牌，成功出牌后可再换一次。"));
		return false;
	}
	if (State.DrawPile.Num() == 0)
	{
		// Draw pile is empty.
		OutReason = FText::FromString(HEXT("牌堆已经空了，暂时不能换牌。"));
		return false;
	}
	return true;
}

bool UHexCardSubsystem::DiscardCard(int32 HandIndex)
{
	FText Reason;
	if (!State.Hand.IsValidIndex(HandIndex) || !CanDiscard(Reason))
	{
		if (!Reason.IsEmpty())
		{
	UHexAudioSubsystem::PlayExact(this, TEXT("Error_Buzz_01"));
			OnPlayRejected.Broadcast();
			OnGuideMessage.Broadcast(Reason, nullptr);
		}
		return false;
	}
	UHexBoardSubsystem* Board = GetBoard();
	if (!Board || !Board->CommitPass())
	{
		return false;
	}

	History.Add(State);
	const FHexCard Discarded = State.Hand[HandIndex];
	State.Hand.RemoveAt(HandIndex);
	--State.DiscardCharges;
	bHasSelection = false;
	ClearGuide(true);
	ClearHighlights();

	UE_LOG(LogHexSokoban, Log, TEXT("Discarded %s (turn skipped, %d discard(s) left)."), *Discarded.ToString(), State.DiscardCharges);
	UHexAudioSubsystem::Play(this, TEXT("solid_card"));
	OnCardDiscarded.Broadcast(Discarded);

	// The replacement is the card shown on top of the pile; the discarded card goes under it.
	DrawOne();
	ReturnToPile(Discarded);
	if (UHexSokobanSettings::Get().bAutoMulligan && !HasAnyMove() && HasAnyGeometricMove())
	{
		Mulligan();
	}
	else
	{
		PrepareNextDraw(false);
	}
	OnDiscardChargesChanged.Broadcast();
	OnSelectionChanged.Broadcast();
	OnHandChanged.Broadcast();
	RefreshHighlights();
	return true;
}

void UHexCardSubsystem::Mulligan()
{
	if (!bActive)
	{
		return;
	}
	const int32 HandSize = State.Hand.Num() > 0 ? State.Hand.Num() : FMath::Clamp(UHexSokobanSettings::Get().HandSize, 1, 10);
	for (const FHexCard& Card : State.Hand)
	{
		ReturnToPile(Card);
	}
	State.Hand.Reset();
	bHasSelection = false;

	DealHand(HandSize, true);
	State.PityMeter = 0;
	++State.MulliganCount;
	PrepareNextDraw(true);

	UE_LOG(LogHexSokoban, Log, TEXT("Mulligan #%d: no neighbouring tile could be entered."), State.MulliganCount);
	UHexAudioSubsystem::Play(this, TEXT("Flick_Switch"));
	OnMulligan.Broadcast();
	OnSelectionChanged.Broadcast();
	OnHandChanged.Broadcast();
	RefreshHighlights();
}

// ---------------------------------------------------------------- undo / restart

void UHexCardSubsystem::UndoLastPlay()
{
	if (!bActive || History.Num() == 0)
	{
		return;
	}
	State = History.Pop();
	bHasSelection = false;
	OnDiscardChargesChanged.Broadcast();
	OnSelectionChanged.Broadcast();
	OnHandChanged.Broadcast();
	RefreshHighlights();
}

void UHexCardSubsystem::RestoreInitialState()
{
	if (!bActive)
	{
		return;
	}
	State = InitialState;
	History.Reset();
	bHasSelection = false;
	ShuffleStream.Initialize(DealSeed ^ 0x5f3759df);
	ApplyLabelsToBoard();
	UHexAudioSubsystem::Deal(this, State.Hand.Num());
	OnHandDealt.Broadcast();
	OnDiscardChargesChanged.Broadcast();
	OnSelectionChanged.Broadcast();
	OnHandChanged.Broadcast();
	RefreshHighlights();
}

// ---------------------------------------------------------------- highlights

void UHexCardSubsystem::ClearHighlights()
{
	for (const TWeakObjectPtr<AHexTile>& Tile : HighlightedTiles)
	{
		if (Tile.IsValid())
		{
			Tile->SetCardHighlight(false);
		}
	}
	HighlightedTiles.Reset();
}

void UHexCardSubsystem::RefreshHighlights()
{
	ClearHighlights();
	if (bGuideActive)
	{
		return;	// the guide preview owns the tiles right now
	}
	const UHexGridMovementComponent* Mover = GetMover();
	if (!bActive || !Mover || Mover->IsMoving() || Mover->IsMovementLocked())
	{
		return;
	}
	for (AHexTile* Tile : GetEnterableNeighbors(GetSelectedCardIndex()))
	{
		Tile->SetCardHighlight(true);
		HighlightedTiles.Add(Tile);
	}
}
