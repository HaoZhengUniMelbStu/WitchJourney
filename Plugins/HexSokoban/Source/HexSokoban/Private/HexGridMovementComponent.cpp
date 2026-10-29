#include "HexGridMovementComponent.h"
#include "HexSokobanVisuals.h"
#include "HexSokobanMath.h"
#include "HexBoardSubsystem.h"
#include "HexGridLibrary.h"
#include "HexTile.h"
#include "HexBox.h"
#include "HexCardSubsystem.h"
#include "HexSokobanSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

UHexGridMovementComponent::UHexGridMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UHexGridMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	// The character is the one thing card faces and path lines never draw over.
	HexSokoban::MarkGuideOccluder(GetOwner(), HexSokoban::StencilCharacter);

	if (UHexBoardSubsystem* Board = GetBoard())
	{
		Board->EnsureBoardBuilt();
		SnapToNearestTile();
		Board->RegisterMover(this);
	}
}

void UHexGridMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopFlightFX();
	ResetBlinkVisual();
	if (UHexBoardSubsystem* Board = GetBoard())
	{
		Board->UnregisterMover(this);
	}
	Super::EndPlay(EndPlayReason);
}

UHexBoardSubsystem* UHexGridMovementComponent::GetBoard() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UHexBoardSubsystem>() : nullptr;
}

UHexCardSubsystem* UHexGridMovementComponent::GetCards() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UHexCardSubsystem>() : nullptr;
}

ACharacter* UHexGridMovementComponent::GetMovingCharacter() const
{
	if (!bUseCharacterMovement)
	{
		return nullptr;
	}
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	return (Character && Character->GetCharacterMovement()) ? Character : nullptr;
}

FVector UHexGridMovementComponent::GetStandLocation(const FIntPoint& InCoord) const
{
	const UHexBoardSubsystem* Board = GetBoard();
	const AHexTile* Tile = Board ? Board->GetTileAt(InCoord) : nullptr;
	const AActor* Owner = GetOwner();
	const FVector Surface = Tile ? Tile->GetSurfaceLocation()
		: UHexGridLibrary::CoordToWorld(InCoord, Owner ? static_cast<float>(Owner->GetActorLocation().Z - HeightAboveSurface) : 0.f);
	return FVector(Surface.X, Surface.Y, Surface.Z + HeightAboveSurface);
}

void UHexGridMovementComponent::SnapToNearestTile()
{
	UHexBoardSubsystem* Board = GetBoard();
	AActor* Owner = GetOwner();
	if (!Board || !Owner)
	{
		return;
	}

	const FVector Location = Owner->GetActorLocation();
	AHexTile* Tile = Board->GetTileAt(UHexGridLibrary::WorldToCoord(Location));
	if (!Tile)
	{
		Tile = Board->FindNearestTile(Location);
	}
	if (!Tile)
	{
		UE_LOG(LogHexSokoban, Warning, TEXT("%s: no HexTile in the level, grid movement disabled."), *Owner->GetName());
		bHasCoord = false;
		return;
	}

	// How high the actor origin sits above its feet.
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		HeightAboveSurface = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0;
	}
	else
	{
		FVector Origin, Extent;
		Owner->GetActorBounds(true, Origin, Extent);
		HeightAboveSurface = FMath::Max(0.0, Location.Z - (Origin.Z - Extent.Z));
	}

	bHasCoord = true;
	TeleportToCoord(Tile->GetCoord());
}

void UHexGridMovementComponent::TeleportToCoord(const FIntPoint& InCoord)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	bMoving = false;
	bTeleportPending = false;
	ResetBlinkVisual();
	StopFlightFX();
	BufferedDirection = EHexDirection::Invalid;
	Coord = InCoord;
	bHasCoord = true;
	ApplyPushSpeed(false);

	Owner->SetActorLocation(GetStandLocation(InCoord), false, nullptr, ETeleportType::TeleportPhysics);
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}
}

void UHexGridMovementComponent::SetMovementLocked(bool bInLocked)
{
	bLocked = bInLocked;
	if (bLocked)
	{
		BufferedDirection = EHexDirection::Invalid;
	}
}

EHexMoveResult UHexGridMovementComponent::TryMove(EHexDirection Direction)
{
	if (const UHexCardSubsystem* Cards = GetCards())
	{
		const auto Hand = Cards->GetHand();
		const int32 Selected = Cards->GetSelectedCardIndex();
		if (Hand.IsValidIndex(Selected) && Hand[Selected].Kind == EHexCardKind::Skill)
			return TryPlaySkillAt(Coord + HexSokoban::DirectionOffset(Direction));
	}
	if (bLocked)
	{
		return EHexMoveResult::Locked;
	}
	UHexBoardSubsystem* Board = GetBoard();
	if (!Board || !bHasCoord || Direction == EHexDirection::Invalid)
	{
		return EHexMoveResult::Invalid;
	}
	if (bMoving)
	{
		if (bBufferInput)
		{
			BufferedDirection = Direction;
		}
		return EHexMoveResult::Busy;
	}

	FHexMoveRecord Record;
	const EHexMoveResult Result = Board->EvaluateMove(Coord, Direction, Record);
	if (Result == EHexMoveResult::Moved || Result == EHexMoveResult::Pushed)
	{
		// Card rule: entering the target tile costs a matching card.
		bool bPaid = false;
		UHexCardSubsystem* Cards = GetCards();
		if (Cards && Cards->IsActive())
		{
			const EHexCardPayment Payment = Cards->PayForStep(Board->GetTileAt(Record.PlayerTo));
			if (Payment == EHexCardPayment::Refused)
			{
				if (bFaceBlockedDirection)
				{
					FaceDirection(Direction);
				}
				Cards->NotifyPaymentRefused(Board->GetTileAt(Record.PlayerTo));
				OnMoveBlocked.Broadcast(Direction, EHexMoveResult::BlockedNoCard);
				return EHexMoveResult::BlockedNoCard;
			}
			bPaid = (Payment == EHexCardPayment::Paid);
		}

		Board->CommitMove(Record, GetOwner(), GetPusherRadius());
		BeginStep(Record);
		bStepPaid = bPaid;
	}
	else
	{
		// Show *why* it failed: the box in the way gets a little shove.
		const FIntPoint Target = Coord + HexSokoban::DirectionOffset(Direction);
		if (AHexBox* BlockingBox = Board->GetBoxAt(Target))
		{
			BlockingBox->PlayBlockedNudge(Direction);
			Board->NotifyPushBlocked(Direction, Result);
			UHexCardSubsystem* Cards = GetCards();
			if (Cards && Cards->IsActive())
			{
				Cards->ReportBlockedMove(Board->GetTileAt(Target), Result);
			}
		}
		if (bFaceBlockedDirection)
		{
			FaceDirection(Direction);
		}
		OnMoveBlocked.Broadcast(Direction, Result);
	}
	return Result;
}

EHexMoveResult UHexGridMovementComponent::TryMoveTowardsLocation(const FVector& WorldLocation)
{
	if (const UHexCardSubsystem* Cards = GetCards())
	{
		const auto Hand = Cards->GetHand();
		const int32 Selected = Cards->GetSelectedCardIndex();
		if (Hand.IsValidIndex(Selected) && Hand[Selected].Kind == EHexCardKind::Skill)
		{
			AHexTile* Tile = GetBoard() ? GetBoard()->FindNearestTile(WorldLocation) : nullptr;
			return Tile ? TryPlaySkillAt(Tile->GetCoord()) : EHexMoveResult::Invalid;
		}
	}
	if (!bHasCoord)
	{
		return EHexMoveResult::Invalid;
	}
	// Measure from the (target) tile center, so clicks during a step are relative to where we will be.
	FVector Delta = WorldLocation - UHexGridLibrary::CoordToWorld(Coord);
	Delta.Z = 0.0;
	if (Delta.Size2D() < IgnoreClickRadius * UHexGridLibrary::GetTileSpacing())
	{
		return EHexMoveResult::Invalid;
	}
	return TryMove(HexSokoban::DirectionFromVector(Delta));
}

EHexMoveResult UHexGridMovementComponent::TryMoveByYaw(float WorldYaw)
{
	return TryMove(HexSokoban::DirectionFromYaw(WorldYaw));
}

void UHexGridMovementComponent::BeginStep(const FHexMoveRecord& Record)
{
	CurrentStep = Record;
	Coord = Record.PlayerTo;
	bMoving = true;
	StepElapsed = 0.f;
	StepStart = GetOwner()->GetActorLocation();
	StepTarget = GetStandLocation(Record.PlayerTo);
	if (Record.SkillRank == 11) StepTarget = StepStart; // Stationary cast, including visual position.

	if (!GetMovingCharacter())
	{
		FaceDirection(Record.Direction);
	}
	ApplyPushSpeed(Record.PushedBox != nullptr);
	if (Record.SkillRank == 11)
	{
		PlayWindBladeFX(Record);
	}
	else if (Record.SkillRank == 13)
	{
		StartFlightFX();
	}
	OnStepStarted.Broadcast(Record);
}

EHexMoveResult UHexGridMovementComponent::TryPlaySkillAt(FIntPoint Target)
{
	if (bLocked) return EHexMoveResult::Locked;
	UHexBoardSubsystem* Board = GetBoard();
	UHexCardSubsystem* Cards = GetCards();
	if (!Board || !Cards || !bHasCoord) return EHexMoveResult::Invalid;
	if (bMoving || Board->IsAnyBoxMoving()) return EHexMoveResult::Busy;
	const auto Hand = Cards->GetHand();
	const int32 Selected = Cards->GetSelectedCardIndex();
	FHexMoveRecord Record;
	if (!Hand.IsValidIndex(Selected) || !Board->EvaluateSkill(Coord, Target, Hand[Selected], Record))
	{
		Cards->NotifyPaymentRefused(Board->GetTileAt(Target));
		return EHexMoveResult::BlockedNoCard;
	}
	if (Cards->PayForStep(Board->GetTileAt(Target)) != EHexCardPayment::Paid) return EHexMoveResult::BlockedNoCard;
	Board->CommitMove(Record, GetOwner(), GetPusherRadius());
	BeginStep(Record);
	bStepPaid = true;
	if (Record.SkillRank == 12) BeginTeleportCast();	// blinks when the cast effect ends (see TickComponent)
	return Record.SkillBoxes.IsEmpty() ? EHexMoveResult::Moved : EHexMoveResult::Pushed;
}

void UHexGridMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bMoving)
	{
		return;
	}

	AActor* Owner = GetOwner();
	StepElapsed += DeltaTime;
	if (bTeleportPending)
	{
		UpdateTeleport(DeltaTime);
		return;
	}
	if (CurrentStep.SkillRank)
	{
		const float Duration = CurrentStep.SkillRank == 13
			? FMath::Clamp(static_cast<float>(FVector::Dist2D(StepStart,StepTarget) / 1700.0), .18f, .75f) : .24f;
		const float Alpha = FMath::Clamp(StepElapsed / Duration, 0.f, 1.f);
		const FVector Previous = Owner->GetActorLocation();
		Owner->SetActorLocation(FMath::Lerp(StepStart,StepTarget,static_cast<double>(FMath::SmoothStep(0.f,1.f,Alpha))));
		if (ACharacter* Character = GetMovingCharacter())
			Character->GetCharacterMovement()->Velocity = (Owner->GetActorLocation()-Previous) / FMath::Max(DeltaTime,.001f);
		FaceDirection(CurrentStep.Direction);
		if (Alpha >= 1.f && (!GetBoard() || !GetBoard()->IsAnyBoxMoving())) FinishStep(true);
		return;
	}

	if (ACharacter* Character = GetMovingCharacter())
	{
		const FVector Location = Owner->GetActorLocation();
		FVector ToTarget = StepTarget - Location;
		ToTarget.Z = 0.0;
		const FVector StepDirection = HexSokoban::DirectionVector(CurrentStep.Direction);
		const bool bPassedTarget = FVector::DotProduct(ToTarget, StepDirection) <= 0.0;

		if (ToTarget.Size2D() <= ArriveTolerance || bPassedTarget || StepElapsed >= StepTimeout)
		{
			FinishStep(true);
			return;
		}
		Character->AddMovementInput(ToTarget.GetSafeNormal2D(), 1.f, true);
	}
	else
	{
		const float Alpha = FMath::Clamp(StepElapsed / FMath::Max(InterpStepDuration, 0.01f), 0.f, 1.f);
		const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);
		Owner->SetActorLocation(FMath::Lerp(StepStart, StepTarget, static_cast<double>(Eased)));
		if (Alpha >= 1.f)
		{
			FinishStep(true);
		}
	}
}

void UHexGridMovementComponent::FinishStep(bool bNotify)
{
	if (!bMoving)
	{
		return;
	}
	bMoving = false;
	bTeleportPending = false;
	StopFlightFX();

	AActor* Owner = GetOwner();
	ACharacter* Character = GetMovingCharacter();

	// Land exactly on the tile center. Character keeps its own Z (it walked there on the floor).
	if (Character)
	{
		const FVector Location = Owner->GetActorLocation();
		Owner->SetActorLocation(FVector(StepTarget.X, StepTarget.Y, Location.Z), false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		Owner->SetActorLocation(StepTarget, false, nullptr, ETeleportType::TeleportPhysics);
	}

	const FHexMoveRecord Finished = CurrentStep;
	UHexBoardSubsystem* Board = GetBoard();

	// Hand the box over: it keeps the pawn's speed and slides to its tile.
	if (Finished.PushedBox)
	{
		const float ReleaseSpeed = Character
			? static_cast<float>(Character->GetVelocity().Size2D())
			: static_cast<float>(UHexGridLibrary::GetTileSpacing() / FMath::Max(InterpStepDuration, 0.01f));
		Finished.PushedBox->ReleasePush(ReleaseSpeed);
	}
	ApplyPushSpeed(false);

	if (bNotify)
	{
		if (Board)
		{
			if (AHexTile* Tile = Board->GetTileAt(Finished.PlayerTo))
			{
				if (LandFlashStrength > 0.f)
				{
					Tile->PlayFlash(LandFlashStrength);
				}
				Tile->OnPlayerEntered(Cast<APawn>(Owner));
			}
		}
		// Draw after walking (play -> walk -> draw).
		if (bStepPaid)
		{
			bStepPaid = false;
			if (UHexCardSubsystem* Cards = GetCards())
			{
				Cards->NotifyStepCompleted();
			}
		}
		OnStepFinished.Broadcast(Finished);
		if (Board)
		{
			Board->NotifyMoveFinished(Finished); // may lock us (level complete)
		}
	}
	bStepPaid = false;

	bool bContinued = false;
	const EHexDirection Next = BufferedDirection;
	BufferedDirection = EHexDirection::Invalid;
	if (bNotify && !bLocked && Next != EHexDirection::Invalid)
	{
		const EHexMoveResult Result = TryMove(Next);
		bContinued = (Result == EHexMoveResult::Moved || Result == EHexMoveResult::Pushed);
	}

	// Keep momentum when chaining steps, otherwise stop dead on the tile center.
	if (!bContinued && Character)
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}
}

void UHexGridMovementComponent::FinishStepImmediately(bool bNotify)
{
	if (!bMoving)
	{
		BufferedDirection = EHexDirection::Invalid;
		return;
	}
	BufferedDirection = EHexDirection::Invalid;
	const FHexMoveRecord Finished = CurrentStep;
	TeleportToCoord(Finished.PlayerTo); // clears bMoving
	if (bStepPaid && bNotify)
	{
		if (UHexCardSubsystem* Cards = GetCards())
		{
			Cards->NotifyStepCompleted();
		}
	}
	bStepPaid = false;

	if (bNotify)
	{
		OnStepFinished.Broadcast(Finished);
		if (UHexBoardSubsystem* Board = GetBoard())
		{
			Board->NotifyMoveFinished(Finished);
		}
	}
}

void UHexGridMovementComponent::FaceDirection(EHexDirection Direction)
{
	if (Direction == EHexDirection::Invalid)
	{
		return;
	}
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorRotation(FRotator(0.f, HexSokoban::DirectionYaw(Direction), 0.f));
	}
}

float UHexGridMovementComponent::GetPusherRadius() const
{
	if (PushContactRadius >= 0.f)
	{
		return PushContactRadius;
	}
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		return Character->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.75f;
	}
	FVector Origin, Extent;
	if (const AActor* Owner = GetOwner())
	{
		Owner->GetActorBounds(true, Origin, Extent);
		return static_cast<float>(FMath::Min(Extent.X, Extent.Y));
	}
	return 30.f;
}

void UHexGridMovementComponent::ApplyPushSpeed(bool bPushing)
{
	ACharacter* Character = GetMovingCharacter();
	if (!Character)
	{
		return;
	}
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (bPushing)
	{
		if (SavedMaxWalkSpeed < 0.f)
		{
			SavedMaxWalkSpeed = Movement->MaxWalkSpeed;
		}
		Movement->MaxWalkSpeed = SavedMaxWalkSpeed * PushSpeedMultiplier;
	}
	else if (SavedMaxWalkSpeed >= 0.f)
	{
		Movement->MaxWalkSpeed = SavedMaxWalkSpeed;
		SavedMaxWalkSpeed = -1.f;
	}
}

// ---------------------------------------------------------------- skill VFX

FVector UHexGridMovementComponent::GetCellSurface(const FIntPoint& InCoord) const
{
	const UHexBoardSubsystem* Board = GetBoard();
	if (const AHexTile* Tile = Board ? Board->GetTileAt(InCoord) : nullptr)
	{
		return Tile->GetSurfaceLocation();
	}
	return GetStandLocation(InCoord) - FVector(0.0, 0.0, HeightAboveSurface);
}

UParticleSystemComponent* UHexGridMovementComponent::SpawnTeleportFX(const FVector& Location) const
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	UParticleSystem* System = Settings.TeleportFX.IsNull() ? nullptr : Settings.TeleportFX.LoadSynchronous();
	if (!System || !GetWorld())
	{
		return nullptr;
	}
	return UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), System, FTransform(FRotator::ZeroRotator, Location, Settings.TeleportFXScale),
		true, EPSCPoolMethod::None, true);
}

void UHexGridMovementComponent::BeginTeleportCast()
{
	// The pawn stays on its tile (still "moving", so no input) for the whole blink - see UpdateTeleport.
	bTeleportPending = true;
	TeleportPhase = 1;
	TeleportPhaseTime = 0.f;
	TeleportWait = UHexSokobanSettings::Get().TeleportCastTime;
	TeleportStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	TeleportCastComponent = SpawnTeleportFX(GetCellSurface(CurrentStep.PlayerFrom));
	if (TeleportCastComponent)
	{
		TeleportCastComponent->OnSystemFinished.AddUniqueDynamic(this, &UHexGridMovementComponent::HandleTeleportCastFinished);
	}
	else
	{
		TeleportWait = 0.f;	// no effect configured: go straight to the vanish
	}
}

void UHexGridMovementComponent::HandleTeleportCastFinished(UParticleSystemComponent* FinishedComponent)
{
	if (bTeleportPending && (TeleportPhase == 1 || TeleportPhase == 3) && FinishedComponent == TeleportCastComponent)
	{
		TeleportWait = 0.f;	// advance on our own tick, not inside the particle callback
	}
}

void UHexGridMovementComponent::UpdateTeleport(float DeltaTime)
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	AActor* Owner = GetOwner();
	TeleportPhaseTime += DeltaTime;

	auto ReleaseCast = [this]()
	{
		if (TeleportCastComponent)
		{
			TeleportCastComponent->OnSystemFinished.RemoveDynamic(this, &UHexGridMovementComponent::HandleTeleportCastFinished);
			TeleportCastComponent = nullptr;
		}
	};

	switch (TeleportPhase)
	{
	case 1:	// cast on the start tile
		TeleportWait -= DeltaTime;
		if (TeleportWait <= 0.f)
		{
			ReleaseCast();
			TeleportPhase = 2;
			TeleportPhaseTime = 0.f;
		}
		break;

	case 2:	// blink out: a quick squash (anticipation), then a stretch into a thin vertical streak - gone
	{
		const float T = FMath::Clamp(TeleportPhaseTime / FMath::Max(Settings.TeleportVanishTime, 0.02f), 0.f, 1.f);
		if (T < 0.35f)
		{
			const float A = T / 0.35f;
			SetBlinkScale(1.f + 0.25f * A, 1.f - 0.25f * A);
		}
		else
		{
			const float B = (T - 0.35f) / 0.65f;
			const float B2 = B * B;
			SetBlinkScale(1.25f - 1.17f * B2, 0.75f + 1.25f * B2);
		}
		if (T >= 1.f)
		{
			if (Owner)
			{
				Owner->SetActorHiddenInGame(true);
				Owner->SetActorLocation(StepTarget, false, nullptr, ETeleportType::TeleportPhysics);
			}
			TeleportCastComponent = SpawnTeleportFX(GetCellSurface(CurrentStep.PlayerTo));
			TeleportWait = Settings.TeleportCastTime;
			if (TeleportCastComponent)
			{
				TeleportCastComponent->OnSystemFinished.AddUniqueDynamic(this, &UHexGridMovementComponent::HandleTeleportCastFinished);
			}
			else
			{
				TeleportWait = 0.f;
			}
			TeleportPhase = 3;
			TeleportPhaseTime = 0.f;
		}
		break;
	}

	case 3:	// cast on the target tile, pawn still invisible
		TeleportWait -= DeltaTime;
		if (TeleportWait <= 0.f)
		{
			ReleaseCast();
			SetBlinkScale(0.08f, 2.f);
			if (Owner)
			{
				Owner->SetActorHiddenInGame(false);
			}
			TeleportPhase = 4;
			TeleportPhaseTime = 0.f;
		}
		break;

	case 4:	// blink in: from the streak back to full size with an elastic wobble (overshoot = squash)
	{
		const float T = FMath::Clamp(TeleportPhaseTime / FMath::Max(Settings.TeleportAppearTime, 0.02f), 0.f, 1.f);
		const float E = 1.f - FMath::Exp(-6.f * T) * FMath::Cos(14.f * T);
		SetBlinkScale(0.08f + 0.92f * E, 2.f - E);
		if (T >= 1.f)
		{
			CompleteTeleport();
		}
		break;
	}

	default:
		CompleteTeleport();
		break;
	}
}

void UHexGridMovementComponent::SetBlinkScale(float ScaleXY, float ScaleZ)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USceneComponent* Visual = Character && Character->GetMesh() ? static_cast<USceneComponent*>(Character->GetMesh())
		: (GetOwner() ? GetOwner()->GetRootComponent() : nullptr);
	if (!Visual)
	{
		return;
	}
	if (!bBlinkScaled)
	{
		BlinkBaseScale = Visual->GetRelativeScale3D();
		bBlinkScaled = true;
	}
	Visual->SetRelativeScale3D(BlinkBaseScale * FVector(ScaleXY, ScaleXY, ScaleZ));
}

void UHexGridMovementComponent::ResetBlinkVisual()
{
	if (bBlinkScaled)
	{
		ACharacter* Character = Cast<ACharacter>(GetOwner());
		USceneComponent* Visual = Character && Character->GetMesh() ? static_cast<USceneComponent*>(Character->GetMesh())
			: (GetOwner() ? GetOwner()->GetRootComponent() : nullptr);
		if (Visual)
		{
			Visual->SetRelativeScale3D(BlinkBaseScale);
		}
		bBlinkScaled = false;
	}
	if (AActor* Owner = GetOwner())
	{
		if (Owner->IsHidden())
		{
			Owner->SetActorHiddenInGame(false);
		}
	}
	if (TeleportCastComponent)
	{
		TeleportCastComponent->OnSystemFinished.RemoveDynamic(this, &UHexGridMovementComponent::HandleTeleportCastFinished);
		TeleportCastComponent = nullptr;
	}
	TeleportPhase = 0;
}

void UHexGridMovementComponent::CompleteTeleport()
{
	UE_LOG(LogHexSokoban, Log, TEXT("Q teleport: done after %.2fs."), GetWorld() ? GetWorld()->GetTimeSeconds() - TeleportStartTime : 0.0);
	bTeleportPending = false;
	ResetBlinkVisual();
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorLocation(StepTarget, false, nullptr, ETeleportType::TeleportPhysics);
	}
	FinishStep(true);
}

void UHexGridMovementComponent::PlayWindBladeFX(const FHexMoveRecord& Record) const
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	UNiagaraSystem* System = Settings.WindBladeFX.IsNull() ? nullptr : Settings.WindBladeFX.LoadSynchronous();
	if (!System || !GetWorld() || Record.Direction == EHexDirection::Invalid)
	{
		return;
	}
	// The shared edge of the caster's tile and the (adjacent) box tile: halfway between the two centres.
	const FIntPoint BoxCell = Record.PlayerFrom + HexSokoban::DirectionOffset(Record.Direction);
	const FVector Location = (GetCellSurface(Record.PlayerFrom) + GetCellSurface(BoxCell)) * 0.5;
	const FRotator Rotation(0.f, HexSokoban::DirectionYaw(Record.Direction), 0.f);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, Location, Rotation, Settings.WindBladeFXScale,
		true, true, ENCPoolMethod::None, true);
}

void UHexGridMovementComponent::StartFlightFX()
{
	StopFlightFX();
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	UNiagaraSystem* System = Settings.FlightFX.IsNull() ? nullptr : Settings.FlightFX.LoadSynchronous();
	AActor* Owner = GetOwner();
	if (!System || !Owner || !Owner->GetRootComponent())
	{
		return;
	}
	// Rides on the player (turns with it) for the whole flight.
	FlightFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Owner->GetRootComponent(), NAME_None,
		FVector::ZeroVector, FRotator::ZeroRotator, Settings.FlightFXScale, EAttachLocation::KeepRelativeOffset,
		true, ENCPoolMethod::None, true, true);
}

void UHexGridMovementComponent::StopFlightFX()
{
	if (FlightFXComponent)
	{
		// The wind trail has long-lived particles: kill it on landing instead of letting it linger.
		FlightFXComponent->DeactivateImmediate();
		FlightFXComponent->DestroyComponent();
		FlightFXComponent = nullptr;
	}
}
