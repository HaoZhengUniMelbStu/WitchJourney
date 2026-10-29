#pragma once

#include "HexCardTypes.h"

namespace HexMoveNarration
{
	constexpr float EnterSeconds = 0.3f;
	constexpr float HoldSeconds = 4.f;
	constexpr float ExitSeconds = 0.65f;
	constexpr float Lifetime = EnterSeconds + HoldSeconds + ExitSeconds;
	FText Message(EHexSuit Terrain, EHexSuit Played);
	float Opacity(float Age);
}
