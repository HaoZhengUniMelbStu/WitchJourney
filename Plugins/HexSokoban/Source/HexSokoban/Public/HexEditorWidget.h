// Level editor UI (UMG, built in code, HUD look). See AHexLevelEditor for the board side.
//
// Right panel: card library (4 suits x A-10, unlimited), the level's hand (ordered, duplicates allowed, drag to
// reorder), "one-click deck", the build palette (4 suit tiles, goal, box, start), a live solvability pill,
// test play / save. Left: back, a hint bar, the trash zone (one for everything: board items and cards).
// Hovering a placed tile shows up/down arrows next to its face; wheel / digit keys change it too
// (1 then 0 quickly = 10; a lone 0 is ignored), S/C/D/H change the suit.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HexCardTypes.h"
#include "HexLevelEditor.h"
#include "HexLevelTools.h"
#include "HexEditorWidget.generated.h"

class UBorder;
class UTextBlock;
class UCanvasPanel;
class UWrapBox;
class UScrollBox;
class UUniformGridPanel;
class UEditableTextBox;
class USizeBox;
class UHexCardWidget;
class UHexMenuButton;
class UHexEditorWidget;

/** A card in the library or in the level's hand: press to drag it. */
UCLASS()
class HEXSOKOBAN_API UHexEditorCardSlot : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(UHexEditorWidget* InOwner, const FHexCard& InCard, int32 InDeckIndex, FVector2D InSize, float AppearDelay);

	FHexCard Card;
	/** Position in the level's hand, INDEX_NONE for the library. */
	int32 DeckIndex = INDEX_NONE;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(Transient) TObjectPtr<USizeBox> Box;
	UPROPERTY(Transient) TObjectPtr<UHexCardWidget> CardWidget;

	bool bAudioHovered = false;
	TWeakObjectPtr<UHexEditorWidget> Owner;
	float Scale = 0.f;
	float ScaleVelocity = 0.f;
	float Delay = 0.f;
};

/** A build item (suit tile, goal, box, start): press to drag it into the scene. */
UCLASS()
class HEXSOKOBAN_API UHexEditorPaletteItem : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(UHexEditorWidget* InOwner, EHexEditorItem InItem, EHexSuit InSuit);

	EHexEditorItem Item = EHexEditorItem::Tile;
	EHexSuit Suit = EHexSuit::Spade;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(Transient) TObjectPtr<USizeBox> Box;
	UPROPERTY(Transient) TObjectPtr<UBorder> Face;
	UPROPERTY(Transient) TObjectPtr<UBorder> Chip;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Icon;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Caption;

	bool bAudioHovered = false;
	TWeakObjectPtr<UHexEditorWidget> Owner;
	float Scale = 1.f;
	float ScaleVelocity = 0.f;
	float Lift = 0.f;
	float LiftVelocity = 0.f;
};

UCLASS()
class HEXSOKOBAN_API UHexEditorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetEditor(AHexLevelEditor* InEditor);

	// Drag starts (from the item widgets).
	FReply BeginCardDrag(UHexEditorCardSlot* CardSlot, const FPointerEvent& MouseEvent);
	FReply BeginPaletteDrag(EHexEditorItem Item, EHexSuit Suit, const FPointerEvent& MouseEvent);

	/** Chip look of a build item (palette + drag chip share it). */
	static void GetItemLook(EHexEditorItem Item, EHexSuit Suit, FString& OutIcon, FString& OutCaption, FLinearColor& OutChip, FLinearColor& OutIconColor);

	UHexCardWidget* MakeCardWidget(const FHexCard& Card);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	enum class EDragKind : uint8 { None, Board, Card, Pan };

	void BuildTree();
	void BuildRightPanel(UCanvasPanel* Root);
	void RebuildDeckView(int32 PopFrom = INDEX_NONE, bool bStagger = false);
	void UpdateDragVisual(const FVector2D& AbsoluteMouse);
	void FinishDrag(const FVector2D& AbsoluteMouse);
	void CancelAll();
	bool IsOver(const UWidget* Widget, const FVector2D& AbsoluteMouse) const;
	bool IsOverUI(const FVector2D& AbsoluteMouse) const;
	int32 GetDeckInsertIndex(const FVector2D& AbsoluteMouse, FVector2D& OutMarkerLocal) const;
	FVector2D AbsoluteToRoot(const FVector2D& Absolute) const;
	void ShowToast(const FString& Message, bool bError = false);
	void UpdateStatus(float DeltaTime);
	void UpdateRankArrows(float DeltaTime);
	void UpdateTrash(float DeltaTime, const FVector2D& AbsoluteMouse);
	void Save(bool bBuiltIn);
	void TestPlay();
	void AutoDeck();
	void RefreshDeckTitle();
	FHexLevelDefinition GetDefinitionForSave() const;

	TWeakObjectPtr<AHexLevelEditor> Editor;

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient) TObjectPtr<UBorder> RightPanel;
	UPROPERTY(Transient) TObjectPtr<UUniformGridPanel> PickerGrid;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> DeckScroll;
	UPROPERTY(Transient) TObjectPtr<UWrapBox> DeckWrap;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DeckTitle;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> NameBox;
	UPROPERTY(Transient) TObjectPtr<UBorder> StatusPill;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UBorder> Trash;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TrashTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TrashSub;
	UPROPERTY(Transient) TObjectPtr<UBorder> Toast;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ToastText;
	UPROPERTY(Transient) TObjectPtr<UHexMenuButton> RankUp;
	UPROPERTY(Transient) TObjectPtr<UHexMenuButton> RankDown;
	UPROPERTY(Transient) TObjectPtr<UBorder> DragChip;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DragChipIcon;
	UPROPERTY(Transient) TObjectPtr<USizeBox> DragCardBox;
	UPROPERTY(Transient) TObjectPtr<UHexCardWidget> DragCard;
	UPROPERTY(Transient) TObjectPtr<UBorder> InsertMarker;
	UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> UIBlockers;	// widgets that count as "over the UI"

	// Drag state.
	EDragKind DragKind = EDragKind::None;
	FHexEditorDrag BoardDrag;
	FHexCard CardDragCard;
	int32 CardDragFrom = INDEX_NONE;
	FVector2D DragChipPos = FVector2D::ZeroVector;
	FVector2D DragChipVelocity = FVector2D::ZeroVector;
	FVector2D LastMouseAbsolute = FVector2D::ZeroVector;
	bool bOverTrash = false;
	float TrashScale = 1.f;
	float TrashScaleVelocity = 0.f;
	float TrashFlash = 0.f;

	// Rank keys.
	float PendingOne = 0.f;
	FIntPoint PendingOneCell = FIntPoint::ZeroValue;

	// Arrows.
	float ArrowAlpha = 0.f;
	FIntPoint ArrowCell = FIntPoint::ZeroValue;

	// Solvability check.
	int32 CheckedSerial = -1;
	int32 RequestedSerial = -1;
	float CheckDelay = 0.f;
	bool bCheckRunning = false;
	FHexLevelCheck LastCheck;
	int32 LastCheckSerial = -1;

	float ToastTime = 0.f;
	float EscapeArmed = 0.f;
	int32 ShownDeckCount = -1;
};
