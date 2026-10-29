#include "HexSokobanGameMode.h"
#include "HexLevelTools.h"
#include "HexGridLibrary.h"
#include "HexTile.h"
#include "HexBox.h"
#include "HexSokobanSettings.h"
#include "HexBoardSubsystem.h"
#include "HexGridMovementComponent.h"
#include "HexAudioSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

void AHexSokobanGameMode::SpawnEntranceTile(const FHexTileDef& Tile)
{
	AActor* Actor = UHexLevelTools::SpawnTileActor(GetWorld(), Tile);
	if (!Actor) return;
	if (AHexTile* Hex = Cast<AHexTile>(Actor)) { Hex->SetCoord(Tile.Coord); if (Tile.HasFace()) Hex->SetCardLabel(Tile.GetFace()); }
	FEntrancePop& Pop = EntrancePops.AddDefaulted_GetRef();
	Pop.Actor = Actor; Pop.BaseScale = Actor->GetActorScale3D();
	Actor->SetActorScale3D(Pop.BaseScale*.55);
	UHexAudioSubsystem::Play(this, TEXT("Pop"), .35f);
}

void AHexSokobanGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuildingLevel) return;
	if (UHexGridMovementComponent* Mover = GetMover())
	{
		Mover->SetMovementLocked(true);
		if (ACharacter* Character = Cast<ACharacter>(Mover->GetOwner()))
			Character->GetCharacterMovement()->DisableMovement();
	}
	EntranceDelay -= DeltaSeconds;
	if (EntranceDelay <= 0 && EntranceIndex < EntranceDefinition.Tiles.Num())
	{
		const double Started = FPlatformTime::Seconds();
		const int32 Batch = EntranceDefinition.NeedsPerformanceMode() ? 3 : 1;
		for (int32 I=0; I<Batch && EntranceIndex<EntranceDefinition.Tiles.Num(); ++I)
		{
			SpawnEntranceTile(EntranceDefinition.Tiles[EntranceIndex++]);
			if (FPlatformTime::Seconds()-Started >= .003) break;
		}
		EntranceDelay = .035f;
	}
	// Same damped squash/spring as editor placement; only the current wave ticks.
	for (int32 I=EntrancePops.Num()-1; I>=0; --I)
	{
		FEntrancePop& Pop=EntrancePops[I]; AActor* Actor=Pop.Actor.Get();
		if (!Actor) { EntrancePops.RemoveAtSwap(I); continue; }
		Pop.Age += DeltaSeconds;
		float Remaining=FMath::Min(DeltaSeconds,.05f);
		while (Remaining>0) {
			const float Dt=FMath::Min(Remaining,.008f); Remaining-=Dt;
			Pop.Velocity+=((1-Pop.Value)*320-Pop.Velocity*14)*Dt; Pop.Value+=Pop.Velocity*Dt;
			Pop.SquashVelocity+=(-Pop.Squash*380-Pop.SquashVelocity*16)*Dt; Pop.Squash+=Pop.SquashVelocity*Dt;
		}
		Actor->SetActorScale3D(Pop.BaseScale*FVector(Pop.Value*(1-.5*Pop.Squash),Pop.Value*(1-.5*Pop.Squash),Pop.Value*(1+Pop.Squash)));
		if (Pop.Age>=.9f) { Actor->SetActorScale3D(Pop.BaseScale); EntrancePops.RemoveAtSwap(I); }
	}
	if (EntranceIndex<EntranceDefinition.Tiles.Num() || !EntrancePops.IsEmpty()) return;
	// Boxes are also created across frames, after their terrain is settled.
	const int32 BoxIndex = EntranceIndex-EntranceDefinition.Tiles.Num();
	if (EntranceDefinition.Boxes.IsValidIndex(BoxIndex))
	{
		UClass* Class=UHexSokobanSettings::Get().BoxClass.LoadSynchronous();
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GetWorld()->SpawnActor<AActor>(Class ? Class : AHexBox::StaticClass(),UHexGridLibrary::CoordToWorld(EntranceDefinition.Boxes[BoxIndex])+FVector(0,0,10),FRotator::ZeroRotator,Params);
		++EntranceIndex;
		return;
	}
	UHexBoardSubsystem* Board=GetBoard();
	Board->RebuildBoard();
	if (UHexGridMovementComponent* Mover=GetMover())
	{
		Mover->TeleportToCoord(EntranceDefinition.PlayerStart);
		if (ACharacter* Character=Cast<ACharacter>(Mover->GetOwner())) Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	bBuildingLevel=false;
	Board->CaptureInitialState();
	InitializeCards();
	SetLevelState(EHexLevelState::Playing);
	EntranceDefinition=FHexLevelDefinition();
}
