#include "HexMenuWidgets.h"
#include "HexLocalization.h"
#include "HexAudioSubsystem.h"
#include "HexUIStyle.h"
#include "HexGameFlowSubsystem.h"
#include "HexSokobanGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Engine/Texture2D.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace HexMenu
{
	FText Txt(const TCHAR* Text) { return FText::FromString(Text); }

	UTextBlock* MakeText(UWidgetTree* Tree, int32 Size, bool bBold, const FLinearColor& Color, ETextJustify::Type Justify = ETextJustify::Left)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetFont(FHexUIStyle::Font(Size, bBold));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(Justify);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	}

	UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Fill, float Radius, const FMargin& Padding)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Border->SetBrush(FHexUIStyle::Panel(Fill, Radius));
		Border->SetPadding(Padding);
		Border->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return Border;
	}

	/** Invisible button chrome: the face border inside draws everything. */
	/** Pressed buttons show the grabbing hand (they capture the mouse, so this is what Slate asks while held). */
	void UpdateButtonCursor(UButton* Button)
	{
		if (Button)
		{
			const EMouseCursor::Type Wanted = Button->IsPressed() ? EMouseCursor::GrabHandClosed : EMouseCursor::Hand;
			if (Button->GetCursor() != Wanted)
			{
				Button->SetCursor(Wanted);
				if (FSlateApplication::IsInitialized())
				{
					FSlateApplication::Get().QueryCursor();	// a press doesn't move the mouse: refresh now
				}
			}
		}
	}

	UButton* MakeBareButton(UWidgetTree* Tree)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		FButtonStyle Style;
		FSlateBrush None;
		None.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.SetNormal(None);
		Style.SetHovered(None);
		Style.SetPressed(None);
		Style.SetDisabled(None);
		Style.SetNormalPadding(FMargin(0.f));
		Style.SetPressedPadding(FMargin(0.f));
		Button->SetStyle(Style);
		Button->SetCursor(EMouseCursor::Hand);	// hover = hand; pressed = grab (see UpdateButtonCursor)
		return Button;
	}

	void Spring(float& Value, float& Velocity, float Target, float DeltaTime, float Stiffness = 420.f, float Damping = 24.f)
	{
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(DeltaTime / (1.f / 120.f)));
		const float Dt = DeltaTime / static_cast<float>(Steps);
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			Velocity += ((Target - Value) * Stiffness - Velocity * Damping) * Dt;
			Value += Velocity * Dt;
		}
	}

	/** Kit star (stage select style): gold when earned, slate when not. */
	UImage* MakeStar(UWidgetTree* Tree, bool bLit, float Size)
	{
		UImage* Star = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		Star->SetBrush(FHexUIStyle::Image(bLit ? TEXT("06_Stage/stage_star") : TEXT("06_Stage/stage_star_dim"), FVector2D(Size, Size * 53.0 / 48.0)));
		Star->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Star;
	}

	/** Resting tint of each button plate (hover / press multiply on top of it). */
	TMap<TWeakObjectPtr<UBorder>, FLinearColor>& PlateTints()
	{
		static TMap<TWeakObjectPtr<UBorder>, FLinearColor> Tints;
		return Tints;
	}

	/** A 9-slice kit panel with its content padding. */
	UBorder* MakeSkinPanel(UWidgetTree* Tree, EHexSkin Skin, float Scale, const FMargin& Extra = FMargin(0.f))
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Border->SetBrush(FHexUIStyle::Skin(Skin, Scale));
		const FMargin Pad = FHexUIStyle::SkinPadding(Skin, Scale);
		Border->SetPadding(FMargin(Pad.Left + Extra.Left, Pad.Top + Extra.Top, Pad.Right + Extra.Right, Pad.Bottom + Extra.Bottom));
		Border->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return Border;
	}

	/** Title on a kit ribbon: outlined ivory text centred on the banner. The banner never shrinks below the art's own size. */
	UWidget* MakeRibbonTitle(UWidgetTree* Tree, EHexSkin Ribbon, float Scale, const FText& Text, int32 FontSize)
	{
		USizeBox* Frame = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		const FSlateBrush Art = FHexUIStyle::Skin(Ribbon, Scale);
		Frame->SetWidthOverride(Art.ImageSize.X);
		Frame->SetHeightOverride(Art.ImageSize.Y);
		Frame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UBorder* Banner = MakeSkinPanel(Tree, Ribbon, Scale);
		Frame->AddChild(Banner);
		Banner->SetHorizontalAlignment(HAlign_Fill);
		Banner->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Title = MakeText(Tree, FontSize, true, FHexUIStyle::Ink, ETextJustify::Center);
		Title->SetFont(FHexUIStyle::OutlinedFont(FontSize, 2, FLinearColor(0.10f, 0.03f, 0.25f, 1.f)));
		Title->SetShadowOffset(FVector2D(0.0, 2.0));
		Title->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f));
		Title->SetText(Text);
		if (Ribbon == EHexSkin::RibbonRed)
		{
			// Keep the instruction readable; longer translations wrap instead of shrinking.
			Frame->SetWidthOverride(560.f);
			Frame->SetHeightOverride(100.f);
			// The painted front panel sits above the ribbon's lower tails and shadow.
			Banner->SetPadding(FMargin(60.f, 7.f, 60.f, 19.f));
			Title->SetWrapTextAt(440.f);
			Banner->SetContent(Title);
			return Frame;
		}
		UScaleBox* Fit=Tree->ConstructWidget<UScaleBox>();
		Fit->SetStretch(EStretch::ScaleToFit);
		Fit->SetStretchDirection(EStretchDirection::DownOnly);
		Fit->SetContent(Title);
		Banner->SetContent(Fit);
		return Frame;
	}

	/** Thin ornamental divider from the kit. */
	UImage* MakeDecoLine(UWidgetTree* Tree, const TCHAR* KitPath, const FVector2D& Size)
	{
		UImage* Line = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		Line->SetBrush(FHexUIStyle::Image(KitPath, Size));
		Line->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Line;
	}
}

// ================================================================ button

void UHexMenuButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	WidgetTree->RootWidget = Box;

	Button = HexMenu::MakeBareButton(WidgetTree);
	Button->OnClicked.AddDynamic(this, &UHexMenuButton::HandleClicked);
	Button->OnPressed.AddDynamic(this, &UHexMenuButton::HandlePressAudio);
	Box->AddChild(Button);

	Face = HexMenu::MakePanel(WidgetTree, FHexUIStyle::PanelLight, 14.f, FMargin(0.f));
	Face->SetHorizontalAlignment(HAlign_Fill);
	Face->SetVerticalAlignment(VAlign_Fill);
	if (UButtonSlot* FaceSlot = Cast<UButtonSlot>(Button->AddChild(Face)))
	{
		FaceSlot->SetPadding(FMargin(0.f));
		FaceSlot->SetHorizontalAlignment(HAlign_Fill);
		FaceSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Face->SetContent(Row);
	StripeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	StripeBox->SetWidthOverride(7.f);
	Stripe = HexMenu::MakePanel(WidgetTree, FHexUIStyle::Accent, 3.5f, FMargin(0.f));
	StripeBox->AddChild(Stripe);
	UHorizontalBoxSlot* StripeSlot = Row->AddChildToHorizontalBox(StripeBox);
	StripeSlot->SetVerticalAlignment(VAlign_Fill);
	StripeSlot->SetPadding(FMargin(8.f, 9.f, 0.f, 9.f));
	Label = HexMenu::MakeText(WidgetTree, 21, true, FHexUIStyle::Ink, ETextJustify::Center);
	UScaleBox* LabelFit = WidgetTree->ConstructWidget<UScaleBox>();
	LabelFit->SetStretch(EStretch::ScaleToFit);
	LabelFit->SetStretchDirection(EStretchDirection::DownOnly);
	LabelFit->SetContent(Label);
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelFit);
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	LabelSlot->SetHorizontalAlignment(HAlign_Fill);
	LabelSlot->SetPadding(FMargin(4.f, 0.f, 15.f, 0.f));
	ApplyStyle();
}

void UHexMenuButton::Setup(const FText& InLabel, EHexButtonStyle InStyle, float InWidth, float InHeight, TFunction<void()> InOnClicked)
{
	Style = InStyle;
	Width = InWidth;
	Height = InHeight;
	OnClickedFunc = MoveTemp(InOnClicked);
	SetLabel(InLabel);
	ApplyStyle();
}

void UHexMenuButton::SetLabel(const FText& InLabel)
{
	if (Label)
	{
		Label->SetText(InLabel);
	}
}

void UHexMenuButton::ApplyStyle()
{
	if (!Box || !Face || !Stripe || !Label || !StripeBox)
	{
		return;
	}
	// Width is a minimum: the plate grows to wrap its label instead of letting the text spill over the bevel.
	Box->ClearWidthOverride();
	Box->SetMinDesiredWidth(Width);
	Box->SetHeightOverride(Height);
	StripeBox->SetVisibility(ESlateVisibility::Collapsed);	// the kit buttons carry their own colour

	// Arrow buttons (the editor's rank arrows, label "\u25b2"): the kit's page arrow, rotated by the owner.
	const bool bArrow = Label->GetText().ToString() == TEXT("\u25b2");
	if (bArrow)
	{
		Face->SetBrush(FHexUIStyle::Image(TEXT("06_Stage/page_arrow_prev"), FVector2D(Height * 0.92f, Height * 0.92f * 99.0 / 64.0)));
		Face->SetPadding(FMargin(0.f));
		Label->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Label->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Small buttons (tool rows, back, auto-deck) use the kit's compact rounded plates: the chunky 180x111 buttons
	// squashed to 30-40 px lose their bevel and stop reading as buttons.
	const bool bCompact = Height <= 52.f;
	EHexSkin Skin = bCompact ? EHexSkin::ChipDim : EHexSkin::ButtonPurple;
	FLinearColor TextColor = FHexUIStyle::Ink;
	FLinearColor PlateTint = FLinearColor::White;
	// Compact controls share the thin plates used by auto-deck and back buttons.
	if (bCompact && Style == EHexButtonStyle::Secondary)
	{
		Skin = EHexSkin::ChipDim;
	}
	switch (Style)
	{
	case EHexButtonStyle::Primary:
		Skin = bCompact ? EHexSkin::ChipYellow : EHexSkin::ButtonYellow;
		TextColor = FHexUIStyle::InkDark;
		break;
	case EHexButtonStyle::Danger:
		Skin = bCompact ? EHexSkin::ChipYellow : EHexSkin::ButtonRed;
		if (bCompact)
		{
			PlateTint = FLinearColor(0.95f, 0.13f, 0.12f, 1.f);	// red
		}
		break;
	case EHexButtonStyle::Dark:
		Skin = bCompact ? EHexSkin::ChipNormal : EHexSkin::ButtonDark;
		break;
	default:
		break;
	}
	// Frame thickness follows the button height (button art is 111 px tall, the compact plates 239 px).
	const float SkinScale = bCompact ? FMath::Clamp(Height / 88.f, 0.3f, 0.5f) : FMath::Clamp(Height / 104.f, 0.34f, 0.72f);
	Face->SetBrush(FHexUIStyle::Skin(Skin, SkinScale, PlateTint));
	HexMenu::PlateTints().Add(TWeakObjectPtr<UBorder>(Face.Get()), PlateTint);
	const FMargin Pad = FHexUIStyle::SkinPadding(Skin, SkinScale);
	Face->SetPadding(FMargin(0.f, 0.f, 0.f, Pad.Bottom - Pad.Top));	// keep the label above the drop shadow
	Label->SetColorAndOpacity(FSlateColor(TextColor));
	const int32 FontSize = Height >= 56.f ? 21 : (Height >= 46.f ? 18 : (Height >= 38.f ? 16 : 14));
	if (UHorizontalBoxSlot* LabelSlot = Cast<UHorizontalBoxSlot>(Label->Slot))
	{
		const float Side = FMath::Max(14.f, Height * 0.42f);	// breathing room inside the bevel
		LabelSlot->SetPadding(FMargin(Side, 0.f, Side, 0.f));
	}
	FSlateFontInfo LabelFont = FHexUIStyle::Font(FontSize, true);
	if (Style != EHexButtonStyle::Primary)
	{
		LabelFont.OutlineSettings.OutlineSize = 1;
		LabelFont.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.55f);
	}
	Label->SetFont(LabelFont);
}

void UHexMenuButton::HandlePressAudio()
{
    UHexAudioSubsystem::PlayExact(this, TEXT("Click_06"));
}

void UHexMenuButton::HandleClicked()
{
	ScaleVelocity -= 3.f;	// a squash on click
	if (OnClickedFunc)
	{
		OnClickedFunc();
	}
}

void UHexMenuButton::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Button || !Box)
	{
		return;
	}
	HexMenu::UpdateButtonCursor(Button);
	const float DeltaTime = FMath::Min(InDeltaTime, 0.05f);
	const bool bHovered = Button->IsHovered();
	if (bHovered && !bAudioHovered) UHexAudioSubsystem::PlayExact(this, TEXT("Click_08"), 0.35f);
    bAudioHovered = bHovered;
	const bool bPressed = Button->IsPressed();
	HexMenu::Spring(Scale, ScaleVelocity, bPressed ? 0.95f : (bHovered ? 1.04f : 1.f), DeltaTime);
	HexMenu::Spring(Lift, LiftVelocity, bHovered && !bPressed ? -3.f : 0.f, DeltaTime);
	Box->SetRenderScale(FVector2D(Scale, Scale));
	Box->SetRenderTranslation(FVector2D(0.0, Lift));
	if (Face)
	{
		const float Glow = bPressed ? 0.88f : (bHovered ? 1.14f : 1.f);	// kit buttons brighten on hover, dip on press
		const FLinearColor* Found = HexMenu::PlateTints().Find(TWeakObjectPtr<UBorder>(Face.Get()));
		const FLinearColor Base = Found ? *Found : FLinearColor::White;
		Face->SetBrushColor(FLinearColor(Base.R * Glow, Base.G * Glow, Base.B * Glow, 1.f));
	}
}

// ================================================================ level card

void UHexLevelCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(232.f);
	Box->SetHeightOverride(266.f);
	Box->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	WidgetTree->RootWidget = Box;

	Button = HexMenu::MakeBareButton(WidgetTree);
	Button->OnClicked.AddDynamic(this, &UHexLevelCardWidget::HandleClicked);
	Button->OnPressed.AddDynamic(this, &UHexLevelCardWidget::HandlePressAudio);
	Box->AddChild(Button);
	Face = HexMenu::MakeSkinPanel(WidgetTree, EHexSkin::FrameDefault, 0.9f);
	if (UButtonSlot* FaceSlot = Cast<UButtonSlot>(Button->AddChild(Face)))
	{
		FaceSlot->SetPadding(FMargin(0.f));
		FaceSlot->SetHorizontalAlignment(HAlign_Fill);
		FaceSlot->SetVerticalAlignment(VAlign_Fill);
	}
	// Stripe is kept (hidden) only as a flag: its opacity says "cleared" for the hover frame swap.
	Stripe = HexMenu::MakePanel(WidgetTree, FHexUIStyle::Accent, 3.5f, FMargin(0.f));
	Stripe->SetVisibility(ESlateVisibility::Collapsed);

	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Face->SetContent(Layers);
	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UOverlaySlot* BodySlot = Layers->AddChildToOverlay(Body))
	{
		BodySlot->SetHorizontalAlignment(HAlign_Fill);
		BodySlot->SetVerticalAlignment(VAlign_Center);
	}
	Layers->AddChildToOverlay(Stripe);
	CustomTag = HexMenu::MakeSkinPanel(WidgetTree, EHexSkin::ChipDim, 0.32f);
	CustomTag->SetPadding(FMargin(10.f, 3.f));
	UTextBlock* TagText = HexMenu::MakeText(WidgetTree, 12, true, FHexUIStyle::Ink);
	TagText->SetText(HexMenu::Txt(HEXT("自制")));	// custom
	CustomTag->SetContent(TagText);
	if (UOverlaySlot* TagSlot = Layers->AddChildToOverlay(CustomTag))
	{
		TagSlot->SetHorizontalAlignment(HAlign_Right);
		TagSlot->SetVerticalAlignment(VAlign_Top);
		TagSlot->SetPadding(FMargin(0.f, -2.f, -2.f, 0.f));	// inside the frame, clear of the hover glow
	}

	NumberText = HexMenu::MakeText(WidgetTree, 44, true, FHexUIStyle::Ink, ETextJustify::Center);
	NumberText->SetFont(FHexUIStyle::OutlinedFont(44, 2));
	Body->AddChildToVerticalBox(NumberText)->SetHorizontalAlignment(HAlign_Center);
	NameText = HexMenu::MakeText(WidgetTree, 15, true, FHexUIStyle::InkSoft, ETextJustify::Center);
	NameText->SetClipping(EWidgetClipping::ClipToBounds);
	Body->AddChildToVerticalBox(NameText)->SetHorizontalAlignment(HAlign_Center);
	StarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* StarSlot = Body->AddChildToVerticalBox(StarRow);
	StarSlot->SetHorizontalAlignment(HAlign_Center);
	StarSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	BestText = HexMenu::MakeText(WidgetTree, 13, false, FHexUIStyle::InkSoft, ETextJustify::Center);
	Body->AddChildToVerticalBox(BestText)->SetHorizontalAlignment(HAlign_Center);
	ToolRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* ToolSlot = Body->AddChildToVerticalBox(ToolRow);
	ToolSlot->SetHorizontalAlignment(HAlign_Center);
	ToolSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	Box->SetRenderScale(FVector2D(0.0, 0.0));
}

void UHexLevelCardWidget::Setup(int32 InNumber, const FHexLevelEntry& Entry, const FHexLevelRecord& Record, float AppearDelay)
{
	Delay = AppearDelay;
    bPlayedAppearSound = false;
	if (NumberText)
	{
		NumberText->SetText(FText::FromString(FString::Printf(TEXT("%02d"), InNumber)));
	}
	if (NameText)
	{
		NameText->SetText(FText::FromString(Entry.Definition.DisplayName.IsEmpty() ? Entry.Definition.LevelId.Left(12) : FHexLocalization::LevelName(Entry.Definition.LevelId,Entry.Definition.DisplayName,Entry.bBuiltIn)));
	}
	if (Stripe)
	{
		Stripe->SetRenderOpacity(Record.bCompleted ? 1.f : 0.5f);	// flag read by the hover frame swap
	}
	if (Face)
	{
		Face->SetBrush(FHexUIStyle::Skin(Record.bCompleted ? EHexSkin::FrameComplete : EHexSkin::FrameDefault, 0.9f));
	}
	if (CustomTag)
	{
		CustomTag->SetVisibility(Entry.bBuiltIn ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (StarRow)
	{
		StarRow->ClearChildren();
		for (int32 Index = 0; Index < 3; ++Index)
		{
			UHorizontalBoxSlot* StarCell = StarRow->AddChildToHorizontalBox(HexMenu::MakeStar(WidgetTree, Index < Record.BestStars, Index == 1 ? 34.f : 30.f));
			StarCell->SetPadding(FMargin(1.f, Index == 1 ? 0.f : 4.f, 1.f, 0.f));
			StarCell->SetVerticalAlignment(VAlign_Top);
		}
	}
	if (BestText)
	{
		BestText->SetText(Record.bCompleted
			? FText::FromString(FHexLocalization::Printf(HEXT("最佳 %d 回合"), Record.BestTurns))	// best N turns
			: HexMenu::Txt(HEXT("未通关")));														// not cleared
	}
	if (ToolRow)
	{
		ToolRow->ClearChildren();
		// Built-in levels can be edited (and re-saved as built-in) only inside the Unreal editor; never deleted here.
		const bool bEditable = !Entry.bBuiltIn || WITH_EDITOR;
		ToolRow->SetVisibility(bEditable ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bEditable)
		{
			APlayerController* PC = GetOwningPlayer();
			if (UHexMenuButton* Edit = PC ? CreateWidget<UHexMenuButton>(PC, UHexMenuButton::StaticClass()) : nullptr)
			{
				Edit->Setup(HexMenu::Txt(HEXT("编辑")), EHexButtonStyle::Secondary, 70.f, 32.f, [this]() { if (OnEdit) { OnEdit(); } });
				ToolRow->AddChildToHorizontalBox(Edit)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			}
			UHexMenuButton* Delete = (PC && !Entry.bBuiltIn) ? CreateWidget<UHexMenuButton>(PC, UHexMenuButton::StaticClass()) : nullptr;
			if (Delete)
			{
				Delete->Setup(HexMenu::Txt(HEXT("删除")), EHexButtonStyle::Danger, 70.f, 32.f, [this]() { if (OnDelete) { OnDelete(); } });
				ToolRow->AddChildToHorizontalBox(Delete);
			}
		}
	}
}

void UHexLevelCardWidget::HandlePressAudio()
{
    UHexAudioSubsystem::PlayExact(this, TEXT("Click_06"));
}

void UHexLevelCardWidget::HandleClicked()
{
	ScaleVelocity -= 3.f;
	if (OnPlay)
	{
		OnPlay();
	}
}

void UHexLevelCardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	HexMenu::UpdateButtonCursor(Button);
	const float DeltaTime = FMath::Min(InDeltaTime, 0.05f);
	if (Delay > 0.f)
	{
		Delay -= DeltaTime;
		return;
	}
	if (!bPlayedAppearSound)
    {
        bPlayedAppearSound = true;
        UHexAudioSubsystem::Play(this, TEXT("Pop"));
    }
	const bool bHovered = Button && Button->IsHovered();
	if (bHovered && !bAudioHovered) UHexAudioSubsystem::PlayExact(this, TEXT("Click_08"), 0.35f);
    bAudioHovered = bHovered;
	const bool bPressed = Button && Button->IsPressed();
	HexMenu::Spring(Scale, ScaleVelocity, bPressed ? 0.96f : (bHovered ? 1.05f : 1.f), DeltaTime, 360.f, 20.f);
	HexMenu::Spring(Lift, LiftVelocity, bHovered ? -5.f : 0.f, DeltaTime);
	if (Face && Stripe)
	{
		// Hover: the kit's gold "current" frame.
		const bool bCleared = Stripe->GetRenderOpacity() > 0.75f;
		Face->SetBrush(FHexUIStyle::Skin(bHovered ? EHexSkin::FrameCurrent : (bCleared ? EHexSkin::FrameComplete : EHexSkin::FrameDefault), 0.9f));
	}
	if (UWidget* Root = WidgetTree ? WidgetTree->RootWidget : nullptr)
	{
		Root->SetRenderScale(FVector2D(FMath::Max(0.f, Scale), FMath::Max(0.f, Scale)));
		Root->SetRenderTranslation(FVector2D(0.0, Lift));
	}
}

// ================================================================ main menu + level select

void UHexMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MenuCanvas"));
	WidgetTree->RootWidget = Root;
	BuildMain(Root);
	BuildSelect(Root);
}

void UHexMenuWidget::BuildMain(UCanvasPanel* Root)
{
    Portrait = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("MenuPortrait"));
    // ScaleToFitX implicitly clips to the box in SScaleBox::OnPaint. On wide
    // windows that cuts the hat inside the screen. Match its scale explicitly
    // with UserSpecified, which allows the artwork to extend outside the box.
    Portrait->SetStretch(EStretch::UserSpecified);
    Portrait->SetRenderTransformPivot(FVector2D(0.5, 0.5));
    Portrait->SetRenderScale(FVector2D(1.25, 1.25)); // Fixed enlargement, never animated.
    Portrait->SetPixelSnapping(EWidgetPixelSnapping::Disabled);
    Portrait->SetVisibility(ESlateVisibility::HitTestInvisible);
    UImage* Art = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
    Art->SetPixelSnapping(EWidgetPixelSnapping::Disabled);
    Art->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, TEXT("/Game/FantasyRPG/Image/CharacterImage.CharacterImage")), true);
    Portrait->AddChild(Art);
    UCanvasPanelSlot* ArtSlot = Root->AddChildToCanvas(Portrait);
    ArtSlot->SetAnchors(FAnchors(1.f / 3.f, 0.f, 1.f, 1.f));
    ArtSlot->SetOffsets(FMargin(0.f));
    ArtSlot->SetZOrder(0);

	MainPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainPanel"));
	UCanvasPanelSlot* MainSlot = Root->AddChildToCanvas(MainPanel);
	MainSlot->SetZOrder(10);
	MainSlot->SetAnchors(FAnchors(0.f, 0.5f));
	MainSlot->SetAlignment(FVector2D(0.0, 0.5));
	MainSlot->SetPosition(FVector2D(96.0, -20.0));
	MainSlot->SetAutoSize(true);

	// Title: the kit's chunky outlined logo type over a red ribbon.
	UVerticalBox* TitleBlock = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MainPanel->AddChildToVerticalBox(TitleBlock)->SetHorizontalAlignment(HAlign_Center);
	UTextBlock* Title = HexMenu::MakeText(WidgetTree, 84, true, FHexUIStyle::Accent, ETextJustify::Center);
	const int32 TitleSize=FHexLocalization::GetLanguage()==2?36:FHexLocalization::GetLanguage()==1?68:84;
	Title->SetFont(FHexUIStyle::OutlinedFont(TitleSize, 5, FLinearColor(0.09f, 0.04f, 0.16f, 1.f)));
	Title->SetShadowOffset(FVector2D(0.0, 6.0));
	Title->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	Title->SetText(HexMenu::Txt(HEXT("魔女之旅")));
	TitleBlock->AddChildToVerticalBox(Title)->SetHorizontalAlignment(HAlign_Center);
	UWidget* Subtitle = HexMenu::MakeRibbonTitle(WidgetTree, EHexSkin::RibbonRed, 0.62f,
		HexMenu::Txt(HEXT("出一张同花色或同点数的牌，走一步")), 17);
	TitleBlock->AddChildToVerticalBox(Subtitle)->SetHorizontalAlignment(HAlign_Center);

	APlayerController* PC = GetOwningPlayer();
	auto AddButton = [this, PC](const TCHAR* Text, EHexButtonStyle ButtonStyle, float Top, TFunction<void()> Action) -> UHexMenuButton*
	{
		UHexMenuButton* Button = PC ? CreateWidget<UHexMenuButton>(PC, UHexMenuButton::StaticClass()) : nullptr;
		if (Button)
		{
			Button->Setup(HexMenu::Txt(Text), ButtonStyle, 340.f, 66.f, MoveTemp(Action));
			UVerticalBoxSlot* ButtonSlot = MainPanel->AddChildToVerticalBox(Button);
			ButtonSlot->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		}
		return Button;
	};
	StartButton = AddButton(HEXT("开始游戏"), EHexButtonStyle::Primary, 30.f, [this]()
	{
		// Play on from the first level not cleared yet (all cleared: from the top).
		if (UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this))
		{
			for (int32 Index = 0; Index < Flow->GetNumLevels(); ++Index)
			{
				const FString& Id = Flow->GetLevelEntry(Index)->Definition.LevelId;
				if (!Flow->GetRecord(Id).bCompleted)
				{
					Flow->PlayLevel(Id);
					return;
				}
			}
			if (Flow->GetNumLevels() > 0)
			{
				Flow->PlayLevel(Flow->GetLevelEntry(0)->Definition.LevelId);
			}
		}
	});
	AddButton(HEXT("选择关卡"), EHexButtonStyle::Secondary, 10.f, [this]() { ShowScreen(EHexMenuScreen::LevelSelect); });
	AddButton(HEXT("关卡编辑器"), EHexButtonStyle::Secondary, 10.f, [this]()
	{
		if (UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this))
		{
			Flow->OpenEditor(FString());
		}
	});
	AddButton(HEXT("语言"), EHexButtonStyle::Secondary, 10.f, [this]()
	{
		FHexLocalization::Cycle();
		if(auto* Flow=UHexGameFlowSubsystem::Get(this)) Flow->OpenMenu(EHexMenuScreen::Main);
	});
	AddButton(HEXT("退出游戏"), EHexButtonStyle::Danger, 10.f, [this]()
	{
		if (UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this))
		{
			Flow->QuitGame();
		}
	});
}

void UHexMenuWidget::BuildSelect(UCanvasPanel* Root)
{
	SelectPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SelectPanel"));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(SelectPanel);
	PanelSlot->SetZOrder(20);
	PanelSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	PanelSlot->SetOffsets(FMargin(0.f));
	SelectPanel->SetVisibility(ESlateVisibility::Collapsed);

	UBorder* Dim = HexMenu::MakePanel(WidgetTree, FHexUIStyle::ScreenDim, 0.f, FMargin(0.f));
	Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* DimSlot = SelectPanel->AddChildToCanvas(Dim);
	DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	DimSlot->SetOffsets(FMargin(0.f));

	APlayerController* PC = GetOwningPlayer();
	if (UHexMenuButton* Back = PC ? CreateWidget<UHexMenuButton>(PC, UHexMenuButton::StaticClass()) : nullptr)
	{
		Back->Setup(HexMenu::Txt(HEXT("返回")), EHexButtonStyle::Dark, 132.f, 44.f, [this]() { ShowScreen(EHexMenuScreen::Main); });
		UCanvasPanelSlot* BackSlot = SelectPanel->AddChildToCanvas(Back);
		BackSlot->SetPosition(FVector2D(44.0, 30.0));
		BackSlot->SetAutoSize(true);
	}

	UVerticalBox* TitleBody = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UCanvasPanelSlot* TitleSlot = SelectPanel->AddChildToCanvas(TitleBody);
	TitleSlot->SetPosition(FVector2D(196.0, 26.0));
	TitleSlot->SetAutoSize(true);
	UTextBlock* Title = HexMenu::MakeText(WidgetTree, 40, true, FHexUIStyle::Ink);
	Title->SetFont(FHexUIStyle::OutlinedFont(40, 2));
	Title->SetShadowOffset(FVector2D(0.0, 3.0));
	Title->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f));
	Title->SetText(HexMenu::Txt(HEXT("选择关卡")));
	TitleBody->AddChildToVerticalBox(Title);
	SelectSubtitle = HexMenu::MakeText(WidgetTree, 16, true, FHexUIStyle::Accent);
	TitleBody->AddChildToVerticalBox(SelectSubtitle);
	UImage* Deco = HexMenu::MakeDecoLine(WidgetTree, TEXT("00_Common/common_title_deco_line"), FVector2D(1920.0, 66.0));
	UCanvasPanelSlot* DecoSlot = SelectPanel->AddChildToCanvas(Deco);
	DecoSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.f));
	DecoSlot->SetOffsets(FMargin(0.f, 150.f, 0.f, 50.f));

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UCanvasPanelSlot* ScrollSlot = SelectPanel->AddChildToCanvas(Scroll);
	ScrollSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	ScrollSlot->SetOffsets(FMargin(60.f, 218.f, 60.f, 110.f));
	LevelGrid = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	LevelGrid->SetInnerSlotPadding(FVector2D(22.0, 22.0));
	// Keep hover and entrance scaling inside the scroll viewport's clipping bounds.
	UScrollBoxSlot* GridScrollSlot = CastChecked<UScrollBoxSlot>(Scroll->AddChild(LevelGrid));
	GridScrollSlot->SetPadding(FMargin(16.f));

	if (UHexMenuButton* New = PC ? CreateWidget<UHexMenuButton>(PC, UHexMenuButton::StaticClass()) : nullptr)
	{
		New->Setup(HexMenu::Txt(HEXT("新建关卡")), EHexButtonStyle::Primary, 220.f, 56.f, [this]()
		{
			if (UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this))
			{
				Flow->OpenEditor(FString());
			}
		});
		UCanvasPanelSlot* NewSlot = SelectPanel->AddChildToCanvas(New);
		NewSlot->SetAnchors(FAnchors(1.f, 1.f));
		NewSlot->SetAlignment(FVector2D(1.0, 1.0));
		NewSlot->SetPosition(FVector2D(-44.0, -32.0));
		NewSlot->SetAutoSize(true);
	}
}

void UHexMenuWidget::RefreshLevelGrid()
{
	UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this);
	APlayerController* PC = GetOwningPlayer();
	if (!LevelGrid || !Flow || !PC)
	{
		return;
	}
	LevelGrid->ClearChildren();
	int32 Stars = 0;
	for (int32 Index = 0; Index < Flow->GetNumLevels(); ++Index)
	{
		const FHexLevelEntry& Entry = *Flow->GetLevelEntry(Index);
		const FHexLevelRecord Record = Flow->GetRecord(Entry.Definition.LevelId);
		Stars += Record.BestStars;
		UHexLevelCardWidget* Card = CreateWidget<UHexLevelCardWidget>(PC, UHexLevelCardWidget::StaticClass());
		if (!Card)
		{
			continue;
		}
		Card->Setup(Index + 1, Entry, Record, 0.04f * static_cast<float>(Index));
		const FString Id = Entry.Definition.LevelId;
		Card->OnPlay = [this, Id]()
		{
			if (UHexGameFlowSubsystem* FlowSys = UHexGameFlowSubsystem::Get(this))
			{
				FlowSys->PlayLevel(Id);
			}
		};
		Card->OnEdit = [this, Id]()
		{
			if (UHexGameFlowSubsystem* FlowSys = UHexGameFlowSubsystem::Get(this))
			{
				FlowSys->OpenEditor(Id);
			}
		};
		Card->OnDelete = [this, Id]()
		{
			if (UHexGameFlowSubsystem* FlowSys = UHexGameFlowSubsystem::Get(this))
			{
				FlowSys->DeleteCustomLevel(Id);
				RefreshLevelGrid();
			}
		};
		LevelGrid->AddChildToWrapBox(Card);
	}
	if (SelectSubtitle)
	{
		SelectSubtitle->SetText(FText::FromString(FHexLocalization::Printf(HEXT("共 %d 关　·　已获得 %d / %d 颗星"),
			Flow->GetNumLevels(), Stars, Flow->GetNumLevels() * 3)));
	}
}

void UHexMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	const UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this);
	if (StartButton && Flow)
	{
		bool bAny = false;
		for (int32 Index = 0; Index < Flow->GetNumLevels() && !bAny; ++Index)
		{
			bAny = Flow->GetRecord(Flow->GetLevelEntry(Index)->Definition.LevelId).bCompleted;
		}
		StartButton->SetLabel(HexMenu::Txt(bAny ? HEXT("继续游戏") : HEXT("开始游戏")));	// continue / start
	}
	const EHexMenuScreen Screen = Flow ? Flow->GetRequestedMenuScreen() : EHexMenuScreen::Main;
	MainAlpha = 0.f;	// fade the first screen in
	ShowScreen(Screen);
	SetKeyboardFocus();
}

void UHexMenuWidget::ShowScreen(EHexMenuScreen Screen)
{
	Current = Screen;
    if (auto* Audio = UHexAudioSubsystem::Get(this)) Audio->SetMenuQuiet(Screen == EHexMenuScreen::LevelSelect);
	if (Screen == EHexMenuScreen::LevelSelect)
	{
		RefreshLevelGrid();
	}
}

FReply UHexMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape && Current == EHexMenuScreen::LevelSelect)
	{
		ShowScreen(EHexMenuScreen::Main);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UHexMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float DeltaTime = FMath::Min(InDeltaTime, 0.05f);
	// Eased cross-slide between the two screens.
	MainAlpha = FMath::FInterpTo(MainAlpha, Current == EHexMenuScreen::Main ? 1.f : 0.f, DeltaTime, 10.f);
	SelectAlpha = FMath::FInterpTo(SelectAlpha, Current == EHexMenuScreen::LevelSelect ? 1.f : 0.f, DeltaTime, 10.f);
	if (Portrait)
	{
		Portrait->SetRenderOpacity(MainAlpha);
		Portrait->SetVisibility(MainAlpha > 0.01f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		// Bounded pendulum motion: no accumulated rotation and no animated scaling.
		const double Time = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
		// Roughly 25-31 second cycles, matching the menu camera's 0.25 rad/s sway.
		// Size the image from the portrait's two-thirds viewport width without a local clip.
		// Offset by one seventh of the enlarged image height, independent of DPI/aspect ratio.
		const FVector2D ImageSize = Portrait->GetContent() ? Portrait->GetContent()->GetDesiredSize() : FVector2D(1.0, 1.0);
		Portrait->SetUserSpecifiedScale(MyGeometry.GetLocalSize().X * (2.0 / 3.0) / FMath::Max(1.0, ImageSize.X));
		const double ImageHeight = MyGeometry.GetLocalSize().X * (2.0 / 3.0) * 1.25 * ImageSize.Y / FMath::Max(1.0, ImageSize.X);
		Portrait->SetRenderTranslation(FVector2D(8.0 * FMath::Sin(Time * 0.22), ImageHeight / 7.0 + 10.0 * FMath::Sin(Time * 0.25)));
		Portrait->SetRenderTransformAngle(1.0f * FMath::Sin(Time * 0.20));
	}
	if (MainPanel)
	{
		MainPanel->SetVisibility(MainAlpha > 0.01f ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		MainPanel->SetRenderOpacity(MainAlpha);
		MainPanel->SetRenderTranslation(FVector2D(-70.0 * (1.0 - MainAlpha), 0.0));
	}
	if (SelectPanel)
	{
		SelectPanel->SetVisibility(SelectAlpha > 0.01f ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		SelectPanel->SetRenderOpacity(SelectAlpha);
		SelectPanel->SetRenderTranslation(FVector2D(0.0, 40.0 * (1.0 - SelectAlpha)));
	}
}

// ================================================================ pause menu

void UHexPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UHexAudioSubsystem::PlayExact(this, TEXT("draw_card2"));
}

void UHexPauseMenuWidget::NativeDestruct()
{
	if (GetWorld() && !GetWorld()->bIsTearingDown)
	{
		UHexAudioSubsystem::PlayExact(this, TEXT("draw_card1"));
	}
	Super::NativeDestruct();
}

void UHexPauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PauseCanvas"));
	WidgetTree->RootWidget = Root;

	Overlay = HexMenu::MakePanel(WidgetTree, FHexUIStyle::ScreenDim, 0.f, FMargin(0.f));
	Overlay->SetVisibility(ESlateVisibility::Visible);	// swallows clicks on the board
	UCanvasPanelSlot* OverlaySlot = Root->AddChildToCanvas(Overlay);
	OverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	OverlaySlot->SetOffsets(FMargin(0.f));

	Card = HexMenu::MakePanel(WidgetTree, FHexUIStyle::PanelLight, 0.f,
		FHexUIStyle::SkinPadding(EHexSkin::Popup, 0.4f) + FMargin(26.f, 8.f, 26.f, 10.f));
	Card->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(Card);
	CardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	CardSlot->SetAlignment(FVector2D(0.5, 0.5));
	CardSlot->SetAutoSize(true);

	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Body);
	FHexUIStyle::DecoratePanel(WidgetTree, Card);
	UWidget* TitleRibbon = HexMenu::MakeRibbonTitle(WidgetTree, EHexSkin::Ribbon, 0.8f, HexMenu::Txt(HEXT("暂停")), 34);	// paused
	UVerticalBoxSlot* RibbonSlot = Body->AddChildToVerticalBox(TitleRibbon);
	RibbonSlot->SetHorizontalAlignment(HAlign_Center);
	RibbonSlot->SetPadding(FMargin(-40.f, -48.f, -40.f, 0.f));	// the ribbon rides over the frame's top edge
	const AHexSokobanGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHexSokobanGameMode>() : nullptr;
	UTextBlock* LevelName = HexMenu::MakeText(WidgetTree, 16, true, FHexUIStyle::Accent, ETextJustify::Center);
	LevelName->SetText(FText::FromString(GameMode ? GameMode->GetLevelDisplayName() : FString()));
	Body->AddChildToVerticalBox(LevelName)->SetHorizontalAlignment(HAlign_Center);
	UVerticalBoxSlot* DecoSlot = Body->AddChildToVerticalBox(HexMenu::MakeDecoLine(WidgetTree, TEXT("08_PauseContinue/pause_deco_line"), FVector2D(300.0, 6.0)));
	DecoSlot->SetHorizontalAlignment(HAlign_Center);
	DecoSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

	UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this);
	APlayerController* PC = GetOwningPlayer();
	float Top = 20.f;
	auto AddButton = [&](const TCHAR* Text, EHexButtonStyle ButtonStyle, TFunction<void()> Action)
	{
		if (UHexMenuButton* Button = PC ? CreateWidget<UHexMenuButton>(PC, UHexMenuButton::StaticClass()) : nullptr)
		{
			Button->Setup(HexMenu::Txt(Text), ButtonStyle, 300.f, 58.f, MoveTemp(Action));
			UVerticalBoxSlot* ButtonSlot = Body->AddChildToVerticalBox(Button);
			ButtonSlot->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Center);
			Top = 6.f;
		}
	};
	AddButton(HEXT("继续游戏"), EHexButtonStyle::Primary, [this]() { if (OnResume) { OnResume(); } });
	AddButton(HEXT("重新开始"), EHexButtonStyle::Secondary, [this]()
	{
		if (OnResume)
		{
			OnResume();
		}
		if (AHexSokobanGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AHexSokobanGameMode>() : nullptr)
		{
			Mode->RestartPuzzle();
		}
	});
	if (Flow && Flow->IsTestPlaying())
	{
		AddButton(HEXT("返回编辑器"), EHexButtonStyle::Secondary, [this]()
		{
			if (UHexGameFlowSubsystem* FlowSys = UHexGameFlowSubsystem::Get(this))
			{
				FlowSys->ReturnToEditor();
			}
		});
	}
	AddButton(HEXT("选择关卡"), EHexButtonStyle::Secondary, [this]()
	{
		if (UHexGameFlowSubsystem* FlowSys = UHexGameFlowSubsystem::Get(this))
		{
			FlowSys->OpenMenu(EHexMenuScreen::LevelSelect);
		}
	});
	AddButton(HEXT("主菜单"), EHexButtonStyle::Secondary, [this]()
	{
		if (UHexGameFlowSubsystem* FlowSys = UHexGameFlowSubsystem::Get(this))
		{
			FlowSys->OpenMenu(EHexMenuScreen::Main);
		}
	});
	AddButton(HEXT("退出游戏"), EHexButtonStyle::Danger, [this]()
	{
		if (UHexGameFlowSubsystem* FlowSys = UHexGameFlowSubsystem::Get(this))
		{
			FlowSys->QuitGame();
		}
	});
	UTextBlock* Footer = HexMenu::MakeText(WidgetTree, 14, false, FHexUIStyle::InkSoft, ETextJustify::Center);
	Footer->SetText(HexMenu::Txt(HEXT("按 ESC 继续")));	// Esc: continue
	UVerticalBoxSlot* FooterSlot = Body->AddChildToVerticalBox(Footer);
	FooterSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	FooterSlot->SetHorizontalAlignment(HAlign_Center);
}

FReply UHexPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::P)
	{
		if (!InKeyEvent.IsRepeat() && OnResume)
		{
			OnResume();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UHexPauseMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Age += FMath::Min(InDeltaTime, 0.05f);
	if (Overlay)
	{
		Overlay->SetRenderOpacity(FMath::Clamp(Age / 0.2f, 0.f, 1.f));
	}
	if (Card)
	{
		const float Pop = 1.f - 0.16f * FMath::Exp(-9.f * Age) * FMath::Cos(Age * 15.f);
		Card->SetRenderScale(FVector2D(Pop, Pop));
		Card->SetRenderOpacity(FMath::Clamp(Age / 0.1f, 0.f, 1.f));
	}
}
