#include "HexLevelTools.h"
#include "HexSokobanStateSpace.h"
#include "HexSokobanSolver.h"
#include "HexSokobanSettings.h"
#include "HexSokobanTypes.h"
#include "HexGridLibrary.h"
#include "HexBoardSubsystem.h"
#include "HexCardSubsystem.h"
#include "HexGridMovementComponent.h"
#include "HexGameFlowSubsystem.h"
#include "HexTile.h"
#include "HexGoalTile.h"
#include "HexTerrainTile.h"
#include "HexSceneryComponent.h"
#include "HexBox.h"
#include "Engine/World.h"

void UHexLevelTools::SpawnLevel(UWorld* World, const FHexLevelDefinition& Definition, TArray<AActor*>* OutActors)
{
	if (!World)
	{
		return;
	}
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	UClass* BoxClass = Settings.BoxClass.IsNull() ? nullptr : Settings.BoxClass.LoadSynchronous();
	BoxClass = BoxClass ? BoxClass : AHexBox::StaticClass();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FHexTileDef& Tile : Definition.Tiles)
	{
		AActor* Actor = SpawnTileActor(World, Tile);
		if (Actor && OutActors)
		{
			OutActors->Add(Actor);
		}
	}
	for (const FIntPoint& Box : Definition.Boxes)
	{
		if (!Definition.FindTile(Box) || Definition.FindTile(Box)->IsScenery())
		{
			continue;
		}
		AActor* Actor = World->SpawnActor<AActor>(BoxClass, UHexGridLibrary::CoordToWorld(Box) + FVector(0.0, 0.0, 10.0), FRotator::ZeroRotator, Params);
		if (Actor && OutActors)
		{
			OutActors->Add(Actor);
		}
	}
}

AActor* UHexLevelTools::SpawnTileActor(UWorld* World, const FHexTileDef& Tile)
{
	if (!World)
	{
		return nullptr;
	}
	// Terrain follows the suit (grass / rock / road / river). A goal keeps its terrain and is flagged before
	// BeginPlay, so it also builds its goal glow.
	UClass* TileClass = UHexSokobanSettings::ResolveTileClass(Tile.bGoal && !Tile.IsScenery(), Tile.Suit);
	const FTransform Transform(FRotator::ZeroRotator, UHexGridLibrary::CoordToWorld(Tile.Coord));
	AActor* Actor = World->SpawnActorDeferred<AActor>(TileClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Actor)
	{
		return nullptr;
	}
	if (AHexTerrainTile* Terrain = Cast<AHexTerrainTile>(Actor))
	{
		Terrain->SetIsGoal(Tile.bGoal && !Tile.IsScenery());
	}
	if (AHexTile* Hex=Cast<AHexTile>(Actor)) Hex->bScenery=Tile.IsScenery();
	Actor->FinishSpawning(Transform);
	if (Tile.IsScenery()) if (AHexTile* Hex=Cast<AHexTile>(Actor))
	{
		Hex->ClearCardLabel();
		auto* Scenery=NewObject<UHexSceneryComponent>(Hex); Hex->AddInstanceComponent(Scenery); Scenery->RegisterComponent(); Scenery->Build(Hex,Tile);
	}
	return Actor;
}

void UHexLevelTools::MakeSolverInput(const FHexLevelDefinition& Definition, FHexSolverInput& OutInput)
{
	OutInput = FHexSolverInput();
	for (const FHexTileDef& Tile : Definition.Tiles)
	{
		if (Tile.IsScenery()) continue;
		OutInput.Tiles.Add(Tile.Coord);
		if (Tile.bGoal)
		{
			OutInput.Goals.Add(Tile.Coord);
		}
	}
	for (const FIntPoint& Box : Definition.Boxes)
	{
		if (Definition.FindTile(Box) && !Definition.FindTile(Box)->IsScenery())
		{
			OutInput.Boxes.AddUnique(Box);
		}
	}
	OutInput.Player = Definition.PlayerStart;
}

TMap<FIntPoint, FHexCard> UHexLevelTools::MakeLabelMap(const FHexLevelDefinition& Definition)
{
	TMap<FIntPoint, FHexCard> Labels;
	for (const FHexTileDef& Tile : Definition.Tiles)
	{
		if (Tile.HasFace())
		{
			Labels.Add(Tile.Coord, Tile.GetFace());
		}
	}
	return Labels;
}

FHexLevelCheck UHexLevelTools::CheckLevel(const FHexLevelDefinition& Definition, int32 MaxStates, int32 MaxNodes)
{
	FHexLevelCheck Check;
	for (const FHexTileDef& Tile : Definition.Tiles)
	{
		Check.FacelessTiles += !Tile.IsScenery() && !Tile.HasFace() ? 1 : 0;
	}
	Check.bPlayable = Definition.IsPlayable(&Check.Problem);
	if (!Check.bPlayable)
	{
		return Check;
	}
	if (Definition.NeedsPerformanceMode()) { Check.bBudgetHit = true; return Check; }
	FHexSolverInput Input;
	MakeSolverInput(Definition, Input);
	FHexStateSpace Space;
	if (!Space.Build(Input, MaxStates))
	{
		Check.bBudgetHit = true;
		return Check;
	}
	Check.GeometricSteps = Space.GetStartCost();
	if (Check.GeometricSteps < 0)
	{
		return Check;
	}
	const FHexDeckSolveResult Solve = FHexDeckSolver::Solve(Space, Space.GetStartState(), MakeLabelMap(Definition), Definition.Deck, MaxNodes);
	Check.DeckTurns = Solve.bSolved ? Solve.Turns : -1;
	Check.bBudgetHit = Solve.bBudgetHit;
	return Check;
}

bool UHexLevelTools::MakeRecommendedDeck(const FHexLevelDefinition& Definition, TArray<FHexCard>& OutDeck)
{
	OutDeck.Reset();
	if (Definition.NeedsPerformanceMode()) return false;
	if (!Definition.IsPlayable())
	{
		return false;
	}
	FHexSolverInput Input;
	MakeSolverInput(Definition, Input);
	FHexStateSpace Space;
	TArray<FIntPoint> Steps;
	if (!Space.Build(Input, UHexSokobanSettings::Get().SolverMaxStates) || !Space.GetOptimalSteps(Space.GetStartState(), Steps))
	{
		return false;
	}
	for (const FIntPoint& Step : Steps)
	{
		if (const FHexTileDef* Tile = Definition.FindTile(Step))
		{
			if (Tile->HasFace())
			{
				OutDeck.Add(Tile->GetFace());
			}
		}
	}
	return true;
}

FString UHexLevelTools::ExportWorldAsBuiltInLevel(const UObject* WorldContext, const FString& DisplayName)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UHexBoardSubsystem* Board = World ? World->GetSubsystem<UHexBoardSubsystem>() : nullptr;
	const UHexCardSubsystem* Cards = World ? World->GetSubsystem<UHexCardSubsystem>() : nullptr;
	UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(WorldContext);
	const UHexGridMovementComponent* Mover = Board ? Board->GetPlayerMover() : nullptr;
	if (!Board || !Flow || !Mover)
	{
		return FString();
	}
	FHexLevelDefinition Definition;
	Definition.DisplayName = DisplayName;
	Definition.Author = TEXT("Built-in");
	for (const AHexTile* Tile : Board->GetAllTiles())
	{
		FHexTileDef& Def = Definition.Tiles.AddDefaulted_GetRef();
		Def.Coord = Tile->GetCoord();
		Def.bGoal = Tile->IsGoal();
		FHexCard Face;
		if (Cards && Cards->GetTileLabel(Def.Coord, Face))
		{
			Def.Suit = Face.Suit;
			Def.Rank = Face.Rank;
		}
		else
		{
			Def.Rank = 0;
		}
	}
	Definition.Tiles.Sort([](const FHexTileDef& A, const FHexTileDef& B)
	{
		return A.Coord.Y != B.Coord.Y ? A.Coord.Y < B.Coord.Y : A.Coord.X < B.Coord.X;
	});
	for (const AHexBox* Box : Board->GetAllBoxes())
	{
		Definition.Boxes.Add(Box->GetCoord());
	}
	Definition.PlayerStart = Mover->GetCoord();
	Definition.bHasPlayerStart = true;
	MakeRecommendedDeck(Definition, Definition.Deck);
	const FHexLevelCheck Check = CheckLevel(Definition, UHexSokobanSettings::Get().SolverMaxStates, UHexSokobanSettings::Get().DeckSolverMaxNodes);
	Definition.CachedOptimalTurns = Check.DeckTurns;
	const FString File = Flow->SaveBuiltInLevel(Definition);
	UE_LOG(LogHexSokoban, Log, TEXT("Exported built-in level '%s' (%d tiles, %d cards, optimal %d turns) -> %s"),
		*DisplayName, Definition.Tiles.Num(), Definition.Deck.Num(), Check.DeckTurns, *File);
	return File;
}
