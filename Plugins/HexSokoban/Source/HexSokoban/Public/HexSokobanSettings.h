// Project Settings > Plugins > Hex Sokoban

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture2D.h"
#include "Engine/FontFace.h"
#include "HexCardTypes.h"
#include "HexSokobanSettings.generated.h"

class UWorld;
class UParticleSystem;
class UNiagaraSystem;

UCLASS(config = Game, defaultconfig, BlueprintType, meta = (DisplayName = "Hex Sokoban"))
class HEXSOKOBAN_API UHexSokobanSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override;

	// ---------------- Grid ----------------

	/** Center-to-center distance between two neighbouring tiles (world units). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1.0"))
	float TileSpacing = 220.f;

	/** World XY of axial coordinate (0,0). Every tile/box snaps to the lattice defined by this + TileSpacing. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Grid")
	FVector2D GridOrigin = FVector2D::ZeroVector;

	// ---------------- Defaults ----------------

	/** Mesh used by tiles / boxes / goal markers when their mesh component is left empty. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Defaults")
	TSoftObjectPtr<UStaticMesh> DefaultHexMesh;

	/** Material tinted per actor. Needs a vector parameter "Color"; a scalar "Flash" enables hit flashes. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Defaults")
	TSoftObjectPtr<UMaterialInterface> DefaultColorMaterial;

	/** Pawn spawned by HexSokobanGameMode when its DefaultPawnClass is left at the engine default. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Defaults")
	TSoftClassPtr<APawn> DefaultPlayerPawnClass;

	// ---------------- Levels ----------------

	/** Levels played in order (used when the GameMode's own LevelSequence is empty). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Levels")
	TArray<TSoftObjectPtr<UWorld>> LevelSequence;

	/** Main menu / level select map (AHexMenuGameMode). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Levels")
	TSoftObjectPtr<UWorld> MenuMap;

	/** Map the data-driven levels are built in (AHexSokobanGameMode, no tiles placed). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Levels")
	TSoftObjectPtr<UWorld> PlayMap;

	/** Level editor map (AHexEditorGameMode). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Levels")
	TSoftObjectPtr<UWorld> EditorMap;

	/** Actor classes the board is built from (data-driven levels and the editor). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Levels")
	TSoftClassPtr<AActor> TileClass;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Levels")
	TSoftClassPtr<AActor> GoalTileClass;

	/** Terrain tile per suit (Spade = grass, Club = rock, Diamond = road, Heart = river). A suit without an entry uses
	 *  TileClass; goal tiles always use GoalTileClass. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Levels")
	TMap<EHexSuit, TSoftClassPtr<AActor>> SuitTileClasses;

	/** Translucent overlay that gives terrain tiles the glow / dim / red guidance (their own materials don't carry
	 *  Flash / Dim / Error parameters). Scalar parameters: Flash, Dim, Error. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Levels")
	TSoftObjectPtr<UMaterialInterface> TileFeedbackOverlay;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Levels")
	TSoftClassPtr<AActor> BoxClass;

	/** Level editor: translucent material for drag previews and the build grid (param "Color", "Opacity"). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Levels")
	TSoftObjectPtr<UMaterialInterface> EditorGhostMaterial;

	/** Seconds the results card stays before the next level starts (0 = wait for a click). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Levels", meta = (ClampMin = "0.0"))
	float AutoAdvanceSeconds = 3.f;

	/** Search budget (nodes) of the card-aware solver (stars baseline, hints, editor check). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Levels", meta = (ClampMin = "1000"))
	int32 DeckSolverMaxNodes = 300000;

	// ---------------- Rules: stuck / game over ----------------

	/** After every move, prove whether the puzzle can still be solved (exact search). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Rules")
	bool bEnableStuckDetection = true;

	/** Seconds the player gets to notice (and undo) before a stuck level ends in game over. 0 = never by time. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0.0", EditCondition = "bEnableStuckDetection"))
	float StuckGameOverDelay = 2.5f;

	/** While stuck, trying to push a box that cannot move ends the level at once. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (EditCondition = "bEnableStuckDetection"))
	bool bGameOverOnBlockedPushWhenStuck = true;

	/** Seconds the game-over banner shows before the level restarts (click skips). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0.0", EditCondition = "bEnableStuckDetection"))
	float GameOverRestartDelay = 1.5f;

	/** Search budget. If exceeded the state counts as "not stuck" (never a false game over). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "100", EditCondition = "bEnableStuckDetection"))
	int32 SolverMaxStates = 200000;

	// ---------------- Cards: play a card to move ----------------

	/** Master switch. Off = move freely (no cards, no labels). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards")
	bool bEnableCardMovement = true;

	/**
	 * Cards in hand. A card fits a given tile with p = 13/40; chance the hand can enter the tile you want:
	 * 3 -> 70%, 4 -> 81%, 5 -> 88%, 6 -> 92%, 7 -> 95%. 4 by default; the Draw Director below keeps it from covering every direction.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards", meta = (ClampMin = "1", ClampMax = "10", EditCondition = "bEnableCardMovement"))
	int32 HandSize = 4;

	/**
	 * Discard zone: drop a hand card on it to throw it away, skip the turn (the pawn stays) and draw a new card.
	 * Each discard costs one charge; every card played to move refunds one, up to this maximum. 0 = no discard zone.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards", meta = (ClampMin = "0", ClampMax = "9", EditCondition = "bEnableCardMovement"))
	int32 MaxDiscards = 3;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards", meta = (EditCondition = "bEnableCardMovement"))
	EHexCardReturnPolicy ReturnPolicy = EHexCardReturnPolicy::Bottom;

	/** Goal tiles carry a label too. Off = a goal tile can be entered without playing a card. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards", meta = (EditCondition = "bEnableCardMovement"))
	bool bLabelGoalTiles = true;

	/** 0 = new random deal every time a level loads; otherwise fixed (reproducible levels). Restart always replays the same deal. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards", meta = (EditCondition = "bEnableCardMovement"))
	int32 RandomSeed = 0;

	/** Comeback 1 - Fair Deal: re-roll labels + shuffle until a winning line exists that needs no mulligan. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Assist", meta = (EditCondition = "bEnableCardMovement"))
	bool bFairDeal = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Assist", meta = (ClampMin = "1", ClampMax = "1000", EditCondition = "bEnableCardMovement && bFairDeal"))
	int32 MaxFairDealAttempts = 64;

	/** Comeback 2 - Lucky Draw: after this many "dry" draws / refused moves, the next draw fits the next tile of the optimal line. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Assist", meta = (EditCondition = "bEnableCardMovement"))
	bool bLuckyDraw = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Assist", meta = (ClampMin = "1", ClampMax = "10", EditCondition = "bEnableCardMovement && bLuckyDraw"))
	int32 LuckyDrawThreshold = 3;

	/**
	 * Anti-loop feed: when the player steps back and forth between two tiles (A-B-A-B), cards that fit the
	 * tiles around those two (the optimal line's first) go on top of the pile. Once per bouncing streak.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Assist", meta = (EditCondition = "bEnableCardMovement"))
	bool bAntiLoopFeed = true;

	/** How many cards the anti-loop feed puts on top at most. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Assist", meta = (ClampMin = "1", ClampMax = "6", EditCondition = "bEnableCardMovement && bAntiLoopFeed"))
	int32 AntiLoopFeedCards = 3;

	/** Comeback 3 - when no neighbouring tile can be entered, swap the whole hand (new hand fits the optimal line). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Assist", meta = (EditCondition = "bEnableCardMovement"))
	bool bAutoMulligan = true;

	/**
	 * Difficulty ramp - steps (cards played in the level) until Dud Draw and the Draw Director reach full strength.
	 * Step 0 deals exactly like plain top-of-pile draws (generous); in between both scale up linearly.
	 * 0 = full difficulty from the first step. Undo walks the ramp back, restart starts it over.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Draw", meta = (ClampMin = "0", ClampMax = "200", EditCondition = "bEnableCardMovement"))
	int32 DifficultyRampSteps = 15;

	/**
	 * Dud Draw - chance (0..1) that a draw is a card matching NONE of the tiles around the player
	 * (neither their suits nor their ranks). Never happens when the player would be left without a move,
	 * and never replaces a Lucky Draw.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Draw", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableCardMovement"))
	float DudDrawChance = 0.4f;

	/**
	 * Draw Director - every draw looks at the top cards of the pile and takes the one that keeps the hand's
	 * reach (share of the level's tiles it can pay for) closest to DirectorTargetCoverage. Stops the hand from
	 * opening every direction while the Lucky Draw / Mulligan still guarantee a way forward.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Draw", meta = (EditCondition = "bEnableCardMovement"))
	bool bDrawDirector = true;

	/** How many cards from the top of the pile the director may choose from (1 = plain top draw). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Draw", meta = (ClampMin = "1", ClampMax = "20", EditCondition = "bEnableCardMovement && bDrawDirector"))
	int32 DirectorLookahead = 8;

	/**
	 * Wanted share of the level's labelled tiles the hand can pay for (0..1). A random 4-card hand reaches ~75%;
	 * 0.55 (default) leaves ~1 in 4 spots fully open. Higher = easier.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Draw", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnableCardMovement && bDrawDirector"))
	float DirectorTargetCoverage = 0.55f;

	/** Masked unlit material that shows one cell of the glyph atlas (params: Atlas, Index, Tint). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Visual")
	TSoftObjectPtr<UMaterialInterface> CardGlyphMaterial;

	/** 4x4 atlas: cells 0-9 = A,2..10, cells 10-13 = spade, heart, club, diamond (white on transparent). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Visual")
	TSoftObjectPtr<UTexture2D> CardGlyphAtlas;

	/** Suit icons for the UMG cards, order: spade, heart, club, diamond (white on transparent, tinted). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Visual")
	TArray<TSoftObjectPtr<UTexture2D>> SuitIcons;

	/**
	 * Scenic card art for the UMG card faces, order: spade, heart, club, diamond - one landscape per suit,
	 * matching the suit's terrain (rock, grass, road, water). Empty entry = plain white face.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Visual")
	TArray<TSoftObjectPtr<UTexture2D>> SuitCardArt;

	// ---- Skill VFX ----

	/** Q (teleport): cast on the start tile; the player blinks when it ends and the same effect plays on arrival. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Skills|VFX")
	TSoftObjectPtr<UParticleSystem> TeleportFX;

	/** Q: longest wait (seconds) before the blink - used when the cast effect is still running or never ends (loops). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Skills|VFX", meta = (ClampMin = "0.05", ClampMax = "5.0"))
	float TeleportCastTime = 1.2f;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Skills|VFX")
	FVector TeleportFXScale = FVector(1.0, 1.0, 1.0);

	/** Q: squash-stretch-vanish time of the blink out (seconds). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Skills|VFX", meta = (ClampMin = "0.02", ClampMax = "2.0"))
	float TeleportVanishTime = 0.18f;

	/** Q: elastic pop-in time on the target tile (seconds). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Skills|VFX", meta = (ClampMin = "0.02", ClampMax = "2.0"))
	float TeleportAppearTime = 0.45f;

	/** J (wind blade): burst on the edge between the player's tile and the target box's tile. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Skills|VFX")
	TSoftObjectPtr<UNiagaraSystem> WindBladeFX;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Skills|VFX")
	FVector WindBladeFXScale = FVector(1.0, 1.0, 1.0);

	/** K (dash flight): carried by the player for the whole flight, released when it lands. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Skills|VFX")
	TSoftObjectPtr<UNiagaraSystem> FlightFX;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Skills|VFX")
	FVector FlightFXScale = FVector(1.0, 1.0, 1.0);

	/** Width of a tile label (world units). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Visual", meta = (ClampMin = "1.0"))
	float LabelSize = 80.f;

	/** Rotate tile labels so they read upright from the board camera. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Visual")
	float LabelYawOffset = 0.f;

	/** Flip label glyphs if the quad's UVs come out mirrored. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Visual")
	bool bLabelMirror = false;

	// ---- Path FX (Path Tracer Toolkit) ----

	/** Outline / path effects drawn with BP_PathTracer children (see UHexPathFXSubsystem). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX")
	bool bEnablePathFX = true;

	/** Orange loop: every tile the hand can pay for (plus the player's tile). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX", meta = (EditCondition = "bEnablePathFX"))
	TSoftClassPtr<AActor> HandRangeFXClass;

	/** Blue loop: tiles the hovered / dragged / selected card can pay for. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX", meta = (EditCondition = "bEnablePathFX"))
	TSoftClassPtr<AActor> CardRangeFXClass;

	/** Line from the player to the tile a card is dragged over. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX", meta = (EditCondition = "bEnablePathFX"))
	TSoftClassPtr<AActor> MovePathFXClass;

	/** Where a pushed box will slide. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX", meta = (EditCondition = "bEnablePathFX"))
	TSoftClassPtr<AActor> BoxPathFXClass;

	/** H: next steps of a winning line. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX", meta = (EditCondition = "bEnablePathFX"))
	TSoftClassPtr<AActor> HintPathFXClass;

	/** Ring around every goal tile. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX", meta = (EditCondition = "bEnablePathFX"))
	TSoftClassPtr<AActor> GoalRingFXClass;

	/** How far the orange loop sits inside the outer tile edges (cm). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX", meta = (EditCondition = "bEnablePathFX"))
	float HandRangeInset = 6.f;

	/** How far the blue loop sits inside the outer tile edges (cm) - keep it larger than the orange inset. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX", meta = (EditCondition = "bEnablePathFX"))
	float CardRangeInset = 24.f;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX", meta = (EditCondition = "bEnablePathFX"))
	float GoalRingInset = 36.f;

	/** Height of the effects above the tile tops (cm). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX", meta = (EditCondition = "bEnablePathFX"))
	float PathFXHeight = 3.f;

	/** Draw the effects on top of lifted tiles / the character. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX", meta = (EditCondition = "bEnablePathFX"))
	bool bPathFXVisibleThroughObjects = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX", meta = (EditCondition = "bEnablePathFX"))
	bool bShowGoalRings = true;

	/** Goal ring colour once a box sits on the goal (HDR, like the Path Tracer colours). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX", meta = (EditCondition = "bEnablePathFX && bShowGoalRings"))
	FLinearColor GoalRingDoneColor = FLinearColor(40.f, 24.f, 1.f, 1.f);

	/**
	 * Size of the target reticle (hex ring + X) drawn at the end of the move / box / hint paths, as a fraction of
	 * the tile radius. It is drawn with the path's own effect, so it shares the line width, colour and see-through fill.
	 * 0 = no reticle.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX", meta = (ClampMin = "0", ClampMax = "0.9", EditCondition = "bEnablePathFX"))
	float PathFXReticleScale = 0.42f;

	/** How many steps of the winning line the H hint shows. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Path FX", meta = (ClampMin = "1", ClampMax = "30", EditCondition = "bEnablePathFX"))
	int32 HintSteps = 4;

	/**
	 * Star rating on the results screen: turns used (steps + discards) vs. the level's optimal baseline
	 * (fewest steps, computed when the level starts). 3 stars up to baseline + max(steps, ratio x baseline) extra.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Scoring", meta = (ClampMin = "0"))
	int32 ThreeStarExtraSteps = 2;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Scoring", meta = (ClampMin = "0", ClampMax = "5"))
	float ThreeStarExtraRatio = 0.15f;

	/** 2 stars up to baseline + max(steps, ratio x baseline) extra; beyond that 1 star (a win is never 0). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Scoring", meta = (ClampMin = "0"))
	int32 TwoStarExtraSteps = 6;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Scoring", meta = (ClampMin = "0", ClampMax = "5"))
	float TwoStarExtraRatio = 0.5f;

	/** UI font, regular weight (every UMG text in the plugin uses it; empty = engine default). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSoftObjectPtr<UFontFace> UIFontRegular;

	/** UI font, bold weight. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSoftObjectPtr<UFontFace> UIFontBold;

	/** In-game HUD widget (UHexHUDWidget or a Blueprint child). Empty = native default. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (MetaClass = "/Script/HexSokoban.HexHUDWidget"))
	FSoftClassPath HUDWidgetClass;

	/** Hand widget (UHexHandWidget or a Blueprint child). Empty = native default. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Visual", meta = (MetaClass = "/Script/HexSokoban.HexHandWidget"))
	FSoftClassPath HandWidgetClass;

	/** Card widget used by the default hand (UHexCardWidget or a Blueprint child). Empty = native default. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Cards|Visual", meta = (MetaClass = "/Script/HexSokoban.HexCardWidget"))
	FSoftClassPath CardWidgetClass;

	// ---------------- Camera ----------------

	/** Use a fixed camera that frames the whole board (puzzle view) instead of the pawn's own camera. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bUseBoardCamera = true;

	/** Degrees the board camera looks down. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "10", ClampMax = "90", EditCondition = "bUseBoardCamera"))
	float BoardCameraPitch = 55.f;

	/** World yaw of the board camera. 0 = looking along +X (Forward is screen-up). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (EditCondition = "bUseBoardCamera"))
	float BoardCameraYaw = 0.f;

	/** Horizontal field of view. Narrow values give a flatter, more "diorama" look. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "5", ClampMax = "120", EditCondition = "bUseBoardCamera"))
	float BoardCameraFOV = 35.f;

	/** Extra room around the board (1 = tight). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "1.0", EditCondition = "bUseBoardCamera"))
	float BoardCameraMargin = 1.45f;

	static const UHexSokobanSettings& Get() { return *GetDefault<UHexSokobanSettings>(); }

	/** Override if set, else DefaultColorMaterial, else the engine BasicShapeMaterial. */
	static UMaterialInterface* ResolveColorMaterial(UMaterialInterface* Override);

	/** Override if set, else DefaultHexMesh. */
	static UStaticMesh* ResolveHexMesh();

	/** Actor class for a tile: GoalTileClass for goals, else the suit's terrain tile, else TileClass, else AHexTile. */
	static UClass* ResolveTileClass(bool bGoal, EHexSuit Suit);
};
