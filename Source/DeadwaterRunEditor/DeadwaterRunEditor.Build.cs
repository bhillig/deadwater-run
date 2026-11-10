// Copyright Brandon Hillig 2026

using UnrealBuildTool;

// Editor-only module: validators, detail customizations, commandlets, editor menus.
// Never part of a Game/Shipping target. See Docs/Architecture/01-Project-Structure.md.
public class DeadwaterRunEditor : ModuleRules
{
	public DeadwaterRunEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"DeadwaterRun"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd",
			"Slate",
			"SlateCore",
			"PropertyEditor",
			"ToolMenus",
			"DataValidation",
			"AssetTools",
			"ContentBrowser",
			"Blutility",
			"UMGEditor",
			"UMG",
			"CommonUI",
			"CommonInput",
			"EditorScriptingUtilities",
			"EnhancedInput",
			"InputCore",
			"GameplayAbilities",
			"GameplayTags",
			"Json",
			"JsonUtilities"
		});
	}
}
