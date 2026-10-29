// Menu UI (UMG, built in code, same look as the HUD: light rounded cards with an accent stripe, dark pills,
// Noto Sans SC, springy feedback):
//  - UHexMenuButton:     reusable button (primary / secondary / dark pill / danger), hover lift + press squash.
//  - UHexLevelCardWidget: one level in the level select grid (number, name, best stars, best turns).
//  - UHexMenuWidget:      main menu + level select (one widget, two screens with a slide between them).
//  - UHexPauseMenuWidget: Esc menu in a level (continue / restart / level select / main menu / quit).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HexLevelTypes.h"
#include "HexGameFlowSubsystem.h"
#include "HexMenuWidgets.generated.h"

class UButton;
class UBorder;
class UTextBlock;
class USizeBox;
class UCanvasPanel;
class UVerticalBox;
class UHorizontalBox;
class UWrapBox;
class UScrollBox;

UENUM()
enum class EHexButtonStyle : uint8
{
	Primary,	// light card, yellow stripe
	Secondary,	// light card, grey stripe
	Dark,		// dark pill (like the HUD hint bar)
	Danger		// light card, red stripe + red text
};

UCLASS()
class HEXSOKOBAN_API UHexMenuButton : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FText& InLabel, EHexButtonStyle InStyle, float InWidth, float InHeight, TFunction<void()> InOnClicked);
	void SetLabel(const FText& InLabel);

	TFunction<void()> OnClickedFunc;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandlePressAudio();

	void ApplyStyle();

	UPROPERTY(Transient) TObjectPtr<USizeBox> Box;
	UPROPERTY(Transient) TObjectPtr<UButton> Button;
	UPROPERTY(Transient) TObjectPtr<UBorder> Face;
	UPROPERTY(Transient) TObjectPtr<UBorder> Stripe;
	UPROPERTY(Transient) TObjectPtr<USizeBox> StripeBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Label;

	EHexButtonStyle Style = EHexButtonStyle::Primary;
	float Width = 320.f;
	float Height = 58.f;
	bool bAudioHovered = false;
	float Scale = 1.f;
	float ScaleVelocity = 0.f;
	float Lift = 0.f;
	float LiftVelocity = 0.f;
};

UCLASS()
class HEXSOKOBAN_API UHexLevelCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(int32 InNumber, const FHexLevelEntry& Entry, const FHexLevelRecord& Record, float AppearDelay);

	TFunction<void()> OnPlay;
	TFunction<void()> OnEdit;
	TFunction<void()> OnDelete;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandlePressAudio();

	UPROPERTY(Transient) TObjectPtr<UButton> Button;
	UPROPERTY(Transient) TObjectPtr<UBorder> Face;
	UPROPERTY(Transient) TObjectPtr<UBorder> Stripe;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NumberText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BestText;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> StarRow;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> ToolRow;
	UPROPERTY(Transient) TObjectPtr<UBorder> CustomTag;

	bool bAudioHovered = false;
	float Scale = 0.f;
	float ScaleVelocity = 0.f;
	bool bPlayedAppearSound = false;
	float Delay = 0.f;
	float Lift = 0.f;
	float LiftVelocity = 0.f;
};

UCLASS()
class HEXSOKOBAN_API UHexMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowScreen(EHexMenuScreen Screen);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void BuildMain(UCanvasPanel* Root);
	void BuildSelect(UCanvasPanel* Root);
	void RefreshLevelGrid();

	UPROPERTY(Transient) TObjectPtr<class UScaleBox> Portrait;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> MainPanel;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> SelectPanel;
	UPROPERTY(Transient) TObjectPtr<UWrapBox> LevelGrid;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SelectSubtitle;
	UPROPERTY(Transient) TObjectPtr<UHexMenuButton> StartButton;

	EHexMenuScreen Current = EHexMenuScreen::Main;
	float MainAlpha = 1.f;
	float SelectAlpha = 0.f;
};

UCLASS()
class HEXSOKOBAN_API UHexPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called when "continue" (or Esc) closes the menu. */
	TFunction<void()> OnResume;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(Transient) TObjectPtr<UBorder> Overlay;
	UPROPERTY(Transient) TObjectPtr<UBorder> Card;

	float Age = 0.f;
};
