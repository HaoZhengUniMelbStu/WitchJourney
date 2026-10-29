#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HexCardSubsystem.h"
#include "HexBoardSubsystem.h"
#include "HexGridMovementComponent.h"
#include "HexGridLibrary.h"
#include "HexTile.h"
#include "HexBox.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexSkillIntegrationTest,"HexSokoban.Cards.SkillIntegration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHexSkillIntegrationTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues IV;
	IV.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);
	UHexBoardSubsystem* Board=World->GetSubsystem<UHexBoardSubsystem>();
	UHexCardSubsystem* Cards=World->GetSubsystem<UHexCardSubsystem>();
	for(int X=0;X<8;++X) for(int Y=0;Y<2;++Y)
	{
		const FIntPoint Coord(X,Y);
		AHexTile* Tile=World->SpawnActor<AHexTile>(UHexGridLibrary::CoordToWorld(Coord),FRotator::ZeroRotator);
		Tile->SetCardLabel(FHexCard(5,EHexSuit::Heart));
		Cards->Labels.Add(Coord,FHexCard(5,EHexSuit::Heart));
	}
	AHexBox* First=World->SpawnActor<AHexBox>(UHexGridLibrary::CoordToWorld(FIntPoint(1,0)),FRotator::ZeroRotator);
	AHexBox* Second=World->SpawnActor<AHexBox>(UHexGridLibrary::CoordToWorld(FIntPoint(2,0)),FRotator::ZeroRotator);
	Board->RebuildBoard();
	AActor* Player=World->SpawnActor<AActor>();
	USceneComponent* Root=NewObject<USceneComponent>(Player); Player->SetRootComponent(Root); Root->RegisterComponent();
	UHexGridMovementComponent* Mover=NewObject<UHexGridMovementComponent>(Player); Mover->RegisterComponent();
	Mover->bUseCharacterMovement=false; Mover->TeleportToCoord(FIntPoint(0,0)); Board->RegisterMover(Mover);
	Cards->bActive=true; Cards->bDesigned=true;
	Cards->State=FHexCardState(); Cards->State.Hand.Add(FHexCard(5,EHexSuit::Heart));
	Cards->InitializeSkillDeck();
	TestEqual(TEXT("Only map suits enter deck"),Cards->State.SkillPile.Num(),3);
	FHexCard Top; Cards->PeekNextSkill(Top);
	Cards->SelectCard(0);
	TestEqual(TEXT("Exact normal move succeeds"),Mover->TryMove(EHexDirection::ForwardRight),EHexMoveResult::Moved);
	Mover->FinishStepImmediately(true);
	TestEqual(TEXT("Reward one skill"),Cards->State.Hand.Num(),1);
	TestTrue(TEXT("Reward is the visible top"),Cards->State.Hand[0].SameFace(Top));
	FHexCard Next; Cards->PeekNextSkill(Next);
	Board->UndoLastMove(); Cards->UndoLastPlay();
	TestTrue(TEXT("Undo restores original hand"),Cards->State.Hand[0].SameFace(FHexCard(5,EHexSuit::Heart)));
	FHexCard Restored; Cards->PeekNextSkill(Restored);
	TestTrue(TEXT("Undo restores visible top"),Restored.SameFace(Top));
	Cards->SelectCard(0); Mover->TryMove(EHexDirection::ForwardRight); Mover->FinishStepImmediately(true);
	Cards->PeekNextSkill(Restored);
	TestTrue(TEXT("Redo cannot reroll next card"),Restored.SameFace(Next));
	Board->UndoLastMove(); Cards->UndoLastPlay();
	FHexCard Wind(11,EHexSuit::Heart); Wind.Kind=EHexCardKind::Skill;
	Cards->State.Hand={Wind}; Cards->State.SkillPile.RemoveAll([&](const FHexCard& C){return C.SameFace(Wind);});
	Cards->SelectCard(0);
	TestFalse(TEXT("J empty neighbouring tile is not highlighted"),Cards->GetEnterableNeighbors(0).Contains(Board->GetTileAt(FIntPoint(0,1))));
	TestEqual(TEXT("J empty cast is refused"),Mover->TryPlaySkillAt(FIntPoint(0,1)),EHexMoveResult::BlockedNoCard);
	TestEqual(TEXT("Invalid J keeps its card"),Cards->State.Hand.Num(),1);
	const FVector CastLocation=Player->GetActorLocation();
	TestEqual(TEXT("J chain committed"),Mover->TryPlaySkillAt(FIntPoint(1,0)),EHexMoveResult::Pushed);
	for(int I=0;I<30;++I) { First->Tick(.04f); Second->Tick(.04f); Mover->TickComponent(.04f,LEVELTICK_All,nullptr); }
	TestEqual(TEXT("J costs four steps"),Board->GetMoveCount(),4);
	TestTrue(TEXT("First box occupies second original cell"),Board->GetBoxAt(FIntPoint(2,0))==First);
	TestTrue(TEXT("Second box advances one"),Board->GetBoxAt(FIntPoint(3,0))==Second);
	TestEqual(TEXT("J leaves player in original cell"),Mover->GetCoord(),FIntPoint(0,0));
	TestTrue(TEXT("J keeps visual location"),Player->GetActorLocation().Equals(CastLocation));
	TestEqual(TEXT("Used skill waits in discard"),Cards->GetSkillDiscardCount(),1);
	TestFalse(TEXT("No J in current round"),Cards->State.SkillPile.ContainsByPredicate([](const FHexCard& C){return C.Rank==11;}));
	Board->UndoLastMove(); Cards->UndoLastPlay();
	TestTrue(TEXT("Undo first box"),Board->GetBoxAt(FIntPoint(1,0))==First);
	TestTrue(TEXT("Undo second box"),Board->GetBoxAt(FIntPoint(2,0))==Second);
	TestEqual(TEXT("Undo restores skill"),Cards->State.Hand.Num(),1);
	TestEqual(TEXT("Undo clears discard"),Cards->GetSkillDiscardCount(),0);
	FHexCard Teleport(12,EHexSuit::Heart); Teleport.Kind=EHexCardKind::Skill;
	Cards->State.Hand={Teleport}; Cards->State.SkillPile.RemoveAll([&](const FHexCard& C){return C.SameFace(Teleport);});
	Cards->SelectCard(0);
	TestEqual(TEXT("Q bypasses boxes"),Mover->TryPlaySkillAt(FIntPoint(7,0)),EHexMoveResult::Moved);
	TestEqual(TEXT("Q arrives instantly"),Mover->GetCoord(),FIntPoint(7,0));
	// Q plays its cast / blink animation in place (no walk); it ends on its own.
	for(int I=0;I<80;++I) Mover->TickComponent(.04f,LEVELTICK_All,nullptr);
	TestFalse(TEXT("Q blink animation completes"),Mover->IsMoving());
	TestEqual(TEXT("Q stays on target"),Mover->GetCoord(),FIntPoint(7,0));
	TestEqual(TEXT("Q costs six steps"),Board->GetMoveCount(),6);
	Board->UndoLastMove(); Cards->UndoLastPlay();
	TestEqual(TEXT("Q undo position"),Mover->GetCoord(),FIntPoint(0,0));
	FHexCard Flight(13,EHexSuit::Heart); Flight.Kind=EHexCardKind::Skill;
	Cards->State.Hand={Flight}; Cards->bHasSelection=false; Cards->SelectCard(0);
	TestFalse(TEXT("K occupied endpoint is not highlighted"),Cards->GetEnterableNeighbors(0).Contains(Board->GetTileAt(FIntPoint(1,0))));
	TestEqual(TEXT("K cannot push endpoint"),Mover->TryPlaySkillAt(FIntPoint(1,0)),EHexMoveResult::BlockedNoCard);
	TestEqual(TEXT("Blocked K fails without paying"),Mover->TryPlaySkillAt(FIntPoint(6,0)),EHexMoveResult::BlockedNoCard);
	TestEqual(TEXT("Invalid target keeps card"),Cards->State.Hand.Num(),1);
	TestEqual(TEXT("Invalid target keeps turn count"),Board->GetMoveCount(),0);
	Mover->TeleportToCoord(FIntPoint(0,1));
	TestEqual(TEXT("K long ray move"),Mover->TryPlaySkillAt(FIntPoint(6,1)),EHexMoveResult::Moved);
	TestTrue(TEXT("K is animated, not teleport"),Mover->IsMoving());
	for(int I=0;I<30;++I) Mover->TickComponent(.04f,LEVELTICK_All,nullptr);
	TestEqual(TEXT("K arrived"),Mover->GetCoord(),FIntPoint(6,1));
	TestFalse(TEXT("K animation completed"),Mover->IsMoving());
	TestEqual(TEXT("K costs two steps"),Board->GetMoveCount(),2);
	Board->UndoLastMove(); Cards->UndoLastPlay();
	TestEqual(TEXT("K undo"),Mover->GetCoord(),FIntPoint(0,1));
	TestEqual(TEXT("Undo refunds full skill cost"),Board->GetMoveCount(),0);
	// Exhaust a full round while returning consumed cards; no returned card appears before exhaustion.
	Cards->State=FHexCardState(); Cards->InitializeSkillDeck();
	TSet<int32> Seen;
	for(int I=0;I<3;++I)
	{
		FHexCard Draw; Cards->PeekNextSkill(Draw);
		TestFalse(TEXT("No duplicate before round ends"),Seen.Contains(Draw.Rank)); Seen.Add(Draw.Rank);
		Cards->AwardSkill(); Cards->State.Hand.Reset(); Cards->State.SkillDiscard.Add(Draw);
		if(Cards->State.SkillPile.IsEmpty()) Cards->PrepareNextSkill();
	}
	TestEqual(TEXT("Every skill type drawn this round"),Seen.Num(),3);
	TestTrue(TEXT("Next round has recycled cards"),Cards->GetSkillPileCount()>0);
	// Loss is based on legal plays, including skills, never the normal route forecast.
	FText LossReason;
	Cards->State.Hand.Reset();
	TestTrue(TEXT("Empty hand cannot move"), Cards->IsOutOfMoves(LossReason));
	Cards->State.Hand={FHexCard(2,EHexSuit::Spade)};
	TestTrue(TEXT("Nonmatching hand cannot move"), Cards->IsOutOfMoves(LossReason));
	Cards->State.Hand.Add(Teleport);
	TestFalse(TEXT("Only a remote Q play is enough to keep playing"), Cards->IsOutOfMoves(LossReason));
	Cards->State.Hand={FHexCard(5,EHexSuit::Heart)};
	TestFalse(TEXT("Ordinary legal play also keeps playing"), Cards->IsOutOfMoves(LossReason));
	Board->EnablePerformanceMode();
	TestTrue(TEXT("Large-map normal card keeps reachable target feedback"), Cards->GetEnterableNeighbors(0).Num()>0);
	Cards->State.Hand={Teleport};
	TestTrue(TEXT("Large-map Q keeps remote target feedback"), Cards->GetEnterableNeighbors(0).Num()>0);
	TestTrue(TEXT("Large-map playable cards remain playable in UI"), Cards->IsCardPlayableNow(0));
	TestNull(TEXT("Performance mode never builds a state graph"), Board->GetStateSpace());
	TArray<FIntPoint> DisabledHint;
	TestFalse(TEXT("Performance mode disables geometric hint fallback"), Board->GetSolutionPlayerSteps(DisabledHint));
	TestEqual(TEXT("Performance mode disables card-aware search"), Cards->PlanHandHint(DisabledHint), EHexHintStatus::None);
	Cards->State.Hand.Reset();
	TestFalse(TEXT("Large map has no forced failure even with empty hand"), Cards->IsOutOfMoves(LossReason));
	Cards->InitializeSkillDeck();
	for (int32 Round=0; Round<5; ++Round)
	{
		TSet<int32> RoundRanks;
		const int32 RoundSize=Cards->State.SkillPile.Num();
		TestTrue(TEXT("Next large-map round is nonempty"),RoundSize>0);
		for (int32 I=0; I<RoundSize; ++I)
		{
			FHexCard Draw; TestTrue(TEXT("Repeated rounds always have a top card"),Cards->PeekNextSkill(Draw));
			RoundRanks.Add(Draw.Rank);
			Cards->AwardSkill(); Cards->State.Hand.Reset(); Cards->State.SkillDiscard.Add(Draw);
			if (Cards->State.SkillPile.IsEmpty()) Cards->PrepareNextSkill();
		}
		TestEqual(TEXT("Physical deck remains unique in each large-map round"),RoundRanks.Num(),RoundSize);
	}
	// Large-map returns preserve the committed top and vary their hidden insertion position.
	Cards->InitializeSkillDeck();
	const TArray<FHexCard> OriginalPile=Cards->State.SkillPile;
	TSet<int32> ReturnPositions;
	for(int32 I=0;I<100;++I)
	{
		Cards->State.SkillPile=OriginalPile;
		const FHexCard Returned=Cards->State.SkillPile.Pop();
		const FHexCard RevealedTop=Cards->State.SkillPile[0];
		Cards->ReturnToPile(Returned);
		TestTrue(TEXT("Returned skill never replaces revealed top"),Cards->State.SkillPile[0].SameFace(RevealedTop));
		TestEqual(TEXT("Return conserves physical card count"),Cards->State.SkillPile.Num(),OriginalPile.Num());
		ReturnPositions.Add(Cards->State.SkillPile.IndexOfByPredicate([&](const FHexCard& C){return C.SameFace(Returned);}));
	}
	TestTrue(TEXT("Returns do not always insert second"),ReturnPositions.Num()>1);
	// Full availability: verify the two-stage lottery independently of physical depletion.
	TArray<FHexCard> WeightedPool;
	for(int32 Rank=11;Rank<=13;++Rank) for(int32 Suit=0;Suit<3;++Suit)
	{
		FHexCard C(Rank,static_cast<EHexSuit>(Suit)); C.Kind=EHexCardKind::Skill; WeightedPool.Add(C);
	}
	Cards->SkillSuitWeights[0]=1; Cards->SkillSuitWeights[1]=3; Cards->SkillSuitWeights[2]=6; Cards->SkillSuitWeights[3]=0;
	Cards->State.SkillRandom.Initialize(20261027);
	int32 RankCounts[3]={0,0,0}, SuitCounts[4]={0,0,0,0};
	for(int32 I=0;I<12000;++I)
	{
		Cards->State.SkillPile=WeightedPool; Cards->PrepareNextSkill();
		const FHexCard C=Cards->State.SkillPile[0]; ++RankCounts[C.Rank-11]; ++SuitCounts[static_cast<int32>(C.Suit)];
	}
	TestTrue(TEXT("J weighted 40 percent"),FMath::Abs(RankCounts[0]/12000.f-.4f)<.025f);
	TestTrue(TEXT("Q weighted 20 percent"),FMath::Abs(RankCounts[1]/12000.f-.2f)<.025f);
	TestTrue(TEXT("K weighted 40 percent"),FMath::Abs(RankCounts[2]/12000.f-.4f)<.025f);
	TestTrue(TEXT("Terrain weighting 10 percent"),FMath::Abs(SuitCounts[0]/12000.f-.1f)<.025f);
	TestTrue(TEXT("Terrain weighting 30 percent"),FMath::Abs(SuitCounts[1]/12000.f-.3f)<.025f);
	TestTrue(TEXT("Terrain weighting 60 percent"),FMath::Abs(SuitCounts[2]/12000.f-.6f)<.025f);
	TestEqual(TEXT("Absent terrain suit never dealt"),SuitCounts[3],0);
	const FHexCardState Snapshot=Cards->State;
	Cards->PrepareNextSkill(); const FHexCard Expected=Cards->State.SkillPile[0];
	Cards->State=Snapshot; Cards->PrepareNextSkill();
	TestTrue(TEXT("Restoring undo state reproduces weighted draw"),Cards->State.SkillPile[0].SameFace(Expected));
	World->DestroyWorld(false);
	return true;
}
#endif
