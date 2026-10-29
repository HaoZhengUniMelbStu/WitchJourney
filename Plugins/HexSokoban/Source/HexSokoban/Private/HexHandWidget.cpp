#include "HexHandWidget.h"
#include "HexLocalization.h"
#include "HexAudioSubsystem.h"
#include "HexCardWidget.h"
#include "HexCardSubsystem.h"
#include "HexBoardSubsystem.h"
#include "HexGridMovementComponent.h"
#include "HexGridLibrary.h"
#include "HexSokobanSettings.h"
#include "HexSokobanGameMode.h"
#include "HexTile.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HexUIStyle.h"
#include "Engine/World.h"

namespace HexHand
{
	constexpr int32 ZDeckTray = 0;
	constexpr int32 ZDeckLayer = 1;
	constexpr int32 ZDeck = 2;
	constexpr int32 ZHandBase = 10;
	constexpr int32 ZHovered = 100;
	constexpr int32 ZFlying = 150;
	constexpr int32 ZDragged = 200;
	constexpr int32 ZDeckText = 300;
	constexpr int32 ZGuide = 400;
	constexpr float ReturnDuration = 0.35f;

	// Discard zone looks (index = UHexHandWidget::DiscardVisual).
	enum EDiscardVisual : int32
	{
		DiscardIdle,		// charges left, nothing dragged
		DiscardEmpty,		// no charges left
		DiscardArmed,		// a card is being dragged and could be discarded
		DiscardArmedEmpty,	// a card is being dragged but no charges left
		DiscardHover,		// dragged card over the zone: release discards
		DiscardHoverEmpty	// dragged card over the zone, not allowed
	};

	float EaseOutCubic(float T)
	{
		const float U = 1.f - FMath::Clamp(T, 0.f, 1.f);
		return 1.f - U * U * U;
	}
}

// ---------------------------------------------------------------- construction

TSharedRef<SWidget> UHexHandWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultTree();
	}
	return Super::RebuildWidget();
}

namespace HexHandMatch
{
	/** Hand index -> entry index. Duplicate faces (designed hands) each get their own entry. */
	template <typename TEntry, typename TIsLive>
	TArray<int32> MatchHand(const TArray<TEntry>& InEntries, const TArray<FHexCard>& Hand, TIsLive IsLive)
	{
		TArray<int32> Map;
		TArray<bool> Used;
		Used.Init(false, InEntries.Num());
		for (const FHexCard& Card : Hand)
		{
			int32 Found = INDEX_NONE;
			for (int32 Index = 0; Index < InEntries.Num(); ++Index)
			{
				if (!Used[Index] && !InEntries[Index].bDead && IsLive(InEntries[Index]) && InEntries[Index].Card.SameFace(Card))
				{
					Found = Index;
					break;
				}
			}
			if (Found != INDEX_NONE)
			{
				Used[Found] = true;
			}
			Map.Add(Found);
		}
		return Map;
	}
}

void UHexHandWidget::BuildDefaultTree()
{
	HandCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HandCanvas"));
	HandCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = HandCanvas;

	DeckCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DeckCountText"));
	DeckCountText->SetFont(FHexUIStyle::Font(26, true));
	DeckCountText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	DeckCountText->SetShadowOffset(FVector2D(2.0, 2.0));
	DeckCountText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	DeckCountText->SetJustification(ETextJustify::Center);
	DeckCountText->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* TextSlot = HandCanvas->AddChildToCanvas(DeckCountText);
	TextSlot->SetAnchors(FAnchors(1.f, 1.f));
	TextSlot->SetAlignment(FVector2D(0.5, 0.5));
	TextSlot->SetAutoSize(true);
	TextSlot->SetPosition(DeckOffset);
	TextSlot->SetZOrder(HexHand::ZDeckText);

	UTextBlock* DeckLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DeckLabel"));
	DeckLabel->SetFont(FHexUIStyle::Font(15, true));
	DeckLabel->SetColorAndOpacity(FSlateColor(FHexUIStyle::TextLight));
	DeckLabel->SetShadowOffset(FVector2D(1.0, 1.0));
	DeckLabel->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f));
	DeckLabel->SetText(FText::FromString(HEXT("牌堆")));
	DeckLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* LabelSlot = HandCanvas->AddChildToCanvas(DeckLabel);
	LabelSlot->SetAnchors(FAnchors(1.f, 1.f));
	LabelSlot->SetAlignment(FVector2D(0.5, 1.0));
	LabelSlot->SetAutoSize(true);
	LabelSlot->SetPosition(DeckOffset + FVector2D(0.0, -96.0));
	LabelSlot->SetZOrder(HexHand::ZDeckText);

	GuideBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GuideBox"));
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(12.0, 12.0, 12.0, 12.0), FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.35f)), 2.f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		GuideBox->SetBrush(Brush);
	}
	GuideBox->SetBrushColor(GuideMessageColor);
	GuideBox->SetPadding(FMargin(22.f, 10.f));
	GuideBox->SetVisibility(ESlateVisibility::Collapsed);
	GuideBox->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	GuideText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GuideText"));
	GuideText->SetFont(FHexUIStyle::Font(20, true));
	GuideText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	GuideText->SetJustification(ETextJustify::Center);
	GuideBox->SetContent(GuideText);
	UCanvasPanelSlot* GuideSlot = HandCanvas->AddChildToCanvas(GuideBox);
	GuideSlot->SetAnchors(FAnchors(0.5f, 1.f));
	GuideSlot->SetAlignment(FVector2D(0.5, 0.5));
	GuideSlot->SetAutoSize(true);
	GuideSlot->SetPosition(FVector2D(0.0, GuideMessageY));
	GuideSlot->SetZOrder(HexHand::ZGuide);
}

void UHexHandWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (!HandCanvas)
	{
		UE_LOG(LogHexSokoban, Warning, TEXT("%s: no CanvasPanel named HandCanvas - the hand cannot be shown."), *GetName());
		return;
	}
	if (!DeckWidget)
	{
		FHexCard BackCard;
		DeckWidget = CreateCardWidget(BackCard);
		if (DeckWidget)
		{
			DeckWidget->SetFaceDown(true);
			DeckWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UCanvasPanelSlot* DeckSlot = Cast<UCanvasPanelSlot>(DeckWidget->Slot))
			{
				DeckSlot->SetZOrder(HexHand::ZDeck);
				DeckSlot->SetPosition(GetDeckPosition());
			}
			CardWidgets.Remove(DeckWidget);
		}
		BuildDeckPile();
	}
	if (!DiscardZone)
	{
		BuildDiscardZone();
	}
	BindToCards();
}

void UHexHandWidget::NativeDestruct()
{
	UnbindFromCards();
	Super::NativeDestruct();
}

UHexCardSubsystem* UHexHandWidget::GetCards() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UHexCardSubsystem>() : nullptr;
}

void UHexHandWidget::BindToCards()
{
	UHexCardSubsystem* Cards = GetCards();
	if (!Cards || BoundCards.Get() == Cards)
	{
		return;
	}
	UnbindFromCards();
	BoundCards = Cards;
	Cards->OnHandChanged.AddDynamic(this, &UHexHandWidget::HandleHandChanged);
	Cards->OnHandDealt.AddDynamic(this, &UHexHandWidget::HandleHandDealt);
	Cards->OnCardPlayed.AddDynamic(this, &UHexHandWidget::HandleCardPlayed);
	Cards->OnCardDrawn.AddDynamic(this, &UHexHandWidget::HandleCardDrawn);
	Cards->OnMulligan.AddDynamic(this, &UHexHandWidget::HandleMulligan);
	Cards->OnSelectionChanged.AddDynamic(this, &UHexHandWidget::HandleSelectionChanged);
	Cards->OnPlayRejected.AddDynamic(this, &UHexHandWidget::HandlePlayRejected);
	Cards->OnGuideMessage.AddDynamic(this, &UHexHandWidget::HandleGuideMessage);
	Cards->OnCardDiscarded.AddDynamic(this, &UHexHandWidget::HandleCardDiscarded);
	Cards->OnDiscardChargesChanged.AddDynamic(this, &UHexHandWidget::HandleDiscardChargesChanged);

	if (Cards->IsActive())
	{
		HandleHandDealt();
	}
}

void UHexHandWidget::UnbindFromCards()
{
	if (UHexCardSubsystem* Cards = BoundCards.Get())
	{
		Cards->OnHandChanged.RemoveAll(this);
		Cards->OnHandDealt.RemoveAll(this);
		Cards->OnCardPlayed.RemoveAll(this);
		Cards->OnCardDrawn.RemoveAll(this);
		Cards->OnMulligan.RemoveAll(this);
		Cards->OnSelectionChanged.RemoveAll(this);
		Cards->OnPlayRejected.RemoveAll(this);
		Cards->OnGuideMessage.RemoveAll(this);
		Cards->OnCardDiscarded.RemoveAll(this);
		Cards->OnDiscardChargesChanged.RemoveAll(this);
	}
	BoundCards.Reset();
}

UHexCardWidget* UHexHandWidget::CreateCardWidget(const FHexCard& Card)
{
	if (!HandCanvas)
	{
		return nullptr;
	}
	TSubclassOf<UHexCardWidget> Class = CardWidgetClass;
	if (!Class)
	{
		Class = UHexSokobanSettings::Get().CardWidgetClass.TryLoadClass<UHexCardWidget>();
	}
	if (!Class)
	{
		Class = UHexCardWidget::StaticClass();
	}
	APlayerController* PC = GetOwningPlayer();
	UHexCardWidget* Widget = PC ? CreateWidget<UHexCardWidget>(PC, Class) : CreateWidget<UHexCardWidget>(GetWorld(), Class);
	if (!Widget)
	{
		return nullptr;
	}
	Widget->SetOwningHand(this);
	Widget->SetCard(Card);
	Widget->SetVisibility(ESlateVisibility::Visible);
	Widget->SetRenderTransformPivot(FVector2D(0.5, 0.5));

	UCanvasPanelSlot* CardSlot = HandCanvas->AddChildToCanvas(Widget);
	CardSlot->SetAnchors(FAnchors(0.5f, 1.f));
	CardSlot->SetAlignment(FVector2D(0.5, 0.5));
	CardSlot->SetAutoSize(false);
	CardSlot->SetSize(Widget->GetCardSize());
	CardWidgets.Add(Widget);
	return Widget;
}

// ---------------------------------------------------------------- events

void UHexHandWidget::HandleHandDealt()
{
	bool bHadCards = false;
	for (FCardEntry& Entry : Entries)
	{
		if (Entry.Mode == EEntryMode::InHand || Entry.Mode == EEntryMode::Drawing)
		{
			Entry.Mode = EEntryMode::ReturnToDeck;
			Entry.ModeTime = 0.f;
			bHadCards = true;
		}
	}
	PressedCard.Reset();
	bDragging = false;
	PendingDealDelay = bHadCards ? 0.3f : 0.15f;
	Reconcile(true);
}

void UHexHandWidget::HandleHandChanged()
{
	Reconcile(false);
}

void UHexHandWidget::HandleCardPlayed(const FHexCard& Card, AHexTile* TargetTile)
{
	// With duplicate faces in hand, the card that flies is the one the player dragged (or selected).
	int32 Index = PressedCard.IsValid() ? FindEntryByWidget(PressedCard.Get()) : INDEX_NONE;
	if (!Entries.IsValidIndex(Index) || !Entries[Index].Card.SameFace(Card))
	{
		Index = INDEX_NONE;
		const UHexCardSubsystem* Cards = GetCards();
		const TArray<int32> Live = GetLiveEntriesInHandOrder();
		const int32 Selected = Cards ? Cards->GetSelectedCardIndex() : INDEX_NONE;
		if (Live.IsValidIndex(Selected) && Entries[Live[Selected]].Card.SameFace(Card))
		{
			Index = Live[Selected];
		}
	}
	if (Index == INDEX_NONE)
	{
		Index = FindEntryByFace(Card, true);
	}
	if (Index != INDEX_NONE)
	{
		FCardEntry& Entry = Entries[Index];
		Entry.Mode = EEntryMode::FlyToTile;
		Entry.ModeTime = 0.f;
		Entry.FlyTime = 0.f;
		Entry.FlyFrom = Entry.Pos.Value;
		Entry.FlyAngleFrom = Entry.Angle.Value;
		Entry.FlipScaleX = 1.f;
		bool bOk = false;
		Entry.FlyTo = TargetTile ? WorldToHandSpace(TargetTile->GetSurfaceLocation() + FVector(0.0, 0.0, 30.0), bOk) : FVector2D::ZeroVector;
		if (!bOk)
		{
			Entry.FlyTo = Entry.FlyFrom + FVector2D(0.0, -300.0);
		}
		if (PressedCard.Get() == Entry.Widget.Get())
		{
			PressedCard.Reset();
			bDragging = false;
		}
	}
	// The rest of the hand gives a small springy bounce as it closes the gap.
	for (FCardEntry& Other : Entries)
	{
		if (!Other.bDead && Other.Mode == EEntryMode::InHand)
		{
			Other.Scale.Velocity += 1.6f;
		}
	}
	OnCardPlayedFx(Card, TargetTile);
}

void UHexHandWidget::HandleCardDiscarded(const FHexCard& Card)
{
	const int32 Index = FindEntryByFace(Card, true);
	if (Index != INDEX_NONE)
	{
		FCardEntry& Entry = Entries[Index];
		Entry.Mode = EEntryMode::FlyToTile;	// same flight as a played card, but into the discard zone
		Entry.ModeTime = 0.f;
		Entry.FlyTime = 0.f;
		Entry.FlyFrom = Entry.Pos.Value;
		Entry.FlyAngleFrom = Entry.Angle.Value;
		Entry.FlipScaleX = 1.f;
		Entry.FlyTo = GetDiscardPosition();
		if (PressedCard.Get() == Entry.Widget.Get())
		{
			PressedCard.Reset();
			bDragging = false;
		}
	}
	OnCardDiscardedFx(Card);
}

void UHexHandWidget::HandleDiscardChargesChanged()
{
	RefreshDiscardPips();
}

void UHexHandWidget::HandleCardDrawn(const FHexCard& Card)
{
	OnCardDrawnFx(Card);
}

void UHexHandWidget::HandleMulligan()
{
	PendingDealDelay = 0.35f;
	OnMulliganFx();
}

void UHexHandWidget::HandleSelectionChanged()
{
	UpdateVisualStates();
}

void UHexHandWidget::HandlePlayRejected()
{
	ShakeTime = ShakeDuration;
	OnRejectedFx();
}

void UHexHandWidget::HandleGuideMessage(const FText& Message, AHexTile* Tile)
{
	ShowGuideMessage(Message);

	// Float the message above the tile it is about (kept clear of the hand and the screen edges).
	UCanvasPanelSlot* GuideSlot = GuideBox ? Cast<UCanvasPanelSlot>(GuideBox->Slot) : nullptr;
	if (!GuideSlot || !Tile)
	{
		return;
	}
	bool bOk = false;
	FVector2D Anchor = WorldToHandSpace(Tile->GetSurfaceLocation() + FVector(0.0, 0.0, 140.0), bOk);
	if (!bOk)
	{
		return;
	}
	const FVector2D View = GetViewportSizeInSlate();
	const double HalfRoom = FMath::Max(0.0, View.X * 0.5 - 300.0);
	Anchor.X = FMath::Clamp(Anchor.X, -HalfRoom, HalfRoom);
	Anchor.Y = FMath::Clamp(Anchor.Y, -View.Y + 120.0, static_cast<double>(HandBaseY) - 170.0);
	GuideSlot->SetPosition(Anchor);
}

void UHexHandWidget::ShowGuideMessage(const FText& Message)
{
	if (GuideText)
	{
		GuideText->SetText(Message);
	}
	if (GuideBox)
	{
		GuideBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (UCanvasPanelSlot* GuideSlot = GuideBox ? Cast<UCanvasPanelSlot>(GuideBox->Slot) : nullptr)
	{
		GuideSlot->SetPosition(FVector2D(0.0, GuideMessageY));	// default spot; tile messages move it
	}
	GuideMessageTime = GuideMessageDuration;
	GuideMessageAge = 0.f;
	OnGuideMessageFx(Message);
}

void UHexHandWidget::UpdateGuideMessage(float DeltaTime)
{
	if (!GuideBox || GuideMessageTime <= 0.f)
	{
		return;
	}
	GuideMessageTime -= DeltaTime;
	GuideMessageAge += DeltaTime;
	if (GuideMessageTime <= 0.f)
	{
		GuideBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	// Pop in with a little overshoot, fade out at the end.
	const float Pop = 1.f + 0.22f * FMath::Exp(-9.f * GuideMessageAge) * FMath::Cos(GuideMessageAge * 28.f);
	const float In = FMath::Clamp(GuideMessageAge / 0.08f, 0.f, 1.f);
	GuideBox->SetRenderScale(FVector2D(Pop * In, Pop * In));
	GuideBox->SetRenderOpacity(FMath::Clamp(GuideMessageTime / 0.3f, 0.f, 1.f));
}

bool UHexHandWidget::IsPlayerBusy() const
{
	const UWorld* World = GetWorld();
	const UHexBoardSubsystem* Board = World ? World->GetSubsystem<UHexBoardSubsystem>() : nullptr;
	const UHexGridMovementComponent* Mover = Board ? Board->GetPlayerMover() : nullptr;
	return Mover && (Mover->IsMoving() || Mover->IsMovementLocked());
}

int32 UHexHandWidget::GetHandIndexOfWidget(const UHexCardWidget* Widget) const
{
	const int32 EntryIndex = FindEntryByWidget(Widget);
	const UHexCardSubsystem* Cards = GetCards();
	if (EntryIndex == INDEX_NONE || !Cards)
	{
		return INDEX_NONE;
	}
	const FHexCard& Card = Entries[EntryIndex].Card;
	return Cards->GetHand().IndexOfByPredicate([&Card](const FHexCard& Other) { return Other.SameFace(Card); });
}

void UHexHandWidget::UpdateGuide()
{
	UHexCardSubsystem* Cards = GetCards();
	if (!Cards)
	{
		return;
	}
	int32 PreviewIndex = INDEX_NONE;
	bool bFocus = false;
	UHexCardWidget* Dragged = bDragging ? PressedCard.Get() : nullptr;
	if (Dragged)
	{
		PreviewIndex = GetHandIndexOfWidget(Dragged);
		bFocus = true;
	}
	else if (Cards->GetSelectedCardIndex() != INDEX_NONE)
	{
		PreviewIndex = Cards->GetSelectedCardIndex();
		bFocus = true;
	}
	else if (HoveredCard.IsValid())
	{
		PreviewIndex = GetHandIndexOfWidget(HoveredCard.Get());
	}

	AHexTile* HoverTile = nullptr;
	const bool bOverDiscard = Dragged && IsOverDiscard(GetMouseInHandSpace());
	if (bFocus && !bOverDiscard && (Dragged || !HoveredCard.IsValid()))
	{
		HoverTile = GetTileUnderMouse();
	}

	if (PreviewIndex != INDEX_NONE)
	{
		Cards->UpdateGuide(PreviewIndex, bFocus, HoverTile);
	}
	else if (Cards->IsGuideActive())
	{
		Cards->ClearGuide();
	}

	// Green / red frame on the dragged card.
	for (const FCardEntry& Entry : Entries)
	{
		if (UHexCardWidget* Widget = Entry.Widget.Get())
		{
			EHexCardDropHint Hint = EHexCardDropHint::None;
			if (Widget == Dragged && bOverDiscard)
			{
				FText Reason;
				Hint = Cards->CanDiscard(Reason) ? EHexCardDropHint::Valid : EHexCardDropHint::Invalid;
			}
			else if (Widget == Dragged && HoverTile && Cards->IsGuideActive())
			{
				Hint = Cards->IsGuideTarget(HoverTile) ? EHexCardDropHint::Valid : EHexCardDropHint::Invalid;
			}
			Widget->SetDropHint(Hint);
		}
	}
}

void UHexHandWidget::Reconcile(bool bDealAll)
{
	bPendingReconcile = false;
	bPendingDealAll = bDealAll;
	const UHexCardSubsystem* Cards = GetCards();
	if (!Cards || !HandCanvas)
	{
		return;
	}
	const TArray<FHexCard> Hand = Cards->GetHand();

	// Match hand cards to live entries one to one (a hand may hold the same face several times).
	const TArray<int32> Matched = HexHandMatch::MatchHand(Entries, Hand, [](const FCardEntry& Entry)
	{
		return Entry.Mode == EEntryMode::InHand || Entry.Mode == EEntryMode::Drawing;
	});

	// Cards that left the hand without being played (undo, mulligan, restart) go back to the deck.
	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		FCardEntry& Entry = Entries[EntryIndex];
		if ((Entry.Mode == EEntryMode::InHand || Entry.Mode == EEntryMode::Drawing) && !Matched.Contains(EntryIndex))
		{
			Entry.Mode = EEntryMode::ReturnToDeck;
			Entry.ModeTime = 0.f;
		}
	}

	// New cards are dealt from the deck pile.
	int32 NewCount = 0;
	const FVector2D Deck = GetDeckPosition();
	for (int32 HandIndex = 0; HandIndex < Hand.Num(); ++HandIndex)
	{
		const FHexCard& Card = Hand[HandIndex];
		const int32 Existing = Matched[HandIndex];
		if (Existing != INDEX_NONE)
		{
			FCardEntry& Entry = Entries[Existing];
			if (Entry.Card.bLucky != Card.bLucky)
			{
				Entry.Card = Card;
				if (UHexCardWidget* Widget = Entry.Widget.Get())
				{
					Widget->SetCard(Card);
				}
			}
			continue;
		}
		if (NewCount >= (Hand.Num()>30 ? 1 : 3))
		{
			bPendingReconcile = true;
			ReconcileDelay = Hand.Num()>30 ? .12f : .22f;
			break;
		}
		UHexCardWidget* Widget = CreateCardWidget(Card);
		if (!Widget)
		{
			continue;
		}
		// The card that was showing face up on the pile slides into the hand as it is (no flip, no wait).
		const bool bFromPeek = !bDealAll && bShownPeekValid && Card.SameFace(ShownPeek);
		Widget->SetFaceDown(!bFromPeek);
		FCardEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Widget = Widget;
		Entry.Card = Card;
		Entry.Mode = EEntryMode::Drawing;
		Entry.Pos.Value = Deck;
		Entry.Scale.Value = DeckScale;
		// Bounded batches: large hands arrive more slowly instead of compressing into one burst.
		const float Stagger = FMath::Clamp(DealStagger + FMath::Max(0, Hand.Num()-15)*0.0015f, 0.065f, 0.14f);
		Entry.Delay = bFromPeek ? 0.f : PendingDealDelay + Stagger * static_cast<float>(NewCount);
		Entry.FlipTime = bFromPeek ? DrawFlipDuration : 0.f;
		Entry.Opacity = bFromPeek ? 1.f : 0.f;
		if (bFromPeek)
		{
			bShownPeekValid = false;	// the pile now reveals the next one
		}
		++NewCount;
		ApplyEntryToWidget(Entry, HexHand::ZDeck + 1, FVector2D::ZeroVector);
	}
	PendingDealDelay = 0.f;
	UpdateVisualStates();
}

int32 UHexHandWidget::FindEntryByFace(const FHexCard& Card, bool bOnlyLive) const
{
	return Entries.IndexOfByPredicate([&Card, bOnlyLive](const FCardEntry& Entry)
	{
		return !Entry.bDead && Entry.Card.SameFace(Card)
			&& (!bOnlyLive || Entry.Mode == EEntryMode::InHand || Entry.Mode == EEntryMode::Drawing);
	});
}

int32 UHexHandWidget::FindEntryByWidget(const UHexCardWidget* Widget) const
{
	if (!Widget)
	{
		return INDEX_NONE;
	}
	return Entries.IndexOfByPredicate([Widget](const FCardEntry& Entry) { return !Entry.bDead && Entry.Widget.Get() == Widget; });
}

TArray<int32> UHexHandWidget::GetLiveEntriesInHandOrder() const
{
	TArray<int32> Result;
	if (const UHexCardSubsystem* Cards = GetCards())
	{
		const TArray<int32> Matched = HexHandMatch::MatchHand(Entries, Cards->GetHand(), [](const FCardEntry& Entry)
		{
			return Entry.Mode == EEntryMode::InHand || Entry.Mode == EEntryMode::Drawing;
		});
		for (const int32 Index : Matched)
		{
			if (Index != INDEX_NONE)
			{
				Result.Add(Index);
			}
		}
	}
	return Result;
}

// ---------------------------------------------------------------- input from card widgets

void UHexHandWidget::HandleCardHovered(UHexCardWidget* Card, bool bHovered)
{
	// Hover is resolved from the resting layout in NativeTick (no flicker while cards lift).
}

void UHexHandWidget::HandleCardPressed(UHexCardWidget* Card)
{
	// Prefer the card the player sees as hovered (cards overlap in the fan).
	UHexCardWidget* Target = HoveredCard.IsValid() ? HoveredCard.Get() : Card;
	const int32 Index = FindEntryByWidget(Target);
	if (Index == INDEX_NONE || Entries[Index].Mode != EEntryMode::InHand)
	{
		return;
	}
	if (!Entries[Index].bPlayable)
	{
		// Dead card (no neighbouring tile accepts it): it can't be selected or played. It can only be picked up
		// to throw it on the discard zone, and only while discards are left.
		const UHexCardSubsystem* Cards = GetCards();
		const bool bCanDiscard = Cards && Cards->GetMaxDiscardCharges() > 0 && Cards->GetDiscardCharges() > 0;
		FString Hint = HEXT("这张牌暂时没有可用的目标。");
		if (bCanDiscard)
		{
			Hint += HEXT("可以拖到左下角换一张。");
		}
		UHexAudioSubsystem::PlayExact(this, TEXT("Error_Buzz_01"));
        ShowGuideMessage(FText::FromString(Hint));
		if (!bCanDiscard)
		{
			return;
		}
	}
	PressedCard = Target;
	PressMouse = GetMouseInHandSpace();
	DragOffset = Entries[Index].Pos.Value - PressMouse;
	bDragging = false;
}

void UHexHandWidget::HandleCardRightClicked(UHexCardWidget* Card)
{
	ClearSelection();
}

void UHexHandWidget::SelectHandIndex(int32 HandIndex)
{
	if (UHexCardSubsystem* Cards = GetCards())
	{
		if (Cards->GetHand().IsValidIndex(HandIndex))
		{
			Cards->SelectCard(HandIndex);
		}
	}
}

void UHexHandWidget::ClearSelection()
{
	if (UHexCardSubsystem* Cards = GetCards())
	{
		if (Cards->GetSelectedCardIndex() != INDEX_NONE)
		{
			Cards->SelectCard(INDEX_NONE);
		}
	}
}

// ---------------------------------------------------------------- tick

void UHexHandWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UHexCardSubsystem* Cards = GetCards();
	if (Cards != BoundCards.Get())
	{
		BindToCards();
	}
	// The hand steps aside for the results card.
	const AHexSokobanGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AHexSokobanGameMode>() : nullptr;
	const bool bActive = Cards && Cards->IsActive() && !(GameMode && GameMode->GetLevelState() == EHexLevelState::Completed);
	if (HandCanvas)
	{
		HandCanvas->SetVisibility(bActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!bActive || !HandCanvas)
	{
		return;
	}

	const float DeltaTime = FMath::Min(InDeltaTime, 0.05f);
	ReconcileDelay -= DeltaTime;
	if (bPendingReconcile && ReconcileDelay <= 0.f) Reconcile(bPendingDealAll);
	const FVector2D Mouse = GetMouseInHandSpace();
	UpdatePress(Mouse);
	UpdateHover(Mouse);
	UpdateEntries(DeltaTime);
	UpdateGuide();
	UpdateGuideMessage(DeltaTime);
	UpdateDiscardZone(DeltaTime, Mouse);

	StateRefreshTimer -= DeltaTime;
	if (StateRefreshTimer <= 0.f)
	{
		StateRefreshTimer = 0.1f;
		UpdateVisualStates();
	}

	UpdateDeckPile(DeltaTime);
}

void UHexHandWidget::UpdatePress(const FVector2D& Mouse)
{
	UHexCardWidget* Pressed = PressedCard.Get();
	if (!Pressed)
	{
		PressedCard.Reset();
		bDragging = false;
		return;
	}
	const int32 EntryIndex = FindEntryByWidget(Pressed);
	if (EntryIndex == INDEX_NONE || Entries[EntryIndex].Mode != EEntryMode::InHand)
	{
		PressedCard.Reset();
		bDragging = false;
		return;
	}

	UHexCardSubsystem* Cards = GetCards();
	const int32 HandIndex = Cards ? Cards->GetHand().IndexOfByPredicate([&](const FHexCard& Card) { return Card.SameFace(Entries[EntryIndex].Card); }) : INDEX_NONE;

	const bool bDown = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);
	const bool bDeadCard = !Entries[EntryIndex].bPlayable;
	if (bDown)
	{
		if (bDeadCard)
		{
			// A dead card only travels to the discard zone: never selected, never lights tiles.
			bDragging = bDragging || FVector2D::Distance(Mouse, PressMouse) > DragThreshold;
			return;
		}
		if (!bDragging && FVector2D::Distance(Mouse, PressMouse) > DragThreshold)
		{
			bDragging = true;
			if (Cards && HandIndex != INDEX_NONE && Cards->GetSelectedCardIndex() != HandIndex)
			{
				Cards->SelectCard(HandIndex);	// glow the tiles this card can pay for
			}
			if (Cards && HandIndex != INDEX_NONE && !Cards->IsCardPlayableNow(HandIndex))
			{
				FString Hint = HEXT("这张牌暂时没有可用的目标。");
				if (Cards->GetMaxDiscardCharges() > 0)
				{
					Hint += HEXT("可以拖到左下角换一张。");
				}
				UHexAudioSubsystem::PlayExact(this, TEXT("Error_Buzz_01"));
        ShowGuideMessage(FText::FromString(Hint));
			}
		}
		return;
	}

	// Released.
	if (bDeadCard)
	{
		if (bDragging && IsOverDiscard(Mouse))
		{
			TryDiscardEntry(EntryIndex);
		}
		PressedCard.Reset();
		bDragging = false;
		return;
	}
	if (bDragging)
	{
		const UHexCardWidget* CardWidget = Entries[EntryIndex].Widget.Get();
		const float CardHeight = CardWidget ? static_cast<float>(CardWidget->GetCardSize().Y) : 176.f;
		const bool bAboveHand = Mouse.Y < HandBaseY - CardHeight * 0.6f;
		// A lit (reachable) tile under the cursor always wins, even when it sits right behind the hand;
		// otherwise only drops clearly above the hand count as "played onto the board".
		const AHexTile* DropTile = GetTileUnderMouse();
		const bool bOnTarget = DropTile && Cards && Cards->IsGuideTarget(DropTile);
		const bool bPlayed = IsOverDiscard(Mouse)
			? TryDiscardEntry(EntryIndex)
			: (bOnTarget || bAboveHand) && TryPlayEntryAtMouse(EntryIndex);
		if (!bPlayed)
		{
			ClearSelection();
		}
	}
	else if (HandIndex != INDEX_NONE)
	{
		SelectHandIndex(HandIndex);	// click toggles selection
	}
	PressedCard.Reset();
	bDragging = false;
}

void UHexHandWidget::UpdateHover(const FVector2D& Mouse)
{
	if (bDragging)
	{
		HoveredCard.Reset();
		return;
	}
	const TArray<int32> Live = GetLiveEntriesInHandOrder();
	const int32 Count = Live.Num();
	int32 Best = INDEX_NONE;
	double BestDistance = TNumericLimits<double>::Max();
	for (int32 SlotIndex = 0; SlotIndex < Count; ++SlotIndex)
	{
		const FCardEntry& Entry = Entries[Live[SlotIndex]];
		const UHexCardWidget* Widget = Entry.Widget.Get();
		if (!Widget || Entry.Mode != EEntryMode::InHand || !Entry.bPlayable)
		{
			continue;	// dead cards don't lift under the cursor: they can't be chosen
		}
		FVector2D RestPos;
		float RestAngle = 0.f;
		GetRestLayout(SlotIndex, Count, RestPos, RestAngle);
		const FVector2D Half = Widget->GetCardSize() * 0.5 * static_cast<double>(GetHandScale(Count));
		const double Extra = (HoveredCard.Get() == Widget) ? HoverLift + Half.Y * (HoverScale - 1.0) : 0.0;
		const double DX = FMath::Abs(Mouse.X - RestPos.X);
		if (DX <= Half.X && Mouse.Y >= RestPos.Y - Half.Y - Extra && Mouse.Y <= 0.0 && DX < BestDistance)
		{
			BestDistance = DX;
			Best = Live[SlotIndex];
		}
	}
	if (Best == INDEX_NONE)
	{
		HoveredCard.Reset();
	}
	else
	{
		if (HoveredCard.Get() != Entries[Best].Widget) UHexAudioSubsystem::Play(this, TEXT("solid_card"), 0.18f);
		HoveredCard = Entries[Best].Widget;
	}
}

bool UHexHandWidget::GetInspectedCard(FHexCard& OutCard) const
{
	const UHexCardWidget* Candidate = PressedCard.IsValid() ? PressedCard.Get() : HoveredCard.Get();
	const int32 EntryIndex = Candidate ? FindEntryByWidget(Candidate) : INDEX_NONE;
	if (Entries.IsValidIndex(EntryIndex) && Entries[EntryIndex].Mode == EEntryMode::InHand)
	{
		OutCard = Entries[EntryIndex].Card;
		return true;
	}
	// Unplayable cards do not lift or become selectable, but still have readable information.
	const FVector2D Mouse = GetMouseInHandSpace();
	const TArray<int32> Live = GetLiveEntriesInHandOrder();
	int32 Best = INDEX_NONE;
	double BestDistance = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < Live.Num(); ++Index)
	{
		const FCardEntry& Entry = Entries[Live[Index]];
		const UHexCardWidget* CardWidget = Entry.Widget.Get();
		if (!CardWidget || Entry.Mode != EEntryMode::InHand || Entry.bPlayable) continue;
		FVector2D Rest;
		float Angle = 0.f;
		GetRestLayout(Index, Live.Num(), Rest, Angle);
		const FVector2D Half = CardWidget->GetCardSize() * .5 * GetHandScale(Live.Num()) * UnplayableScale;
		const double Distance = FMath::Abs(Mouse.X - Rest.X);
		if (Distance <= Half.X && Mouse.Y >= Rest.Y - Half.Y && Mouse.Y <= Rest.Y + Half.Y && Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Live[Index];
		}
	}
	if (Best != INDEX_NONE) { OutCard = Entries[Best].Card; return true; }
	if (const UHexCardSubsystem* Cards = GetCards())
	{
		const TArray<FHexCard> Hand = Cards->GetHand();
		const int32 Selected = Cards->GetSelectedCardIndex();
		if (Hand.IsValidIndex(Selected)) { OutCard = Hand[Selected]; return true; }
	}
	return false;
}

void UHexHandWidget::UpdateEntries(float DeltaTime)
{
	const UHexCardSubsystem* Cards = GetCards();
	const int32 Selected = Cards ? Cards->GetSelectedCardIndex() : INDEX_NONE;
	const TArray<int32> Live = GetLiveEntriesInHandOrder();
	const int32 Count = Live.Num();
	const float HandScale = GetHandScale(Count);
	const FVector2D Mouse = GetMouseInHandSpace();
	const FVector2D Deck = GetDeckPosition();

	int32 HoveredSlot = INDEX_NONE;
	for (int32 SlotIndex = 0; SlotIndex < Count; ++SlotIndex)
	{
		if (HoveredCard.IsValid() && Entries[Live[SlotIndex]].Widget.Get() == HoveredCard.Get())
		{
			HoveredSlot = SlotIndex;
		}
	}

	FVector2D Shake = FVector2D::ZeroVector;
	if (ShakeTime > 0.f)
	{
		ShakeTime = FMath::Max(0.f, ShakeTime - DeltaTime);
		const float Fade = ShakeDuration > 0.f ? ShakeTime / ShakeDuration : 0.f;
		Shake.X = FMath::Sin(ShakeTime * 70.f) * ShakeAmplitude * Fade;
	}

	// Live hand cards (and cards being dealt).
	for (int32 SlotIndex = 0; SlotIndex < Count; ++SlotIndex)
	{
		FCardEntry& Entry = Entries[Live[SlotIndex]];
		Entry.ModeTime += DeltaTime;

		FVector2D TargetPos;
		float TargetAngle = 0.f;
		GetRestLayout(SlotIndex, Count, TargetPos, TargetAngle);
		float TargetScale = HandScale;
		int32 ZOrder = HexHand::ZHandBase + SlotIndex;

		const bool bIsDragged = bDragging && PressedCard.IsValid() && Entry.Widget.Get() == PressedCard.Get();
		if (bIsDragged)
		{
			TargetPos = Mouse + DragOffset * 0.35;
			TargetAngle = FMath::Clamp(static_cast<float>(Entry.Pos.Velocity.X) * 0.012f, -14.f, 14.f);
			TargetScale = 0.9f;
			ZOrder = HexHand::ZDragged;
		}
		else if (SlotIndex == HoveredSlot)
		{
			TargetPos.Y -= HoverLift;
			TargetAngle = 0.f;
			TargetScale = HoverScale;
			ZOrder = HexHand::ZHovered;
		}
		else
		{
			if (HoveredSlot != INDEX_NONE)
			{
				// Push the neighbours apart so the hovered card shows. The push decays by less than one slot per
				// card, so cards never cross each other (which made crowded hands clump).
				const int32 Away = SlotIndex - HoveredSlot;
				FVector2D SlotA, SlotB;
				float IgnoredAngle = 0.f;
				GetRestLayout(0, Count, SlotA, IgnoredAngle);
				GetRestLayout(1, Count, SlotB, IgnoredAngle);
				const float Step = FMath::Max(1.f, static_cast<float>(SlotB.X - SlotA.X));
				const float Push = FMath::Max(0.f, HoverSpread - static_cast<float>(FMath::Abs(Away) - 1) * Step * 0.6f);
				TargetPos.X += FMath::Sign(static_cast<float>(Away)) * Push;
			}
			if (Cards && Cards->GetHand().IsValidIndex(Selected) && Entry.Card.SameFace(Cards->GetHand()[Selected]))
			{
				TargetPos.Y -= SelectLift;
				TargetScale = 1.08f * HandScale;
			}
			else if (Entry.bPlayable)
			{
				TargetPos.Y -= PlayableLift;
			}
			else
			{
				TargetScale = UnplayableScale * HandScale;
			}
		}

		if (Entry.Mode == EEntryMode::Drawing)
		{
			if (Entry.Delay > 0.f)
			{
				Entry.Delay -= DeltaTime;
				Entry.Pos.Value = Deck;
				Entry.Opacity = 0.f;
				ApplyEntryToWidget(Entry, HexHand::ZDeck + 1, FVector2D::ZeroVector);
				continue;
			}
			Entry.Opacity = 1.f;
			Entry.FlipTime += DeltaTime;
			const float FlipAlpha = DrawFlipDuration > 0.f ? FMath::Clamp(Entry.FlipTime / DrawFlipDuration, 0.f, 1.f) : 1.f;
			Entry.FlipScaleX = FMath::Abs(FMath::Cos(PI * FlipAlpha));
			if (UHexCardWidget* Widget = Entry.Widget.Get())
			{
				Widget->SetFaceDown(FlipAlpha < 0.5f);
			}
			if (FlipAlpha >= 1.f)
			{
				Entry.FlipScaleX = 1.f;
				Entry.Mode = EEntryMode::InHand;
				Entry.ModeTime = 0.f;
			}
		}

		StepSpring(Entry.Pos, TargetPos, DeltaTime);
		StepSpring(Entry.Angle, TargetAngle, DeltaTime);
		StepSpring(Entry.Scale, TargetScale, DeltaTime);
		ApplyEntryToWidget(Entry, ZOrder, bIsDragged ? FVector2D::ZeroVector : Shake);
	}

	// Leaving cards.
	for (FCardEntry& Entry : Entries)
	{
		if (Entry.bDead)
		{
			continue;
		}
		if (Entry.Mode == EEntryMode::FlyToTile)
		{
			Entry.FlyTime += DeltaTime;
			const float T = PlayFlyDuration > 0.f ? FMath::Clamp(Entry.FlyTime / PlayFlyDuration, 0.f, 1.f) : 1.f;
			const float E = HexHand::EaseOutCubic(T);
			Entry.Pos.Value = FMath::Lerp(Entry.FlyFrom, Entry.FlyTo, static_cast<double>(E)) + FVector2D(0.0, -70.0 * FMath::Sin(PI * T));
			Entry.Angle.Value = FMath::Lerp(Entry.FlyAngleFrom, 0.f, E);
			Entry.Scale.Value = FMath::Lerp(1.f, 0.35f, E);
			Entry.Opacity = T < 0.7f ? 1.f : FMath::Lerp(1.f, 0.f, (T - 0.7f) / 0.3f);
			ApplyEntryToWidget(Entry, HexHand::ZFlying, FVector2D::ZeroVector);
			Entry.bDead = T >= 1.f;
		}
		else if (Entry.Mode == EEntryMode::ReturnToDeck)
		{
			Entry.ModeTime += DeltaTime;
			if (UHexCardWidget* Widget = Entry.Widget.Get())
			{
				Widget->SetFaceDown(true);
			}
			Entry.FlipScaleX = 1.f;
			StepSpring(Entry.Pos, Deck, DeltaTime);
			StepSpring(Entry.Angle, 0.f, DeltaTime);
			StepSpring(Entry.Scale, 0.85f, DeltaTime);
			const float T = FMath::Clamp(Entry.ModeTime / HexHand::ReturnDuration, 0.f, 1.f);
			Entry.Opacity = 1.f - T * T;
			ApplyEntryToWidget(Entry, HexHand::ZDeck + 1, FVector2D::ZeroVector);
			Entry.bDead = T >= 1.f;
		}
	}

	// Remove finished cards.
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		if (!Entries[Index].bDead && Entries[Index].Widget.IsValid())
		{
			continue;
		}
		if (UHexCardWidget* Widget = Entries[Index].Widget.Get())
		{
			if (HoveredCard.Get() == Widget)
			{
				HoveredCard.Reset();
			}
			if (PressedCard.Get() == Widget)
			{
				PressedCard.Reset();
				bDragging = false;
			}
			Widget->RemoveFromParent();
			CardWidgets.Remove(Widget);
		}
		Entries.RemoveAt(Index);
	}
}

void UHexHandWidget::UpdateVisualStates()
{
	const UHexCardSubsystem* Cards = GetCards();
	if (!Cards)
	{
		return;
	}
	const TArray<FHexCard> Hand = Cards->GetHand();
	const int32 Selected = Cards->GetSelectedCardIndex();
	const TArray<int32> Matched = HexHandMatch::MatchHand(Entries, Hand, [](const FCardEntry& Entry)
	{
		return Entry.Mode == EEntryMode::InHand || Entry.Mode == EEntryMode::Drawing;
	});
	for (int32 HandIndex = 0; HandIndex < Hand.Num(); ++HandIndex)
	{
		const int32 EntryIndex = Matched[HandIndex];
		if (EntryIndex == INDEX_NONE)
		{
			continue;
		}
		UHexCardWidget* Widget = Entries[EntryIndex].Widget.Get();
		if (!Widget)
		{
			continue;
		}
		// Can this card move the player right now? (stays "playable" while walking so the hand doesn't flicker)
		const bool bPlayable = Cards->IsCardPlayableNow(HandIndex) || (Entries[EntryIndex].bPlayable && IsPlayerBusy());
		Entries[EntryIndex].bPlayable = bPlayable;
		Widget->SetPlayableHint(bPlayable);

		EHexCardVisualState NewState = EHexCardVisualState::Normal;
		if (HandIndex == Selected)
		{
			NewState = EHexCardVisualState::Selected;
		}
		else if (HoveredCard.Get() == Widget)
		{
			NewState = EHexCardVisualState::Hovered;
		}
		else if (!bPlayable)
		{
			NewState = EHexCardVisualState::Unplayable;
		}
		Widget->SetVisualState(NewState);
	}
}

void UHexHandWidget::ApplyEntryToWidget(FCardEntry& Entry, int32 ZOrder, const FVector2D& Shake)
{
	UHexCardWidget* Widget = Entry.Widget.Get();
	if (!Widget)
	{
		return;
	}
	if (UCanvasPanelSlot* CardSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		CardSlot->SetPosition(Entry.Pos.Value + Shake);
		CardSlot->SetZOrder(ZOrder);
	}
	Widget->SetRenderTransformAngle(Entry.Angle.Value);
	const float Scale = FMath::Max(0.f, Entry.Scale.Value);
	Widget->SetRenderScale(FVector2D(Scale * Entry.FlipScaleX, Scale));
	Widget->SetRenderOpacity(Entry.Opacity);
}

// ---------------------------------------------------------------- playing by drag

AHexTile* UHexHandWidget::GetTileUnderMouse() const
{
	APlayerController* PC = GetOwningPlayer();
	const UWorld* World = GetWorld();
	UHexBoardSubsystem* Board = World ? World->GetSubsystem<UHexBoardSubsystem>() : nullptr;
	const UHexGridMovementComponent* Mover = Board ? Board->GetPlayerMover() : nullptr;
	if (!PC || !Board || !Mover || !Mover->IsOnBoard())
	{
		return nullptr;
	}
	const AHexTile* Current = Board->GetTileAt(Mover->GetCoord());
	const double PlaneZ = Current ? Current->GetSurfaceLocation().Z : 0.0;

	float MouseX = 0.f, MouseY = 0.f;
	FVector Origin, Direction;
	if (!PC->GetMousePosition(MouseX, MouseY) || !PC->DeprojectScreenPositionToWorld(MouseX, MouseY, Origin, Direction)
		|| FMath::IsNearlyZero(Direction.Z))
	{
		return nullptr;
	}
	const double T = (PlaneZ - Origin.Z) / Direction.Z;
	if (T <= 0.0)
	{
		return nullptr;
	}
	const FVector Hit = Origin + Direction * T;
	AHexTile* Tile = Board->FindNearestTile(Hit);
	if (!Tile || FVector::Dist2D(Tile->GetActorLocation(), Hit) > UHexGridLibrary::GetTileSpacing() * 0.6f)
	{
		return nullptr;
	}
	return Tile;
}

bool UHexHandWidget::TryPlayEntryAtMouse(int32 EntryIndex)
{
	UHexCardSubsystem* Cards = GetCards();
	const UWorld* World = GetWorld();
	UHexBoardSubsystem* Board = World ? World->GetSubsystem<UHexBoardSubsystem>() : nullptr;
	UHexGridMovementComponent* Mover = Board ? Board->GetPlayerMover() : nullptr;
	AHexTile* Tile = GetTileUnderMouse();
	if (!Cards || !Mover || !Tile || !Entries.IsValidIndex(EntryIndex))
	{
		return false;
	}

	const FIntPoint From = Mover->GetCoord();
	EHexDirection Direction = EHexDirection::Invalid;
	for (uint8 Dir = 0; Dir < 6; ++Dir)
	{
		if (UHexGridLibrary::GetNeighbor(From, static_cast<EHexDirection>(Dir)) == Tile->GetCoord())
		{
			Direction = static_cast<EHexDirection>(Dir);
			break;
		}
	}
	if (Tile->GetCoord() == From)
	{
		return false;	// dropped back on the player's own tile: just return the card
	}
	if (Direction == EHexDirection::Invalid)
	{
		if (Entries[EntryIndex].Card.Kind != EHexCardKind::Skill)
		{
			Cards->ReportNotNeighbor(Tile);
			return false;
		}
	}

	const FHexCard Card = Entries[EntryIndex].Card;
	const int32 HandIndex = Cards->GetHand().IndexOfByPredicate([&Card](const FHexCard& Other) { return Other.SameFace(Card); });
	if (HandIndex == INDEX_NONE)
	{
		return false;
	}
	// Refusals (wrong card, box can't move) are reported by the movement / card system with a reason.
	if (Cards->GetSelectedCardIndex() != HandIndex)
	{
		Cards->SelectCard(HandIndex);
	}
	const EHexMoveResult Result = Card.Kind == EHexCardKind::Skill ? Mover->TryPlaySkillAt(Tile->GetCoord()) : Mover->TryMove(Direction);
	return Result == EHexMoveResult::Moved || Result == EHexMoveResult::Pushed || Result == EHexMoveResult::Busy;
}

// ---------------------------------------------------------------- deck pile

void UHexHandWidget::BuildDeckPile()
{
	if (!HandCanvas || !WidgetTree || !DeckWidget)
	{
		return;
	}
	const FVector2D CardSize = DeckWidget->GetCardSize() * DeckScale;

	// Dark tray: the pile sits in a slot of its own, unlike the floating hand.
	DeckTray = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DeckTray"));
	DeckTray->SetBrush(FHexUIStyle::Panel(FHexUIStyle::PanelDark, 16.f, FLinearColor(1.f, 1.f, 1.f, 0.22f), 2.f));
	DeckTray->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* TraySlot = HandCanvas->AddChildToCanvas(DeckTray);
	TraySlot->SetAnchors(FAnchors(0.5f, 1.f));
	TraySlot->SetAlignment(FVector2D(0.5, 0.5));
	TraySlot->SetAutoSize(false);
	TraySlot->SetSize(CardSize + FVector2D(30.0, 30.0));
	TraySlot->SetZOrder(HexHand::ZDeckTray);

	// Card backs under the top (thickness).
	for (int32 Layer = 0; Layer < 4; ++Layer)
	{
		FHexCard BackCard;
		UHexCardWidget* Back = CreateCardWidget(BackCard);
		if (!Back)
		{
			break;
		}
		CardWidgets.Remove(Back);
		Back->SetFaceDown(true);
		Back->SetVisibility(ESlateVisibility::HitTestInvisible);
		Back->SetRenderScale(FVector2D(DeckScale, DeckScale));
		if (UCanvasPanelSlot* BackSlot = Cast<UCanvasPanelSlot>(Back->Slot))
		{
			BackSlot->SetZOrder(HexHand::ZDeckLayer);
		}
		DeckLayers.Add(Back);
	}

	// "Next" tag on the face-up top card.
	NextTag = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DeckNextTag"));
	NextTag->SetBrush(FHexUIStyle::Panel(FHexUIStyle::Accent, 9.f, FLinearColor(0.75f, 0.45f, 0.f, 1.f), 1.5f));
	NextTag->SetPadding(FMargin(9.f, 1.f, 9.f, 2.f));
	NextTag->SetVisibility(ESlateVisibility::HitTestInvisible);
	NextTag->SetRenderTransformPivot(FVector2D(0.5, 0.5));
	UTextBlock* TagText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DeckNextTagText"));
	TagText->SetFont(FHexUIStyle::Font(13, true));
	TagText->SetColorAndOpacity(FSlateColor(FHexUIStyle::InkDark));
	TagText->SetText(FText::FromString(HEXT("下一张")));
	NextTag->SetContent(TagText);
	UCanvasPanelSlot* TagSlot = HandCanvas->AddChildToCanvas(NextTag);
	TagSlot->SetAnchors(FAnchors(0.5f, 1.f));
	TagSlot->SetAlignment(FVector2D(0.0, 0.5));
	TagSlot->SetAutoSize(true);
	TagSlot->SetZOrder(HexHand::ZDeck + 2);

	// The old big pile count becomes a quiet "N left" line under the tray.
	if (DeckCountText)
	{
		DeckCountText->SetFont(FHexUIStyle::Font(14, false));
		DeckCountText->SetColorAndOpacity(FSlateColor(FHexUIStyle::TextLight.CopyWithNewOpacity(0.85f)));
		DeckCountText->SetShadowOffset(FVector2D(1.0, 1.0));
	}
	DeckWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	DeckWidget->SetPlayableHint(false);
	bShownPeekValid = false;
	PeekFlipTime = 10.f;
}

void UHexHandWidget::UpdateDeckPile(float DeltaTime)
{
	const UHexCardSubsystem* Cards = GetCards();
	if (!Cards || !DeckWidget)
	{
		return;
	}
	if (!Cards->IsActive())
	{
		// Designed hands have no pile: the whole deck area stays hidden.
		const ESlateVisibility Hidden = ESlateVisibility::Collapsed;
		DeckWidget->SetVisibility(Hidden);
		for (UHexCardWidget* Back : DeckLayers)
		{
			if (Back)
			{
				Back->SetVisibility(Hidden);
			}
		}
		for (UWidget* Part : { static_cast<UWidget*>(DeckTray.Get()), static_cast<UWidget*>(NextTag.Get()), static_cast<UWidget*>(DeckCountText.Get()),
			WidgetTree ? WidgetTree->FindWidget(TEXT("DeckLabel")) : nullptr })
		{
			if (Part)
			{
				Part->SetVisibility(Hidden);
			}
		}
		bShownPeekValid = false;
		return;
	}
	if (DeckTray)
	{
		DeckTray->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (DeckCountText)
	{
		DeckCountText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (UWidget* PileLabel = WidgetTree ? WidgetTree->FindWidget(TEXT("DeckLabel")) : nullptr)
	{
		PileLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	const FVector2D Deck = GetDeckPosition();
	const FVector2D CardSize = DeckWidget->GetCardSize() * DeckScale;
	const int32 PileCount = Cards->GetSkillPileCount();

	// A new top card turns over (drawn card left, or the pile was reshaped by an assist / undo).
	FHexCard Top;
	const bool bHasTop = Cards->PeekNextSkill(Top);
	if (bHasTop != bShownPeekValid || (bHasTop && (!Top.SameFace(ShownPeek) || Top.bLucky != ShownPeek.bLucky)))
	{
		ShownPeek = Top;
		bShownPeekValid = bHasTop;
		PeekFlipTime = 0.f;
		if (bHasTop)
		{
			DeckWidget->SetCard(Top);
		}
	}
	PeekFlipTime += DeltaTime;
	const float FlipAlpha = DrawFlipDuration > 0.f ? FMath::Clamp(PeekFlipTime / DrawFlipDuration, 0.f, 1.f) : 1.f;
	const float FlipScaleX = FMath::Abs(FMath::Cos(PI * FlipAlpha));

	if (UCanvasPanelSlot* TraySlot = DeckTray ? Cast<UCanvasPanelSlot>(DeckTray->Slot) : nullptr)
	{
		TraySlot->SetPosition(Deck + FVector2D(5.0, 6.0));
		TraySlot->SetSize(CardSize + FVector2D(34.0, 36.0));
	}

	// Thickness: one back per CardsPerDeckLayer cards beyond the top, offset down-right.
	const int32 Layers = PileCount > 1 ? FMath::Clamp(FMath::DivideAndRoundUp(PileCount - 1, FMath::Max(1, CardsPerDeckLayer)), 1, DeckLayers.Num()) : 0;
	for (int32 Layer = 0; Layer < DeckLayers.Num(); ++Layer)
	{
		UHexCardWidget* Back = DeckLayers[Layer];
		if (!Back)
		{
			continue;
		}
		const bool bShown = Layer < Layers;
		Back->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* BackSlot = Cast<UCanvasPanelSlot>(Back->Slot))
		{
			const double Step = 3.5 * static_cast<double>(Layers - Layer);	// deepest layer furthest out
			BackSlot->SetPosition(Deck + FVector2D(Step, Step));
		}
		Back->SetRenderOpacity(1.f - 0.12f * static_cast<float>(Layers - Layer));
	}

	// Face-up top card (flips in when it changes).
	DeckWidget->SetVisibility(bHasTop ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	DeckWidget->SetFaceDown(FlipAlpha < 0.5f);
	DeckWidget->SetRenderScale(FVector2D(DeckScale * FlipScaleX, DeckScale));
	DeckWidget->SetRenderTransformAngle(-2.f);
	if (UCanvasPanelSlot* DeckSlot = Cast<UCanvasPanelSlot>(DeckWidget->Slot))
	{
		DeckSlot->SetPosition(Deck);
		DeckSlot->SetZOrder(HexHand::ZDeck);
	}

	if (NextTag)
	{
		NextTag->SetVisibility(bHasTop && FlipAlpha >= 0.5f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		const float Pop = 1.f + 0.25f * FMath::Exp(-8.f * FMath::Max(0.f, PeekFlipTime - DrawFlipDuration * 0.5f))
			* (PeekFlipTime > DrawFlipDuration * 0.5f ? 1.f : 0.f);
		NextTag->SetRenderScale(FVector2D(Pop, Pop));
		NextTag->SetRenderTransformAngle(-8.f);
		if (UCanvasPanelSlot* TagSlot = Cast<UCanvasPanelSlot>(NextTag->Slot))
		{
			TagSlot->SetPosition(Deck + FVector2D(-CardSize.X * 0.5 - 12.0, -CardSize.Y * 0.5 + 2.0));
		}
	}

	if (DeckCountText)
	{
		const UHexBoardSubsystem* DeckBoard=GetWorld()->GetSubsystem<UHexBoardSubsystem>();
		DeckCountText->SetText(PileCount > 0
			? FText::FromString(FHexLocalization::Printf(DeckBoard && DeckBoard->IsPerformanceMode() ? HEXT("牌堆剩余 %d 张") : HEXT("本轮剩余 %d 张"), PileCount))
			: FText::FromString(HEXT("牌堆空了")));
		if (UCanvasPanelSlot* TextSlot = Cast<UCanvasPanelSlot>(DeckCountText->Slot))
		{
			TextSlot->SetPosition(DeckOffset + FVector2D(5.0, CardSize.Y * 0.5 + 36.0));
		}
	}
	if (UWidget* Label = WidgetTree ? WidgetTree->FindWidget(TEXT("DeckLabel")) : nullptr)
	{
		if (UTextBlock* LabelText=Cast<UTextBlock>(Label)) LabelText->SetText(FText::FromString(HEXT("技能牌堆")));
		if (UCanvasPanelSlot* LabelSlot = Cast<UCanvasPanelSlot>(Label->Slot))
		{
			LabelSlot->SetPosition(DeckOffset + FVector2D(5.0, -CardSize.Y * 0.5 - 18.0));
		}
	}
}

// ---------------------------------------------------------------- discard zone

void UHexHandWidget::BuildDiscardZone()
{
	if (!HandCanvas || !WidgetTree)
	{
		return;
	}
	auto MakeText = [this](const TCHAR* Name, int32 Size, bool bBold, const FLinearColor& Color)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetFont(FHexUIStyle::Font(Size, bBold));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	};

	// Card-sized dark panel, same family as the rule panel (top left) and the toast.
	DiscardZone = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DiscardZone"));
	DiscardZone->SetPadding(FMargin(8.f, 12.f));
	DiscardZone->SetHorizontalAlignment(HAlign_Center);
	DiscardZone->SetVerticalAlignment(VAlign_Center);
	DiscardZone->SetVisibility(ESlateVisibility::HitTestInvisible);
	DiscardZone->SetRenderTransformPivot(FVector2D(0.5, 0.5));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DiscardColumn"));
	DiscardZone->SetContent(Column);
	DiscardTitle = MakeText(TEXT("DiscardTitle"), 30, true, FHexUIStyle::TextLight);
	DiscardTitle->SetText(FText::FromString(HEXT("弃牌")));
	Column->AddChildToVerticalBox(DiscardTitle)->SetHorizontalAlignment(HAlign_Center);
	DiscardSubText = MakeText(TEXT("DiscardSubText"), 13, false, FHexUIStyle::TextLight);
	Column->AddChildToVerticalBox(DiscardSubText)->SetHorizontalAlignment(HAlign_Center);
	DiscardPips = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DiscardPips"));
	UVerticalBoxSlot* PipsSlot = Column->AddChildToVerticalBox(DiscardPips);
	PipsSlot->SetHorizontalAlignment(HAlign_Center);
	PipsSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));

	UCanvasPanelSlot* ZoneSlot = HandCanvas->AddChildToCanvas(DiscardZone);
	ZoneSlot->SetAnchors(FAnchors(0.f, 1.f));
	ZoneSlot->SetAlignment(FVector2D(0.5, 0.5));
	ZoneSlot->SetAutoSize(false);
	ZoneSlot->SetSize(GetDiscardSize());
	ZoneSlot->SetPosition(DiscardOffset);
	ZoneSlot->SetZOrder(HexHand::ZDeck);

	// Caption above, like the deck pile's.
	DiscardLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DiscardLabel"));
	DiscardLabel->SetFont(FHexUIStyle::Font(15, true));
	DiscardLabel->SetColorAndOpacity(FSlateColor(FHexUIStyle::TextLight));
	DiscardLabel->SetShadowOffset(FVector2D(1.0, 1.0));
	DiscardLabel->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f));
	DiscardLabel->SetText(FText::FromString(HEXT("弃牌区")));
	DiscardLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* LabelSlot = HandCanvas->AddChildToCanvas(DiscardLabel);
	LabelSlot->SetAnchors(FAnchors(0.f, 1.f));
	LabelSlot->SetAlignment(FVector2D(0.5, 1.0));
	LabelSlot->SetAutoSize(true);
	LabelSlot->SetPosition(DiscardOffset + FVector2D(0.0, -96.0));
	LabelSlot->SetZOrder(HexHand::ZDeckText);

	DiscardVisual = -1;
	ShownDiscardCharges = -1;
	DiscardScale.Value = 1.f;
	RefreshDiscardPips();
}

FVector2D UHexHandWidget::GetDiscardSize() const
{
	return DeckWidget ? DeckWidget->GetCardSize() : FVector2D(126.0, 176.0);
}

FVector2D UHexHandWidget::GetDiscardPosition() const
{
	const FVector2D View = GetViewportSizeInSlate();
	return FVector2D(-View.X * 0.5 + DiscardOffset.X, DiscardOffset.Y);
}

bool UHexHandWidget::IsOverDiscard(const FVector2D& Mouse) const
{
	const UHexCardSubsystem* Cards = GetCards();
	if (!DiscardZone || !Cards || Cards->GetMaxDiscardCharges() <= 0)
	{
		return false;
	}
	const FVector2D Half = GetDiscardSize() * 0.5 + FVector2D(DiscardCatchMargin, DiscardCatchMargin);
	const FVector2D Delta = Mouse - GetDiscardPosition();
	return FMath::Abs(Delta.X) <= Half.X && FMath::Abs(Delta.Y) <= Half.Y;
}

bool UHexHandWidget::TryDiscardEntry(int32 EntryIndex)
{
	UHexCardSubsystem* Cards = GetCards();
	if (!Cards || !Entries.IsValidIndex(EntryIndex))
	{
		return false;
	}
	const FHexCard Card = Entries[EntryIndex].Card;
	const int32 HandIndex = Cards->GetHand().IndexOfByPredicate([&Card](const FHexCard& Other) { return Other.SameFace(Card); });
	return HandIndex != INDEX_NONE && Cards->DiscardCard(HandIndex);
}

void UHexHandWidget::RefreshDiscardPips()
{
	const UHexCardSubsystem* Cards = GetCards();
	if (!DiscardPips || !Cards)
	{
		return;
	}
	const int32 Max = Cards->GetMaxDiscardCharges();
	const int32 Charges = FMath::Clamp(Cards->GetDiscardCharges(), 0, Max);
	if (Max == ShownDiscardMax && Charges == ShownDiscardCharges)
	{
		return;
	}
	if (ShownDiscardCharges >= 0)
	{
		DiscardPulse = 0.35f;	// spent or refunded: pop the row
	}
	ShownDiscardMax = Max;
	ShownDiscardCharges = Charges;
	DiscardPips->ClearChildren();
	for (int32 Index = 0; Index < Max; ++Index)
	{
		// Same pip as the goal counter in the level card.
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(16.f);
		Box->SetHeightOverride(16.f);
		UImage* Pip = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		const bool bOn = Index < Charges;
		Pip->SetBrush(FHexUIStyle::Panel(bOn ? FHexUIStyle::Accent : FLinearColor(1.f, 1.f, 1.f, 0.16f), 5.f,
			bOn ? FLinearColor(0.75f, 0.45f, 0.f, 1.f) : FLinearColor(1.f, 1.f, 1.f, 0.38f), 1.5f));
		Box->SetContent(Pip);
		DiscardPips->AddChildToHorizontalBox(Box)->SetPadding(FMargin(Index > 0 ? 6.f : 0.f, 0.f, 0.f, 0.f));
	}
	DiscardVisual = -1;	// re-pick the look (empty <-> idle)
}

void UHexHandWidget::UpdateDiscardZone(float DeltaTime, const FVector2D& Mouse)
{
	UHexCardSubsystem* Cards = GetCards();
	if (!DiscardZone || !Cards)
	{
		return;
	}
	const bool bEnabled = Cards->GetMaxDiscardCharges() > 0;
	const ESlateVisibility Wanted = bEnabled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (DiscardZone->GetVisibility() != Wanted)
	{
		DiscardZone->SetVisibility(Wanted);
		if (DiscardLabel)
		{
			DiscardLabel->SetVisibility(Wanted);
		}
	}
	if (!bEnabled)
	{
		return;
	}
	RefreshDiscardPips();
	if (UCanvasPanelSlot* ZoneSlot = Cast<UCanvasPanelSlot>(DiscardZone->Slot))
	{
		ZoneSlot->SetSize(GetDiscardSize());
	}

	FText Reason;
	const bool bCan = Cards->CanDiscard(Reason);
	const bool bHasCharges = Cards->GetDiscardCharges() > 0;
	const bool bDraggingCard = bDragging && PressedCard.IsValid();
	const bool bOver = bDraggingCard && IsOverDiscard(Mouse);
	int32 Visual = bHasCharges ? HexHand::DiscardIdle : HexHand::DiscardEmpty;
	if (bOver)
	{
		Visual = bCan ? HexHand::DiscardHover : HexHand::DiscardHoverEmpty;
	}
	else if (bDraggingCard)
	{
		Visual = bHasCharges ? HexHand::DiscardArmed : HexHand::DiscardArmedEmpty;
	}

	if (Visual != DiscardVisual)
	{
		DiscardVisual = Visual;
		FLinearColor Fill = FHexUIStyle::PanelDark;
		FLinearColor Outline(1.f, 1.f, 1.f, 0.40f);
		float OutlineWidth = 2.f;
		FLinearColor TextColor = FHexUIStyle::TextLight;
		const TCHAR* Sub = HEXT("跳过本回合");	// skip this turn
		float Opacity = 1.f;
		switch (Visual)
		{
		case HexHand::DiscardEmpty:
			Outline = FLinearColor(1.f, 1.f, 1.f, 0.18f);
			Sub = HEXT("走一步恢复");	// move to refund
			Opacity = 0.6f;
			break;
		case HexHand::DiscardArmed:
			Outline = FHexUIStyle::Accent;
			OutlineWidth = 3.f;
			break;
		case HexHand::DiscardArmedEmpty:
			Outline = FLinearColor(FHexUIStyle::Bad.R, FHexUIStyle::Bad.G, FHexUIStyle::Bad.B, 0.8f);
			Sub = HEXT("次数用完");	// none left
			Opacity = 0.75f;
			break;
		case HexHand::DiscardHover:
			Fill = FHexUIStyle::Accent;
			Outline = FLinearColor::White;
			OutlineWidth = 3.f;
			TextColor = FHexUIStyle::InkDark;
			Sub = HEXT("松手弃牌");	// release to discard
			break;
		case HexHand::DiscardHoverEmpty:
			Fill = FHexUIStyle::Bad;
			Outline = FLinearColor(1.f, 1.f, 1.f, 0.6f);
			Sub = HEXT("次数用完");
			break;
		default:
			break;
		}
		DiscardZone->SetBrush(FHexUIStyle::Panel(Fill, 14.f, Outline, OutlineWidth));
		DiscardZone->SetRenderOpacity(Opacity);
		if (DiscardTitle)
		{
			DiscardTitle->SetColorAndOpacity(FSlateColor(TextColor));
		}
		if (DiscardSubText)
		{
			DiscardSubText->SetColorAndOpacity(FSlateColor(TextColor.CopyWithNewOpacity(0.8f)));
			DiscardSubText->SetText(FText::FromString(Sub));
		}
	}

	// Scale: grows under the dragged card, breathes while a card is in the air, pops on a charge change.
	DiscardClock += DeltaTime;
	float TargetScale = 1.f;
	if (Visual == HexHand::DiscardHover)
	{
		TargetScale = 1.12f;
	}
	else if (Visual == HexHand::DiscardHoverEmpty)
	{
		TargetScale = 1.04f;
	}
	else if (Visual == HexHand::DiscardArmed)
	{
		TargetScale = 1.f + 0.03f * FMath::Sin(DiscardClock * 6.f);
	}
	StepSpring(DiscardScale, TargetScale, DeltaTime);
	DiscardZone->SetRenderScale(FVector2D(DiscardScale.Value, DiscardScale.Value));

	if (DiscardPips)
	{
		DiscardPulse = FMath::Max(0.f, DiscardPulse - DeltaTime);
		const float Pop = 1.f + 0.35f * FMath::Sin(PI * (1.f - DiscardPulse / 0.35f)) * (DiscardPulse > 0.f ? 1.f : 0.f);
		DiscardPips->SetRenderTransformPivot(FVector2D(0.5, 0.5));
		DiscardPips->SetRenderScale(FVector2D(Pop, Pop));
	}
}

// ---------------------------------------------------------------- layout helpers

void UHexHandWidget::GetRestLayout(int32 SlotIndex, int32 Count, FVector2D& OutPos, float& OutAngle) const
{
	// Up to ~6 cards this is the classic fan; bigger hands widen (within the screen), shrink, and keep the
	// arc's tilt / drop capped so the outer cards never leave the screen.
	const float Half = 0.5f * static_cast<float>(FMath::Max(0, Count - 1));
	const float Offset = static_cast<float>(SlotIndex) - Half;
	const float Norm = Half > 0.f ? Offset / Half : 0.f;
	const float Room = FMath::Max(320.f, static_cast<float>(GetViewportSizeInSlate().X) - 2.f * HandSideMargin);
	const float Width = FMath::Min(MaxHandWidth * (Count > CrowdedHandCount ? 1.45f : 1.f), Room);
	const float Spacing = Count > 1 ? FMath::Min(CardSpacing * GetHandScale(Count), Width / static_cast<float>(Count - 1)) : CardSpacing;
	const float Tilt = FMath::Min(FanAnglePerCard * Half, MaxFanAngle);
	const float Drop = FMath::Min(ArcDrop * Half * Half, MaxArcDrop);
	OutPos = FVector2D(Offset * Spacing, HandBaseY + Drop * Norm * Norm);
	OutAngle = Norm * Tilt;
}

float UHexHandWidget::GetHandScale(int32 Count) const
{
	return Count > CrowdedHandCount
		? FMath::Clamp(static_cast<float>(CrowdedHandCount) / static_cast<float>(Count), MinHandScale, 1.f)
		: 1.f;
}

FVector2D UHexHandWidget::GetViewportSizeInSlate() const
{
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(const_cast<UHexHandWidget*>(this));
	const FVector2D Size = UWidgetLayoutLibrary::GetViewportSize(const_cast<UHexHandWidget*>(this));
	return Scale > KINDA_SMALL_NUMBER ? Size / Scale : Size;
}

FVector2D UHexHandWidget::GetMouseInHandSpace() const
{
	const FVector2D Mouse = UWidgetLayoutLibrary::GetMousePositionOnViewport(const_cast<UHexHandWidget*>(this));
	const FVector2D View = GetViewportSizeInSlate();
	return FVector2D(Mouse.X - View.X * 0.5, Mouse.Y - View.Y);
}

FVector2D UHexHandWidget::GetDeckPosition() const
{
	const UHexCardSubsystem* Cards = GetCards();
	if (Cards && !Cards->IsActive())
	{
		return FVector2D(0.0, 240.0);	// designed hands have no pile: cards rise from below the screen
	}
	const FVector2D View = GetViewportSizeInSlate();
	return FVector2D(View.X * 0.5 + DeckOffset.X, DeckOffset.Y);
}

FVector2D UHexHandWidget::WorldToHandSpace(const FVector& World, bool& bOutOk) const
{
	FVector2D Screen = FVector2D::ZeroVector;
	bOutOk = false;
	if (APlayerController* PC = GetOwningPlayer())
	{
		bOutOk = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, World, Screen, false);
	}
	const FVector2D View = GetViewportSizeInSlate();
	return FVector2D(Screen.X - View.X * 0.5, Screen.Y - View.Y);
}

void UHexHandWidget::StepSpring(FSpring2D& Spring, const FVector2D& Target, float DeltaTime) const
{
	const int32 Steps = FMath::Max(1, FMath::CeilToInt32(DeltaTime / (1.f / 120.f)));
	const double Dt = static_cast<double>(DeltaTime) / Steps;
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		Spring.Velocity += ((Target - Spring.Value) * static_cast<double>(SpringStiffness) - Spring.Velocity * static_cast<double>(SpringDamping)) * Dt;
		Spring.Value += Spring.Velocity * Dt;
	}
}

void UHexHandWidget::StepSpring(FSpring1D& Spring, float Target, float DeltaTime) const
{
	const int32 Steps = FMath::Max(1, FMath::CeilToInt32(DeltaTime / (1.f / 120.f)));
	const float Dt = DeltaTime / static_cast<float>(Steps);
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		Spring.Velocity += ((Target - Spring.Value) * SpringStiffness - Spring.Velocity * SpringDamping) * Dt;
		Spring.Value += Spring.Velocity * Dt;
	}
}
