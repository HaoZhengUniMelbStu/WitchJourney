// HUD host. Shows UHexHUDWidget (UMG, Chinese, Project Settings > Hex Sokoban > UI). The old canvas
// drawing is kept only as a fallback when the widget can't be created. bDrawDefaultHUD = false hides both.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HexSokobanHUD.generated.h"

class UFont;
class UHexHUDWidget;

UCLASS()
class HEXSOKOBAN_API AHexSokobanHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	UHexHUDWidget* GetHUDWidget() const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hex Sokoban")
	bool bDrawDefaultHUD = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hex Sokoban")
	FLinearColor AccentColor = FLinearColor(1.f, 0.69f, 0.08f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hex Sokoban")
	FLinearColor BannerColor = FLinearColor(0.05f, 0.55f, 0.22f, 0.92f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hex Sokoban")
	FLinearColor FailColor = FLinearColor(0.75f, 0.12f, 0.12f, 0.92f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Hex Sokoban")
	int32 HUDWidgetZOrder = 5;

private:
	UPROPERTY(Transient)
	TObjectPtr<UHexHUDWidget> HUDWidget;

	void DrawShadowedText(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale);
	void DrawCenteredText(const FString& Text, const FLinearColor& Color, float CenterX, float Y, UFont* Font, float Scale);
	void DrawBanner(const FString& Title, const FString& Line1, const FString& Line2, const FLinearColor& Color, float StartTime, float UIScale);
};
