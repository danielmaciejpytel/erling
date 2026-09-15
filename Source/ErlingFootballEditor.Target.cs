using UnrealBuildTool;
public class ErlingFootballEditorTarget : TargetRules { public ErlingFootballEditorTarget(TargetInfo Target) : base(Target) { bOverrideBuildEnvironment=true; WindowsPlatform.Compiler=WindowsCompiler.VisualStudio2022; Type=TargetType.Editor; DefaultBuildSettings=BuildSettingsVersion.V7; IncludeOrderVersion=EngineIncludeOrderVersion.Unreal5_8; ExtraModuleNames.Add("ErlingFootball"); } }


