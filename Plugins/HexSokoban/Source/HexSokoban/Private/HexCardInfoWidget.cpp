#include "HexCardInfoWidget.h"
#include "HexLocalization.h"
#include "HexCardWidget.h"
#include "HexSokobanSettings.h"
#include "HexUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"

FHexCardInfo UHexCardInfoWidget::ResolveInfo_Implementation(const FHexCard& Card) const
{
	FHexCardInfo Info;
	auto Text = [](const TCHAR* Value) { return FText::FromString(Value); };
	if (Card.Kind != EHexCardKind::Number)
	{
		const FString Suit=Card.GetSuitSymbol();
		if (Card.Rank==12)
		{
			Info.Title=Text(HEXT("传送阵"));
			Info.Description=Text(HEXT("伊雷娜的传送魔法。借助同花色的传送阵，瞬间抵达远方，节省旅途的路程和时间。"));
			Info.Terrain=Text(HEXT("全地图同花色空地台"));
			Info.Condition=FText::FromString(FHexLocalization::Printf(HEXT("需要 %s\n无箱子，非脚下"),*Suit));
			Info.Effect=Text(HEXT("同花色瞬移 · 消耗6步"));
			return Info;
		}
		if (Card.Rank==11)
		{
			Info.Title=Text(HEXT("疾风刃"));
			Info.Description=Text(HEXT("伊雷娜原地向相邻箱子释放疾风刃。前方两格无阻时箱子推进两格；冲量会沿紧贴或隔一格的箱子传递，能动的箱子各前进一格，被顶住的箱子留在原地，不会拖累身后的箱子。目标箱前没有任何空位时无法释放。"));
			Info.Terrain=Text(HEXT("相邻同花色有箱地台"));
			Info.Condition=FText::FromString(FHexLocalization::Printf(HEXT("需要 %s\n目标箱前至少一格空位"),*Suit));
			Info.Effect=Text(HEXT("原地击飞箱子 · 消耗4步"));
			return Info;
		}
		if (Card.Rank==13)
		{
			Info.Title=Text(HEXT("疾速飞行"));
			Info.Description=Text(HEXT("伊雷娜沿六个方向之一疾速飞行，直达没有箱子的同花色地台。途中不能跨越空缺或箱子，不能推动箱子。"));
			Info.Terrain=Text(HEXT("直线内同花色空地台"));
			Info.Condition=FText::FromString(FHexLocalization::Printf(HEXT("需要 %s\n路径畅通，目标无箱子"),*Suit));
			Info.Effect=Text(HEXT("直线飞行，不推箱 · 消耗2步"));
			return Info;
		}
		Info.Title = Text(HEXT("技能牌"));
		Info.Description = Text(HEXT("这张卡牌的技能说明尚未配置。"));
		Info.Terrain = Text(HEXT("由技能决定"));
		Info.Condition = Text(HEXT("待配置"));
		Info.Effect = Text(HEXT("待配置"));
		return Info;
	}
	switch (Card.Suit)
	{
	case EHexSuit::Heart:
		Info.Title = Text(HEXT("指南针"));
		Info.Description = Text(HEXT("伊雷娜的指南针，伊雷娜在穿越草地和森林的时候会迷路，使用指南针可以帮助伊雷娜通过草地和森林。"));
		Info.Terrain = Text(HEXT("草地和森林"));
		break;
	case EHexSuit::Spade:
		Info.Title = Text(HEXT("篝火"));
		Info.Description = Text(HEXT("伊雷娜的露营装备。山路漫长而寒冷，伊雷娜在石洞中点燃篝火，休息取暖、恢复体力，再继续穿越山地。"));
		Info.Terrain = Text(HEXT("山地"));
		break;
	case EHexSuit::Club:
		Info.Title = Text(HEXT("贿赂守卫"));
		Info.Description = Text(HEXT("伊雷娜的贿赂，城镇需要魔法师徽章，伊雷娜徽章被偷走了，使用金钱贿赂守卫可以帮助伊雷娜进入城镇。"));
		Info.Terrain = Text(HEXT("城镇"));
		break;
	case EHexSuit::Diamond:
		Info.Title = Text(HEXT("帆船"));
		Info.Description = Text(HEXT("伊雷娜的船票，水域不能飞行通过，只能坐船，乘坐帆船可以帮助伊雷娜通过大片的水域。"));
		Info.Terrain = Text(HEXT("水域"));
		break;
	}
	Info.Condition = FText::FromString(FHexLocalization::Printf(HEXT("需要 %s 或者 %s"), *Card.GetSuitSymbol(), *Card.GetRankText()));
	Info.Effect = Text(HEXT("移动一格"));
	return Info;
}

TSharedRef<SWidget> UHexCardInfoWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(300.f);
		WidgetTree->RootWidget = Size;
		Frame = WidgetTree->ConstructWidget<UBorder>();
		Frame->SetBrush(FHexUIStyle::Panel(FLinearColor::White, 12.f));
		Frame->SetBrushColor(FHexUIStyle::PanelDark);
		Frame->SetPadding(FMargin(16.f));
		Size->SetContent(Frame);
		Body = WidgetTree->ConstructWidget<UVerticalBox>();
		Frame->SetContent(Body);
		auto MakeText = [this](int32 FontSize, bool Bold, FLinearColor Color)
		{
			UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
			Text->SetFont(FHexUIStyle::Font(FontSize, Bold));
			Text->SetColorAndOpacity(Color);
			Text->SetAutoWrapText(true);
			Text->SetJustification(ETextJustify::Left);
			return Text;
		};
		Title = MakeText(22, true, FHexUIStyle::Accent);
		Body->AddChildToVerticalBox(Title)->SetPadding(FMargin(0,0,0,12));
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Body->AddChildToVerticalBox(Row);
		USizeBox* CardBox = WidgetTree->ConstructWidget<USizeBox>();
		CardBox->SetWidthOverride(110.f);
		CardBox->SetHeightOverride(154.f);
		Row->AddChildToHorizontalBox(CardBox);
		UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>();
		Fit->SetStretch(EStretch::ScaleToFit);
		CardBox->SetContent(Fit);
		TSubclassOf<UHexCardWidget> CardClass = UHexSokobanSettings::Get().CardWidgetClass.TryLoadClass<UHexCardWidget>();
		if (!CardClass) CardClass = UHexCardWidget::StaticClass();
		Preview = CreateWidget<UHexCardWidget>(GetOwningPlayer(), CardClass);
		Fit->SetContent(Preview);
		UVerticalBox* Facts = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBoxSlot* FactsSlot = Row->AddChildToHorizontalBox(Facts);
		FactsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		FactsSlot->SetPadding(FMargin(14,4,0,0));
		auto AddFact = [&](const TCHAR* Label, UTextBlock*& Value)
		{
			UTextBlock* Caption = MakeText(12, false, FHexUIStyle::InkSoft);
			Caption->SetText(FText::FromString(Label));
			Facts->AddChildToVerticalBox(Caption)->SetPadding(FMargin(0,0,0,4));
			Value = MakeText(15, true, FHexUIStyle::TextLight);
			Facts->AddChildToVerticalBox(Value)->SetPadding(FMargin(0,0,0,12));
		};
		UTextBlock* Value = nullptr;
		AddFact(HEXT("能通过的地块"), Value); Terrain = Value;
		AddFact(HEXT("使用条件"), Value); Condition = Value;
		Description = MakeText(14, false, FHexUIStyle::TextLight);
		Description->SetLineHeightPercentage(1.15f);
		Body->AddChildToVerticalBox(Description)->SetPadding(FMargin(0,12,0,12));
		Effect = MakeText(15, true, FHexUIStyle::Accent);
		Body->AddChildToVerticalBox(Effect);
	}
	return Super::RebuildWidget();
}

void UHexCardInfoWidget::UpdateInspection(const FHexCard* Card, float DeltaTime)
{
	if (!Frame) return;
	if (Card && (!bHasCard || !DisplayedCard.SameFace(*Card)))
	{
		DisplayedCard = *Card;
		bHasCard = true;
		ContentProgress = 0.f;
		const FHexCardInfo Info = ResolveInfo(*Card);
		Title->SetText(Info.Title);
		Description->SetText(Info.Description);
		Terrain->SetText(Info.Terrain);
		Condition->SetText(Info.Condition);
		Effect->SetText(FText::Format(FText::FromString(HEXT("使用效果　{0}")), Info.Effect));
		Preview->SetCard(*Card);
		Preview->SetFaceDown(false);
		Preview->SetVisualState(EHexCardVisualState::Normal);
	}
	FrameProgress = FMath::Clamp(FrameProgress + DeltaTime * (Card ? 1.f / .28f : -1.f / .20f), 0.f, 1.f);
	if (Card && FrameProgress >= 1.f) ContentProgress = FMath::Min(1.f, ContentProgress + DeltaTime / .18f);
	if (FrameProgress <= 0.f) { bHasCard = false; ContentProgress = 0.f; }
	const float Ease = 1.f - FMath::Pow(1.f - FrameProgress, 3.f);
	SetRenderTranslation(FVector2D(-340.f * (1.f - Ease), 0.f));
	SetRenderOpacity(FrameProgress);
	Body->SetRenderOpacity(ContentProgress);
	SetVisibility(FrameProgress > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}
