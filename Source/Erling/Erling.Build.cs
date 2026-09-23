using UnrealBuildTool;
public class Erling : ModuleRules { public Erling(ReadOnlyTargetRules Target) : base(Target) { PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs; PublicIncludePaths.Add(ModuleDirectory); PublicDependencyModuleNames.AddRange(new string[]{"Core","CoreUObject","Engine","InputCore","Slate","SlateCore","UMG","Json","JsonUtilities","PhysicsCore","ImageWrapper"}); } }
