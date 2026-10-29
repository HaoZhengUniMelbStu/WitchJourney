using UnrealBuildTool;

public class HexSokoban : ModuleRules
{
	public HexSokoban(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"DeveloperSettings",
			"UMG",
			"Slate",
			"SlateCore",
			"Json",
			"JsonUtilities",
            "AssetRegistry",
            "Niagara"
		});
	}
}
