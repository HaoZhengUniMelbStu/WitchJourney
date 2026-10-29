#include "HexSokobanPlayerController.h"
#include "HexSokobanGameMode.h"
#include "HexCameraBounds.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"

void AHexSokobanPlayerController::ZoomIn() { ZoomBoard(.88f); }
void AHexSokobanPlayerController::ZoomOut() { ZoomBoard(1.f/.88f); }
void AHexSokobanPlayerController::ZoomBoard(float Factor)
{
	if (!BoardCamera || UGameplayStatics::IsGamePaused(this) || IsLevelCompleted()) return;
	CameraTargetDistance=FMath::Clamp(CameraTargetDistance*Factor,CameraMinDistance,CameraMaxDistance);
}
FVector AHexSokobanPlayerController::ConstrainBoardFocus(const FVector& Focus) const
{
	// At full-map zoom the board is centred; closer views may pan to its outer tiles.
	const double Range=CameraMaxDistance-CameraMinDistance;
	const double Scale=Range>1.f ? FMath::Clamp((CameraMaxDistance-CameraTargetDistance)/Range,0.0,1.0) : 0.0;
	if(Scale<0.001) return CameraBoardCenter;
	const FVector2D Center(CameraBoardCenter);
	const FVector2D Local=Center+(FVector2D(Focus)-Center)/Scale;
	const FVector2D Clamped=Center+(HexCameraBounds::Clamp(Local,CameraPanHull)-Center)*Scale;
	return FVector(Clamped.X,Clamped.Y,CameraBoardCenter.Z);
}
void AHexSokobanPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!BoardCamera || UGameplayStatics::IsGamePaused(this) || IsLevelCompleted()) { bPanningBoard=false; return; }
	float X=0.f,Y=0.f;
	const bool bPan=IsInputKeyDown(EKeys::RightMouseButton) && GetMousePosition(X,Y);
	if (bPan)
	{
		const FVector2D Mouse(X,Y);
		if (bPanningBoard)
		{
			const FVector2D Delta=Mouse-LastPanMouse;
			const FRotator R=BoardCamera->GetActorRotation();
			const FVector Right=FRotationMatrix(FRotator(0,R.Yaw,0)).GetUnitAxis(EAxis::Y);
			const FVector Forward=FRotationMatrix(FRotator(0,R.Yaw,0)).GetUnitAxis(EAxis::X);
			int32 Width,Height; GetViewportSize(Width,Height);
			const float Units=CameraDistance*0.72f/FMath::Max(1,Width);
			const FVector Proposed=CameraTargetFocus+(-Right*Delta.X+Forward*Delta.Y)*Units;
			const FVector Edge=ConstrainBoardFocus(Proposed);
			const FVector2D Soft=HexCameraBounds::Rubber(FVector2D(Proposed),FVector2D(Edge),CameraEdgeAllowance);
			CameraTargetFocus=FVector(Soft.X,Soft.Y,CameraBoardCenter.Z);
		}
		LastPanMouse=Mouse;
	}
	bPanningBoard=bPan;
	if (!bPan) CameraTargetFocus=ConstrainBoardFocus(CameraTargetFocus);
	for (float Remaining=FMath::Min(DeltaTime,0.1f); Remaining>0.f;)
	{
		const float Step=FMath::Min(Remaining,1.f/120.f); Remaining-=Step;
		CameraPanVelocity+=((CameraTargetFocus-CameraFocus)*80.f-CameraPanVelocity*15.f)*Step;
		CameraFocus+=CameraPanVelocity*Step;
	}
	CameraDistance=FMath::FInterpTo(CameraDistance,CameraTargetDistance,DeltaTime,12.f);
	BoardCamera->SetActorLocation(CameraFocus-BoardCamera->GetActorForwardVector()*CameraDistance);
}
