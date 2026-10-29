#include "HexSkyDecor.h"
#include "HexBoardSubsystem.h"
#include "HexTile.h"
#include "HexSokobanSettings.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

namespace HexSky
{
	static const TCHAR* PlaneMesh = TEXT("/Engine/BasicShapes/Plane.Plane");
	static const TCHAR* StarMaterial = TEXT("/Game/HexSokoban/Materials/M_HexSkyStar.M_HexSkyStar");
	static const TCHAR* LineMaterial = TEXT("/Game/HexSokoban/Materials/M_HexSkyLine.M_HexSkyLine");

	static void SetupLayer(UInstancedStaticMeshComponent* Layer)
	{
		Layer->SetMobility(EComponentMobility::Movable);
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Layer->SetGenerateOverlapEvents(false);
		Layer->SetCastShadow(false);
		Layer->SetCanEverAffectNavigation(false);
		Layer->bReceivesDecals = false;
		Layer->NumCustomDataFloats = 2; // 0 = seed, 1 = brightness
	}
}

AHexSkyDecor::AHexSkyDecor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetCanBeDamaged(false);

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetMobility(EComponentMobility::Movable);
	RootComponent = Pivot;

	Stars = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Stars"));
	Stars->SetupAttachment(Pivot);
	HexSky::SetupLayer(Stars);

	Lines = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Lines"));
	Lines->SetupAttachment(Pivot);
	HexSky::SetupLayer(Lines);
}

void AHexSkyDecor::BeginPlay()
{
	Super::BeginPlay();
	Stars->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, HexSky::PlaneMesh));
	Lines->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, HexSky::PlaneMesh));
	if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, HexSky::StarMaterial)) Stars->SetMaterial(0, Material);
	if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, HexSky::LineMaterial)) Lines->SetMaterial(0, Material);
}

void AHexSkyDecor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Stars->GetInstanceCount() > 0) Pivot->AddLocalRotation(FRotator(0.f, OrbitDegreesPerSecond * DeltaSeconds, 0.f));

	PollTimer -= DeltaSeconds;
	if (PollTimer > 0.f) return;
	PollTimer = .5f;

	FVector Center;
	float Radius = 0.f, SurfaceZ = 0.f;
	if (!QueryBoard(Center, Radius, SurfaceZ))
	{
		if (BuiltRadius >= 0.f)
		{
			Stars->ClearInstances();
			Lines->ClearInstances();
			BuiltRadius = -1.f;
		}
		return;
	}
	if (BuiltRadius >= 0.f && FMath::IsNearlyEqual(Radius, BuiltRadius, 1.f) && Center.Equals(BuiltCenter, 1.f)) return;
	Rebuild(Center, Radius, SurfaceZ);
}

bool AHexSkyDecor::QueryBoard(FVector& OutCenter, float& OutRadius, float& OutSurfaceZ) const
{
	const UHexBoardSubsystem* Board = GetWorld() ? GetWorld()->GetSubsystem<UHexBoardSubsystem>() : nullptr;
	if (!Board) return false;
	TArray<FVector> Surfaces;
	for (const AHexTile* Tile : Board->GetAllTiles())
	{
		if (IsValid(Tile)) Surfaces.Add(Tile->GetSurfaceLocation());
	}
	if (Surfaces.IsEmpty()) return false;

	FBox Bounds(ForceInit);
	OutSurfaceZ = -UE_BIG_NUMBER;
	for (const FVector& Surface : Surfaces)
	{
		Bounds += Surface;
		OutSurfaceZ = FMath::Max(OutSurfaceZ, static_cast<float>(Surface.Z));
	}
	OutCenter = Bounds.GetCenter();
	float Reach = 0.f;
	for (const FVector& Surface : Surfaces) Reach = FMath::Max(Reach, static_cast<float>(FVector::Dist2D(Surface, OutCenter)));
	// Quantise to whole tiles so editing a level does not reshuffle the sky on every small change.
	const float Spacing = FMath::Max(1.f, UHexSokobanSettings::Get().TileSpacing);
	OutRadius = FMath::CeilToFloat((Reach + Spacing * .5f) / Spacing) * Spacing;
	OutCenter.X = FMath::GridSnap(OutCenter.X, Spacing * .5f);
	OutCenter.Y = FMath::GridSnap(OutCenter.Y, Spacing * .5f);
	return true;
}

void AHexSkyDecor::AddStar(const FVector& Local, float Size, float Brightness, float Seed)
{
	// Plane mesh is 100x100 lying flat, so it faces the top-down board camera.
	const FTransform Transform(FRotator(0.f, Seed * 90.f, 0.f), Local, FVector(Size / 100.f));
	const int32 Index = Stars->AddInstance(Transform, false);
	Stars->SetCustomDataValue(Index, 0, Seed, false);
	Stars->SetCustomDataValue(Index, 1, Brightness, false);
}

void AHexSkyDecor::AddLine(const FVector& A, const FVector& B, float Gap, float Brightness, float Seed)
{
	const FVector Delta = B - A;
	const float Length = Delta.Size() - Gap * 2.f;
	if (Length <= 1.f) return;
	const FVector Direction = Delta.GetSafeNormal();
	// A flat light ribbon (engine plane, 100x100, facing up): X runs along the line, the soft
	// cross-section is drawn by the material, so it never aliases into a dashed line.
	const FTransform Transform(FRotationMatrix::MakeFromXZ(Direction, FVector::UpVector).Rotator(), (A + B) * .5f,
		FVector(Length / 100.f, LineRadius / 100.f, 1.f));
	const int32 Index = Lines->AddInstance(Transform, false);
	Lines->SetCustomDataValue(Index, 0, Seed, false);
	Lines->SetCustomDataValue(Index, 1, Brightness, false);
}

void AHexSkyDecor::Rebuild(const FVector& Center, float Radius, float SurfaceZ)
{
	BuiltCenter = Center;
	BuiltRadius = Radius;
	Pivot->SetWorldLocation(FVector(Center.X, Center.Y, SurfaceZ));
	Stars->ClearInstances();
	Lines->ClearInstances();

	const float S = FMath::Max(1.f, UHexSokobanSettings::Get().TileSpacing);
	const float Inner = Radius + InnerGapTiles * S;
	const float Outer = Inner + BandWidthTiles * S;
	// The backdrop sits just under the tiles: keep every star above it, and low enough that a
	// star on the camera side of the board never projects onto a tile (pitch 55 deg: reach 0.7h).
	const float MinZ = FMath::Min(-10.f, 40.f - SurfaceZ);
	auto MaxZAt = [&](float Dist) { return FMath::Lerp(.35f, 1.0f, FMath::Clamp((Dist - Inner) / (Outer - Inner), 0.f, 1.f)) * S; };
	auto InBand = [&](const FVector& P) { const float D = FVector2D(P.X, P.Y).Size(); return D >= Inner && D <= Outer; };
	LineRadius = .07f * S;

	FRandomStream Rng(0x48455853); // stable layout for a given board size
	const int32 NumGroups = FMath::Max(0, NumConstellations);
	for (int32 Group = 0; Group < NumGroups; ++Group)
	{
		const float Angle = (Group + Rng.FRandRange(-.25f, .25f)) * 2.f * PI / FMath::Max(1, NumGroups);
		const float Dist = Rng.FRandRange(Inner + .7f * S, Outer - .9f * S);
		FVector Start(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
		Start.Z = Rng.FRandRange(MinZ, MaxZAt(Dist));

		TArray<FVector> Points;
		Points.Add(Start);
		const int32 Count = Rng.RandRange(5, 7);
		float Heading = Angle + PI * .5f * (Rng.FRand() < .5f ? 1.f : -1.f); // run along the band
		for (int32 Tries = 0; Points.Num() < Count && Tries < 60; ++Tries)
		{
			const FVector& From = Points.Last();
			const float Turn = Heading + Rng.FRandRange(-1.1f, 1.1f);
			const float Step = Rng.FRandRange(.55f, .9f) * S;
			FVector Next = From + FVector(FMath::Cos(Turn) * Step, FMath::Sin(Turn) * Step, 0.f);
			if (!InBand(Next)) { Heading += PI * .35f; continue; }
			Next.Z = FMath::Clamp(From.Z + Rng.FRandRange(-.35f, .35f) * S, MinZ, MaxZAt(FVector2D(Next.X, Next.Y).Size()));
			Points.Add(Next);
			Heading = Turn;
		}
		const float GroupSeed = Rng.FRand();
		const int32 Main = Rng.RandRange(0, Points.Num() - 1);
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const bool bMain = Index == Main;
			AddStar(Points[Index], (bMain ? .62f : Rng.FRandRange(.30f, .42f)) * S, bMain ? 1.6f : Rng.FRandRange(.8f, 1.15f), Rng.FRand());
			if (Index > 0) AddLine(Points[Index - 1], Points[Index], .025f * S, .9f, GroupSeed + Index * .07f);
		}
		if (Points.Num() >= 4 && Rng.FRand() < .6f)
		{
			// One branch from a middle star gives the figure a recognisable shape.
			const int32 From = Rng.RandRange(1, Points.Num() - 2);
			const float Turn = Rng.FRandRange(0.f, 2.f * PI);
			FVector Tip = Points[From] + FVector(FMath::Cos(Turn), FMath::Sin(Turn), 0.f) * Rng.FRandRange(.5f, .8f) * S;
			if (InBand(Tip))
			{
				Tip.Z = FMath::Clamp(Points[From].Z + Rng.FRandRange(-.3f, .3f) * S, MinZ, MaxZAt(FVector2D(Tip.X, Tip.Y).Size()));
				AddStar(Tip, Rng.FRandRange(.28f, .38f) * S, Rng.FRandRange(.75f, 1.05f), Rng.FRand());
				AddLine(Points[From], Tip, .025f * S, .8f, GroupSeed + .5f);
			}
		}
	}

	for (int32 Index = 0; Index < NumDustStars; ++Index)
	{
		const float Angle = Rng.FRandRange(0.f, 2.f * PI);
		const float Dist = FMath::Sqrt(Rng.FRandRange(FMath::Square(Inner - .2f * S), FMath::Square(Outer + 1.5f * S)));
		FVector Point(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
		Point.Z = Rng.FRandRange(MinZ, MaxZAt(Dist) * .8f);
		AddStar(Point, Rng.FRandRange(.12f, .24f) * S, Rng.FRandRange(.35f, .8f), Rng.FRand());
	}
	Stars->MarkRenderStateDirty();
	Lines->MarkRenderStateDirty();
}

void UHexSkyDecorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld() || IsRunningDedicatedServer()) return;
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	InWorld.SpawnActor<AHexSkyDecor>(AHexSkyDecor::StaticClass(), FTransform::Identity, Params);
}
