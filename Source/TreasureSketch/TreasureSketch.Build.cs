using UnrealBuildTool;

public class TreasureSketch : ModuleRules
{
    public TreasureSketch(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "ProceduralMeshComponent",
            "OnlineSubsystem", "OnlineSubsystemUtils"
        });
    }
}
