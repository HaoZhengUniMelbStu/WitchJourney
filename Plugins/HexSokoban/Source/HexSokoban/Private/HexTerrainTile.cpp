#include "HexTerrainTile.h"
#include "HexSokobanVisuals.h"
#include "HexGridLibrary.h"
#include "HexSokobanSettings.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RandomStream.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"

AHexTerrainTile::AHexTerrainTile()
{
	// The terrain materials are the look: no prototype tint on top of them.
	bApplyTintColor = false;
}

void AHexTerrainTile::OnConstruction(const FTransform& Transform)
{
	ApplyTerrainLook();
	Super::OnConstruction(Transform);
}

void AHexTerrainTile::ApplyTerrainLook()
{
	if (!TileMesh)
	{
		return;
	}
	if (TerrainMesh && TileMesh->GetStaticMesh() != TerrainMesh)
	{
		TileMesh->SetStaticMesh(TerrainMesh);
	}
	for (int32 Slot = 0; Slot < TerrainMaterials.Num(); ++Slot)
	{
		if (TerrainMaterials[Slot] && TileMesh->GetMaterial(Slot) != TerrainMaterials[Slot])
		{
			TileMesh->SetMaterial(Slot, TerrainMaterials[Slot]);
		}
	}
	// The guidance overlay is not drawn on Nanite meshes, and these ground meshes are tiny anyway.
	TileMesh->SetForceDisableNanite(true);
	// Ground never moves: the lake water's Gerstner waves would make the surface bob and slide on a low-poly tile.
	// Only the vertex displacement is skipped; the water's ripples, foam and glints are pixel-side and keep animating.
	if (TileMesh->bEvaluateWorldPositionOffset)
	{
		TileMesh->SetEvaluateWorldPositionOffset(false);
	}
}

void AHexTerrainTile::BeginPlay()
{
	ApplyTerrainLook();
	Super::BeginPlay();	// wraps the ground materials for Flash / Dim / Error (harmless if they lack them)

	// Water: the guidance feedback lives in the ripple layer on the water top. An overlay on the whole mesh would be
	// drawn on every face of the see-through water block (top, bottom, sides, the bed behind) and stack up into an
	// opaque white slab.
	CreateRipple();
	if (RippleInstance)
	{
		RippleInstance->SetScalarParameterValue(FlashParameterName, 0.f);
		RippleInstance->SetScalarParameterValue(DimParameterName, 0.f);
		RippleInstance->SetScalarParameterValue(ErrorParameterName, 0.f);
		AddFeedbackMaterial(RippleInstance);
	}

	// Guidance feedback through an overlay, since the stylized ground materials have no such parameters.
	UMaterialInterface* Overlay = IsWater() ? nullptr : FeedbackOverlay.Get();
	if (!Overlay && !IsWater())
	{
		const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
		Overlay = Settings.TileFeedbackOverlay.IsNull() ? nullptr : Settings.TileFeedbackOverlay.LoadSynchronous();
	}
	if (Overlay && TileMesh)
	{
		FeedbackOverlayInstance = UMaterialInstanceDynamic::Create(Overlay, this);
		FeedbackOverlayInstance->SetScalarParameterValue(FlashParameterName, 0.f);
		FeedbackOverlayInstance->SetScalarParameterValue(DimParameterName, 0.f);
		FeedbackOverlayInstance->SetScalarParameterValue(ErrorParameterName, 0.f);
		TileMesh->SetOverlayMaterial(FeedbackOverlayInstance);
		AddFeedbackMaterial(FeedbackOverlayInstance);
	}

	// Goal: the terrain stays visible, a light column around it says "bring a box here".
	if (IsGoal() && GoalGlowMesh && GoalGlowMaterial && TileMesh)
	{
		GoalGlow = NewObject<UStaticMeshComponent>(this, TEXT("GoalGlow"));
		GoalGlow->SetMobility(EComponentMobility::Movable);
		GoalGlow->SetupAttachment(TileMesh);
		GoalGlow->SetStaticMesh(GoalGlowMesh);
		GoalGlow->SetMaterial(0, GoalGlowMaterial);
		for (int32 Slot = 1; Slot < GoalGlow->GetNumMaterials(); ++Slot)
		{
			GoalGlow->SetMaterial(Slot, GoalGlowMaterial);
		}
		GoalGlow->SetRelativeScale3D(GoalGlowScale);
		GoalGlow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GoalGlow->SetCastShadow(false);
		GoalGlow->SetCanEverAffectNavigation(false);
		GoalGlow->SetForceDisableNanite(true);
		GoalGlow->RegisterComponent();
	}

	RebuildScatter();

	if (IsWater())
	{
		// Neighbours may not be spawned yet (board load spawns tile by tile): settle the shoreline next tick,
		// for this tile and the water around it (their shared edge stops being a shore).
		TWeakObjectPtr<AHexTerrainTile> WeakThis(this);
		GetWorldTimerManager().SetTimerForNextTick([WeakThis]()
		{
			if (AHexTerrainTile* Self = WeakThis.Get())
			{
				Self->UpdateShoreEdges();
				Self->ForEachWaterNeighbour(UHexGridLibrary::WorldToCoord(Self->GetActorLocation()), Self,
					[](AHexTerrainTile& Neighbour) { Neighbour.UpdateShoreEdges(); });
			}
		});
	}
}

void AHexTerrainTile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UWorld* World = GetWorld();
	if (IsWater() && World && (EndPlayReason == EEndPlayReason::Destroyed || EndPlayReason == EEndPlayReason::RemovedFromWorld))
	{
		// The water around a removed tile gets a new shoreline where it was. Next tick: by then this tile is gone.
		TArray<TWeakObjectPtr<AHexTerrainTile>> Neighbours;
		ForEachWaterNeighbour(UHexGridLibrary::WorldToCoord(GetActorLocation()), this,
			[&Neighbours](AHexTerrainTile& Neighbour) { Neighbours.Add(&Neighbour); });
		if (!Neighbours.IsEmpty())
		{
			World->GetTimerManager().SetTimerForNextTick([Neighbours]()
			{
				for (const TWeakObjectPtr<AHexTerrainTile>& Neighbour : Neighbours)
				{
					if (Neighbour.IsValid())
					{
						Neighbour->UpdateShoreEdges();
					}
				}
			});
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AHexTerrainTile::ForEachWaterNeighbour(FIntPoint Cell, const AActor* Ignore, TFunctionRef<void(AHexTerrainTile&)> Fn) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AHexTerrainTile> It(World); It; ++It)
	{
		AHexTerrainTile* Other = *It;
		if (Other == Ignore || !IsValid(Other) || Other->IsActorBeingDestroyed() || !Other->IsWater())
		{
			continue;
		}
		const FIntPoint OtherCell = UHexGridLibrary::WorldToCoord(Other->GetActorLocation());
		if (OtherCell != Cell && UHexGridLibrary::GetHexDistance(OtherCell, Cell) == 1)
		{
			Fn(*Other);
		}
	}
}

void AHexTerrainTile::CreateRipple()
{
	if (!IsWater() || !TileMesh || Ripple)
	{
		return;
	}
	UStaticMesh* Mesh = RippleMesh;
	if (!Mesh)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	}
	if (!Mesh)
	{
		return;
	}
	RippleInstance = UMaterialInstanceDynamic::Create(RippleMaterial, this);
	Ripple = NewObject<UStaticMeshComponent>(this, TEXT("WaterRipple"));
	Ripple->SetMobility(EComponentMobility::Movable);	// rides the guidance lift with the tile
	Ripple->SetupAttachment(TileMesh);
	Ripple->SetStaticMesh(Mesh);
	Ripple->SetMaterial(0, RippleInstance);
	// The quad is 100 cm wide: cover the whole hex top (corner radius ~126).
	const double Cover = 2.0 * (ShoreApothem / 0.8660254) * 1.06 / 100.0;
	Ripple->SetRelativeLocation(FVector(0.0, 0.0, GetLocalTopZ() + RippleHeightOffset));
	Ripple->SetRelativeScale3D(FVector(Cover, Cover, 1.0));
	// The shader works in tile-local units (quad space x PlaneScale), so the tile's own scale - e.g. the level
	// editor's spawn pop-in - never shrinks the shoreline or the rock rings.
	RippleInstance->SetScalarParameterValue(TEXT("PlaneScale"), static_cast<float>(Cover));
	Ripple->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ripple->SetCastShadow(false);
	Ripple->SetCanEverAffectNavigation(false);
	Ripple->SetGenerateOverlapEvents(false);
	Ripple->SetTranslucentSortPriority(10);	// above the water; guide lines (100) and card faces (200) stay on top
	Ripple->RegisterComponent();

	// Until the neighbours are known every edge is a shore.
	UpdateShoreEdges();
}

void AHexTerrainTile::UpdateShoreEdges()
{
	if (!RippleInstance || !TileMesh)
	{
		return;
	}
	const FIntPoint Cell = UHexGridLibrary::WorldToCoord(GetActorLocation());
	TSet<FIntPoint> WaterCells;
	ForEachWaterNeighbour(Cell, this, [&WaterCells](AHexTerrainTile& Neighbour)
	{
		WaterCells.Add(UHexGridLibrary::WorldToCoord(Neighbour.GetActorLocation()));
	});

	// Tile-local units (see CreateRipple): no component scale here.
	const double Apothem = ShoreApothem;
	const FVector Center = UHexGridLibrary::CoordToWorld(Cell);
	const FTransform& TileTransform = TileMesh->GetComponentTransform();
	for (int32 Dir = 0; Dir < 6; ++Dir)
	{
		const FIntPoint NeighbourCell = UHexGridLibrary::GetNeighbor(Cell, static_cast<EHexDirection>(Dir));
		const FVector Normal = TileTransform.InverseTransformVectorNoScale(UHexGridLibrary::CoordToWorld(NeighbourCell) - Center).GetSafeNormal2D();
		const bool bShore = !WaterCells.Contains(NeighbourCell);
		RippleInstance->SetVectorParameterValue(*FString::Printf(TEXT("Edge%d"), Dir),
			FLinearColor(Normal.X, Normal.Y, Apothem, bShore ? 1.f : 0.f));
	}
}

void AHexTerrainTile::ApplyRockRipples()
{
	if (!RippleInstance)
	{
		return;
	}
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FLinearColor Value = RippleRocks.IsValidIndex(Index)
			? FLinearColor(RippleRocks[Index].X, RippleRocks[Index].Y, RippleRocks[Index].Z, 1.f)
			: FLinearColor(0.f, 0.f, 0.f, 0.f);
		RippleInstance->SetVectorParameterValue(*FString::Printf(TEXT("Rock%d"), Index), Value);
	}
}

void AHexTerrainTile::ClearScatter()
{
	for (UInstancedStaticMeshComponent* Component : ScatterComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	ScatterComponents.Reset();
}

UInstancedStaticMeshComponent* AHexTerrainTile::GetScatterComponent(UStaticMesh* Mesh)
{
	for (UInstancedStaticMeshComponent* Component : ScatterComponents)
	{
		if (Component && Component->GetStaticMesh() == Mesh)
		{
			return Component;
		}
	}
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this);
	Component->SetMobility(EComponentMobility::Movable);	// rides the tile's guidance lift
	Component->SetupAttachment(TileMesh);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);	// never blocks clicks / traces
	Component->SetCanEverAffectNavigation(false);
	Component->SetGenerateOverlapEvents(false);
	Component->RegisterComponent();
	ScatterComponents.Add(Component);
	return Component;
}

void AHexTerrainTile::RebuildScatter()
{
	ClearScatter();
	RippleRocks.Reset();
	if (!TileMesh || Scatter.IsEmpty())
	{
		ApplyRockRipples();
		return;
	}

	// Same cell + same terrain = same dressing, every time the level is loaded.
	const FIntPoint Cell = UHexGridLibrary::WorldToCoord(GetActorLocation());
	// FName hashes are per-session name-table indices: hash the class name text so the dressing is stable across runs.
	uint32 Seed = HashCombine(GetTypeHash(Cell), FCrc::StrCrc32(*GetClass()->GetName()));
	Seed = HashCombine(Seed, GetTypeHash(ScatterSeed));
	FRandomStream Rng(static_cast<int32>(Seed));

	const float TopZ = GetLocalTopZ();
	struct FPlaced
	{
		FVector2D Position;
		float Spacing;
	};
	TArray<FPlaced> Placed;

	for (const FHexScatterLayer& Layer : Scatter)
	{
		if (Layer.Meshes.IsEmpty() || Rng.FRand() > Layer.Chance)
		{
			continue;
		}
		const int32 Count = Rng.RandRange(Layer.MinCount, FMath::Max(Layer.MinCount, Layer.MaxCount));
		const double RadiusMin = FMath::Max(0.0, FMath::Min(Layer.RadiusRange.X, Layer.RadiusRange.Y));
		const double RadiusMax = FMath::Max(Layer.RadiusRange.X, Layer.RadiusRange.Y);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			UStaticMesh* Mesh = Layer.Meshes[Rng.RandRange(0, Layer.Meshes.Num() - 1)];
			if (!Mesh)
			{
				continue;
			}

			// A free spot in the ring (uniform over its area), respecting everyone's personal space.
			FVector2D Position = FVector2D::ZeroVector;
			bool bFound = false;
			for (int32 Try = 0; Try < 16 && !bFound; ++Try)
			{
				const double Radius = FMath::Sqrt(FMath::Lerp(RadiusMin * RadiusMin, RadiusMax * RadiusMax, static_cast<double>(Rng.FRand())));
				const double Angle = Rng.FRand() * 2.0 * UE_DOUBLE_PI;
				Position = FVector2D(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);
				bFound = true;
				for (const FPlaced& Other : Placed)
				{
					if (FVector2D::Distance(Other.Position, Position) < 0.5 * (Other.Spacing + Layer.Spacing))
					{
						bFound = false;
						break;
					}
				}
			}
			if (!bFound)
			{
				continue;	// crowded: fewer is fine
			}

			// Size: normalise to a target height (the kit's meshes range from 5 cm pebbles to 50 m trees), cap the footprint.
			const FBox Bounds = Mesh->GetBoundingBox();
			const FVector Size = Bounds.GetSize();
			const double Height = FMath::Max(1.0, Size.Z);
			const double Width = FMath::Max3(1.0, Size.X, Size.Y);
			double Scale = Layer.HeightRange.Y > 0.0
				? FMath::Lerp(Layer.HeightRange.X, Layer.HeightRange.Y, static_cast<double>(Rng.FRand())) / Height
				: FMath::Lerp(Layer.ScaleRange.X, Layer.ScaleRange.Y, static_cast<double>(Rng.FRand()));
			if (Layer.MaxWidth > 0.f && Width * Scale > Layer.MaxWidth)
			{
				// Capped by footprint: still vary the size a little so a row of rocks doesn't look cloned.
				Scale = Layer.MaxWidth * FMath::Lerp(0.7, 1.0, static_cast<double>(Rng.FRand())) / Width;
			}
			Scale = FMath::Max(Scale, 0.001);
			if (IsGoal() && GoalMaxScatterHeight > 0.f && Height * Scale > GoalMaxScatterHeight)
			{
				continue;	// no trees in front of the goal light
			}

			const double Yaw = Rng.FRand() * 360.0;
			const double Pitch = Layer.MaxTilt > 0.f ? Rng.FRandRange(-Layer.MaxTilt, Layer.MaxTilt) : 0.0;
			const double Roll = Layer.MaxTilt > 0.f ? Rng.FRandRange(-Layer.MaxTilt, Layer.MaxTilt) : 0.0;
			const FRotator Rotation(Pitch, Yaw, Roll);

			// Put the mesh's footprint centre (not its pivot) on the spot, its base on the ground (slightly sunk).
			const FVector CenterOffset = Rotation.RotateVector(FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, 0.0) * Scale);
			const double BaseZ = TopZ - Bounds.Min.Z * Scale - Layer.Sink * Height * Scale;
			const FVector Location(Position.X - CenterOffset.X, Position.Y - CenterOffset.Y, BaseZ);

			UInstancedStaticMeshComponent* Component = GetScatterComponent(Mesh);
			Component->SetCastShadow(Layer.bCastShadow);
			Component->SetRenderCustomDepth(Layer.bBlocksGuideLines);
			Component->SetCustomDepthStencilValue(HexSokoban::StencilBlocker);
			Component->AddInstance(FTransform(Rotation, Location, FVector(Scale)));
			Placed.Add({ Position, Layer.Spacing });

			if (Layer.bWaterRipple && RippleRocks.Num() < 4)
			{
				// Ring centre / radius in tile-local units (the ripple shader works in tile space).
				const double Radius = Width * Scale * Layer.RippleRadiusScale;
				RippleRocks.Add(FVector3f(static_cast<float>(Position.X), static_cast<float>(Position.Y), static_cast<float>(Radius)));
			}
		}
	}
	ApplyRockRipples();
}
