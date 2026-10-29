#include "HexCardSubsystem.h"
#include "HexBoardSubsystem.h"
#include "HexGridMovementComponent.h"
#include "HexSokobanMath.h"
#include "HexTile.h"
#include "HexBox.h"
#include "HexAudioSubsystem.h"

void UHexCardSubsystem::InitializeSkillDeck()
{
	State.SkillPile.Reset(); State.SkillDiscard.Reset(); State.bRewardSkill=false;
	State.SkillRandom.Initialize(FMath::Rand());
	for (int32& Weight : SkillSuitWeights) Weight=0;
	for (const auto& Pair : Labels)
	{
		const int32 Suit=static_cast<int32>(Pair.Value.Suit);
		if (Suit>=0 && Suit<4) ++SkillSuitWeights[Suit];
	}
	for (int Rank=11; Rank<=13; ++Rank)
		for (int Suit=0; Suit<4; ++Suit)
		{
			bool bPresent=false;
			for (const auto& Pair : Labels) if (static_cast<int>(Pair.Value.Suit)==Suit) bPresent=true;
			if (bPresent)
			{
				FHexCard Card(Rank,static_cast<EHexSuit>(Suit)); Card.Kind=EHexCardKind::Skill;
				State.SkillPile.Add(Card);
			}
		}
	PrepareNextSkill();
}

bool UHexCardSubsystem::PeekNextSkill(FHexCard& OutCard) const
{
	if (State.SkillPile.IsEmpty()) return false;
	OutCard=State.SkillPile[0]; return true;
}

void UHexCardSubsystem::PrepareNextSkill()
{
	// A new round contains only cards already returned; cards in hand cannot be duplicated.
	if (State.SkillPile.IsEmpty()) { State.SkillPile=MoveTemp(State.SkillDiscard); State.SkillDiscard.Reset(); }
	if (State.SkillPile.IsEmpty()) return;
	TArray<int32> Types;
	for (int Rank=11; Rank<=13; ++Rank)
		if (State.SkillPile.ContainsByPredicate([Rank](const FHexCard& Card){return Card.Rank==Rank;})) Types.Add(Rank);
	// J:Q:K = 2:1:2, renormalized only when physical copies of a type are unavailable.
	int32 TotalRankWeight=0;
	for (int32 Type:Types) TotalRankWeight+=Type==12?1:2;
	int32 RankRoll=State.SkillRandom.RandRange(1,TotalRankWeight);
	int32 Rank=Types.Last();
	for (int32 Type:Types) { RankRoll-=Type==12?1:2; if(RankRoll<=0){Rank=Type;break;} }
	UHexBoardSubsystem* Board=GetBoard();
	const UHexGridMovementComponent* Mover=GetMover();
	if (Board && Board->IsPerformanceMode())
	{
		int32 Total=0;
		for (const FHexCard& Card:State.SkillPile) if(Card.Rank==Rank) Total+=SkillSuitWeights[static_cast<int32>(Card.Suit)];
		int32 Roll=State.SkillRandom.RandRange(1,FMath::Max(1,Total));
		for (int32 I=0; I<State.SkillPile.Num(); ++I)
		{
			const FHexCard& Card=State.SkillPile[I]; if(Card.Rank!=Rank) continue;
			Roll-=SkillSuitWeights[static_cast<int32>(Card.Suit)];
			if(Roll<=0) {State.SkillPile.Swap(0,I);break;}
		}
		return; // opening terrain histogram only; never invokes route prediction
	}
	TArray<FIntPoint> Route;
	PlanHandHint(Route);
	if (Route.IsEmpty() && Board) Board->GetSolutionPlayerSteps(Route);
	float Scores[4]={0,0,0,0};
	if (Board && Mover)
	{
		for (AHexTile* Tile:Board->GetAllTiles())
		{
			if (!Tile->HasCardLabel()) continue;
			const FIntPoint Coord=Tile->GetCoord();
			const int Suit=static_cast<int>(Tile->GetCardLabel().Suit);
			FHexCard Candidate(Rank,Tile->GetCardLabel().Suit); Candidate.Kind=EHexCardKind::Skill;
			FHexMoveRecord Plan;
			if (Rank==12 && Coord!=Mover->GetCoord() && !Board->GetBoxAt(Coord))
			{
				float Score=Route.Contains(Coord) ? 20.f : 1.f;
				if (Tile->IsGoal()) Score+=8.f;
				for (int D=0;D<6;++D)
				{
					const FIntPoint N=Coord+HexSokoban::DirectionOffset(static_cast<EHexDirection>(D));
					if (Board->GetBoxAt(N)) Score+=12.f;
					if (AHexTile* Neighbor=Board->GetTileAt(N)) if (Neighbor->IsGoal()) Score+=6.f;
				}
				Scores[Suit]=FMath::Max(Scores[Suit],Score);
			}
			else if (Rank==13 && Board->EvaluateSkill(Mover->GetCoord(),Coord,Candidate,Plan))
			{
				const FIntPoint Delta=Coord-Mover->GetCoord();
				const float Distance=(FMath::Abs(Delta.X)+FMath::Abs(Delta.Y)+FMath::Abs(Delta.X+Delta.Y))*.5f;
				Scores[Suit]=FMath::Max(Scores[Suit],Distance*10.f+(Route.Contains(Coord)?3.f:0.f));
			}
			else if (Rank==11 && Board->GetBoxAt(Coord))
			{
				for (int D=0;D<6;++D)
				{
					const FIntPoint Stand=Coord-HexSokoban::DirectionOffset(static_cast<EHexDirection>(D));
					if (!Board->GetTileAt(Stand) || Board->GetBoxAt(Stand) || !Board->EvaluateSkill(Stand,Coord,Candidate,Plan)) continue;
					float Score=Plan.SkillBoxes.Num()>1 ? 100.f : 0.f;
					for (const FHexSkillBoxMove& Shift:Plan.SkillBoxes)
						if (Board->GetTileAt(Shift.To)->IsGoal()) Score+=50.f;
					if (Score>0 && (Route.Contains(Coord)||Route.Contains(Stand))) Score+=20.f;
					Scores[Suit]=FMath::Max(Scores[Suit],Score);
				}
			}
		}
	}
	float Best=-1.f; TArray<int32> Choices;
	for (int Index=0;Index<State.SkillPile.Num();++Index)
	{
		const FHexCard& Card=State.SkillPile[Index]; if (Card.Rank!=Rank) continue;
		const float Score=Scores[static_cast<int>(Card.Suit)];
		if (Score>Best) { Best=Score; Choices.Reset(); }
		if (Score==Best) Choices.Add(Index);
	}
	State.SkillPile.Swap(0,Choices[State.SkillRandom.RandRange(0,Choices.Num()-1)]);
}

void UHexCardSubsystem::ReturnSkill(FHexCard Card)
{
	if (GetBoard() && GetBoard()->IsPerformanceMode())
	{
		// Index zero has already been shown. Returning cards join the hidden weighted pool
		// immediately, but cannot replace that committed top card or duplicate cards in hand.
		const int32 Position=State.SkillPile.IsEmpty()?0:State.SkillRandom.RandRange(1,State.SkillPile.Num());
		State.SkillPile.Insert(Card,Position);
	}
	else State.SkillDiscard.Add(Card);
}

void UHexCardSubsystem::AwardSkill()
{
	if (State.SkillPile.IsEmpty()) return;
	const FHexCard Card=State.SkillPile[0];
	State.SkillPile.RemoveAt(0); State.Hand.Add(Card);
	UHexAudioSubsystem::Play(this,TEXT("draw_card"));
	OnCardDrawn.Broadcast(Card);
	PrepareNextSkill();
}
