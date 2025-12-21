// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class material : ModuleRules
{
	public material(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"material",
			"material/Variant_Platforming",
			"material/Variant_Platforming/Animation",
			"material/Variant_Combat",
			"material/Variant_Combat/AI",
			"material/Variant_Combat/Animation",
			"material/Variant_Combat/Gameplay",
			"material/Variant_Combat/Interfaces",
			"material/Variant_Combat/UI",
			"material/Variant_SideScrolling",
			"material/Variant_SideScrolling/AI",
			"material/Variant_SideScrolling/Gameplay",
			"material/Variant_SideScrolling/Interfaces",
			"material/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
