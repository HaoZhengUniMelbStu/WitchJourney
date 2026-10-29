#include "HexGridLibrary.h"
#include "HexSokobanMath.h"
#include "HexSokobanSettings.h"

namespace
{
	constexpr double Sqrt3Over2 = 0.86602540378443864676;
}

float UHexGridLibrary::GetTileSpacing()
{
	return FMath::Max(UHexSokobanSettings::Get().TileSpacing, 1.f);
}

FVector UHexGridLibrary::CoordToWorld(FIntPoint Coord, float Z)
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	const double Spacing = GetTileSpacing();
	const double X = Settings.GridOrigin.X + Spacing * (static_cast<double>(Coord.X) + 0.5 * static_cast<double>(Coord.Y));
	const double Y = Settings.GridOrigin.Y + Spacing * Sqrt3Over2 * static_cast<double>(Coord.Y);
	return FVector(X, Y, Z);
}

FIntPoint UHexGridLibrary::WorldToCoord(const FVector& WorldLocation)
{
	const UHexSokobanSettings& Settings = UHexSokobanSettings::Get();
	const double Spacing = GetTileSpacing();
	const double LocalX = WorldLocation.X - Settings.GridOrigin.X;
	const double LocalY = WorldLocation.Y - Settings.GridOrigin.Y;

	// Fractional axial coordinates.
	const double R = LocalY / (Spacing * Sqrt3Over2);
	const double Q = LocalX / Spacing - 0.5 * R;
	const double S = -Q - R;

	// Cube rounding.
	int32 RoundQ = FMath::RoundToInt32(Q);
	int32 RoundR = FMath::RoundToInt32(R);
	const int32 RoundS = FMath::RoundToInt32(S);

	const double DiffQ = FMath::Abs(static_cast<double>(RoundQ) - Q);
	const double DiffR = FMath::Abs(static_cast<double>(RoundR) - R);
	const double DiffS = FMath::Abs(static_cast<double>(RoundS) - S);

	if (DiffQ > DiffR && DiffQ > DiffS)
	{
		RoundQ = -RoundR - RoundS;
	}
	else if (DiffR > DiffS)
	{
		RoundR = -RoundQ - RoundS;
	}

	return FIntPoint(RoundQ, RoundR);
}

FVector UHexGridLibrary::SnapToGrid(const FVector& WorldLocation)
{
	return CoordToWorld(WorldToCoord(WorldLocation), static_cast<float>(WorldLocation.Z));
}

FIntPoint UHexGridLibrary::GetNeighbor(FIntPoint Coord, EHexDirection Direction)
{
	return Coord + HexSokoban::DirectionOffset(Direction);
}

FVector UHexGridLibrary::GetDirectionVector(EHexDirection Direction)
{
	return HexSokoban::DirectionVector(Direction);
}

EHexDirection UHexGridLibrary::GetDirectionFromVector(const FVector& Vector)
{
	return HexSokoban::DirectionFromVector(Vector);
}

EHexDirection UHexGridLibrary::GetDirectionFromYaw(float Yaw)
{
	return HexSokoban::DirectionFromYaw(Yaw);
}

int32 UHexGridLibrary::GetHexDistance(FIntPoint A, FIntPoint B)
{
	const int32 DQ = A.X - B.X;
	const int32 DR = A.Y - B.Y;
	return (FMath::Abs(DQ) + FMath::Abs(DR) + FMath::Abs(DQ + DR)) / 2;
}
