#include "HexSokobanHUD.h"
#include "HexBoardSubsystem.h"
#include "HexSokobanGameMode.h"
#include "HexHUDWidget.h"
#include "HexSokobanSettings.h"
#include "GameFramework/PlayerController.h"
#include "HexCardSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"

void AHexSokobanHUD::DrawShadowedText(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale)
{
	const float Offset = FMath::Max(1.f, 2.f * Scale);
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, 0.45f * Color.A), X + Offset, Y + Offset, Font, Scale);
	DrawText(Text, Color, X, Y, Font, Scale);
}

void AHexSokobanHUD::DrawCenteredText(const FString& Text, const FLinearColor& Color, float CenterX, float Y, UFont* Font, float Scale)
{
	float Width = 0.f, Height = 0.f;
	GetTextSize(Text, Width, Height, Font, Scale);
	DrawShadowedText(Text, Color, CenterX - Width * 0.5f, Y, Font, Scale);
}

void AHexSokobanHUD::BeginPlay()
{
	Super::BeginPlay();
	APlayerController* PC = GetOwningPlayerController();
	if (!bDrawDefaultHUD || !PC || !PC->IsLocalController())
	{
		return;
	}
	TSubclassOf<UHexHUDWidget> Class = UHexSokobanSettings::Get().HUDWidgetClass.TryLoadClass<UHexHUDWidget>();
	if (!Class)
	{
		Class = UHexHUDWidget::StaticClass();
	}
	HUDWidget = CreateWidget<UHexHUDWidget>(PC, Class);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(HUDWidgetZOrder);
	}
}

UHexHUDWidget* AHexSokobanHUD::GetHUDWidget() const
{
	return HUDWidget;
}

void AHexSokobanHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!bDrawDefaultHUD || HUDWidget || !Canvas || !GEngine)
	{
		return;	// the UMG HUD is up (or the HUD is off)
	}

	UWorld* World = GetWorld();
	const UHexBoardSubsystem* Board = World ? World->GetSubsystem<UHexBoardSubsystem>() : nullptr;
	const AHexSokobanGameMode* GameMode = World ? World->GetAuthGameMode<AHexSokobanGameMode>() : nullptr;
	if (!Board)
	{
		return;
	}

	UFont* SmallFont = GEngine->GetMediumFont();
	UFont* LargeFont = GEngine->GetLargeFont();
	const float ScreenW = Canvas->ClipX;
	const float ScreenH = Canvas->ClipY;
	const float S = FMath::Clamp(ScreenH / 1080.f, 0.6f, 2.f); // UI scale

	// ---- Level / moves card ----
	const float CardX = 28.f * S, CardY = 28.f * S, CardW = 250.f * S, CardH = 118.f * S;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.28f), CardX + 5.f * S, CardY + 6.f * S, CardW, CardH);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.93f), CardX, CardY, CardW, CardH);
	DrawRect(AccentColor, CardX, CardY, 10.f * S, CardH);

	FString LevelLabel = TEXT("LEVEL");
	if (GameMode && GameMode->GetNumLevels() > 0)
	{
		const int32 Index = GameMode->GetCurrentLevelIndex();
		LevelLabel = FString::Printf(TEXT("LEVEL %d / %d"), Index >= 0 ? Index + 1 : 1, GameMode->GetNumLevels());
	}
	const FLinearColor Ink(0.12f, 0.16f, 0.28f, 1.f);
	DrawText(LevelLabel, FLinearColor(0.45f, 0.5f, 0.62f, 1.f), CardX + 26.f * S, CardY + 12.f * S, SmallFont, 1.15f * S);
	DrawText(TEXT("MOVES"), FLinearColor(0.45f, 0.5f, 0.62f, 1.f), CardX + 26.f * S, CardY + 72.f * S, SmallFont, 1.f * S);
	DrawText(FString::FromInt(Board->GetMoveCount()), Ink, CardX + 120.f * S, CardY + 38.f * S, LargeFont, 2.6f * S);

	// ---- Goal pips ----
	const int32 Goals = Board->GetNumGoals();
	const int32 Covered = Board->GetNumCoveredGoals();
	const float Pip = 22.f * S, Gap = 8.f * S;
	float PipX = CardX + 4.f * S;
	const float PipY = CardY + CardH + 14.f * S;
	for (int32 Index = 0; Index < Goals; ++Index)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.3f), PipX + 3.f * S, PipY + 3.f * S, Pip, Pip);
		DrawRect(FLinearColor::White, PipX, PipY, Pip, Pip);
		const FLinearColor Fill = Index < Covered ? AccentColor : FLinearColor(0.75f, 0.8f, 0.88f, 1.f);
		DrawRect(Fill, PipX + 4.f * S, PipY + 4.f * S, Pip - 8.f * S, Pip - 8.f * S);
		PipX += Pip + Gap;
	}

	// ---- Control hint ----
	const UHexCardSubsystem* Cards = GetWorld()->GetSubsystem<UHexCardSubsystem>();
	if (Cards && Cards->IsActive())
	{
		// The hand sits at the bottom, so the hint goes to the top.
		DrawCenteredText(TEXT("MATCH SUIT OR RANK TO STEP      DRAG A CARD ONTO A TILE  /  1-4 SELECT  /  CLICK  Q W E A S D      Z UNDO   R RESTART"),
			FLinearColor(1.f, 1.f, 1.f, 0.8f), ScreenW * 0.5f, 18.f * S, SmallFont, 1.0f * S);
	}
	else
	{
		DrawCenteredText(TEXT("CLICK A NEIGHBOUR  /  Q W E  A S D      Z  UNDO      R  RESTART"),
			FLinearColor(1.f, 1.f, 1.f, 0.8f), ScreenW * 0.5f, ScreenH - 52.f * S, SmallFont, 1.05f * S);
	}

	// ---- Stuck warning (counting down to game over) ----
	if (GameMode && GameMode->IsStuck() && GameMode->GetLevelState() == EHexLevelState::Playing)
	{
		const float Pulse = 0.75f + 0.25f * FMath::Sin(World->GetTimeSeconds() * 10.f);
		const float PillW = 620.f * S, PillH = 74.f * S;
		const float PillX = (ScreenW - PillW) * 0.5f, PillY = 30.f * S;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.3f), PillX + 5.f * S, PillY + 6.f * S, PillW, PillH);
		DrawRect(FLinearColor(FailColor.R, FailColor.G, FailColor.B, Pulse), PillX, PillY, PillW, PillH);
		const float Remaining = GameMode->GetStuckTimeRemaining();
		const FString Warning = Remaining >= 0.f
			? FString::Printf(TEXT("STUCK!   Z TO UNDO   -   RESTART IN %d"), FMath::CeilToInt(Remaining))
			: FString(TEXT("STUCK!   Z TO UNDO   /   R TO RESTART"));
		DrawCenteredText(Warning, FLinearColor::White, ScreenW * 0.5f, PillY + 20.f * S, SmallFont, 1.5f * S);
	}

	// ---- Banners ----
	if (GameMode && GameMode->GetLevelState() == EHexLevelState::Failed)
	{
		DrawBanner(TEXT("STUCK!"), TEXT("NO WAY TO FINISH FROM HERE"), TEXT("RESTARTING...   (CLICK TO RESTART NOW)"),
			FailColor, GameMode->GetFailedTime(), S);
	}
	else if (GameMode && GameMode->GetLevelState() == EHexLevelState::Completed)
	{
		const FString Hint = GameMode->HasNextLevel()
			? TEXT("CLICK / ENTER  -  NEXT LEVEL        R  -  REPLAY")
			: TEXT("ALL LEVELS CLEARED!     CLICK  -  BACK TO LEVEL 1     R  -  REPLAY");
		DrawBanner(TEXT("LEVEL CLEAR!"), FString::Printf(TEXT("%d MOVES"), GameMode->GetMoveCount()), Hint,
			BannerColor, GameMode->GetCompletedTime(), S);
	}
}

void AHexSokobanHUD::DrawBanner(const FString& Title, const FString& Line1, const FString& Line2, const FLinearColor& Color, float StartTime, float UIScale)
{
	UWorld* World = GetWorld();
	UFont* SmallFont = GEngine->GetMediumFont();
	UFont* LargeFont = GEngine->GetLargeFont();
	const float ScreenW = Canvas->ClipX;
	const float ScreenH = Canvas->ClipY;
	const float S = UIScale;

	const float Elapsed = (StartTime >= 0.f && World) ? World->GetTimeSeconds() - StartTime : 1.f;
	const float Appear = FMath::Clamp(Elapsed / 0.25f, 0.f, 1.f);
	const float Ease = 1.f - FMath::Pow(1.f - Appear, 3.f);
	const float Pop = 1.f + 0.35f * FMath::Exp(-Elapsed * 7.f) * FMath::Cos(Elapsed * 18.f); // overshoot

	const float BandH = 190.f * S * Ease;
	const float BandY = ScreenH * 0.40f - BandH * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.25f * Ease), 0.f, BandY + 8.f * S, ScreenW, BandH);
	DrawRect(FLinearColor(Color.R, Color.G, Color.B, Color.A * Ease), 0.f, BandY, ScreenW, BandH);
	DrawRect(FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, Ease), 0.f, BandY, ScreenW, 8.f * S * Ease);

	if (Appear >= 1.f)
	{
		DrawCenteredText(Title, FLinearColor::White, ScreenW * 0.5f, BandY + 22.f * S, LargeFont, 3.f * S * Pop);
		DrawCenteredText(Line1, AccentColor, ScreenW * 0.5f, BandY + 102.f * S, SmallFont, 1.6f * S);
		DrawCenteredText(Line2, FLinearColor(1.f, 1.f, 1.f, 0.9f), ScreenW * 0.5f, BandY + 142.f * S, SmallFont, 1.2f * S);
	}
}
