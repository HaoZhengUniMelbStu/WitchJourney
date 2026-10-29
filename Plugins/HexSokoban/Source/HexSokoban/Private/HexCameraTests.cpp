#include "HexCameraBounds.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCameraBoundsTest,"HexSokoban.Camera.Bounds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHexCameraBoundsTest::RunTest(const FString& Parameters)
{
	const auto H=HexCameraBounds::Hull({{0,0},{100,0},{100,100},{0,100},{50,50},{0,0}});
	TestEqual(TEXT("Only outer vertices retained"),H.Num(),4);
	TestTrue(TEXT("Interior unaffected"),HexCameraBounds::Clamp({40,60},H).Equals({40,60}));
	TestTrue(TEXT("Outside drag lands on edge"),HexCameraBounds::Clamp({1000,50},H).Equals({100,50}));
	const auto Diagonal=HexCameraBounds::Hull({{0,0},{100,100},{50,50}});
	TestTrue(TEXT("Narrow diagonal map cannot pan into empty bounding-box corners"),HexCameraBounds::Clamp({100,0},Diagonal).Equals({50,50}));
	const auto Single=HexCameraBounds::Hull({{10,20}});
	TestTrue(TEXT("Single-tile map bounded"),HexCameraBounds::Clamp({1000,1000},Single).Equals({10,20}));
	const auto Soft=HexCameraBounds::Rubber({100000,50},{100,50},77);
	TestTrue(TEXT("Extreme drag has bounded elastic overscroll"),Soft.X>100 && Soft.X<177);
	TestTrue(TEXT("Release returns to boundary"),HexCameraBounds::Clamp(Soft,H).Equals({100,50}));
	return true;
}
#endif
