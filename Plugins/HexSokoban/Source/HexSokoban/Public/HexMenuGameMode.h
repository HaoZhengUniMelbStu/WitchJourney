// Menu map: the first level's board as a slowly swaying backdrop, the main menu / level select widget on top.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HexMenuGameMode.generated.h"

class ACameraActor;
class UHexMenuWidget;

UCLASS()
class HEXSOKOBAN_API AHexMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHexMenuGameMode();
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	ACameraActor* GetMenuCamera() const { return MenuCamera; }

	/** Frame Actors from the HUD camera angle; ScreenShift moves the framed board sideways (fraction of its width). */
	static bool FrameActors(const UWorld* World, const TArray<AActor*>& Actors, float ScreenShift, FVector& OutLocation, FRotator& OutRotation);

private:
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> MenuCamera;

	FVector BaseLocation = FVector::ZeroVector;
	FRotator BaseRotation = FRotator::ZeroRotator;
	FVector Pivot = FVector::ZeroVector;
	float Clock = 0.f;
};

UCLASS()
class HEXSOKOBAN_API AHexMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHexMenuPlayerController();
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UHexMenuWidget> MenuWidget;
};
