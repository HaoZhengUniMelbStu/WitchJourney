// Card label shown on top of a tile (or a box): suit icon + rank, e.g. "♠A".
// Runtime-only visuals (two quads sampling a glyph atlas, or a text fallback) with bouncy pop / hide /
// rise animations driven by a damped spring, matching the box squash feel.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "HexCardTypes.h"
#include "HexCardLabelComponent.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

UCLASS(ClassGroup = (HexSokoban), meta = (BlueprintSpawnableComponent))
class HEXSOKOBAN_API UHexCardLabelComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UHexCardLabelComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Hex Card Label")
	void SetCard(const FHexCard& InCard);

	UFUNCTION(BlueprintCallable, Category = "Hex Card Label")
	void ClearCard();

	UFUNCTION(BlueprintPure, Category = "Hex Card Label")
	bool HasCard() const { return bHasCard; }

	UFUNCTION(BlueprintPure, Category = "Hex Card Label")
	FHexCard GetCard() const { return Card; }

	/** Show with a bouncy pop after Delay seconds (or instantly). */
	UFUNCTION(BlueprintCallable, Category = "Hex Card Label")
	void ShowLabel(bool bAnimate = true, float Delay = 0.f);

	UFUNCTION(BlueprintCallable, Category = "Hex Card Label")
	void HideLabel(bool bAnimate = true);

	/** Float up by RiseHeight while swelling, then vanish: the label "becomes" the top of the box landing here. */
	UFUNCTION(BlueprintCallable, Category = "Hex Card Label")
	void RiseAndHide(float InRiseHeight);

	UFUNCTION(BlueprintPure, Category = "Hex Card Label")
	bool IsLabelShown() const { return bWantShown; }

	/** Resting position relative to the parent (set by the owning tile / box). */
	void SetBaseLocation(const FVector& InBaseLocation);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card Label")
	float PopStiffness = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card Label")
	float PopDamping = 15.f;

	/** Extra outward kick when popping in (bigger = more overshoot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card Label")
	float PopKick = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card Label")
	float RiseDuration = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card Label")
	FLinearColor RedColor = FLinearColor(0.80f, 0.04f, 0.04f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex Card Label")
	FLinearColor BlackColor = FLinearColor(0.03f, 0.03f, 0.05f, 1.f);

private:
	void EnsureVisuals();
	void ApplyCardVisuals();
	void ApplyTransform();
	UStaticMeshComponent* MakeQuad(const FVector& Location, float Size);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SuitQuad;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RankQuad;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> FallbackText;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SuitMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RankMID;

	FHexCard Card;
	bool bHasCard = false;
	bool bWantShown = false;
	bool bVisualsBuilt = false;

	FVector BaseLocation = FVector::ZeroVector;
	float Scale = 0.f;
	float ScaleVelocity = 0.f;
	float TargetScale = 0.f;
	float ShowDelay = 0.f;
	bool bPendingKick = false;

	float RiseTime = -1.f;		// < 0 = not rising
	float RiseHeight = 0.f;
	float RiseOffset = 0.f;
};
