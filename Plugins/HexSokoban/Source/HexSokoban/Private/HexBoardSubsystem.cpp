#include "HexBoardSubsystem.h"
#include "HexSkillRules.h"
#include "HexSokobanGameMode.h"
#include "HexGameFlowSubsystem.h"
#include "HexSokobanMath.h"
#include "HexTile.h"
#include "HexBox.h"
#include "HexGridLibrary.h"
#include "HexGridMovementComponent.h"
#include "HexSokobanSolver.h"
#include "HexSokobanStateSpace.h"
#include "HexSokobanSettings.h"
#include "EngineUtils.h"
#include "Engine/World.h"

bool UHexBoardSubsystem::IsPerformanceMode() const
{
	const UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this);
	const FHexLevelDefinition* Definition = Flow ? Flow->GetCurrentDefinition() : nullptr;
	return bRuntimePerformanceMode || (Definition && Definition->NeedsPerformanceMode()) || Tiles.Num() >= 80 || Boxes.Num() >= 8;
}

void UHexBoardSubsystem::RebuildBoard()
{
	Tiles.Reset();
	Boxes.Reset();
	History.Reset();
	ScoredMoveCount = 0;
	bHasInitialState = false;
	StateSpace.Reset();
	bStateSpaceTried = false;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 NumTiles = 0;
	for (TActorIterator<AHexTile> It(World); It; ++It)
	{
		AHexTile* Tile = *It;
		if (!IsValid(Tile) || Tile->bScenery)
		{
			continue;
		}
		const FIntPoint Coord = UHexGridLibrary::WorldToCoord(Tile->GetActorLocation());
		if (Tiles.Contains(Coord))
		{
			UE_LOG(LogHexSokoban, Warning, TEXT("Tile %s overlaps another tile at (%d,%d); ignored."), *Tile->GetName(), Coord.X, Coord.Y);
			continue;
		}
		const FVector Lattice = UHexGridLibrary::CoordToWorld(Coord);
		if (FVector::Dist2D(Lattice, Tile->GetActorLocation()) > 0.25 * UHexGridLibrary::GetTileSpacing())
		{
			UE_LOG(LogHexSokoban, Warning, TEXT("Tile %s is far from its lattice center (%d,%d). Check TileSpacing / GridOrigin in Project Settings > Plugins > Hex Sokoban."), *Tile->GetName(), Coord.X, Coord.Y);
		}
		Tile->SetCoord(Coord);
		Tiles.Add(Coord, Tile);
		++NumTiles;
	}

	int32 NumBoxes = 0;
	for (TActorIterator<AHexBox> It(World); It; ++It)
	{
		AHexBox* Box = *It;
		if (!IsValid(Box))
		{
			continue;
		}
		FIntPoint Coord = UHexGridLibrary::WorldToCoord(Box->GetActorLocation());
		AHexTile* Tile = GetTileAt(Coord);
		if (!Tile)
		{
			Tile = FindNearestTile(Box->GetActorLocation());
			if (!Tile)
			{
				UE_LOG(LogHexSokoban, Warning, TEXT("Box %s has no tile to stand on; ignored."), *Box->GetName());
				continue;
			}
			UE_LOG(LogHexSokoban, Warning, TEXT("Box %s was not on a tile; moved to nearest tile %s."), *Box->GetName(), *Tile->GetName());
			Coord = Tile->GetCoord();
		}
		if (Boxes.Contains(Coord))
		{
			UE_LOG(LogHexSokoban, Warning, TEXT("Box %s overlaps another box at (%d,%d); ignored."), *Box->GetName(), Coord.X, Coord.Y);
			continue;
		}
		Box->SetOnGoal(Tile->IsGoal());
		Box->PlaceOnTile(Coord, Tile);
		Boxes.Add(Coord, Box);
		++NumBoxes;
	}

	bBuilt = true;
	UE_LOG(LogHexSokoban, Log, TEXT("Board built: %d tiles, %d boxes, %d goals."), NumTiles, NumBoxes, GetNumGoals());
	const int32 NumGoals = GetNumGoals();
	if (NumGoals == 0)
	{
		UE_LOG(LogHexSokoban, Warning, TEXT("Board has no goal tiles; the level can never be completed."));
	}
	else if (NumBoxes < NumGoals)
	{
		UE_LOG(LogHexSokoban, Warning, TEXT("Board has %d goals but only %d boxes; every goal needs a box, so the level can never be completed."), NumGoals, NumBoxes);
	}
}

void UHexBoardSubsystem::EnsureBoardBuilt()
{
	if (!bBuilt)
	{
		RebuildBoard();
	}
}

AHexTile* UHexBoardSubsystem::GetTileAt(FIntPoint Coord) const
{
	const TWeakObjectPtr<AHexTile>* Found = Tiles.Find(Coord);
	return Found ? Found->Get() : nullptr;
}

AHexBox* UHexBoardSubsystem::GetBoxAt(FIntPoint Coord) const
{
	const TWeakObjectPtr<AHexBox>* Found = Boxes.Find(Coord);
	return Found ? Found->Get() : nullptr;
}

AHexTile* UHexBoardSubsystem::FindNearestTile(const FVector& WorldLocation) const
{
	AHexTile* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (const TPair<FIntPoint, TWeakObjectPtr<AHexTile>>& Pair : Tiles)
	{
		if (AHexTile* Tile = Pair.Value.Get())
		{
			const double DistSq = FVector::DistSquared2D(Tile->GetActorLocation(), WorldLocation);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Tile;
			}
		}
	}
	return Best;
}

TArray<AHexTile*> UHexBoardSubsystem::GetAllTiles() const
{
	TArray<AHexTile*> Result;
	Result.Reserve(Tiles.Num());
	for (const TPair<FIntPoint, TWeakObjectPtr<AHexTile>>& Pair : Tiles)
	{
		if (AHexTile* Tile = Pair.Value.Get())
		{
			Result.Add(Tile);
		}
	}
	return Result;
}

TArray<AHexBox*> UHexBoardSubsystem::GetAllBoxes() const
{
	TArray<AHexBox*> Result;
	Result.Reserve(Boxes.Num());
	for (const TPair<FIntPoint, TWeakObjectPtr<AHexBox>>& Pair : Boxes)
	{
		if (AHexBox* Box = Pair.Value.Get())
		{
			Result.Add(Box);
		}
	}
	return Result;
}

int32 UHexBoardSubsystem::GetNumGoals() const
{
	int32 Count = 0;
	for (const TPair<FIntPoint, TWeakObjectPtr<AHexTile>>& Pair : Tiles)
	{
		const AHexTile* Tile = Pair.Value.Get();
		if (Tile && Tile->IsGoal())
		{
			++Count;
		}
	}
	return Count;
}

int32 UHexBoardSubsystem::GetNumCoveredGoals() const
{
	int32 Count = 0;
	for (const TPair<FIntPoint, TWeakObjectPtr<AHexTile>>& Pair : Tiles)
	{
		const AHexTile* Tile = Pair.Value.Get();
		if (Tile && Tile->IsGoal() && GetBoxAt(Pair.Key))
		{
			++Count;
		}
	}
	return Count;
}

bool UHexBoardSubsystem::AreAllGoalsCovered() const
{
	const int32 Goals = GetNumGoals();
	return Goals > 0 && GetNumCoveredGoals() == Goals;
}

EHexMoveResult UHexBoardSubsystem::EvaluateMove(const FIntPoint& From, EHexDirection Direction, FHexMoveRecord& OutRecord) const
{
	OutRecord = FHexMoveRecord();
	OutRecord.Direction = Direction;
	OutRecord.PlayerFrom = From;
	OutRecord.PlayerTo = From;

	if (Direction == EHexDirection::Invalid)
	{
		return EHexMoveResult::Invalid;
	}

	const FIntPoint Offset = HexSokoban::DirectionOffset(Direction);
	const FIntPoint Target = From + Offset;
	if (!GetTileAt(Target))
	{
		return EHexMoveResult::BlockedNoTile;
	}

	if (AHexBox* Box = GetBoxAt(Target))
	{
		const FIntPoint BoxTarget = Target + Offset;
		if (!GetTileAt(BoxTarget))
		{
			return EHexMoveResult::BlockedNoTile;
		}
		if (GetBoxAt(BoxTarget))
		{
			return EHexMoveResult::BlockedByBox;
		}
		OutRecord.PlayerTo = Target;
		OutRecord.PushedBox = Box;
		OutRecord.BoxFrom = Target;
		OutRecord.BoxTo = BoxTarget;
		return EHexMoveResult::Pushed;
	}

	OutRecord.PlayerTo = Target;
	return EHexMoveResult::Moved;
}

void UHexBoardSubsystem::CommitMove(const FHexMoveRecord& Record, AActor* Pusher, float PusherRadius)
{
	if (Record.SkillRank)
	{
		// Occupancy changes atomically, so adjacent boxes never overwrite each other's entries.
		for (const FHexSkillBoxMove& Shift : Record.SkillBoxes) Boxes.Remove(Shift.From);
		for (const FHexSkillBoxMove& Shift : Record.SkillBoxes) Boxes.Add(Shift.To, Shift.Box);
		for (int32 Index = 0; Index < Record.SkillBoxes.Num(); ++Index)
		{
			const FHexSkillBoxMove& Shift = Record.SkillBoxes[Index];
			AHexTile* From = GetTileAt(Shift.From);
			AHexTile* To = GetTileAt(Shift.To);
			if (From) From->OnBoxLeft(Shift.Box);
			const float FlightDuration = FMath::Clamp(static_cast<float>(FVector::Dist2D(
				UHexGridLibrary::CoordToWorld(Record.PlayerFrom), UHexGridLibrary::CoordToWorld(Record.PlayerTo))/1700.0), .18f, .75f);
			const float Delay = Record.SkillRank == 11 ? .10f + Index * .14f : FMath::Max(.08f, FlightDuration-.10f);
			// Wind blade: a box that hands the impulse on, or ends against a box that could not move, rebounds.
			FIntPoint Step = Shift.To - Shift.From;
			if (Step.X % 2 == 0 && Step.Y % 2 == 0) Step /= 2;
			const bool bRebound = Record.SkillRank == 11 && (Index + 1 < Record.SkillBoxes.Num() || Boxes.Contains(Shift.To + Step));
			Shift.Box->BeginSkillPush(Shift.To, To, Delay, bRebound);
			Shift.Box->SetOnGoal(To && To->IsGoal());
			if (To) To->OnBoxEntered(Shift.Box);
		}
	}
	else if (Record.PushedBox)
	{
		MoveBoxOnBoard(Record.PushedBox, Record.BoxFrom, Record.BoxTo, Record.Direction, Pusher, PusherRadius);
	}
	History.Add(Record);
	ScoredMoveCount += Record.GetStepCost();
	OnMoveStarted.Broadcast(Record);
}

bool UHexBoardSubsystem::EvaluateSkill(FIntPoint From, FIntPoint Target, const FHexCard& Card, FHexMoveRecord& OutRecord) const
{
	TMap<FIntPoint,FHexCard> Labels;
	TSet<FIntPoint> Occupied;
	for (AHexTile* Tile : GetAllTiles()) if (Tile->HasCardLabel()) Labels.Add(Tile->GetCoord(),Tile->GetCardLabel());
	for (const auto& Pair : Boxes) if (Pair.Value.IsValid()) Occupied.Add(Pair.Key);
	FHexSkillPlan Plan;
	if (!HexSkillRules::Plan(Labels,Occupied,From,Target,Card,Plan)) return false;
	OutRecord = FHexMoveRecord();
	OutRecord.PlayerFrom = From; OutRecord.PlayerTo = Plan.Destination;
	OutRecord.Direction = Plan.Direction; OutRecord.SkillRank = Card.Rank;
	for (const FHexSkillBoxShift& Shift : Plan.Boxes)
	{
		FHexSkillBoxMove Move; Move.Box=GetBoxAt(Shift.From); Move.From=Shift.From; Move.To=Shift.To;
		OutRecord.SkillBoxes.Add(Move);
	}
	return true;
}

void UHexBoardSubsystem::NotifyMoveFinished(const FHexMoveRecord& Record)
{
	if (Record.SkillRank)
	{
		for (AHexBox* Box : GetAllBoxes()) Box->RefreshCardLabel(false);
		for (AHexTile* Tile : GetAllTiles()) Tile->SetLabelCovered(GetBoxAt(Tile->GetCoord()) != nullptr, false);
	}
	OnMoveFinished.Broadcast(Record);
}

bool UHexBoardSubsystem::IsAnyBoxMoving() const
{
	for (const TPair<FIntPoint, TWeakObjectPtr<AHexBox>>& Pair : Boxes)
	{
		const AHexBox* Box = Pair.Value.Get();
		if (Box && Box->IsMoving())
		{
			return true;
		}
	}
	return false;
}

void UHexBoardSubsystem::MoveBoxOnBoard(AHexBox* Box, const FIntPoint& From, const FIntPoint& To, EHexDirection Direction, AActor* Pusher, float PusherRadius)
{
	if (!Box)
	{
		return;
	}

	if (GetBoxAt(From) == Box)
	{
		Boxes.Remove(From);
	}
	Boxes.Add(To, Box);

	AHexTile* FromTile = GetTileAt(From);
	AHexTile* ToTile = GetTileAt(To);

	if (FromTile && From != To)
	{
		FromTile->OnBoxLeft(Box);
	}
	if (Pusher)
	{
		Box->BeginPush(To, ToTile, Direction, Pusher, PusherRadius);
		Box->SetOnGoal(ToTile && ToTile->IsGoal()); // visual change waits for the landing
	}
	else
	{
		Box->SetOnGoal(ToTile && ToTile->IsGoal());
		Box->PlaceOnTile(To, ToTile);
	}
	if (ToTile && From != To)
	{
		ToTile->OnBoxEntered(Box);
	}
}

void UHexBoardSubsystem::RegisterMover(UHexGridMovementComponent* InMover)
{
	EnsureBoardBuilt();
	Mover = InMover;
	CaptureInitialState();
}

void UHexBoardSubsystem::UnregisterMover(UHexGridMovementComponent* InMover)
{
	if (Mover.Get() == InMover)
	{
		Mover.Reset();
	}
}

void UHexBoardSubsystem::CaptureInitialState()
{
	InitialBoxes.Reset();
	InitialBoxCoords.Reset();
	for (const TPair<FIntPoint, TWeakObjectPtr<AHexBox>>& Pair : Boxes)
	{
		if (Pair.Value.IsValid())
		{
			InitialBoxes.Add(Pair.Value);
			InitialBoxCoords.Add(Pair.Key);
		}
	}
	if (UHexGridMovementComponent* M = Mover.Get())
	{
		InitialPlayerCoord = M->GetCoord();
	}
	bHasInitialState = true;
	StateSpace.Reset();
	bStateSpaceTried = false;
	if (Mover.IsValid())
	{
		if (const FHexStateSpace* Space = GetStateSpace())
		{
			UE_LOG(LogHexSokoban, Log, TEXT("Optimal baseline: %d steps (%d positions in the state space)."), Space->GetStartCost(), Space->GetNumStates());
		}
	}

	// Level-design aid: is the level as built solvable, and how short is the best solution?
	if (Mover.IsValid())
	{
		const FHexSolveResult Check = AnalyzeBoard(UHexSokobanSettings::Get().SolverMaxStates);
		switch (Check.Status)
		{
		case EHexSolveStatus::Solvable:
			UE_LOG(LogHexSokoban, Log, TEXT("Level is solvable: at least %d pushes (%d states searched)."), Check.MinPushes, Check.StatesExplored);
			break;
		case EHexSolveStatus::Unsolvable:
			UE_LOG(LogHexSokoban, Warning, TEXT("Level is NOT solvable as built%s."), Check.bDeadBox ? TEXT(" (a box can never reach a goal)") : TEXT(""));
			break;
		default:
			UE_LOG(LogHexSokoban, Log, TEXT("Level solvability unknown (search budget of %d states exceeded)."), UHexSokobanSettings::Get().SolverMaxStates);
			break;
		}
	}
}

const FHexStateSpace* UHexBoardSubsystem::GetStateSpace() const
{
	if (const auto* GM = GetWorld()->GetAuthGameMode<AHexSokobanGameMode>()) if (GM->IsBuildingLevel()) return nullptr;
	if (IsPerformanceMode()) return nullptr;
	if (!bStateSpaceTried && bHasInitialState && Mover.IsValid())
	{
		bStateSpaceTried = true;
		FHexSolverInput Input;
		BuildSolverInput(Input);
		Input.Player = InitialPlayerCoord;
		Input.Boxes = InitialBoxCoords;
		TSharedPtr<FHexStateSpace> Built = MakeShared<FHexStateSpace>();
		if (Built->Build(Input, UHexSokobanSettings::Get().SolverMaxStates))
		{
			StateSpace = Built;
		}
		else
		{
			if (Built->WasBudgetExceeded()) EnablePerformanceMode();
			UE_LOG(LogHexSokoban, Log, TEXT("State space too big or invalid: hints / dealing fall back to the push solver, no star baseline."));
		}
	}
	return StateSpace.IsValid() && StateSpace->IsValid() ? StateSpace.Get() : nullptr;
}

int32 UHexBoardSubsystem::GetCurrentStateIndex() const
{
	const FHexStateSpace* Space = GetStateSpace();
	const UHexGridMovementComponent* M = Mover.Get();
	if (!Space || !M || !M->IsOnBoard())
	{
		return INDEX_NONE;
	}
	TArray<FIntPoint> BoxCoords;
	for (const TPair<FIntPoint, TWeakObjectPtr<AHexBox>>& Pair : Boxes)
	{
		if (Pair.Value.IsValid())
		{
			BoxCoords.Add(Pair.Key);
		}
	}
	return Space->FindState(M->GetCoord(), BoxCoords);
}

int32 UHexBoardSubsystem::GetBaselineMoves() const
{
	const FHexStateSpace* Space = GetStateSpace();
	return Space ? Space->GetStartCost() : -1;
}

int32 UHexBoardSubsystem::GetMovesToWin() const
{
	const FHexStateSpace* Space = GetStateSpace();
	return Space ? Space->GetCost(GetCurrentStateIndex()) : -1;
}

void UHexBoardSubsystem::GetRecentPlayerCells(int32 Count, TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();
	const UHexGridMovementComponent* M = Mover.Get();
	if (!M || Count <= 0)
	{
		return;
	}
	OutCells.Add(M->GetCoord());
	for (int32 Index = History.Num() - 1; Index >= 0 && OutCells.Num() < Count; --Index)
	{
		const FHexMoveRecord& Record = History[Index];
		if (Record.PlayerFrom != Record.PlayerTo)
		{
			OutCells.Add(Record.PlayerFrom);
		}
	}
}

bool UHexBoardSubsystem::GetSolutionPlayerSteps(TArray<FIntPoint>& OutSteps, int32 MaxStates) const
{
	if (IsPerformanceMode()) { OutSteps.Reset(); return false; }
	if (const FHexStateSpace* Space = GetStateSpace())
	{
		const int32 Current = GetCurrentStateIndex();
		if (Current != INDEX_NONE)
		{
			return Space->GetOptimalSteps(Current, OutSteps);
		}
	}
	OutSteps.Reset();
	const UHexGridMovementComponent* M = Mover.Get();
	if (!M || !M->IsOnBoard())
	{
		return false;
	}
	FHexSolverInput Input;
	BuildSolverInput(Input);
	const FHexSolveResult Result = FHexSokobanSolver::FindSolutionSteps(Input, MaxStates, OutSteps);
	return Result.Status == EHexSolveStatus::Solvable && OutSteps.Num() > 0;
}

void UHexBoardSubsystem::BuildSolverInput(FHexSolverInput& Input) const
{
	if (const UHexGridMovementComponent* M = Mover.Get())
	{
		Input.Player = M->GetCoord();
	}
	for (const TPair<FIntPoint, TWeakObjectPtr<AHexTile>>& Pair : Tiles)
	{
		if (const AHexTile* Tile = Pair.Value.Get())
		{
			Input.Tiles.Add(Pair.Key);
			if (Tile->IsGoal())
			{
				Input.Goals.Add(Pair.Key);
			}
		}
	}
	for (const TPair<FIntPoint, TWeakObjectPtr<AHexBox>>& Pair : Boxes)
	{
		if (Pair.Value.IsValid())
		{
			Input.Boxes.Add(Pair.Key);
		}
	}
}

FHexSolveResult UHexBoardSubsystem::AnalyzeBoard(int32 MaxStates) const
{
	if (const auto* GM = GetWorld()->GetAuthGameMode<AHexSokobanGameMode>()) if (GM->IsBuildingLevel()) return FHexSolveResult();
	if (IsPerformanceMode()) return FHexSolveResult();
	const UHexGridMovementComponent* M = Mover.Get();
	if (!M || !M->IsOnBoard())
	{
		return FHexSolveResult();
	}

	FHexSolverInput Input;
	BuildSolverInput(Input);
	return FHexSokobanSolver::Solve(Input, MaxStates);
}

void UHexBoardSubsystem::NotifyPushBlocked(EHexDirection Direction, EHexMoveResult Result)
{
	OnPushBlocked.Broadcast(Direction, Result);
}

bool UHexBoardSubsystem::CommitPass()
{
	const UHexGridMovementComponent* M = Mover.Get();
	if (!M || !M->IsOnBoard())
	{
		return false;
	}
	FHexMoveRecord Record;
	Record.PlayerFrom = M->GetCoord();
	Record.PlayerTo = Record.PlayerFrom;	// Direction stays Invalid: a pass
	History.Add(Record);
	ScoredMoveCount += Record.GetStepCost();
	return true;
}

bool UHexBoardSubsystem::UndoLastMove()
{
	if (History.Num() == 0)
	{
		return false;
	}

	UHexGridMovementComponent* M = Mover.Get();
	if (M)
	{
		M->FinishStepImmediately(false);
	}

	const FHexMoveRecord Record = History.Pop();
	ScoredMoveCount -= Record.GetStepCost();
	if (Record.SkillRank)
	{
		for (const FHexSkillBoxMove& Shift : Record.SkillBoxes) { Shift.Box->FinishMoveImmediately(); Boxes.Remove(Shift.To); }
		for (const FHexSkillBoxMove& Shift : Record.SkillBoxes)
		{
			Shift.Box->SetOnGoal(GetTileAt(Shift.From) && GetTileAt(Shift.From)->IsGoal());
			Shift.Box->PlaceOnTile(Shift.From, GetTileAt(Shift.From));
			Boxes.Add(Shift.From, Shift.Box);
		}
		// Restore covered labels after every box has been placed (adjacent targets overlap old cells).
		for (AHexTile* Tile : GetAllTiles()) Tile->SetLabelCovered(GetBoxAt(Tile->GetCoord()) != nullptr, false);
	}
	else if (Record.PushedBox)
	{
		Record.PushedBox->FinishMoveImmediately();
		MoveBoxOnBoard(Record.PushedBox, Record.BoxTo, Record.BoxFrom, EHexDirection::Invalid, nullptr, 0.f);
	}
	if (M)
	{
		M->TeleportToCoord(Record.PlayerFrom);
	}

	OnBoardReset.Broadcast();
	return true;
}

void UHexBoardSubsystem::RestoreInitialState()
{
	if (!bHasInitialState)
	{
		return;
	}

	UHexGridMovementComponent* M = Mover.Get();
	if (M)
	{
		M->FinishStepImmediately(false);
	}

	Boxes.Reset();
	for (int32 Index = 0; Index < InitialBoxes.Num(); ++Index)
	{
		AHexBox* Box = InitialBoxes[Index].Get();
		if (!Box)
		{
			continue;
		}
		const FIntPoint Coord = InitialBoxCoords[Index];
		AHexTile* Tile = GetTileAt(Coord);
		Box->FinishMoveImmediately();
		Box->SetOnGoal(Tile && Tile->IsGoal());
		Box->PlaceOnTile(Coord, Tile);
		Boxes.Add(Coord, Box);
	}

	if (M)
	{
		M->TeleportToCoord(InitialPlayerCoord);
	}

	History.Reset();
	ScoredMoveCount = 0;
	OnBoardReset.Broadcast();
}
