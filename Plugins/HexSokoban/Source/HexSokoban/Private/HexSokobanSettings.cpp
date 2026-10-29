#include "HexSokobanSettings.h"
#include "HexTile.h"
#include "HexGoalTile.h"
#include "HexTerrainTile.h"

FName UHexSokobanSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

UMaterialInterface* UHexSokobanSettings::ResolveColorMaterial(UMaterialInterface* Override)
{
	if (Override)
	{
		return Override;
	}
	const UHexSokobanSettings& Settings = Get();
	if (!Settings.DefaultColorMaterial.IsNull())
	{
		if (UMaterialInterface* Material = Settings.DefaultColorMaterial.LoadSynchronous())
		{
			return Material;
		}
	}
	static const FSoftObjectPath BasicShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	return Cast<UMaterialInterface>(BasicShapeMaterial.TryLoad());
}

UClass* UHexSokobanSettings::ResolveTileClass(bool bGoal, EHexSuit Suit)
{
	const UHexSokobanSettings& Settings = Get();
	UClass* Class = nullptr;
	if (const TSoftClassPtr<AActor>* Terrain = Settings.SuitTileClasses.Find(Suit))
	{
		Class = Terrain->IsNull() ? nullptr : Terrain->LoadSynchronous();
	}
	if (bGoal)
	{
		// A goal keeps its terrain (flagged as goal when spawned) so players still see the ground it is.
		if (Class && Class->IsChildOf(AHexTerrainTile::StaticClass()))
		{
			return Class;
		}
		Class = Settings.GoalTileClass.IsNull() ? nullptr : Settings.GoalTileClass.LoadSynchronous();
		return Class ? Class : AHexGoalTile::StaticClass();
	}
	if (!Class)
	{
		Class = Settings.TileClass.IsNull() ? nullptr : Settings.TileClass.LoadSynchronous();
	}
	return Class ? Class : AHexTile::StaticClass();
}

UStaticMesh* UHexSokobanSettings::ResolveHexMesh()
{
	const UHexSokobanSettings& Settings = Get();
	return Settings.DefaultHexMesh.IsNull() ? nullptr : Settings.DefaultHexMesh.LoadSynchronous();
}
