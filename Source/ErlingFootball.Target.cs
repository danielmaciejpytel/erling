using UnrealBuildTool;
public class ErlingFootballTarget : TargetRules { public ErlingFootballTarget(TargetInfo Target) : base(Target) { bOverrideBuildEnvironment=true; WindowsPlatform.Compiler=WindowsCompiler.VisualStudio2022; Type=TargetType.Game; DefaultBuildSettings=BuildSettingsVersion.V7; IncludeOrderVersion=EngineIncludeOrderVersion.Unreal5_8; ExtraModuleNames.Add("ErlingFootball"); } }


