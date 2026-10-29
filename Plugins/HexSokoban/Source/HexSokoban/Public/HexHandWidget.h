// The player's hand (UMG), Slay-the-Spire style: cards fanned along an arc at the bottom centre,
// hover lifts and enlarges a card and spreads its neighbours, click selects (the reachable tiles for
// that card glow), drag a card onto a glowing neighbour tile to play it. Played cards fly to their tile,
// new cards are dealt from the deck pile (bottom right); refused moves shake the hand.
// Deck pile: a dark tray holding a stack of card backs (thickness = cards left) with the NEXT card face up
// on top (smaller than hand cards, tagged "next"): drawing slides it into the hand and flips the next one.
// Discard zone (bottom left, mirrors the deck): drop a card there to throw it away and skip the turn.
//
// Everything is driven by UHexCardSubsystem events, so the widget holds no game state of its own.
// Customise: Widget Blueprint child (WBP_HexHand). Optional named parts: HandCanvas (CanvasPanel root),
// DeckCountText. Card look comes from CardWidgetClass (Project Settings > Hex Sokoban > Cards).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HexCardTypes.h"
#include "HexHandWidget.generated.h"

class UCanvasPanel;
class UTextBlock;
class UBorder;
class UHorizontalBox;
class UHexCardWidget;
class UHexCardSubsystem;
class AHexTile;

UCLASS(Blueprintable)
class HEXSOKOBAN_API UHexHandWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Card widget class used for every card (empty = Project Settings, then native default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand")
	TSubclassOf<UHexCardWidget> CardWidgetClass;

	// ---- Layout (slate units, relative to the bottom centre of the screen) ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float HandBaseY = -105.f;

	/** Horizontal distance between card centres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float CardSpacing = 108.f;

	/** The fan never gets wider than this; spacing shrinks instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float MaxHandWidth = 760.f;

	/** Rotation per card away from the centre (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float FanAnglePerCard = 6.f;

	/** The outermost card never tilts more than this, however big the hand (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float MaxFanAngle = 16.f;

	/** How far outer cards drop along the arc (per (offset)^2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float ArcDrop = 9.f;

	/** ...but never more than this at the ends, so big hands stay on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float MaxArcDrop = 40.f;

	/** Free space kept at each side of the screen (the deck / discard zone live there in random mode). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float HandSideMargin = 150.f;

	/** Hands bigger than this shrink their cards (down to MinHandScale) instead of overlapping too much. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	int32 CrowdedHandCount = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float MinHandScale = 0.76f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float HoverLift = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float HoverScale = 1.22f;

	/** Neighbours of the hovered card move this far aside. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float HoverSpread = 34.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float SelectLift = 40.f;

	/** Cards that can be played this turn sit this much higher than the rest... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float PlayableLift = 16.f;

	/** ...and cards that can't are drawn this much smaller. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float UnplayableScale = 0.94f;

	/** Discard zone centre, offset from the bottom-left corner (mirrors the deck pile). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	FVector2D DiscardOffset = FVector2D(110.0, -138.0);

	/** Extra catch radius around the discard zone for drops (slate units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float DiscardCatchMargin = 18.f;

	/** Deck pile offset from the bottom-right corner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	FVector2D DeckOffset = FVector2D(-110.0, -138.0);

	/** Pile cards are drawn this much smaller than hand cards, so the face-up next card never reads as held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	float DeckScale = 0.84f;

	/** One card back under the top per this many cards left (max 4): the pile visibly thins out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Layout")
	int32 CardsPerDeckLayer = 8;

	// ---- Feel ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Feel")
	float SpringStiffness = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Feel")
	float SpringDamping = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Feel")
	float PlayFlyDuration = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Feel")
	float DrawFlipDuration = 0.22f;

	/** Delay between cards when several are dealt at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Feel")
	float DealStagger = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Feel")
	float ShakeAmplitude = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Feel")
	float ShakeDuration = 0.32f;

	/** How long a guidance message stays on screen (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Guide")
	float GuideMessageDuration = 2.4f;

	/** Guidance message position above the bottom centre (slate units, negative = up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Guide")
	float GuideMessageY = -300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Guide")
	FLinearColor GuideMessageColor = FLinearColor(0.42f, 0.04f, 0.04f, 0.88f);

	/** Mouse travel (slate units) before a press turns into a drag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Hand|Feel")
	float DragThreshold = 14.f;

	// ---- Card widget callbacks ----

	void HandleCardHovered(UHexCardWidget* Card, bool bHovered);
	void HandleCardPressed(UHexCardWidget* Card);
	void HandleCardRightClicked(UHexCardWidget* Card);

	/** Select hand card by index (keyboard 1..N). Selecting the selected card deselects it. */
	UFUNCTION(BlueprintCallable, Category = "Hex Hand")
	void SelectHandIndex(int32 HandIndex);

	UFUNCTION(BlueprintCallable, Category = "Hex Hand")
	void ClearSelection();

	/** Pop a short guidance line above the hand (why a move was refused, ...). */
	UFUNCTION(BlueprintCallable, Category = "Hex Hand|Guide")
	void ShowGuideMessage(const FText& Message);

	/** True while a card is pressed / dragged (the controller then ignores board clicks). */
	UFUNCTION(BlueprintPure, Category = "Hex Hand")
	bool IsInteracting() const { return PressedCard.IsValid(); }

	/** Is the mouse over the hand area (cards)? */
	UFUNCTION(BlueprintPure, Category = "Hex Hand")
	bool IsMouseOverHand() const { return HoveredCard.IsValid(); }

	/** Inspect the same card as fan hover, with clicked selection as a fallback. */
	UFUNCTION(BlueprintPure, Category = "Hex Hand")
	bool GetInspectedCard(FHexCard& OutCard) const;

	/** Is the cursor over the discard zone? */
	UFUNCTION(BlueprintPure, Category = "Hex Hand")
	bool IsMouseOverDiscard() const { return IsOverDiscard(GetMouseInHandSpace()); }

	// ---- Blueprint hooks ----

	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Hand")
	void OnCardPlayedFx(const FHexCard& Card, AHexTile* Tile);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Hand")
	void OnCardDrawnFx(const FHexCard& Card);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Hand")
	void OnMulliganFx();

	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Hand")
	void OnCardDiscardedFx(const FHexCard& Card);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Hand")
	void OnRejectedFx();

	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Hand|Guide")
	void OnGuideMessageFx(const FText& Message);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Hand|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> HandCanvas;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Hand|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DeckCountText;

	/** Guidance message box + text. */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Hand|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> GuideBox;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Hand|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GuideText;

	/** Discard zone (card-sized panel) with its title, sub line and charge pips; built at runtime when absent. */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Hand|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> DiscardZone;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Hand|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DiscardTitle;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Hand|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DiscardSubText;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Hand|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> DiscardPips;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Hand|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DiscardLabel;

private:
	struct FSpring2D
	{
		FVector2D Value = FVector2D::ZeroVector;
		FVector2D Velocity = FVector2D::ZeroVector;
	};
	struct FSpring1D
	{
		float Value = 0.f;
		float Velocity = 0.f;
	};

	enum class EEntryMode : uint8
	{
		InHand,
		Drawing,		// waiting / flipping in from the deck
		FlyToTile,		// played
		ReturnToDeck	// undo / mulligan
	};

	struct FCardEntry
	{
		TWeakObjectPtr<UHexCardWidget> Widget;
		FHexCard Card;
		EEntryMode Mode = EEntryMode::InHand;
		FSpring2D Pos;
		FSpring1D Angle;
		FSpring1D Scale;
		float Delay = 0.f;		// before the draw starts
		float FlipTime = 0.f;	// draw flip progress (seconds)
		float FlyTime = 0.f;
		FVector2D FlyFrom = FVector2D::ZeroVector;
		FVector2D FlyTo = FVector2D::ZeroVector;
		float FlyAngleFrom = 0.f;
		float Opacity = 1.f;
		float FlipScaleX = 1.f;
		bool bPlayable = true;
		float ModeTime = 0.f;
		bool bDead = false;
	};

	UHexCardSubsystem* GetCards() const;
	void BuildDefaultTree();
	void BindToCards();
	void UnbindFromCards();

	UFUNCTION()
	void HandleHandChanged();

	UFUNCTION()
	void HandleHandDealt();

	UFUNCTION()
	void HandleCardPlayed(const FHexCard& Card, AHexTile* TargetTile);

	UFUNCTION()
	void HandleCardDrawn(const FHexCard& Card);

	UFUNCTION()
	void HandleMulligan();

	UFUNCTION()
	void HandleSelectionChanged();

	UFUNCTION()
	void HandlePlayRejected();

	UFUNCTION()
	void HandleGuideMessage(const FText& Message, AHexTile* Tile);

	UFUNCTION()
	void HandleCardDiscarded(const FHexCard& Card);

	UFUNCTION()
	void HandleDiscardChargesChanged();

	void BuildDiscardZone();
	void UpdateDiscardZone(float DeltaTime, const FVector2D& Mouse);
	void RefreshDiscardPips();
	bool IsOverDiscard(const FVector2D& Mouse) const;
	bool TryDiscardEntry(int32 EntryIndex);
	FVector2D GetDiscardPosition() const;
	FVector2D GetDiscardSize() const;

	void UpdateGuide();
	void UpdateGuideMessage(float DeltaTime);
	int32 GetHandIndexOfWidget(const UHexCardWidget* Widget) const;
	bool IsPlayerBusy() const;

	void Reconcile(bool bDealAll);
	UHexCardWidget* CreateCardWidget(const FHexCard& Card);
	int32 FindEntryByFace(const FHexCard& Card, bool bOnlyLive) const;
	int32 FindEntryByWidget(const UHexCardWidget* Widget) const;

	void UpdateHover(const FVector2D& Mouse);
	void UpdatePress(const FVector2D& Mouse);
	void UpdateEntries(float DeltaTime);
	void UpdateVisualStates();
	void ApplyEntryToWidget(FCardEntry& Entry, int32 ZOrder, const FVector2D& Shake);
	bool TryPlayEntryAtMouse(int32 EntryIndex);
	AHexTile* GetTileUnderMouse() const;

	/** Hand-slot layout for live hand card SlotIndex of Count (ignores hover). */
	void GetRestLayout(int32 SlotIndex, int32 Count, FVector2D& OutPos, float& OutAngle) const;

	/** Resting card scale for a hand of Count cards (big hands shrink). */
	float GetHandScale(int32 Count) const;
	FVector2D GetMouseInHandSpace() const;
	FVector2D GetViewportSizeInSlate() const;
	FVector2D GetDeckPosition() const;
	FVector2D WorldToHandSpace(const FVector& World, bool& bOutOk) const;
	TArray<int32> GetLiveEntriesInHandOrder() const;

	void StepSpring(FSpring2D& Spring, const FVector2D& Target, float DeltaTime) const;
	void StepSpring(FSpring1D& Spring, float Target, float DeltaTime) const;

	TArray<FCardEntry> Entries;
	TWeakObjectPtr<UHexCardSubsystem> BoundCards;

	UPROPERTY(Transient)
	TObjectPtr<UHexCardWidget> DeckWidget;	// face-up next card on top of the pile

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHexCardWidget>> DeckLayers;	// card backs under it

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DeckTray;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> NextTag;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHexCardWidget>> CardWidgets;	// keeps widgets alive while animating

	TWeakObjectPtr<UHexCardWidget> HoveredCard;
	TWeakObjectPtr<UHexCardWidget> PressedCard;
	bool bDragging = false;
	FVector2D PressMouse = FVector2D::ZeroVector;
	FVector2D DragOffset = FVector2D::ZeroVector;

	float ShakeTime = 0.f;
	float GuideMessageTime = 0.f;
	float GuideMessageAge = 0.f;
	float StateRefreshTimer = 0.f;
	float PendingDealDelay = 0.f;
	bool bPendingReconcile = false;
	bool bPendingDealAll = false;
	float ReconcileDelay = 0.f;

	// Deck pile presentation.
	void BuildDeckPile();
	void UpdateDeckPile(float DeltaTime);
	FHexCard ShownPeek;
	bool bShownPeekValid = false;
	float PeekFlipTime = 10.f;

	// Discard zone presentation.
	int32 DiscardVisual = -1;
	int32 ShownDiscardCharges = -1;
	int32 ShownDiscardMax = -1;
	float DiscardPulse = 0.f;	// pip-row pop after a charge change
	float DiscardClock = 0.f;
	FSpring1D DiscardScale;
};
