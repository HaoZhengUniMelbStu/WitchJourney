// The basic building block of a Hex Sokoban level.
//
// Customising: make a Blueprint child (Content Browser > C++ Classes > HexSokoban > Public > HexTile >
// Create Blueprint Class), then
//   * TileMesh: set your own Static Mesh / materials (a material you assign is always kept),
//   * untick "Apply Tint Color" to show the mesh's own materials,
//   * add any components you like under TileMesh,
//   * implement the On... events below for your own effects.
// Its grid coordinate comes from its world location; tiles snap to the lattice when moved in the editor.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HexSokobanTypes.h"
#include "HexCardTypes.h"
#include "HexTile.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class AHexBox;
class APawn;
class UHexCardLabelComponent;

UCLASS(Blueprintable, meta = (DisplayName = "Hex Tile"))
class HEXSOKOBAN_API AHexTile : public AActor
{
	GENERATED_BODY()

public:
	AHexTile();
	UPROPERTY(BlueprintReadOnly, Category="Hex Tile|Scenery")
	bool bScenery = false;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
#endif

	UFUNCTION(BlueprintPure, Category = "Hex Tile")
	EHexTileType GetTileType() const { return TileType; }

	UFUNCTION(BlueprintPure, Category = "Hex Tile")
	bool IsGoal() const { return TileType == EHexTileType::Goal; }

	/** Axial grid coordinate (valid once the board is built at BeginPlay). */
	UFUNCTION(BlueprintPure, Category = "Hex Tile")
	FIntPoint GetCoord() const { return Coord; }

	/** Where pawns and boxes stand: tile center in XY, top of TileMesh (+ StandHeightOffset) in Z. */
	UFUNCTION(BlueprintPure, Category = "Hex Tile")
	FVector GetSurfaceLocation() const;

	UFUNCTION(BlueprintPure, Category = "Hex Tile")
	UStaticMeshComponent* GetTileMesh() const { return TileMesh; }

	/** Short bright pulse. Works on any material that has a scalar parameter named FlashParameterName. */
	UFUNCTION(BlueprintCallable, Category = "Hex Tile")
	void PlayFlash(float Strength = 0.3f);

	/** Board-internal. */
	void SetCoord(const FIntPoint& InCoord) { Coord = InCoord; }

	// ---- Card label (card-movement rule) ----

	UFUNCTION(BlueprintPure, Category = "Hex Tile|Card")
	bool HasCardLabel() const { return bHasCardLabel; }

	UFUNCTION(BlueprintPure, Category = "Hex Tile|Card")
	FHexCard GetCardLabel() const { return CardLabelData; }

	UFUNCTION(BlueprintPure, Category = "Hex Tile|Card")
	UHexCardLabelComponent* GetCardLabelComponent() const { return CardLabel; }

	/** Card system: assign / remove this tile's label. */
	void SetCardLabel(const FHexCard& InLabel);
	void ClearCardLabel();

	/** A box covers this tile: the label rises into the box (bCovered) or pops back (uncovered). */
	void SetLabelCovered(bool bCovered, bool bAnimate, float RiseHeight = 0.f);

	/** Glow as a tile the player can currently enter with the hand / selected card. */
	void SetCardHighlight(bool bOn);

	// ---- Player guidance ----

	/** Look of this tile while a card is previewed (lift / glow / dim / red). */
	UFUNCTION(BlueprintCallable, Category = "Hex Tile|Guide")
	void SetGuideState(EHexTileGuide NewState, bool bSnap = false);

	UFUNCTION(BlueprintPure, Category = "Hex Tile|Guide")
	EHexTileGuide GetGuideState() const { return GuideState; }

	/** "Can't go here": red blinks + wobble. */
	UFUNCTION(BlueprintCallable, Category = "Hex Tile|Guide")
	void PlayInvalidFeedback();

	/** Current guidance lift (cm). Logic ignores it; boxes on the tile follow it visually. */
	UFUNCTION(BlueprintPure, Category = "Hex Tile|Guide")
	float GetGuideLift() const { return LiftValue; }

	/** Lift the current guide state is heading for (cm). */
	UFUNCTION(BlueprintPure, Category = "Hex Tile|Guide")
	float GetGuideTargetLift() const
	{
		return GuideState == EHexTileGuide::ValidHovered ? GuideHoverLift : (GuideState == EHexTileGuide::Valid ? GuideLift : 0.f);
	}

	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Tile|Guide")
	void OnGuideStateChanged(EHexTileGuide NewGuideState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Tile|Guide")
	void OnInvalidFeedback();

	/** Label assigned at level start / restart. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Tile|Card")
	void OnCardLabelAssigned(const FHexCard& Label);

	/** Highlight as enterable switched on / off. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Tile|Card")
	void OnCardHighlightChanged(bool bNowHighlighted);

	/** A push towards this tile was committed (the box is still on its way). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Tile")
	void OnBoxEntered(AHexBox* Box);

	/** A push away from this tile was committed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Tile")
	void OnBoxLeft(AHexBox* Box);

	/** A pushed box came to rest on this tile (for goals: the moment it "locks in"). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Tile")
	void OnBoxLanded(AHexBox* Box);

	/** The player finished a step onto this tile. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Tile")
	void OnPlayerEntered(APawn* Player);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Tile")
	TObjectPtr<UStaticMeshComponent> TileMesh;

	/** Suit + rank shown on the tile top (runtime only, filled by the card system). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Tile|Card")
	TObjectPtr<UHexCardLabelComponent> CardLabel;

	/** Strength of the steady glow on enterable tiles (Flash parameter). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Visual", meta = (ClampMin = "0.0"))
	float HighlightStrength = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile")
	EHexTileType TileType = EHexTileType::Normal;

	/** Snap XY to the hex lattice when the actor is moved in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile")
	bool bSnapToGrid = true;

	/** Raise / lower where pawns and boxes stand (e.g. a model with a recessed top). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile")
	float StandHeightOffset = 0.f;

	/** Prototype colouring: tints TileMesh slots that have no material assigned by you. Untick to use the mesh's own materials. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Visual")
	bool bApplyTintColor = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Visual", meta = (EditCondition = "bApplyTintColor"))
	FLinearColor NormalColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Visual", meta = (EditCondition = "bApplyTintColor"))
	FLinearColor GoalColor;

	/** Material used for tinting (needs a vector parameter ColorParameterName). Empty = Project Settings default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Visual", meta = (EditCondition = "bApplyTintColor"))
	TObjectPtr<UMaterialInterface> TintMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Visual", meta = (EditCondition = "bApplyTintColor"))
	FName ColorParameterName = TEXT("Color");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Visual")
	FName FlashParameterName = TEXT("Flash");

	/** Flash fade speed (strength per second). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Visual", meta = (ClampMin = "0.1"))
	float FlashFadeSpeed = 2.5f;

	// ---- Guidance feel ----

	/** Lift of a tile the previewed card can reach (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Guide")
	float GuideLift = 10.f;

	/** Lift of a reachable tile under the cursor (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Guide")
	float GuideHoverLift = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Guide", meta = (ClampMin = "1.0"))
	float LiftStiffness = 320.f;

	/** Lower = bouncier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Guide", meta = (ClampMin = "0.0"))
	float LiftDamping = 13.f;

	/** Glow of a reachable tile under the cursor (Flash parameter). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Guide", meta = (ClampMin = "0.0"))
	float HoverGlowStrength = 0.45f;

	/** How dark unreachable tiles get (Dim parameter, 0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Guide", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DimAmount = 0.75f;

	/** Red tint of an unreachable tile under the cursor (Error parameter, 0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Guide", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InvalidHoverTint = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Guide", meta = (ClampMin = "0.05"))
	float ErrorDuration = 0.55f;

	/** Sideways wobble of the "can't go here" feedback (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Guide")
	float ErrorShake = 7.f;

	/** Scalar parameter that darkens the material (0 = normal, 1 = black). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Guide")
	FName DimParameterName = TEXT("Dim");

	/** Scalar parameter that tints the material red (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Tile|Guide")
	FName ErrorParameterName = TEXT("Error");

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hex Tile")
	FIntPoint Coord = FIntPoint::ZeroValue;

	void ApplyDefaultsAndVisuals();
	void SnapToGridNow();

	/** Top of TileMesh in its own space (where the label sits). */
	float GetLocalTopZ() const;

	/** Include an extra dynamic material in the guidance feedback (Flash / Dim / Error), e.g. an overlay. */
	void AddFeedbackMaterial(UMaterialInstanceDynamic* Material);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TintMaterials;

	/** Every material on the actor as a dynamic instance (runtime only) for Flash. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> RuntimeMaterials;

	float FlashValue = 0.f;
	float HighlightValue = 0.f;
	float HighlightTime = 0.f;
	bool bHighlighted = false;

	void SyncBoxLift();

	EHexTileGuide GuideState = EHexTileGuide::None;
	float LiftValue = 0.f;
	float LiftVelocity = 0.f;
	float DimValue = 0.f;
	float ErrorTime = -1.f;
	float LastBoxLift = 0.f;
	FVector GuideBaseLocation = FVector::ZeroVector;
	bool bHasGuideBase = false;

	FHexCard CardLabelData;
	bool bHasCardLabel = false;
	bool bLabelCovered = false;
};
