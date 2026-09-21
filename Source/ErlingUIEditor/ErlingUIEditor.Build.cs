using UnrealBuildTool;

public class ErlingUIEditor : ModuleRules
{
    public ErlingUIEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Erling",
            "UMG", "Slate", "SlateCore", "UnrealEd", "UMGEditor", "Kismet", "KismetCompiler",
            "AssetRegistry", "Json", "JsonUtilities" });
    }
}
