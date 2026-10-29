// Pure hex math helpers (no reflection, no settings).

#pragma once

#include "CoreMinimal.h"
#include "HexSokobanTypes.h"

/** Pure hex math helpers (no settings involved). */
namespace HexSokoban
{
	inline FIntPoint DirectionOffset(EHexDirection Direction)
	{
		switch (Direction)
		{
		case EHexDirection::Forward:		return FIntPoint(1, 0);
		case EHexDirection::ForwardRight:	return FIntPoint(0, 1);
		case EHexDirection::BackRight:		return FIntPoint(-1, 1);
		case EHexDirection::Back:			return FIntPoint(-1, 0);
		case EHexDirection::BackLeft:		return FIntPoint(0, -1);
		case EHexDirection::ForwardLeft:	return FIntPoint(1, -1);
		default:							return FIntPoint::ZeroValue;
		}
	}

	inline float DirectionYaw(EHexDirection Direction)
	{
		return 60.f * static_cast<float>(static_cast<uint8>(Direction));
	}

	inline EHexDirection DirectionFromYaw(float Yaw)
	{
		float Normalized = FMath::Fmod(Yaw, 360.f);
		if (Normalized < 0.f)
		{
			Normalized += 360.f;
		}
		const int32 Index = FMath::RoundToInt32(Normalized / 60.f) % 6;
		return static_cast<EHexDirection>(Index);
	}

	inline EHexDirection DirectionFromVector(const FVector& Vector)
	{
		if (Vector.SizeSquared2D() < KINDA_SMALL_NUMBER)
		{
			return EHexDirection::Invalid;
		}
		const float Yaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Vector.Y, Vector.X)));
		return DirectionFromYaw(Yaw);
	}

	inline FVector DirectionVector(EHexDirection Direction)
	{
		if (Direction == EHexDirection::Invalid)
		{
			return FVector::ZeroVector;
		}
		const float Radians = FMath::DegreesToRadians(DirectionYaw(Direction));
		return FVector(FMath::Cos(Radians), FMath::Sin(Radians), 0.f);
	}
}
