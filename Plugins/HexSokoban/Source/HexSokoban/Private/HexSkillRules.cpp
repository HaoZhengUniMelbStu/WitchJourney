#include "HexSkillRules.h"
#include "HexSokobanMath.h"

bool HexSkillRules::Plan(const TMap<FIntPoint, FHexCard>& Tiles, const TSet<FIntPoint>& Boxes,
	FIntPoint Player, FIntPoint Target, const FHexCard& Card, FHexSkillPlan& Out)
{
	Out = FHexSkillPlan();
	const FHexCard* Label = Tiles.Find(Target);
	if (Card.Kind != EHexCardKind::Skill || !Label || Label->Suit != Card.Suit || Target == Player) return false;
	Out.Destination = Target;
	if (Card.Rank == 12) return !Boxes.Contains(Target);
	if (Card.Rank != 11 && Card.Rank != 13) return false;
	if (Card.Rank == 11)
	{
		if (!Boxes.Contains(Target)) return false;
		Out.Destination = Player;
	}
	else if (Boxes.Contains(Target)) return false;
	for (uint8 D = 0; D < 6; ++D)
	{
		const EHexDirection Direction = static_cast<EHexDirection>(D);
		const FIntPoint Offset = HexSokoban::DirectionOffset(Direction);
		FIntPoint Cell = Player + Offset;
		if (Card.Rank == 13)
		{
			while (Tiles.Contains(Cell) && Cell != Target && !Boxes.Contains(Cell)) Cell += Offset;
		}
		if (Cell != Target) continue;
		Out.Direction = Direction;
		if (!Boxes.Contains(Target)) return true;
		// 1) Impulse chain: touching boxes, or a box one empty cell ahead (inside the two-cell reach).
		//    A wall / board edge only ends the chain; it never cancels the cast by itself.
		TArray<FIntPoint> Chain;
		Chain.Add(Target);
		for (FIntPoint Last = Target;;)
		{
			const FIntPoint Next = Last + Offset;
			if (!Tiles.Contains(Next)) break;
			if (Boxes.Contains(Next)) { Chain.Add(Next); Last = Next; continue; }
			const FIntPoint Second = Next + Offset;
			if (Tiles.Contains(Second) && Boxes.Contains(Second)) { Chain.Add(Second); Last = Second; continue; }
			break;
		}
		// 2) Resolve from the far end back. A box advances one cell when that cell is a tile and is
		//    either empty or vacated by the box in front of it. A stuck box stays put and only blocks
		//    the box directly touching it, so a gap in front of the target still lets it move.
		TArray<bool> CanMove;
		CanMove.SetNumZeroed(Chain.Num());
		for (int32 Index = Chain.Num() - 1; Index >= 0; --Index)
		{
			const FIntPoint Dest = Chain[Index] + Offset;
			if (!Tiles.Contains(Dest)) continue;
			const bool bTouching = Chain.IsValidIndex(Index + 1) && Chain[Index + 1] == Dest;
			CanMove[Index] = bTouching ? CanMove[Index + 1] : !Boxes.Contains(Dest);
		}
		// 3) Only a target with no free cell at all ahead refuses the cast.
		if (!CanMove[0]) return false;
		// 4) The full two-cell impulse is used only when nothing stands in either cell;
		//    otherwise every moving box advances one cell (the impulse is shared down the line).
		const FIntPoint First = Target + Offset;
		const bool bFullReach = !Boxes.Contains(First) && Tiles.Contains(First + Offset) && !Boxes.Contains(First + Offset);
		for (int32 Index = 0; Index < Chain.Num(); ++Index)
		{
			if (!CanMove[Index]) continue;
			const FIntPoint From = Chain[Index];
			Out.Boxes.Add({From, From + Offset * (Index == 0 && bFullReach ? 2 : 1)});
		}
		return true;
	}
	return false;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexSkillRulesTest, "HexSokoban.Cards.SkillRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexSkillRulesTest::RunTest(const FString& Parameters)
{
	TMap<FIntPoint,FHexCard> Tiles;
	for (int X=0; X<8; ++X) Tiles.Add(FIntPoint(X,0),FHexCard(5,EHexSuit::Heart));
	FHexCard Card(12,EHexSuit::Heart); Card.Kind=EHexCardKind::Skill;
	FHexSkillPlan Plan;
	TestTrue(TEXT("Q crosses blockers"),HexSkillRules::Plan(Tiles,{FIntPoint(1,0)},FIntPoint(0,0),FIntPoint(7,0),Card,Plan));
	TestFalse(TEXT("Q cannot overlap box"),HexSkillRules::Plan(Tiles,{FIntPoint(7,0)},FIntPoint(0,0),FIntPoint(7,0),Card,Plan));
	TestFalse(TEXT("Q cannot target self"),HexSkillRules::Plan(Tiles,{},FIntPoint(0,0),FIntPoint(0,0),Card,Plan));
	Card.Suit=EHexSuit::Club;
	TestFalse(TEXT("Same suit mandatory"),HexSkillRules::Plan(Tiles,{},FIntPoint(0,0),FIntPoint(7,0),Card,Plan));
	Card.Suit=EHexSuit::Heart; Card.Rank=13;
	TestTrue(TEXT("K follows open ray"),HexSkillRules::Plan(Tiles,{},FIntPoint(0,0),FIntPoint(6,0),Card,Plan));
	TestFalse(TEXT("K blocked by earlier box"),HexSkillRules::Plan(Tiles,{FIntPoint(2,0)},FIntPoint(0,0),FIntPoint(6,0),Card,Plan));
	TestFalse(TEXT("K cannot target occupied endpoint even with push space"),HexSkillRules::Plan(Tiles,{FIntPoint(6,0)},FIntPoint(0,0),FIntPoint(6,0),Card,Plan));
	Tiles.Remove(FIntPoint(3,0));
	TestFalse(TEXT("K cannot cross missing tile"),HexSkillRules::Plan(Tiles,{},FIntPoint(0,0),FIntPoint(6,0),Card,Plan));
	Tiles.Add(FIntPoint(3,0),FHexCard(5,EHexSuit::Heart)); Card.Rank=11;
	TestFalse(TEXT("J cannot cast on empty adjacent tile"),HexSkillRules::Plan(Tiles,{},FIntPoint(0,0),FIntPoint(1,0),Card,Plan));
	TestFalse(TEXT("J cannot target distant box"),HexSkillRules::Plan(Tiles,{FIntPoint(2,0)},FIntPoint(0,0),FIntPoint(2,0),Card,Plan));
	TestTrue(TEXT("J single box"),HexSkillRules::Plan(Tiles,{FIntPoint(1,0)},FIntPoint(0,0),FIntPoint(1,0),Card,Plan));
	TestEqual(TEXT("J caster stays in place"),Plan.Destination,FIntPoint(0,0));
	TestEqual(TEXT("Single box two steps"),Plan.Boxes[0].To,FIntPoint(3,0));
	TestTrue(TEXT("J chain"),HexSkillRules::Plan(Tiles,{FIntPoint(1,0),FIntPoint(2,0)},FIntPoint(0,0),FIntPoint(1,0),Card,Plan));
	TestEqual(TEXT("First box one step"),Plan.Boxes[0].To,FIntPoint(2,0));
	TestEqual(TEXT("Last box one step"),Plan.Boxes[1].To,FIntPoint(3,0));
	TestTrue(TEXT("J near wall falls back one cell"),HexSkillRules::Plan(Tiles,{FIntPoint(6,0)},FIntPoint(5,0),FIntPoint(6,0),Card,Plan));
	TestEqual(TEXT("Stopped at wall"),Plan.Boxes[0].To,FIntPoint(7,0));
	TestFalse(TEXT("J jammed chain"),HexSkillRules::Plan(Tiles,{FIntPoint(6,0),FIntPoint(7,0)},FIntPoint(5,0),FIntPoint(6,0),Card,Plan));
	TestFalse(TEXT("J target against wall"),HexSkillRules::Plan(Tiles,{FIntPoint(7,0)},FIntPoint(6,0),FIntPoint(7,0),Card,Plan));
	// Gap-linked box ahead moves too when it has room.
	TestTrue(TEXT("J gap chain"),HexSkillRules::Plan(Tiles,{FIntPoint(1,0),FIntPoint(3,0)},FIntPoint(0,0),FIntPoint(1,0),Card,Plan));
	if (TestEqual(TEXT("Gap chain moves both"),Plan.Boxes.Num(),2))
	{
		TestEqual(TEXT("Gap chain target one step"),Plan.Boxes[0].To,FIntPoint(2,0));
		TestEqual(TEXT("Gap chain tail one step"),Plan.Boxes[1].To,FIntPoint(4,0));
	}
	// Gap-linked box stuck at the edge: the target still fills the gap, the stuck box stays.
	Tiles.Remove(FIntPoint(4,0));
	TestTrue(TEXT("J gap then stuck box still casts"),HexSkillRules::Plan(Tiles,{FIntPoint(1,0),FIntPoint(3,0)},FIntPoint(0,0),FIntPoint(1,0),Card,Plan));
	if (TestEqual(TEXT("Only target moves"),Plan.Boxes.Num(),1)) TestEqual(TEXT("Target fills the gap"),Plan.Boxes[0].To,FIntPoint(2,0));
	// Touching pair, then a gap, then a stuck box: the pair advances, the stuck box stays.
	Tiles.Add(FIntPoint(-1,0),FHexCard(5,EHexSuit::Heart));
	TestTrue(TEXT("J pair before stuck box"),HexSkillRules::Plan(Tiles,{FIntPoint(0,0),FIntPoint(1,0),FIntPoint(3,0)},FIntPoint(-1,0),FIntPoint(0,0),Card,Plan));
	if (TestEqual(TEXT("Pair moves"),Plan.Boxes.Num(),2)) TestEqual(TEXT("Pair tail into gap"),Plan.Boxes[1].To,FIntPoint(2,0));
	// Touching stuck box directly ahead: no free cell at all.
	TestFalse(TEXT("J blocked by stuck neighbour"),HexSkillRules::Plan(Tiles,{FIntPoint(2,0),FIntPoint(3,0)},FIntPoint(1,0),FIntPoint(2,0),Card,Plan));
	return true;
}
#endif
