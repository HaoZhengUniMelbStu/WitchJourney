#include "HexPathFXSubsystem.h"
#include "HexLocalization.h"
#include "HexBoardSubsystem.h"
#include "HexCardSubsystem.h"
#include "HexGridLibrary.h"
#include "HexGridMovementComponent.h"
#include "HexSokobanGameMode.h"
#include "HexSokobanMath.h"
#include "HexSokobanSettings.h"
#include "HexTile.h"
#include "HexBox.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Components/MeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace HexPathFX
{
	FProperty* FindProp(UObject* Object, const TCHAR* Name)
	{
		// BP variable names may carry stray spaces ("VisibleThroughObjects ").
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			if (It->GetName().TrimStartAndEnd().Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

	void SetBool(UObject* Object, const TCHAR* Name, bool bValue)
	{
		if (FBoolProperty* Prop = CastField<FBoolProperty>(FindProp(Object, Name)))
		{
			Prop->SetPropertyValue_InContainer(Object, bValue);
		}
	}

	bool GetBool(UObject* Object, const TCHAR* Name)
	{
		const FBoolProperty* Prop = CastField<FBoolProperty>(FindProp(Object, Name));
		return Prop && Prop->GetPropertyValue_InContainer(Object);
	}

	void ClearObject(UObject* Object, const TCHAR* Name)
	{
		if (FObjectPropertyBase* Prop = CastField<FObjectPropertyBase>(FindProp(Object, Name)))
		{
			Prop->SetObjectPropertyValue_InContainer(Object, nullptr);
		}
	}

	void SetColor(UObject* Object, const TCHAR* Name, const FLinearColor& Value)
	{
		FStructProperty* Prop = CastField<FStructProperty>(FindProp(Object, Name));
		if (Prop && Prop->Struct == TBaseStructure<FLinearColor>::Get())
		{
			*Prop->ContainerPtrToValuePtr<FLinearColor>(Object) = Value;
		}
	}

	/**
	 * Guidance lines are drawn above everything, in one colour: the Path Tracer line materials have depth test
	 * off (set on M_PT_DottedLine_Anim / M_PT_Color), so nothing here touches their parameters - the effect's own
	 * dash / solid logic stays in charge. This only keeps them after other translucency and out of shadows.
	 */
	void DrawOnTop(AActor* Actor)
	{
		TArray<UMeshComponent*> Meshes;
		Actor->GetComponents<UMeshComponent>(Meshes);
		for (UMeshComponent* Mesh : Meshes)
		{
			Mesh->SetCastShadow(false);
			Mesh->SetRenderCustomDepth(false);	// lines are not occluders (no Path Tracer see-through pass)
			Mesh->SetTranslucentSortPriority(100);	// after the tiles' translucent feedback overlay
		}
	}

	bool CallDrawPath(AActor* Actor, const TArray<FVector>& Points)
	{
		UFunction* Function = Actor ? Actor->FindFunction(TEXT("DrawPath")) : nullptr;
		if (!Function)
		{
			return false;
		}
		uint8* Params = static_cast<uint8*>(FMemory::Malloc(FMath::Max<int32>(1, Function->ParmsSize), Function->GetMinAlignment()));
		FMemory::Memzero(Params, Function->ParmsSize);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Params);
		}
		bool bOk = false;
		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Function->FindPropertyByName(TEXT("PathPoints"))))
		{
			FStructProperty* Inner = CastField<FStructProperty>(ArrayProp->Inner);
			if (Inner && Inner->Struct == TBaseStructure<FVector>::Get())
			{
				*ArrayProp->ContainerPtrToValuePtr<TArray<FVector>>(Params) = Points;
				Actor->ProcessEvent(Function, Params);
				bOk = true;
				DrawOnTop(Actor);	// DrawPath rebuilt the spline meshes
			}
		}
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Params);
		}
		FMemory::Free(Params);
		return bOk;
	}

	UClass* LoadFXClass(const TSoftClassPtr<AActor>& Ptr)
	{
		return Ptr.IsNull() ? nullptr : Ptr.LoadSynchronous();
	}

	bool SamePoints(const TArray<FVector>& A, const TArray<FVector>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (!A[Index].Equals(B[Index], 0.5))
			{
				return false;
			}
		}
		return true;
	}

	FVector CellCenter(const FIntPoint& Coord, double Z)
	{
		const FVector Location = UHexGridLibrary::CoordToWorld(Coord);
		return FVector(Location.X, Location.Y, Z);
	}
}

// ---------------------------------------------------------------- lifecycle

bool UHexPathFXSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UHexPathFXSubsystem::Deinitialize()
{
	ClearAll();
	Super::Deinitialize();
}

TStatId UHexPathFXSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UHexPathFXSubsystem, STATGROUP_Tickables);
}

void UHexPathFXSubsystem::SetHintVisible(bool bVisible)
{
	if (GetWorld()->GetSubsystem<UHexBoardSubsystem>()->IsPerformanceMode()) bVisible = false;
	bHintVisible = bVisible;
	if (!bHintVisible)
	{
		ClearSlot(HintSlot);
	}
}

void UHexPathFXSubsystem::ClearSlot(FFXSlot& Slot)
{
	if (AActor* Actor = Slot.Actor.Get())
	{
		Actor->Destroy();
	}
	Slot = FFXSlot();
}

void UHexPathFXSubsystem::ClearAll()
{
	for (FFXSlot& Slot : HandSlots) { ClearSlot(Slot); }
	for (FFXSlot& Slot : CardSlots) { ClearSlot(Slot); }
	for (FFXSlot& Slot : GoalSlots) { ClearSlot(Slot); }
	for (TArray<FFXSlot>* Reticle : { &MoveReticle, &BoxReticle, &HintReticle })
	{
		for (FFXSlot& Slot : *Reticle) { ClearSlot(Slot); }
		Reticle->Reset();
	}
	HandSlots.Reset();
	CardSlots.Reset();
	GoalSlots.Reset();
	ClearSlot(MoveSlot);
	ClearSlot(BoxSlot);
	ClearSlot(HintSlot);
	HintStateKey.Reset();
}

// ---------------------------------------------------------------- geometry

double UHexPathFXSubsystem::GetCellLift(const FIntPoint& Coord) const
{
	const UWorld* World = GetWorld();
	const UHexBoardSubsystem* Board = World ? World->GetSubsystem<UHexBoardSubsystem>() : nullptr;
	const AHexTile* Tile = Board ? Board->GetTileAt(Coord) : nullptr;
	if (!Tile)
	{
		return 0.0;
	}
	// Rising / lifted: sit above the target plus the spring's overshoot, so the bounce never moves the line.
	const double Target = Tile->GetGuideTargetLift();
	if (Target > 0.0)
	{
		return Target + 6.0;
	}
	// Settling back down: follow the tile in coarse steps (monotonic, so no back-and-forth rebuilds).
	const double Current = Tile->GetGuideLift();
	return Current > 2.0 ? FMath::CeilToDouble(Current / 8.0) * 8.0 : 0.0;
}

void UHexPathFXSubsystem::BuildRegionLoops(const TArray<FIntPoint>& Cells, float Inset, double Z, TArray<TArray<FVector>>& OutLoops,
	TFunctionRef<double(const FIntPoint&)> CellLift)
{
	OutLoops.Reset();
	if (Cells.Num() == 0)
	{
		return;
	}
	const TSet<FIntPoint> CellSet(Cells);
	const double Radius = UHexGridLibrary::GetTileSpacing() / FMath::Sqrt(3.0);	// centre -> corner

	auto Corner = [Radius](const FIntPoint& Cell, int32 Index)
	{
		const FVector Center = UHexGridLibrary::CoordToWorld(Cell);
		const double Angle = FMath::DegreesToRadians(30.0 + 60.0 * static_cast<double>(((Index % 6) + 6) % 6));
		return FVector2D(Center.X + Radius * FMath::Cos(Angle), Center.Y + Radius * FMath::Sin(Angle));
	};
	auto Key = [](const FVector2D& P) { return FIntPoint(FMath::RoundToInt32(P.X * 4.0), FMath::RoundToInt32(P.Y * 4.0)); };

	// Highest lift among the region cells sharing each corner.
	TMap<FIntPoint, double> CornerLift;
	for (const FIntPoint& Cell : CellSet)
	{
		const double Lift = CellLift(Cell);
		for (int32 CornerIndex = 0; CornerIndex < 6; ++CornerIndex)
		{
			double& Value = CornerLift.FindOrAdd(Key(Corner(Cell, CornerIndex)), 0.0);
			Value = FMath::Max(Value, Lift);
		}
	}

	// Boundary edges, each running counter-clockwise around its cell (the region is on the left).
	struct FEdge { FVector2D A; FVector2D B; bool bUsed = false; };
	TArray<FEdge> Edges;
	TMultiMap<FIntPoint, int32> ByStart;
	for (const FIntPoint& Cell : CellSet)
	{
		for (int32 Dir = 0; Dir < 6; ++Dir)
		{
			const FIntPoint Neighbor = UHexGridLibrary::GetNeighbor(Cell, static_cast<EHexDirection>(Dir));
			if (CellSet.Contains(Neighbor))
			{
				continue;
			}
			FEdge Edge;
			Edge.A = Corner(Cell, Dir - 1);	// yaw 60*Dir - 30
			Edge.B = Corner(Cell, Dir);		// yaw 60*Dir + 30
			ByStart.Add(Key(Edge.A), Edges.Add(Edge));
		}
	}

	for (int32 StartIndex = 0; StartIndex < Edges.Num(); ++StartIndex)
	{
		if (Edges[StartIndex].bUsed)
		{
			continue;
		}
		TArray<FVector2D> Loop;
		int32 Current = StartIndex;
		while (Current != INDEX_NONE && !Edges[Current].bUsed)
		{
			Edges[Current].bUsed = true;
			Loop.Add(Edges[Current].A);
			TArray<int32> Next;
			ByStart.MultiFind(Key(Edges[Current].B), Next);
			Current = INDEX_NONE;
			for (const int32 Candidate : Next)
			{
				if (!Edges[Candidate].bUsed)
				{
					Current = Candidate;
					break;
				}
			}
		}
		if (Loop.Num() < 3)
		{
			continue;
		}

		// Shift every edge to its left by Inset (mitred corners).
		TArray<FVector>& Out = OutLoops.AddDefaulted_GetRef();
		const int32 Count = Loop.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2D Prev = Loop[(Index + Count - 1) % Count];
			const FVector2D Cur = Loop[Index];
			const FVector2D Next = Loop[(Index + 1) % Count];
			const FVector2D DirIn = (Cur - Prev).GetSafeNormal();
			const FVector2D DirOut = (Next - Cur).GetSafeNormal();
			const FVector2D N1(-DirIn.Y, DirIn.X);
			const FVector2D N2(-DirOut.Y, DirOut.X);
			const double Denominator = FMath::Max(0.2, 1.0 + FVector2D::DotProduct(N1, N2));
			const FVector2D Shifted = Cur + (N1 + N2) * (static_cast<double>(Inset) / Denominator);
			const double* Lift = CornerLift.Find(Key(Cur));
			Out.Add(FVector(Shifted.X, Shifted.Y, Z + (Lift ? *Lift : 0.0)));
		}
	}
}

// ---------------------------------------------------------------- slots

void UHexPathFXSubsystem::ApplyStyle(AActor* Actor, const FFXSlot& Slot, bool bClosedLoop, int32 Style) const
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	HexPathFX::SetBool(Actor, TEXT("ClosedLoop"), bClosedLoop);
	HexPathFX::SetBool(Actor, TEXT("ExpandedPath"), false);	// insets are computed here
	HexPathFX::SetBool(Actor, TEXT("ManualEditMode"), false);
	// End points are marked with reticles drawn by the same effect (same width / colour / see-through fill),
	// so the Path Tracer's sprite markers stay off.
	HexPathFX::ClearObject(Actor, TEXT("PathMarkStartClass"));
	HexPathFX::ClearObject(Actor, TEXT("PathMarkEndClass"));
	// No two-tone "seen through objects" pass: the line materials draw on top of everything in one colour
	// (see HexPathFX::DrawOnTop).
	HexPathFX::SetBool(Actor, TEXT("VisibleThroughObjects"), false);
	if (Style == 2)	// reticle: always solid
	{
		HexPathFX::SetBool(Actor, TEXT("DashedLine"), false);
	}
	if (Style == 1)	// goal ring, box on it
	{
		HexPathFX::SetColor(Actor, TEXT("PathColor"), Settings.GoalRingDoneColor);
		HexPathFX::SetBool(Actor, TEXT("DashedLine"), false);
	}
}

void UHexPathFXSubsystem::UpdateSlot(FFXSlot& Slot, UClass* Class, const TArray<FVector>& Points, bool bClosedLoop, int32 Style)
{
	if (!Class || Points.Num() < 2)
	{
		ClearSlot(Slot);
		return;
	}
	if (Slot.Actor.IsValid() && Slot.Class == Class && Slot.Style == Style)
	{
		if (HexPathFX::SamePoints(Slot.Points, Points))
		{
			return;	// unchanged
		}
		// Same effect, new shape: redraw in place (no destroy / respawn flicker).
		if (HexPathFX::CallDrawPath(Slot.Actor.Get(), Points))
		{
			Slot.Points = Points;
			return;
		}
	}
	ClearSlot(Slot);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Actor = World->SpawnActor<AActor>(Class, FTransform::Identity, Params);
	if (!Actor)
	{
		return;
	}
	ApplyStyle(Actor, Slot, bClosedLoop, Style);
	if (!HexPathFX::CallDrawPath(Actor, Points))
	{
		UE_LOG(LogHexSokoban, Warning, TEXT("Path FX: %s has no DrawPath(PathPoints) function - is it a BP_PathTracer child?"), *Class->GetName());
		Actor->Destroy();
		return;
	}
	Slot.Actor = Actor;
	Slot.Points = Points;
	Slot.Class = Class;
	Slot.Style = Style;
}

void UHexPathFXSubsystem::UpdateLoopSlots(TArray<FFXSlot>& Slots, UClass* Class, const TArray<TArray<FVector>>& Loops, int32 Style)
{
	for (int32 Index = Loops.Num(); Index < Slots.Num(); ++Index)
	{
		ClearSlot(Slots[Index]);
	}
	Slots.SetNum(Loops.Num());
	for (int32 Index = 0; Index < Loops.Num(); ++Index)
	{
		UpdateSlot(Slots[Index], Class, Loops[Index], true, Style);
	}
}

// ---------------------------------------------------------------- tick

void UHexPathFXSubsystem::Tick(float DeltaTime)
{
	if (GetWorld()->GetSubsystem<UHexBoardSubsystem>()->IsPerformanceMode()) bHintVisible = false;
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	UWorld* World = GetWorld();
	UHexBoardSubsystem* Board = World ? World->GetSubsystem<UHexBoardSubsystem>() : nullptr;
	UHexCardSubsystem* Cards = World ? World->GetSubsystem<UHexCardSubsystem>() : nullptr;
	const UHexGridMovementComponent* Mover = Board ? Board->GetPlayerMover() : nullptr;
	const AHexSokobanGameMode* GameMode = World ? World->GetAuthGameMode<AHexSokobanGameMode>() : nullptr;
	if (!Settings.bEnablePathFX || !Board || !Mover || !Mover->IsOnBoard())
	{
		if (HandSlots.Num() || CardSlots.Num() || GoalSlots.Num() || MoveSlot.Actor.IsValid() || HintSlot.Actor.IsValid()
			|| MoveReticle.Num() || BoxReticle.Num() || HintReticle.Num())
		{
			ClearAll();
		}
		return;
	}

	const FIntPoint PlayerCoord = Mover->GetCoord();
	const AHexTile* PlayerTile = Board->GetTileAt(PlayerCoord);
	const double Z = (PlayerTile ? PlayerTile->GetSurfaceLocation().Z : UHexGridLibrary::CoordToWorld(PlayerCoord).Z) + Settings.PathFXHeight;
	const bool bPlaying = !GameMode || GameMode->GetLevelState() == EHexLevelState::Playing;
	const bool bIdle = bPlaying && !Mover->IsMoving() && !Mover->IsMovementLocked();
	const bool bCards = Cards && Cards->IsActive();

	// ---- Orange: everything the hand can reach ----
	TArray<TArray<FVector>> Loops;
	if (bIdle && bCards)
	{
		TArray<FIntPoint> Cells = { PlayerCoord };
		for (const AHexTile* Tile : Cards->GetEnterableNeighbors(INDEX_NONE))
		{
			Cells.AddUnique(Tile->GetCoord());
		}
		if (Cells.Num() > 1)
		{
			BuildRegionLoops(Cells, Settings.HandRangeInset, Z, Loops, [this](const FIntPoint& Cell) { return GetCellLift(Cell); });
		}
	}
	UpdateLoopSlots(HandSlots, HexPathFX::LoadFXClass(Settings.HandRangeFXClass), Loops);

	// ---- Blue: what the previewed card reaches (subset of the orange region) ----
	Loops.Reset();
	const int32 Preview = bCards ? Cards->GetGuidePreviewIndex() : INDEX_NONE;
	TArray<AHexTile*> CardTargets;
	if (bIdle && Preview != INDEX_NONE)
	{
		CardTargets = Cards->GetEnterableNeighbors(Preview);
		if (CardTargets.Num() > 0)
		{
			TArray<FIntPoint> Cells = { PlayerCoord };
			for (const AHexTile* Tile : CardTargets)
			{
				Cells.AddUnique(Tile->GetCoord());
			}
			BuildRegionLoops(Cells, Settings.CardRangeInset, Z + 0.5, Loops, [this](const FIntPoint& Cell) { return GetCellLift(Cell); });
		}
	}
	UpdateLoopSlots(CardSlots, HexPathFX::LoadFXClass(Settings.CardRangeFXClass), Loops);

	// ---- Move / box path while a card points at a reachable tile ----
	TArray<FVector> MovePoints;
	TArray<FVector> BoxPoints;
	AHexTile* HoverTile = (bIdle && bCards && Cards->IsGuideFocused()) ? Cards->GetGuideHoverTile() : nullptr;
	if (HoverTile && CardTargets.Contains(HoverTile))
	{
		const FIntPoint Target = HoverTile->GetCoord();
		const double TargetLift = GetCellLift(Target);
		MovePoints = { HexPathFX::CellCenter(PlayerCoord, Z + 1.0 + GetCellLift(PlayerCoord)), HexPathFX::CellCenter(Target, Z + 1.0 + TargetLift) };
		if (Board->GetBoxAt(Target))
		{
			const FIntPoint Behind = Target + (Target - PlayerCoord);
			BoxPoints = { HexPathFX::CellCenter(Target, Z + 1.5 + TargetLift), HexPathFX::CellCenter(Behind, Z + 1.5 + GetCellLift(Behind)) };
		}
	}
	UpdateSlot(MoveSlot, HexPathFX::LoadFXClass(Settings.MovePathFXClass), MovePoints, false);
	UpdateSlot(BoxSlot, HexPathFX::LoadFXClass(Settings.BoxPathFXClass), BoxPoints, false);
	UpdateReticle(MoveReticle, HexPathFX::LoadFXClass(Settings.MovePathFXClass), MovePoints);
	UpdateReticle(BoxReticle, HexPathFX::LoadFXClass(Settings.BoxPathFXClass), BoxPoints);

	UpdateHint(bHintVisible && bIdle, Z + 0.8);
	UpdateGoalRings(Settings.bShowGoalRings, Z);
}

void UHexPathFXSubsystem::UpdateHint(bool bShow, double Z)
{
	UWorld* World = GetWorld();
	UHexBoardSubsystem* Board = World ? World->GetSubsystem<UHexBoardSubsystem>() : nullptr;
	const UHexGridMovementComponent* Mover = Board ? Board->GetPlayerMover() : nullptr;
	if (!bShow || !Board || !Mover)
	{
		ClearSlot(HintSlot);
		UpdateReticle(HintReticle, nullptr, TArray<FVector>());
		return;
	}
	// Re-plan only when the board or the hand changed.
	UHexCardSubsystem* Cards = World->GetSubsystem<UHexCardSubsystem>();
	const bool bCards = Cards && Cards->IsActive();
	TArray<FIntPoint> StateKey = { Mover->GetCoord() };
	for (const AHexBox* Box : Board->GetAllBoxes())
	{
		StateKey.Add(Box->GetCoord());
	}
	if (bCards)
	{
		for (const FHexCard& Card : Cards->GetHand())
		{
			StateKey.Add(FIntPoint(static_cast<int32>(Card.Suit), Card.Rank));
		}
	}
	if (StateKey != HintStateKey)
	{
		HintStateKey = StateKey;
		HintPoints.Reset();
		TArray<FIntPoint> Steps;
		bool bHasSteps = false;
		if (bCards)
		{
			// Planned with the actual hand; may leave the optimal line when the cards don't allow it.
			const EHexHintStatus Status = Cards->PlanHandHint(Steps);
			bHasSteps = Steps.Num() > 0;
			if (Status == EHexHintStatus::Detour)
			{
				// The hand can't follow an optimal line: the hint is the best route with these cards.
				Cards->OnGuideMessage.Broadcast(FText::FromString(HEXT("现有普通牌需要绕路，已为你换一条路线。")), nullptr);
			}
			else if (Status == EHexHintStatus::NoMove)
			{
				// No card in hand moves the player: suggest the discard zone.
				Cards->OnGuideMessage.Broadcast(Cards->IsDesigned()
					? FText::FromString(HEXT("普通牌暂时走不动了。可以试试技能牌，或按 Z 撤销。"))
					: FText::FromString(HEXT("普通牌暂时走不动了，可以拖到弃牌区换一张。")), nullptr);
			}
			else if (Status == EHexHintStatus::Lost)
			{
				// These cards can no longer win: show the best line left and say so.
				Cards->OnGuideMessage.Broadcast(FText::FromString(HEXT("仅靠现有普通牌找不到通关路线，可以试试技能牌，或按 Z 撤销。")), nullptr);
			}
			else if (Status == EHexHintStatus::None)
			{
				bHasSteps = Board->GetSolutionPlayerSteps(Steps, UHexSokobanSettings::Get().SolverMaxStates);
			}
		}
		else
		{
			bHasSteps = Board->GetSolutionPlayerSteps(Steps, UHexSokobanSettings::Get().SolverMaxStates);
		}
		if (bHasSteps && Steps.Num() > 0)
		{
			HintPoints.Add(HexPathFX::CellCenter(Mover->GetCoord(), Z));
			const int32 Count = FMath::Min(Steps.Num(), FMath::Max(1, UHexSokobanSettings::Get().HintSteps));
			for (int32 Index = 0; Index < Count; ++Index)
			{
				HintPoints.Add(HexPathFX::CellCenter(Steps[Index], Z));	// hint shows only while idle (no lifts)
			}
		}
	}
	UClass* HintClass = HexPathFX::LoadFXClass(UHexSokobanSettings::Get().HintPathFXClass);
	UpdateSlot(HintSlot, HintClass, HintPoints, false);
	UpdateReticle(HintReticle, HintClass, HintPoints);
}

void UHexPathFXSubsystem::UpdateReticle(TArray<FFXSlot>& Slots, UClass* Class, const TArray<FVector>& PathPoints)
{
	const float Scale = UHexSokobanSettings::Get().PathFXReticleScale;
	TArray<FVector> Ring;
	TArray<FVector> CrossA;
	TArray<FVector> CrossB;
	if (Class && PathPoints.Num() >= 2 && Scale > 0.f)
	{
		const FVector Center = PathPoints.Last() + FVector(0.0, 0.0, 0.5);
		const double Radius = UHexGridLibrary::GetTileSpacing() / FMath::Sqrt(3.0) * Scale;
		for (int32 Corner = 0; Corner < 6; ++Corner)
		{
			const double Angle = FMath::DegreesToRadians(30.0 + 60.0 * Corner);	// same orientation as the tiles
			Ring.Add(Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0));
		}
		const double Arm = Radius * 0.45;
		CrossA = { Center + FVector(-Arm, -Arm, 0.0), Center + FVector(Arm, Arm, 0.0) };
		CrossB = { Center + FVector(-Arm, Arm, 0.0), Center + FVector(Arm, -Arm, 0.0) };
	}
	Slots.SetNum(3);
	UpdateSlot(Slots[0], Class, Ring, true, 2);
	UpdateSlot(Slots[1], Class, CrossA, false, 2);
	UpdateSlot(Slots[2], Class, CrossB, false, 2);
}

void UHexPathFXSubsystem::UpdateGoalRings(bool bShow, double Z)
{
	UWorld* World = GetWorld();
	UHexBoardSubsystem* Board = World ? World->GetSubsystem<UHexBoardSubsystem>() : nullptr;
	UClass* Class = HexPathFX::LoadFXClass(UHexSokobanSettings::Get().GoalRingFXClass);
	TArray<AHexTile*> Goals;
	if (bShow && Board && Class)
	{
		for (AHexTile* Tile : Board->GetAllTiles())
		{
			if (Tile->IsGoal())
			{
				Goals.Add(Tile);
			}
		}
	}
	for (int32 Index = Goals.Num(); Index < GoalSlots.Num(); ++Index)
	{
		ClearSlot(GoalSlots[Index]);
	}
	GoalSlots.SetNum(Goals.Num());
	for (int32 Index = 0; Index < Goals.Num(); ++Index)
	{
		TArray<TArray<FVector>> Loops;
		BuildRegionLoops({ Goals[Index]->GetCoord() }, UHexSokobanSettings::Get().GoalRingInset, Z - 0.5, Loops,
			[this](const FIntPoint& Cell) { return GetCellLift(Cell); });
		const AHexBox* Box = Board->GetBoxAt(Goals[Index]->GetCoord());
		const bool bDone = Box && Box->IsVisuallyOnGoal();
		UpdateSlot(GoalSlots[Index], Class, Loops.Num() > 0 ? Loops[0] : TArray<FVector>(), true, bDone ? 1 : 0);
	}
}
