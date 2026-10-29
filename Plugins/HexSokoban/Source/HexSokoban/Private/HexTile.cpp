#include "HexTile.h"
#include "HexGridLibrary.h"
#include "HexSokobanSettings.h"
#include "HexSokobanVisuals.h"
#include "HexCardLabelComponent.h"
#include "HexBoardSubsystem.h"
#include "HexBox.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

AHexTile::AHexTile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	TileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh"));
	SetRootComponent(TileMesh);
	TileMesh->SetMobility(EComponentMobility::Movable);	// guidance lift / wobble
	TileMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	CardLabel = CreateDefaultSubobject<UHexCardLabelComponent>(TEXT("CardLabel"));
	CardLabel->SetupAttachment(TileMesh);

	NormalColor = FLinearColor::FromSRGBColor(FColor(0, 150, 255));
	GoalColor = FLinearColor::FromSRGBColor(FColor(0, 150, 70));
}

void AHexTile::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyDefaultsAndVisuals();
}

void AHexTile::BeginPlay()
{
	Super::BeginPlay();
	ApplyDefaultsAndVisuals();
	if (TileMesh && TileMesh->Mobility != EComponentMobility::Movable)
	{
		TileMesh->SetMobility(EComponentMobility::Movable);	// levels saved before the guidance lift existed
	}
	HexSokoban::CollectDynamicMaterials(this, RuntimeMaterials, CardLabel);
	GuideBaseLocation = GetActorLocation();
	bHasGuideBase = true;
}

void AHexTile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bGuideValid = GuideState == EHexTileGuide::Valid || GuideState == EHexTileGuide::ValidHovered;

	// Glow: steady "enterable" highlight, stronger under the cursor.
	FlashValue = FMath::Max(0.f, FlashValue - FlashFadeSpeed * DeltaSeconds);
	HighlightValue = FMath::FInterpTo(HighlightValue, (bHighlighted || bGuideValid) ? 1.f : 0.f, DeltaSeconds, 10.f);
	HighlightTime += DeltaSeconds;
	const float GlowStrength = GuideState == EHexTileGuide::ValidHovered ? HoverGlowStrength : HighlightStrength;
	const float Glow = HighlightValue * GlowStrength * (0.75f + 0.25f * FMath::Sin(HighlightTime * 5.f));
	HexSokoban::SetScalarOnAll(RuntimeMaterials, FlashParameterName, FMath::Max(FlashValue, Glow));

	// Lift: a bouncy spring.
	const float TargetLift = GuideState == EHexTileGuide::ValidHovered ? GuideHoverLift : (GuideState == EHexTileGuide::Valid ? GuideLift : 0.f);
	float Remaining = DeltaSeconds;
	while (Remaining > 0.f)
	{
		const float Step = FMath::Min(Remaining, 1.f / 120.f);
		LiftVelocity += (LiftStiffness * (TargetLift - LiftValue) - LiftDamping * LiftVelocity) * Step;
		LiftValue += LiftVelocity * Step;
		Remaining -= Step;
	}

	// Dim.
	DimValue = FMath::FInterpTo(DimValue, GuideState == EHexTileGuide::Dimmed ? DimAmount : 0.f, DeltaSeconds, 12.f);
	HexSokoban::SetScalarOnAll(RuntimeMaterials, DimParameterName, DimValue);

	// Red: faint while hovered-invalid, three blinks + wobble after a refused drop.
	float Error = GuideState == EHexTileGuide::InvalidHovered ? InvalidHoverTint : 0.f;
	double Shake = 0.0;
	if (ErrorTime >= 0.f)
	{
		ErrorTime += DeltaSeconds;
		const float T = ErrorTime / ErrorDuration;
		if (T >= 1.f)
		{
			ErrorTime = -1.f;
		}
		else
		{
			const float Fade = 1.f - T;
			Error = FMath::Max(Error, FMath::Abs(FMath::Sin(T * PI * 3.f)) * (0.35f + 0.65f * Fade));
			Shake = FMath::Sin(ErrorTime * 55.f) * ErrorShake * Fade;
		}
	}
	HexSokoban::SetScalarOnAll(RuntimeMaterials, ErrorParameterName, Error);

	if (bHasGuideBase)
	{
		SetActorLocation(GuideBaseLocation + FVector(0.0, Shake, static_cast<double>(LiftValue)));
	}
	SyncBoxLift();

	const bool bLiftAtRest = FMath::IsNearlyEqual(LiftValue, TargetLift, 0.02f) && FMath::Abs(LiftVelocity) < 0.05f;
	const bool bDimAtRest = FMath::IsNearlyEqual(DimValue, GuideState == EHexTileGuide::Dimmed ? DimAmount : 0.f, 0.002f);
	if (FlashValue <= 0.f && !bHighlighted && !bGuideValid && HighlightValue < 0.01f
		&& bLiftAtRest && bDimAtRest && ErrorTime < 0.f && GuideState != EHexTileGuide::InvalidHovered)
	{
		HighlightValue = 0.f;
		LiftValue = TargetLift;
		LiftVelocity = 0.f;
		HexSokoban::SetScalarOnAll(RuntimeMaterials, FlashParameterName, 0.f);
		HexSokoban::SetScalarOnAll(RuntimeMaterials, ErrorParameterName, 0.f);
		if (bHasGuideBase)
		{
			SetActorLocation(GuideBaseLocation + FVector(0.0, 0.0, static_cast<double>(LiftValue)));
		}
		SyncBoxLift();
		SetActorTickEnabled(false);
	}
}

void AHexTile::SyncBoxLift()
{
	if (FMath::IsNearlyEqual(LastBoxLift, LiftValue, 0.01f))
	{
		return;
	}
	LastBoxLift = LiftValue;
	const UWorld* World = GetWorld();
	const UHexBoardSubsystem* Board = World ? World->GetSubsystem<UHexBoardSubsystem>() : nullptr;
	if (AHexBox* Box = Board ? Board->GetBoxAt(Coord) : nullptr)
	{
		if (!Box->IsMoving())
		{
			Box->SetGuideLift(LiftValue);
		}
	}
}

void AHexTile::SetGuideState(EHexTileGuide NewState, bool bSnap)
{
	if (bSnap)
	{
		LiftValue = 0.f;
		LiftVelocity = 0.f;
		DimValue = 0.f;
		if (bHasGuideBase)
		{
			SetActorLocation(GuideBaseLocation);
		}
		SyncBoxLift();
	}
	if (GuideState == NewState)
	{
		if (bSnap)
		{
			SetActorTickEnabled(true);
		}
		return;
	}
	const bool bWasValid = GuideState == EHexTileGuide::Valid || GuideState == EHexTileGuide::ValidHovered;
	GuideState = NewState;
	const bool bNowValid = GuideState == EHexTileGuide::Valid || GuideState == EHexTileGuide::ValidHovered;
	if (bNowValid && !bWasValid)
	{
		LiftVelocity += 60.f;	// little hop so it reads as "pick me"
	}
	SetActorTickEnabled(true);
	OnGuideStateChanged(GuideState);
}

void AHexTile::PlayInvalidFeedback()
{
	ErrorTime = 0.f;
	LiftVelocity -= 90.f;	// knocked down a bit, then bounces back
	SetActorTickEnabled(true);
	OnInvalidFeedback();
}

float AHexTile::GetLocalTopZ() const
{
	const UStaticMesh* Mesh = TileMesh ? TileMesh->GetStaticMesh() : nullptr;
	return Mesh ? static_cast<float>(Mesh->GetBoundingBox().Max.Z) : 0.f;
}

void AHexTile::SetCardLabel(const FHexCard& InLabel)
{
	if (bScenery) { ClearCardLabel(); return; }
	CardLabelData = InLabel;
	bHasCardLabel = true;
	if (CardLabel)
	{
		CardLabel->SetCard(InLabel);
		if (!bLabelCovered)
		{
			// Staggered reveal across the board.
			const float Delay = 0.03f * static_cast<float>((Coord.X * 7 + Coord.Y * 13) & 7);
			CardLabel->ShowLabel(true, Delay);
		}
		else
		{
			CardLabel->HideLabel(false);
		}
	}
	OnCardLabelAssigned(InLabel);
}

void AHexTile::ClearCardLabel()
{
	bHasCardLabel = false;
	if (CardLabel)
	{
		CardLabel->ClearCard();
	}
}

void AHexTile::SetLabelCovered(bool bCovered, bool bAnimate, float RiseHeight)
{
	bLabelCovered = bCovered;
	if (!CardLabel || !bHasCardLabel)
	{
		return;
	}
	if (bCovered)
	{
		if (bAnimate)
		{
			CardLabel->RiseAndHide(RiseHeight);
		}
		else
		{
			CardLabel->HideLabel(false);
		}
	}
	else
	{
		CardLabel->ShowLabel(bAnimate);
	}
}

void AHexTile::SetCardHighlight(bool bOn)
{
	if (bHighlighted == bOn)
	{
		return;
	}
	bHighlighted = bOn;
	SetActorTickEnabled(true);
	OnCardHighlightChanged(bOn);
}

#if WITH_EDITOR
void AHexTile::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	if (bFinished)
	{
		SnapToGridNow();
	}
}
#endif

FVector AHexTile::GetSurfaceLocation() const
{
	FVector Location = GetActorLocation();
	if (TileMesh && TileMesh->GetStaticMesh())
	{
		Location.Z = TileMesh->Bounds.GetBox().Max.Z;
	}
	if (bHasGuideBase)
	{
		// Logic always uses the resting position (guidance lift / wobble is visual only).
		Location.X = GuideBaseLocation.X;
		Location.Y = GuideBaseLocation.Y;
		Location.Z -= static_cast<double>(GetActorLocation().Z - GuideBaseLocation.Z);
	}
	Location.Z += StandHeightOffset;
	return Location;
}

void AHexTile::AddFeedbackMaterial(UMaterialInstanceDynamic* Material)
{
	if (Material)
	{
		RuntimeMaterials.AddUnique(Material);
	}
}

void AHexTile::PlayFlash(float Strength)
{
	FlashValue = FMath::Max(FlashValue, Strength);
	HexSokoban::SetScalarOnAll(RuntimeMaterials, FlashParameterName, FlashValue);
	SetActorTickEnabled(true);
}

void AHexTile::ApplyDefaultsAndVisuals()
{
	if (!TileMesh)
	{
		return;
	}
	if (!TileMesh->GetStaticMesh())
	{
		if (UStaticMesh* Mesh = UHexSokobanSettings::ResolveHexMesh())
		{
			TileMesh->SetStaticMesh(Mesh);
		}
	}
	HexSokoban::ApplyTint(TileMesh, bApplyTintColor, UHexSokobanSettings::ResolveColorMaterial(TintMaterial),
		ColorParameterName, IsGoal() ? GoalColor : NormalColor, this, TintMaterials);

	if (CardLabel)
	{
		CardLabel->SetBaseLocation(FVector(0.f, 0.f, GetLocalTopZ() + 0.6f + StandHeightOffset));
	}
}

void AHexTile::SnapToGridNow()
{
	if (!bSnapToGrid)
	{
		return;
	}
	const FVector EditorLocation = GetActorLocation();
	const FVector Snapped = UHexGridLibrary::SnapToGrid(EditorLocation);
	if (!Snapped.Equals(EditorLocation, 0.01))
	{
		SetActorLocation(Snapped);
	}
}
