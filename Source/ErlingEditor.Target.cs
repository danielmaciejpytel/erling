using UnrealBuildTool;
public class ErlingEditorTarget : TargetRules { public ErlingEditorTarget(TargetInfo Target) : base(Target) { bOverrideBuildEnvironment=true; WindowsPlatform.Compiler=WindowsCompiler.VisualStudio2022; Type=TargetType.Editor; DefaultBuildSettings=BuildSettingsVersion.V7; IncludeOrderVersion=EngineIncludeOrderVersion.Unreal5_8; ExtraModuleNames.AddRange(new[] {"Erling", "ErlingUIEditor"}); } }


