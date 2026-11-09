// Copyright Brandon Hillig 2026

using UnrealBuildTool;

public class DeadwaterRun : ModuleRules
{
	public DeadwaterRun(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Dependencies per Docs/Architecture/01-Project-Structure.md "Module dependencies".
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"UMG",
			"CommonUI",
			"CommonInput"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
			"Niagara",
			"DeveloperSettings",
			"Json",
			"JsonUtilities"
		});

		// Module root, so includes read "Core/DwGameMode.h", "Board/DwTile.h", ...
		PublicIncludePaths.Add(ModuleDirectory);
	}
}
