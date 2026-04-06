using UnrealBuildTool;
using System.Collections.Generic;

public class MOBAMMOEditorTarget : TargetRules
{
    public MOBAMMOEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("MOBAMMO");
    }
}
