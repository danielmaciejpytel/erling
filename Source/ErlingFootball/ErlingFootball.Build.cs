using UnrealBuildTool;
public class ErlingFootball : ModuleRules { public ErlingFootball(ReadOnlyTargetRules Target) : base(Target) { PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs; PublicDependencyModuleNames.AddRange(new string[]{"Core","CoreUObject","Engine","InputCore","Slate","SlateCore","UMG","Json","JsonUtilities","PhysicsCore"}); } }
