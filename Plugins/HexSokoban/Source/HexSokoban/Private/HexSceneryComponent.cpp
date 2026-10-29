#include "HexSceneryComponent.h"
#include "HexTile.h"
#include "HexGridLibrary.h"
#include "HexSokobanSettings.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

UHexSceneryComponent::UHexSceneryComponent()
{
	PrimaryComponentTick.bCanEverTick=true;
	PrimaryComponentTick.bStartWithTickEnabled=false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Cube=Mesh.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/HexSokoban/Materials/M_HexSceneryBlocks.M_HexSceneryBlocks"));
	BlockMaterial=Material.Object;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Tree(TEXT("/Game/HexSokoban/Environment/Foliage/Meshes/SM_StylizedTree_Vol13p.SM_StylizedTree_Vol13p"));
	ForestTree=Tree.Object;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> NormalTree1(TEXT("/Game/HexSokoban/Environment/Foliage/Meshes/SM_StylizedTree_Vol01.SM_StylizedTree_Vol01"));
	if(NormalTree1.Object) NormalTrees.Add(NormalTree1.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> NormalTree2(TEXT("/Game/HexSokoban/Environment/Foliage/Meshes/SM_StylizedTree_Vol02.SM_StylizedTree_Vol02"));
	if(NormalTree2.Object) NormalTrees.Add(NormalTree2.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> NormalTree3(TEXT("/Game/HexSokoban/Environment/Foliage/Meshes/SM_StylizedTree_Vol03.SM_StylizedTree_Vol03"));
	if(NormalTree3.Object) NormalTrees.Add(NormalTree3.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> NormalTree4(TEXT("/Game/HexSokoban/Environment/Foliage/Meshes/SM_StylizedTree_Vol04.SM_StylizedTree_Vol04"));
	if(NormalTree4.Object) NormalTrees.Add(NormalTree4.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> NormalTree5(TEXT("/Game/HexSokoban/Environment/Foliage/Meshes/SM_StylizedTree_Vol05.SM_StylizedTree_Vol05"));
	if(NormalTree5.Object) NormalTrees.Add(NormalTree5.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> NormalTree6(TEXT("/Game/HexSokoban/Environment/Foliage/Meshes/SM_StylizedTree_Vol06.SM_StylizedTree_Vol06"));
	if(NormalTree6.Object) NormalTrees.Add(NormalTree6.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> NormalTree7(TEXT("/Game/HexSokoban/Environment/Foliage/Meshes/SM_StylizedTree_Vol07.SM_StylizedTree_Vol07"));
	if(NormalTree7.Object) NormalTrees.Add(NormalTree7.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> NormalTree8(TEXT("/Game/HexSokoban/Environment/Foliage/Meshes/SM_StylizedTree_Vol08.SM_StylizedTree_Vol08"));
	if(NormalTree8.Object) NormalTrees.Add(NormalTree8.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> NormalTree9(TEXT("/Game/HexSokoban/Environment/Foliage/Meshes/SM_StylizedTree_Vol09.SM_StylizedTree_Vol09"));
	if(NormalTree9.Object) NormalTrees.Add(NormalTree9.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> NormalTree10(TEXT("/Game/HexSokoban/Environment/Foliage/Meshes/SM_StylizedTree_Vol10.SM_StylizedTree_Vol10"));
	if(NormalTree10.Object) NormalTrees.Add(NormalTree10.Object);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Ship(TEXT("/Game/HexSokoban/Environment/Ships/Meshes/SM_StylizedShip.SM_StylizedShip"));
	ShipMesh=Ship.Object;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Castle(TEXT("/Game/HexSokoban/Environment/Building/Meshes/SM_StylizedBuilding_Vol01.SM_StylizedBuilding_Vol01"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> City(TEXT("/Game/HexSokoban/Environment/Building/Meshes/SM_StylizedBuilding_Vol02.SM_StylizedBuilding_Vol02"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cottage(TEXT("/Game/HexSokoban/Environment/Building/Meshes/SM_StylizedBuilding_Vol03.SM_StylizedBuilding_Vol03"));
	CastleMesh=Castle.Object; CityMesh=City.Object; CottageMesh=Cottage.Object;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Town04(TEXT("/Game/HexSokoban/Environment/Building/Meshes/SM_StylizedBuilding_Vol04.SM_StylizedBuilding_Vol04"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Town05(TEXT("/Game/HexSokoban/Environment/Building/Meshes/SM_StylizedBuilding_Vol05.SM_StylizedBuilding_Vol05"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Town06(TEXT("/Game/HexSokoban/Environment/Building/Meshes/SM_StylizedBuilding_Vol06.SM_StylizedBuilding_Vol06"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Town07(TEXT("/Game/HexSokoban/Environment/Building/Meshes/SM_StylizedBuilding_Vol07.SM_StylizedBuilding_Vol07"));
	AdditionalTownMeshes={Town04.Object,Town05.Object,Town06.Object,Town07.Object};
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock0(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedBigStone_Vol01.SM_StylizedBigStone_Vol01"));
	if(Rock0.Object) Rocks.Add(Rock0.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock1(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedBigStone_Vol02.SM_StylizedBigStone_Vol02"));
	if(Rock1.Object) Rocks.Add(Rock1.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock2(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedLargeStone_Vol01.SM_StylizedLargeStone_Vol01"));
	if(Rock2.Object) Rocks.Add(Rock2.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock3(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedLargeStone_Vol02.SM_StylizedLargeStone_Vol02"));
	if(Rock3.Object) Rocks.Add(Rock3.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock4(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedLargeStone_Vol03.SM_StylizedLargeStone_Vol03"));
	if(Rock4.Object) Rocks.Add(Rock4.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock5(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedLargeStone_Vol04.SM_StylizedLargeStone_Vol04"));
	if(Rock5.Object) Rocks.Add(Rock5.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock6(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedLargeStone_Vol05.SM_StylizedLargeStone_Vol05"));
	if(Rock6.Object) Rocks.Add(Rock6.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock7(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedLargeStone_Vol06.SM_StylizedLargeStone_Vol06"));
	if(Rock7.Object) Rocks.Add(Rock7.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock8(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedLargeStone_Vol07.SM_StylizedLargeStone_Vol07"));
	if(Rock8.Object) Rocks.Add(Rock8.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock9(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedLargeStone_Vol08.SM_StylizedLargeStone_Vol08"));
	if(Rock9.Object) Rocks.Add(Rock9.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock10(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedMediumStone_Vol01.SM_StylizedMediumStone_Vol01"));
	if(Rock10.Object) Rocks.Add(Rock10.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock11(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedMediumStone_Vol02.SM_StylizedMediumStone_Vol02"));
	if(Rock11.Object) Rocks.Add(Rock11.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock12(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedSmallStone_Vol01.SM_StylizedSmallStone_Vol01"));
	if(Rock12.Object) Rocks.Add(Rock12.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock13(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedSmallStone_Vol02.SM_StylizedSmallStone_Vol02"));
	if(Rock13.Object) Rocks.Add(Rock13.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock14(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedSmallStone_Vol03.SM_StylizedSmallStone_Vol03"));
	if(Rock14.Object) Rocks.Add(Rock14.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock15(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedTinyStone_Vol01.SM_StylizedTinyStone_Vol01"));
	if(Rock15.Object) Rocks.Add(Rock15.Object);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rock16(TEXT("/Game/HexSokoban/Environment/Rocks/Meshes/SM_StylizedTinyStone_Vol02.SM_StylizedTinyStone_Vol02"));
	if(Rock16.Object) Rocks.Add(Rock16.Object);

}
void UHexSceneryWorldSubsystem::RefreshAround(FIntPoint Cell)
{
	if(auto* C=Cells.FindRef(Cell).Get()) C->RefreshConnections();
	for(int32 D=0;D<6;++D) if(auto* C=Cells.FindRef(UHexGridLibrary::GetNeighbor(Cell,static_cast<EHexDirection>(D))).Get()) C->RefreshConnections();
}
void UHexSceneryComponent::Build(AHexTile* Tile,const FHexTileDef& Def)
{
	Definition=Def;
	auto* Registry=GetWorld()->GetSubsystem<UHexSceneryWorldSubsystem>();
	Registry->Cells.Add(Def.Coord,this);
	Registry->RefreshAround(Def.Coord);
}
void UHexSceneryComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if(auto* Registry=GetWorld()->GetSubsystem<UHexSceneryWorldSubsystem>())
	{
		if(Registry->Cells.FindRef(Definition.Coord).Get()==this) Registry->Cells.Remove(Definition.Coord);
		if(!GetWorld()->bIsTearingDown) Registry->RefreshAround(Definition.Coord);
	}
	Super::EndPlay(Reason);
}
void UHexSceneryComponent::RefreshConnections()
{
	uint8 Mask=0;
	auto* Registry=GetWorld()->GetSubsystem<UHexSceneryWorldSubsystem>();
	if(IsMountain()) for(int32 D=0;D<6;++D)
		if(auto* C=Registry->Cells.FindRef(UHexGridLibrary::GetNeighbor(Definition.Coord,static_cast<EHexDirection>(D))).Get())
			if(C->IsMountain()) Mask|=1<<D;
	if(Mask==NeighborMask) return;
	NeighborMask=Mask;
	auto* Tile=Cast<AHexTile>(GetOwner());if(!Tile) return;
	TArray<UStaticMeshComponent*> Components;Tile->GetComponents(Components);
	for(auto* C:Components) if(C->ComponentHasTag(TEXT("SceneryArt")) || C->ComponentHasTag(TEXT("SceneryBlock"))) C->DestroyComponent();
	FloatingShip=nullptr;RotatingModel=nullptr;SetComponentTickEnabled(false);
	RebuildVisuals(Tile,Definition);
}

void UHexSceneryComponent::SetRotationSteps(int32 Steps)
{
	if(!Definition.CanRotateScenery()) return;
	const int32 Normalized=(Steps%6+6)%6;
	const double Delta=(Normalized-Definition.SceneryRotationSteps)*60.0;
	Definition.SceneryRotationSteps=Normalized;
	if(!RotatingModel) return;
	const FRotator Turn(0,Delta,0);
	const FVector Position=RotatingModel->GetRelativeLocation();
	FRotator Rotation=RotatingModel->GetRelativeRotation();Rotation.Yaw+=Delta;
	RotatingModel->SetRelativeLocation(ModelPivot+Turn.RotateVector(Position-ModelPivot));
	RotatingModel->SetRelativeRotation(Rotation);
	if(FloatingShip==RotatingModel)
	{
		ShipRestPosition=ModelPivot+Turn.RotateVector(ShipRestPosition-ModelPivot);
		ShipRestRotation.Yaw+=Delta;
	}
}
void UHexSceneryComponent::RebuildVisuals(AHexTile* Tile,const FHexTileDef& Def)
{
	if(!Tile || !Cube || !Def.IsScenery()) return;
	const double W=UHexGridLibrary::GetTileSpacing();
	const double H=FMath::Clamp(Tile->GetTileMesh()->GetStaticMesh()->GetBoundingBox().GetExtent().Z*Tile->GetTileMesh()->GetRelativeScale3D().Z*8.0,W*.45,W*1.1);
	const FVector Surface=Tile->GetSurfaceLocation();
	ModelPivot=Tile->GetActorTransform().InverseTransformPosition(Surface);
	const FLinearColor Colors[]={FLinearColor(.18,.065,.025),FLinearColor(.07,.28,.11),FLinearColor(.28,.34,.40),FLinearColor(.65,.55,.38),FLinearColor(.72,.15,.07),FLinearColor(.1,.22,.32),FLinearColor(.95,.72,.19)};
	TArray<UInstancedStaticMeshComponent*> Groups; Groups.SetNumZeroed(UE_ARRAY_COUNT(Colors));
	auto Block=[&](double X,double Y,double Z,double SX,double SY,double SZ,int32 Color,FRotator Rotation=FRotator(0,0,0))
	{
		if(!Groups[Color])
		{
			auto* ISM=NewObject<UInstancedStaticMeshComponent>(Tile);
			Tile->AddInstanceComponent(ISM); ISM->SetupAttachment(Tile->GetRootComponent());
			ISM->ComponentTags.Add(TEXT("SceneryBlock"));
			ISM->SetStaticMesh(Cube); ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision); ISM->SetCanEverAffectNavigation(false);
			if(auto* Material=BlockMaterial.Get())
			{
				auto* MID=UMaterialInstanceDynamic::Create(Material,Tile); MID->SetVectorParameterValue(TEXT("Color"),Colors[Color]); ISM->SetMaterial(0,MID);
			}
			ISM->RegisterComponent(); Groups[Color]=ISM;
		}
		const FVector Local=Tile->GetActorTransform().InverseTransformPosition(Surface+FVector(X*W,Y*W,Z*H));
		Groups[Color]->AddInstance(FTransform(Rotation,Local,FVector(SX*W,SY*W,SZ*H)/100.0));
	};
	FRandomStream Rng(static_cast<int32>(HashCombine(GetTypeHash(Def.Coord),GetTypeHash(static_cast<uint8>(Def.Suit))) ^ 0x5172A3u));
	// Keep the authored materials. Decorations have no collision or navigation influence.
	auto Art=[&](UStaticMesh* Mesh,FVector Position,FVector Scale,FRotator Rotation,bool bFloat=false)
	{
		if(!Mesh) return;
		const FBox B=Mesh->GetBoundingBox();
		const FVector Center=Rotation.RotateVector(FVector(B.GetCenter().X,B.GetCenter().Y,0)*Scale);
		FVector Local=Tile->GetActorTransform().InverseTransformPosition(Surface)+Position-Center;
		if(bFloat || Def.Suit==EHexSuit::Diamond)
		{
			auto* C=NewObject<UStaticMeshComponent>(Tile);Tile->AddInstanceComponent(C);C->SetupAttachment(Tile->GetRootComponent());
			C->ComponentTags.Add(TEXT("SceneryArt"));C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(Mesh);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);
			C->SetRelativeTransform(FTransform(Rotation,Local,Scale));C->RegisterComponent();
			if(Mesh==ShipMesh) RotatingModel=C;
			if(bFloat){FloatingShip=C;ShipRestPosition=Local;ShipRestRotation=Rotation;FloatTime=Rng.FRandRange(0,20);SetComponentTickEnabled(true);}
		}
		else
		{
			UInstancedStaticMeshComponent* C=nullptr;
			TArray<UInstancedStaticMeshComponent*> Existing;Tile->GetComponents(Existing);
			for(auto* E:Existing) if(E->ComponentHasTag(TEXT("SceneryArt")) && E->GetStaticMesh()==Mesh){C=E;break;}
			if(!C){C=NewObject<UInstancedStaticMeshComponent>(Tile);Tile->AddInstanceComponent(C);C->SetupAttachment(Tile->GetRootComponent());C->ComponentTags.Add(TEXT("SceneryArt"));C->SetStaticMesh(Mesh);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);C->RegisterComponent();}
			C->AddInstance(FTransform(Rotation,Local,Scale));
		}
	};
	const bool Alternate=Def.SceneryVariant==2;
	const double SceneryYaw=Def.SceneryRotationSteps*60.0;
	UStaticMesh* Building=Def.Suit==EHexSuit::Club ? (Alternate?CityMesh.Get():CastleMesh.Get())
		: Def.Suit==EHexSuit::Heart && Alternate ? CottageMesh.Get() : nullptr;
	if(Def.Suit==EHexSuit::Club && AdditionalTownMeshes.IsValidIndex(Def.SceneryVariant-3))
		Building=AdditionalTownMeshes[Def.SceneryVariant-3].Get();
	if(Building)
	{
		// These assets have authored ground-centred pivots and orientation. Do not
		// recenter their bounds, randomize rotation, or normalize their dimensions.
		auto* C=NewObject<UStaticMeshComponent>(Tile);
		Tile->AddInstanceComponent(C);C->SetupAttachment(Tile->GetRootComponent());
		C->ComponentTags.Add(TEXT("SceneryArt"));
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Building);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetRelativeTransform(FTransform(FRotator(0,SceneryYaw,0),Tile->GetActorTransform().InverseTransformPosition(Surface),FVector::OneVector));
		C->RegisterComponent();
		RotatingModel=C;
		return;
	}
	if(Def.Suit==EHexSuit::Heart && !Alternate && ForestTree)
	{
		// One jittered world-space lattice, shared by every forest cell: no per-tile clump.
		const double Step=W*(Def.SceneryVariant==3?.33:.29);
		for(int32 GX=FMath::FloorToInt((Surface.X-W*.65)/Step);GX<=FMath::CeilToInt((Surface.X+W*.65)/Step);++GX)
		for(int32 GY=FMath::FloorToInt((Surface.Y-W*.65)/Step);GY<=FMath::CeilToInt((Surface.Y+W*.65)/Step);++GY)
		{
			FRandomStream CellRandom(static_cast<int32>(GetTypeHash(FIntPoint(GX,GY)) ^ 0x1A7B91u));
			const FVector Point((GX+.5*(GY&1))*Step+CellRandom.FRandRange(-Step*.18,Step*.18),GY*Step+CellRandom.FRandRange(-Step*.18,Step*.18),Surface.Z);
			if(UHexGridLibrary::WorldToCoord(Point)!=Def.Coord) continue;
			UStaticMesh* Tree=Def.SceneryVariant==3 && !NormalTrees.IsEmpty()?NormalTrees[CellRandom.RandRange(0,NormalTrees.Num()-1)].Get():ForestTree.Get();
			const FVector TreeSize=Tree->GetBoundingBox().GetSize();
			const double Scale=Def.SceneryVariant==3?FMath::Min(210./TreeSize.Z,125./FMath::Max(TreeSize.X,TreeSize.Y))*CellRandom.FRandRange(.95,1.05):CellRandom.FRandRange(.075,.085);
			Art(Tree,FVector(Point.X-Surface.X,Point.Y-Surface.Y,-Tree->GetBoundingBox().Min.Z*Scale-4),FVector(Scale),FRotator(0,CellRandom.FRandRange(0,360),0));
		}
		return;
	}
	if(Def.Suit==EHexSuit::Spade && !Rocks.IsEmpty())
	{
		// Fixed scale preserves texture density. Shape comes only from placement and burial.
		TArray<UStaticMesh*> Mountain;UStaticMesh* Foundation=nullptr;
		for(auto M:Rocks)
		{
			const FString Name=M->GetName();
			if(Name==TEXT("SM_StylizedLargeStone_Vol08")) Foundation=M.Get();
			else if(Name.Contains(TEXT("BigStone")) || Name.Contains(TEXT("LargeStone"))) Mountain.Add(M.Get());
		}
		// The cave faces -X, toward the gameplay camera. Reserve actual empty space,
		// including the full rotated bounds of each rock, rather than hiding it with a dark decal.
		const double CaveHeight=H*.48, CaveHalfWidth=W*.14;
		auto PlaceRock=[&](UStaticMesh* M,FVector Position,double Scale,FRotator Rotation)
		{
			if(Alternate)
			{
				const FBox Bounds=M->GetBoundingBox();
				const FVector Center=Rotation.RotateVector(FVector(Bounds.GetCenter().X,Bounds.GetCenter().Y,0)*Scale);
				const FBox Placed=Bounds.TransformBy(FTransform(Rotation,Position-Center,FVector(Scale)));
				if(Placed.Min.X<W*.12 && Placed.Max.X>-W*.65 &&
					Placed.Min.Y<CaveHalfWidth && Placed.Max.Y>-CaveHalfWidth &&
					Placed.Min.Z<CaveHeight && Placed.Max.Z>0) return;
			}
			Art(M,Position,FVector(Scale),Rotation);
		};
		if(Foundation)
		{
			// Keep the cave floor flush with the terrain; its broad rock also forms the lintel.
			const FBox B=Foundation->GetBoundingBox();
			const double Z=Alternate?-B.Max.Z*.14:-B.Min.Z*.14-8;
			Art(Foundation,FVector(0,0,Z),FVector(.14),FRotator::ZeroRotator);
			if(Alternate) PlaceRock(Foundation,FVector(-W*.08,0,CaveHeight+1-B.Min.Z*.14),.14,FRotator::ZeroRotator);
		}
		const double Step=W*.27;
		for(int32 GX=-2;GX<=2;++GX) for(int32 GY=-2;GY<=2;++GY)
		{
			const double X=GX*Step,Y=GY*Step;
			if(FMath::Abs(X)>W*.43 || FMath::Abs(Y)>W*.49-FMath::Abs(X)*.57735) continue;
			FRandomStream CellRandom(static_cast<int32>(HashCombine(GetTypeHash(Def.Coord),GetTypeHash(FIntPoint(GX,GY))) ^ 0x71D42u));
			const FVector2D P(X,Y);double Distance=P.Size();
			// Raise the ridge to the shared edge only where a neighbouring mountain exists.
			for(int32 D=0;D<6;++D) if(NeighborMask&(1<<D))
			{
				const FVector Delta=UHexGridLibrary::CoordToWorld(UHexGridLibrary::GetNeighbor(Def.Coord,static_cast<EHexDirection>(D)))-UHexGridLibrary::CoordToWorld(Def.Coord);
				const FVector2D End(Delta.X*.5,Delta.Y*.5);
				const double T=FMath::Clamp(FVector2D::DotProduct(P,End)/End.SizeSquared(),0.,1.);
				Distance=FMath::Min(Distance,(P-End*T).Size());
			}
			const double Height=H*(.36+.54*FMath::Clamp(1.-Distance/(W*.55),0.,1.));
			UStaticMesh* M=Mountain[CellRandom.RandRange(0,Mountain.Num()-1)];const FBox B=M->GetBoundingBox();
			const double Scale=CellRandom.FRandRange(.135,.145),RockHeight=B.GetSize().Z*Scale;
			// Overlap large pieces; no tiny stones enlarged into boulders and no pebbles on top.
			const double Base=Height-RockHeight;
			if(Base>20) PlaceRock(M,FVector(X,Y,-B.Min.Z*Scale-10),Scale,FRotator(0,CellRandom.FRandRange(0,360),0));
			PlaceRock(M,FVector(X+CellRandom.FRandRange(-4,4),Y+CellRandom.FRandRange(-4,4),Base-B.Min.Z*Scale),Scale,FRotator(0,CellRandom.FRandRange(0,360),0));
		}
		if(Alternate)
		{
			// Upright authored stones support the roof on both sides of the mouth.
			for(auto M:Mountain) if(M->GetName()==TEXT("SM_StylizedLargeStone_Vol02"))
				for(int32 Side:{-1,1})
					PlaceRock(M,FVector(-W*.22,Side*W*.35,-M->GetBoundingBox().Min.Z*.14-4),.14,FRotator::ZeroRotator);
		}

		return;
	}
	if(Def.Suit==EHexSuit::Diamond && ShipMesh)
	{
		// The inverted mast is deliberately submerged; only the overturned hull remains above water.
		// A fixed circular footprint fits within the hexagon at every orientation.
		// Reserve clearance for the upright ship's gentle rocking too.
		const FVector Extent=ShipMesh->GetBoundingBox().GetExtent();
		const double Radius=FMath::Sqrt(Extent.X*Extent.X+Extent.Y*Extent.Y)+Extent.Z*.04;
		const double ShipScale=FMath::Min(.25,W*.46/FMath::Max(1.,Radius));
		Art(ShipMesh,FVector(0,0,Alternate?4:10),FVector(ShipScale),FRotator(0,25+SceneryYaw,Alternate?180:0),!Alternate);
		if(Alternate && !Rocks.IsEmpty()) for(int32 I=0;I<5;++I)
		{
			UStaticMesh* M=Rocks[Rng.RandRange(10,Rocks.Num()-1)];const FBox B=M->GetBoundingBox();const FVector Size=B.GetSize();
			const double S=Rng.FRandRange(.135,.145),A=I*2*PI/5+.2;
			Art(M,FVector(cos(A)*W*.40,sin(A)*W*.40,-B.Min.Z*S-4),FVector(S),FRotator(0,Rng.FRandRange(0,360),0));
		}
		return;
	}

	switch(Def.Suit)
	{
	case EHexSuit::Heart:
		if(!Alternate)
		{
			for(int32 I=0;I<9;++I)
			{
				const double A=I*2.39996, R=I? .14+.025*I:0, X=FMath::Cos(A)*R,Y=FMath::Sin(A)*R;
				const double Height=.72+.025*(I%4);
				Block(X,Y,Height*.4,.065,.065,Height*.8,0);
				Block(X,Y,Height*.75,.22,.22,.34,1);
				Block(X,Y,Height+.025,.14,.14,.18,1);
			}
		}
		else
		{
			Block(0,0,.22,.5,.44,.44,0);
			for(int32 I=0;I<4;++I) Block(0,0,.48+I*.055,.58-I*.1,.54,.065,4);
			Block(-.255,0,.13,.015,.13,.26,5); // front door
			Block(-.26,.15,.3,.02,.10,.12,6); Block(-.26,-.15,.3,.02,.10,.12,6);
			Block(.12,.13,.72,.07,.07,.25,2);
		}
		break;
	case EHexSuit::Spade:
	{
		// A world-aligned height field makes adjacent plateau tiles meet at similar elevations.
		const double Step=W/12.0;
		for(int32 GX=FMath::FloorToInt((Surface.X-W*.53)/Step);GX<=FMath::CeilToInt((Surface.X+W*.53)/Step);++GX)
		for(int32 GY=FMath::FloorToInt((Surface.Y-W*.60)/Step);GY<=FMath::CeilToInt((Surface.Y+W*.60)/Step);++GY)
		{
			const double X=(GX*Step-Surface.X)/W,Y=(GY*Step-Surface.Y)/W;
			if(FMath::Abs(X)>.5 || FMath::Abs(Y)>.57735-FMath::Abs(X)*.57735) continue;
			const double Height=.85+.08*FMath::Sin(GX*.35)+.06*FMath::Cos(GY*.3);
			const bool Opening=Alternate && X<.12 && FMath::Abs(Y)<.15;
			const double Bottom=Opening?.58:0;
			Block(X,Y,(Height+Bottom)*.5,1.02/12,1.02/12,Height-Bottom,2);
		}
		break;
	}
	case EHexSuit::Diamond:
		if(!Alternate)
		{
			Block(0,0,.12,.66,.29,.24,5);
			Block(-.36,0,.14,.12,.20,.18,5); Block(.36,0,.14,.12,.20,.18,5);
			Block(0,0,.27,.55,.27,.07,3); Block(.05,0,.40,.29,.23,.22,3);
			Block(.12,0,.57,.08,.08,.18,4); Block(-.13,0,.55,.025,.025,.5,0);
			for(int32 I=-2;I<=2;++I) Block(I*.16,.39,.08,.15,.12,.10,0); // dock
		}
		else
		{
			Block(0,0,.16,.65,.31,.25,0,FRotator(0,22,145));
			Block(-.30,-.10,.13,.18,.25,.22,5,FRotator(12,28,130));
			Block(.12,.05,.29,.25,.20,.22,3,FRotator(0,22,145));
			Block(-.12,.12,.3,.025,.025,.42,0,FRotator(32,22,30));
		}
		break;
	case EHexSuit::Club:
		if(!Alternate)
		{
			Block(0,0,.24,.62,.60,.48,3);
			for(int32 X=-1;X<=1;X+=2) for(int32 Y=-1;Y<=1;Y+=2)
			{
				Block(X*.3,Y*.29,.37,.18,.18,.74,2);
				Block(X*.3,Y*.29,.78,.21,.21,.10,6);
			}
			Block(0,0,.59,.24,.24,.38,3); Block(0,0,.81,.29,.29,.08,4);
			Block(-.315,0,.17,.02,.17,.34,5);
		}
		else
		{
			for(int32 X=-1;X<=1;++X) for(int32 Y=-1;Y<=1;++Y)
			{
				const double Height=.28+.09*((X+Y+4)%3);
				Block(X*.28,Y*.28,Height*.5,.20,.20,Height,3);
				Block(X*.28,Y*.28,Height+.045,.23,.23,.09,(X+Y)%2?4:2);
				Block(X*.28-.105,Y*.28,.15,.015,.07,.10,6);
			}
		}
		break;
	default: break;
	}
}


void UHexSceneryComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
	if(!FloatingShip) return;
	FloatTime+=DeltaTime;
	FloatingShip->SetRelativeLocation(ShipRestPosition+FVector(0,0,2.0*FMath::Sin(FloatTime*.8)));
	FloatingShip->SetRelativeRotation(ShipRestRotation+FRotator(.7*FMath::Sin(FloatTime*.53),0,1.2*FMath::Sin(FloatTime*.67)));
}

