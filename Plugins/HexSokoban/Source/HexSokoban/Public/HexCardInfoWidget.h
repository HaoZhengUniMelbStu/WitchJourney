#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HexCardTypes.h"
#include "HexCardInfoWidget.generated.h"

class UBorder;
class UVerticalBox;
class UTextBlock;
class UHexCardWidget;

/** Presentation data, separate from movement rules; skill cards can supply their own fields. */
USTRUCT(BlueprintType)
struct HEXSOKOBAN_API FHexCardInfo
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Terrain;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Condition;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Effect;
};

UCLASS(Blueprintable)
class HEXSOKOBAN_API UHexCardInfoWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Override in a child widget to resolve future skill-card metadata. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Card Info")
	FHexCardInfo ResolveInfo(const FHexCard& Card) const;
	void UpdateInspection(const FHexCard* Card, float DeltaTime);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	UPROPERTY(Transient) TObjectPtr<UBorder> Frame;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Body;
	UPROPERTY(Transient) TObjectPtr<UHexCardWidget> Preview;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Title;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Description;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Terrain;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Condition;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Effect;
	FHexCard DisplayedCard;
	bool bHasCard = false;
	float FrameProgress = 0.f;
	float ContentProgress = 0.f;
};
