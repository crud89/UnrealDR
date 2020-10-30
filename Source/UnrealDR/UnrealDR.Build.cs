using UnrealBuildTool;

public class UnrealDR : ModuleRules
{
	public UnrealDR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(new string[] { });
		PrivateIncludePaths.AddRange(new string[] { });
		DynamicallyLoadedModuleNames.AddRange(new string[] { });

		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core" 
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"HeadMountedDisplay",
			"SteamVR",
			"OpenVR",
			"ProceduralMeshComponent"
		});
	}
}
