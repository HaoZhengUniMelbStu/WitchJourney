#include "HexDeckSolver.h"
#include "HexSokobanStateSpace.h"

namespace HexDeck
{
	struct FKey
	{
		int32 Position = INDEX_NONE;
		TArray<uint8> Counts;

		bool operator==(const FKey& Other) const { return Position == Other.Position && Counts == Other.Counts; }

		friend uint32 GetTypeHash(const FKey& Key)
		{
			uint32 Hash = ::GetTypeHash(Key.Position);
			for (const uint8 Count : Key.Counts)
			{
				Hash = HashCombine(Hash, Count);
			}
			return Hash;
		}
	};

	struct FNode
	{
		int32 Position = INDEX_NONE;
		int32 G = 0;
		int32 F = 0;
		int32 Parent = INDEX_NONE;
		int32 Face = INDEX_NONE;	// distinct face paid for the step into this node (INDEX_NONE = free tile)
		int32 CountsAt = 0;			// offset into the counts pool
		FIntPoint Entered = FIntPoint::ZeroValue;
	};
}

FHexDeckSolveResult FHexDeckSolver::Solve(const FHexStateSpace& Space, int32 RootState, const TMap<FIntPoint, FHexCard>& Labels,
	const TArray<FHexCard>& Hand, int32 MaxNodes)
{
	FHexDeckSolveResult Result;
	const int32 RootCost = Space.GetCost(RootState);
	if (!Space.IsValid() || RootCost < 0)
	{
		return Result;	// unknown position or already lost
	}
	if (RootCost == 0)
	{
		Result.bSolved = true;
		Result.Turns = 0;
		return Result;
	}

	// Duplicates collapse into one face with a count: identical cards never branch twice.
	TArray<FHexCard> Faces;
	TArray<uint8> RootCounts;
	for (const FHexCard& Card : Hand)
	{
		if (Card.Kind != EHexCardKind::Number) continue;
		const int32 Index = Faces.IndexOfByPredicate([&Card](const FHexCard& Face) { return Face.SameFace(Card); });
		if (Index == INDEX_NONE)
		{
			Faces.Add(Card);
			RootCounts.Add(1);
		}
		else if (RootCounts[Index] < MAX_uint8)
		{
			++RootCounts[Index];
		}
	}
	const int32 NumFaces = Faces.Num();

	// Without free tiles every remaining step needs a card: positions further away than the cards left are dead.
	bool bAllLabelled = true;
	for (const FIntPoint& Cell : Space.GetCells())
	{
		if (!Labels.Contains(Cell))
		{
			bAllLabelled = false;
			break;
		}
	}

	TArray<HexDeck::FNode> Nodes;
	TArray<uint8> Pool;
	TArray<int32> Open;
	TMap<HexDeck::FKey, int32> BestG;
	const auto Better = [&Nodes](int32 X, int32 Y)
	{
		return Nodes[X].F != Nodes[Y].F ? Nodes[X].F < Nodes[Y].F : Nodes[X].G > Nodes[Y].G;
	};

	HexDeck::FNode Root;
	Root.Position = RootState;
	Root.F = RootCost;
	Root.CountsAt = 0;
	Pool.Append(RootCounts);
	Nodes.Add(Root);
	Open.HeapPush(0, Better);

	int32 Solved = INDEX_NONE;
	int32 BestPartial = INDEX_NONE;
	while (Open.Num() > 0)
	{
		if (Nodes.Num() >= MaxNodes)
		{
			Result.bBudgetHit = true;
			break;
		}
		int32 Current = INDEX_NONE;
		Open.HeapPop(Current, Better, EAllowShrinking::No);
		const HexDeck::FNode Node = Nodes[Current];
		HexDeck::FKey Key;
		Key.Position = Node.Position;
		Key.Counts = TArray<uint8>(Pool.GetData() + Node.CountsAt, NumFaces);
		if (const int32* Seen = BestG.Find(Key))
		{
			if (*Seen <= Node.G)
			{
				continue;
			}
		}
		BestG.Add(MoveTemp(Key), Node.G);
		++Result.Expanded;

		const int32 Cost = Space.GetCost(Node.Position);
		if (Cost == 0)
		{
			Solved = Current;
			break;
		}
		if (Node.G > 0 && (BestPartial == INDEX_NONE || Nodes[BestPartial].F > Node.F
			|| (Nodes[BestPartial].F == Node.F && Nodes[BestPartial].G < Node.G)))
		{
			BestPartial = Current;
		}

		for (int32 Dir = 0; Dir < FHexStateSpace::NumDirs; ++Dir)
		{
			FIntPoint Entered;
			const int32 Child = Space.Step(Node.Position, Dir, Entered);
			const int32 ChildCost = Space.GetCost(Child);
			if (Child == INDEX_NONE || ChildCost < 0)
			{
				continue;	// blocked, or the level can no longer be won from there
			}
			auto Push = [&](int32 Face)
			{
				TArray<uint8, TInlineAllocator<64>> Counts(Pool.GetData() + Node.CountsAt, NumFaces);	// copy: Pool may grow
				if (Face != INDEX_NONE)
				{
					--Counts[Face];
				}
				int32 Left = 0;
				for (const uint8 Count : Counts)
				{
					Left += Count;
				}
				if (bAllLabelled && ChildCost > Left)
				{
					return;	// not enough cards for the steps still needed
				}
				const int32 At = Pool.Num();
				Pool.Append(Counts.GetData(), Counts.Num());
				HexDeck::FNode Next;
				Next.Position = Child;
				Next.G = Node.G + 1;
				Next.F = Next.G + ChildCost;
				Next.Parent = Current;
				Next.Face = Face;
				Next.CountsAt = At;
				Next.Entered = Entered;
				Open.HeapPush(Nodes.Add(Next), Better);
			};
			const FHexCard* Label = Labels.Find(Entered);
			if (!Label)
			{
				Push(INDEX_NONE);
				continue;
			}
			for (int32 Face = 0; Face < NumFaces; ++Face)
			{
				if (Pool[Node.CountsAt + Face] > 0 && Faces[Face].Matches(*Label))
				{
					Push(Face);
				}
			}
		}
	}

	const int32 Chosen = Solved != INDEX_NONE ? Solved : BestPartial;
	if (Chosen != INDEX_NONE)
	{
		for (int32 Index = Chosen; Index > 0; Index = Nodes[Index].Parent)
		{
			Result.Steps.Insert(Nodes[Index].Entered, 0);
			Result.Cards.Insert(Nodes[Index].Face != INDEX_NONE ? Faces[Nodes[Index].Face] : FHexCard(), 0);
		}
	}
	if (Solved != INDEX_NONE)
	{
		Result.bSolved = true;
		Result.Turns = Nodes[Solved].G;
	}
	return Result;
}
