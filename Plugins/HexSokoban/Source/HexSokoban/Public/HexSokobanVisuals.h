// Material helpers shared by tiles and boxes (no reflection).
//
// Rules:
//  * A material slot the user set (Details panel / Blueprint component) is never replaced.
//  * Other slots may get an auto-tint dynamic material (prototype colours), owned by the actor.
//  * At runtime every slot is wrapped in a dynamic instance of whatever it shows, so scalar
//    parameters such as "Flash" can be driven on custom materials too.

#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace HexSokoban
{
	inline bool IsOwnedDynamicMaterial(const UMaterialInterface* Material, const UObject* Owner)
	{
		const UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Material);
		return Dynamic && Dynamic->GetOuter() == Owner;
	}

	/** True when the user assigned a material to this slot (as opposed to one of our own tint instances). */
	inline bool HasUserMaterial(const UMeshComponent* Mesh, int32 Slot, const UObject* Owner)
	{
		if (!Mesh || !Mesh->OverrideMaterials.IsValidIndex(Slot))
		{
			return false;
		}
		const UMaterialInterface* Override = Mesh->OverrideMaterials[Slot];
		return Override && !IsOwnedDynamicMaterial(Override, Owner);
	}

	/**
	 * Tint every slot of Mesh that has no user material. With bEnable false our tint is removed again
	 * so the mesh shows its own materials. OutTintMaterials receives the tint instances.
	 */
	inline void ApplyTint(UMeshComponent* Mesh, bool bEnable, UMaterialInterface* TintMaterial, FName ColorParameter,
		const FLinearColor& Color, UObject* Owner, TArray<TObjectPtr<UMaterialInstanceDynamic>>& OutTintMaterials)
	{
		OutTintMaterials.Reset();
		if (!Mesh)
		{
			return;
		}
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			if (HasUserMaterial(Mesh, Slot, Owner))
			{
				continue;
			}
			UMaterialInterface* Existing = Mesh->OverrideMaterials.IsValidIndex(Slot) ? Mesh->OverrideMaterials[Slot].Get() : nullptr;
			if (!bEnable || !TintMaterial)
			{
				if (Existing)
				{
					Mesh->SetMaterial(Slot, nullptr); // back to the mesh's own material
				}
				continue;
			}
			UMaterialInstanceDynamic* Tint = Cast<UMaterialInstanceDynamic>(Existing);
			if (!Tint || Tint->Parent != TintMaterial)
			{
				Tint = UMaterialInstanceDynamic::Create(TintMaterial, Owner);
				Mesh->SetMaterial(Slot, Tint);
			}
			Tint->SetVectorParameterValue(ColorParameter, Color);
			OutTintMaterials.Add(Tint);
		}
	}

	/** Runtime: make every slot of every mesh on Owner an owned dynamic instance (same look) and collect them.
	 *  Meshes attached under Exclude (e.g. the card label) are skipped. */
	inline void CollectDynamicMaterials(AActor* Owner, TArray<TObjectPtr<UMaterialInstanceDynamic>>& OutMaterials, const USceneComponent* Exclude = nullptr)
	{
		OutMaterials.Reset();
		if (!Owner)
		{
			return;
		}
		TArray<UMeshComponent*> Meshes;
		Owner->GetComponents<UMeshComponent>(Meshes);
		for (UMeshComponent* Mesh : Meshes)
		{
			if (Exclude && Mesh->IsAttachedTo(Exclude))
			{
				continue;
			}
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				UMaterialInterface* Material = Mesh->GetMaterial(Slot);
				if (!Material)
				{
					continue;
				}
				UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Material);
				if (!Dynamic || Dynamic->GetOuter() != Owner)
				{
					Dynamic = UMaterialInstanceDynamic::Create(Material, Owner);
					Mesh->SetMaterial(Slot, Dynamic);
				}
				OutMaterials.AddUnique(Dynamic);
			}
		}
	}

	/** Custom-depth stencils read by the guidance materials (card faces, path lines; see the Hex materials):
	 *  the character hides card faces and lines, blockers (trees, rocks, boxes) hide lines only. */
	constexpr int32 StencilCharacter = 1;
	constexpr int32 StencilBlocker = 2;

	/** Write Owner's meshes into custom depth with Stencil (meshes under Exclude, e.g. a card label, are skipped). */
	inline void MarkGuideOccluder(AActor* Owner, int32 Stencil, const USceneComponent* Exclude = nullptr)
	{
		if (!Owner)
		{
			return;
		}
		TArray<UMeshComponent*> Meshes;
		Owner->GetComponents<UMeshComponent>(Meshes);
		for (UMeshComponent* Mesh : Meshes)
		{
			if ((Exclude && (Mesh == Exclude || Mesh->IsAttachedTo(Exclude))) || !Mesh->IsVisible())
			{
				continue;
			}
			Mesh->SetRenderCustomDepth(true);
			Mesh->SetCustomDepthStencilValue(Stencil);
		}
	}

	inline void SetScalarOnAll(const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials, FName Parameter, float Value)
	{
		for (UMaterialInstanceDynamic* Material : Materials)
		{
			if (Material)
			{
				Material->SetScalarParameterValue(Parameter, Value);
			}
		}
	}

	inline void SetVectorOnAll(const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Materials, FName Parameter, const FLinearColor& Value)
	{
		for (UMaterialInstanceDynamic* Material : Materials)
		{
			if (Material)
			{
				Material->SetVectorParameterValue(Parameter, Value);
			}
		}
	}
}
