// One playing card (UMG). Scenic poker look: a cream paper card with the suit's landscape painting
// (Project Settings > Hex Sokoban > Suit Card Art) inset as the face, rank + suit on paper tabs in the
// top-left corner and upside down in the bottom-right, the suit in a medallion in the middle; red hearts /
// diamonds, black spades / clubs. Without card art the face falls back to plain white.
//
// Customise: make a Widget Blueprint child (WBP_HexCard). If its designer tree is empty the default
// card below is built for you; otherwise name any of the optional parts like the BindWidgetOptional
// members (FaceBackground, RankTopLeft, SuitTopLeft, CenterPip, DecorationSlot, ...) and they are
// driven automatically. DecorationSlot is an empty overlay for your own backgrounds / frames / effects.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HexCardTypes.h"
#include "HexCardWidget.generated.h"

class UImage;
class UTextBlock;
class UOverlay;
class UBorder;
class UTexture2D;
class UHexHandWidget;

/** While dragging: is the card over a tile it can be played on? */
UENUM(BlueprintType)
enum class EHexCardDropHint : uint8
{
	None,
	Valid,
	Invalid
};

UCLASS(Blueprintable)
class HEXSOKOBAN_API UHexCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Hex Card")
	void SetCard(const FHexCard& InCard);

	UFUNCTION(BlueprintPure, Category = "Hex Card")
	FHexCard GetCard() const { return Card; }

	UFUNCTION(BlueprintCallable, Category = "Hex Card")
	void SetVisualState(EHexCardVisualState NewState);

	UFUNCTION(BlueprintPure, Category = "Hex Card")
	EHexCardVisualState GetVisualState() const { return VisualState; }

	/** Soft outline on cards that can be played this turn. */
	UFUNCTION(BlueprintCallable, Category = "Hex Card")
	void SetPlayableHint(bool bInPlayable);

	UFUNCTION(BlueprintPure, Category = "Hex Card")
	bool IsPlayableHint() const { return bPlayableHint; }

	/** Green / red frame while the card is dragged over a valid / invalid tile. */
	UFUNCTION(BlueprintCallable, Category = "Hex Card")
	void SetDropHint(EHexCardDropHint NewHint);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Card")
	void OnDropHintChanged(EHexCardDropHint NewHint);

	/** Show the card back instead of the face (draw / flip animation). */
	UFUNCTION(BlueprintCallable, Category = "Hex Card")
	void SetFaceDown(bool bInFaceDown);

	UFUNCTION(BlueprintPure, Category = "Hex Card")
	FVector2D GetCardSize() const { return CardSize; }

	/** Show the big centre suit (the level editor's card library thumbnails use it; the hand doesn't). */
	UFUNCTION(BlueprintCallable, Category = "Hex Card")
	void SetShowCenterSuit(bool bShow) { bShowCenterSuit = bShow; RefreshVisuals(); }

	/** A new card face was set. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Card")
	void OnCardSet(const FHexCard& NewCard);

	/** Normal / Hovered / Selected / Unplayable changed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hex Card")
	void OnVisualStateChanged(EHexCardVisualState NewState);

	void SetOwningHand(UHexHandWidget* InHand);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	void BuildDefaultTree();
	void RefreshVisuals();
	UTexture2D* GetSuitTexture(EHexSuit Suit) const;
	UTexture2D* GetCardArt(EHexSuit Suit) const;

	// ---- Optional named parts (built automatically when the designer tree is empty) ----

	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UImage> ShadowImage;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UImage> FaceBackground;

	/** The suit's landscape painting, inset in the paper face. */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UImage> FaceArt;

	/** Thin paper-coloured line just inside the painting (classic scenic deck frame). */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UImage> ArtFrame;

	/** Paper disc behind the centre suit so it reads on any painting. */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UImage> CenterMedallion;

	/** Paper tabs behind the corner indices. */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> CornerTabTopLeft;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> CornerTabBottomRight;

	/** Empty overlay for your own decorations (drawn above the face, below the indices). */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> DecorationSlot;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RankTopLeft;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UImage> SuitTopLeft;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RankBottomRight;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UImage> SuitBottomRight;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UImage> CenterPip;

	/** Used instead of the suit images when no suit textures are configured. */
	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CenterSuitText;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UImage> CardBack;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UImage> HighlightFrame;

	UPROPERTY(BlueprintReadOnly, Category = "Hex Card|Parts", meta = (BindWidgetOptional))
	TObjectPtr<UImage> DisabledTint;

	// ---- Style ----

	/** Card size in slate units (standard 63:88 poker ratio). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	FVector2D CardSize = FVector2D(126.0, 176.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	float CornerRadius = 9.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	float OutlineWidth = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	FLinearColor FaceColor = FLinearColor(0.98f, 0.98f, 0.96f, 1.f);

	/** Card stock around the painting (warm paper). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	FLinearColor PaperColor = FLinearColor(0.93f, 0.88f, 0.76f, 1.f);

	/** Paper margin around the painting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	float ArtInset = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	float ArtCornerRadius = 5.f;

	/** Big suit in the middle of the face (off: the painting stays clear, the corners carry the suit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	bool bShowCenterSuit = false;

	/** Centre medallion diameter (painted cards); the suit inside is CenterArtPipSize. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	float MedallionSize = 44.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	float CenterArtPipSize = 26.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	FLinearColor OutlineColor = FLinearColor(0.12f, 0.12f, 0.14f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	FLinearColor RedColor = FLinearColor(0.72f, 0.03f, 0.03f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	FLinearColor BlackColor = FLinearColor(0.02f, 0.02f, 0.03f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	FLinearColor SelectedColor = FLinearColor(0.10f, 0.55f, 1.0f, 1.f);

	/** Outline of a card that can be played this turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	FLinearColor PlayableColor = FLinearColor(1.0f, 0.86f, 0.35f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	FLinearColor DropValidColor = FLinearColor(0.15f, 0.85f, 0.25f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	FLinearColor DropInvalidColor = FLinearColor(0.95f, 0.12f, 0.10f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	FLinearColor LuckyColor = FLinearColor(1.0f, 0.70f, 0.05f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	FLinearColor BackColor = FLinearColor(0.08f, 0.22f, 0.55f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	int32 RankFontSize = 26;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	float CornerPipSize = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card|Style")
	float CenterPipSize = 62.f;

private:
	FHexCard Card;
	bool bHasCard = false;
	bool bFaceDown = false;
	EHexCardVisualState VisualState = EHexCardVisualState::Normal;
	EHexCardDropHint DropHint = EHexCardDropHint::None;
	bool bPlayableHint = false;
	TWeakObjectPtr<UHexHandWidget> OwningHand;
};
