// Runtime level editor (editor map, AHexEditorGameMode).
//
// AHexLevelEditor owns the level being edited (FHexLevelDefinition) and its 3D presentation: real tile / goal /
// box actors, a character marker for the start, a faint grid of buildable cells, translucent drag previews that
// snap to cells with a spring, and the springy place / upgrade / downgrade animations. UHexEditorWidget is the
// UI (palette, deck, trash, rank arrows) and drives it.
//
// Drag rules (what the player picks with the mouse, topmost first: box / start, then a goal, then a bare tile):
//  - suit tile (palette)  -> empty cell: new tile (spread-out rank)      | on a tile: change its suit
//  - picked tile          -> empty cell: move                            | trash: delete | else: back home
//  - goal                 -> a non-goal tile: it upgrades to a goal tile | trash: gone (tile stays normal)
//  - box / start          -> a free tile                                 | trash: delete
// Picking a goal "lifts the goal out" of its tile (the tile downgrades to normal at once).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HexLevelTypes.h"
#include "HexLevelEditor.generated.h"

class ACameraActor;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UHexEditorWidget;
class AHexTile;

UENUM()
enum class EHexEditorItem : uint8
{
	None,
	Tile,
	Goal,
	Box,
	Start,
	Scenery
};

/** What is being dragged and where it came from. */
struct FHexEditorDrag
{
	EHexEditorItem Item = EHexEditorItem::None;
	EHexSuit Suit = EHexSuit::Spade;
	int32 Rank = 1;
	int32 SceneryVariant = 1;
	int32 SceneryRotationSteps = 0;
	/** Picked up from the board (else: new from the palette). */
	bool bFromWorld = false;
	FIntPoint Origin = FIntPoint::ZeroValue;
	/** A picked tile keeps its goal flag (only when moved as a whole). */
	bool bOriginGoal = false;
};

UCLASS()
class HEXSOKOBAN_API AHexLevelEditor : public AActor
{
	GENERATED_BODY()

public:
	AHexLevelEditor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	// ---- Model ----

	void LoadDefinition(const FHexLevelDefinition& InDefinition);
	const FHexLevelDefinition& GetDefinition() const { return Definition; }
	FHexLevelDefinition& EditDefinition() { return Definition; }

	/** Something changed: re-check solvability etc. (the widget watches the serial). */
	void NotifyChanged() { ++ChangeSerial; }
	int32 GetChangeSerial() const { return ChangeSerial; }

	bool HasTile(const FIntPoint& Cell) const { return Definition.FindTile(Cell) != nullptr; }
	bool HasBox(const FIntPoint& Cell) const { return Definition.Boxes.Contains(Cell); }
	bool IsStart(const FIntPoint& Cell) const { return Definition.bHasPlayerStart && Definition.PlayerStart == Cell; }

	// ---- Mouse (positions are read from the player controller, in viewport pixels) ----

	/** Cell under the mouse (on the tile surface plane). */
	bool GetMouseCell(FIntPoint& OutCell, FVector& OutWorld) const;

	/** Pick up whatever is under the mouse; it leaves the model until dropped (or cancelled). */
	bool PickUnderMouse(FHexEditorDrag& OutDrag);

	/** Drag preview: ghost follows the mouse and snaps to a valid cell. bOverUI hides it (the widget draws a chip). */
	void UpdateDragPreview(const FHexEditorDrag& Drag, bool bOverUI);

	/** Release over the board. Returns false if nothing valid was under the mouse (then call CancelDrag). */
	bool DropUnderMouse(const FHexEditorDrag& Drag);

	/** Put a picked item back where it came from. */
	void CancelDrag(const FHexEditorDrag& Drag);

	void HidePreview();

	/** Released over the trash: the picked item is already out of the level, just drop the preview. */
	void TrashDrag(const FHexEditorDrag& Drag);

	// ---- Hover + faces ----

	/** Track the tile under the mouse (sticky); lifts it. */
	void UpdateHover(bool bAllowed);
	bool GetHoveredTile(FIntPoint& OutCell) const;
	FVector GetLabelWorldLocation(const FIntPoint& Cell) const;

	void ChangeRank(const FIntPoint& Cell, int32 Delta);
	void RotateScenery(const FIntPoint& Cell, int32 Delta);
	void SetRank(const FIntPoint& Cell, int32 Rank);
	void SetSuit(const FIntPoint& Cell, EHexSuit Suit);

	/** Rank for a new tile: the least used so far (then least used in its suit, then unlike its neighbours). */
	int32 PickSpreadRank(EHexSuit Suit, const FIntPoint& Cell) const;

	// ---- Camera ----

	void Zoom(float Steps);
	void Pan(const FVector2D& PixelDelta);

	/** Width of the UI panel covering the right of the screen (the board is framed in the rest). */
	float RightPanelPixels = 0.f;

	ACameraActor* GetCamera() const { return Camera; }

private:
	struct FPop
	{
		TWeakObjectPtr<AActor> Actor;
		FVector BaseScale = FVector::OneVector;
		float Value = 1.f;
		float Velocity = 0.f;
		float SquashZ = 0.f;	// extra Z squash that springs back
		float SquashVelocity = 0.f;
		float Delay = 0.f;
		float Age = 0.f;
		bool bShrinkAndDestroy = false;
	};

	struct FRingPulse
	{
		TWeakObjectPtr<AActor> Actor;
		FVector Center = FVector::ZeroVector;
		float Age = 0.f;
	};

	UClass* GetTileClass(bool bGoal) const;
	AActor* SpawnTileActor(const FHexTileDef& Tile, bool bAnimate, bool bUpgrade);
	AActor* SpawnBoxActor(const FIntPoint& Cell, bool bAnimate);
	AActor* SpawnStartActor(const FIntPoint& Cell, bool bAnimate);
	void DestroyCellActor(TMap<FIntPoint, TWeakObjectPtr<AActor>>& Map, const FIntPoint& Cell, bool bAnimate);
	void RespawnTile(const FIntPoint& Cell, bool bUpgrade, bool bDowngrade);
	FVector GetSurface(const FIntPoint& Cell) const;
	void AddPop(AActor* Actor, float StartScale, float Kick, float Squash, float Delay = 0.f);
	void PulseRing(const FIntPoint& Cell);
	void RefreshGrid();
	void ApplyGhostLook(AActor* Actor, const FLinearColor& Color);
	AActor* GetGhost(EHexEditorItem Item, EHexSuit Suit, int32 Rank);
	bool IsDropValid(const FHexEditorDrag& Drag, const FIntPoint& Cell, bool& bOutOnTile) const;
	void SetHighlightTile(const FIntPoint* Cell, bool bValid);
	void UpdateCamera(float DeltaSeconds, bool bSnap);
	APlayerController* GetPC() const;

	UPROPERTY(VisibleAnywhere, Category = "Hex Editor")
	TObjectPtr<UInstancedStaticMeshComponent> Grid;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GridMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> Ghosts;	// tile, goal, box, start (one shown at a time)

	FHexLevelDefinition Definition;
	int32 ChangeSerial = 0;

	TMap<FIntPoint, TWeakObjectPtr<AActor>> TileActors;
	TMap<FIntPoint, TWeakObjectPtr<AActor>> BoxActors;
	TWeakObjectPtr<AActor> StartActor;

	TArray<FPop> Pops;
	TArray<FRingPulse> Rings;

	// Drag preview state.
	TWeakObjectPtr<AActor> ActiveGhost;
	FVector GhostLocation = FVector::ZeroVector;
	FVector GhostVelocity = FVector::ZeroVector;
	bool bGhostPlaced = false;
	TOptional<FIntPoint> HighlightCell;

	// Hover.
	TOptional<FIntPoint> HoverCell;

	// Camera.
	FVector CameraFocus = FVector::ZeroVector;
	FVector PanOffset = FVector::ZeroVector;
	float ZoomFactor = 1.f;
	FVector CameraLocation = FVector::ZeroVector;
	FVector CameraVelocity = FVector::ZeroVector;
	bool bCameraSnapped = false;

	FVector GridMeshScale = FVector::OneVector;
	double SurfaceZ = 0.0;
	bool bSurfaceKnown = false;
	double StartHeight = 90.0;
};

UCLASS()
class HEXSOKOBAN_API AHexEditorGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHexEditorGameMode();
	virtual void StartPlay() override;

	AHexLevelEditor* GetEditor() const { return Editor; }

private:
	UPROPERTY(Transient)
	TObjectPtr<AHexLevelEditor> Editor;
};

UCLASS()
class HEXSOKOBAN_API AHexEditorPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHexEditorPlayerController();
	virtual void BeginPlay() override;

	/** Called by the game mode once the editor exists. */
	void OpenEditorUI(AHexLevelEditor* InEditor);

private:
	UPROPERTY(Transient)
	TObjectPtr<UHexEditorWidget> EditorWidget;
};
