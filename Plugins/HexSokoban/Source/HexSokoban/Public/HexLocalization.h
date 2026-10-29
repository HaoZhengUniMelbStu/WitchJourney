#pragma once

#include "CoreMinimal.h"

/** Game-only language preferences; never changes editor culture or saved level names. */
class HEXSOKOBAN_API FHexLocalization
{
public:
	static void Initialize();
	static int32 GetLanguage(); // Chinese, Japanese, English, Korean
	static int32 ResolveSystemLanguage(const FString& Culture);
	static void Cycle();
	static const TCHAR* Text(const TCHAR* Source);
	static FString Printf(const TCHAR* Format, ...);
	static FString LevelName(const FString& Id, const FString& Source, bool bBuiltIn);
};

#define HEXT(Source) FHexLocalization::Text(TEXT(Source))
