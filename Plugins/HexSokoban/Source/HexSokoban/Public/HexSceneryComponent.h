#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HexLevelTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "HexSceneryComponent.generated.h"
class UStaticMesh;
class UMaterialInterface;
class AHexTile;
class UStaticMeshComponent;
/** Visual-only blockout scenery. Never participates in board occupancy or collision. */
UCLASS()
class HEXSOKOBAN_API UHexSceneryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHexSceneryComponent();
	void Build(AHexTile* Tile,const FHexTileDef& Definition);
	void RefreshConnections();
	void SetRotationSteps(int32 Steps);
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	bool IsMountain() const { return Definition.Suit==EHexSuit::Spade && Definition.IsScenery(); }
	virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
private:
	friend class FHexSceneryTest;
	void RebuildVisuals(AHexTile* Tile,const FHexTileDef& Def);
	FHexTileDef Definition;
	uint8 NeighborMask=255;
	UPROPERTY() TArray<TObjectPtr<UStaticMesh>> NormalTrees;
	UPROPERTY() TObjectPtr<UStaticMesh> ForestTree;
	UPROPERTY() TObjectPtr<UStaticMesh> ShipMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CastleMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CityMesh;
	UPROPERTY() TArray<TObjectPtr<UStaticMesh>> AdditionalTownMeshes;
	UPROPERTY() TObjectPtr<UStaticMesh> CottageMesh;
	UPROPERTY() TArray<TObjectPtr<UStaticMesh>> Rocks;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> FloatingShip;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RotatingModel;
	FVector ModelPivot=FVector::ZeroVector;
	FVector ShipRestPosition=FVector::ZeroVector;
	FRotator ShipRestRotation=FRotator::ZeroRotator;
	double FloatTime=0;
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UMaterialInterface> BlockMaterial;
};

/** Local visual registry; never feeds the playable board or solver. */
UCLASS()
class HEXSOKOBAN_API UHexSceneryWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	TMap<FIntPoint,TWeakObjectPtr<UHexSceneryComponent>> Cells;
	void RefreshAround(FIntPoint Cell);
};
