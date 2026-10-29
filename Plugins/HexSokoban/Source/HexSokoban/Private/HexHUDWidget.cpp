#include "HexHUDWidget.h"
#include "HexLocalization.h"
#include "HexCardInfoWidget.h"
#include "HexAudioSubsystem.h"
#include "HexMoveNarration.h"
#include "HexUIStyle.h"
#include "HexSokobanGameMode.h"
#include "HexGameFlowSubsystem.h"
#include "HexBoardSubsystem.h"
#include "HexCardSubsystem.h"
#include "HexSokobanSettings.h"
#include "HexSokobanPlayerController.h"
#include "HexHandWidget.h"
#include "HexTile.h"
#include "HexGridLibrary.h"
#include "HexGridMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Framework/Application/SlateApplication.h"
#include "HexCardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"

namespace
{
	FText Txt(const TCHAR* Text) { return FText::FromString(Text); }
}

TSharedRef<SWidget> UHexHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultTree();
	}
	return Super::RebuildWidget();
}

void UHexHUDWidget::BuildDefaultTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDCanvas"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	auto MakeText = [this](const TCHAR* Name, int32 Size, bool bBold, const FLinearColor& Color)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetFont(FHexUIStyle::Font(Size, bBold));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	};
	auto MakeBorder = [this](const TCHAR* Name, const FLinearColor& Fill, float Radius, const FMargin& InPadding)
	{
		UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Border->SetBrush(FHexUIStyle::Panel(FLinearColor::White, Radius));
		Border->SetBrushColor(Fill);
		Border->SetPadding(InPadding);
		Border->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Border;
	};

	// ---- Top-left: level card + rule card ----
	UVerticalBox* LeftColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LeftColumn"));
	USizeBox* LeftWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LeftColumnWidth"));
	LeftWidth->SetWidthOverride(360.f);
	LeftWidth->SetContent(LeftColumn);
	UCanvasPanelSlot* LeftSlot = Root->AddChildToCanvas(LeftWidth);
	LeftSlot->SetAnchors(FAnchors(0.f, 0.f));
	LeftSlot->SetPosition(FVector2D(24.0, 20.0));
	LeftSlot->SetAutoSize(true);

	UBorder* LevelCard = MakeBorder(TEXT("LevelCard"), FLinearColor::White, 14.f, FMargin(0.f));
	LevelCard->SetBrush(FHexUIStyle::Skin(EHexSkin::FrameDefault, 0.6f));	// kit stage frame
	LeftColumn->AddChildToVerticalBox(LevelCard);
	UHorizontalBox* CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LevelCardRow"));
	LevelCard->SetContent(CardRow);
	USizeBox* Stripe = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AccentStripe"));
	Stripe->SetWidthOverride(8.f);
	UBorder* StripeFill = MakeBorder(TEXT("AccentStripeFill"), FHexUIStyle::Accent, 4.f, FMargin(0.f));
	Stripe->SetContent(StripeFill);
	CardRow->AddChildToHorizontalBox(Stripe)->SetVerticalAlignment(VAlign_Fill);
	Stripe->SetVisibility(ESlateVisibility::Collapsed);	// the kit frame replaces the accent stripe

	UVerticalBox* CardBody = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LevelCardBody"));
	UHorizontalBoxSlot* BodySlot = CardRow->AddChildToHorizontalBox(CardBody);
	BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	BodySlot->SetHorizontalAlignment(HAlign_Fill);
	BodySlot->SetPadding(FMargin(26.f, 16.f, 30.f, 22.f));

	UHorizontalBox* LevelRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LevelRow"));
	CardBody->AddChildToVerticalBox(LevelRow);
	LevelText = MakeText(TEXT("LevelText"), 24, true, FHexUIStyle::Ink);
	LevelText->SetFont(FHexUIStyle::OutlinedFont(24, 1));
	LevelRow->AddChildToHorizontalBox(LevelText)->SetVerticalAlignment(VAlign_Bottom);
	LevelTotalText = MakeText(TEXT("LevelTotalText"), 15, false, FHexUIStyle::InkSoft);
	UHorizontalBoxSlot* TotalSlot = LevelRow->AddChildToHorizontalBox(LevelTotalText);
	TotalSlot->SetVerticalAlignment(VAlign_Bottom);
	TotalSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 3.f));

	UHorizontalBox* MovesRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MovesRow"));
	CardBody->AddChildToVerticalBox(MovesRow)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	UTextBlock* MovesLabel = MakeText(TEXT("MovesLabel"), 15, false, FHexUIStyle::InkSoft);
	MovesLabel->SetText(Txt(HEXT("步数")));
	MovesRow->AddChildToHorizontalBox(MovesLabel)->SetVerticalAlignment(VAlign_Center);
	MovesText = MakeText(TEXT("MovesText"), 30, true, FHexUIStyle::Accent);
	MovesText->SetFont(FHexUIStyle::OutlinedFont(30, 2));
	MovesRow->AddChildToHorizontalBox(MovesText)->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));

	UHorizontalBox* GoalRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GoalRow"));
	CardBody->AddChildToVerticalBox(GoalRow)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	UTextBlock* GoalLabel = MakeText(TEXT("GoalLabel"), 15, false, FHexUIStyle::InkSoft);
	GoalLabel->SetText(Txt(HEXT("目标")));
	GoalRow->AddChildToHorizontalBox(GoalLabel)->SetVerticalAlignment(VAlign_Center);
	GoalPips = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GoalPips"));
	UHorizontalBoxSlot* PipsSlot = GoalRow->AddChildToHorizontalBox(GoalPips);
	PipsSlot->SetVerticalAlignment(VAlign_Center);
	PipsSlot->SetPadding(FMargin(12.f, 0.f, 8.f, 0.f));
	GoalCountText = MakeText(TEXT("GoalCountText"), 15, true, FHexUIStyle::Ink);
	GoalRow->AddChildToHorizontalBox(GoalCountText)->SetVerticalAlignment(VAlign_Center);

	RulePanel = MakeBorder(TEXT("RulePanel"), FHexUIStyle::PanelDark, 12.f, FMargin(16.f, 10.f));
	LeftColumn->AddChildToVerticalBox(RulePanel)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	UTextBlock* RuleText = MakeText(TEXT("RuleText"), 15, false, FHexUIStyle::TextLight);
	RuleText->SetText(Txt(HEXT("普通牌：同花色或同点数即可移动。\n推箱子时，看箱顶的花色与数字。")));
	RuleText->SetWrapTextAt(0.f);
	RuleText->SetAutoWrapText(true);
	RuleText->SetLineHeightPercentage(1.15f);
	RulePanel->SetContent(RuleText);

	UBorder* DrawRule = MakeBorder(TEXT("SkillDrawRule"), FHexUIStyle::PanelDark, 12.f, FMargin(14.f, 10.f));
	UTextBlock* DrawText = MakeText(TEXT("SkillDrawRuleText"), 14, false, FHexUIStyle::TextLight);
	const UHexBoardSubsystem* DrawBoard = GetWorld() ? GetWorld()->GetSubsystem<UHexBoardSubsystem>() : nullptr;
	DrawText->SetText(Txt(DrawBoard && DrawBoard->IsPerformanceMode()
		? HEXT("抽技能牌\n普通牌与目标地台花色、数字完全相同，成功移动后抽取右下角顶牌。\n技能用后回队，顶牌不变；花色按地形数量加权。")
		: HEXT("抽技能牌\n普通牌与目标地台花色、数字完全相同，成功移动后抽取右下角顶牌。\n本轮抽完，已使用的技能牌回到牌堆。")));
	DrawText->SetWrapTextAt(220.f);
	DrawRule->SetContent(DrawText);
	UCanvasPanelSlot* DrawRuleSlot = Root->AddChildToCanvas(DrawRule);
	DrawRuleSlot->SetAnchors(FAnchors(1.f, 0.55f));
	DrawRuleSlot->SetAlignment(FVector2D(1.f, 0.f));
	DrawRuleSlot->SetPosition(FVector2D(-24.f, 0.f));
	DrawRuleSlot->SetAutoSize(true);

	// ---- Top centre: control hint ----
	UBorder* HintBar = MakeBorder(TEXT("HintBar"), FHexUIStyle::PanelDark, 18.f, FMargin(20.f, 7.f));
	UCanvasPanelSlot* HintSlot = Root->AddChildToCanvas(HintBar);
	HintSlot->SetAnchors(FAnchors(0.5f, 0.f));
	HintSlot->SetAlignment(FVector2D(0.5, 0.0));
	HintSlot->SetPosition(FVector2D(0.0, 18.0));
	HintSlot->SetAutoSize(true);
	HintText = MakeText(TEXT("HintText"), 15, false, FHexUIStyle::TextLight);
	HintBar->SetContent(HintText);

	// ---- Stuck warning ----
	StuckPanel = MakeBorder(TEXT("StuckPanel"), FHexUIStyle::Bad, 20.f, FMargin(28.f, 10.f));
	UCanvasPanelSlot* StuckSlot = Root->AddChildToCanvas(StuckPanel);
	StuckSlot->SetAnchors(FAnchors(0.5f, 0.f));
	StuckSlot->SetAlignment(FVector2D(0.5, 0.0));
	StuckSlot->SetPosition(FVector2D(0.0, 70.0));
	StuckSlot->SetAutoSize(true);
	StuckText = MakeText(TEXT("StuckText"), 22, true, FLinearColor::White);
	StuckPanel->SetContent(StuckText);
	StuckPanel->SetVisibility(ESlateVisibility::Collapsed);

	// ---- Fail banner: the kit's boss-warning art (red screen glow + crest banner + skull) ----
	UImage* BannerGlow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BannerGlow"));
	BannerGlow->SetBrush(FHexUIStyle::Image(TEXT("07_PalyBoss/play_boss_bg_glow"), FVector2D(218.0, 500.0)));
	BannerGlow->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* GlowSlot = Root->AddChildToCanvas(BannerGlow);
	GlowSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GlowSlot->SetOffsets(FMargin(0.f));

	BannerPanel = MakeBorder(TEXT("BannerPanel"), FLinearColor::White, 0.f, FMargin(0.f));
	BannerPanel->SetBrush(FSlateNoResource());
	UCanvasPanelSlot* BannerSlot = Root->AddChildToCanvas(BannerPanel);
	BannerSlot->SetAnchors(FAnchors(0.f, 0.42f, 1.f, 0.42f));
	BannerSlot->SetAlignment(FVector2D(0.0, 0.5));
	BannerSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, BannerHeight));
	BannerSlot->SetAutoSize(true);	// grow with the text; BannerHeight only matters if auto size is turned off
	BannerPanel->SetHorizontalAlignment(HAlign_Center);
	BannerPanel->SetVerticalAlignment(VAlign_Center);
	BannerPanel->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	const float CrestScale = 1.15f;	// play_boss_title_bg is 803 x 406; its dark band runs from y 130 to 295
	USizeBox* Crest = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BannerCrest"));
	Crest->SetWidthOverride(803.f * CrestScale);
	Crest->SetHeightOverride(406.f * CrestScale);
	BannerPanel->SetContent(Crest);
	UOverlay* CrestLayers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BannerLayers"));
	Crest->AddChild(CrestLayers);
	UImage* CrestArt = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BannerCrestArt"));
	CrestArt->SetBrush(FHexUIStyle::Image(TEXT("07_PalyBoss/play_boss_title_bg"), FVector2D(803.0, 406.0) * CrestScale));
	CrestLayers->AddChildToOverlay(CrestArt);
	UImage* Skull = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BannerSkull"));
	Skull->SetBrush(FHexUIStyle::Image(TEXT("07_PalyBoss/play_boss_title_icon"), FVector2D(82.0, 65.0) * CrestScale));
	Skull->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	if (UOverlaySlot* SkullSlot = CrestLayers->AddChildToOverlay(Skull))
	{
		SkullSlot->SetHorizontalAlignment(HAlign_Center);
		SkullSlot->SetVerticalAlignment(VAlign_Top);
		SkullSlot->SetPadding(FMargin(0.f, (87.f - 33.f) * CrestScale, 0.f, 0.f));	// sits in the crest's red disc
	}
	UVerticalBox* BannerBody = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BannerBody"));
	if (UOverlaySlot* BannerBodySlot = CrestLayers->AddChildToOverlay(BannerBody))
	{
		BannerBodySlot->SetHorizontalAlignment(HAlign_Center);
		BannerBodySlot->SetVerticalAlignment(VAlign_Top);
		BannerBodySlot->SetPadding(FMargin(0.f, 128.f * CrestScale, 0.f, 0.f));
	}
	BannerTitle = MakeText(TEXT("BannerTitle"), 48, true, FLinearColor::White);
	BannerTitle->SetFont(FHexUIStyle::OutlinedFont(48, 3, FLinearColor(0.12f, 0.f, 0.f, 1.f)));
	BannerTitle->SetShadowOffset(FVector2D(0.0, 3.0));
	BannerTitle->SetShadowColorAndOpacity(FLinearColor(0.6f, 0.f, 0.f, 0.6f));
	BannerTitle->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	BannerBody->AddChildToVerticalBox(BannerTitle)->SetHorizontalAlignment(HAlign_Center);
	BannerLine1 = MakeText(TEXT("BannerLine1"), 24, true, FHexUIStyle::Accent);
	BannerBody->AddChildToVerticalBox(BannerLine1)->SetHorizontalAlignment(HAlign_Center);
	BannerLine2 = MakeText(TEXT("BannerLine2"), 17, false, FHexUIStyle::TextLight);
	UVerticalBoxSlot* Line2Slot = BannerBody->AddChildToVerticalBox(BannerLine2);
	Line2Slot->SetHorizontalAlignment(HAlign_Center);
	Line2Slot->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	BannerPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UHexHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Also extend existing Blueprint HUD trees, rather than only the native fallback.
	if (!LevelNameText && LevelTotalText)
	{
		if (UHorizontalBox* LevelRow = Cast<UHorizontalBox>(LevelTotalText->GetParent()))
		{
			LevelNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LevelNameText"));
			LevelNameText->SetFont(FHexUIStyle::Font(15, true));
			LevelNameText->SetColorAndOpacity(FSlateColor(FHexUIStyle::Accent));
			LevelNameText->SetJustification(ETextJustify::Center);
			LevelNameText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
			USizeBox* NameBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			NameBox->SetContent(LevelNameText);
			UHorizontalBoxSlot* NameSlot = LevelRow->AddChildToHorizontalBox(NameBox);
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			NameSlot->SetHorizontalAlignment(HAlign_Fill);
			NameSlot->SetVerticalAlignment(VAlign_Bottom);
			NameSlot->SetPadding(FMargin(8.f, 0.f, 8.f, 3.f));
		}
	}
	BuildNarrationPanel();
	if (!CardInfoPanel)
	{
		if (UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget))
		{
			CardInfoPanel = CreateWidget<UHexCardInfoWidget>(GetOwningPlayer(), CardInfoClass ? CardInfoClass.Get() : UHexCardInfoWidget::StaticClass());
			UCanvasPanelSlot* InfoSlot = Root->AddChildToCanvas(CardInfoPanel);
			InfoSlot->SetPosition(FVector2D(24, 280));
			InfoSlot->SetAutoSize(true);
			InfoSlot->SetZOrder(2);
			CardInfoPanel->SetVisibility(ESlateVisibility::Hidden);
			CardInfoPanel->SetRenderOpacity(0.f);
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetSubsystem<UHexCardSubsystem>()->OnCardPlayed.AddUniqueDynamic(this, &UHexHUDWidget::HandleNarrationCard);
		World->GetSubsystem<UHexBoardSubsystem>()->OnBoardReset.AddUniqueDynamic(this, &UHexHUDWidget::ClearNarration);
	}
	if (!ResultCard)
	{
		BuildResultPanel();
	}
}

void UHexHUDWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		if (UHexCardSubsystem* Cards = World->GetSubsystem<UHexCardSubsystem>())
			Cards->OnCardPlayed.RemoveDynamic(this, &UHexHUDWidget::HandleNarrationCard);
		if (UHexBoardSubsystem* Board = World->GetSubsystem<UHexBoardSubsystem>())
			Board->OnBoardReset.RemoveDynamic(this, &UHexHUDWidget::ClearNarration);
	}
	ClearNarration();
	Super::NativeDestruct();
}

void UHexHUDWidget::BuildNarrationPanel()
{
	if (NarrationCanvas || !WidgetTree) return;
	UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!Root) return;
	NarrationCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MoveNarration"));
	NarrationCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	NarrationCanvas->SetClipping(EWidgetClipping::ClipToBounds);
	UCanvasPanelSlot* FeedSlot = Root->AddChildToCanvas(NarrationCanvas);
	FeedSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	FeedSlot->SetOffsets(FMargin(0.f));
	FeedSlot->SetZOrder(5); // Above the board, below result/pause overlays.
}

void UHexHUDWidget::HandleNarrationCard(const FHexCard& Card, AHexTile* TargetTile)
{
	// PayForStep broadcasts only after movement validation, with the actual entered tile.
	// Discards and refused moves do not fire this event, including blocked pushes.
	if (!NarrationCanvas || !TargetTile) return;
	const FText Line = Card.Kind == EHexCardKind::Skill
		? FText::FromString(Card.Rank == 12 ? HEXT("伊蕾娜借助传送阵，瞬间抵达了远方的同花色地台。")
			: Card.Rank == 11 ? HEXT("伊蕾娜原地挥出疾风刃，将前方箱子击飞。")
			: HEXT("伊蕾娜施展疾速飞行，沿直线冲向同花色地台。"))
		: HexMoveNarration::Message(TargetTile->GetCardLabel().Suit, Card.Suit);
	if (Line.IsEmpty()) return;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	// Match the existing top control hint's brush, radius, padding and typography.
	Panel->SetBrush(FHexUIStyle::Panel(FHexUIStyle::PanelDark, 18.f));
	Panel->SetPadding(FMargin(20.f, 7.f));
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	Panel->SetRenderOpacity(0.f);
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(Line);
	Text->SetFont(FHexUIStyle::Font(15, false));
	Text->SetColorAndOpacity(FSlateColor(FHexUIStyle::TextLight));
	Text->SetJustification(ETextJustify::Left);
	Text->SetAutoWrapText(false);
	Text->SetWrapTextAt(320.f);
	Panel->SetContent(Text);
	UCanvasPanelSlot* FeedSlot = NarrationCanvas->AddChildToCanvas(Panel);
	FeedSlot->SetAnchors(FAnchors(1.f, 0.34f));
	FeedSlot->SetAlignment(FVector2D(1.f, 1.f));
	FeedSlot->SetPosition(FVector2D(-24.f, 24.f));
	FeedSlot->SetSize(FVector2D(440.f, 52.f));
	FHexNarrationEntry Entry;
	Entry.Panel = Panel;
	NarrationEntries.Add(Entry); // Never restart another entry's age when pushing it up.
}

void UHexHUDWidget::ClearNarration()
{
	if (NarrationCanvas) NarrationCanvas->ClearChildren();
	NarrationEntries.Reset();
}

void UHexHUDWidget::TickNarration(float DeltaTime, const FVector2D& ViewSize)
{
	if (!NarrationCanvas) return;
	const float Dt = GetWorld() && GetWorld()->IsPaused() ? 0.f : DeltaTime;
	for (int32 I = NarrationEntries.Num() - 1; I >= 0; --I)
	{
		FHexNarrationEntry& Entry = NarrationEntries[I];
		Entry.Age += Dt;
		if (!Entry.Panel.IsValid() || Entry.Age >= HexMoveNarration::Lifetime)
		{
			if (Entry.Panel.IsValid()) Entry.Panel->RemoveFromParent();
			NarrationEntries.RemoveAt(I);
		}
	}
	const float Width = FMath::Clamp(ViewSize.X * 0.23f, 240.f, 360.f);
	float TargetY = 0.f;
	// Stack by measured wrapped heights; position changes never restart an entry's age.
	for (int32 I = NarrationEntries.Num() - 1; I >= 0; --I)
	{
		FHexNarrationEntry& Entry = NarrationEntries[I];
		UBorder* Panel = Entry.Panel.Get();
		UTextBlock* Text = CastChecked<UTextBlock>(Panel->GetContent());
		Text->SetWrapTextAt(Width - 40.f);
		Text->ForceLayoutPrepass();
		const float Height = FMath::CeilToFloat(Text->GetDesiredSize().Y) + 14.f;
		Entry.Y = FMath::Lerp(Entry.Y, TargetY, 1.f - FMath::Exp(-12.f * Dt));
		const float Exit = FMath::Clamp((Entry.Age - HexMoveNarration::EnterSeconds - HexMoveNarration::HoldSeconds) / HexMoveNarration::ExitSeconds, 0.f, 1.f);
		Panel->SetRenderOpacity(HexMoveNarration::Opacity(Entry.Age));
		UCanvasPanelSlot* FeedSlot = CastChecked<UCanvasPanelSlot>(Panel->Slot);
		FeedSlot->SetPosition(FVector2D(-24.f, 26.f + Entry.Y - 24.f * Exit));
		FeedSlot->SetSize(FVector2D(Width, Height));
		TargetY -= Height + 8.f;
	}
}

void UHexHUDWidget::BuildResultPanel()
{
	UCanvasPanel* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Root)
	{
		return;
	}
	auto MakeText = [this](int32 Size, bool bBold, const FLinearColor& Color)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetFont(FHexUIStyle::Font(Size, bBold));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	};
	auto Centered = [](UVerticalBoxSlot* InSlot, float Top)
	{
		InSlot->SetHorizontalAlignment(HAlign_Center);
		InSlot->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
	};

	// Dim the board behind the card.
	ResultOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultOverlay"));
	ResultOverlay->SetBrushColor(FHexUIStyle::ScreenDim);
	ResultOverlay->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* OverlaySlot = Root->AddChildToCanvas(ResultOverlay);
	OverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	OverlaySlot->SetOffsets(FMargin(0.f));
	OverlaySlot->SetZOrder(50);

	// Character-select kit frame, shared with the editor and pause menu.
	ResultCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultCard"));
	ResultCard->SetPadding(FHexUIStyle::SkinPadding(EHexSkin::Popup, 0.42f) + FMargin(4.f, 6.f, 4.f, 4.f));
	ResultCard->SetVisibility(ESlateVisibility::Collapsed);
	ResultCard->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(ResultCard);
	CardSlot->SetAnchors(FAnchors(0.5f, 0.44f));
	CardSlot->SetAlignment(FVector2D(0.5, 0.5));
	CardSlot->SetAutoSize(true);
	CardSlot->SetZOrder(51);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ResultColumn"));
	ResultCard->SetContent(Column);
	FHexUIStyle::DecoratePanel(WidgetTree, ResultCard);
	USizeBox* Stripe = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ResultStripe"));
	Stripe->SetHeightOverride(8.f);
	UBorder* StripeFill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultStripeFill"));
	StripeFill->SetBrush(FHexUIStyle::Panel(FHexUIStyle::Accent, 4.f));
	Stripe->SetContent(StripeFill);
	UVerticalBoxSlot* StripeSlot = Column->AddChildToVerticalBox(Stripe);
	StripeSlot->SetHorizontalAlignment(HAlign_Fill);
	Stripe->SetVisibility(ESlateVisibility::Collapsed);	// the ribbon title replaces the stripe

	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ResultBody"));
	Column->AddChildToVerticalBox(Body)->SetPadding(FMargin(28.f, 0.f, 28.f, 6.f));

	// "Stage clear" on the kit's big ribbon, riding over the frame's top edge.
	UBorder* Ribbon = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultRibbon"));
	Ribbon->SetBrush(FHexUIStyle::Skin(EHexSkin::RibbonResult, 0.82f));
	// Compensate for the font's line-box bearing and the ribbon's lower folded tails.
	Ribbon->SetPadding(FMargin(0.f, 7.f, 0.f, 21.f));
	Ribbon->SetHorizontalAlignment(HAlign_Center);
	Ribbon->SetVerticalAlignment(VAlign_Center);
	UTextBlock* Title = MakeText(40, true, FHexUIStyle::Ink);
	Title->SetFont(FHexUIStyle::OutlinedFont(40, 2, FLinearColor(0.10f, 0.03f, 0.25f, 1.f)));
	Title->SetShadowOffset(FVector2D(0.0, 2.0));
	Title->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f));
	Title->SetJustification(ETextJustify::Center);
	// Compact punctuation avoids the full-width exclamation mark's optical right-side gap.
	Title->SetText(Txt(HEXT("过关!")));
	Ribbon->SetContent(Title);
	USizeBox* RibbonFrame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());	// never squeeze the art
	const FSlateBrush RibbonArt = FHexUIStyle::Skin(EHexSkin::RibbonResult, 0.82f);
	// The brush fixes both 124px end caps; only its central strip gains width.
	RibbonFrame->SetWidthOverride(480.f);
	RibbonFrame->SetHeightOverride(RibbonArt.ImageSize.Y);
	RibbonFrame->AddChild(Ribbon);
	UVerticalBoxSlot* RibbonSlot = Body->AddChildToVerticalBox(RibbonFrame);
	RibbonSlot->SetHorizontalAlignment(HAlign_Center);
	RibbonSlot->SetPadding(FMargin(-90.f, -64.f, -90.f, 0.f));
	ResultLevelText = MakeText(16, true, FHexUIStyle::Accent);
	Centered(Body->AddChildToVerticalBox(ResultLevelText), 0.f);

	// Stars: middle one a bit higher and larger.
	UHorizontalBox* StarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ResultStars"));
	Centered(Body->AddChildToVerticalBox(StarRow), 8.f);
	ResultStars.Reset();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		const float StarSize = Index == 1 ? 104.f : 86.f;
		Cell->SetWidthOverride(StarSize + 8.f);
		Cell->SetHeightOverride(112.f);
		// Kit result star: [0] soft glow behind, [1] the star (gold / dim), [2] invisible text kept as the animation handle.
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Layers->SetRenderTransformPivot(FVector2D(0.5, 0.5));
		Layers->SetVisibility(ESlateVisibility::HitTestInvisible);
		UImage* Glow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Glow->SetBrush(FHexUIStyle::Image(TEXT("09_Result/result_star_glow"), FVector2D(StarSize * 1.9f, StarSize * 1.53f)));
		if (UOverlaySlot* GlowSlot = Layers->AddChildToOverlay(Glow))
		{
			GlowSlot->SetHorizontalAlignment(HAlign_Center);
			GlowSlot->SetVerticalAlignment(VAlign_Center);
		}
		UImage* StarImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		StarImage->SetBrush(FHexUIStyle::Image(TEXT("09_Result/result_star_dim"), FVector2D(StarSize, StarSize * 169.0 / 163.0)));
		if (UOverlaySlot* ImageSlot = Layers->AddChildToOverlay(StarImage))
		{
			ImageSlot->SetHorizontalAlignment(HAlign_Center);
			ImageSlot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* Star = MakeText(12, true, FHexUIStyle::Accent);
		Star->SetVisibility(ESlateVisibility::Hidden);
		Layers->AddChildToOverlay(Star);
		if (USizeBoxSlot* StarSlot = Cast<USizeBoxSlot>(Cell->AddChild(Layers)))
		{
			StarSlot->SetHorizontalAlignment(HAlign_Center);
			StarSlot->SetVerticalAlignment(VAlign_Center);
		}
		StarRow->AddChildToHorizontalBox(Cell)->SetVerticalAlignment(VAlign_Center);
		ResultStars.Add(Star);
	}

	// Turns vs. optimal.
	UHorizontalBox* Stats = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ResultStats"));
	Centered(Body->AddChildToVerticalBox(Stats), 14.f);
	auto AddStat = [&](const TCHAR* Label, TObjectPtr<UTextBlock>& OutValue)
	{
		UVerticalBox* Stat = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock* LabelText = MakeText(15, false, FHexUIStyle::InkSoft);
		LabelText->SetText(Txt(Label));
		Stat->AddChildToVerticalBox(LabelText)->SetHorizontalAlignment(HAlign_Center);
		OutValue = MakeText(34, true, FHexUIStyle::Ink);
		OutValue->SetFont(FHexUIStyle::OutlinedFont(34, 2));
		Stat->AddChildToVerticalBox(OutValue)->SetHorizontalAlignment(HAlign_Center);
		Stats->AddChildToHorizontalBox(Stat)->SetPadding(FMargin(26.f, 0.f));
	};
	AddStat(HEXT("你的步数"), ResultTurnsText);
	USizeBox* Divider = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Divider->SetWidthOverride(2.f);
	Divider->SetHeightOverride(52.f);
	UBorder* DividerFill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	DividerFill->SetBrushColor(FLinearColor(0.55f, 0.50f, 0.75f, 0.45f));
	Divider->SetContent(DividerFill);
	Stats->AddChildToHorizontalBox(Divider)->SetVerticalAlignment(VAlign_Center);
	AddStat(HEXT("最优步数"), ResultBestText);

	ResultVerdictText = MakeText(18, true, FHexUIStyle::Ink);
	Centered(Body->AddChildToVerticalBox(ResultVerdictText), 10.f);
	ResultThresholdText = MakeText(14, false, FHexUIStyle::InkSoft);
	Centered(Body->AddChildToVerticalBox(ResultThresholdText), 4.f);

	// Controls, in the dark pill style of the top hint bar.
	UBorder* Footer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ResultFooter"));
	Footer->SetBrush(FHexUIStyle::Panel(FHexUIStyle::PanelDark, 16.f));
	Footer->SetPadding(FMargin(20.f, 7.f));
	ResultFooterText = MakeText(15, false, FHexUIStyle::TextLight);
	Footer->SetContent(ResultFooterText);
	Centered(Body->AddChildToVerticalBox(Footer), 18.f);
}

void UHexHUDWidget::UpdateResultPanel(bool bShow, const FHexLevelResult& Result, int32 LevelNumber, bool bHasNext, float Elapsed)
{
	if (!ResultCard)
	{
		return;
	}
	const ESlateVisibility NewVisibility = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	ResultCard->SetVisibility(NewVisibility);
	if (ResultOverlay)
	{
		ResultOverlay->SetVisibility(NewVisibility);
	}
	if (!bShow)
	{
        PlayedResultStarSounds = 0;
		return;
	}
	const bool bHasBaseline = Result.Baseline >= 0;

	// Dim in, card pops with a little overshoot.
	if (ResultOverlay)
	{
		ResultOverlay->SetRenderOpacity(FMath::Clamp(Elapsed / 0.25f, 0.f, 1.f));
	}
	const float Pop = 1.f - 0.18f * FMath::Exp(-8.f * Elapsed) * FMath::Cos(Elapsed * 14.f);
	ResultCard->SetRenderScale(FVector2D(Pop, Pop));
	ResultCard->SetRenderOpacity(FMath::Clamp(Elapsed / 0.12f, 0.f, 1.f));

	if (ResultLevelText)
	{
		ResultLevelText->SetText(FText::FromString(FHexLocalization::Printf(HEXT("第 %d 关"), LevelNumber)));
	}
	// Turns count up, then the stars land one by one.
	if (ResultTurnsText)
	{
		const float Count = FMath::Clamp((Elapsed - 0.15f) / 0.5f, 0.f, 1.f);
		ResultTurnsText->SetText(FText::AsNumber(FMath::RoundToInt(static_cast<float>(Result.Turns) * (1.f - FMath::Square(1.f - Count)))));
	}
	if (ResultBestText)
	{
		ResultBestText->SetText(bHasBaseline ? FText::AsNumber(Result.Baseline) : FText::FromString(TEXT("\u2014")));
	}
	for (int32 Index = 0; Index < ResultStars.Num(); ++Index)
	{
		UTextBlock* StarHandle = ResultStars[Index];
		UOverlay* Layers = StarHandle ? Cast<UOverlay>(StarHandle->GetParent()) : nullptr;
		if (!Layers || Layers->GetChildrenCount() < 2)
		{
			continue;
		}
		const bool bEarned = Index < Result.Stars;
		const float Local = Elapsed - (0.55f + 0.22f * static_cast<float>(Index));
        if (bEarned && Local > 0.f && !(PlayedResultStarSounds & (1 << Index)))
        {
            static const FName StarSounds[] = { TEXT("Coins_10"), TEXT("Coins_11"), TEXT("Coins_13") };
            if (Index < UE_ARRAY_COUNT(StarSounds)) UHexAudioSubsystem::PlayExact(this, StarSounds[Index]);
            PlayedResultStarSounds |= (1 << Index);
        }
		UImage* Glow = Cast<UImage>(Layers->GetChildAt(0));
		UImage* StarImage = Cast<UImage>(Layers->GetChildAt(1));
		const float StarSize = Index == 1 ? 104.f : 86.f;
		if (StarImage)
		{
			const bool bLitNow = bEarned && Local > 0.f;
			StarImage->SetBrush(FHexUIStyle::Image(bLitNow ? TEXT("09_Result/result_star") : TEXT("09_Result/result_star_dim"), FVector2D(StarSize, StarSize * 169.0 / 163.0)));
		}
		if (Glow)
		{
			// The glow blooms when the star lands, then breathes softly.
			const float Bloom = bEarned && Local > 0.f ? FMath::Clamp(Local / 0.25f, 0.f, 1.f) * (0.75f + 0.25f * FMath::Sin(Elapsed * 3.f + Index)) : 0.f;
			Glow->SetRenderOpacity(Bloom);
		}
		float Scale = 1.f;
		if (bEarned)
		{
			Scale = Local <= 0.f ? 1.f : 1.f + 0.45f * FMath::Exp(-9.f * Local) * FMath::Cos(Local * 15.f);	// slam in, overshoot, settle
		}
		Layers->SetRenderOpacity(FMath::Clamp(Elapsed / 0.3f, 0.f, 1.f));
		Layers->SetRenderScale(FVector2D(FMath::Max(0.f, Scale), FMath::Max(0.f, Scale)));
		Layers->SetRenderTranslation(FVector2D(0.0, Index == 1 ? -10.0 : 0.0));
		Layers->SetRenderTransformAngle(bEarned && Local > 0.f ? 12.f * FMath::Exp(-7.f * Local) * FMath::Sin(Local * 12.f) : 0.f);
	}

	const float VerdictAlpha = FMath::Clamp((Elapsed - 1.15f) / 0.2f, 0.f, 1.f);
	if (ResultVerdictText)
	{
		FString Verdict;
		FLinearColor Color = FHexUIStyle::Ink;
		if (!bHasBaseline)
		{
			Verdict = HEXT("最优解未知");
			Color = FHexUIStyle::InkSoft;
		}
		else if (Result.Turns <= Result.Baseline)
		{
			Verdict = HEXT("完美！和最优解一样");
			Color = FHexUIStyle::Good;
		}
		else
		{
			Verdict = FHexLocalization::Printf(HEXT("多走了 %d 步"), Result.Turns - Result.Baseline);
			Color = Result.Stars >= 3 ? FHexUIStyle::Good : (Result.Stars == 2 ? FHexUIStyle::Accent : FHexUIStyle::Bad);
		}
		ResultVerdictText->SetText(FText::FromString(Verdict));
		ResultVerdictText->SetColorAndOpacity(FSlateColor(Color));
		ResultVerdictText->SetRenderOpacity(VerdictAlpha);
	}
	if (ResultThresholdText)
	{
		ResultThresholdText->SetVisibility(bHasBaseline ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		ResultThresholdText->SetText(FText::FromString(FHexLocalization::Printf(HEXT("★★★ ≤ %d 步　·　★★ ≤ %d 步"), Result.ThreeStarTurns, Result.TwoStarTurns)));
		ResultThresholdText->SetRenderOpacity(VerdictAlpha);
	}
	if (ResultFooterText)
	{
		const AHexSokobanGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHexSokobanGameMode>() : nullptr;
		const UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this);
		const float Remaining = GameMode ? GameMode->GetAutoAdvanceRemaining() : -1.f;
		if (GameMode && GameMode->IsDataDrivenLevel())
		{
			FString Where;
			if (Remaining >= 0.f)
			{
				const int32 Seconds = FMath::CeilToInt(Remaining);
				Where = Flow && Flow->IsTestPlaying() ? FHexLocalization::Printf(HEXT("%d 秒后返回编辑器"), Seconds)	// back to the editor in N s
					: (bHasNext ? FHexLocalization::Printf(HEXT("%d 秒后进入下一关"), Seconds)							// next level in N s
						: FHexLocalization::Printf(HEXT("%d 秒后返回关卡选择"), Seconds));							// level select in N s
				Where += TEXT("\u3000\u00b7\u3000");
			}
			// "... \u00b7 click / Enter: continue \u00b7 R: play again \u00b7 Esc: menu"
			ResultFooterText->SetText(FText::FromString(Where + HEXT("点击 / 回车：继续　·　R：再玩一次　·　ESC 菜单")));
		}
		else
		{
			ResultFooterText->SetText(bHasNext
				? Txt(HEXT("点击 / 回车：下一关　·　R：再玩一次"))
				: Txt(HEXT("全部关卡完成！点击：回到第一关　·　R：再玩一次")));
		}
	}
}

void UHexHUDWidget::RefreshGoalPips(int32 Goals, int32 Covered)
{
	if (!GoalPips || (Goals == ShownGoals && Covered == ShownCovered))
	{
		return;
	}
	ShownGoals = Goals;
	ShownCovered = Covered;
	GoalPips->ClearChildren();
	for (int32 Index = 0; Index < Goals; ++Index)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(22.f);
		Box->SetHeightOverride(24.f);
		UImage* Pip = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		const bool bOn = Index < Covered;
		Pip->SetBrush(FHexUIStyle::Image(bOn ? TEXT("06_Stage/stage_star") : TEXT("06_Stage/stage_star_dim"), FVector2D(22.0, 24.0)));
		Box->SetContent(Pip);
		GoalPips->AddChildToHorizontalBox(Box)->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	}
	if (GoalCountText)
	{
		GoalCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Covered, Goals)));
	}
}

void UHexHUDWidget::ShowBanner(bool bShow, const FText& Title, const FText& Line1, const FText& Line2, const FLinearColor& Color, float Elapsed)
{
	if (!BannerPanel)
	{
		return;
	}
	if (!bShow)
	{
		BannerPanel->SetVisibility(ESlateVisibility::Collapsed);
		if (UWidget* Glow = WidgetTree->FindWidget(TEXT("BannerGlow")))
		{
			Glow->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	BannerPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (BannerTitle) { BannerTitle->SetText(Title); }
	if (BannerLine1) { BannerLine1->SetText(Line1); }
	if (BannerLine2) { BannerLine2->SetText(Line2); }

	// Band unfolds, title pops with a little overshoot.
	const float Appear = FMath::Clamp(Elapsed / 0.25f, 0.f, 1.f);
	const float Ease = 1.f - FMath::Pow(1.f - Appear, 3.f);
	BannerPanel->SetRenderScale(FVector2D(1.0, FMath::Max(0.01f, Ease)));
	// Boss-warning dressing: pulsing red screen glow and a throbbing skull (failure banners only).
	const bool bWarning = Color.Equals(FHexUIStyle::Bad);
	if (UImage* Glow = Cast<UImage>(WidgetTree->FindWidget(TEXT("BannerGlow"))))
	{
		Glow->SetVisibility(bWarning ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		const float Beat = 0.82f + 0.18f * FMath::Sin(Elapsed * 5.f);
		Glow->SetRenderOpacity(0.62f * Ease * Beat);
	}
	if (UWidget* Skull = WidgetTree->FindWidget(TEXT("BannerSkull")))
	{
		Skull->SetVisibility(bWarning ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		const float Throb = 1.f + 0.08f * FMath::Max(0.f, FMath::Sin(Elapsed * 5.f));
		Skull->SetRenderScale(FVector2D(Throb, Throb));
	}
	if (BannerTitle)
	{
		const float Pop = 1.f + 0.3f * FMath::Exp(-Elapsed * 7.f) * FMath::Cos(Elapsed * 18.f);
		BannerTitle->SetRenderScale(FVector2D(Pop, Pop));
	}
}

namespace HexHudCursor
{
	/** Is the mouse over a tile a click would move the player onto (next to it, and payable by a card)? */
	bool IsMoveTargetUnderCursor(APlayerController* PC, const UHexBoardSubsystem* Board, const UHexCardSubsystem* Cards)
	{
		const AHexSokobanPlayerController* HexPC = Cast<AHexSokobanPlayerController>(PC);
		const UHexGridMovementComponent* Mover = HexPC ? HexPC->GetHexMover() : nullptr;
		if (!Mover || !Board || !Mover->IsOnBoard() || !PC->GetPawn())
		{
			return false;
		}
		double PlaneZ = PC->GetPawn()->GetActorLocation().Z;
		if (const AHexTile* Current = Board->GetTileAt(Mover->GetCoord()))
		{
			PlaneZ = Current->GetSurfaceLocation().Z;
		}
		FVector RayOrigin, RayDirection;
		if (!PC->DeprojectMousePositionToWorld(RayOrigin, RayDirection) || FMath::IsNearlyZero(RayDirection.Z))
		{
			return false;
		}
		const double T = (PlaneZ - RayOrigin.Z) / RayDirection.Z;
		if (T <= 0.0)
		{
			return false;
		}
		AHexTile* Tile = Board->GetTileAt(UHexGridLibrary::WorldToCoord(RayOrigin + RayDirection * T));
		if (!Tile)
		{
			return false;
		}
		return Cards && Cards->IsActive() ? Cards->GetEnterableNeighbors(Cards->GetSelectedCardIndex()).Contains(Tile)
			: UHexGridLibrary::GetHexDistance(Tile->GetCoord(), Mover->GetCoord()) == 1;
	}

	/**
	 * Software cursors (Project Settings > User Interface): Default = idle, Hand = something the mouse can act
	 * on, GrabHandClosed = pressing / dragging it. UMG buttons carry their own; everything else (cards, HUD
	 * panels, the 3D board) falls back to the player controller's cursor, set here.
	 */
	void Update(UUserWidget* Hud, APlayerController* PC, const AHexSokobanGameMode* GameMode, const UHexBoardSubsystem* Board, const UHexCardSubsystem* Cards)
	{
		if (!PC)
		{
			return;
		}
		const AHexSokobanPlayerController* HexPC = Cast<AHexSokobanPlayerController>(PC);
		UHexHandWidget* Hand = HexPC ? HexPC->GetHandWidget() : nullptr;
		const bool bMouseDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);
		EMouseCursor::Type Wanted = EMouseCursor::Default;
		if (PC->IsPaused())
		{
			Wanted = EMouseCursor::Default;	// the pause menu's buttons set their own
		}
		else if (Hand && Hand->IsInteracting())
		{
			Wanted = EMouseCursor::GrabHandClosed;	// a card is held / dragged, wherever the mouse is
		}
		else if (Hand && Hand->IsMouseOverHand())
		{
			Wanted = bMouseDown ? EMouseCursor::GrabHandClosed : EMouseCursor::Hand;
		}
		else if (GameMode && GameMode->GetLevelState() != EHexLevelState::Playing)
		{
			Wanted = EMouseCursor::Hand;	// result / fail card: click anywhere to continue
		}
		else if (IsMoveTargetUnderCursor(PC, Board, Cards))
		{
			Wanted = bMouseDown ? EMouseCursor::GrabHandClosed : EMouseCursor::Hand;
		}
		// Hit-testable UMG (cards, HUD panels) answers Slate's cursor query before the viewport does, so the
		// hand and the HUD carry the same cursor as the controller.
		bool bChanged = PC->CurrentMouseCursor != Wanted;
		PC->CurrentMouseCursor = Wanted;
		for (UUserWidget* Widget : { static_cast<UUserWidget*>(Hand), Hud })
		{
			if (Widget && Widget->GetCursor() != Wanted)
			{
				Widget->SetCursor(Wanted);
				bChanged = true;
			}
		}
		// Cards are their own widgets (and their Blueprint may set a cursor inside): give them the same one.
		if (Hand && Hand->WidgetTree)
		{
			Hand->WidgetTree->ForEachWidget([Wanted, &bChanged](UWidget* Child)
			{
				UHexCardWidget* CardWidget = Cast<UHexCardWidget>(Child);
				if (!CardWidget)
				{
					return;
				}
				if (CardWidget->GetCursor() != Wanted)
				{
					CardWidget->SetCursor(Wanted);
					bChanged = true;
				}
				if (CardWidget->WidgetTree)
				{
					CardWidget->WidgetTree->ForEachWidget([Wanted](UWidget* Inner)
					{
						if (Inner->GetCursor() != Wanted)
						{
							Inner->SetCursor(Wanted);
						}
					});
				}
			});
		}
		if (bChanged && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().QueryCursor();	// Slate only re-asks on mouse moves: show the change now
		}
	}
}

void UHexHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TickNarration(InDeltaTime, MyGeometry.GetLocalSize());

	UWorld* World = GetWorld();
	const UHexBoardSubsystem* Board = World ? World->GetSubsystem<UHexBoardSubsystem>() : nullptr;
	const AHexSokobanGameMode* GameMode = World ? World->GetAuthGameMode<AHexSokobanGameMode>() : nullptr;
	const UHexCardSubsystem* Cards = World ? World->GetSubsystem<UHexCardSubsystem>() : nullptr;
	const bool bCards = Cards && Cards->IsActive();
	if (CardInfoPanel)
	{
		const AHexSokobanPlayerController* HexPC = Cast<AHexSokobanPlayerController>(GetOwningPlayer());
		const UHexHandWidget* Hand = HexPC ? HexPC->GetHandWidget() : nullptr;
		FHexCard Inspected;
		const bool bInspect = bCards && Hand && !World->IsPaused() && GameMode
			&& GameMode->GetLevelState() == EHexLevelState::Playing && Hand->GetInspectedCard(Inspected);
		CardInfoPanel->UpdateInspection(bInspect ? &Inspected : nullptr, InDeltaTime);
		if (RulePanel && RulePanel->GetCachedGeometry().GetLocalSize().Y > 0)
		{
			const FGeometry& RuleGeometry = RulePanel->GetCachedGeometry();
			const FVector2D BelowRule = MyGeometry.AbsoluteToLocal(RuleGeometry.LocalToAbsolute(FVector2D(0, RuleGeometry.GetLocalSize().Y)));
			CastChecked<UCanvasPanelSlot>(CardInfoPanel->Slot)->SetPosition(BelowRule + FVector2D(0, 12));
		}
	}
	HexHudCursor::Update(this, GetOwningPlayer(), GameMode, Board, Cards);
	if (!Board)
	{
		return;
	}

	// Level / moves / goals.
	if (LevelText)
	{
		const int32 Index = GameMode ? GameMode->GetCurrentLevelIndex() : INDEX_NONE;
		const UHexGameFlowSubsystem* FlowSys = UHexGameFlowSubsystem::Get(this);
		LevelText->SetText(FlowSys && FlowSys->IsTestPlaying()
			? FText::FromString(HEXT("试玩"))
			: FText::FromString(FHexLocalization::Printf(HEXT("第 %d 关"), Index >= 0 ? Index + 1 : 1)));
	}
	if (LevelTotalText)
	{
		const int32 Total = GameMode ? GameMode->GetNumLevels() : 0;
		LevelTotalText->SetText(Total > 0 ? FText::FromString(FHexLocalization::Printf(HEXT("共 %d 关"), Total)) : FText::GetEmpty());
	}
	if (LevelNameText)
    {
        const FText Name = GameMode ? FText::FromString(GameMode->GetLevelDisplayName()) : FText::GetEmpty();
        LevelNameText->SetText(Name);
        LevelNameText->SetToolTipText(Name);
    }
	if (MovesText)
	{
		MovesText->SetText(FText::AsNumber(Board->GetMoveCount()));
	}
	RefreshGoalPips(Board->GetNumGoals(), Board->GetNumCoveredGoals());

	if (RulePanel)
	{
		RulePanel->SetVisibility(bCards ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (HintText)
	{
		if (bCards)
		{
			// Drag cards onto lit tiles \u00b7 H hint \u00b7 Z undo \u00b7 R restart \u00b7 Esc menu
			HintText->SetText(Txt(HEXT("把手牌拖到发亮的地台上移动　·　H 提示路线　·　Z 撤销　·　R 重来　·　ESC 菜单")));
		}
		else
		{
			HintText->SetText(Txt(HEXT("点击相邻地台或按 Q W E A S D 移动　·　Z 撤销　·　R 重来")));
		}
	}

	// Stuck warning.
	const bool bPlaying = GameMode && GameMode->GetLevelState() == EHexLevelState::Playing;
	if (StuckPanel)
	{
		const bool bStuck = bPlaying && GameMode->IsStuck();
		StuckPanel->SetVisibility(bStuck ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bStuck)
		{
			const float Pulse = 0.78f + 0.22f * FMath::Sin(World->GetTimeSeconds() * 10.f);
			StuckPanel->SetBrushColor(FLinearColor(FHexUIStyle::Bad.R, FHexUIStyle::Bad.G, FHexUIStyle::Bad.B, Pulse));
			const float Remaining = GameMode->GetStuckTimeRemaining();
			if (StuckText)
			{
				StuckText->SetText(Remaining >= 0.f
					? FText::FromString(FHexLocalization::Printf(HEXT("没有能打出的牌了。按 Z 撤销，%d 秒后重新开始。"), FMath::CeilToInt(Remaining)))
					: Txt(HEXT("没有能打出的牌了。按 Z 撤销，或按 R 重来。")));
			}
		}
	}

	// Banners.
	if (GameMode && GameMode->GetLevelState() == EHexLevelState::Failed)
	{
		const float Elapsed = World->GetTimeSeconds() - GameMode->GetFailedTime();
		if (GameMode->GetFailReason() == EHexFailReason::Stuck)
		{
			ShowBanner(true, Txt(HEXT("暂时走不动了")), Txt(HEXT("没有能打出的牌了，试试换个走法吧。")), Txt(HEXT("即将重新开始，也可以点击立即重来。")), FHexUIStyle::Bad, Elapsed);
		}
		else
		{
			// Out of cards / no card fits: wait for undo or restart.
			ShowBanner(true,
				GameMode->GetFailReason() == EHexFailReason::OutOfCards ? Txt(HEXT("手牌用完了")) : Txt(HEXT("暂时走不动了")),
				Txt(HEXT("还没把箱子全部推到终点，换个走法再试试。")),
				Txt(HEXT("按 Z 撤销上一步，或按 R 重新开始。")), FHexUIStyle::Bad, Elapsed);
		}
	}
	const bool bCompleted = GameMode && GameMode->GetLevelState() == EHexLevelState::Completed;
	if (bCompleted && !ResultCard)
	{
		// Fallback if the results card couldn't be built: the old banner.
		const float Elapsed = World->GetTimeSeconds() - GameMode->GetCompletedTime();
		ShowBanner(true, Txt(HEXT("过关！")), FText::FromString(FHexLocalization::Printf(HEXT("共 %d 步"), GameMode->GetMoveCount())), FText::GetEmpty(), FHexUIStyle::Good, Elapsed);
	}
	else if (!(GameMode && GameMode->GetLevelState() == EHexLevelState::Failed))
	{
		ShowBanner(false, FText::GetEmpty(), FText::GetEmpty(), FText::GetEmpty(), FLinearColor::White, 0.f);
	}
	const int32 LevelIndex = GameMode ? GameMode->GetCurrentLevelIndex() : INDEX_NONE;
	UpdateResultPanel(bCompleted, GameMode ? GameMode->GetLevelResult() : FHexLevelResult(), LevelIndex >= 0 ? LevelIndex + 1 : 1,
		GameMode && GameMode->HasNextLevel(), bCompleted ? World->GetTimeSeconds() - GameMode->GetCompletedTime() : 0.f);
}
