// Path Tracer integration (Marketplace "Path Tracer Toolkit", BP_PathTracer children in /Game/HexSokoban/FX/PathFX):
//   Hand range  (orange loop)  - the player's tile + every tile the hand can pay for right now.
//   Card range  (blue dashes)  - the player's tile + the tiles the hovered / dragged / selected card can pay for
//                                (always inside the orange loop).
//   Move path   (blue line)    - while a card is dragged over a reachable tile: player -> tile, with markers.
//   Box path    (orange line)  - when that move pushes a box: where the box will slide.
//   Hint path   (yellow dashes)- H toggles the next steps of a winning line (from the solver).
//   Goal rings  (green dashes) - every goal tile; solid gold once a box sits on it.
// The subsystem only spawns those actors, feeds them world points through their DrawPath function and
// rebuilds one only when its points change. Classes / insets: Project Settings > Hex Sokoban > Path FX.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HexPathFXSubsystem.generated.h"

class AHexTile;

UCLASS()
class HEXSOKOBAN_API UHexPathFXSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Show / hide the solver hint path (H). */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Path FX")
	void SetHintVisible(bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Path FX")
	void ToggleHint() { SetHintVisible(!bHintVisible); }

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Path FX")
	bool IsHintVisible() const { return bHintVisible; }

	/** Destroy every effect actor (they are rebuilt on the next tick when needed). */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Path FX")
	void ClearAll();

	/**
	 * Closed outline(s) around a set of hex cells, in world space (Inset > 0 shrinks towards the cells).
	 * CellLift (optional) raises every corner to the highest lift of the region cells sharing it, so lifted tiles
	 * never cover the line.
	 */
	static void BuildRegionLoops(const TArray<FIntPoint>& Cells, float Inset, double Z, TArray<TArray<FVector>>& OutLoops,
		TFunctionRef<double(const FIntPoint&)> CellLift = [](const FIntPoint&) { return 0.0; });

	/** Visual lift of the tile at Coord (guidance bounce), quantised so the effects don't rebuild every frame. */
	double GetCellLift(const FIntPoint& Coord) const;

private:
	struct FFXSlot
	{
		TWeakObjectPtr<AActor> Actor;
		TArray<FVector> Points;
		UClass* Class = nullptr;
		int32 Style = 0;
	};

	/** Spawn / rebuild / remove one effect actor so it shows Points (fewer than 2 points = hidden). */
	void UpdateSlot(FFXSlot& Slot, UClass* Class, const TArray<FVector>& Points, bool bClosedLoop, int32 Style = 0);
	void UpdateLoopSlots(TArray<FFXSlot>& Slots, UClass* Class, const TArray<TArray<FVector>>& Loops, int32 Style = 0);
	void ClearSlot(FFXSlot& Slot);
	void ApplyStyle(AActor* Actor, const FFXSlot& Slot, bool bClosedLoop, int32 Style) const;

	/** Target reticle (closed hex ring + X, three effect actors) at the last point of a path; empty path = hidden. */
	void UpdateReticle(TArray<FFXSlot>& Slots, UClass* Class, const TArray<FVector>& PathPoints);

	void UpdateHint(bool bShow, double Z);
	void UpdateGoalRings(bool bShow, double Z);

	TArray<FFXSlot> HandSlots;
	TArray<FFXSlot> CardSlots;
	FFXSlot MoveSlot;
	FFXSlot BoxSlot;
	FFXSlot HintSlot;
	TArray<FFXSlot> GoalSlots;
	TArray<FFXSlot> MoveReticle;
	TArray<FFXSlot> BoxReticle;
	TArray<FFXSlot> HintReticle;

	bool bHintVisible = false;
	TArray<FIntPoint> HintStateKey;
	TArray<FVector> HintPoints;
};
