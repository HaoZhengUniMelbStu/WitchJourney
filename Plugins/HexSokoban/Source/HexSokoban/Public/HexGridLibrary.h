// Blueprint-exposed hex grid math, using the lattice from UHexSokobanSettings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HexSokobanTypes.h"
#include "HexGridLibrary.generated.h"

UCLASS()
class HEXSOKOBAN_API UHexGridLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Grid")
	static float GetTileSpacing();

	/** Lattice center of an axial coordinate. Z is passed through. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Grid")
	static FVector CoordToWorld(FIntPoint Coord, float Z = 0.f);

	/** Nearest axial coordinate to a world location (Z ignored). */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Grid")
	static FIntPoint WorldToCoord(const FVector& WorldLocation);

	/** Snaps XY to the nearest lattice center, keeps Z. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Grid")
	static FVector SnapToGrid(const FVector& WorldLocation);

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Grid")
	static FIntPoint GetNeighbor(FIntPoint Coord, EHexDirection Direction);

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Grid")
	static FVector GetDirectionVector(EHexDirection Direction);

	/** Snaps an arbitrary world vector to the closest of the six hex directions. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Grid")
	static EHexDirection GetDirectionFromVector(const FVector& Vector);

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Grid")
	static EHexDirection GetDirectionFromYaw(float Yaw);

	/** Number of steps between two coordinates. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Grid")
	static int32 GetHexDistance(FIntPoint A, FIntPoint B);
};
