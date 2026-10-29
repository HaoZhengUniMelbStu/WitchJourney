#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HexLevelTypes.h"
#include "HexLevelTools.h"
#include "HexSokobanSolver.h"
#include "HexBoardSubsystem.h"
#include "HexGridLibrary.h"
#include "HexTile.h"
#include "HexSceneryComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "JsonObjectConverter.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexSceneryTest,"HexSokoban.Scenery.Isolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHexSceneryTest::RunTest(const FString& Parameters)
{
	FHexLevelDefinition Def; Def.bHasPlayerStart=true; Def.PlayerStart={0,0}; Def.Boxes={{1,0}};
	for(int32 I=0;I<4;++I){FHexTileDef T;T.Coord={I,0};T.Rank=7;T.bGoal=I==2;Def.Tiles.Add(T);}
	TestTrue(TEXT("Unmodified ordinary level playable"),Def.IsPlayable());
	FHexSolverInput Input; UHexLevelTools::MakeSolverInput(Def,Input); TestEqual(TEXT("All original tiles retained"),Input.Tiles.Num(),4);
	Def.Tiles[3].SceneryVariant=2;
	TestFalse(TEXT("Scenery has no face"),Def.Tiles[3].HasFace());
	UHexLevelTools::MakeSolverInput(Def,Input);TestEqual(TEXT("Only scenery removed from solver"),Input.Tiles.Num(),3);
	TestFalse(TEXT("Scenery excluded from labels"),UHexLevelTools::MakeLabelMap(Def).Contains({3,0}));
	FHexLevelDefinition Decorated=Def;
	for(int32 I=0;I<100;++I) {FHexTileDef T;T.Coord={I,20};T.SceneryVariant=1;Decorated.Tiles.Add(T);}
	TestFalse(TEXT("Adding scenery does not change small-map feeding or solver mode"),Decorated.NeedsPerformanceMode());
	Def.Tiles[3].SceneryRotationSteps=4;
	FString Json;FJsonObjectConverter::UStructToJsonObjectString(Def,Json);FHexLevelDefinition Loaded;
	FJsonObjectConverter::JsonObjectStringToUStruct(Json,&Loaded);
	TestEqual(TEXT("Variant saved"),Loaded.Tiles[3].SceneryVariant,2);
	TestEqual(TEXT("Scenery orientation survives save/load"),Loaded.Tiles[3].SceneryRotationSteps,4);
	Loaded.Tiles[3].SceneryVariant=0;
	TestTrue(TEXT("Removal restores original face"),Loaded.Tiles[3].HasFace() && Loaded.Tiles[3].Rank==7);
	Def.Tiles[0].SceneryVariant=1; TestFalse(TEXT("Start cannot occupy scenery"),Def.IsPlayable()); Def.Tiles[0].SceneryVariant=0;
	Def.Tiles[1].SceneryVariant=1; TestFalse(TEXT("Box cannot occupy scenery"),Def.IsPlayable());
	UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);
	World->SpawnActor<AHexTile>(FVector::ZeroVector,FRotator::ZeroRotator);
	for(int32 Suit=0;Suit<4;++Suit) for(int32 Variant=1;Variant<=(static_cast<EHexSuit>(Suit)==EHexSuit::Club?6:(static_cast<EHexSuit>(Suit)==EHexSuit::Heart?3:2));++Variant)
	{
		FHexTileDef D;D.Coord={Suit*10+Variant,0};D.Suit=static_cast<EHexSuit>(Suit);D.SceneryVariant=Variant;
		auto* Tile=Cast<AHexTile>(UHexLevelTools::SpawnTileActor(World,D));
		TestNotNull(TEXT("Scenery base created"),Tile);
		if(!Tile) continue;
		Tile->SetCardLabel(D.GetFace());TestFalse(TEXT("Scenery cannot redisplay face"),Tile->HasCardLabel());
		TArray<UInstancedStaticMeshComponent*> Meshes;Tile->GetComponents(Meshes);
		for(auto* M:Meshes) if(M->ComponentHasTag(TEXT("SceneryArt")))
			for(int32 I=0;I<M->GetInstanceCount();++I)
			{
				FTransform T;M->GetInstanceTransform(I,T);const FVector S=T.GetScale3D();
				TestTrue(TEXT("Art meshes retain uniform XYZ proportions"),FMath::IsNearlyEqual(S.X,S.Y) && FMath::IsNearlyEqual(S.X,S.Z));
			}
		int32 Cubes=0;for(auto* M:Meshes) Cubes+=M->GetInstanceCount();
		TArray<UStaticMeshComponent*> Art;Tile->GetComponents(Art);
		for(auto* M:Art) if(M!=Tile->GetTileMesh() && !Cast<UInstancedStaticMeshComponent>(M) && M->GetStaticMesh()) ++Cubes;
		TestTrue(TEXT("Each variant builds geometry"),Cubes>0);
		if(D.CanRotateScenery())
		{
			auto* SceneryComponent=Tile->FindComponentByClass<UHexSceneryComponent>();
			UStaticMeshComponent* OriginalModel=SceneryComponent->RotatingModel;
			TestNotNull(TEXT("Rotatable scenery has a primary model"),OriginalModel);
			if(D.Suit==EHexSuit::Club && OriginalModel)
			{
				const int32 ModelIndex=Variant<=2?Variant:Variant+1;
				TestEqual(TEXT("Town variant uses its assigned building"),OriginalModel->GetStaticMesh()->GetName(),FString::Printf(TEXT("SM_StylizedBuilding_Vol%02d"),ModelIndex));
			}
			const float OriginalHeight=OriginalModel?OriginalModel->GetRelativeLocation().Z:0;
			Tile->AddActorWorldOffset(FVector(0,0,65)); // editor hover/placement lift
			const FTransform BaseTransform=Tile->GetActorTransform();
			for(int32 Step=0;Step<6;++Step)
			{
				Tile->FindComponentByClass<UHexSceneryComponent>()->SetRotationSteps(Step);
				TestTrue(TEXT("Scenery rotation never rotates the tile"),Tile->GetActorTransform().Equals(BaseTransform));
				TestTrue(TEXT("Rotation retains the existing model component"),SceneryComponent->RotatingModel==OriginalModel);
				if(OriginalModel) TestTrue(TEXT("Rotation on raised tile preserves relative height"),FMath::IsNearlyEqual(OriginalModel->GetRelativeLocation().Z,static_cast<double>(OriginalHeight),.001));
				TArray<UStaticMeshComponent*> Parts;Tile->GetComponents(Parts);
				for(auto* Part:Parts) if(Part->ComponentHasTag(TEXT("SceneryArt")) && Part->GetStaticMesh() && Part->GetStaticMesh()->GetName()==TEXT("SM_StylizedShip"))
				{
					const FBox B=Part->GetStaticMesh()->GetBoundingBox();
					const FVector Center=Tile->GetSurfaceLocation();
					for(int32 Corner=0;Corner<8;++Corner)
					{
						const FVector P=Part->GetComponentTransform().TransformPosition(FVector(Corner&1?B.Max.X:B.Min.X,Corner&2?B.Max.Y:B.Min.Y,Corner&4?B.Max.Z:B.Min.Z))-Center;
						TestTrue(TEXT("Ship bounds remain inside tile at every 60-degree angle"),P.Size2D()<=UHexGridLibrary::GetTileSpacing()*.5);
					}
				}
			}
		}
	}
	auto* Board=World->GetSubsystem<UHexBoardSubsystem>();Board->RebuildBoard();
	TestEqual(TEXT("Runtime board contains only original ordinary tile"),Board->GetAllTiles().Num(),1);
	World->SpawnActor<AHexTile>(UHexGridLibrary::CoordToWorld({10,0}),FRotator::ZeroRotator);
	Board->RebuildBoard();
	FHexMoveRecord Move;
	for(int32 Rank=11;Rank<=13;++Rank)
	{
		FHexCard Skill(Rank,EHexSuit::Spade);Skill.Kind=EHexCardKind::Skill;
		TestFalse(TEXT("No skill can target decorative terrain"),Board->EvaluateSkill({0,0},{1,0},Skill,Move));
	}
	FHexCard Flight(13,EHexSuit::Spade);Flight.Kind=EHexCardKind::Skill;
	TestFalse(TEXT("Flight cannot cross scenery holes"),Board->EvaluateSkill({0,0},{10,0},Flight,Move));
	FHexTileDef Mountain;Mountain.Coord={20,0};Mountain.Suit=EHexSuit::Spade;Mountain.SceneryVariant=1;
	auto* First=UHexLevelTools::SpawnTileActor(World,Mountain);
	auto* Scenery=First->FindComponentByClass<UHexSceneryComponent>();
	TestEqual(TEXT("Isolated ridge has no connections"),Scenery->NeighborMask,uint8(0));
	Mountain.Coord={21,0};Mountain.SceneryVariant=2;auto* Second=UHexLevelTools::SpawnTileActor(World,Mountain);
	TestTrue(TEXT("Adding a cave refreshes the already placed ridge"),Scenery->NeighborMask!=0);
	TestTrue(TEXT("Cave connects back to the neighbouring mountain"),Second->FindComponentByClass<UHexSceneryComponent>()->NeighborMask!=0);
	TArray<UInstancedStaticMeshComponent*> CaveMeshes;Second->GetComponents(CaveMeshes);
	for(auto* M:CaveMeshes) TestFalse(TEXT("Cave uses authored rocks instead of block pillars"),M->ComponentHasTag(TEXT("SceneryBlock")));
	// Start the component lifecycle before exercising the removal callback.
	auto* RemovedScenery=Second->FindComponentByClass<UHexSceneryComponent>();
	RemovedScenery->BeginPlay();
	RemovedScenery->EndPlay(EEndPlayReason::Destroyed);
	TestEqual(TEXT("Removing a neighbour restores the outer slope"),Scenery->NeighborMask,uint8(0));
	World->DestroyWorld(false);return true;
}
#endif
