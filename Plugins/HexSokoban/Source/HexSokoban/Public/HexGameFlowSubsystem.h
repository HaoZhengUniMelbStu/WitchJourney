// Game life cycle (one per game instance): the level list (built-in JSON + player JSON), the save game with
// the best result per level, and the hops between the main menu, level select, play and the level editor.
//
//  Built-in levels: Content/HexSokoban/BuiltInLevels/*.json, played in file-name order (prefix 01_, 02_ ...).
//                   Design them with the in-game level editor and "save as built-in" (editor builds only).
//  Player levels:   Saved/HexLevels/*.json, listed after the built-ins by creation time.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/SaveGame.h"
#include "HexLevelTypes.h"
#include "HexGameFlowSubsystem.generated.h"

UCLASS()
class HEXSOKOBAN_API UHexSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	TMap<FString, FHexLevelRecord> Records;

	UPROPERTY(SaveGame)
	FString LastPlayedLevelId;
};

/** Which screen the menu map opens on. */
UENUM(BlueprintType)
enum class EHexMenuScreen : uint8
{
	Main,
	LevelSelect
};

UCLASS()
class HEXSOKOBAN_API UHexGameFlowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static UHexGameFlowSubsystem* Get(const UObject* WorldContext);

	// ---- Levels ----

	/** Re-read every JSON level from disk. */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Flow")
	void ReloadLevels();

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Flow")
	TArray<FHexLevelEntry> GetLevels() const { return Levels; }

	int32 GetNumLevels() const { return Levels.Num(); }
	/** Last saved play selection; deleted/missing levels fall back to the first available level. */
	int32 GetMenuBackdropLevelIndex() const
	{
		const int32 Index = Save ? FindLevelIndex(Save->LastPlayedLevelId) : INDEX_NONE;
		return Index != INDEX_NONE ? Index : (Levels.IsEmpty() ? INDEX_NONE : 0);
	}
	const FHexLevelEntry* GetLevelEntry(int32 Index) const { return Levels.IsValidIndex(Index) ? &Levels[Index] : nullptr; }

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Flow")
	int32 FindLevelIndex(const FString& LevelId) const;

	/** The level being played (or edited); null when a legacy map was opened directly. */
	const FHexLevelDefinition* GetCurrentDefinition() const;

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Flow")
	int32 GetCurrentLevelIndex() const { return FindLevelIndex(CurrentLevelId); }

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Flow")
	bool HasNextLevel() const;

	/** Save a player level (new GUID when LevelId is empty). Returns the file written, empty on failure. */
	FString SaveCustomLevel(FHexLevelDefinition& Definition);

	/** Editor builds only: write into Content/HexSokoban/BuiltInLevels (ships with the game). */
	FString SaveBuiltInLevel(FHexLevelDefinition& Definition);

	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Flow")
	bool DeleteCustomLevel(const FString& LevelId);

	/** Level the editor should open (empty = new level). */
	FString EditorLevelId;

	/** Definition handed straight to the play map (editor test play: plays unsaved edits). */
	TOptional<FHexLevelDefinition> TestPlayDefinition;

	/** Unsaved editor work, kept across a test play so the editor comes back exactly as it was. */
	TOptional<FHexLevelDefinition> EditorDraft;

	// ---- Progress ----

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Flow")
	FHexLevelRecord GetRecord(const FString& LevelId) const;

	/** Keep the best stars / turns. */
	void RecordResult(const FString& LevelId, int32 Stars, int32 Turns);

	// ---- Navigation ----

	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Flow")
	void PlayLevel(const FString& LevelId);

	/** Make LevelId current without travelling (the play map opened directly). */
	void SetCurrentLevel(const FString& LevelId) { CurrentLevelId = LevelId; TestPlayDefinition.Reset(); }

	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Flow")
	void PlayNextLevel();

	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Flow")
	void OpenMenu(EHexMenuScreen Screen);

	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Flow")
	void OpenEditor(const FString& LevelId);

	/** Editor "test play": play Definition without saving it; Esc / win returns to the editor. */
	void TestPlay(const FHexLevelDefinition& Definition);

	/** Back from a test play to the editor with the unsaved draft. */
	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Flow")
	void ReturnToEditor();

	UFUNCTION(BlueprintCallable, Category = "Hex Sokoban|Flow")
	void QuitGame();

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Flow")
	EHexMenuScreen GetRequestedMenuScreen() const { return RequestedScreen; }

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Flow")
	bool IsTestPlaying() const { return TestPlayDefinition.IsSet(); }

	UFUNCTION(BlueprintPure, Category = "Hex Sokoban|Flow")
	FString GetCurrentLevelId() const { return CurrentLevelId; }

	static FString GetBuiltInDir();
	static FString GetCustomDir();

private:
	bool LoadLevelFile(const FString& File, bool bBuiltIn, FHexLevelEntry& OutEntry) const;
	bool WriteLevelFile(const FString& File, const FHexLevelDefinition& Definition) const;
	void LoadSave();
	void WriteSave();
	void OpenMap(const TSoftObjectPtr<UWorld>& Map);

	UPROPERTY(Transient)
	TArray<FHexLevelEntry> Levels;

	UPROPERTY(Transient)
	TObjectPtr<UHexSaveGame> Save;

	FString CurrentLevelId;
	EHexMenuScreen RequestedScreen = EHexMenuScreen::Main;
};
