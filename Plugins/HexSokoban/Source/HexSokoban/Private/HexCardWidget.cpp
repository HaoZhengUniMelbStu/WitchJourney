#include "HexCardWidget.h"
#include "HexHandWidget.h"
#include "HexSokobanSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "HexUIStyle.h"

namespace
{
	FSlateBrush MakeRoundedBrush(const FLinearColor& Fill, const FLinearColor& Outline, float Radius, float Width)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), Width);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	UOverlaySlot* AddFill(UOverlay* Overlay, UWidget* Child, const FMargin& Padding = FMargin(0.f))
	{
		UOverlaySlot* OverlaySlot = Overlay->AddChildToOverlay(Child);
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);
		OverlaySlot->SetPadding(Padding);
		return OverlaySlot;
	}
}

void UHexCardWidget::SetOwningHand(UHexHandWidget* InHand)
{
	OwningHand = InHand;
}

TSharedRef<SWidget> UHexCardWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultTree();
	}
	TSharedRef<SWidget> Result = Super::RebuildWidget();
	RefreshVisuals();
	return Result;
}

void UHexCardWidget::BuildDefaultTree()
{
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CardRoot"));
	Root->SetWidthOverride(CardSize.X);
	Root->SetHeightOverride(CardSize.Y);
	Root->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Root;

	UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("CardStack"));
	Root->SetContent(Stack);

	ShadowImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ShadowImage"));
	ShadowImage->SetBrush(MakeRoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.35f), FLinearColor(0.f, 0.f, 0.f, 0.f), CornerRadius, 0.f));
	ShadowImage->SetRenderTranslation(FVector2D(4.0, 6.0));
	AddFill(Stack, ShadowImage);

	FaceBackground = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FaceBackground"));
	FaceBackground->SetBrush(MakeRoundedBrush(FaceColor, OutlineColor, CornerRadius, OutlineWidth));
	AddFill(Stack, FaceBackground);

	// Scenic face: the suit's painting inset in the paper, with a thin paper line just inside it.
	FaceArt = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FaceArt"));
	AddFill(Stack, FaceArt, FMargin(ArtInset));
	ArtFrame = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ArtFrame"));
	AddFill(Stack, ArtFrame, FMargin(ArtInset + 3.f));

	DecorationSlot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("DecorationSlot"));
	AddFill(Stack, DecorationSlot, FMargin(OutlineWidth));

	auto AddCentered = [Stack](UWidget* Child)
	{
		UOverlaySlot* OverlaySlot = Stack->AddChildToOverlay(Child);
		OverlaySlot->SetHorizontalAlignment(HAlign_Center);
		OverlaySlot->SetVerticalAlignment(VAlign_Center);
	};
	CenterMedallion = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CenterMedallion"));
	AddCentered(CenterMedallion);
	CenterPip = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CenterPip"));
	AddCentered(CenterPip);
	CenterSuitText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CenterSuitText"));
	AddCentered(CenterSuitText);

	auto MakeCorner = [this, Stack](const TCHAR* TabName, const TCHAR* BoxName, const TCHAR* RankName, const TCHAR* SuitName, bool bBottomRight,
		TObjectPtr<UBorder>& OutTab, TObjectPtr<UTextBlock>& OutRank, TObjectPtr<UImage>& OutSuit)
	{
		OutTab = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TabName);
		OutTab->SetPadding(FMargin(4.f, 1.f, 4.f, 3.f));
		OutTab->SetBrush(MakeRoundedBrush(FLinearColor::Transparent, FLinearColor::Transparent, 4.f, 0.f));
		UOverlaySlot* OverlaySlot = Stack->AddChildToOverlay(OutTab);
		OverlaySlot->SetHorizontalAlignment(bBottomRight ? HAlign_Right : HAlign_Left);
		OverlaySlot->SetVerticalAlignment(bBottomRight ? VAlign_Bottom : VAlign_Top);
		OverlaySlot->SetPadding(FMargin(5.f, 4.f));
		if (bBottomRight)
		{
			OutTab->SetRenderTransformAngle(180.f);
		}
		UVerticalBox* Corner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), BoxName);
		OutTab->SetContent(Corner);
		OutRank = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), RankName);
		Corner->AddChildToVerticalBox(OutRank)->SetHorizontalAlignment(HAlign_Center);
		OutSuit = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), SuitName);
		Corner->AddChildToVerticalBox(OutSuit)->SetHorizontalAlignment(HAlign_Center);
	};
	MakeCorner(TEXT("CornerTabTopLeft"), TEXT("CornerTopLeft"), TEXT("RankTopLeft"), TEXT("SuitTopLeft"), false, CornerTabTopLeft, RankTopLeft, SuitTopLeft);
	MakeCorner(TEXT("CornerTabBottomRight"), TEXT("CornerBottomRight"), TEXT("RankBottomRight"), TEXT("SuitBottomRight"), true, CornerTabBottomRight, RankBottomRight, SuitBottomRight);

	CardBack = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CardBack"));
	CardBack->SetBrush(MakeRoundedBrush(BackColor, FaceColor, CornerRadius, 5.f));
	if (UTexture2D* BackTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/FantasyRPG/Card/T_CardBack.T_CardBack")))
	{
		FSlateBrush BackBrush = MakeRoundedBrush(FLinearColor::White, FaceColor, CornerRadius, 2.f);
		BackBrush.SetResourceObject(BackTexture);
		BackBrush.ImageSize = CardSize;
		CardBack->SetBrush(BackBrush);
	}
	AddFill(Stack, CardBack);

	HighlightFrame = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("HighlightFrame"));
	AddFill(Stack, HighlightFrame, FMargin(-3.f));

	DisabledTint = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DisabledTint"));
	DisabledTint->SetBrush(MakeRoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.62f), FLinearColor(0.f, 0.f, 0.f, 0.f), CornerRadius, 0.f));
	AddFill(Stack, DisabledTint);
}

UTexture2D* UHexCardWidget::GetSuitTexture(EHexSuit Suit) const
{
	const TArray<TSoftObjectPtr<UTexture2D>>& Icons = UHexSokobanSettings::Get().SuitIcons;
	const int32 Index = static_cast<int32>(Suit);
	return (Icons.IsValidIndex(Index) && !Icons[Index].IsNull()) ? Icons[Index].LoadSynchronous() : nullptr;
}

UTexture2D* UHexCardWidget::GetCardArt(EHexSuit Suit) const
{
	if (Card.Kind == EHexCardKind::Skill)
	{
		const int32 ArtIndex = Card.Rank == 12 ? 1 : (Card.Rank == 11 ? 2 : 3);
		return LoadObject<UTexture2D>(nullptr,*FString::Printf(TEXT("/Game/FantasyRPG/Card/T_Card_Skill%02d.T_Card_Skill%02d"),ArtIndex,ArtIndex));
	}
	const TArray<TSoftObjectPtr<UTexture2D>>& Art = UHexSokobanSettings::Get().SuitCardArt;
	const int32 Index = static_cast<int32>(Suit);
	return (Art.IsValidIndex(Index) && !Art[Index].IsNull()) ? Art[Index].LoadSynchronous() : nullptr;
}

void UHexCardWidget::SetCard(const FHexCard& InCard)
{
	Card = InCard;
	bHasCard = true;
	RefreshVisuals();
	OnCardSet(Card);
}

void UHexCardWidget::SetVisualState(EHexCardVisualState NewState)
{
	if (VisualState == NewState)
	{
		return;
	}
	VisualState = NewState;
	RefreshVisuals();
	OnVisualStateChanged(VisualState);
}

void UHexCardWidget::SetPlayableHint(bool bInPlayable)
{
	if (bPlayableHint == bInPlayable)
	{
		return;
	}
	bPlayableHint = bInPlayable;
	RefreshVisuals();
}

void UHexCardWidget::SetDropHint(EHexCardDropHint NewHint)
{
	if (DropHint == NewHint)
	{
		return;
	}
	DropHint = NewHint;
	RefreshVisuals();
	OnDropHintChanged(DropHint);
}

void UHexCardWidget::SetFaceDown(bool bInFaceDown)
{
	if (bFaceDown == bInFaceDown)
	{
		return;
	}
	bFaceDown = bInFaceDown;
	RefreshVisuals();
}

void UHexCardWidget::RefreshVisuals()
{
	if (!bHasCard)
	{
		return;
	}
	const FLinearColor Ink = Card.IsRed() ? RedColor : BlackColor;
	UTexture2D* SuitTexture = GetSuitTexture(Card.Suit);
	UTexture2D* ArtTexture = GetCardArt(Card.Suit);
	const bool bArt = ArtTexture != nullptr;
	const ESlateVisibility FaceVisibility = bFaceDown ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible;
	const ESlateVisibility ArtVisibility = (bArt && !bFaceDown) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	const FLinearColor TabColor(PaperColor.R, PaperColor.G, PaperColor.B, 0.93f);

	if (FaceBackground)
	{
		FaceBackground->SetBrush(MakeRoundedBrush(bArt ? PaperColor : FaceColor, OutlineColor, CornerRadius, OutlineWidth));
	}
	if (FaceArt)
	{
		if (bArt)
		{
			// Rounded painting: a textured rounded box with a fine dark keyline, like a printed scenic deck.
			FSlateBrush ArtBrush = MakeRoundedBrush(FLinearColor::White, FLinearColor(0.20f, 0.15f, 0.10f, 0.9f), ArtCornerRadius, 1.f);
			ArtBrush.SetResourceObject(ArtTexture);
			ArtBrush.ImageSize = CardSize - FVector2D(2.0 * ArtInset, 2.0 * ArtInset);
			FaceArt->SetBrush(ArtBrush);
		}
		FaceArt->SetVisibility(ArtVisibility);
	}
	if (ArtFrame)
	{
		ArtFrame->SetBrush(MakeRoundedBrush(FLinearColor::Transparent, FLinearColor(PaperColor.R, PaperColor.G, PaperColor.B, 0.8f), FMath::Max(ArtCornerRadius - 2.f, 1.f), 1.f));
		ArtFrame->SetVisibility(ArtVisibility);
	}
	if (CenterMedallion)
	{
		FSlateBrush Disc = MakeRoundedBrush(TabColor, FLinearColor(Ink.R, Ink.G, Ink.B, 0.55f), MedallionSize * 0.5f, 1.5f);
		Disc.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		CenterMedallion->SetBrush(Disc);
		CenterMedallion->SetDesiredSizeOverride(FVector2D(MedallionSize, MedallionSize));
		CenterMedallion->SetVisibility(SuitTexture ? ArtVisibility : ESlateVisibility::Collapsed);
	}
	for (UBorder* Tab : { CornerTabTopLeft.Get(), CornerTabBottomRight.Get() })
	{
		if (Tab)
		{
			// Indices sit straight on the painting (no paper tab); a soft paper-coloured shadow keeps them legible.
			Tab->SetBrush(MakeRoundedBrush(FLinearColor::Transparent, FLinearColor::Transparent, 4.f, 0.f));
			Tab->SetVisibility(FaceVisibility);
		}
	}
	const float CenterSize = bArt ? CenterArtPipSize : CenterPipSize;
	const int32 RankSize = bArt ? FMath::RoundToInt32(RankFontSize * 0.85f) : RankFontSize;
	const float CornerSize = bArt ? CornerPipSize * 0.85f : CornerPipSize;

	for (UTextBlock* Rank : { RankTopLeft.Get(), RankBottomRight.Get() })
	{
		if (Rank)
		{
			Rank->SetText(FText::FromString(SuitTexture ? Card.GetRankText() : Card.GetRankText() + TEXT("\n") + Card.GetSuitSymbol()));
			Rank->SetFont(FHexUIStyle::Font(RankSize, true));
			Rank->SetColorAndOpacity(FSlateColor(Ink));
			Rank->SetShadowOffset(bArt ? FVector2D(1.0, 1.0) : FVector2D::ZeroVector);
			Rank->SetShadowColorAndOpacity(bArt ? FLinearColor(PaperColor.R, PaperColor.G, PaperColor.B, 0.85f) : FLinearColor::Transparent);
			Rank->SetJustification(ETextJustify::Center);
			Rank->SetVisibility(FaceVisibility);
		}
	}
	for (UImage* Suit : { SuitTopLeft.Get(), SuitBottomRight.Get() })
	{
		if (Suit)
		{
			if (SuitTexture)
			{
				Suit->SetBrushFromTexture(SuitTexture, false);
				Suit->SetDesiredSizeOverride(FVector2D(CornerSize, CornerSize));
				Suit->SetColorAndOpacity(Ink);
				Suit->SetVisibility(FaceVisibility);
			}
			else
			{
				Suit->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
	if (CenterPip)
	{
		if (SuitTexture)
		{
			CenterPip->SetBrushFromTexture(SuitTexture, false);
			CenterPip->SetDesiredSizeOverride(FVector2D(CenterSize, CenterSize));
			CenterPip->SetColorAndOpacity(Ink);
			CenterPip->SetVisibility(FaceVisibility);
		}
		else
		{
			CenterPip->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (CenterSuitText)
	{
		CenterSuitText->SetText(FText::FromString(Card.GetSuitSymbol()));
		CenterSuitText->SetFont(FHexUIStyle::Font(FMath::RoundToInt32(CenterSize * 0.9f), false));
		CenterSuitText->SetColorAndOpacity(FSlateColor(Ink));
		CenterSuitText->SetVisibility(SuitTexture || bFaceDown ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (!bShowCenterSuit)
	{
		// Scenic deck: the painting is the face; the suit lives only in the corner indices.
		for (UWidget* Center : { static_cast<UWidget*>(CenterPip.Get()), static_cast<UWidget*>(CenterSuitText.Get()), static_cast<UWidget*>(CenterMedallion.Get()) })
		{
			if (Center)
			{
				Center->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
	if (CardBack)
	{
		CardBack->SetVisibility(bFaceDown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DecorationSlot)
	{
		DecorationSlot->SetVisibility(bFaceDown ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}

	const bool bSelected = VisualState == EHexCardVisualState::Selected;
	if (HighlightFrame)
	{
		const bool bPlayableFrame = bPlayableHint && VisualState != EHexCardVisualState::Unplayable;
		const bool bShowFrame = !bFaceDown && (bSelected || Card.bLucky || DropHint != EHexCardDropHint::None || bPlayableFrame);
		FLinearColor FrameColor = bSelected ? SelectedColor : (Card.bLucky ? LuckyColor : PlayableColor);
		if (DropHint != EHexCardDropHint::None)
		{
			FrameColor = DropHint == EHexCardDropHint::Valid ? DropValidColor : DropInvalidColor;
		}
		HighlightFrame->SetBrush(MakeRoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.f), FrameColor, CornerRadius + 3.f, DropHint != EHexCardDropHint::None ? 6.f : ((bSelected || Card.bLucky) ? 4.f : 3.f)));
		HighlightFrame->SetVisibility(bShowFrame ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DisabledTint)
	{
		DisabledTint->SetVisibility(!bFaceDown && VisualState == EHexCardVisualState::Unplayable ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UHexCardWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (UHexHandWidget* Hand = OwningHand.Get())
	{
		Hand->HandleCardHovered(this, true);
	}
}

void UHexCardWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (UHexHandWidget* Hand = OwningHand.Get())
	{
		Hand->HandleCardHovered(this, false);
	}
}

FReply UHexCardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (UHexHandWidget* Hand = OwningHand.Get())
	{
		if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			Hand->HandleCardPressed(this);
			return FReply::Handled();
		}
		if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			Hand->HandleCardRightClicked(this);
			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
