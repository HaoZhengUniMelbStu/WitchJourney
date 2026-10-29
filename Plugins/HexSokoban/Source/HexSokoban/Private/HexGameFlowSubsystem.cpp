#include "HexGameFlowSubsystem.h"
#include "HexLocalization.h"
#include "HexSokobanSettings.h"
#include "HexSokobanTypes.h"
#include "JsonObjectConverter.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace HexFlow
{
	const TCHAR* SaveSlot = TEXT("HexProgress");
}

bool FHexLevelDefinition::IsPlayable(FString* OutReason) const
{
	auto Fail = [OutReason](const TCHAR* Reason)
	{
		if (OutReason)
		{
			*OutReason = Reason;
		}
		return false;
	};
	if (Tiles.Num() == 0)
	{
		return Fail(HEXT("请先放置地台。"));								// no tiles yet
	}
	if (!bHasPlayerStart || !FindTile(PlayerStart) || FindTile(PlayerStart)->IsScenery())
	{
		return Fail(HEXT("请在可行走的地台上放置玩家起点。"));		// no player start
	}
	const int32 Goals = GetNumGoals();
	if (Goals == 0)
	{
		return Fail(HEXT("请至少放置一个终点。"));								// no goal
	}
	if (Boxes.Num() < Goals)
	{
		return Fail(HEXT("箱子数量不足，请补上箱子或减少终点。"));						// fewer boxes than goals
	}
	for (const FIntPoint& Box : Boxes)
	{
		if (!FindTile(Box) || FindTile(Box)->IsScenery() || Box == PlayerStart)
		{
			return Fail(HEXT("箱子要放在可行走的地台上，也不能占用玩家起点。"));	// box / start misplaced
		}
	}
	return true;
}

// ---------------------------------------------------------------- setup

void UHexGameFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadLevels();
	LoadSave();
}

UHexGameFlowSubsystem* UHexGameFlowSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	return Instance ? Instance->GetSubsystem<UHexGameFlowSubsystem>() : nullptr;
}

FString UHexGameFlowSubsystem::GetBuiltInDir()
{
	return FPaths::ProjectContentDir() / TEXT("HexSokoban/BuiltInLevels");
}

FString UHexGameFlowSubsystem::GetCustomDir()
{
	return FPaths::ProjectSavedDir() / TEXT("HexLevels");
}

// ---------------------------------------------------------------- level files

bool UHexGameFlowSubsystem::LoadLevelFile(const FString& File, bool bBuiltIn, FHexLevelEntry& OutEntry) const
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *File))
	{
		return false;
	}
	FHexLevelDefinition Definition;
	if (!FJsonObjectConverter::JsonObjectStringToUStruct(Text, &Definition, 0, 0))
	{
		UE_LOG(LogHexSokoban, Warning, TEXT("Level file %s is not valid level JSON; skipped."), *File);
		return false;
	}
	if (Definition.LevelId.IsEmpty())
	{
		Definition.LevelId = FPaths::GetBaseFilename(File);
	}
	Definition.bPerformanceMode = Definition.NeedsPerformanceMode();
	OutEntry.Definition = MoveTemp(Definition);
	OutEntry.bBuiltIn = bBuiltIn;
	OutEntry.SourceFile = File;
	return true;
}

bool UHexGameFlowSubsystem::WriteLevelFile(const FString& File, const FHexLevelDefinition& Definition) const
{
	FString Text;
	FHexLevelDefinition SavedDefinition = Definition;
	SavedDefinition.bPerformanceMode = Definition.NeedsPerformanceMode();
	if (!FJsonObjectConverter::UStructToJsonObjectString(SavedDefinition, Text))
	{
		return false;
	}
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
	return FFileHelper::SaveStringToFile(Text, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void UHexGameFlowSubsystem::ReloadLevels()
{
	Levels.Reset();
	auto ReadDir = [this](const FString& Dir, bool bBuiltIn)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.hexlevel")), true, false);
		Files.Sort();
		TArray<FHexLevelEntry> Found;
		for (const FString& Name : Files)
		{
			FHexLevelEntry Entry;
			if (LoadLevelFile(Dir / Name, bBuiltIn, Entry))
			{
				Found.Add(MoveTemp(Entry));
			}
		}
		if (!bBuiltIn)
		{
			Found.StableSort([](const FHexLevelEntry& A, const FHexLevelEntry& B) { return A.Definition.CreatedUtc < B.Definition.CreatedUtc; });
		}
		Levels.Append(MoveTemp(Found));
	};
	ReadDir(GetBuiltInDir(), true);
	ReadDir(GetCustomDir(), false);
	UE_LOG(LogHexSokoban, Log, TEXT("Levels: %d loaded (%s, %s)."), Levels.Num(), *GetBuiltInDir(), *GetCustomDir());
}

int32 UHexGameFlowSubsystem::FindLevelIndex(const FString& LevelId) const
{
	return LevelId.IsEmpty() ? INDEX_NONE
		: Levels.IndexOfByPredicate([&LevelId](const FHexLevelEntry& Entry) { return Entry.Definition.LevelId == LevelId; });
}

const FHexLevelDefinition* UHexGameFlowSubsystem::GetCurrentDefinition() const
{
	if (TestPlayDefinition.IsSet())
	{
		return &TestPlayDefinition.GetValue();
	}
	const int32 Index = FindLevelIndex(CurrentLevelId);
	return Levels.IsValidIndex(Index) ? &Levels[Index].Definition : nullptr;
}

bool UHexGameFlowSubsystem::HasNextLevel() const
{
	if (TestPlayDefinition.IsSet())
	{
		return false;
	}
	const int32 Index = FindLevelIndex(CurrentLevelId);
	return Index != INDEX_NONE && Levels.IsValidIndex(Index + 1);
}

FString UHexGameFlowSubsystem::SaveCustomLevel(FHexLevelDefinition& Definition)
{
	const int32 Existing = FindLevelIndex(Definition.LevelId);
	if (Definition.LevelId.IsEmpty() || (Levels.IsValidIndex(Existing) && Levels[Existing].bBuiltIn))
	{
		Definition.LevelId = FGuid::NewGuid().ToString(EGuidFormats::Digits);	// new level (or a copy of a built-in one)
	}
	if (Definition.CreatedUtc.IsEmpty())
	{
		Definition.CreatedUtc = FDateTime::UtcNow().ToIso8601();
	}
	const FString File = GetCustomDir() / (Definition.LevelId + TEXT(".hexlevel"));
	if (!WriteLevelFile(File, Definition))
	{
		return FString();
	}
	ReloadLevels();
	return File;
}

FString UHexGameFlowSubsystem::SaveBuiltInLevel(FHexLevelDefinition& Definition)
{
#if WITH_EDITOR
	const int32 Existing = FindLevelIndex(Definition.LevelId);
	if (!Levels.IsValidIndex(Existing) || !Levels[Existing].bBuiltIn)
	{
		// New built-in: next number in the play order + a readable id.
		int32 Count = 0;
		for (const FHexLevelEntry& Entry : Levels)
		{
			Count += Entry.bBuiltIn ? 1 : 0;
		}
		Definition.LevelId = FString::Printf(TEXT("%02d_Level"), Count + 1);
		// Deleted levels can leave gaps; never reuse an existing level's identity or file.
		while (FindLevelIndex(Definition.LevelId) != INDEX_NONE ||
			IFileManager::Get().FileExists(*(GetBuiltInDir() / (Definition.LevelId + TEXT(".hexlevel")))))
		{
			Definition.LevelId = FString::Printf(TEXT("%02d_Level"), ++Count + 1);
		}
	}
	if (Definition.CreatedUtc.IsEmpty())
	{
		Definition.CreatedUtc = FDateTime::UtcNow().ToIso8601();
	}
	const FString File = Levels.IsValidIndex(Existing) && Levels[Existing].bBuiltIn
		? Levels[Existing].SourceFile
		: GetBuiltInDir() / (Definition.LevelId + TEXT(".hexlevel"));
	if (!WriteLevelFile(File, Definition))
	{
		return FString();
	}
	ReloadLevels();
	return File;
#else
	return FString();
#endif
}

bool UHexGameFlowSubsystem::DeleteCustomLevel(const FString& LevelId)
{
	const int32 Index = FindLevelIndex(LevelId);
	if (!Levels.IsValidIndex(Index) || Levels[Index].bBuiltIn)
	{
		return false;
	}
	const bool bDeleted = IFileManager::Get().Delete(*Levels[Index].SourceFile);
	ReloadLevels();
	return bDeleted;
}

// ---------------------------------------------------------------- progress

void UHexGameFlowSubsystem::LoadSave()
{
	Save = Cast<UHexSaveGame>(UGameplayStatics::LoadGameFromSlot(HexFlow::SaveSlot, 0));
	if (!Save)
	{
		Save = Cast<UHexSaveGame>(UGameplayStatics::CreateSaveGameObject(UHexSaveGame::StaticClass()));
	}
}

void UHexGameFlowSubsystem::WriteSave()
{
	if (Save)
	{
		UGameplayStatics::SaveGameToSlot(Save, HexFlow::SaveSlot, 0);
	}
}

FHexLevelRecord UHexGameFlowSubsystem::GetRecord(const FString& LevelId) const
{
	const FHexLevelRecord* Record = Save ? Save->Records.Find(LevelId) : nullptr;
	return Record ? *Record : FHexLevelRecord();
}

void UHexGameFlowSubsystem::RecordResult(const FString& LevelId, int32 Stars, int32 Turns)
{
	if (!Save || LevelId.IsEmpty() || TestPlayDefinition.IsSet())
	{
		return;
	}
	FHexLevelRecord& Record = Save->Records.FindOrAdd(LevelId);
	Record.BestTurns = Record.bCompleted ? FMath::Min(Record.BestTurns, Turns) : Turns;
	Record.BestStars = FMath::Max(Record.BestStars, Stars);
	Record.bCompleted = true;
	WriteSave();
}

// ---------------------------------------------------------------- navigation

void UHexGameFlowSubsystem::OpenMap(const TSoftObjectPtr<UWorld>& Map)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || Map.IsNull())
	{
		UE_LOG(LogHexSokoban, Warning, TEXT("Flow: map not set (Project Settings > Hex Sokoban > Levels)."));
		return;
	}
	UGameplayStatics::SetGamePaused(World, false);
	UGameplayStatics::OpenLevelBySoftObjectPtr(World, Map);
}

void UHexGameFlowSubsystem::PlayLevel(const FString& LevelId)
{
	if (FindLevelIndex(LevelId) == INDEX_NONE)
	{
		return;
	}
	TestPlayDefinition.Reset();
	CurrentLevelId = LevelId;
	if (Save)
	{
		Save->LastPlayedLevelId = LevelId;
		WriteSave();
	}
	OpenMap(UHexSokobanSettings::Get().PlayMap);
}

void UHexGameFlowSubsystem::PlayNextLevel()
{
	const int32 Index = FindLevelIndex(CurrentLevelId);
	if (TestPlayDefinition.IsSet())
	{
		ReturnToEditor();
	}
	else if (Levels.IsValidIndex(Index + 1))
	{
		PlayLevel(Levels[Index + 1].Definition.LevelId);
	}
	else
	{
		OpenMenu(EHexMenuScreen::LevelSelect);	// finished the run
	}
}

void UHexGameFlowSubsystem::OpenMenu(EHexMenuScreen Screen)
{
	TestPlayDefinition.Reset();
	EditorDraft.Reset();
	RequestedScreen = Screen;
	OpenMap(UHexSokobanSettings::Get().MenuMap);
}

void UHexGameFlowSubsystem::OpenEditor(const FString& LevelId)
{
	TestPlayDefinition.Reset();
	EditorDraft.Reset();
	EditorLevelId = LevelId;
	OpenMap(UHexSokobanSettings::Get().EditorMap);
}

void UHexGameFlowSubsystem::ReturnToEditor()
{
	TestPlayDefinition.Reset();
	OpenMap(UHexSokobanSettings::Get().EditorMap);
}

void UHexGameFlowSubsystem::TestPlay(const FHexLevelDefinition& Definition)
{
	TestPlayDefinition = Definition;
	EditorDraft = Definition;
	CurrentLevelId = Definition.LevelId;
	OpenMap(UHexSokobanSettings::Get().PlayMap);
}

void UHexGameFlowSubsystem::QuitGame()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	UKismetSystemLibrary::QuitGame(World, World ? World->GetFirstPlayerController() : nullptr, EQuitPreference::Quit, false);
}
