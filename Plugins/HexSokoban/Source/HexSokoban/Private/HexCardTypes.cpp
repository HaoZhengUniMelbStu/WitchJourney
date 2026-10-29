#include "HexCardTypes.h"

FString FHexCard::GetRankText() const
{
	switch (Rank)
	{
	case 1:  return TEXT("A");
	case 11: return TEXT("J");
	case 12: return TEXT("Q");
	case 13: return TEXT("K");
	default: return FString::FromInt(Rank);
	}
}

FString FHexCard::GetSuitSymbol() const
{
	switch (Suit)
	{
	case EHexSuit::Spade:   return FString(TEXT("♠"));
	case EHexSuit::Heart:   return FString(TEXT("♥"));
	case EHexSuit::Club:    return FString(TEXT("♣"));
	case EHexSuit::Diamond: return FString(TEXT("♦"));
	default:                return FString();
	}
}
