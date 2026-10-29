// In-game HUD (UMG, Chinese, one font family): level / moves / goals card, rule card, control hint,
// "stuck" warning, the game-over banner and the level-clear results card (turns vs. optimal, 1-3 stars).
// Created by AHexSokobanHUD.
//
// Customise: Widget Blueprint child (Project Settings > Hex Sokoban > UI > HUD Widget Class). An empty
// designer tree gets the default layout; otherwise name parts like the BindWidgetOptional members.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HexSokobanTypes.h"
#include "HexCardTypes.h"
#include "HexHUDWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UTextBlock;
class UHorizontalBox;
class UImage;
class AHexSokobanGameMode;
class UHexBoardSubsystem;
class AHexTile;
class UHexCardInfoWidget;

struct FHexNarrationEntry
{
	TWeakObjectPtr<UBorder> Panel;
	float Age = 0.f;
	float Y = 24.f;
};

UCLASS(Blueprintable)
class HEXSOKOBAN_API UHexHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** Optional child class supplies skill-card descriptions without changing HUD layout. */
	UPROPERTY(EditAnywhere, Category = "Hex HUD|Card Info")
	TSubclassOf<UHexCardInfoWidget> CardInfoClass;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Hex HUD|Card Info")
	TObjectPtr<UHexCardInfoWidget> CardInfoPanel;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Results card (built at runtime when the designer tree has none). */
	void BuildResultPanel();
	void UpdateResultPanel(bool bShow, const FHexLevelResult& Result, int32 LevelNumber, bool bHasNext, float Elapsed);

	void BuildDefaultTree();
	void RefreshGoalPips(int32 Goals, int32 Covered);
	void ShowBanner(bool bShow, const FText& Title, const FText& Line1, const FText& Line2, const FLinearColor& Color, float Elapsed);

	// ---- Parts ----
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> LevelText;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> LevelTotalText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LevelNameText;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> MovesText;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> GoalPips;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> GoalCountText;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UBorder> RulePanel;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HintText;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UBorder> StuckPanel;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StuckText;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UBorder> BannerPanel;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> BannerTitle;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> BannerLine1;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> BannerLine2;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UBorder> ResultOverlay;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UBorder> ResultCard;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResultLevelText;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResultTurnsText;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResultBestText;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResultVerdictText;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResultThresholdText;
	UPROPERTY(BlueprintReadOnly, Category = "Hex HUD|Parts", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ResultFooterText;

	/** The three stars (left to right). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ResultStars;

	/** Banner height (slate units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex HUD")
	float BannerHeight = 270.f;

private:
	void BuildNarrationPanel();
	void TickNarration(float DeltaTime, const FVector2D& ViewSize);
	UFUNCTION() void HandleNarrationCard(const FHexCard& Card, AHexTile* TargetTile);
	UFUNCTION() void ClearNarration();
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> NarrationCanvas;
	TArray<FHexNarrationEntry> NarrationEntries;
	uint8 PlayedResultStarSounds = 0;
	int32 ShownGoals = -1;
	int32 ShownCovered = -1;
};
