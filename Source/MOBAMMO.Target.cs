using UnrealBuildTool;
using System.Collections.Generic;

public class MOBAMMOTarget : TargetRules
{
    public MOBAMMOTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("MOBAMMO");
    }
}
