#include "HexLevelEditor.h"
#include "HexLocalization.h"
#include "Engine/StaticMeshActor.h"
#include "HexSceneryComponent.h"
#include "HexEditorWidget.h"
#include "HexGameFlowSubsystem.h"
#include "HexSokobanSettings.h"
#include "HexSokobanTypes.h"
#include "HexGridLibrary.h"
#include "HexTile.h"
#include "HexGoalTile.h"
#include "HexLevelTools.h"
#include "HexBox.h"
#include "HexCardLabelComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "UObject/UnrealType.h"

namespace HexEditor
{
	const FLinearColor GhostValid(0.15f, 1.f, 0.4f, 1.f);
	const FLinearColor GhostInvalid(1.f, 0.12f, 0.08f, 1.f);
	const FLinearColor GhostFree(0.6f, 0.85f, 1.f, 1.f);

	FProperty* FindProp(UObject* Object, const TCHAR* Name)
	{
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			if (It->GetName().TrimStartAndEnd().Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Path Tracer DrawPath(PathPoints) by reflection (same as the path FX). */
	void DrawPath(AActor* Actor, const TArray<FVector>& Points)
	{
		UFunction* Function = Actor ? Actor->FindFunction(TEXT("DrawPath")) : nullptr;
		if (!Function)
		{
			return;
		}
		uint8* Params = static_cast<uint8*>(FMemory::Malloc(FMath::Max<int32>(1, Function->ParmsSize), Function->GetMinAlignment()));
		FMemory::Memzero(Params, Function->ParmsSize);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Params);
		}
		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Function->FindPropertyByName(TEXT("PathPoints"))))
		{
			*ArrayProp->ContainerPtrToValuePtr<TArray<FVector>>(Params) = Points;
			Actor->ProcessEvent(Function, Params);
		}
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Params);
		}
		FMemory::Free(Params);
	}

	TArray<FVector> HexRing(const FVector& Center, double Radius)
	{
		TArray<FVector> Points;
		for (int32 Corner = 0; Corner < 6; ++Corner)
		{
			const double Angle = FMath::DegreesToRadians(30.0 + 60.0 * Corner);
			Points.Add(Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0));
		}
		return Points;
	}
}

// ================================================================ setup

AHexLevelEditor::AHexLevelEditor()
{
	PrimaryActorTick.bCanEverTick = true;
	Grid = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BuildGrid"));
	Grid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Grid->SetCastShadow(false);
	RootComponent = Grid;
}

UClass* AHexLevelEditor::GetTileClass(bool bGoal) const
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	UClass* Class = bGoal
		? (Settings.GoalTileClass.IsNull() ? nullptr : Settings.GoalTileClass.LoadSynchronous())
		: (Settings.TileClass.IsNull() ? nullptr : Settings.TileClass.LoadSynchronous());
	return Class ? Class : (bGoal ? AHexGoalTile::StaticClass() : AHexTile::StaticClass());
}

APlayerController* AHexLevelEditor::GetPC() const
{
	return GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
}

void AHexLevelEditor::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();

	// Surface height of a tile (measured once on a throwaway tile at rest).
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AHexTile* Probe = World->SpawnActor<AHexTile>(GetTileClass(false), UHexGridLibrary::CoordToWorld(FIntPoint::ZeroValue), FRotator::ZeroRotator, Params))
	{
		SurfaceZ = Probe->GetSurfaceLocation().Z - Probe->GetActorLocation().Z;
		bSurfaceKnown = true;
		// The build grid uses the tile mesh, flattened, in the translucent editor material.
		if (UStaticMeshComponent* Mesh = Probe->GetTileMesh())
		{
			Grid->SetStaticMesh(Mesh->GetStaticMesh());
			Grid->SetForceDisableNanite(true);	// translucent grid: Nanite can't draw it
			GridMeshScale = Mesh->GetComponentScale();
		}
		Probe->Destroy();
	}
	if (UMaterialInterface* GhostMaterial = Settings.EditorGhostMaterial.IsNull() ? nullptr : Settings.EditorGhostMaterial.LoadSynchronous())
	{
		GridMaterial = UMaterialInstanceDynamic::Create(GhostMaterial, this);
		GridMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.35f, 0.62f, 1.f, 1.f));
		GridMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.1f);
		for (int32 Slot = 0; Slot < Grid->GetNumMaterials(); ++Slot)
		{
			Grid->SetMaterial(Slot, GridMaterial);
		}
	}

	Camera = World->SpawnActor<ACameraActor>(Params);
	if (Camera)
	{
		Camera->GetCameraComponent()->SetFieldOfView(Settings.BoardCameraFOV);
		Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
	}
}

void AHexLevelEditor::LoadDefinition(const FHexLevelDefinition& InDefinition)
{
	for (TPair<FIntPoint, TWeakObjectPtr<AActor>>& Pair : TileActors)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Destroy();
		}
	}
	for (TPair<FIntPoint, TWeakObjectPtr<AActor>>& Pair : BoxActors)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Destroy();
		}
	}
	if (StartActor.IsValid())
	{
		StartActor->Destroy();
	}
	TileActors.Reset();
	BoxActors.Reset();
	Definition = InDefinition;
	for (const FHexTileDef& Tile : Definition.Tiles)
	{
		SpawnTileActor(Tile, false, false);
	}
	for (const FIntPoint& Box : Definition.Boxes)
	{
		SpawnBoxActor(Box, false);
	}
	if (Definition.bHasPlayerStart)
	{
		SpawnStartActor(Definition.PlayerStart, false);
	}
	RefreshGrid();
	bCameraSnapped = false;
	NotifyChanged();
}

// ================================================================ actors

FVector AHexLevelEditor::GetSurface(const FIntPoint& Cell) const
{
	return UHexGridLibrary::CoordToWorld(Cell) + FVector(0.0, 0.0, SurfaceZ);
}

AActor* AHexLevelEditor::SpawnTileActor(const FHexTileDef& Tile, bool bAnimate, bool bUpgrade)
{
	AHexTile* Actor = Cast<AHexTile>(UHexLevelTools::SpawnTileActor(GetWorld(), Tile));
	if (!Actor)
	{
		return nullptr;
	}
	Actor->SetCoord(Tile.Coord);
	if (Tile.HasFace())
	{
		Actor->SetCardLabel(Tile.GetFace());
	}
	TileActors.Add(Tile.Coord, Actor);
	if (bAnimate)
	{
		// Upgrade: a bigger, bouncier pop; a plain placement: drops in and squashes.
		AddPop(Actor, bUpgrade ? 0.8f : 0.55f, bUpgrade ? 9.f : 6.f, bUpgrade ? -0.35f : -0.25f);
	}
	return Actor;
}

AActor* AHexLevelEditor::SpawnBoxActor(const FIntPoint& Cell, bool bAnimate)
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	UClass* BoxClass = Settings.BoxClass.IsNull() ? nullptr : Settings.BoxClass.LoadSynchronous();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHexBox* Box = GetWorld()->SpawnActor<AHexBox>(BoxClass ? BoxClass : AHexBox::StaticClass(), GetSurface(Cell), FRotator::ZeroRotator, Params);
	if (!Box)
	{
		return nullptr;
	}
	AHexTile* Tile = Cast<AHexTile>(TileActors.FindRef(Cell).Get());
	Box->SetOnGoal(Tile && Tile->IsGoal());
	Box->PlaceOnTile(Cell, Tile);
	BoxActors.Add(Cell, Box);
	if (bAnimate)
	{
		AddPop(Box, 0.5f, 7.f, -0.3f);
	}
	return Box;
}

AActor* AHexLevelEditor::SpawnStartActor(const FIntPoint& Cell, bool bAnimate)
{
	// The player's character as a marker: its skeletal mesh + anim class from the pawn class defaults.
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	UClass* PawnClass = Settings.DefaultPlayerPawnClass.IsNull() ? nullptr : Settings.DefaultPlayerPawnClass.LoadSynchronous();
	const ACharacter* Defaults = PawnClass ? Cast<ACharacter>(PawnClass->GetDefaultObject()) : nullptr;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Marker = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!Marker)
	{
		return nullptr;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Marker, TEXT("Root"));
	Marker->SetRootComponent(Root);
	Root->RegisterComponent();
	if (Defaults && Defaults->GetMesh() && Defaults->GetMesh()->GetSkeletalMeshAsset())
	{
		USkeletalMeshComponent* Source = Defaults->GetMesh();
		USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Marker, TEXT("Character"));
		Mesh->SetupAttachment(Root);
		Mesh->SetSkeletalMesh(Source->GetSkeletalMeshAsset());
		Mesh->SetRelativeTransform(Source->GetRelativeTransform());
		if (Source->GetAnimClass())
		{
			Mesh->SetAnimInstanceClass(Source->GetAnimClass());
		}
		for (int32 Slot = 0; Slot < Source->GetNumOverrideMaterials(); ++Slot)
		{
			Mesh->SetMaterial(Slot, Source->OverrideMaterials[Slot]);
		}
		Mesh->SetRenderCustomDepth(true);
		Mesh->SetCustomDepthStencilValue(2);
		Mesh->RegisterComponent();
		StartHeight = Defaults->GetCapsuleComponent() ? Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0 : 90.0;
	}
	Marker->SetActorLocation(GetSurface(Cell) + FVector(0.0, 0.0, StartHeight));
	Marker->SetActorRotation(FRotator(0.f, 180.f, 0.f));	// face the camera
	StartActor = Marker;
	if (bAnimate)
	{
		AddPop(Marker, 0.4f, 8.f, -0.3f);
	}
	return Marker;
}

void AHexLevelEditor::DestroyCellActor(TMap<FIntPoint, TWeakObjectPtr<AActor>>& Map, const FIntPoint& Cell, bool bAnimate)
{
	TWeakObjectPtr<AActor> Actor;
	if (Map.RemoveAndCopyValue(Cell, Actor) && Actor.IsValid())
	{
		if (bAnimate)
		{
			FPop& Pop = Pops.AddDefaulted_GetRef();
			Pop.Actor = Actor;
			Pop.BaseScale = Actor->GetActorScale3D();
			Pop.bShrinkAndDestroy = true;
		}
		else
		{
			Actor->Destroy();
		}
	}
}

void AHexLevelEditor::RespawnTile(const FIntPoint& Cell, bool bUpgrade, bool bDowngrade)
{
	const FHexTileDef* Tile = Definition.FindTile(Cell);
	if (!Tile)
	{
		return;
	}
	DestroyCellActor(TileActors, Cell, false);
	AActor* Actor = SpawnTileActor(*Tile, false, false);
	if (Actor && bUpgrade)
	{
		AddPop(Actor, 0.75f, 10.f, -0.45f);	// squash down, spring up past full size: "levelled up"
		PulseRing(Cell);
	}
	else if (Actor && bDowngrade)
	{
		AddPop(Actor, 1.12f, -4.f, 0.2f);	// deflates back to a plain tile
	}
	// A box standing here re-reads the tile (goal state + face on its top).
	if (AHexBox* Box = Cast<AHexBox>(BoxActors.FindRef(Cell).Get()))
	{
		Box->SetOnGoal(Tile->bGoal);
		Box->PlaceOnTile(Cell, Cast<AHexTile>(Actor));
	}
}

void AHexLevelEditor::AddPop(AActor* Actor, float StartScale, float Kick, float Squash, float Delay)
{
	if (!Actor)
	{
		return;
	}
	FPop& Pop = Pops.AddDefaulted_GetRef();
	Pop.Actor = Actor;
	Pop.BaseScale = Actor->GetActorScale3D();
	Pop.Value = StartScale;
	Pop.Velocity = Kick;
	Pop.SquashZ = Squash;
	Pop.Delay = Delay;
	Actor->SetActorScale3D(Pop.BaseScale * StartScale);
}

void AHexLevelEditor::PulseRing(const FIntPoint& Cell)
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	UClass* RingClass = Settings.GoalRingFXClass.IsNull() ? nullptr : Settings.GoalRingFXClass.LoadSynchronous();
	if (!RingClass)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Ring = GetWorld()->SpawnActor<AActor>(RingClass, FTransform::Identity, Params);
	if (!Ring)
	{
		return;
	}
	if (FBoolProperty* Closed = CastField<FBoolProperty>(HexEditor::FindProp(Ring, TEXT("ClosedLoop"))))
	{
		Closed->SetPropertyValue_InContainer(Ring, true);
	}
	if (FBoolProperty* Dashed = CastField<FBoolProperty>(HexEditor::FindProp(Ring, TEXT("DashedLine"))))
	{
		Dashed->SetPropertyValue_InContainer(Ring, false);
	}
	FRingPulse& Pulse = Rings.AddDefaulted_GetRef();
	Pulse.Actor = Ring;
	Pulse.Center = GetSurface(Cell) + FVector(0.0, 0.0, 4.0);
}

void AHexLevelEditor::RefreshGrid()
{
	Grid->ClearInstances();
	if (!Grid->GetStaticMesh())
	{
		return;
	}
	TSet<FIntPoint> Cells;
	if (Definition.Tiles.Num() == 0)
	{
		Cells.Add(FIntPoint::ZeroValue);
	}
	for (const FHexTileDef& Tile : Definition.Tiles)
	{
		for (int32 Dir = 0; Dir < 6; ++Dir)
		{
			Cells.Add(UHexGridLibrary::GetNeighbor(Tile.Coord, static_cast<EHexDirection>(Dir)));
		}
	}
	for (const FIntPoint& Cell : Cells)
	{
		if (HasTile(Cell))
		{
			continue;
		}
		const FVector Location = UHexGridLibrary::CoordToWorld(Cell) + FVector(0.0, 0.0, SurfaceZ * 0.1);
		Grid->AddInstance(FTransform(FRotator::ZeroRotator, Location, GridMeshScale * FVector(0.94, 0.94, 0.06)), true);
	}
}

// ================================================================ ghosts

void AHexLevelEditor::ApplyGhostLook(AActor* Actor, const FLinearColor& Color)
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	UMaterialInterface* GhostMaterial = Settings.EditorGhostMaterial.IsNull() ? nullptr : Settings.EditorGhostMaterial.LoadSynchronous();
	if (!Actor || !GhostMaterial)
	{
		return;
	}
	TArray<UMeshComponent*> Meshes;
	Actor->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetAttachParent() && Mesh->GetAttachParent()->IsA<UHexCardLabelComponent>())
		{
			continue;	// the card face stays readable on the preview
		}
		if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Mesh))
		{
			StaticMesh->SetForceDisableNanite(true);	// Nanite can't draw translucent materials
		}
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
		if (!MID || MID->Parent != GhostMaterial)
		{
			MID = UMaterialInstanceDynamic::Create(GhostMaterial, Actor);
		}
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetScalarParameterValue(TEXT("Opacity"), 0.6f);
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			Mesh->SetMaterial(Slot, MID);
		}
		Mesh->SetRenderCustomDepth(false);
		Mesh->SetCastShadow(false);
	}
}

AActor* AHexLevelEditor::GetGhost(EHexEditorItem Item, EHexSuit Suit, int32 Rank)
{
	const int32 Index = Item == EHexEditorItem::Scenery ? 4 : Item == EHexEditorItem::Tile ? 0 : (Item == EHexEditorItem::Goal ? 1 : (Item == EHexEditorItem::Box ? 2 : 3));
	if (Ghosts.Num() < 5)
	{
		Ghosts.SetNum(5);
	}
	AActor* Ghost = Ghosts[Index];
	if (!Ghost)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Far(0.0, 0.0, -100000.0);
		switch (Item)
		{
		case EHexEditorItem::Tile:
		case EHexEditorItem::Goal:
			Ghost = GetWorld()->SpawnActor<AActor>(GetTileClass(Item == EHexEditorItem::Goal), Far, FRotator::ZeroRotator, Params);
			break;
		case EHexEditorItem::Scenery:
		{
			auto* Marker=GetWorld()->SpawnActor<AStaticMeshActor>(Far,FRotator::ZeroRotator,Params);
			Marker->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Marker->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
			Marker->SetActorScale3D(FVector(.65,.65,.8)); Ghost=Marker;
			break;
		}
		case EHexEditorItem::Box:
		{
			const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
			UClass* BoxClass = Settings.BoxClass.IsNull() ? nullptr : Settings.BoxClass.LoadSynchronous();
			Ghost = GetWorld()->SpawnActor<AActor>(BoxClass ? BoxClass : AHexBox::StaticClass(), Far, FRotator::ZeroRotator, Params);
			break;
		}
		default:
		{
			const TWeakObjectPtr<AActor> Saved = StartActor;
			Ghost = SpawnStartActor(FIntPoint::ZeroValue, false);
			StartActor = Saved;
			break;
		}
		}
		if (Ghost)
		{
			Ghost->SetActorEnableCollision(false);
			Ghost->SetActorTickEnabled(false);	// tiles animate their own lift from a base location: not for a preview
			ApplyGhostLook(Ghost, HexEditor::GhostFree);
		}
		Ghosts[Index] = Ghost;
	}
	if (AHexTile* Tile = Cast<AHexTile>(Ghost))
	{
		if (Item == EHexEditorItem::Tile && (!Tile->HasCardLabel() || Tile->GetCardLabel().Suit != Suit || Tile->GetCardLabel().Rank != Rank))
		{
			Tile->SetCardLabel(FHexCard(FMath::Max(1, Rank), Suit));
		}
	}
	return Ghost;
}

void AHexLevelEditor::HidePreview()
{
	if (AActor* Ghost = ActiveGhost.Get())
	{
		Ghost->SetActorHiddenInGame(true);
	}
	ActiveGhost.Reset();
	bGhostPlaced = false;
	SetHighlightTile(nullptr, true);
}

void AHexLevelEditor::SetHighlightTile(const FIntPoint* Cell, bool bValid)
{
	if (HighlightCell.IsSet() && (!Cell || HighlightCell.GetValue() != *Cell))
	{
		if (AHexTile* Old = Cast<AHexTile>(TileActors.FindRef(HighlightCell.GetValue()).Get()))
		{
			Old->SetGuideState(EHexTileGuide::None);
		}
		HighlightCell.Reset();
	}
	if (Cell)
	{
		if (AHexTile* Tile = Cast<AHexTile>(TileActors.FindRef(*Cell).Get()))
		{
			Tile->SetGuideState(bValid ? EHexTileGuide::ValidHovered : EHexTileGuide::InvalidHovered);
			HighlightCell = *Cell;
		}
	}
}

bool AHexLevelEditor::IsDropValid(const FHexEditorDrag& Drag, const FIntPoint& Cell, bool& bOutOnTile) const
{
	bOutOnTile = HasTile(Cell);
	switch (Drag.Item)
	{
	case EHexEditorItem::Scenery:
		return bOutOnTile && !Definition.FindTile(Cell)->IsScenery() && !Definition.FindTile(Cell)->bGoal && !HasBox(Cell) && !IsStart(Cell);

	case EHexEditorItem::Tile:
		return Drag.bFromWorld ? !bOutOnTile : true;	// new tiles may also land on a tile (= change its suit)
	case EHexEditorItem::Goal:
	{
		const FHexTileDef* Tile = Definition.FindTile(Cell);
		return Tile && !Tile->bGoal && !Tile->IsScenery();
	}
	case EHexEditorItem::Box:
		return bOutOnTile && !Definition.FindTile(Cell)->IsScenery() && !HasBox(Cell) && !IsStart(Cell);
	case EHexEditorItem::Start:
		return bOutOnTile && !Definition.FindTile(Cell)->IsScenery() && !HasBox(Cell);
	default:
		return false;
	}
}

// ================================================================ mouse

bool AHexLevelEditor::GetMouseCell(FIntPoint& OutCell, FVector& OutWorld) const
{
	const APlayerController* PC = GetPC();
	// Read the cursor from Slate: while the editor UI captures the mouse (dragging), the player
	// controller's own mouse position stops updating.
	AHexLevelEditor* Self = const_cast<AHexLevelEditor*>(this);
	const FVector2D Mouse = UWidgetLayoutLibrary::GetMousePositionOnViewport(Self) * UWidgetLayoutLibrary::GetViewportScale(Self);
	const float MouseX = static_cast<float>(Mouse.X), MouseY = static_cast<float>(Mouse.Y);
	FVector Origin, Direction;
	if (!PC || !PC->DeprojectScreenPositionToWorld(MouseX, MouseY, Origin, Direction)
		|| FMath::IsNearlyZero(Direction.Z))
	{
		return false;
	}
	const double PlaneZ = UHexGridLibrary::CoordToWorld(FIntPoint::ZeroValue).Z + SurfaceZ;
	const double T = (PlaneZ - Origin.Z) / Direction.Z;
	if (T <= 0.0)
	{
		return false;
	}
	OutWorld = Origin + Direction * T;
	OutCell = UHexGridLibrary::WorldToCoord(OutWorld);
	return true;
}

bool AHexLevelEditor::PickUnderMouse(FHexEditorDrag& OutDrag)
{
	FIntPoint Cell;
	FVector World;
	if (!GetMouseCell(Cell, World) || !HasTile(Cell))
	{
		return false;
	}
	OutDrag = FHexEditorDrag();
	OutDrag.bFromWorld = true;
	OutDrag.Origin = Cell;
	SetHighlightTile(nullptr, true);
	if (HoverCell.IsSet())
	{
		if (AHexTile* Hovered = Cast<AHexTile>(TileActors.FindRef(HoverCell.GetValue()).Get()))
		{
			Hovered->SetGuideState(EHexTileGuide::None);
		}
		HoverCell.Reset();
	}

	// Topmost first: box, start, the goal "spirit", then the bare tile.
	if (HasBox(Cell))
	{
		OutDrag.Item = EHexEditorItem::Box;
		Definition.Boxes.Remove(Cell);
		DestroyCellActor(BoxActors, Cell, false);
		if (AHexTile* Under = Cast<AHexTile>(TileActors.FindRef(Cell).Get()))
		{
			Under->SetLabelCovered(false, true);	// the box carried the tile's face: show it on the tile again
		}
	}
	else if (IsStart(Cell))
	{
		OutDrag.Item = EHexEditorItem::Start;
		Definition.bHasPlayerStart = false;
		if (StartActor.IsValid())
		{
			StartActor->Destroy();
		}
	}
	else if (FHexTileDef* Tile = Definition.Tiles.FindByPredicate([&Cell](const FHexTileDef& Def) { return Def.Coord == Cell; }))
	{
		if (Tile->IsScenery())
		{
			OutDrag.Item=EHexEditorItem::Scenery;
			OutDrag.SceneryVariant=Tile->SceneryVariant;
			OutDrag.SceneryRotationSteps=Tile->SceneryRotationSteps;
			OutDrag.Suit=Tile->Suit;
			Tile->SceneryVariant=0;
			Tile->SceneryRotationSteps=0;
			RespawnTile(Cell,false,true);
		}
		else if (Tile->bGoal)
		{
			OutDrag.Item = EHexEditorItem::Goal;
			Tile->bGoal = false;
			RespawnTile(Cell, false, true);	// the goal leaves: plain tile again
		}
		else
		{
			OutDrag.Item = EHexEditorItem::Tile;
			OutDrag.Suit = Tile->Suit;
			OutDrag.Rank = Tile->Rank;
			Definition.Tiles.RemoveAll([&Cell](const FHexTileDef& Def) { return Def.Coord == Cell; });
			DestroyCellActor(TileActors, Cell, false);
			RefreshGrid();
		}
	}
	GhostLocation = World;
	GhostVelocity = FVector::ZeroVector;
	bGhostPlaced = false;
	NotifyChanged();
	return OutDrag.Item != EHexEditorItem::None;
}

void AHexLevelEditor::UpdateDragPreview(const FHexEditorDrag& Drag, bool bOverUI)
{
	AActor* Ghost = GetGhost(Drag.Item, Drag.Suit, Drag.Item == EHexEditorItem::Tile ? Drag.Rank : 1);
	if (ActiveGhost.Get() != Ghost)
	{
		HidePreview();
		ActiveGhost = Ghost;
	}
	FIntPoint Cell;
	FVector World;
	if (!Ghost || bOverUI || !GetMouseCell(Cell, World))
	{
		if (Ghost)
		{
			Ghost->SetActorHiddenInGame(true);
		}
		bGhostPlaced = false;
		SetHighlightTile(nullptr, true);
		return;
	}
	Ghost->SetActorHiddenInGame(false);
	if (Drag.Item == EHexEditorItem::Tile && !Drag.bFromWorld && !HasTile(Cell))
	{
		GetGhost(Drag.Item, Drag.Suit, PickSpreadRank(Drag.Suit, Cell));	// preview the number it will get
	}

	bool bOnTile = false;
	const bool bValid = IsDropValid(Drag, Cell, bOnTile);
	const double TileHeight = Drag.Item == EHexEditorItem::Start ? StartHeight : 0.0;
	FVector Target = World + FVector(0.0, 0.0, TileHeight + 30.0);	// floats a little above the board while free
	if (bValid)
	{
		// Magnet: inside the snap radius the preview springs onto the cell.
		const FVector Center = (Drag.Item == EHexEditorItem::Tile && !bOnTile) ? UHexGridLibrary::CoordToWorld(Cell) : GetSurface(Cell);
		if (FVector::Dist2D(World, Center) < UHexGridLibrary::GetTileSpacing() * 0.5)
		{
			Target = Center + FVector(0.0, 0.0, TileHeight + (Drag.Item == EHexEditorItem::Tile && !bOnTile ? 6.0 : 26.0));
		}
	}
	if (!bGhostPlaced)
	{
		GhostLocation = Target;
		GhostVelocity = FVector::ZeroVector;
		bGhostPlaced = true;
	}
	// Highlight the tile it would land on (green lift) or refuse (red).
	if (bOnTile && Drag.Item != EHexEditorItem::Tile)
	{
		SetHighlightTile(&Cell, bValid);
	}
	else if (bOnTile && Drag.Item == EHexEditorItem::Tile && !Drag.bFromWorld)
	{
		SetHighlightTile(&Cell, true);	// will change that tile's suit
	}
	else
	{
		SetHighlightTile(nullptr, true);
	}
	ApplyGhostLook(Ghost, bValid ? HexEditor::GhostValid : HexEditor::GhostInvalid);
	const double DeltaTime = FMath::Min(GetWorld()->GetDeltaSeconds(), 0.05f);
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(DeltaTime / (1.0 / 120.0)));
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		const double Dt = DeltaTime / Steps;
		GhostVelocity += ((Target - GhostLocation) * 520.0 - GhostVelocity * 26.0) * Dt;
		GhostLocation += GhostVelocity * Dt;
	}
	Ghost->SetActorLocation(GhostLocation);
	const double Tilt = FMath::Clamp(GhostVelocity.X * 0.01, -8.0, 8.0);
	Ghost->SetActorRotation(FRotator(Tilt, Drag.Item == EHexEditorItem::Start ? 180.0 : 0.0, FMath::Clamp(-GhostVelocity.Y * 0.01, -8.0, 8.0)));
}

bool AHexLevelEditor::DropUnderMouse(const FHexEditorDrag& Drag)
{
	FIntPoint Cell;
	FVector World;
	bool bOnTile = false;
	if (!GetMouseCell(Cell, World) || !IsDropValid(Drag, Cell, bOnTile))
	{
		HidePreview();
		return false;
	}
	HidePreview();
	switch (Drag.Item)
	{
	case EHexEditorItem::Scenery:
		if (FHexTileDef* Tile=Definition.Tiles.FindByPredicate([&Cell](const FHexTileDef& T){return T.Coord==Cell;}))
		{
			Tile->SceneryVariant=FMath::Clamp(Drag.SceneryVariant,1,Tile->GetSceneryVariantCount());
			Tile->SceneryRotationSteps=Drag.SceneryRotationSteps;
			RespawnTile(Cell,true,false);
		}
		break;

	case EHexEditorItem::Tile:
		if (bOnTile)
		{
			SetSuit(Cell, Drag.Suit);	// a palette tile dropped on a tile re-suits it
		}
		else
		{
			FHexTileDef& Tile = Definition.Tiles.AddDefaulted_GetRef();
			Tile.Coord = Cell;
			Tile.Suit = Drag.Suit;
			Tile.Rank = Drag.bFromWorld ? Drag.Rank : PickSpreadRank(Drag.Suit, Cell);
			Tile.bGoal = false;
			SpawnTileActor(Tile, true, false);
			RefreshGrid();
		}
		break;
	case EHexEditorItem::Goal:
		if (FHexTileDef* Tile = Definition.Tiles.FindByPredicate([&Cell](const FHexTileDef& Def) { return Def.Coord == Cell; }))
		{
			Tile->bGoal = true;
			RespawnTile(Cell, true, false);
		}
		break;
	case EHexEditorItem::Box:
		Definition.Boxes.Add(Cell);
		SpawnBoxActor(Cell, true);
		break;
	case EHexEditorItem::Start:
		if (StartActor.IsValid())
		{
			StartActor->Destroy();	// one start only: it moves
		}
		Definition.PlayerStart = Cell;
		Definition.bHasPlayerStart = true;
		SpawnStartActor(Cell, true);
		break;
	default:
		return false;
	}
	NotifyChanged();
	return true;
}

void AHexLevelEditor::TrashDrag(const FHexEditorDrag& Drag)
{
	HidePreview();
	NotifyChanged();
}

void AHexLevelEditor::CancelDrag(const FHexEditorDrag& Drag)
{
	HidePreview();
	if (!Drag.bFromWorld)
	{
		return;
	}
	const FIntPoint Cell = Drag.Origin;
	switch (Drag.Item)
	{
	case EHexEditorItem::Scenery:
		if (FHexTileDef* Tile=Definition.Tiles.FindByPredicate([&Cell](const FHexTileDef& T){return T.Coord==Cell;}))
		{
			Tile->SceneryVariant=FMath::Clamp(Drag.SceneryVariant,1,Tile->GetSceneryVariantCount());
			Tile->SceneryRotationSteps=Drag.SceneryRotationSteps;
			RespawnTile(Cell,true,false);
		}
		break;

	case EHexEditorItem::Tile:
		if (!HasTile(Cell))
		{
			FHexTileDef& Tile = Definition.Tiles.AddDefaulted_GetRef();
			Tile.Coord = Cell;
			Tile.Suit = Drag.Suit;
			Tile.Rank = Drag.Rank;
			SpawnTileActor(Tile, true, false);
			RefreshGrid();
		}
		break;
	case EHexEditorItem::Goal:
		if (FHexTileDef* Tile = Definition.Tiles.FindByPredicate([&Cell](const FHexTileDef& Def) { return Def.Coord == Cell; }))
		{
			Tile->bGoal = true;
			RespawnTile(Cell, true, false);
		}
		break;
	case EHexEditorItem::Box:
		if (HasTile(Cell) && !HasBox(Cell))
		{
			Definition.Boxes.Add(Cell);
			SpawnBoxActor(Cell, true);
		}
		break;
	case EHexEditorItem::Start:
		if (HasTile(Cell))
		{
			Definition.PlayerStart = Cell;
			Definition.bHasPlayerStart = true;
			SpawnStartActor(Cell, true);
		}
		break;
	default:
		break;
	}
	NotifyChanged();
}

// ================================================================ hover + faces

void AHexLevelEditor::UpdateHover(bool bAllowed)
{
	FIntPoint Cell;
	FVector World;
	TOptional<FIntPoint> NewHover;
	if (bAllowed && GetMouseCell(Cell, World))
	{
		if (HasTile(Cell))
		{
			NewHover = Cell;
		}
		else if (HoverCell.IsSet() && FVector::Dist2D(World, UHexGridLibrary::CoordToWorld(HoverCell.GetValue())) < UHexGridLibrary::GetTileSpacing() * 0.75)
		{
			NewHover = HoverCell;	// sticky: the arrows sit just outside the tile
		}
	}
	if (NewHover == HoverCell)
	{
		return;
	}
	if (HoverCell.IsSet())
	{
		if (AHexTile* Old = Cast<AHexTile>(TileActors.FindRef(HoverCell.GetValue()).Get()))
		{
			Old->SetGuideState(EHexTileGuide::None);
		}
	}
	HoverCell = NewHover;
	if (HoverCell.IsSet())
	{
		if (AHexTile* Tile = Cast<AHexTile>(TileActors.FindRef(HoverCell.GetValue()).Get()))
		{
			Tile->SetGuideState(EHexTileGuide::ValidHovered);
		}
	}
}

bool AHexLevelEditor::GetHoveredTile(FIntPoint& OutCell) const
{
	if (HoverCell.IsSet() && HasTile(HoverCell.GetValue()))
	{
		OutCell = HoverCell.GetValue();
		return true;
	}
	return false;
}

FVector AHexLevelEditor::GetLabelWorldLocation(const FIntPoint& Cell) const
{
	const AHexTile* Tile = Cast<AHexTile>(TileActors.FindRef(Cell).Get());
	return GetSurface(Cell) + FVector(0.0, 0.0, Tile ? Tile->GetGuideLift() : 0.0);
}

void AHexLevelEditor::SetRank(const FIntPoint& Cell, int32 Rank)
{
	FHexTileDef* Tile = Definition.Tiles.FindByPredicate([&Cell](const FHexTileDef& Def) { return Def.Coord == Cell; });
	if (!Tile)
	{
		return;
	}
	if (Tile->IsScenery()) return; // Keep the original face until scenery is removed.
	Tile->Rank = FMath::Clamp(Rank, 1, 10);
	if (AHexTile* Actor = Cast<AHexTile>(TileActors.FindRef(Cell).Get()))
	{
		Actor->SetCardLabel(Tile->GetFace());	// the label pops in with its own spring
		Actor->PlayFlash(0.35f);
	}
	if (AHexBox* Box = Cast<AHexBox>(BoxActors.FindRef(Cell).Get()))
	{
		Box->RefreshCardLabel(true);
	}
	NotifyChanged();
}

void AHexLevelEditor::ChangeRank(const FIntPoint& Cell, int32 Delta)
{
	if (FHexTileDef* Tile=Definition.Tiles.FindByPredicate([&Cell](const FHexTileDef& T){return T.Coord==Cell;}))
	{
		if (Tile->IsScenery())
		{
			Tile->SceneryVariant=((Tile->SceneryVariant-1+Delta)%Tile->GetSceneryVariantCount()+Tile->GetSceneryVariantCount())%Tile->GetSceneryVariantCount()+1;
			RespawnTile(Cell,true,false); NotifyChanged(); return;
		}
	}

	if (const FHexTileDef* Tile = Definition.FindTile(Cell))
	{
		const int32 Rank = ((FMath::Max(1, Tile->Rank) - 1 + Delta) % 10 + 10) % 10 + 1;	// A..10, wrapping
		SetRank(Cell, Rank);
	}
}

void AHexLevelEditor::RotateScenery(const FIntPoint& Cell, int32 Delta)
{
	if (FHexTileDef* Tile=Definition.Tiles.FindByPredicate([&](const FHexTileDef& T){return T.Coord==Cell;}))
		if(Tile->CanRotateScenery())
		{
			Tile->SceneryRotationSteps=((Tile->SceneryRotationSteps+Delta)%6+6)%6;
			if(auto* Actor=TileActors.FindRef(Cell).Get())
				if(auto* Scenery=Actor->FindComponentByClass<UHexSceneryComponent>()) Scenery->SetRotationSteps(Tile->SceneryRotationSteps);
			NotifyChanged();
		}
}

void AHexLevelEditor::SetSuit(const FIntPoint& Cell, EHexSuit Suit)
{
	FHexTileDef* Tile = Definition.Tiles.FindByPredicate([&Cell](const FHexTileDef& Def) { return Def.Coord == Cell; });
	if (!Tile)
	{
		return;
	}
	Tile->Suit = Suit;
	if (Tile->IsScenery())
	{
		Tile->SceneryVariant = FMath::Clamp(Tile->SceneryVariant, 1, Tile->GetSceneryVariantCount());
		RespawnTile(Cell, false, false);
		NotifyChanged();
		return;
	}
	AHexTile* Actor = Cast<AHexTile>(TileActors.FindRef(Cell).Get());
	if (Actor && Actor->GetClass() != UHexSokobanSettings::ResolveTileClass(Tile->bGoal, Suit))
	{
		// Another suit is another terrain: swap the tile actor (it brings its own ground and plants).
		RespawnTile(Cell, false, false);
		Actor = Cast<AHexTile>(TileActors.FindRef(Cell).Get());
		if (Actor)
		{
			AddPop(Actor, 0.82f, 6.f, -0.25f);
		}
	}
	else if (Actor)
	{
		Actor->SetCardLabel(Tile->GetFace());
		AddPop(Actor, 0.9f, 4.f, -0.15f);
	}
	if (AHexBox* Box = Cast<AHexBox>(BoxActors.FindRef(Cell).Get()))
	{
		Box->RefreshCardLabel(true);
	}
	NotifyChanged();
}

int32 AHexLevelEditor::PickSpreadRank(EHexSuit Suit, const FIntPoint& Cell) const
{
	int32 Used[11] = { 0 };
	int32 UsedInSuit[11] = { 0 };
	bool bNeighbour[11] = { false };
	for (const FHexTileDef& Tile : Definition.Tiles)
	{
		if (Tile.Rank >= 1 && Tile.Rank <= 10)
		{
			++Used[Tile.Rank];
			UsedInSuit[Tile.Rank] += Tile.Suit == Suit ? 1 : 0;
			if (UHexGridLibrary::GetHexDistance(Tile.Coord, Cell) == 1)
			{
				bNeighbour[Tile.Rank] = true;
			}
		}
	}
	int32 Best = 1;
	int32 BestScore = MAX_int32;
	for (int32 Rank = 1; Rank <= 10; ++Rank)
	{
		// Least used overall, then in this suit, then not next to the same rank; ties by a cell-based shuffle.
		const int32 Jitter = ((Cell.X * 73856093) ^ (Cell.Y * 19349663) ^ (Rank * 83492791)) & 7;
		const int32 Score = Used[Rank] * 1000 + UsedInSuit[Rank] * 100 + (bNeighbour[Rank] ? 50 : 0) + Jitter;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Rank;
		}
	}
	return Best;
}

// ================================================================ camera

void AHexLevelEditor::Zoom(float Steps)
{
	ZoomFactor = FMath::Clamp(ZoomFactor * FMath::Pow(0.88f, Steps), 0.45f, 2.6f);
}

void AHexLevelEditor::Pan(const FVector2D& PixelDelta)
{
	if (!Camera)
	{
		return;
	}
	const FRotator Rotation = Camera->GetActorRotation();
	const FVector Right = FRotationMatrix(FRotator(0.f, Rotation.Yaw, 0.f)).GetUnitAxis(EAxis::Y);
	const FVector Forward = FRotationMatrix(FRotator(0.f, Rotation.Yaw, 0.f)).GetUnitAxis(EAxis::X);
	const double Scale = 0.85 * ZoomFactor;
	PanOffset += (-Right * PixelDelta.X + Forward * PixelDelta.Y) * Scale;
}

void AHexLevelEditor::UpdateCamera(float DeltaSeconds, bool bSnap)
{
	if (!Camera)
	{
		return;
	}
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	FBox Bounds(ForceInit);
	for (const FHexTileDef& Tile : Definition.Tiles)
	{
		Bounds += UHexGridLibrary::CoordToWorld(Tile.Coord);
	}
	if (!Bounds.IsValid)
	{
		Bounds += UHexGridLibrary::CoordToWorld(FIntPoint::ZeroValue);
	}
	const double Radius = FMath::Max(Bounds.GetExtent().Size2D() + UHexGridLibrary::GetTileSpacing() * 2.5, UHexGridLibrary::GetTileSpacing() * 5.0);
	const FRotator Rotation(-Settings.BoardCameraPitch, Settings.BoardCameraYaw, 0.f);
	const double HalfFov = FMath::DegreesToRadians(Settings.BoardCameraFOV * 0.5);
	const double Distance = Radius / FMath::Tan(HalfFov) * ZoomFactor;

	// Keep the board in the part of the screen the right-hand panel doesn't cover.
	int32 ViewX = 1920, ViewY = 1080;
	if (const APlayerController* PC = GetPC())
	{
		PC->GetViewportSize(ViewX, ViewY);
	}
	const double PanelShare = ViewX > 0 ? FMath::Clamp(static_cast<double>(RightPanelPixels) / ViewX, 0.0, 0.6) : 0.0;
	const FVector Right = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y);
	const double HalfWidth = Distance * FMath::Tan(HalfFov);
	const FVector Focus = Bounds.GetCenter() + PanOffset + Right * (HalfWidth * PanelShare);
	const FVector Desired = Focus - Rotation.Vector() * Distance;
	if (bSnap || !bCameraSnapped)
	{
		CameraLocation = Desired;
		CameraVelocity = FVector::ZeroVector;
		bCameraSnapped = true;
	}
	else
	{
		for (float Remaining=FMath::Min(DeltaSeconds,0.1f); Remaining>0.f;)
		{
			const float Step=FMath::Min(Remaining,1.f/120.f); Remaining-=Step;
			CameraVelocity+=((Desired-CameraLocation)*80.f-CameraVelocity*15.f)*Step;
			CameraLocation+=CameraVelocity*Step;
		}
	}
	Camera->SetActorLocationAndRotation(CameraLocation, Rotation);
}

// ================================================================ tick

void AHexLevelEditor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float DeltaTime = FMath::Min(DeltaSeconds, 0.05f);

	// Springy scale animations (place / upgrade / downgrade / delete).
	for (int32 Index = Pops.Num() - 1; Index >= 0; --Index)
	{
		FPop& Pop = Pops[Index];
		AActor* Actor = Pop.Actor.Get();
		if (!Actor)
		{
			Pops.RemoveAt(Index);
			continue;
		}
		if (Pop.Delay > 0.f)
		{
			Pop.Delay -= DeltaTime;
			continue;
		}
		Pop.Age += DeltaTime;
		if (Pop.bShrinkAndDestroy)
		{
			const float T = FMath::Clamp(Pop.Age / 0.2f, 0.f, 1.f);
			Actor->SetActorScale3D(Pop.BaseScale * FMath::Max(0.01f, 1.f - T * T));
			Actor->AddActorWorldRotation(FRotator(0.f, 600.f * DeltaTime, 0.f));
			if (T >= 1.f)
			{
				Actor->Destroy();
				Pops.RemoveAt(Index);
			}
			continue;
		}
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(DeltaTime / (1.f / 120.f)));
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const float Dt = DeltaTime / static_cast<float>(Steps);
			Pop.Velocity += ((1.f - Pop.Value) * 320.f - Pop.Velocity * 14.f) * Dt;
			Pop.Value += Pop.Velocity * Dt;
			Pop.SquashVelocity += ((0.f - Pop.SquashZ) * 380.f - Pop.SquashVelocity * 16.f) * Dt;
			Pop.SquashZ += Pop.SquashVelocity * Dt;
		}
		const FVector Scale = Pop.BaseScale * FVector(Pop.Value * (1.f - 0.5f * Pop.SquashZ), Pop.Value * (1.f - 0.5f * Pop.SquashZ), Pop.Value * (1.f + Pop.SquashZ));
		Actor->SetActorScale3D(Scale);
		if (Pop.Age > 1.2f && FMath::Abs(Pop.Value - 1.f) < 0.002f && FMath::Abs(Pop.SquashZ) < 0.002f)
		{
			Actor->SetActorScale3D(Pop.BaseScale);
			Pops.RemoveAt(Index);
		}
	}

	// Upgrade ring: grows out of the tile and fades.
	for (int32 Index = Rings.Num() - 1; Index >= 0; --Index)
	{
		FRingPulse& Ring = Rings[Index];
		Ring.Age += DeltaTime;
		AActor* Actor = Ring.Actor.Get();
		if (!Actor || Ring.Age > 0.55f)
		{
			if (Actor)
			{
				Actor->Destroy();
			}
			Rings.RemoveAt(Index);
			continue;
		}
		const double T = Ring.Age / 0.55;
		const double Radius = UHexGridLibrary::GetTileSpacing() / FMath::Sqrt(3.0) * (0.35 + 0.95 * (1.0 - FMath::Square(1.0 - T)));
		HexEditor::DrawPath(Actor, HexEditor::HexRing(Ring.Center, Radius));
	}

	// Boxes / start ride the hover lift of their tile.
	for (const TPair<FIntPoint, TWeakObjectPtr<AActor>>& Pair : BoxActors)
	{
		const AHexTile* Tile = Cast<AHexTile>(TileActors.FindRef(Pair.Key).Get());
		if (AHexBox* Box = Cast<AHexBox>(Pair.Value.Get()))
		{
			Box->SetGuideLift(Tile ? Tile->GetGuideLift() : 0.f);
		}
	}
	if (AActor* Start = StartActor.Get())
	{
		const AHexTile* Tile = Cast<AHexTile>(TileActors.FindRef(Definition.PlayerStart).Get());
		Start->SetActorLocation(GetSurface(Definition.PlayerStart) + FVector(0.0, 0.0, StartHeight + (Tile ? Tile->GetGuideLift() : 0.0)));
	}

	UpdateCamera(DeltaTime, false);
}

// ================================================================ game mode / controller

AHexEditorGameMode::AHexEditorGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AHexEditorPlayerController::StaticClass();
	HUDClass = AHUD::StaticClass();
}

void AHexEditorGameMode::StartPlay()
{
	Super::StartPlay();
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Editor = World->SpawnActor<AHexLevelEditor>(Params);
	if (!Editor)
	{
		return;
	}
	// What to edit: the draft kept across a test play, a saved level, or a new one.
	UHexGameFlowSubsystem* Flow = UHexGameFlowSubsystem::Get(this);
	FHexLevelDefinition Definition;
	if (Flow && Flow->EditorDraft.IsSet())
	{
		Definition = Flow->EditorDraft.GetValue();
	}
	else if (Flow && Flow->FindLevelIndex(Flow->EditorLevelId) != INDEX_NONE)
	{
		Definition = Flow->GetLevelEntry(Flow->FindLevelIndex(Flow->EditorLevelId))->Definition;
	}
	else
	{
		int32 Custom = 1;
		for (int32 Index = 0; Flow && Index < Flow->GetNumLevels(); ++Index)
		{
			Custom += Flow->GetLevelEntry(Index)->bBuiltIn ? 0 : 1;
		}
		Definition.DisplayName = FHexLocalization::Printf(HEXT("我的关卡 %d"), Custom);	// "my level N"
	}
	Editor->LoadDefinition(Definition);
	if (AHexEditorPlayerController* PC = Cast<AHexEditorPlayerController>(World->GetFirstPlayerController()))
	{
		PC->OpenEditorUI(Editor);
	}
}

AHexEditorPlayerController::AHexEditorPlayerController()
{
	bShowMouseCursor = true;
	bAutoManageActiveCameraTarget = false;
}

void AHexEditorPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (const AHexEditorGameMode* GameMode = GetWorld()->GetAuthGameMode<AHexEditorGameMode>())
	{
		if (GameMode->GetEditor())
		{
			OpenEditorUI(GameMode->GetEditor());
		}
	}
}

void AHexEditorPlayerController::OpenEditorUI(AHexLevelEditor* InEditor)
{
	if (!IsLocalController() || EditorWidget || !InEditor)
	{
		return;
	}
	if (InEditor->GetCamera())
	{
		SetViewTarget(InEditor->GetCamera());
	}
	EditorWidget = CreateWidget<UHexEditorWidget>(this, UHexEditorWidget::StaticClass());
	if (EditorWidget)
	{
		EditorWidget->SetEditor(InEditor);
		EditorWidget->AddToViewport(10);
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(EditorWidget->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
	}
}
