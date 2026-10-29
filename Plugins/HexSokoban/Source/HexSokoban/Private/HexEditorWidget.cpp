#include "HexEditorWidget.h"
#include "HexLocalization.h"
#include "HexAudioSubsystem.h"
#include "HexMenuWidgets.h"
#include "HexCardWidget.h"
#include "HexUIStyle.h"
#include "HexGameFlowSubsystem.h"
#include "HexSokobanSettings.h"
#include "HexSokobanTypes.h"
#include "Async/Async.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"

namespace HexEd
{
	FText Txt(const FString& Text) { return FText::FromString(Text); }

	UTextBlock* MakeText(UWidgetTree* Tree, int32 Size, bool bBold, const FLinearColor& Color, ETextJustify::Type Justify = ETextJustify::Left)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetFont(FHexUIStyle::Font(Size, bBold));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(Justify);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	}

	UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Fill, float Radius, const FMargin& Padding,
		ESlateVisibility Visibility = ESlateVisibility::SelfHitTestInvisible)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Border->SetBrush(FHexUIStyle::Panel(Fill, Radius));
		Border->SetPadding(Padding);
		Border->SetVisibility(Visibility);
		return Border;
	}

	void Spring(float& Value, float& Velocity, float Target, float DeltaTime, float Stiffness = 420.f, float Damping = 22.f)
	{
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(DeltaTime / (1.f / 120.f)));
		const float Dt = DeltaTime / static_cast<float>(Steps);
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			Velocity += ((Target - Value) * Stiffness - Velocity * Damping) * Dt;
			Value += Velocity * Dt;
		}
	}

	FString SuitSymbol(EHexSuit Suit)
	{
		return FHexCard(1, Suit).GetSuitSymbol();
	}

	const FLinearColor TileChip(0.29f, 0.80f, 0.94f, 1.f);	// the board's cyan
	const FLinearColor GoalChip(0.05f, 0.62f, 0.32f, 1.f);
	const FLinearColor BoxChip(0.62f, 0.62f, 0.70f, 1.f);
	const FLinearColor StartChip(0.10f, 0.16f, 0.32f, 1.f);
}

// ================================================================ card slot

void UHexEditorCardSlot::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	Box->SetVisibility(ESlateVisibility::Visible);
	WidgetTree->RootWidget = Box;
	SetCursor(EMouseCursor::Hand);	// a card you can pick up
}

void UHexEditorCardSlot::Setup(UHexEditorWidget* InOwner, const FHexCard& InCard, int32 InDeckIndex, FVector2D InSize, float AppearDelay)
{
	Owner = InOwner;
	Card = InCard;
	DeckIndex = InDeckIndex;
	Delay = AppearDelay;
	Scale = AppearDelay >= 0.f ? 0.f : 1.f;
	if (!Box || !InOwner)
	{
		return;
	}
	Box->SetWidthOverride(InSize.X);
	Box->SetHeightOverride(InSize.Y);
	UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	Fit->SetStretch(EStretch::ScaleToFit);
	Fit->SetVisibility(ESlateVisibility::HitTestInvisible);
	Box->SetContent(Fit);
	CardWidget = InOwner->MakeCardWidget(InCard);
	if (CardWidget)
	{
		Fit->AddChild(CardWidget);
	}
	Box->SetRenderScale(FVector2D(Scale, Scale));
}

FReply UHexEditorCardSlot::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Owner.IsValid())
	{
		ScaleVelocity -= 3.f;
		return Owner->BeginCardDrag(this, InMouseEvent);
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UHexEditorCardSlot::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float DeltaTime = FMath::Min(InDeltaTime, 0.05f);
	if (Delay > 0.f)
	{
		Delay -= DeltaTime;
		return;
	}
	HexEd::Spring(Scale, ScaleVelocity, IsHovered() ? 1.1f : 1.f, DeltaTime, 380.f, 18.f);
	if (Box)
	{
		Box->SetRenderScale(FVector2D(FMath::Max(0.f, Scale), FMath::Max(0.f, Scale)));
	}
}

// ================================================================ palette item

void UHexEditorPaletteItem::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(96.f);
	Box->SetHeightOverride(96.f);
	Box->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	Box->SetVisibility(ESlateVisibility::Visible);
	WidgetTree->RootWidget = Box;
	SetCursor(EMouseCursor::Hand);	// a build item you can drag out
	Face = HexEd::MakePanel(WidgetTree, FLinearColor::White, 14.f, FMargin(6.f));
	Face->SetBrush(FHexUIStyle::Panel(FHexUIStyle::PanelDark, 7.f, FLinearColor(0.35f, 0.34f, 0.43f, 1.f), 2.f));
	Face->SetPadding(FMargin(8.f));
	Face->SetVerticalAlignment(VAlign_Center);
	Box->SetContent(Face);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Face->SetContent(Column);
	Chip = HexEd::MakePanel(WidgetTree, HexEd::TileChip, 12.f, FMargin(0.f));
	USizeBox* ChipBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ChipBox->SetWidthOverride(52.f);
	ChipBox->SetHeightOverride(52.f);
	ChipBox->SetContent(Chip);
	Chip->SetHorizontalAlignment(HAlign_Center);
	Chip->SetVerticalAlignment(VAlign_Center);
	Icon = HexEd::MakeText(WidgetTree, 26, true, FLinearColor::White, ETextJustify::Center);
	Chip->SetContent(Icon);
	Column->AddChildToVerticalBox(ChipBox)->SetHorizontalAlignment(HAlign_Center);
	Caption = HexEd::MakeText(WidgetTree, 13, true, FHexUIStyle::Ink, ETextJustify::Center);
	Column->AddChildToVerticalBox(Caption)->SetHorizontalAlignment(HAlign_Center);
}

void UHexEditorPaletteItem::Setup(UHexEditorWidget* InOwner, EHexEditorItem InItem, EHexSuit InSuit)
{
	Owner = InOwner;
	Item = InItem;
	Suit = InSuit;
	FString IconText, CaptionText;
	FLinearColor ChipColor, IconColor;
	UHexEditorWidget::GetItemLook(InItem, InSuit, IconText, CaptionText, ChipColor, IconColor);
	if (Chip)
	{
		Chip->SetBrush(FHexUIStyle::Panel(ChipColor, 12.f));
	}
	if (Icon)
	{
		Icon->SetText(HexEd::Txt(IconText));
		Icon->SetColorAndOpacity(FSlateColor(IconColor));
	}
	if (Caption)
	{
		Caption->SetText(HexEd::Txt(CaptionText));
	}
	// Palette-only presentation: do not change the shared suit/drag visuals.
	if (InItem == EHexEditorItem::Tile && Chip)
	{
		const TCHAR* AssetName = TEXT("T_Rock_Icon");
		const TCHAR* TerrainName = HEXT("山地");
		switch (InSuit)
		{
		case EHexSuit::Club: AssetName = TEXT("T_Road_Icon"); TerrainName = HEXT("城镇"); break;
		case EHexSuit::Diamond: AssetName = TEXT("T_River_Icon"); TerrainName = HEXT("水域"); break;
		case EHexSuit::Heart: AssetName = TEXT("T_Grass_Icon"); TerrainName = HEXT("草地"); break;
		default: break;
		}
		const FString Path = FString::Printf(TEXT("/Game/FantasyRPG/Card/%s.%s"), AssetName, AssetName);
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path))
		{
			UImage* TerrainImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			FSlateBrush Art;
			Art.SetResourceObject(Texture);
			Art.DrawAs = ESlateBrushDrawType::Image;
			// Trim only the empty margins in the supplied art; retain its aspect ratio.
			if (InSuit == EHexSuit::Heart)
			{
				Art.SetUVRegion(FBox2f(FVector2f(0.12f, 0.f), FVector2f(0.91f, 0.95f)));
				Art.ImageSize = FVector2D(43.24f, 52.f);
			}
			else
			{
				Art.SetUVRegion(FBox2f(FVector2f(0.12f, 0.31f), FVector2f(0.91f, 0.95f)));
				Art.ImageSize = FVector2D(52.f, 42.13f);
			}
			TerrainImage->SetBrush(Art);
			TerrainImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			FSlateBrush ClearBrush;
			ClearBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
			Chip->SetBrush(ClearBrush);
			Chip->SetContent(TerrainImage);
		}
		Caption->SetText(HexEd::Txt(TerrainName));
	}
	else if (Chip)
	{
		const TCHAR* AssetName = nullptr;
		switch (InItem)
		{
		case EHexEditorItem::Goal: AssetName = TEXT("T_End"); break;
		case EHexEditorItem::Box: AssetName = TEXT("T_Box"); break;
		case EHexEditorItem::Start: AssetName = TEXT("T_Start"); break;
		case EHexEditorItem::Scenery: AssetName = TEXT("T_Scene"); break;
		default: break;
		}
		if (AssetName)
		{
			const FString Path = FString::Printf(TEXT("/Game/FantasyRPG/PNG/07_Play/%s.%s"), AssetName, AssetName);
			if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path))
			{
				UImage* ItemImage = WidgetTree->ConstructWidget<UImage>();
				FSlateBrush Art;
				Art.SetResourceObject(Texture);
				Art.DrawAs = ESlateBrushDrawType::Image;
				const FVector2D SourceSize(Texture->GetSizeX(), Texture->GetSizeY());
				Art.ImageSize = SourceSize * (52.f / FMath::Max(1.0, FMath::Max(SourceSize.X, SourceSize.Y)));
				ItemImage->SetBrush(Art);
				ItemImage->SetVisibility(ESlateVisibility::HitTestInvisible);
				FSlateBrush ClearBrush;
				ClearBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
				Chip->SetBrush(ClearBrush);
				Chip->SetContent(ItemImage);
			}
		}
	}
}

FReply UHexEditorPaletteItem::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Owner.IsValid())
	{
		ScaleVelocity -= 3.5f;
		UHexAudioSubsystem::PlayExact(this, TEXT("Click_06"));
		return Owner->BeginPaletteDrag(Item, Suit, InMouseEvent);
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UHexEditorPaletteItem::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float DeltaTime = FMath::Min(InDeltaTime, 0.05f);
	const bool bHovered = IsHovered();
    if (bHovered && !bAudioHovered) UHexAudioSubsystem::PlayExact(this, TEXT("Click_08"), 0.35f);
    bAudioHovered = bHovered;
	HexEd::Spring(Scale, ScaleVelocity, bHovered ? 1.06f : 1.f, DeltaTime);
	HexEd::Spring(Lift, LiftVelocity, bHovered ? -4.f : 0.f, DeltaTime);
	if (Box)
	{
		Box->SetRenderScale(FVector2D(Scale, Scale));
		Box->SetRenderTranslation(FVector2D(0.0, Lift));
	}
	if (Face)
	{
		// A true square with a two-unit outline, independent of texture bevel/shadow bounds.
		Face->SetBrush(FHexUIStyle::Panel(FHexUIStyle::PanelDark, 7.f,
			bHovered ? FHexUIStyle::Accent : FLinearColor(0.35f, 0.34f, 0.43f, 1.f), 2.f));
	}
}

// ================================================================ editor widget: build

void UHexEditorWidget::GetItemLook(EHexEditorItem Item, EHexSuit Suit, FString& OutIcon, FString& OutCaption, FLinearColor& OutChip, FLinearColor& OutIconColor)
{
	switch (Item)
	{
	case EHexEditorItem::Tile:
	{
		const TCHAR* Names[] = { HEXT("黑桃地台"), HEXT("红桃地台"), HEXT("梅花地台"), HEXT("方片地台") };
		OutIcon = HexEd::SuitSymbol(Suit);
		OutCaption = Names[static_cast<int32>(Suit)];
		OutChip = HexEd::TileChip;
		OutIconColor = (Suit == EHexSuit::Heart || Suit == EHexSuit::Diamond) ? FLinearColor(0.85f, 0.10f, 0.10f, 1.f) : FHexUIStyle::Ink;
		break;
	}
	case EHexEditorItem::Scenery:
		OutIcon=HEXT("景"); OutCaption=HEXT("造景"); OutChip=FLinearColor(.18f,.35f,.27f,1.f); OutIconColor=FLinearColor::White;
		break;
	case EHexEditorItem::Goal:
		OutIcon = HEXT("终");
		OutCaption = HEXT("终点");
		OutChip = HexEd::GoalChip;
		OutIconColor = FLinearColor::White;
		break;
	case EHexEditorItem::Box:
		OutIcon = HEXT("箱");
		OutCaption = HEXT("箱子");
		OutChip = HexEd::BoxChip;
		OutIconColor = FLinearColor::White;
		break;
	default:
		OutIcon = HEXT("起");
		OutCaption = HEXT("玩家起点");
		OutChip = HexEd::StartChip;
		OutIconColor = FHexUIStyle::Accent;
		break;
	}
}

UHexCardWidget* UHexEditorWidget::MakeCardWidget(const FHexCard& Card)
{
	TSubclassOf<UHexCardWidget> Class = UHexSokobanSettings::Get().CardWidgetClass.TryLoadClass<UHexCardWidget>();
	if (!Class)
	{
		Class = UHexCardWidget::StaticClass();
	}
	APlayerController* PC = GetOwningPlayer();
	UHexCardWidget* Widget = PC ? CreateWidget<UHexCardWidget>(PC, Class) : nullptr;
	if (Widget)
	{
		Widget->SetShowCenterSuit(true);	// small thumbnails: the centre suit makes the suit readable at a glance
		Widget->SetCard(Card);
		Widget->SetPlayableHint(false);
		Widget->SetVisualState(EHexCardVisualState::Normal);
		Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	return Widget;
}

void UHexEditorWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
}

void UHexEditorWidget::BuildTree()
{
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EditorCanvas"));
	RootCanvas->SetVisibility(ESlateVisibility::Visible);	// the empty area is the 3D board: we take its mouse
	WidgetTree->RootWidget = RootCanvas;
	APlayerController* PC = GetOwningPlayer();

	// ---- Top left: back to the menu.
	if (UHexMenuButton* Back = PC ? CreateWidget<UHexMenuButton>(PC, UHexMenuButton::StaticClass()) : nullptr)
	{
		Back->Setup(HexEd::Txt(HEXT("主菜单")), EHexButtonStyle::Dark, 140.f, 44.f, [this]()
		{
			if (UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this))
			{
				Flow->OpenMenu(EHexMenuScreen::Main);
			}
		});
		UCanvasPanelSlot* BackSlot = RootCanvas->AddChildToCanvas(Back);
		BackSlot->SetPosition(FVector2D(28.0, 22.0));
		BackSlot->SetAutoSize(true);
		UIBlockers.Add(Back);
	}

	// ---- Hint bar (same pill as the in-game control hint).
	UBorder* HintBar = HexEd::MakePanel(WidgetTree, FHexUIStyle::PanelDark, 18.f, FMargin(20.f, 7.f));
	UTextBlock* Hint = HexEd::MakeText(WidgetTree, 14, false, FHexUIStyle::TextLight);
	Hint->SetText(HexEd::Txt(HEXT("拖拽放置　·　悬停地台：箭头 / 滚轮 / 数字键改点数，S C D H 改花色　·　右键拖动平移　·　滚轮缩放")));
	HintBar->SetContent(Hint);
	UCanvasPanelSlot* HintSlot = RootCanvas->AddChildToCanvas(HintBar);
	HintSlot->SetPosition(FVector2D(190.0, 26.0));
	HintSlot->SetAutoSize(true);

	// ---- Trash (one zone for everything; same family as the discard zone).
	Trash = HexEd::MakePanel(WidgetTree, FHexUIStyle::PanelDark, 16.f, FMargin(10.f), ESlateVisibility::Visible);
	Trash->SetHorizontalAlignment(HAlign_Center);
	Trash->SetVerticalAlignment(VAlign_Center);
	Trash->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	UVerticalBox* TrashColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Trash->SetContent(TrashColumn);
	TrashTitle = HexEd::MakeText(WidgetTree, 26, true, FHexUIStyle::TextLight, ETextJustify::Center);
	TrashTitle->SetText(HexEd::Txt(HEXT("删除")));
	TrashColumn->AddChildToVerticalBox(TrashTitle)->SetHorizontalAlignment(HAlign_Center);
	TrashSub = HexEd::MakeText(WidgetTree, 13, false, FHexUIStyle::TextLight.CopyWithNewOpacity(0.75f), ETextJustify::Center);
	TrashSub->SetText(HexEd::Txt(HEXT("拖到这里")));
	TrashColumn->AddChildToVerticalBox(TrashSub)->SetHorizontalAlignment(HAlign_Center);
	UCanvasPanelSlot* TrashSlot = RootCanvas->AddChildToCanvas(Trash);
	TrashSlot->SetAnchors(FAnchors(0.f, 1.f));
	TrashSlot->SetAlignment(FVector2D(0.5, 0.5));
	TrashSlot->SetSize(FVector2D(160.0, 118.0));
	TrashSlot->SetPosition(FVector2D(118.0, -96.0));
	UIBlockers.Add(Trash);

	BuildRightPanel(RootCanvas);

	// ---- Toast.
	Toast = HexEd::MakePanel(WidgetTree, FHexUIStyle::Good, 12.f, FMargin(22.f, 9.f));
	Toast->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	ToastText = HexEd::MakeText(WidgetTree, 18, true, FLinearColor::White, ETextJustify::Center);
	Toast->SetContent(ToastText);
	Toast->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* ToastSlot = RootCanvas->AddChildToCanvas(Toast);
	ToastSlot->SetAnchors(FAnchors(0.35f, 0.f));
	ToastSlot->SetAlignment(FVector2D(0.5, 0.0));
	ToastSlot->SetPosition(FVector2D(0.0, 84.0));
	ToastSlot->SetAutoSize(true);
	ToastSlot->SetZOrder(60);

	// ---- Rank arrows next to the hovered tile's face.
	auto MakeArrow = [this, PC](const TCHAR* Text, int32 Delta) -> UHexMenuButton*
	{
		UHexMenuButton* Arrow = PC ? CreateWidget<UHexMenuButton>(PC, UHexMenuButton::StaticClass()) : nullptr;
		if (Arrow)
		{
			Arrow->Setup(HexEd::Txt(Text), EHexButtonStyle::Dark, 46.f, 46.f, [this, Delta]()
			{
				if (AHexLevelEditor* Ed = Editor.Get())
				{
					Ed->ChangeRank(ArrowCell, Delta);
				}
			});
			UCanvasPanelSlot* ArrowSlot = RootCanvas->AddChildToCanvas(Arrow);
			ArrowSlot->SetAlignment(FVector2D(0.5, 0.5));
			ArrowSlot->SetAutoSize(true);
			ArrowSlot->SetZOrder(40);
			Arrow->SetVisibility(ESlateVisibility::Collapsed);
			Arrow->SetRenderTransformPivot(FVector2D(0.5, 0.5));
		}
		return Arrow;
	};
	// The kit's page arrow points left: turned a quarter clockwise it points up, a quarter back it points down.
	RankUp = MakeArrow(TEXT("\u25b2"), 1);
	RankDown = MakeArrow(TEXT("\u25b2"), -1);
	if (RankUp)
	{
		RankUp->SetRenderTransformAngle(90.f);
	}
	if (RankDown)
	{
		RankDown->SetRenderTransformAngle(-90.f);
	}

	// ---- Drag visuals (follow the mouse, never hit-testable).
	DragChip = HexEd::MakePanel(WidgetTree, HexEd::TileChip, 14.f, FMargin(0.f), ESlateVisibility::Collapsed);
	DragChip->SetHorizontalAlignment(HAlign_Center);
	DragChip->SetVerticalAlignment(VAlign_Center);
	DragChip->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	DragChipIcon = HexEd::MakeText(WidgetTree, 30, true, FLinearColor::White, ETextJustify::Center);
	DragChip->SetContent(DragChipIcon);
	UCanvasPanelSlot* ChipSlot = RootCanvas->AddChildToCanvas(DragChip);
	ChipSlot->SetSize(FVector2D(64.0, 56.0));
	ChipSlot->SetAlignment(FVector2D(0.5, 0.5));
	ChipSlot->SetZOrder(100);

	DragCardBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DragCardBox->SetWidthOverride(70.f);
	DragCardBox->SetHeightOverride(98.f);
	DragCardBox->SetVisibility(ESlateVisibility::Collapsed);
	DragCardBox->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	UScaleBox* CardFit = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	CardFit->SetStretch(EStretch::ScaleToFit);
	DragCardBox->SetContent(CardFit);
	DragCard = MakeCardWidget(FHexCard());
	if (DragCard)
	{
		CardFit->AddChild(DragCard);
	}
	UCanvasPanelSlot* CardSlot = RootCanvas->AddChildToCanvas(DragCardBox);
	CardSlot->SetAutoSize(true);
	CardSlot->SetAlignment(FVector2D(0.5, 0.5));
	CardSlot->SetZOrder(100);

	InsertMarker = HexEd::MakePanel(WidgetTree, FHexUIStyle::Accent, 3.f, FMargin(0.f), ESlateVisibility::Collapsed);
	UCanvasPanelSlot* MarkerSlot = RootCanvas->AddChildToCanvas(InsertMarker);
	MarkerSlot->SetSize(FVector2D(6.0, 76.0));
	MarkerSlot->SetAlignment(FVector2D(0.5, 0.5));
	MarkerSlot->SetZOrder(90);
}

void UHexEditorWidget::BuildRightPanel(UCanvasPanel* Root)
{
	APlayerController* PC = GetOwningPlayer();
	RightPanel = HexEd::MakePanel(WidgetTree, FHexUIStyle::PanelLight, 18.f, FMargin(0.f), ESlateVisibility::Visible);
	RightPanel->SetPadding(FHexUIStyle::SkinPadding(EHexSkin::Popup, 0.4f) + FMargin(2.f, 4.f, 2.f, 2.f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(RightPanel);
	PanelSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 1.f));
	PanelSlot->SetOffsets(FMargin(-556.f, 16.f, 540.f, 16.f));	// left offset, top, width, bottom
	PanelSlot->SetAlignment(FVector2D(0.0, 0.0));
	UIBlockers.Add(RightPanel);

	UHorizontalBox* Frame = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RightPanel->SetContent(Frame);
	FHexUIStyle::DecoratePanel(WidgetTree, RightPanel);
	USizeBox* StripeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	StripeBox->SetWidthOverride(8.f);
	StripeBox->SetContent(HexEd::MakePanel(WidgetTree, FHexUIStyle::Accent, 4.f, FMargin(0.f)));
	UHorizontalBoxSlot* StripeSlot = Frame->AddChildToHorizontalBox(StripeBox);
	StripeSlot->SetVerticalAlignment(VAlign_Fill);
	StripeBox->SetVisibility(ESlateVisibility::Collapsed);	// the kit frame replaces the stripe
	// Scrolling body above a fixed footer, so the save / test buttons never scroll out of reach.
	UVerticalBox* MainColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Frame->AddChildToHorizontalBox(MainColumn)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Scroll->SetScrollbarThickness(FVector2D(5.0, 5.0));
	Scroll->SetScrollbarPadding(FMargin(4.f, 0.f, 0.f, 0.f));
	MainColumn->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(Column);

	auto Section = [this, Column](const TCHAR* Text, float Top)
	{
		UTextBlock* Title = HexEd::MakeText(WidgetTree, 15, true, FHexUIStyle::InkSoft);
		Title->SetText(HexEd::Txt(Text));
		Column->AddChildToVerticalBox(Title)->SetPadding(FMargin(20.f, Top, 20.f, 6.f));
		return Title;
	};

	// Header: title + status pill.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(Header)->SetPadding(FMargin(20.f, 14.f, 20.f, 0.f));
	UTextBlock* Title = HexEd::MakeText(WidgetTree, 26, true, FHexUIStyle::Ink);
	Title->SetText(HexEd::Txt(HEXT("关卡编辑器")));
	Header->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Center);
	Header->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	StatusPill = HexEd::MakePanel(WidgetTree, FHexUIStyle::InkSoft, 13.f, FMargin(12.f, 4.f, 12.f, 5.f));
	StatusText = HexEd::MakeText(WidgetTree, 13, true, FLinearColor::White);
	StatusPill->SetContent(StatusText);
	Header->AddChildToHorizontalBox(StatusPill)->SetVerticalAlignment(VAlign_Center);

	// Name.
	NameBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	NameBox->WidgetStyle.TextStyle.SetFont(FHexUIStyle::Font(16, true));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	{
		FEditableTextBoxStyle FieldStyle = NameBox->WidgetStyle;
		const FSlateBrush Field = FHexUIStyle::Panel(FHexUIStyle::PanelInset, 10.f, FLinearColor(0.42f, 0.38f, 0.58f, 1.f), 2.f);
		const FSlateBrush FieldFocus = FHexUIStyle::Panel(FHexUIStyle::PanelInset, 10.f, FHexUIStyle::Accent, 2.f);
		FieldStyle.SetBackgroundImageNormal(Field);
		FieldStyle.SetBackgroundImageHovered(Field);
		FieldStyle.SetBackgroundImageFocused(FieldFocus);
		FieldStyle.SetBackgroundImageReadOnly(Field);
		FieldStyle.SetForegroundColor(FSlateColor(FHexUIStyle::Ink));
		FieldStyle.SetPadding(FMargin(14.f, 8.f));
		NameBox->WidgetStyle = FieldStyle;
		NameBox->SetForegroundColor(FHexUIStyle::Ink);
	}
	NameBox->SetHintText(HexEd::Txt(HEXT("关卡名称")));
	Column->AddChildToVerticalBox(NameBox)->SetPadding(FMargin(20.f, 10.f, 20.f, 0.f));

	// Card library: 4 suits x A-10.
	Section(HEXT("卡牌库　·　拖到下方加入本关手牌（可重复）"), 14.f);
	PickerGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	PickerGrid->SetSlotPadding(FMargin(2.f));
	Column->AddChildToVerticalBox(PickerGrid)->SetPadding(FMargin(16.f, 0.f, 16.f, 0.f));
	const EHexSuit Suits[4] = { EHexSuit::Spade, EHexSuit::Heart, EHexSuit::Club, EHexSuit::Diamond };
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Rank = 1; Rank <= 10; ++Rank)
		{
			UHexEditorCardSlot* Card = PC ? CreateWidget<UHexEditorCardSlot>(PC, UHexEditorCardSlot::StaticClass()) : nullptr;
			if (Card)
			{
				Card->Setup(this, FHexCard(Rank, Suits[Row]), INDEX_NONE, FVector2D(44.0, 62.0), -1.f);
				PickerGrid->AddChildToUniformGrid(Card, Row, Rank - 1);
			}
		}
	}
	UIBlockers.Add(PickerGrid);

	// The level's hand.
	UHorizontalBox* DeckHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(DeckHeader)->SetPadding(FMargin(20.f, 16.f, 20.f, 6.f));
	DeckTitle = HexEd::MakeText(WidgetTree, 15, true, FHexUIStyle::InkSoft);
	DeckHeader->AddChildToHorizontalBox(DeckTitle)->SetVerticalAlignment(VAlign_Center);
	DeckHeader->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	if (UHexMenuButton* Auto = PC ? CreateWidget<UHexMenuButton>(PC, UHexMenuButton::StaticClass()) : nullptr)
	{
		Auto->Setup(HexEd::Txt(HEXT("一键配卡组")), EHexButtonStyle::Primary, 150.f, 40.f, [this]() { AutoDeck(); });
		DeckHeader->AddChildToHorizontalBox(Auto)->SetVerticalAlignment(VAlign_Center);
	}
	UBorder* DeckTray = HexEd::MakePanel(WidgetTree, FHexUIStyle::PanelInset, 12.f, FMargin(8.f), ESlateVisibility::Visible);
	DeckTray->SetBrush(FHexUIStyle::Panel(FHexUIStyle::PanelInset, 12.f, FLinearColor(0.36f, 0.32f, 0.50f, 1.f), 2.f));
	USizeBox* DeckBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DeckBox->SetHeightOverride(176.f);
	DeckBox->SetContent(DeckTray);
	Column->AddChildToVerticalBox(DeckBox)->SetPadding(FMargin(16.f, 0.f, 16.f, 0.f));
	DeckScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	DeckTray->SetContent(DeckScroll);
	DeckWrap = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	DeckWrap->SetInnerSlotPadding(FVector2D(6.0, 6.0));
	DeckScroll->AddChild(DeckWrap);
	UIBlockers.Add(DeckBox);

	// Build palette.
	Section(HEXT("建造　·　终点 / 箱子 / 起点要放在地台上"), 16.f);
	UUniformGridPanel* Palette = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	Palette->SetSlotPadding(FMargin(5.f));
	Column->AddChildToVerticalBox(Palette)->SetPadding(FMargin(14.f, 0.f, 14.f, 0.f));
	struct FEntry { EHexEditorItem Item; EHexSuit Suit; };
	const FEntry Entries[] = {
		{ EHexEditorItem::Tile, EHexSuit::Spade }, { EHexEditorItem::Tile, EHexSuit::Club },
		{ EHexEditorItem::Tile, EHexSuit::Diamond }, { EHexEditorItem::Tile, EHexSuit::Heart },
		{ EHexEditorItem::Goal, EHexSuit::Spade }, { EHexEditorItem::Box, EHexSuit::Spade }, { EHexEditorItem::Start, EHexSuit::Spade }, { EHexEditorItem::Scenery, EHexSuit::Spade } };
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Entries)); ++Index)
	{
		if (UHexEditorPaletteItem* Item = PC ? CreateWidget<UHexEditorPaletteItem>(PC, UHexEditorPaletteItem::StaticClass()) : nullptr)
		{
			Item->Setup(this, Entries[Index].Item, Entries[Index].Suit);
			UUniformGridSlot* PaletteSlot = Palette->AddChildToUniformGrid(Item, Index / 4, Index % 4);
			PaletteSlot->SetHorizontalAlignment(HAlign_Center);
			PaletteSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	UIBlockers.Add(Palette);

	// Test play / save.
	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* ActionsSlot = MainColumn->AddChildToVerticalBox(Actions);
	ActionsSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 6.f));
	ActionsSlot->SetHorizontalAlignment(HAlign_Center);
	auto AddAction = [this, PC, Actions](const TCHAR* Text, EHexButtonStyle ButtonStyle, float Width, TFunction<void()> Action)
	{
		if (UHexMenuButton* Button = PC ? CreateWidget<UHexMenuButton>(PC, UHexMenuButton::StaticClass()) : nullptr)
		{
			Button->Setup(HexEd::Txt(Text), ButtonStyle, Width, 40.f, MoveTemp(Action));
			Actions->AddChildToHorizontalBox(Button)->SetPadding(FMargin(5.f, 0.f, 5.f, 0.f));
		}
	};
	AddAction(HEXT("试玩"), EHexButtonStyle::Secondary, 100.f, [this]() { TestPlay(); });
	AddAction(HEXT("保存"), EHexButtonStyle::Primary, 100.f, [this]() { Save(false); });
#if WITH_EDITOR
	AddAction(HEXT("存为内置关卡"), EHexButtonStyle::Secondary, 190.f, [this]() { Save(true); });
#endif
}

void UHexEditorWidget::SetEditor(AHexLevelEditor* InEditor)
{
	Editor = InEditor;
	if (InEditor && NameBox)
	{
		NameBox->SetText(HexEd::Txt(InEditor->GetDefinition().DisplayName));
	}
	RebuildDeckView(INDEX_NONE, true);
}

// ================================================================ deck view

void UHexEditorWidget::RefreshDeckTitle()
{
	const AHexLevelEditor* Ed = Editor.Get();
	if (DeckTitle && Ed)
	{
		DeckTitle->SetText(HexEd::Txt(FHexLocalization::Printf(HEXT("本关手牌　%d 张"), Ed->GetDefinition().Deck.Num())));
	}
}

void UHexEditorWidget::RebuildDeckView(int32 PopFrom, bool bStagger)
{
	AHexLevelEditor* Ed = Editor.Get();
	APlayerController* PC = GetOwningPlayer();
	if (!DeckWrap || !Ed || !PC)
	{
		return;
	}
	DeckWrap->ClearChildren();
	const TArray<FHexCard>& Deck = Ed->GetDefinition().Deck;
	for (int32 Index = 0; Index < Deck.Num(); ++Index)
	{
		UHexEditorCardSlot* Card = CreateWidget<UHexEditorCardSlot>(PC, UHexEditorCardSlot::StaticClass());
		if (!Card)
		{
			continue;
		}
		// New cards pop in (one by one after a one-click deck); the rest just appear.
		const float Delay = bStagger ? 0.035f * static_cast<float>(Index) : (Index == PopFrom ? 0.f : -1.f);
		Card->Setup(this, Deck[Index], Index, FVector2D(52.0, 72.0), Delay);
		DeckWrap->AddChildToWrapBox(Card);
	}
	ShownDeckCount = Deck.Num();
	RefreshDeckTitle();
}

void UHexEditorWidget::AutoDeck()
{
	AHexLevelEditor* Ed = Editor.Get();
	if (!Ed)
	{
		return;
	}
	TArray<FHexCard> Deck;
	FString Reason;
	if (!Ed->GetDefinition().IsPlayable(&Reason))
	{
		ShowToast(FHexLocalization::Printf(HEXT("先调整一下关卡：%s"), *Reason), true);
		return;
	}
	if (!UHexLevelTools::MakeRecommendedDeck(Ed->GetDefinition(), Deck))
	{
		ShowToast(HEXT("还没找到能推完箱子的路线，请检查箱子和终点的位置。"), true);
		return;
	}
	Ed->EditDefinition().Deck = Deck;
	Ed->NotifyChanged();
	RebuildDeckView(INDEX_NONE, true);
	ShowToast(FHexLocalization::Printf(HEXT("已按最优解生成 %d 张手牌"), Deck.Num()));
}

// ================================================================ geometry helpers

bool UHexEditorWidget::IsOver(const UWidget* Widget, const FVector2D& AbsoluteMouse) const
{
	return Widget && Widget->IsVisible() && Widget->GetCachedGeometry().IsUnderLocation(AbsoluteMouse);
}

bool UHexEditorWidget::IsOverUI(const FVector2D& AbsoluteMouse) const
{
	for (const UWidget* Widget : UIBlockers)
	{
		if (IsOver(Widget, AbsoluteMouse))
		{
			return true;
		}
	}
	return false;
}

FVector2D UHexEditorWidget::AbsoluteToRoot(const FVector2D& Absolute) const
{
	return RootCanvas ? RootCanvas->GetCachedGeometry().AbsoluteToLocal(Absolute) : Absolute;
}

int32 UHexEditorWidget::GetDeckInsertIndex(const FVector2D& AbsoluteMouse, FVector2D& OutMarkerLocal) const
{
	// Reading order: the first card whose centre is after the mouse (same row: to the right; or a later row).
	const int32 Count = DeckWrap ? DeckWrap->GetChildrenCount() : 0;
	OutMarkerLocal = FVector2D::ZeroVector;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const UWidget* Child = DeckWrap->GetChildAt(Index);
		const FGeometry& Geometry = Child->GetCachedGeometry();
		const FVector2D TopLeft = Geometry.GetAbsolutePosition();
		const FVector2D Size = Geometry.GetAbsoluteSize();
		const FVector2D Center = TopLeft + Size * 0.5;
		const bool bSameRow = FMath::Abs(AbsoluteMouse.Y - Center.Y) <= Size.Y * 0.5;
		if ((bSameRow && AbsoluteMouse.X < Center.X) || AbsoluteMouse.Y < TopLeft.Y)
		{
			OutMarkerLocal = AbsoluteToRoot(TopLeft + FVector2D(-3.0, Size.Y * 0.5));
			return Index;
		}
	}
	if (Count > 0)
	{
		const FGeometry& Last = DeckWrap->GetChildAt(Count - 1)->GetCachedGeometry();
		OutMarkerLocal = AbsoluteToRoot(Last.GetAbsolutePosition() + FVector2D(Last.GetAbsoluteSize().X + 3.0, Last.GetAbsoluteSize().Y * 0.5));
	}
	else if (DeckWrap)
	{
		OutMarkerLocal = AbsoluteToRoot(DeckWrap->GetCachedGeometry().GetAbsolutePosition() + FVector2D(4.0, 36.0));
	}
	return Count;
}

// ================================================================ drag & drop

FReply UHexEditorWidget::BeginCardDrag(UHexEditorCardSlot* CardSlot, const FPointerEvent& MouseEvent)
{
	if (!CardSlot || DragKind != EDragKind::None)
	{
		return FReply::Handled();
	}
	DragKind = EDragKind::Card;
	CardDragCard = CardSlot->Card;
	CardDragFrom = CardSlot->DeckIndex;
	if (DragCard)
	{
		DragCard->SetCard(CardDragCard);
	}
	DragChipPos = AbsoluteToRoot(MouseEvent.GetScreenSpacePosition());
	DragChipVelocity = FVector2D::ZeroVector;
	if (CardDragFrom != INDEX_NONE)
	{
		CardSlot->SetRenderOpacity(0.25f);	// its place in the hand shows as a gap while dragged
	}
	UpdateDragVisual(MouseEvent.GetScreenSpacePosition());
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UHexEditorWidget::BeginPaletteDrag(EHexEditorItem Item, EHexSuit Suit, const FPointerEvent& MouseEvent)
{
	if (DragKind != EDragKind::None)
	{
		return FReply::Handled();
	}
	DragKind = EDragKind::Board;
	BoardDrag = FHexEditorDrag();
	BoardDrag.Item = Item;
	BoardDrag.Suit = Suit;
	BoardDrag.Rank = 1;
	BoardDrag.bFromWorld = false;
	DragChipPos = AbsoluteToRoot(MouseEvent.GetScreenSpacePosition());
	DragChipVelocity = FVector2D::ZeroVector;
	UpdateDragVisual(MouseEvent.GetScreenSpacePosition());
	return FReply::Handled().CaptureMouse(TakeWidget());
}

void UHexEditorWidget::UpdateDragVisual(const FVector2D& AbsoluteMouse)
{
	LastMouseAbsolute = AbsoluteMouse;
	const bool bOverUI = IsOverUI(AbsoluteMouse);
	bOverTrash = (DragKind == EDragKind::Board || DragKind == EDragKind::Card) && IsOver(Trash, AbsoluteMouse);

	if (DragKind == EDragKind::Board)
	{
		// Over the board the 3D ghost takes over; over the UI a chip follows the cursor.
		if (AHexLevelEditor* Ed = Editor.Get())
		{
			Ed->UpdateDragPreview(BoardDrag, bOverUI);
		}
		FString IconText, CaptionText;
		FLinearColor ChipColor, IconColor;
		GetItemLook(BoardDrag.Item, BoardDrag.Suit, IconText, CaptionText, ChipColor, IconColor);
		if (DragChip)
		{
			DragChip->SetVisibility(bOverUI ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			DragChip->SetBrush(FHexUIStyle::Panel(ChipColor, 14.f, FLinearColor::White, 2.f));
		}
		if (DragChipIcon)
		{
			DragChipIcon->SetText(HexEd::Txt(IconText));
			DragChipIcon->SetColorAndOpacity(FSlateColor(IconColor));
		}
		if (InsertMarker)
		{
			InsertMarker->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else if (DragKind == EDragKind::Card)
	{
		if (DragCardBox)
		{
			DragCardBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		FVector2D Marker = FVector2D::ZeroVector;
		const bool bOverDeck = IsOver(DeckScroll, AbsoluteMouse);
		if (bOverDeck)
		{
			GetDeckInsertIndex(AbsoluteMouse, Marker);
		}
		if (InsertMarker)
		{
			InsertMarker->SetVisibility(bOverDeck ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(InsertMarker->Slot))
			{
				MarkerSlot->SetPosition(Marker);
			}
		}
	}
}

void UHexEditorWidget::FinishDrag(const FVector2D& AbsoluteMouse)
{
	AHexLevelEditor* Ed = Editor.Get();
	const EDragKind Kind = DragKind;
	DragKind = EDragKind::None;
	if (DragChip)
	{
		DragChip->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DragCardBox)
	{
		DragCardBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (InsertMarker)
	{
		InsertMarker->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!Ed)
	{
		return;
	}
	const bool bTrash = IsOver(Trash, AbsoluteMouse);
	if (Kind == EDragKind::Board)
	{
		if (bTrash)
		{
			if (BoardDrag.bFromWorld)
			{
				Ed->TrashDrag(BoardDrag);
                UHexAudioSubsystem::Play(this, TEXT("Back_Click"));
				TrashScaleVelocity -= 6.f;	// gulp
				TrashFlash = 1.f;
			}
			else
			{
				Ed->CancelDrag(BoardDrag);
			}
		}
		else if (IsOverUI(AbsoluteMouse) || !Ed->DropUnderMouse(BoardDrag))
		{
            UHexAudioSubsystem::PlayExact(this, TEXT("Error_Buzz_01"));
			Ed->CancelDrag(BoardDrag);
		}
        else { UHexAudioSubsystem::Play(this, TEXT("Pop")); }
	}
	else if (Kind == EDragKind::Card)
	{
		TArray<FHexCard>& Deck = Ed->EditDefinition().Deck;
		FVector2D Marker = FVector2D::ZeroVector;
		if (IsOver(DeckScroll, AbsoluteMouse))
		{
			int32 Insert = GetDeckInsertIndex(AbsoluteMouse, Marker);
			if (CardDragFrom != INDEX_NONE && Deck.IsValidIndex(CardDragFrom))
			{
				Deck.RemoveAt(CardDragFrom);	// a reorder
				Insert -= Insert > CardDragFrom ? 1 : 0;
			}
			Insert = FMath::Clamp(Insert, 0, Deck.Num());
			Deck.Insert(CardDragCard, Insert);
            UHexAudioSubsystem::Play(this, TEXT("play_a_card"));
			Ed->NotifyChanged();
			RebuildDeckView(Insert, false);
		}
		else if (CardDragFrom != INDEX_NONE && (bTrash || IsOver(PickerGrid, AbsoluteMouse)))
		{
			if (Deck.IsValidIndex(CardDragFrom))
			{
				UHexAudioSubsystem::Play(this, TEXT("Back_Click"));
				Deck.RemoveAt(CardDragFrom);	// back to the library / into the trash
			}
			if (bTrash)
			{
				TrashScaleVelocity -= 6.f;
				TrashFlash = 1.f;
			}
			Ed->NotifyChanged();
			RebuildDeckView(INDEX_NONE, false);
		}
		else
		{
			RebuildDeckView(INDEX_NONE, false);	// dropped nowhere: the card stays where it was
		}
	}
}

void UHexEditorWidget::CancelAll()
{
	if (DragKind == EDragKind::Board)
	{
		if (AHexLevelEditor* Ed = Editor.Get())
		{
			Ed->CancelDrag(BoardDrag);
		}
	}
	DragKind = EDragKind::None;
	if (DragChip)
	{
		DragChip->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DragCardBox)
	{
		DragCardBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (InsertMarker)
	{
		InsertMarker->SetVisibility(ESlateVisibility::Collapsed);
	}
	RebuildDeckView(INDEX_NONE, false);
}

// ================================================================ input

FReply UHexEditorWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FKey Button = InMouseEvent.GetEffectingButton();
	AHexLevelEditor* Ed = Editor.Get();
	if (Ed && DragKind == EDragKind::None && !IsOverUI(InMouseEvent.GetScreenSpacePosition()))
	{
		if (Button == EKeys::LeftMouseButton)
		{
			FHexEditorDrag Drag;
			if (Ed->PickUnderMouse(Drag))
			{
				DragKind = EDragKind::Board;
				BoardDrag = Drag;
				DragChipPos = AbsoluteToRoot(InMouseEvent.GetScreenSpacePosition());
				UpdateDragVisual(InMouseEvent.GetScreenSpacePosition());
				return FReply::Handled().CaptureMouse(TakeWidget());
			}
		}
		else if (Button == EKeys::RightMouseButton || Button == EKeys::MiddleMouseButton)
		{
			DragKind = EDragKind::Pan;
			return FReply::Handled().CaptureMouse(TakeWidget());
		}
	}
	return FReply::Handled().SetUserFocus(TakeWidget());
}

FReply UHexEditorWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (DragKind == EDragKind::Pan)
	{
		DragKind = EDragKind::None;
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (DragKind != EDragKind::None && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FinishDrag(InMouseEvent.GetScreenSpacePosition());
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply UHexEditorWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	LastMouseAbsolute = InMouseEvent.GetScreenSpacePosition();
	if (DragKind == EDragKind::Pan)
	{
		if (AHexLevelEditor* Ed = Editor.Get())
		{
			Ed->Pan(InMouseEvent.GetCursorDelta());
		}
		return FReply::Handled();
	}
	if (DragKind == EDragKind::Board || DragKind == EDragKind::Card)
	{
		UpdateDragVisual(LastMouseAbsolute);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UHexEditorWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	AHexLevelEditor* Ed = Editor.Get();
	if (!Ed || IsOverUI(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Unhandled();
	}
	FIntPoint Cell = FIntPoint::ZeroValue;
	if (DragKind == EDragKind::None && Ed->GetHoveredTile(Cell))
	{
		const int32 Delta=InMouseEvent.GetWheelDelta()>0.f?1:-1;
		if(InMouseEvent.IsShiftDown()) Ed->RotateScenery(Cell,Delta);
		else Ed->ChangeRank(Cell,Delta);
		if (const FHexTileDef* Tile=Ed->GetDefinition().FindTile(Cell)) if(Tile->IsScenery())
			ShowToast(Tile->GetSceneryName()+(Tile->CanRotateScenery()?FHexLocalization::Printf(HEXT(" · 朝向 %d° · Shift+滚轮旋转60°，滚轮切换"),Tile->SceneryRotationSteps*60):HEXT(" · 滚轮切换，拖到删除区移除造景")));
	}
	else
	{
		Ed->Zoom(InMouseEvent.GetWheelDelta());
	}
	return FReply::Handled();
}

FReply UHexEditorWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	AHexLevelEditor* Ed = Editor.Get();
	if (Key == EKeys::Escape)
	{
		if (DragKind != EDragKind::None)
		{
			CancelAll();
		}
		else if (EscapeArmed > 0.f)
		{
			if (UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this))
			{
				Flow->OpenMenu(EHexMenuScreen::Main);
			}
		}
		else
		{
			EscapeArmed = 2.f;
			ShowToast(HEXT("再按一次 ESC 返回主菜单（未保存的修改会丢失）"), true);
		}
		return FReply::Handled();
	}
	if (Key == EKeys::S && InKeyEvent.IsControlDown())
	{
		Save(false);
		return FReply::Handled();
	}
	FIntPoint Cell = FIntPoint::ZeroValue;
	if (!Ed || DragKind != EDragKind::None || !Ed->GetHoveredTile(Cell))
	{
		return FReply::Unhandled();
	}
	// Digits: 1 shows A at once and waits a moment for a 0 (= 10); a lone 0 does nothing.
	static const FKey Digits[10] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	static const FKey Numpad[10] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
	for (int32 Digit = 0; Digit < 10; ++Digit)
	{
		if (Key != Digits[Digit] && Key != Numpad[Digit])
		{
			continue;
		}
		if (Digit == 0)
		{
			if (PendingOne > 0.f && PendingOneCell == Cell)
			{
				Ed->SetRank(Cell, 10);
				PendingOne = 0.f;
			}
			return FReply::Handled();
		}
		Ed->SetRank(Cell, Digit);
		PendingOne = Digit == 1 ? 0.8f : 0.f;
		PendingOneCell = Cell;
		return FReply::Handled();
	}
	if (Key == EKeys::A)
	{
		Ed->SetRank(Cell, 1);
		return FReply::Handled();
	}
	const FKey SuitKeys[4] = { EKeys::S, EKeys::H, EKeys::C, EKeys::D };
	const EHexSuit SuitValues[4] = { EHexSuit::Spade, EHexSuit::Heart, EHexSuit::Club, EHexSuit::Diamond };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (Key == SuitKeys[Index])
		{
			Ed->SetSuit(Cell, SuitValues[Index]);
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}

// ================================================================ tick

void UHexEditorWidget::ShowToast(const FString& Message, bool bError)
{
    if (bError) UHexAudioSubsystem::PlayExact(this, TEXT("Error_Buzz_04"));
	if (!Toast || !ToastText)
	{
		return;
	}
	ToastText->SetText(HexEd::Txt(Message));
	Toast->SetBrush(FHexUIStyle::Panel(bError ? FHexUIStyle::Bad : FHexUIStyle::Good, 12.f));
	Toast->SetVisibility(ESlateVisibility::HitTestInvisible);
	ToastTime = 2.6f;
}

void UHexEditorWidget::UpdateTrash(float DeltaTime, const FVector2D& AbsoluteMouse)
{
	if (!Trash)
	{
		return;
	}
	const bool bDragging = DragKind == EDragKind::Board || DragKind == EDragKind::Card;
	const bool bHot = bDragging && IsOver(Trash, AbsoluteMouse);
	const bool bUseful = bDragging && (DragKind == EDragKind::Card ? CardDragFrom != INDEX_NONE : BoardDrag.bFromWorld);
	TrashFlash = FMath::Max(0.f, TrashFlash - DeltaTime * 3.f);
	// Kit buttons: dark at rest, gold-tinted when a drag could be deleted here, red under the cursor.
	const FLinearColor Tint = FMath::Lerp(bUseful && !bHot ? FLinearColor(1.25f, 1.1f, 0.7f, 1.f) : FLinearColor::White, FLinearColor(1.6f, 1.2f, 1.1f, 1.f), TrashFlash);
	Trash->SetBrush(FHexUIStyle::Skin(bHot && bUseful ? EHexSkin::ButtonRed : EHexSkin::ButtonDark, 0.62f, Tint));
	Trash->SetPadding(FMargin(10.f, 8.f, 10.f, 16.f));
	if (TrashSub)
	{
		TrashSub->SetText(HexEd::Txt(bHot && bUseful ? HEXT("松手删除") : HEXT("拖到这里")));
	}
	const float Breath = bUseful && !bHot ? 1.f + 0.03f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 6.f) : 1.f;
	HexEd::Spring(TrashScale, TrashScaleVelocity, bHot && bUseful ? 1.14f : Breath, DeltaTime, 360.f, 18.f);
	Trash->SetRenderScale(FVector2D(TrashScale, TrashScale));
}

void UHexEditorWidget::UpdateRankArrows(float DeltaTime)
{
	AHexLevelEditor* Ed = Editor.Get();
	APlayerController* PC = GetOwningPlayer();
	FIntPoint Cell = FIntPoint::ZeroValue;
	const bool bShow = Ed && PC && DragKind == EDragKind::None && Ed->GetHoveredTile(Cell);
	ArrowAlpha = FMath::FInterpTo(ArrowAlpha, bShow ? 1.f : 0.f, DeltaTime, 14.f);
	if (bShow)
	{
		ArrowCell = Cell;
	}
	FVector2D Screen = FVector2D::ZeroVector;
	const bool bProjected = Ed && PC && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Ed->GetLabelWorldLocation(ArrowCell), Screen, false);
	for (UHexMenuButton* Arrow : { RankUp.Get(), RankDown.Get() })
	{
		if (!Arrow)
		{
			continue;
		}
		const bool bVisible = bProjected && ArrowAlpha > 0.02f;
		Arrow->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Arrow->SetRenderOpacity(ArrowAlpha);
		const float Pop = 0.6f + 0.4f * ArrowAlpha;
		Arrow->SetRenderScale(FVector2D(Pop, Pop));
		if (UCanvasPanelSlot* ArrowSlot = Cast<UCanvasPanelSlot>(Arrow->Slot))
		{
			ArrowSlot->SetPosition(Screen + FVector2D(0.0, Arrow == RankUp ? -80.0 : 70.0));	// above / below the face
		}
	}
}

void UHexEditorWidget::UpdateStatus(float DeltaTime)
{
	AHexLevelEditor* Ed = Editor.Get();
	if (!Ed || !StatusPill || !StatusText)
	{
		return;
	}
	const int32 Serial = Ed->GetChangeSerial();
	if (Serial != RequestedSerial)
	{
		RequestedSerial = Serial;
		CheckDelay = 0.35f;	// wait for the edits to settle
	}
	CheckDelay -= DeltaTime;
	if (!bCheckRunning && CheckedSerial != RequestedSerial && CheckDelay <= 0.f)
	{
		bCheckRunning = true;
		CheckedSerial = RequestedSerial;
		const FHexLevelDefinition Snapshot = Ed->GetDefinition();
		const int32 MaxStates = UHexSokobanSettings::Get().SolverMaxStates;
		const int32 MaxNodes = UHexSokobanSettings::Get().DeckSolverMaxNodes;
		const int32 ForSerial = CheckedSerial;
		TWeakObjectPtr<UHexEditorWidget> WeakThis(this);
		Async(EAsyncExecution::ThreadPool, [WeakThis, Snapshot, MaxStates, MaxNodes, ForSerial]()
		{
			const FHexLevelCheck Check = UHexLevelTools::CheckLevel(Snapshot, MaxStates, MaxNodes);
			AsyncTask(ENamedThreads::GameThread, [WeakThis, Check, ForSerial]()
			{
				if (UHexEditorWidget* Widget = WeakThis.Get())
				{
					Widget->LastCheck = Check;
					Widget->LastCheckSerial = ForSerial;
					Widget->bCheckRunning = false;
				}
			});
		});
	}

	FString Text;
	FLinearColor Color = FHexUIStyle::InkSoft;
	if (Ed->GetDefinition().NeedsPerformanceMode() || (LastCheckSerial == RequestedSerial && LastCheck.bBudgetHit))
	{
		Text = HEXT("大地图 · 性能模式（不计算路线）");
	}
	else if (bCheckRunning || LastCheckSerial != RequestedSerial)
	{
		Text = HEXT("计算中…");
	}
	else if (!LastCheck.bPlayable)
	{
		Text = LastCheck.Problem;
		Color = FLinearColor(0.72f, 0.42f, 0.f, 1.f);
	}
	else if (LastCheck.GeometricSteps < 0 && LastCheck.bBudgetHit)
	{
		Text = HEXT("关卡较大，暂时算不出路线，可以先试玩。");
	}
	else if (LastCheck.GeometricSteps < 0)
	{
		Text = HEXT("箱子无法全部到达终点，请调整布局。");
		Color = FHexUIStyle::Bad;
	}
	else if (Ed->GetDefinition().Deck.Num() == 0)
	{
		Text = FHexLocalization::Printf(HEXT("还没配置手牌，这个布局最少需要 %d 步。"), LastCheck.GeometricSteps);
		Color = FLinearColor(0.72f, 0.42f, 0.f, 1.f);
	}
	else if (LastCheck.DeckTurns >= 0)
	{
		Text = FHexLocalization::Printf(HEXT("有解　·　最少 %d 回合"), LastCheck.DeckTurns);
		Color = FHexUIStyle::Good;
	}
	else if (LastCheck.bBudgetHit)
	{
		Text = HEXT("手牌组合太多，未能验证");
	}
	else
	{
		Text = HEXT("现有手牌无法通关，请调整卡组。");
		Color = FHexUIStyle::Bad;
	}
	StatusText->SetText(HexEd::Txt(Text));
	StatusPill->SetBrush(FHexUIStyle::Panel(Color, 13.f));
}

void UHexEditorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float DeltaTime = FMath::Min(InDeltaTime, 0.05f);
	AHexLevelEditor* Ed = Editor.Get();
	const FVector2D Mouse = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetCursorPos() : LastMouseAbsolute;

	if (Ed)
	{
		// The board is framed in the part of the screen the panel leaves free.
		Ed->RightPanelPixels = 572.f * UWidgetLayoutLibrary::GetViewportScale(this);
		// Over a rank arrow the hover is frozen (the arrow may sit above a neighbouring tile).
		const bool bOverArrow = IsOver(RankUp, Mouse) || IsOver(RankDown, Mouse);
		if (!bOverArrow)
		{
			Ed->UpdateHover(DragKind == EDragKind::None && !IsOverUI(Mouse));
		}
		if (DragKind == EDragKind::Board)
		{
			Ed->UpdateDragPreview(BoardDrag, IsOverUI(Mouse));
		}
		if (ShownDeckCount != Ed->GetDefinition().Deck.Num() && DragKind != EDragKind::Card)
		{
			RebuildDeckView(INDEX_NONE, false);
		}
	}

	// Drag chips follow the cursor on a spring (a little lag + tilt reads as "carried").
	if (DragKind == EDragKind::Board || DragKind == EDragKind::Card)
	{
		const FVector2D Target = AbsoluteToRoot(Mouse);
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(DeltaTime / (1.f / 120.f)));
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const double Dt = DeltaTime / Steps;
			DragChipVelocity += ((Target - DragChipPos) * 600.0 - DragChipVelocity * 30.0) * Dt;
			DragChipPos += DragChipVelocity * Dt;
		}
		const float Tilt = FMath::Clamp(static_cast<float>(DragChipVelocity.X) * 0.02f, -16.f, 16.f);
		const float Shrink = bOverTrash ? 0.8f : 1.f;
		for (UWidget* Visual : { static_cast<UWidget*>(DragChip.Get()), static_cast<UWidget*>(DragCardBox.Get()) })
		{
			if (UCanvasPanelSlot* VisualSlot = Visual ? Cast<UCanvasPanelSlot>(Visual->Slot) : nullptr)
			{
				VisualSlot->SetPosition(DragChipPos);
				Visual->SetRenderTransformAngle(Tilt);
				Visual->SetRenderScale(FVector2D(Shrink, Shrink));
			}
		}
	}

	// Cursor of the editor's own area (the 3D board, and everything while a drag holds the mouse):
	// grab while dragging / panning, hand over something you can pick up, else the default arrow.
	{
		FIntPoint HoverCell;
		EMouseCursor::Type Wanted = EMouseCursor::Default;
		if (DragKind != EDragKind::None)
		{
			Wanted = EMouseCursor::GrabHandClosed;
		}
		else if (Ed && !IsOverUI(Mouse) && Ed->GetHoveredTile(HoverCell))
		{
			Wanted = EMouseCursor::Hand;
		}
		if (GetCursor() != Wanted)
		{
			SetCursor(Wanted);
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().QueryCursor();
			}
		}
	}

	PendingOne = FMath::Max(0.f, PendingOne - DeltaTime);
	EscapeArmed = FMath::Max(0.f, EscapeArmed - DeltaTime);
	if (ToastTime > 0.f && Toast)
	{
		ToastTime -= DeltaTime;
		const float Age = 2.6f - ToastTime;
		const float Pop = 1.f + 0.2f * FMath::Exp(-9.f * Age) * FMath::Cos(Age * 26.f);
		Toast->SetRenderScale(FVector2D(Pop, Pop));
		Toast->SetRenderOpacity(FMath::Clamp(ToastTime / 0.3f, 0.f, 1.f));
		if (ToastTime <= 0.f)
		{
			Toast->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	UpdateTrash(DeltaTime, Mouse);
	UpdateRankArrows(DeltaTime);
	UpdateStatus(DeltaTime);
}

// ================================================================ save / test

FHexLevelDefinition UHexEditorWidget::GetDefinitionForSave() const
{
	FHexLevelDefinition Definition = Editor.IsValid() ? Editor->GetDefinition() : FHexLevelDefinition();
	if (NameBox && !NameBox->GetText().IsEmpty())
	{
		Definition.DisplayName = NameBox->GetText().ToString();
	}
	Definition.DealMode = EHexDealMode::Designed;
	Definition.bPerformanceMode = Definition.NeedsPerformanceMode() || (LastCheckSerial == RequestedSerial && LastCheck.bBudgetHit);
	Definition.CachedOptimalTurns = (LastCheckSerial == RequestedSerial) ? LastCheck.DeckTurns : -1;
	return Definition;
}

void UHexEditorWidget::Save(bool bBuiltIn)
{
	AHexLevelEditor* Ed = Editor.Get();
	UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this);
	if (!Ed || !Flow)
	{
		return;
	}
	FHexLevelDefinition Definition = GetDefinitionForSave();
	FString Reason;
	if (!Definition.IsPlayable(&Reason))
	{
		ShowToast(FHexLocalization::Printf(HEXT("保存前请检查：%s"), *Reason), true);
		return;
	}
	// Solvability is not required (skill cards may change it later) - it is only shown.
	const FString File = bBuiltIn ? Flow->SaveBuiltInLevel(Definition) : Flow->SaveCustomLevel(Definition);
	if (File.IsEmpty())
	{
		ShowToast(HEXT("没能保存成功，请重试。"), true);
		return;
	}
	Ed->EditDefinition().LevelId = Definition.LevelId;
	Ed->EditDefinition().DisplayName = Definition.DisplayName;
	Ed->EditDefinition().CreatedUtc = Definition.CreatedUtc;
	Flow->EditorLevelId = Definition.LevelId;
	UE_LOG(LogHexSokoban, Log, TEXT("Editor saved '%s' -> %s"), *Definition.DisplayName, *File);
	ShowToast(bBuiltIn ? HEXT("已存为内置关卡") : HEXT("已保存　·　可以在「选择关卡」里游玩"));
}

void UHexEditorWidget::TestPlay()
{
	UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this);
	const FHexLevelDefinition Definition = GetDefinitionForSave();
	FString Reason;
	if (!Flow || !Definition.IsPlayable(&Reason))
	{
		ShowToast(FHexLocalization::Printf(HEXT("试玩前请检查：%s"), *Reason), true);
		return;
	}
	Flow->TestPlay(Definition);
}
