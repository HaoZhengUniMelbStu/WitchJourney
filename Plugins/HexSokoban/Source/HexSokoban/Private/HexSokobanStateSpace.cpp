#include "HexSokobanStateSpace.h"
#include "HexSokobanSolver.h"
#include "HexSokobanMath.h"

namespace HexStateSpace
{
	inline int32 Opposite(int32 Dir) { return (Dir + 3) % FHexStateSpace::NumDirs; }

	/** Slot of a box on Cell (1..), or INDEX_NONE. */
	inline int32 FindBox(const FHexStateKey& Key, int32 Cell)
	{
		for (int32 Index = 1; Index < Key.Cells.Num(); ++Index)
		{
			if (Key.Cells[Index] == Cell)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
}

bool FHexStateSpace::Build(const FHexSolverInput& Input, int32 MaxStates)
{
	bValid = false;
	bBudgetExceeded = false;
	Keys.Reset();
	IndexOf.Reset();
	Children.Reset();
	Cost.Reset();
	CellOf.Reset();
	StartState = INDEX_NONE;

	// ---- Index the board.
	for (const FIntPoint& Tile : Input.Tiles)
	{
		if (!CellOf.Contains(Tile))
		{
			CellOf.Add(Tile, CellOf.Num());
		}
	}
	NumCells = CellOf.Num();
	if (NumCells == 0 || NumCells > MAX_int16)
	{
		return false;
	}
	CoordOf.SetNum(NumCells);
	for (const TPair<FIntPoint, int32>& Pair : CellOf)
	{
		CoordOf[Pair.Value] = Pair.Key;
	}
	Neighbors.Init(-1, NumCells * NumDirs);
	for (int32 Cell = 0; Cell < NumCells; ++Cell)
	{
		for (int32 Dir = 0; Dir < NumDirs; ++Dir)
		{
			if (const int32* Neighbor = CellOf.Find(CoordOf[Cell] + HexSokoban::DirectionOffset(static_cast<EHexDirection>(Dir))))
			{
				Neighbors[Cell * NumDirs + Dir] = *Neighbor;
			}
		}
	}
	IsGoal.Init(false, NumCells);
	NumGoals = 0;
	for (const FIntPoint& Goal : Input.Goals)
	{
		const int32* Cell = CellOf.Find(Goal);
		if (Cell && !IsGoal[*Cell])
		{
			IsGoal[*Cell] = true;
			++NumGoals;
		}
	}
	const int32* PlayerCell = CellOf.Find(Input.Player);
	if (!PlayerCell || NumGoals == 0)
	{
		return false;
	}

	// ---- Live cells: a box there can still be pushed onto some goal (reverse pushes from the goals).
	Live.Init(false, NumCells);
	{
		TArray<int32> Queue;
		for (int32 Cell = 0; Cell < NumCells; ++Cell)
		{
			if (IsGoal[Cell])
			{
				Live[Cell] = true;
				Queue.Add(Cell);
			}
		}
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 To = Queue[Head];
			for (int32 Dir = 0; Dir < NumDirs; ++Dir)
			{
				const int32 From = Next(To, HexStateSpace::Opposite(Dir));
				const int32 Pusher = From >= 0 ? Next(From, HexStateSpace::Opposite(Dir)) : -1;
				if (From >= 0 && Pusher >= 0 && !Live[From])
				{
					Live[From] = true;
					Queue.Add(From);
				}
			}
		}
	}

	// ---- Start position.
	FHexStateKey Start;
	Start.Cells.Add(static_cast<int16>(*PlayerCell));
	{
		TArray<int16> Boxes;
		for (const FIntPoint& Box : Input.Boxes)
		{
			const int32* Cell = CellOf.Find(Box);
			if (!Cell || *Cell == *PlayerCell)
			{
				return false;
			}
			Boxes.Add(static_cast<int16>(*Cell));
		}
		Boxes.Sort();
		Start.Cells.Append(Boxes);
	}
	auto IsSolved = [this](const FHexStateKey& Key)
	{
		int32 Covered = 0;
		for (int32 Index = 1; Index < Key.Cells.Num(); ++Index)
		{
			Covered += IsGoal[Key.Cells[Index]] ? 1 : 0;
		}
		return Covered >= NumGoals;
	};

	// ---- Forward BFS over single steps.
	Keys.Add(Start);
	IndexOf.Add(Start, 0);
	StartState = 0;
	TArray<bool> Solved;
	Solved.Add(IsSolved(Start));
	for (int32 Head = 0; Head < Keys.Num(); ++Head)
	{
		Children.AddUninitialized(NumDirs);
		for (int32 Dir = 0; Dir < NumDirs; ++Dir)
		{
			Children[Head * NumDirs + Dir] = INDEX_NONE;
		}
		if (Solved[Head])
		{
			continue;	// the level ends here
		}
		const FHexStateKey Current = Keys[Head];	// copy: Keys may grow below
		const int32 Player = Current.Cells[0];
		for (int32 Dir = 0; Dir < NumDirs; ++Dir)
		{
			const int32 Target = Next(Player, Dir);
			if (Target < 0)
			{
				continue;
			}
			FHexStateKey Child = Current;
			Child.Cells[0] = static_cast<int16>(Target);
			const int32 BoxSlot = HexStateSpace::FindBox(Current, Target);
			if (BoxSlot != INDEX_NONE)
			{
				const int32 BoxTarget = Next(Target, Dir);
				if (BoxTarget < 0 || !Live[BoxTarget] || HexStateSpace::FindBox(Current, BoxTarget) != INDEX_NONE)
				{
					continue;	// blocked, or a push that can never be undone into a win
				}
				Child.Cells[BoxSlot] = static_cast<int16>(BoxTarget);
				TArray<int16> Boxes(Child.Cells.GetData() + 1, Child.Cells.Num() - 1);
				Boxes.Sort();
				FMemory::Memcpy(Child.Cells.GetData() + 1, Boxes.GetData(), Boxes.Num() * sizeof(int16));
			}
			int32 ChildIndex = INDEX_NONE;
			if (const int32* Found = IndexOf.Find(Child))
			{
				ChildIndex = *Found;
			}
			else
			{
				if (Keys.Num() >= MaxStates)
				{
					bBudgetExceeded = true;
					Keys.Reset();
					IndexOf.Reset();
					Children.Reset();
					return false;	// too big: callers fall back to the push solver
				}
				ChildIndex = Keys.Num();
				Solved.Add(IsSolved(Child));
				IndexOf.Add(Child, ChildIndex);
				Keys.Add(MoveTemp(Child));
			}
			Children[Head * NumDirs + Dir] = ChildIndex;
		}
	}

	// ---- Reverse BFS from the solved positions: exact cost to go.
	const int32 NumStates = Keys.Num();
	TArray<int32> ParentStart;
	TArray<int32> ParentList;
	{
		TArray<int32> Count;
		Count.Init(0, NumStates + 1);
		for (int32 Edge = 0; Edge < Children.Num(); ++Edge)
		{
			if (Children[Edge] != INDEX_NONE)
			{
				++Count[Children[Edge] + 1];
			}
		}
		for (int32 Index = 1; Index <= NumStates; ++Index)
		{
			Count[Index] += Count[Index - 1];
		}
		ParentStart = Count;
		ParentList.SetNumUninitialized(Count[NumStates]);
		TArray<int32> Fill = Count;
		for (int32 Edge = 0; Edge < Children.Num(); ++Edge)
		{
			const int32 Child = Children[Edge];
			if (Child != INDEX_NONE)
			{
				ParentList[Fill[Child]++] = Edge / NumDirs;
			}
		}
	}
	Cost.Init(-1, NumStates);
	TArray<int32> Queue;
	for (int32 Index = 0; Index < NumStates; ++Index)
	{
		if (Solved[Index])
		{
			Cost[Index] = 0;
			Queue.Add(Index);
		}
	}
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const int32 State = Queue[Head];
		for (int32 Edge = ParentStart[State]; Edge < ParentStart[State + 1]; ++Edge)
		{
			const int32 Parent = ParentList[Edge];
			if (Cost[Parent] < 0)
			{
				Cost[Parent] = Cost[State] + 1;
				Queue.Add(Parent);
			}
		}
	}
	bValid = true;
	return true;
}

int32 FHexStateSpace::FindState(const FIntPoint& Player, const TArray<FIntPoint>& Boxes) const
{
	if (!bValid)
	{
		return INDEX_NONE;
	}
	const int32* PlayerCell = CellOf.Find(Player);
	if (!PlayerCell)
	{
		return INDEX_NONE;
	}
	FHexStateKey Key;
	Key.Cells.Add(static_cast<int16>(*PlayerCell));
	TArray<int16> Cells;
	for (const FIntPoint& Box : Boxes)
	{
		const int32* Cell = CellOf.Find(Box);
		if (!Cell)
		{
			return INDEX_NONE;
		}
		Cells.Add(static_cast<int16>(*Cell));
	}
	Cells.Sort();
	Key.Cells.Append(Cells);
	const int32* Found = IndexOf.Find(Key);
	return Found ? *Found : INDEX_NONE;
}

int32 FHexStateSpace::Step(int32 State, int32 Dir, FIntPoint& OutEntered) const
{
	if (!Keys.IsValidIndex(State) || Dir < 0 || Dir >= NumDirs)
	{
		return INDEX_NONE;
	}
	const int32 Target = Next(Keys[State].Cells[0], Dir);
	if (Target < 0)
	{
		return INDEX_NONE;
	}
	OutEntered = CoordOf[Target];
	return Children[State * NumDirs + Dir];
}

bool FHexStateSpace::GetOptimalSteps(int32 State, TArray<FIntPoint>& OutSteps) const
{
	OutSteps.Reset();
	int32 Remaining = GetCost(State);
	if (Remaining <= 0)
	{
		return false;
	}
	while (Remaining > 0)
	{
		int32 Chosen = INDEX_NONE;
		FIntPoint Entered = FIntPoint::ZeroValue;
		for (int32 Dir = 0; Dir < NumDirs && Chosen == INDEX_NONE; ++Dir)
		{
			FIntPoint Cell;
			const int32 Child = Step(State, Dir, Cell);
			if (Child != INDEX_NONE && GetCost(Child) == Remaining - 1)
			{
				Chosen = Child;
				Entered = Cell;
			}
		}
		if (Chosen == INDEX_NONE)
		{
			OutSteps.Reset();
			return false;	// cannot happen with a consistent table
		}
		OutSteps.Add(Entered);
		State = Chosen;
		--Remaining;
	}
	return true;
}
