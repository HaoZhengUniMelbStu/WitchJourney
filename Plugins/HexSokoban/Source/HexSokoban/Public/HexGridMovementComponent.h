// Put on the player pawn (HexSokobanPlayerController adds it automatically if missing).
// Moves the pawn exactly one tile per step, center to center, and pushes boxes via the board.
// With a Character owner it walks with CharacterMovement so the run animation plays;
// otherwise it interpolates the actor.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HexSokobanTypes.h"
#include "HexGridMovementComponent.generated.h"

class UHexBoardSubsystem;
class UHexCardSubsystem;
class ACharacter;
class UParticleSystemComponent;
class UNiagaraComponent;

UCLASS(ClassGroup = (HexSokoban), meta = (BlueprintSpawnableComponent, DisplayName = "Hex Grid Movement"))
class HEXSOKOBAN_API UHexGridMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHexGridMovementComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Try one step in a hex direction. Pushes a box if one is in the way and can move. */
	UFUNCTION(BlueprintCallable, Category = "Hex Movement")
	EHexMoveResult TryMove(EHexDirection Direction);
	UFUNCTION(BlueprintCallable, Category = "Hex Movement")
	EHexMoveResult TryPlaySkillAt(FIntPoint Target);

	/** Step towards a world point (e.g. mouse click): snaps the direction to the nearest of six. */
	UFUNCTION(BlueprintCallable, Category = "Hex Movement")
	EHexMoveResult TryMoveTowardsLocation(const FVector& WorldLocation);

	/** Step along a world yaw, snapped to the nearest hex direction. */
	UFUNCTION(BlueprintCallable, Category = "Hex Movement")
	EHexMoveResult TryMoveByYaw(float WorldYaw);

	UFUNCTION(BlueprintPure, Category = "Hex Movement")
	FIntPoint GetCoord() const { return Coord; }

	UFUNCTION(BlueprintPure, Category = "Hex Movement")
	bool IsOnBoard() const { return bHasCoord; }

	UFUNCTION(BlueprintPure, Category = "Hex Movement")
	bool IsMoving() const { return bMoving; }

	UFUNCTION(BlueprintPure, Category = "Hex Movement")
	bool IsMovementLocked() const { return bLocked; }

	UFUNCTION(BlueprintCallable, Category = "Hex Movement")
	void SetMovementLocked(bool bInLocked);

	/** Puts the pawn on the tile under it (or the nearest tile). Called automatically at BeginPlay. */
	UFUNCTION(BlueprintCallable, Category = "Hex Movement")
	void SnapToNearestTile();

	/** Instantly place the pawn on a tile (undo / restart). */
	void TeleportToCoord(const FIntPoint& InCoord);

	/** Complete the current step right now. bNotify=false skips OnStepFinished / win checks. */
	void FinishStepImmediately(bool bNotify);

	UPROPERTY(BlueprintAssignable, Category = "Hex Movement")
	FHexMoveSignature OnStepStarted;

	UPROPERTY(BlueprintAssignable, Category = "Hex Movement")
	FHexMoveSignature OnStepFinished;

	/** A step was refused; the pawn stayed where it was. */
	UPROPERTY(BlueprintAssignable, Category = "Hex Movement")
	FHexMoveBlockedSignature OnMoveBlocked;

	/** Walk with CharacterMovement (animations, collision) when the owner is a Character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Movement")
	bool bUseCharacterMovement = true;

	/** Step time when interpolating (non-Character owners, or bUseCharacterMovement off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Movement", meta = (ClampMin = "0.01"))
	float InterpStepDuration = 0.25f;

	/** Character mode: distance at which the step counts as arrived. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Movement", meta = (ClampMin = "0.1"))
	float ArriveTolerance = 5.f;

	/** Character mode: a step that takes longer than this is snapped to its target (never get stuck). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Movement", meta = (ClampMin = "0.1"))
	float StepTimeout = 1.5f;

	/** Remember one input made during a step and run it right after. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Movement")
	bool bBufferInput = true;

	/** Turn the pawn to face a blocked direction as feedback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Movement")
	bool bFaceBlockedDirection = true;

	/** Walk speed multiplier while pushing a box (Character mode): pushing feels heavier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Movement|Push", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float PushSpeedMultiplier = 0.7f;

	/** Distance from the pawn's center to its front when touching a box. -1 = 75% of the capsule radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Movement|Push")
	float PushContactRadius = -1.f;

	/** Tile flash when the pawn lands on it (0 = off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Movement", meta = (ClampMin = "0.0"))
	float LandFlashStrength = 0.15f;

	/** Clicks closer than this fraction of TileSpacing to the current tile center are ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IgnoreClickRadius = 0.3f;

private:
	UHexBoardSubsystem* GetBoard() const;
	UHexCardSubsystem* GetCards() const;
	ACharacter* GetMovingCharacter() const;
	FVector GetStandLocation(const FIntPoint& InCoord) const;
	void BeginStep(const FHexMoveRecord& Record);
	void FinishStep(bool bNotify);
	void FaceDirection(EHexDirection Direction);
	float GetPusherRadius() const;
	void ApplyPushSpeed(bool bPushing);

	// ---- Skill VFX (Project Settings > Hex Sokoban > Skills|VFX) ----
	/** Ground point (tile surface) under a cell. */
	FVector GetCellSurface(const FIntPoint& InCoord) const;
	/**
	 * Q (instant transmission): cast circle on the start tile -> the pawn squashes, stretches into a thin streak and
	 * vanishes -> circle on the target tile -> when it ends the pawn snaps back in with an elastic pop.
	 */
	void BeginTeleportCast();
	void UpdateTeleport(float DeltaTime);
	/** Q: everything played, finish the step. */
	void CompleteTeleport();
	/** Squash / stretch of the pawn's visuals (mesh for a Character) around its feet. */
	void SetBlinkScale(float ScaleXY, float ScaleZ);
	/** Back to normal size and visible (end of the blink, undo, restart). */
	void ResetBlinkVisual();
	UParticleSystemComponent* SpawnTeleportFX(const FVector& Location) const;
	/** J: wind blade on the edge between the caster and the box. */
	void PlayWindBladeFX(const FHexMoveRecord& Record) const;
	/** K: attach the flight trail / stop it (lets the particles finish). */
	void StartFlightFX();
	void StopFlightFX();

	UFUNCTION()
	void HandleTeleportCastFinished(UParticleSystemComponent* FinishedComponent);

	/** Q is waiting for its cast effect before the blink. */
	bool bTeleportPending = false;
	/** 1 = start-tile cast, 2 = vanish, 3 = target-tile cast, 4 = appear. */
	uint8 TeleportPhase = 0;
	float TeleportPhaseTime = 0.f;
	float TeleportWait = 0.f;
	double TeleportStartTime = 0.0;
	FVector BlinkBaseScale = FVector::OneVector;
	bool bBlinkScaled = false;

	UPROPERTY(Transient)
	TObjectPtr<UParticleSystemComponent> TeleportCastComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> FlightFXComponent;

	FIntPoint Coord = FIntPoint::ZeroValue;
	bool bHasCoord = false;
	bool bMoving = false;
	bool bLocked = false;

	UPROPERTY(Transient)
	FHexMoveRecord CurrentStep;

	FVector StepStart = FVector::ZeroVector;
	FVector StepTarget = FVector::ZeroVector;
	float StepElapsed = 0.f;
	EHexDirection BufferedDirection = EHexDirection::Invalid;

	/** The current step was paid with a card (draw one when it finishes). */
	bool bStepPaid = false;

	/** Original MaxWalkSpeed while a push slowdown is active (< 0 = none). */
	float SavedMaxWalkSpeed = -1.f;

	/** Actor Z above the tile surface (bottom of the actor bounds). */
	double HeightAboveSurface = 0.0;
};
