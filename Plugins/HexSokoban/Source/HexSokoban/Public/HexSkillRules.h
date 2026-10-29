#pragma once
#include "CoreMinimal.h"
#include "HexCardTypes.h"
#include "HexSokobanTypes.h"

struct FHexSkillBoxShift { FIntPoint From; FIntPoint To; };
struct FHexSkillPlan
{
	EHexDirection Direction = EHexDirection::Invalid;
	FIntPoint Destination = FIntPoint::ZeroValue;
	TArray<FHexSkillBoxShift> Boxes;
};

/** Pure skill rules. No animations, RNG, payments or solver changes. */
namespace HexSkillRules
{
	HEXSOKOBAN_API bool Plan(const TMap<FIntPoint, FHexCard>& Tiles, const TSet<FIntPoint>& Boxes,
		FIntPoint Player, FIntPoint Target, const FHexCard& Card, FHexSkillPlan& Out);
}
