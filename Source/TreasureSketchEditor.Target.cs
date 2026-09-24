using UnrealBuildTool;

public class TreasureSketchEditorTarget : TargetRules
{
    public TreasureSketchEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        ExtraModuleNames.Add("TreasureSketch");
    }
}
