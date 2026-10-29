#include "HexBox.h"
#include "HexAudioSubsystem.h"
#include "HexSokobanMath.h"
#include "HexSokobanVisuals.h"
#include "HexTile.h"
#include "HexCardLabelComponent.h"
#include "HexGridLibrary.h"
#include "HexSokobanSettings.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	constexpr float MaxSpringStep = 1.f / 30.f;
	constexpr float CarriedTimeout = 3.f;
}

AHexBox::AHexBox()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(Root);
	VisualRoot->SetMobility(EComponentMobility::Movable);

	BoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoxMesh"));
	BoxMesh->SetupAttachment(VisualRoot);
	BoxMesh->SetMobility(EComponentMobility::Movable);
	BoxMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.5f));
	BoxMesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	// Grid logic decides who blocks whom; the capsule must never get stuck on a box mid-step.
	BoxMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	BoxMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	CardLabel = CreateDefaultSubobject<UHexCardLabelComponent>(TEXT("CardLabel"));
	CardLabel->SetupAttachment(VisualRoot);

	BoxColor = FLinearColor::FromSRGBColor(FColor(100, 104, 128));
	OnGoalColor = FLinearColor::FromSRGBColor(FColor(255, 176, 20));
}

// ---------------------------------------------------------------- setup

void AHexBox::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyDefaultsAndVisuals();
}

void AHexBox::BeginPlay()
{
	Super::BeginPlay();
	ApplyDefaultsAndVisuals();
	HexSokoban::CollectDynamicMaterials(this, RuntimeMaterials, CardLabel);
	ApplyColors();	// your own materials get the current state colour from the start
	CacheGeometry();
	HexSokoban::MarkGuideOccluder(this, HexSokoban::StencilBlocker, CardLabel);	// boxes hide path lines
}

#if WITH_EDITOR
void AHexBox::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	if (bFinished && bSnapToGrid)
	{
		const FVector EditorLocation = GetActorLocation();
		const FVector Snapped = UHexGridLibrary::SnapToGrid(EditorLocation);
		if (!Snapped.Equals(EditorLocation, 0.01))
		{
			SetActorLocation(Snapped);
		}
	}
}
#endif

void AHexBox::ApplyDefaultsAndVisuals()
{
	if (!BoxMesh)
	{
		return;
	}
	if (!BoxMesh->GetStaticMesh())
	{
		if (UStaticMesh* Mesh = UHexSokobanSettings::ResolveHexMesh())
		{
			BoxMesh->SetStaticMesh(Mesh);
		}
	}
	HexSokoban::ApplyTint(BoxMesh, bApplyTintColor, UHexSokobanSettings::ResolveColorMaterial(TintMaterial),
		ColorParameterName, bVisualOnGoal ? OnGoalColor : BoxColor, this, TintMaterials);
}

void AHexBox::ApplyColors()
{
	// Colour replacement only on the prototype tint materials. Your own materials keep their colours and get
	// state scalars instead: Flash (landing pulse) and GoalAmount (0..1, how "on a goal" the box looks).
	HexSokoban::SetVectorOnAll(TintMaterials, ColorParameterName, bVisualOnGoal ? OnGoalColor : BoxColor);
	HexSokoban::SetScalarOnAll(RuntimeMaterials, FlashParameterName, GoalFlash);
	HexSokoban::SetScalarOnAll(RuntimeMaterials, GoalParameterName, GoalAmount);
}

void AHexBox::CacheGeometry()
{
	if (bGeometryCached || !VisualRoot)
	{
		return;
	}
	VisualBaseScale = VisualRoot->GetRelativeScale3D();
	VisualBaseLocation = VisualRoot->GetRelativeLocation();

	// Measure everything on the actor (your Blueprint components included, the card label excluded).
	FBox Bounds(ForceInit);
	ForEachComponent<UPrimitiveComponent>(false, [this, &Bounds](const UPrimitiveComponent* Primitive)
	{
		if (Primitive->IsRegistered() && !(CardLabel && Primitive->IsAttachedTo(CardLabel)))
		{
			Bounds += Primitive->Bounds.GetBox();
		}
	});
	if (Bounds.IsValid)
	{
		BottomOffset = GetActorLocation().Z - Bounds.Min.Z;
		VisualBottomDelta = Bounds.Min.Z - VisualRoot->GetComponentLocation().Z;
		const FVector Extent = Bounds.GetExtent();
		BoxInradius = static_cast<float>(FMath::Min(Extent.X, Extent.Y));
		BoxHeight = static_cast<float>(Bounds.GetSize().Z);
		if (CardLabel)
		{
			const double RootScaleZ = FMath::Max(1.0e-4, FMath::Abs(Root->GetComponentScale().Z));
			const double TopLocal = (Bounds.Max.Z - VisualRoot->GetComponentLocation().Z) / RootScaleZ;
			CardLabel->SetBaseLocation(FVector(0.0, 0.0, TopLocal + 0.6));
		}
	}
	bGeometryCached = true;
}

FVector AHexBox::GetStandLocation(const AHexTile* Tile, const FIntPoint& InCoord) const
{
	const FVector Surface = Tile ? Tile->GetSurfaceLocation()
		: UHexGridLibrary::CoordToWorld(InCoord, static_cast<float>(GetActorLocation().Z - BottomOffset));
	return FVector(Surface.X, Surface.Y, Surface.Z + BottomOffset);
}

// ---------------------------------------------------------------- board API

bool AHexBox::IsMoving() const
{
	return Phase != EPushPhase::Idle || bHasPending;
}

void AHexBox::PlaceOnTile(const FIntPoint& InCoord, AHexTile* Tile)
{
	CacheGeometry();
	GuideLift = 0.f;
	Phase = EPushPhase::Idle;
	bHasPending = false;
	Coord = InCoord;
	if (AHexTile* Previous = TargetTile.Get())
	{
		if (Previous != Tile)
		{
			Previous->SetLabelCovered(false, false);
		}
	}
	TargetTile = Tile;
	Position = 0.0;
	SetActorLocation(GetStandLocation(Tile, InCoord), false, nullptr, ETeleportType::TeleportPhysics);
	ApplyVisualGoalState(bOnGoal, false);
	RefreshCardLabel(false);
}

void AHexBox::RefreshCardLabel(bool bAnimate)
{
	if (!CardLabel)
	{
		return;
	}
	AHexTile* Tile = TargetTile.Get();
	if (Tile && Tile->HasCardLabel())
	{
		CardLabel->SetCard(Tile->GetCardLabel());
		if (bAnimate)
		{
			CardLabel->HideLabel(false);
			CardLabel->ShowLabel(true, 0.1f);			// pops onto the box as the tile label rises into it
		}
		else
		{
			CardLabel->ShowLabel(false);
		}
		Tile->SetLabelCovered(true, bAnimate, BoxHeight);
	}
	else
	{
		CardLabel->HideLabel(bAnimate);
	}
}

void AHexBox::RevealFromTileIfClear(FPushSegment& Segment, double Distance)
{
	// Once the box is half way off its old tile, that tile's label pops back up.
	if (!Segment.bFromRevealed && Distance >= 0.5 * Segment.Length)
	{
		Segment.bFromRevealed = true;
		if (AHexTile* From = Segment.FromTile.Get())
		{
			From->SetLabelCovered(false, true);
		}
	}
}

void AHexBox::BeginPush(const FIntPoint& InCoord, AHexTile* Tile, EHexDirection Direction, AActor* Pusher, float PusherRadius)
{
	CacheGeometry();
	GuideLift = 0.f;
	Coord = InCoord;

	// A box can only be carried once at a time; collapse any backlog first.
	if (Phase == EPushPhase::Carried || (Phase == EPushPhase::Sliding && bHasPending))
	{
		FinishMoveImmediately();
	}

	FPushSegment Segment;
	Segment.Start = (Phase == EPushPhase::Idle) ? GetActorLocation() : Current.End;
	Segment.End = GetStandLocation(Tile, InCoord);
	FVector Delta = Segment.End - Segment.Start;
	Delta.Z = 0.0;
	Segment.Length = Delta.Size();
	Segment.Dir = Segment.Length > KINDA_SMALL_NUMBER ? Delta / Segment.Length : HexSokoban::DirectionVector(Direction);
	Segment.ContactDistance = BoxInradius + PusherRadius + ContactPadding;
	Segment.Direction = Direction;
	Segment.Pusher = Pusher;
	Segment.Tile = Tile;
	Segment.FromTile = (Phase == EPushPhase::Idle) ? TargetTile : Current.Tile;

	if (Phase == EPushPhase::Idle)
	{
		StartSegment(Segment);
	}
	else
	{
		// Still sliding from the previous push: queue this one, it takes over on landing.
		Pending = Segment;
		bHasPending = true;
	}
}

void AHexBox::ReleasePush(float PusherSpeed)
{
	if (bHasPending)
	{
		Pending.bReleased = true;
		Pending.ReleaseSpeed = PusherSpeed;
		return;
	}
	if (Phase == EPushPhase::Carried)
	{
		UpdateCarried();
		if (Phase == EPushPhase::Carried)
		{
			StartSlide(PusherSpeed);
		}
	}
}

void AHexBox::BeginSkillPush(const FIntPoint& InCoord, AHexTile* Tile, float Delay, bool bRebound)
{
	CacheGeometry();
	GuideLift = 0.f;
	if (IsMoving()) FinishMoveImmediately();
	Current = FPushSegment();
	Current.Start = GetActorLocation();
	Current.End = GetStandLocation(Tile, InCoord);
	Current.Tile = Tile;
	Current.FromTile = TargetTile;
	Current.Length = FVector::Dist2D(Current.Start, Current.End);
	Current.Dir = (Current.End - Current.Start).GetSafeNormal2D();
	Coord = InCoord;
	SkillDelay = Delay;
	bSkillRebound = bRebound;
	SlideTime = 0.f;
	Phase = EPushPhase::Skill;
	SetActorTickEnabled(true);
}

void AHexBox::SetOnGoal(bool bInOnGoal)
{
	bOnGoal = bInOnGoal;
	if (!IsMoving())
	{
		ApplyVisualGoalState(bOnGoal, false);
	}
}

void AHexBox::FinishMoveImmediately()
{
	if (!IsMoving())
	{
		return;
	}
	for (AHexTile* Destination : { Current.Tile.Get(), bHasPending ? Pending.Tile.Get() : nullptr })
    {
        if (Destination && Destination->IsGoal()) UHexAudioSubsystem::PlayExact(this, TEXT("Coins_05"));
    }
    const FPushSegment& Final = bHasPending ? Pending : Current;
	const FVector FinalLocation = Final.End;
	for (AHexTile* Passed : { Current.FromTile.Get(), bHasPending ? Pending.FromTile.Get() : nullptr })
	{
		if (Passed && Passed != Final.Tile.Get())
		{
			Passed->SetLabelCovered(false, false);
		}
	}
	TargetTile = Final.Tile;
	bHasPending = false;
	Phase = EPushPhase::Idle;
	Position = Current.Length;
	SetActorLocation(FinalLocation, false, nullptr, ETeleportType::TeleportPhysics);
	ApplyVisualGoalState(bOnGoal, false);
	RefreshCardLabel(false);
}

void AHexBox::PlayBlockedNudge(EHexDirection Direction)
{
	if (Direction == EHexDirection::Invalid)
	{
		return;
	}
	NudgeDir = HexSokoban::DirectionVector(Direction);
	NudgeVelocity += BlockedNudgeImpulse;
	Kick(0.6f);
	OnBlocked(Direction);
}

// ---------------------------------------------------------------- push motion

void AHexBox::StartSegment(const FPushSegment& Segment)
{
	Current = Segment;
	Position = 0.0;
	bContactMade = false;
	Phase = EPushPhase::Carried;
	SlideTime = 0.0;
	TargetTile = Segment.Tile;
	SetActorTickEnabled(true);

	if (Segment.bReleased)
	{
		// The pusher already finished its step while we were still sliding before.
		UpdateCarried();
		if (Phase == EPushPhase::Carried)
		{
			StartSlide(Segment.ReleaseSpeed);
		}
	}
}

void AHexBox::UpdateCarried()
{
	const AActor* Pusher = Current.Pusher.Get();
	if (!Pusher)
	{
		StartSlide(SlideMinSpeed);
		return;
	}

	FVector FromStart = Pusher->GetActorLocation() - Current.Start;
	FromStart.Z = 0.0;
	const double PusherAlong = FVector::DotProduct(FromStart, Current.Dir);
	const double Desired = FMath::Clamp(PusherAlong + Current.ContactDistance, 0.0, Current.Length);

	if (Desired > Position)
	{
		if (!bContactMade)
		{
			bContactMade = true;
			Kick(ContactSquashImpulse);
			OnPushed(Current.Direction);
		}
		SetPositionAlongSegment(Desired);
	}
}

void AHexBox::StartSlide(float Speed)
{
	if (!bContactMade)
	{
		bContactMade = true;
		Kick(ContactSquashImpulse);
		OnPushed(Current.Direction);
	}

	const double Remaining = Current.Length - Position;
	if (Remaining <= 0.5)
	{
		SetPositionAlongSegment(Current.Length);
		Land();
		return;
	}

	// Constant deceleration that stops exactly on the target.
	const double StartSpeed = FMath::Max(static_cast<double>(Speed), static_cast<double>(SlideMinSpeed));
	SlideDuration = FMath::Min(2.0 * Remaining / StartSpeed, static_cast<double>(SlideMaxDuration));
	SlideSpeed = 2.0 * Remaining / SlideDuration;
	SlideFrom = Position;
	SlideTime = 0.0;
	Phase = EPushPhase::Sliding;
	SetActorTickEnabled(true);
}

void AHexBox::SetPositionAlongSegment(double Distance)
{
	Position = FMath::Clamp(Distance, 0.0, Current.Length);
	RevealFromTileIfClear(Current, Position);
	const double Alpha = Current.Length > KINDA_SMALL_NUMBER ? Position / Current.Length : 1.0;
	SetActorLocation(FMath::Lerp(Current.Start, Current.End, Alpha));
}

void AHexBox::Land()
{
	SetActorLocation(Current.End, false, nullptr, ETeleportType::TeleportPhysics);
	Phase = EPushPhase::Idle;
	Kick(LandSquashImpulse);
	OnLanded();

	// Intermediate landing (another push queued): show this tile's goal state, not the final one.
	AHexTile* LandedTile = Current.Tile.Get();
	const bool bShowGoal = bHasPending ? (LandedTile && LandedTile->IsGoal()) : bOnGoal;
	ApplyVisualGoalState(bShowGoal, true);
	if (LandedTile)
	{
		if (LandedTile->IsGoal()) UHexAudioSubsystem::PlayExact(this, TEXT("Coins_05"));
		LandedTile->OnBoxLanded(this);
	}
	TargetTile = LandedTile;
	RefreshCardLabel(true);

	if (bHasPending)
	{
		bHasPending = false;
		const FPushSegment Next = Pending;
		StartSegment(Next);
	}
}

void AHexBox::ApplyVisualGoalState(bool bShowOnGoal, bool bWithEffects)
{
	if (bShowOnGoal == bVisualOnGoal)
	{
		return;
	}
	bVisualOnGoal = bShowOnGoal;
	if (bWithEffects)
	{
		SetActorTickEnabled(true);	// GoalAmount springs to its new value
	}
	else
	{
		GoalAmount = bVisualOnGoal ? 1.f : 0.f;	// placed / undone: no animation
		GoalAmountVelocity = 0.f;
	}
	if (bWithEffects && bVisualOnGoal)
	{
		Kick(-GoalPopImpulse); // stretch up first: a "pop"
		GoalFlash = 0.9f;
		if (AHexTile* Tile = TargetTile.Get())
		{
			Tile->PlayFlash(0.9f);
		}
		SetActorTickEnabled(true);
	}
	ApplyColors();
	OnGoalStateChanged(bVisualOnGoal);
}

// ---------------------------------------------------------------- tick & springs

void AHexBox::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	switch (Phase)
	{
	case EPushPhase::Skill:
	{
		SlideTime += DeltaSeconds;
		const double Alpha = FMath::Clamp((SlideTime - SkillDelay) / .28, 0.0, 1.0);
		if (Alpha > 0.0)
		{
			const double Travel = bSkillRebound
				? (Alpha < .65 ? 1.18 * FMath::Sin(Alpha / .65 * HALF_PI) : FMath::Lerp(1.18, 1.0, (Alpha-.65)/.35))
				: 1.0 - FMath::Square(1.0 - Alpha);
			RevealFromTileIfClear(Current, Current.Length * Travel);
			SetActorLocation(FMath::Lerp(Current.Start, Current.End, Travel));
		}
		if (Alpha >= 1.0) Land();
		break;
	}
	case EPushPhase::Carried:
		SlideTime += DeltaSeconds; // reused as a safety timer while carried
		UpdateCarried();
		if (Phase == EPushPhase::Carried && SlideTime > CarriedTimeout)
		{
			StartSlide(SlideMinSpeed);
		}
		break;

	case EPushPhase::Sliding:
	{
		SlideTime += DeltaSeconds;
		if (SlideTime >= SlideDuration)
		{
			SetPositionAlongSegment(Current.Length);
			Land();
		}
		else
		{
			const double Decel = SlideSpeed / SlideDuration;
			SetPositionAlongSegment(SlideFrom + SlideSpeed * SlideTime - 0.5 * Decel * SlideTime * SlideTime);
		}
		break;
	}

	default:
		break;
	}

	UpdateSprings(DeltaSeconds);

	// GoalAmount: a soft spring with a little overshoot (brightens a touch past its rest value, settles).
	const float GoalTarget = bVisualOnGoal ? 1.f : 0.f;
	const bool bGoalAtRest = FMath::IsNearlyEqual(GoalAmount, GoalTarget, 0.002f) && FMath::Abs(GoalAmountVelocity) < 0.01f;
	if (!bGoalAtRest)
	{
		float Remaining = DeltaSeconds;
		while (Remaining > 0.f)
		{
			const float Step = FMath::Min(Remaining, 1.f / 120.f);
			GoalAmountVelocity += (GoalStiffness * (GoalTarget - GoalAmount) - GoalDamping * GoalAmountVelocity) * Step;
			GoalAmount += GoalAmountVelocity * Step;
			Remaining -= Step;
		}
	}
	else if (GoalAmount != GoalTarget)
	{
		GoalAmount = GoalTarget;
		GoalAmountVelocity = 0.f;
	}

	const bool bWasFlashing = GoalFlash > 0.f;
	if (bWasFlashing)
	{
		GoalFlash = FMath::Max(0.f, GoalFlash - 2.f * DeltaSeconds);
	}
	if (bWasFlashing || !bGoalAtRest)
	{
		ApplyColors();
	}

	if (!IsMoving() && SpringsAtRest() && GoalFlash <= 0.f && bGoalAtRest)
	{
		SetActorTickEnabled(false);
	}
}

void AHexBox::Kick(float Impulse)
{
	SquashVelocity += Impulse;
	SetActorTickEnabled(true);
}

void AHexBox::UpdateSprings(float DeltaSeconds)
{
	if (!VisualRoot || !bGeometryCached)
	{
		return;
	}

	float Remaining = DeltaSeconds;
	while (Remaining > 0.f)
	{
		const float Step = FMath::Min(Remaining, MaxSpringStep);
		SquashVelocity += (-SpringStiffness * Squash - SpringDamping * SquashVelocity) * Step;
		Squash += SquashVelocity * Step;
		NudgeVelocity += (-SpringStiffness * Nudge - SpringDamping * NudgeVelocity) * Step;
		Nudge += NudgeVelocity * Step;
		Remaining -= Step;
	}

	if (SpringsAtRest())
	{
		Squash = SquashVelocity = Nudge = NudgeVelocity = 0.f;
	}

	const float Clamped = FMath::Clamp(Squash, -0.35f, 0.35f);
	VisualRoot->SetRelativeScale3D(VisualBaseScale * FVector(1.f + 0.5f * Clamped, 1.f + 0.5f * Clamped, 1.f - Clamped));

	// Keep the bottom planted while squashing (any pivot), and add the sideways shove.
	const FTransform& RootTransform = Root->GetComponentTransform();
	const double RootScaleZ = FMath::Max(1.0e-4, FMath::Abs(RootTransform.GetScale3D().Z));
	const FVector PivotFix(0.0, 0.0, VisualBottomDelta * Clamped / RootScaleZ);
	const FVector LocalNudge = RootTransform.InverseTransformVectorNoScale(NudgeDir * Nudge);
	const FVector Lift(0.0, 0.0, static_cast<double>(GuideLift) / RootScaleZ);
	VisualRoot->SetRelativeLocation(VisualBaseLocation + PivotFix + LocalNudge + Lift);
}

void AHexBox::SetGuideLift(float WorldLift)
{
	GuideLift = WorldLift;
	UpdateSprings(0.f);
}

bool AHexBox::SpringsAtRest() const
{
	return FMath::Abs(Squash) < 1e-3f && FMath::Abs(SquashVelocity) < 1e-2f
		&& FMath::Abs(Nudge) < 0.05f && FMath::Abs(NudgeVelocity) < 0.5f;
}
