// A tile with a terrain look: its own ground mesh / materials plus randomly scattered set dressing
// (grass, flowers, bushes, trees, rocks). One Blueprint child per terrain (BP_HexTile_Grass / Rock / Road / River);
// the board picks the child from the tile's suit (Project Settings > Hex Sokoban > Suit Tile Classes).
//
// Scatter is decorative only: no collision, never in the middle of the tile (the card face, boxes and the player
// live there), and seeded from the grid cell so a level always looks the same.

#pragma once

#include "CoreMinimal.h"
#include "HexTile.h"
#include "HexTerrainTile.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UInstancedStaticMeshComponent;

/** One kind of set dressing scattered on a terrain tile. */
USTRUCT(BlueprintType)
struct HEXSOKOBAN_API FHexScatterLayer
{
	GENERATED_BODY()

	/** Picked at random per placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	TArray<TObjectPtr<UStaticMesh>> Meshes;

	/** Chance (0..1) that this layer appears on a tile at all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Chance = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0"))
	int32 MaxCount = 1;

	/** Target height in cm (random in range). Each mesh is scaled to it, whatever its authored size. 0 = use ScaleRange. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	FVector2D HeightRange = FVector2D::ZeroVector;

	/** Plain uniform scale when HeightRange is 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	FVector2D ScaleRange = FVector2D(1.0, 1.0);

	/** Footprint cap in cm (widest side). 0 = no cap. Keeps wide trees / boulders from swallowing the tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0.0"))
	float MaxWidth = 0.f;

	/** Distance from the tile centre in cm (the tile's flat-side radius is about 110). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	FVector2D RadiusRange = FVector2D(55.0, 100.0);

	/** Personal space in cm: placements keep (A + B) / 2 apart. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0.0"))
	float Spacing = 20.f;

	/** How deep the base sinks into the ground, as a fraction of the placed height (rocks look embedded). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float Sink = 0.03f;

	/** Small random tilt in degrees (rocks, bushes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0.0", ClampMax = "30.0"))
	float MaxTilt = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	bool bCastShadow = true;

	/** Hides the guidance path lines behind it (trees, bushes, rocks). Off for grass and flowers, which lines draw over. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	bool bBlocksGuideLines = true;

	/** Water tiles: each placement gets cartoon ripples around it where it breaks the surface (rocks in a river). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter")
	bool bWaterRipple = false;

	/** Ripple ring radius at the waterline, as a fraction of the placed footprint width. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scatter", meta = (ClampMin = "0.05", ClampMax = "1.0", EditCondition = "bWaterRipple"))
	float RippleRadiusScale = 0.42f;
};

UCLASS(Blueprintable, meta = (DisplayName = "Hex Terrain Tile"))
class HEXSOKOBAN_API AHexTerrainTile : public AHexTile
{
	GENERATED_BODY()

public:
	AHexTerrainTile();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Water tile: has shore / rock ripples. Neighbouring water tiles form one body of water. */
	bool IsWater() const { return RippleMaterial != nullptr; }

	/** Re-check which edges face land (shore ripples run along the outline of the whole body of water). */
	void UpdateShoreEdges();

	/** Spawn-time: make this terrain tile a goal (call before FinishSpawning). Adds the goal glow at BeginPlay. */
	void SetIsGoal(bool bGoal) { TileType = bGoal ? EHexTileType::Goal : EHexTileType::Normal; }

	/** Rebuild the scatter (e.g. after editing the layers at runtime). */
	UFUNCTION(BlueprintCallable, Category = "Hex Tile|Terrain")
	void RebuildScatter();

protected:
	/** Ground mesh of this terrain (SM_Land, SM_River ...). Empty = Project Settings hex mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Terrain")
	TObjectPtr<UStaticMesh> TerrainMesh;

	/** Material per slot of TerrainMesh. Empty entries keep the mesh's own material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Terrain")
	TArray<TObjectPtr<UMaterialInterface>> TerrainMaterials;

	/** Set dressing, placed in order (earlier layers claim space first: put big things first). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Terrain")
	TArray<FHexScatterLayer> Scatter;

	/** Mixed into the per-cell seed: change it to reroll every tile of this terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Terrain")
	int32 ScatterSeed = 0;

	/** Goal marker: a glowing shell around the tile (PathTracer-style light column) on goal tiles only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Goal")
	TObjectPtr<UStaticMesh> GoalGlowMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Goal")
	TObjectPtr<UMaterialInterface> GoalGlowMaterial;

	/** Shell scale relative to the tile (slightly wider so it wraps the sides, taller so the light rises). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Goal")
	FVector GoalGlowScale = FVector(1.05, 1.05, 2.4);

	/** Goal tiles skip set dressing taller than this (cm) so the light column stays readable. 0 = keep all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Goal", meta = (ClampMin = "0.0"))
	float GoalMaxScatterHeight = 60.f;

	/**
	 * Water: a ripple layer drawn on the water surface. Set = this terrain is water. Its shore ripples follow the
	 * outline of the connected water (edges shared with another water tile get none), rocks get rings of their own.
	 * Parameters fed per tile: Edge0..5 (normal.xy, apothem, isShore), Rock0..3 (offset.xy, radius, on).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Water")
	TObjectPtr<UMaterialInterface> RippleMaterial;

	/** Flat quad carrying the ripple material (100 cm, centred). Empty = /Engine/BasicShapes/Plane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Water")
	TObjectPtr<UStaticMesh> RippleMesh;

	/** Distance from the tile centre to the shoreline (the water top's flat side), in mesh units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Water", meta = (ClampMin = "1.0"))
	float ShoreApothem = 106.f;

	/** Ripple layer height above the water top (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Water")
	float RippleHeightOffset = 0.4f;

	/** Guidance overlay (Flash / Dim / Error). Empty = Project Settings Tile Feedback Overlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Terrain")
	TObjectPtr<UMaterialInterface> FeedbackOverlay;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ScatterComponents;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FeedbackOverlayInstance;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> GoalGlow;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Ripple;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RippleInstance;

	/** Rock ripple sources from the last scatter: offset from the tile centre (world xy), waterline radius. */
	TArray<FVector3f> RippleRocks;

	void CreateRipple();
	void ApplyRockRipples();
	/** Water tiles around Cell (excluding Ignore). */
	void ForEachWaterNeighbour(FIntPoint Cell, const AActor* Ignore, TFunctionRef<void(AHexTerrainTile&)> Fn) const;

	void ApplyTerrainLook();
	void ClearScatter();
	UInstancedStaticMeshComponent* GetScatterComponent(UStaticMesh* Mesh);
};
