// Floating 3D constellations and star dust that fill the empty space around the board.
// Purely cosmetic: spawned automatically in game worlds, follows the board bounds (also while
// editing a level), never collides, casts no shadow and stays clear of the tiles on screen.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "HexSkyDecor.generated.h"

class UInstancedStaticMeshComponent;

UCLASS(NotPlaceable, Transient)
class HEXSOKOBAN_API AHexSkyDecor : public AActor
{
	GENERATED_BODY()

public:
	AHexSkyDecor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Slow orbit of the whole star field around the board (degrees per second). */
	UPROPERTY(EditAnywhere, Category = "Sky")
	float OrbitDegreesPerSecond = 0.5f;

	/** Gap between the board edge and the closest star, in tiles. */
	UPROPERTY(EditAnywhere, Category = "Sky")
	float InnerGapTiles = 0.8f;

	/** Depth of the star band, in tiles. */
	UPROPERTY(EditAnywhere, Category = "Sky")
	float BandWidthTiles = 2.6f;

	UPROPERTY(EditAnywhere, Category = "Sky")
	int32 NumConstellations = 6;

	UPROPERTY(EditAnywhere, Category = "Sky")
	int32 NumDustStars = 70;

private:
	bool QueryBoard(FVector& OutCenter, float& OutRadius, float& OutSurfaceZ) const;
	void Rebuild(const FVector& Center, float Radius, float SurfaceZ);
	void AddStar(const FVector& Local, float Size, float Brightness, float Seed);
	void AddLine(const FVector& A, const FVector& B, float Gap, float Brightness, float Seed);

	UPROPERTY(VisibleAnywhere, Category = "Sky")
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(VisibleAnywhere, Category = "Sky")
	TObjectPtr<UInstancedStaticMeshComponent> Stars;

	UPROPERTY(VisibleAnywhere, Category = "Sky")
	TObjectPtr<UInstancedStaticMeshComponent> Lines;

	float PollTimer = 0.f;
	float BuiltRadius = -1.f;
	FVector BuiltCenter = FVector::ZeroVector;
	float LineRadius = 1.2f; // ribbon width for line instances
};

/** Spawns one AHexSkyDecor per game world. */
UCLASS()
class HEXSOKOBAN_API UHexSkyDecorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
