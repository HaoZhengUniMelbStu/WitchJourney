#include "HexLocalization.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/StringBuilder.h"
#include "HAL/PlatformMisc.h"

namespace
{
	int32 Language = INDEX_NONE;
	const TCHAR* Catalog[][4] = {
#include "HexLocalizationCatalog.inl"
	};
	FString PreferenceFile() { return FPaths::ProjectSavedDir() / TEXT("Config/HexLanguage.ini"); }
}

int32 FHexLocalization::ResolveSystemLanguage(const FString& Culture)
{
	const FString Code=Culture.Left(2).ToLower();
	return Code==TEXT("zh")?0:Code==TEXT("ja")?1:Code==TEXT("ko")?3:2;
}

void FHexLocalization::Initialize()
{
	if(Language!=INDEX_NONE) return;
	FConfigFile Preferences;
	Preferences.Read(PreferenceFile());
	int32 Saved=INDEX_NONE;
	if(Preferences.GetInt(TEXT("Language"),TEXT("Selection"),Saved) && Saved>=0 && Saved<4)
		Language=Saved;
	else
	{
		Language=ResolveSystemLanguage(FPlatformMisc::GetDefaultLanguage());
		Preferences.SetString(TEXT("Language"),TEXT("Selection"),*FString::FromInt(Language));
		Preferences.Write(PreferenceFile());
	}
}

int32 FHexLocalization::GetLanguage() { Initialize(); return Language; }

void FHexLocalization::Cycle()
{
	Language=(GetLanguage()+1)%4;
	FConfigFile Preferences;
	Preferences.SetString(TEXT("Language"),TEXT("Selection"),*FString::FromInt(Language));
	Preferences.Write(PreferenceFile());
}

const TCHAR* FHexLocalization::Text(const TCHAR* Source)
{
	const int32 Index=GetLanguage();
	if(Index==0) return Source;
	static const TMap<FString,int32> Lookup=[]()
	{
		TMap<FString,int32> Result;
		for(int32 I=0;I<UE_ARRAY_COUNT(Catalog);++I) Result.Add(Catalog[I][0],I);
		return Result;
	}();
	if(const int32* Row=Lookup.Find(Source)) return Catalog[*Row][Index];
	return Source;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexLanguageTest,"HexSokoban.UI.Languages",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHexLanguageTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Chinese system variants"),FHexLocalization::ResolveSystemLanguage(TEXT("zh-TW")),0);
	TestEqual(TEXT("Japanese system"),FHexLocalization::ResolveSystemLanguage(TEXT("ja-JP")),1);
	TestEqual(TEXT("Korean system"),FHexLocalization::ResolveSystemLanguage(TEXT("ko-KR")),3);
	TestEqual(TEXT("Unsupported falls back to English"),FHexLocalization::ResolveSystemLanguage(TEXT("fr-FR")),2);
	const int32 Original=Language;
	FString OriginalFile; const bool HadFile=FFileHelper::LoadFileToString(OriginalFile,*PreferenceFile());
	for(int32 I=0;I<4;++I)
	{
		Language=I;
		for(const auto& Row:Catalog) TestTrue(TEXT("Every translation is present"),FCString::Strlen(Row[I])>0);
		TestEqual(TEXT("New custom names are untouched"),FHexLocalization::LevelName(TEXT("custom-id"),TEXT("初见"),false),FString(TEXT("初见")));
		TestEqual(TEXT("Custom level with same ID remains untouched"),FHexLocalization::LevelName(TEXT("16_Level"),TEXT("初见"),false),FString(TEXT("初见")));
		TestEqual(TEXT("Renamed built-in remains untouched"),FHexLocalization::LevelName(TEXT("16_Level"),TEXT("新名称"),true),FString(TEXT("新名称")));
		TestTrue(TEXT("Skill cost survives translation"),FString(HEXT("同花色瞬移 · 消耗6步")).Contains(TEXT("6")));
		const FString Before=HEXT("语言");
		FHexLocalization::Cycle();
		TestEqual(TEXT("Cycle follows Chinese Japanese English Korean"),Language,(I+1)%4);
		TestNotEqual(TEXT("Label changes immediately"),FString(HEXT("语言")),Before);
		Language=INDEX_NONE;
		TestEqual(TEXT("Saved choice overrides system on next initialization"),FHexLocalization::GetLanguage(),(I+1)%4);
	}
	Language=Original;
	if(HadFile) FFileHelper::SaveStringToFile(OriginalFile,*PreferenceFile());
	else IFileManager::Get().Delete(*PreferenceFile());
	return true;
}
#endif

FString FHexLocalization::Printf(const TCHAR* Format, ...)
{
	va_list Args; va_start(Args,Format);
	TStringBuilder<512> Buffer; Buffer.AppendV(Format,Args);
	va_end(Args);
	return FString(Buffer.ToString());
}

FString FHexLocalization::LevelName(const FString& Id,const FString& Source,bool bBuiltIn)
{
	// Match both identity and original name. Renamed or custom levels stay untouched.
	static const TCHAR* Names[][5]={
		{TEXT("16_Level"),TEXT("初见"),TEXT("初めての出会い"),TEXT("First Encounter"),TEXT("첫 만남")},
		{TEXT("17_Level"),TEXT("小小魔女"),TEXT("小さな魔女"),TEXT("Little Witch"),TEXT("작은 마녀")},
		{TEXT("18_Level"),TEXT("伊雷娜的梦想"),TEXT("イレイナの夢"),TEXT("Elaina's Dream"),TEXT("일레이나의 꿈")},
		{TEXT("19_Level"),TEXT("滨海小镇"),TEXT("海辺の街"),TEXT("Seaside Town"),TEXT("바닷가 마을")},
		{TEXT("20_Level"),TEXT("四处碰壁"),TEXT("行き止まりばかり"),TEXT("Dead Ends"),TEXT("막다른 길")},
		{TEXT("21_Level"),TEXT("林地魔女"),TEXT("森の魔女"),TEXT("Witch of the Woods"),TEXT("숲의 마녀")},
		{TEXT("22_Level"),TEXT("芙兰的试炼"),TEXT("フランの試練"),TEXT("Fran's Trial"),TEXT("프랑의 시련")},
		{TEXT("23_Level"),TEXT("魔法使之国"),TEXT("魔法使いの国"),TEXT("The Land of Mages"),TEXT("마법사의 나라")},
		{TEXT("24_Level"),TEXT("沙耶的请求"),TEXT("サヤの頼み"),TEXT("Saya's Request"),TEXT("사야의 부탁")},
		{TEXT("25_Level"),TEXT("真相"),TEXT("真相"),TEXT("The Truth"),TEXT("진실")},
		{TEXT("26_Level"),TEXT("海峡之匙"),TEXT("海峡の鍵"),TEXT("Key to the Strait"),TEXT("해협의 열쇠")},
		{TEXT("27_Level"),TEXT("送花的陷阱"),TEXT("花の贈り物の罠"),TEXT("A Floral Trap"),TEXT("꽃 선물의 함정")},
		{TEXT("28_Level"),TEXT("准备启航"),TEXT("出航の準備"),TEXT("Ready to Set Sail"),TEXT("출항 준비")},
		{TEXT("29_Level"),TEXT("轮船失事"),TEXT("難破"),TEXT("Shipwreck"),TEXT("난파")},
		{TEXT("30_Level"),TEXT("水门都市"),TEXT("水門都市"),TEXT("Watergate City"),TEXT("수문 도시")}
	};
	if(bBuiltIn) for(const auto& Row:Names)
		if(Id==Row[0] && Source==Row[1]) return Row[GetLanguage()+1];
	return Source;
}
