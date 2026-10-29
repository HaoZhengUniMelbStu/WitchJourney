// Input for Hex Sokoban. Builds its own Enhanced Input actions at runtime, so no input assets are needed:
//   Left click   : step towards the clicked point (snapped to one of six directions)
//   W E D S A Q  : step up / up-right / down-right / down / down-left / up-left (relative to the camera)
//   Z, Backspace : undo        R : restart        Enter, Space : next level (after a win)
// Card mode (Project Settings > Hex Sokoban > Cards):
//   H : show / hide the next steps of a winning line (Path FX)
//   1..7 : select hand card     Esc, right click : deselect     drag a card onto a glowing tile : play it

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HexSokobanPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UHexGridMovementComponent;
class AHexSokobanGameMode;
class ACameraActor;
class UHexHandWidget;
class UHexPauseMenuWidget;

UCLASS()
class HEXSOKOBAN_API AHexSokobanPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHexSokobanPlayerController();

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban")
	UHexGridMovementComponent* GetHexMover() const;

	/** The on-screen hand (card mode), or null. */
	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Cards")
	UHexHandWidget* GetHandWidget() const { return HandWidget; }

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

	/** Priority of the runtime mapping context. */
	UPROPERTY(EditDefaultsOnly, Category = "Hex Sokoban|Input")
	int32 MappingPriority = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Hex Sokoban|Input")
	bool bEnableKeyboardMovement = true;

	UPROPERTY(EditDefaultsOnly, Category = "Hex Sokoban|Input")
	bool bEnableMouseMovement = true;

	/** Step towards a screen direction, 0 = up, 90 = right (degrees, clockwise). */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban")
	void MoveInScreenDirection(float ScreenAngle);

	/** Step towards whatever is under the mouse cursor. */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban")
	void MoveTowardsCursor();

	/** (Re)frame the fixed board camera around all tiles (Project Settings > Hex Sokoban > Camera). */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban")
	void SetupBoardCamera();

	/** Create the hand widget (Project Settings > Hex Sokoban > Cards > Hand Widget Class). */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Cards")
	void CreateHandWidget();

	/** Z order of the hand widget in the viewport. */
	UPROPERTY(EditDefaultsOnly, Category = "Hex Sokoban|Cards")
	int32 HandWidgetZOrder = 10;

private:
	void ZoomIn();
	void ZoomOut();
	void ZoomBoard(float Factor);
	FVector ConstrainBoardFocus(const FVector& Focus) const;
	TArray<FVector2D> CameraPanHull;
	FVector CameraBoardCenter = FVector::ZeroVector;
	float CameraMinDistance = 2800.f;
	float CameraEdgeAllowance = 80.f;
	FVector CameraFocus = FVector::ZeroVector;
	FVector CameraPanVelocity = FVector::ZeroVector;
	FVector CameraTargetFocus = FVector::ZeroVector;
	float CameraDistance = 2000.f;
	float CameraTargetDistance = 2000.f;
	float CameraMaxDistance = 12000.f;
	FVector2D LastPanMouse = FVector2D::ZeroVector;
	bool bPanningBoard = false;
	void BuildRuntimeInput();
	UInputAction* MakeAction(const TCHAR* Name);
	AHexSokobanGameMode* GetHexGameMode() const;
	bool IsLevelCompleted() const;

	void OnClick();
	void OnUndo();
	void OnRestart();
	void OnConfirm();
	void OnDirUp();
	void OnDirUpRight();
	void OnDirDownRight();
	void OnDirDown();
	void OnDirDownLeft();
	void OnDirUpLeft();
	void OnDeselect();
	void OnToggleHint();

	/** Esc: drop the selected card first; otherwise open / close the pause menu (pauses the game). */
	void OnPause();
	void ClosePauseMenu();
	void SelectCardSlot(int32 HandIndex);
	void OnSelect1() { SelectCardSlot(0); }
	void OnSelect2() { SelectCardSlot(1); }
	void OnSelect3() { SelectCardSlot(2); }
	void OnSelect4() { SelectCardSlot(3); }
	void OnSelect5() { SelectCardSlot(4); }
	void OnSelect6() { SelectCardSlot(5); }
	void OnSelect7() { SelectCardSlot(6); }

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> BoardCamera;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RuntimeMapping;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ClickAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> UndoAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RestartAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ConfirmAction;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> DirectionActions;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DeselectAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> HintAction;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> CardSelectActions;

	UPROPERTY(Transient)
	TObjectPtr<UHexHandWidget> HandWidget;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PauseAction;

	UPROPERTY(Transient)
	TObjectPtr<UHexPauseMenuWidget> PauseMenu;
};
