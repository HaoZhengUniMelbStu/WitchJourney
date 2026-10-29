#include "HexSokobanSolver.h"
#include "HexSokobanMath.h"

namespace
{
	constexpr int32 NumDirs = 6;

	inline int32 Opposite(int32 Dir) { return (Dir + 3) % NumDirs; }

	/** Search node: Cells[0] = normalised player cell, Cells[1..] = sorted box cells. */
	struct FSolverState
	{
		TArray<int16> Cells;

		bool operator==(const FSolverState& Other) const { return Cells == Other.Cells; }

		friend uint32 GetTypeHash(const FSolverState& State)
		{
			uint32 Hash = 0;
			for (const int16 Cell : State.Cells)
			{
				Hash = HashCombine(Hash, static_cast<uint32>(static_cast<uint16>(Cell)));
			}
			return Hash;
		}
	};

	class FBoardGraph
	{
	public:
		int32 Num = 0;
		TArray<int32> Neighbors;	// Num * 6, -1 = no tile
		TArray<bool> IsGoal;
		TArray<bool> Live;
		TArray<FIntPoint> CoordOf;

		int32 Next(int32 Cell, int32 Dir) const { return Neighbors[Cell * NumDirs + Dir]; }

		/** Flood fill the player's region; returns the lowest reachable cell (canonical player). */
		int32 Flood(int32 Start, const TArray<bool>& Occupied, TArray<bool>& OutReach, TArray<int32>& Stack) const
		{
			OutReach.Init(false, Num);
			Stack.Reset();
			Stack.Add(Start);
			OutReach[Start] = true;
			int32 Lowest = Start;
			while (Stack.Num() > 0)
			{
				const int32 Cell = Stack.Pop(EAllowShrinking::No);
				Lowest = FMath::Min(Lowest, Cell);
				for (int32 Dir = 0; Dir < NumDirs; ++Dir)
				{
					const int32 Neighbor = Next(Cell, Dir);
					if (Neighbor >= 0 && !OutReach[Neighbor] && !Occupied[Neighbor])
					{
						OutReach[Neighbor] = true;
						Stack.Add(Neighbor);
					}
				}
			}
			return Lowest;
		}

		/** Shortest walk From -> To avoiding Occupied. Appends the cells entered (excluding From). */
		bool Walk(int32 From, int32 To, const TArray<bool>& Occupied, TArray<int32>& OutCells) const
		{
			if (From == To)
			{
				return true;
			}
			TArray<int32> Prev;
			Prev.Init(-1, Num);
			TArray<int32> Queue;
			Queue.Add(From);
			Prev[From] = From;
			for (int32 Head = 0; Head < Queue.Num(); ++Head)
			{
				const int32 Cell = Queue[Head];
				if (Cell == To)
				{
					break;
				}
				for (int32 Dir = 0; Dir < NumDirs; ++Dir)
				{
					const int32 Neighbor = Next(Cell, Dir);
					if (Neighbor >= 0 && Prev[Neighbor] < 0 && !Occupied[Neighbor])
					{
						Prev[Neighbor] = Cell;
						Queue.Add(Neighbor);
					}
				}
			}
			if (Prev[To] < 0)
			{
				return false;
			}
			TArray<int32> Reversed;
			for (int32 Cell = To; Cell != From; Cell = Prev[Cell])
			{
				Reversed.Add(Cell);
			}
			for (int32 Index = Reversed.Num() - 1; Index >= 0; --Index)
			{
				OutCells.Add(Reversed[Index]);
			}
			return true;
		}
	};

	FHexSolveResult RunSearch(const FHexSolverInput& Input, int32 MaxStates, TArray<FIntPoint>* OutPlayerSteps)
	{
		FHexSolveResult Result;
		if (OutPlayerSteps)
		{
			OutPlayerSteps->Reset();
		}

		// ---- Index the board.
		FBoardGraph Graph;
		TMap<FIntPoint, int32> CellOf;
		for (const FIntPoint& Tile : Input.Tiles)
		{
			if (!CellOf.Contains(Tile))
			{
				CellOf.Add(Tile, CellOf.Num());
			}
		}
		Graph.Num = CellOf.Num();
		if (Graph.Num == 0 || Graph.Num > MAX_int16)
		{
			return Result; // Unknown
		}

		Graph.CoordOf.SetNum(Graph.Num);
		for (const TPair<FIntPoint, int32>& Pair : CellOf)
		{
			Graph.CoordOf[Pair.Value] = Pair.Key;
		}
		Graph.Neighbors.Init(-1, Graph.Num * NumDirs);
		for (int32 Cell = 0; Cell < Graph.Num; ++Cell)
		{
			for (int32 Dir = 0; Dir < NumDirs; ++Dir)
			{
				if (const int32* Neighbor = CellOf.Find(Graph.CoordOf[Cell] + HexSokoban::DirectionOffset(static_cast<EHexDirection>(Dir))))
				{
					Graph.Neighbors[Cell * NumDirs + Dir] = *Neighbor;
				}
			}
		}

		Graph.IsGoal.Init(false, Graph.Num);
		int32 NumGoals = 0;
		for (const FIntPoint& Goal : Input.Goals)
		{
			const int32* Cell = CellOf.Find(Goal);
			if (Cell && !Graph.IsGoal[*Cell])
			{
				Graph.IsGoal[*Cell] = true;
				++NumGoals;
			}
		}

		const int32* PlayerCell = CellOf.Find(Input.Player);
		TArray<int16> StartBoxes;
		for (const FIntPoint& Box : Input.Boxes)
		{
			const int32* Cell = CellOf.Find(Box);
			if (!Cell)
			{
				return Result; // a box off the board: don't judge
			}
			StartBoxes.Add(static_cast<int16>(*Cell));
		}
		if (!PlayerCell || NumGoals == 0)
		{
			return Result; // Unknown
		}
		if (StartBoxes.Num() < NumGoals)
		{
			Result.Status = EHexSolveStatus::Unsolvable;
			return Result;
		}

		// ---- 1. Live cells (reverse pushes from every goal).
		Graph.Live.Init(false, Graph.Num);
		{
			TArray<int32> Queue;
			for (int32 Cell = 0; Cell < Graph.Num; ++Cell)
			{
				if (Graph.IsGoal[Cell])
				{
					Graph.Live[Cell] = true;
					Queue.Add(Cell);
				}
			}
			for (int32 Head = 0; Head < Queue.Num(); ++Head)
			{
				const int32 To = Queue[Head];
				for (int32 Dir = 0; Dir < NumDirs; ++Dir)
				{
					// Box moved From -> To along Dir; pusher stood behind From.
					const int32 From = Graph.Next(To, Opposite(Dir));
					const int32 Pusher = From >= 0 ? Graph.Next(From, Opposite(Dir)) : -1;
					if (From >= 0 && Pusher >= 0 && !Graph.Live[From])
					{
						Graph.Live[From] = true;
						Queue.Add(From);
					}
				}
			}
		}

		int32 LiveBoxes = 0;
		for (const int16 Box : StartBoxes)
		{
			LiveBoxes += Graph.Live[Box] ? 1 : 0;
		}
		if (LiveBoxes < NumGoals)
		{
			Result.Status = EHexSolveStatus::Unsolvable;
			Result.bDeadBox = true;
			return Result;
		}

		// ---- 2. Breadth-first search over pushes.
		auto IsSolved = [&Graph, NumGoals](const TArray<int16>& Cells)
		{
			int32 Covered = 0;
			for (int32 Index = 1; Index < Cells.Num(); ++Index)
			{
				Covered += Graph.IsGoal[Cells[Index]] ? 1 : 0;
			}
			return Covered >= NumGoals;
		};

		TArray<bool> Occupied;
		TArray<bool> Reach;
		TArray<int32> Stack;

		FSolverState Start;
		{
			Occupied.Init(false, Graph.Num);
			for (const int16 Box : StartBoxes)
			{
				Occupied[Box] = true;
			}
			if (Occupied[*PlayerCell])
			{
				return Result; // player inside a box: don't judge
			}
			StartBoxes.Sort();
			Start.Cells.Add(static_cast<int16>(Graph.Flood(*PlayerCell, Occupied, Reach, Stack)));
			Start.Cells.Append(StartBoxes);
		}
		if (IsSolved(Start.Cells))
		{
			Result.Status = EHexSolveStatus::Solvable;
			Result.MinPushes = 0;
			return Result;
		}

		TArray<FSolverState> Queue;
		TArray<int32> Depth;
		TArray<int32> Parent;		// queue index of the parent state
		TArray<int16> PushFrom;		// box cell pushed to reach this state
		TArray<int8> PushDir;		// direction of that push
		TSet<FSolverState> Visited;
		Queue.Add(Start);
		Depth.Add(0);
		Parent.Add(INDEX_NONE);
		PushFrom.Add(-1);
		PushDir.Add(-1);
		Visited.Add(Start);

		int32 SolvedIndex = INDEX_NONE;
		for (int32 Head = 0; Head < Queue.Num() && SolvedIndex == INDEX_NONE; ++Head)
		{
			if (Visited.Num() > MaxStates)
			{
				Result.StatesExplored = Visited.Num();
				return Result; // Unknown
			}

			const FSolverState State = Queue[Head];
			const int32 StateDepth = Depth[Head];

			Occupied.Init(false, Graph.Num);
			for (int32 Index = 1; Index < State.Cells.Num(); ++Index)
			{
				Occupied[State.Cells[Index]] = true;
			}
			Graph.Flood(State.Cells[0], Occupied, Reach, Stack);
			const TArray<bool> PlayerReach = Reach;

			for (int32 BoxIndex = 1; BoxIndex < State.Cells.Num() && SolvedIndex == INDEX_NONE; ++BoxIndex)
			{
				const int32 Box = State.Cells[BoxIndex];
				for (int32 Dir = 0; Dir < NumDirs; ++Dir)
				{
					const int32 Behind = Graph.Next(Box, Opposite(Dir));
					const int32 Target = Graph.Next(Box, Dir);
					if (Behind < 0 || Target < 0 || !PlayerReach[Behind] || Occupied[Target] || !Graph.Live[Target])
					{
						continue;
					}

					TArray<int16> Boxes(State.Cells.GetData() + 1, State.Cells.Num() - 1);
					Boxes[BoxIndex - 1] = static_cast<int16>(Target);
					Boxes.Sort();

					Occupied[Box] = false;
					Occupied[Target] = true;
					const int32 Player = Graph.Flood(Box, Occupied, Reach, Stack);
					Occupied[Target] = false;
					Occupied[Box] = true;

					FSolverState Child;
					Child.Cells.Reserve(Boxes.Num() + 1);
					Child.Cells.Add(static_cast<int16>(Player));
					Child.Cells.Append(Boxes);

					if (Visited.Contains(Child))
					{
						continue;
					}
					const bool bSolved = IsSolved(Child.Cells);
					Visited.Add(Child);
					Queue.Add(MoveTemp(Child));
					Depth.Add(StateDepth + 1);
					Parent.Add(Head);
					PushFrom.Add(static_cast<int16>(Box));
					PushDir.Add(static_cast<int8>(Dir));
					if (bSolved)
					{
						SolvedIndex = Queue.Num() - 1;
						break;
					}
				}
			}
		}

		Result.StatesExplored = Visited.Num();
		if (SolvedIndex == INDEX_NONE)
		{
			Result.Status = EHexSolveStatus::Unsolvable;
			return Result;
		}
		Result.Status = EHexSolveStatus::Solvable;
		Result.MinPushes = Depth[SolvedIndex];

		// ---- 3. Rebuild the player's walk (optional).
		if (OutPlayerSteps)
		{
			TArray<int32> Chain;
			for (int32 Node = SolvedIndex; Node > 0; Node = Parent[Node])
			{
				Chain.Add(Node);
			}

			TArray<bool> Boxes;
			Boxes.Init(false, Graph.Num);
			for (const int16 Box : StartBoxes)
			{
				Boxes[Box] = true;
			}
			int32 PlayerAt = *PlayerCell;
			TArray<int32> Cells;
			for (int32 Index = Chain.Num() - 1; Index >= 0; --Index)
			{
				const int32 Node = Chain[Index];
				const int32 From = PushFrom[Node];
				const int32 Dir = PushDir[Node];
				const int32 Behind = Graph.Next(From, Opposite(Dir));
				const int32 Target = Graph.Next(From, Dir);
				if (!Graph.Walk(PlayerAt, Behind, Boxes, Cells))
				{
					OutPlayerSteps->Reset();
					return Result; // should not happen; keep the verdict, drop the path
				}
				Cells.Add(From);		// the pushing step
				Boxes[From] = false;
				Boxes[Target] = true;
				PlayerAt = From;
			}
			for (const int32 Cell : Cells)
			{
				OutPlayerSteps->Add(Graph.CoordOf[Cell]);
			}
		}
		return Result;
	}
}

FHexSolveResult FHexSokobanSolver::Solve(const FHexSolverInput& Input, int32 MaxStates)
{
	return RunSearch(Input, MaxStates, nullptr);
}

FHexSolveResult FHexSokobanSolver::FindSolutionSteps(const FHexSolverInput& Input, int32 MaxStates, TArray<FIntPoint>& OutPlayerSteps)
{
	return RunSearch(Input, MaxStates, &OutPlayerSteps);
}
