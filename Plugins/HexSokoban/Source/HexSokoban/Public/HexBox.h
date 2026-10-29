// A pushable box. Occupies exactly one tile; the board owns the logic, the box owns the feel:
//   1. Push starts  -> the box waits until the pusher's front actually touches it,
//   2. Contact      -> the box is carried along by the pusher (no gap, no overlap),
//   3. Pusher stops -> the box keeps sliding and decelerates onto its target tile,
//   4. Landing      -> squash & stretch, goal colour / flash only now.
//
// Customising: make a Blueprint child, set BoxMesh's Static Mesh / materials (assigned materials are kept,
// untick "Apply Tint Color" to use the mesh's own), and attach extra components under VisualRoot so they
// squash and shove together with the box. Size/height are measured from the bounds, any pivot works.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HexSokobanTypes.h"
#include "HexBox.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class AHexTile;
class UHexCardLabelComponent;

UCLASS(Blueprintable, meta = (DisplayName = "Hex Box"))
class HEXSOKOBAN_API AHexBox : public AActor
{
	GENERATED_BODY()

public:
	AHexBox();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
#endif

	UFUNCTION(BlueprintPure, Category = "Hex Box")
	FIntPoint GetCoord() const { return Coord; }

	/** Logical goal state (true as soon as a push towards a goal is committed). */
	UFUNCTION(BlueprintPure, Category = "Hex Box")
	bool IsOnGoal() const { return bOnGoal; }

	/** Goal state as currently shown (switches when the box lands). */
	UFUNCTION(BlueprintPure, Category = "Hex Box")
	bool IsVisuallyOnGoal() const { return bVisualOnGoal; }

	/** True while waiting for contact, being carried or sliding. */
	UFUNCTION(BlueprintPure, Category = "Hex Box")
	bool IsMoving() const;

	// --- Board API (called by UHexBoardSubsystem) ---

	/** Instantly place on a tile (level start, undo, restart). */
	void PlaceOnTile(const FIntPoint& InCoord, AHexTile* Tile);

	/** Start a push: the box is carried by Pusher from contact on, then slides to Tile. */
	void BeginPush(const FIntPoint& InCoord, AHexTile* Tile, EHexDirection Direction, AActor* Pusher, float PusherRadius);
	void BeginSkillPush(const FIntPoint& InCoord, AHexTile* Tile, float Delay, bool bRebound);

	/** The pusher has stopped (end of its step). The box slides the rest of the way. */
	void ReleasePush(float PusherSpeed);

	/** Logical goal state. The visual change waits for the box to land. */
	void SetOnGoal(bool bInOnGoal);

	/** Jump to the final logical position now (undo / restart). */
	void FinishMoveImmediately();

	/** Visual lift from the tile underneath (player guidance). */
	void SetGuideLift(float WorldLift);

	/** Show the card label of the tile underneath on the box top (card-movement rule). */
	void RefreshCardLabel(bool bAnimate);

	/** Little shove when a push is refused. */
	UFUNCTION(BlueprintCallable, Category = "Hex Box")
	void PlayBlockedNudge(EHexDirection Direction);

	/** The pusher made contact and the box starts moving. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Box")
	void OnPushed(EHexDirection Direction);

	/** The box came to rest on its target tile. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Box")
	void OnLanded();

	/** A push into this box was refused (wall / another box behind it). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Box")
	void OnBlocked(EHexDirection Direction);

	/** The box visibly landed on / left a goal. Swap materials, play sounds, etc. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Box")
	void OnGoalStateChanged(bool bNowOnGoal);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Box")
	TObjectPtr<USceneComponent> Root;

	/** Everything under this squashes / shoves as one. Attach your own components here. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Box")
	TObjectPtr<USceneComponent> VisualRoot;

	/** Default: the hex mesh from Project Settings, one ring smaller than a tile. Replace freely. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Box")
	TObjectPtr<UStaticMeshComponent> BoxMesh;

	/** The covered tile's card label, lifted onto the box top (squashes with the box). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hex Box")
	TObjectPtr<UHexCardLabelComponent> CardLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box")
	bool bSnapToGrid = true;

	// ---- Push feel ----

	/** Extra gap kept between pusher and box (negative = overlap a bit). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Push")
	float ContactPadding = 0.f;

	/** Slowest speed the box leaves the pusher with (keeps short slides lively). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Push", meta = (ClampMin = "1.0"))
	float SlideMinSpeed = 300.f;

	/** Longest the final slide may take. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Push", meta = (ClampMin = "0.05"))
	float SlideMaxDuration = 0.45f;

	// ---- Squash & stretch (damped springs on VisualRoot) ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Juice", meta = (ClampMin = "1.0"))
	float SpringStiffness = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Juice", meta = (ClampMin = "0.0"))
	float SpringDamping = 13.f;

	/** Squash kick at first contact. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Juice")
	float ContactSquashImpulse = 1.2f;

	/** Squash kick when the slide ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Juice")
	float LandSquashImpulse = 1.8f;

	/** Extra kick when it lands on a goal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Juice")
	float GoalPopImpulse = 2.6f;

	/** Shove speed (cm/s) when a push is refused. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Juice")
	float BlockedNudgeImpulse = 220.f;

	// ---- Visual ----

	/** Prototype colouring: tints BoxMesh slots that have no material assigned by you. Untick to use the mesh's own materials. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Visual")
	bool bApplyTintColor = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Visual", meta = (EditCondition = "bApplyTintColor"))
	FLinearColor BoxColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Visual", meta = (EditCondition = "bApplyTintColor"))
	FLinearColor OnGoalColor;

	/** Material used for tinting (needs a vector parameter ColorParameterName). Empty = Project Settings default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Visual", meta = (EditCondition = "bApplyTintColor"))
	TObjectPtr<UMaterialInterface> TintMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Visual", meta = (EditCondition = "bApplyTintColor"))
	FName ColorParameterName = TEXT("Color");

	/** Scalar parameter pulsed when the box locks onto a goal (works on your own materials too). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Visual")
	FName FlashParameterName = TEXT("Flash");

	/** Scalar written to every material on the box: 0 = off a goal, 1 = resting on a goal. Springs in with a
	 *  small overshoot when the box locks on (cartoon pop) and eases out when it leaves. M_HexSurface uses it
	 *  to brighten the box slightly while keeping its own colours. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Visual")
	FName GoalParameterName = TEXT("GoalAmount");

	/** Spring of GoalAmount: stiffness (higher = faster) and damping (lower = more overshoot). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Visual", meta = (ClampMin = "1.0"))
	float GoalStiffness = 170.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hex Box|Visual", meta = (ClampMin = "0.0"))
	float GoalDamping = 11.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hex Box")
	FIntPoint Coord = FIntPoint::ZeroValue;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hex Box")
	bool bOnGoal = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hex Box")
	bool bVisualOnGoal = false;

private:
	enum class EPushPhase : uint8
	{
		Idle,
		Carried,	// waiting for / in contact with the pusher
		Sliding		// pusher released, decelerating to the end
		,Skill
	};

	struct FPushSegment
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		FVector Dir = FVector::ForwardVector;
		double Length = 0.0;
		double ContactDistance = 0.0;
		EHexDirection Direction = EHexDirection::Invalid;
		TWeakObjectPtr<AActor> Pusher;
		TWeakObjectPtr<AHexTile> Tile;
		TWeakObjectPtr<AHexTile> FromTile;
		bool bFromRevealed = false;
		bool bReleased = false;
		float ReleaseSpeed = 0.f;
	};

	void ApplyDefaultsAndVisuals();
	void ApplyColors();
	void ApplyVisualGoalState(bool bShowOnGoal, bool bWithEffects);
	void CacheGeometry();
	FVector GetStandLocation(const AHexTile* Tile, const FIntPoint& InCoord) const;

	void StartSegment(const FPushSegment& Segment);
	void UpdateCarried();
	void StartSlide(float Speed);
	void SetPositionAlongSegment(double Distance);
	void Land();
	void Kick(float Impulse);
	void UpdateSprings(float DeltaSeconds);
	bool SpringsAtRest() const;
	void RevealFromTileIfClear(FPushSegment& Segment, double Distance);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TintMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> RuntimeMaterials;

	EPushPhase Phase = EPushPhase::Idle;
	float SkillDelay = 0.f;
	bool bSkillRebound = false;
	FPushSegment Current;
	FPushSegment Pending;
	bool bHasPending = false;
	bool bContactMade = false;

	double Position = 0.0;		// along Current, 0..Length
	double SlideFrom = 0.0;
	double SlideSpeed = 0.0;
	double SlideDuration = 0.0;
	double SlideTime = 0.0;

	TWeakObjectPtr<AHexTile> TargetTile;

	// Geometry cache (from the bounds of everything on the actor).
	bool bGeometryCached = false;
	double BottomOffset = 0.0;
	float BoxInradius = 80.f;
	FVector VisualBaseScale = FVector::OneVector;
	FVector VisualBaseLocation = FVector::ZeroVector;
	double VisualBottomDelta = 0.0;	// world Z of the bounds bottom minus VisualRoot's Z (squash pivot)
	float BoxHeight = 100.f;			// world height (label rise distance)

	// Springs: squash (+ = flatter) and nudge (cm along NudgeDir).
	float Squash = 0.f;
	float SquashVelocity = 0.f;
	float Nudge = 0.f;
	float NudgeVelocity = 0.f;
	FVector NudgeDir = FVector::ForwardVector;
	float GoalFlash = 0.f;
	float GoalAmount = 0.f;
	float GoalAmountVelocity = 0.f;
	float GuideLift = 0.f;
};
