using UnrealBuildTool;

public class MOBAMMO : ModuleRules
{
    public MOBAMMO(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "EnhancedInput",
                "Slate",
                "SlateCore",
                "UMG",
                "HTTP",
                "Json",
                "JsonUtilities",
                "WebBrowserWidget",
                "GameplayTags",
                "GameplayTasks",
                "NetCore",
                "MOBAMMOAI"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "MOBAMMOAbilities"
            }
        );
    }
}
