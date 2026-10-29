#include "HexMoveNarration.h"
#include "HexLocalization.h"

FText HexMoveNarration::Message(EHexSuit Terrain, EHexSuit Played)
{
	// Rows: destination terrain. Columns: paid card. Enum order: spade, heart, club, diamond.
	const TCHAR* Lines[4][4] = {
		{
			HEXT("伊蕾娜使用露营装备在石洞中生火休息，恢复体力后通过了山地。"),
			HEXT("伊蕾娜用指南针找到了山中的营地，借用露营装备休息后通过了山地。"),
			HEXT("伊蕾娜在镇上买到了一套露营装备，在篝火旁休息后通过了山地。"),
			HEXT("伊蕾娜在船上向商人购得了露营装备，在山洞中休整后通过了山地。")
		},
		{
			HEXT("伊蕾娜背着露营装备穿过了草地，沿途生火休息恢复体力。"),
			HEXT("伊蕾娜使用指南针通过了草地。"),
			HEXT("伊蕾娜在镇上买到了一块指南针通过了草地。"),
			HEXT("伊蕾娜在船上遇到了好心的向导帮忙通过了草地。")
		},
		{
			HEXT("伊蕾娜将一套精致的露营装备作为贿赂收买了城镇守卫。"),
			HEXT("伊蕾娜将一块精美的珍藏版指南针作为贿赂收买了城镇守卫。"),
			HEXT("伊蕾娜使用金钱贿赂了城镇守卫。"),
			HEXT("伊蕾娜在船上结识了偷渡客帮忙躲过了卫兵的检查。")
		},
		{
			HEXT("伊蕾娜将一套精致的露营装备作为交换得到了最后一张船票。"),
			HEXT("伊蕾娜将一块精美的珍藏版指南针作为交换得到了最后一张船票。"),
			HEXT("伊蕾娜在镇上买到了一张船票通过了水域。"),
			HEXT("伊蕾娜使用船通过了水域。")
		}
	};
	const uint8 Row = static_cast<uint8>(Terrain), Column = static_cast<uint8>(Played);
	return Row < 4 && Column < 4 ? FText::FromString(Lines[Row][Column]) : FText::GetEmpty();
}

float HexMoveNarration::Opacity(float Age)
{
	const float In = FMath::Clamp(Age / EnterSeconds, 0.f, 1.f);
	const float Out = FMath::Clamp((Lifetime - Age) / ExitSeconds, 0.f, 1.f);
	return In * Out;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexNarrationTest, "HexSokoban.UI.MoveNarration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexNarrationTest::RunTest(const FString& Parameters)
{
	TSet<FString> Unique;
	for (uint8 Terrain = 0; Terrain < 4; ++Terrain)
	{
		for (uint8 Card = 0; Card < 4; ++Card)
		{
			const FString Line = HexMoveNarration::Message(static_cast<EHexSuit>(Terrain), static_cast<EHexSuit>(Card)).ToString();
			TestFalse(TEXT("Every combination has a line"), Line.IsEmpty());
			Unique.Add(Line);
		}
	}
	TestEqual(TEXT("All sixteen scenarios are distinct"), Unique.Num(), 16);
	TestTrue(TEXT("Boat crossing grass uses guide story"), HexMoveNarration::Message(EHexSuit::Heart, EHexSuit::Diamond).ToString().Contains(HEXT("好心的向导")));
	TestTrue(TEXT("Compass crossing water uses ticket story"), HexMoveNarration::Message(EHexSuit::Diamond, EHexSuit::Heart).ToString().Contains(HEXT("最后一张船票")));
	TestEqual(TEXT("Starts transparent"), HexMoveNarration::Opacity(0.f), 0.f);
	TestEqual(TEXT("Holds opaque"), HexMoveNarration::Opacity(4.f), 1.f);
	TestTrue(TEXT("Old entry fades at its own age"), HexMoveNarration::Opacity(4.6f) < 1.f);
	TestEqual(TEXT("Expires transparent"), HexMoveNarration::Opacity(HexMoveNarration::Lifetime), 0.f);
	return true;
}
#endif
