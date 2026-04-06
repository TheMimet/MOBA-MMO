using UnrealBuildTool;
using System.Collections.Generic;

public class MOBAMMOServerTarget : TargetRules
{
    public MOBAMMOServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("MOBAMMO");
    }
}
