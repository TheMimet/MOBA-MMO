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
                "UMG",
                "HTTP",
                "Json",
                "JsonUtilities"
            }
        );
    }
}
