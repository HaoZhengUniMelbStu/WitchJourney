#include "HexCardLabelComponent.h"
#include "HexSokobanSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace HexLabel
{
	// Labels are nudged this far towards the camera, along the view ray (so they don't move on screen),
	// which puts them in front of the path FX lines / reticles drawn a few cm above the tiles.
	constexpr double DepthBias = 30.0;
}

UHexCardLabelComponent::UHexCardLabelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetMobility(EComponentMobility::Movable);
}

void UHexCardLabelComponent::BeginPlay()
{
	Super::BeginPlay();
	SetRelativeRotation(FRotator(0.f, UHexSokobanSettings::Get().LabelYawOffset, 0.f));
	EnsureVisuals();
	ApplyCardVisuals();
	ApplyTransform();
}

UStaticMeshComponent* UHexCardLabelComponent::MakeQuad(const FVector& Location, float Size)
{
	static const FSoftObjectPath PlanePath(TEXT("/Engine/BasicShapes/Plane.Plane"));
	UStaticMesh* Plane = Cast<UStaticMesh>(PlanePath.TryLoad());
	AActor* Owner = GetOwner();
	if (!Plane || !Owner)
	{
		return nullptr;
	}
	UStaticMeshComponent* Quad = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
	Quad->SetupAttachment(this);
	Quad->SetMobility(EComponentMobility::Movable);
	Quad->SetStaticMesh(Plane);
	Quad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Quad->SetCastShadow(false);
	// Card faces draw over everything except the character (M_HexCardGlyph: no depth test, masked by the
	// character's custom depth), and above the path lines.
	Quad->SetRenderCustomDepth(false);
	Quad->SetTranslucentSortPriority(200);
	const float MirrorSign = UHexSokobanSettings::Get().bLabelMirror ? -1.f : 1.f;
	Quad->SetRelativeLocation(Location);
	Quad->SetRelativeScale3D(FVector(Size / 100.f, MirrorSign * Size / 100.f, 1.f));
	Quad->RegisterComponent();
	return Quad;
}

void UHexCardLabelComponent::EnsureVisuals()
{
	if (bVisualsBuilt || !GetOwner())
	{
		return;
	}
	bVisualsBuilt = true;

	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	const float Size = FMath::Max(Settings.LabelSize, 1.f);
	UMaterialInterface* Material = Settings.CardGlyphMaterial.IsNull() ? nullptr : Settings.CardGlyphMaterial.LoadSynchronous();
	UTexture2D* Atlas = Settings.CardGlyphAtlas.IsNull() ? nullptr : Settings.CardGlyphAtlas.LoadSynchronous();

	if (Material && Atlas)
	{
		// Suit on the left, rank on the right (screen right = +Y with the default board camera).
		SuitQuad = MakeQuad(FVector(0.f, -0.25f * Size, 0.f), 0.5f * Size);
		RankQuad = MakeQuad(FVector(0.f, 0.25f * Size, 0.f), 0.5f * Size);
		if (SuitQuad)
		{
			SuitMID = UMaterialInstanceDynamic::Create(Material, this);
			SuitMID->SetTextureParameterValue(TEXT("Atlas"), Atlas);
			SuitQuad->SetMaterial(0, SuitMID);
		}
		if (RankQuad)
		{
			RankMID = UMaterialInstanceDynamic::Create(Material, this);
			RankMID->SetTextureParameterValue(TEXT("Atlas"), Atlas);
			RankQuad->SetMaterial(0, RankMID);
		}
	}
	else if (AActor* Owner = GetOwner())
	{
		// Fallback: plain text lying on the surface.
		FallbackText = NewObject<UTextRenderComponent>(Owner, NAME_None, RF_Transient);
		FallbackText->SetupAttachment(this);
		FallbackText->SetMobility(EComponentMobility::Movable);
		FallbackText->SetHorizontalAlignment(EHTA_Center);
		FallbackText->SetVerticalAlignment(EVRTA_TextCenter);
		FallbackText->SetWorldSize(0.45f * Size);
		FallbackText->SetRelativeRotation(FRotator(90.f, 180.f, 0.f));
		FallbackText->SetRelativeLocation(FVector(0.f, 0.f, 0.5f));
		FallbackText->RegisterComponent();
	}
}

void UHexCardLabelComponent::ApplyCardVisuals()
{
	if (!bHasCard)
	{
		return;
	}
	const FLinearColor Tint = Card.IsRed() ? RedColor : BlackColor;
	if (SuitMID)
	{
		SuitMID->SetScalarParameterValue(TEXT("Index"), static_cast<float>(UHexCardLibrary::GetSuitAtlasIndex(Card.Suit)));
		SuitMID->SetVectorParameterValue(TEXT("Tint"), Tint);
	}
	if (RankMID)
	{
		RankMID->SetScalarParameterValue(TEXT("Index"), static_cast<float>(UHexCardLibrary::GetRankAtlasIndex(Card.Rank)));
		RankMID->SetVectorParameterValue(TEXT("Tint"), Tint);
	}
	if (FallbackText)
	{
		static const TCHAR* SuitLetters[] = { TEXT("S"), TEXT("H"), TEXT("C"), TEXT("D") };
		FallbackText->SetText(FText::FromString(FString::Printf(TEXT("%s%s"), SuitLetters[static_cast<int32>(Card.Suit) & 3], *Card.GetRankText())));
		FallbackText->SetTextRenderColor(Tint.ToFColor(true));
	}
}

void UHexCardLabelComponent::SetBaseLocation(const FVector& InBaseLocation)
{
	BaseLocation = InBaseLocation;
	ApplyTransform();
}

void UHexCardLabelComponent::SetCard(const FHexCard& InCard)
{
	Card = InCard;
	bHasCard = true;
	EnsureVisuals();
	ApplyCardVisuals();
}

void UHexCardLabelComponent::ClearCard()
{
	bHasCard = false;
	HideLabel(false);
}

void UHexCardLabelComponent::ShowLabel(bool bAnimate, float Delay)
{
	if (!bHasCard)
	{
		return;
	}
	bWantShown = true;
	RiseTime = -1.f;
	RiseOffset = 0.f;
	TargetScale = 1.f;
	if (!bAnimate)
	{
		Scale = 1.f;
		ScaleVelocity = 0.f;
		ShowDelay = 0.f;
		bPendingKick = false;
		ApplyTransform();
		return;
	}
	Scale = FMath::Min(Scale, 0.05f);
	ShowDelay = FMath::Max(0.f, Delay);
	bPendingKick = true;
	SetComponentTickEnabled(true);
	ApplyTransform();
}

void UHexCardLabelComponent::HideLabel(bool bAnimate)
{
	bWantShown = false;
	TargetScale = 0.f;
	bPendingKick = false;
	if (!bAnimate)
	{
		Scale = 0.f;
		ScaleVelocity = 0.f;
		RiseTime = -1.f;
		RiseOffset = 0.f;
		ApplyTransform();
		return;
	}
	SetComponentTickEnabled(true);
}

void UHexCardLabelComponent::RiseAndHide(float InRiseHeight)
{
	if (!bHasCard || Scale <= 0.01f)
	{
		HideLabel(false);
		return;
	}
	bWantShown = false;
	RiseHeight = InRiseHeight;
	RiseTime = 0.f;
	SetComponentTickEnabled(true);
}

void UHexCardLabelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (RiseTime >= 0.f)
	{
		RiseTime += DeltaTime;
		const float Alpha = FMath::Clamp(RiseTime / FMath::Max(RiseDuration, 0.01f), 0.f, 1.f);
		RiseOffset = RiseHeight * (1.f - FMath::Square(1.f - Alpha));			// ease out
		Scale = 1.f + 0.35f * FMath::Sin(Alpha * PI);								// swell while rising
		if (Alpha >= 1.f)
		{
			RiseTime = -1.f;
			RiseOffset = 0.f;
			Scale = 0.f;
			ScaleVelocity = 0.f;
			TargetScale = 0.f;
		}
		ApplyTransform();
		return;
	}

	if (ShowDelay > 0.f)
	{
		ShowDelay -= DeltaTime;
		ApplyTransform();
		return;
	}
	if (bPendingKick)
	{
		bPendingKick = false;
		ScaleVelocity += PopKick;
	}

	float Remaining = FMath::Min(DeltaTime, 0.1f);
	while (Remaining > 0.f)
	{
		const float Step = FMath::Min(Remaining, 1.f / 60.f);
		ScaleVelocity += (-PopStiffness * (Scale - TargetScale) - PopDamping * ScaleVelocity) * Step;
		Scale += ScaleVelocity * Step;
		Remaining -= Step;
	}
	if (TargetScale <= 0.f && Scale < 0.f)
	{
		Scale = 0.f;
		ScaleVelocity = 0.f;
	}
	const bool bSettled = FMath::Abs(Scale - TargetScale) < 0.002f && FMath::Abs(ScaleVelocity) < 0.01f;
	if (bSettled)
	{
		Scale = TargetScale;
		ScaleVelocity = 0.f;
		if (TargetScale <= 0.f)
		{
			SetComponentTickEnabled(false);	// shown labels keep ticking so the view-ray nudge follows the camera
		}
	}
	ApplyTransform();
}

void UHexCardLabelComponent::ApplyTransform()
{
	const bool bDelayed = ShowDelay > 0.f;
	const float Shown = bDelayed ? 0.f : FMath::Max(Scale, 0.f);
	FVector Location = BaseLocation + FVector(0.f, 0.f, RiseOffset);
	float ViewScale = 1.f;
	const USceneComponent* Parent = GetAttachParent();
	const UWorld* World = GetWorld();
	const APlayerController* PC = World && World->IsGameWorld() ? World->GetFirstPlayerController() : nullptr;
	if (Parent && PC && PC->PlayerCameraManager)
	{
		// Slide along the view ray towards the camera and shrink to match: same spot and size on screen,
		// but drawn over the lines (the camera barely moves, so this is refreshed only with the label).
		const FVector WorldLocation = Parent->GetComponentTransform().TransformPosition(Location);
		const FVector ToCamera = PC->PlayerCameraManager->GetCameraLocation() - WorldLocation;
		const double Distance = ToCamera.Size();
		if (Distance > HexLabel::DepthBias * 4.0)
		{
			const FVector Offset = ToCamera / Distance * HexLabel::DepthBias;
			Location += Parent->GetComponentTransform().InverseTransformVector(Offset);
			ViewScale = static_cast<float>((Distance - HexLabel::DepthBias) / Distance);
		}
	}
	SetRelativeLocation(Location);
	SetRelativeScale3D(FVector(FMath::Max(Shown * ViewScale, 0.001f)));
	if (World && World->IsGameWorld() && Shown > 0.01f && !IsComponentTickEnabled())
	{
		SetComponentTickEnabled(true);	// keep following the camera (intro sequences move it after the label settled)
	}
	SetVisibility(bHasCard && Shown > 0.01f, true);
}
