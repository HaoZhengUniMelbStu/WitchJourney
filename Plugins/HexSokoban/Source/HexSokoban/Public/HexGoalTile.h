// Goal variant of AHexTile: identical, but counts as a goal. The level is complete when EVERY goal
// tile in the level holds a box (any number of goals / boxes). Give it its own look in a Blueprint child.

#pragma once

#include "CoreMinimal.h"
#include "HexTile.h"
#include "HexGoalTile.generated.h"

UCLASS(Blueprintable, meta = (DisplayName = "Hex Goal Tile"))
class HEXSOKOBAN_API AHexGoalTile : public AHexTile
{
	GENERATED_BODY()

public:
	AHexGoalTile();
};
