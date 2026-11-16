[CmdletBinding()]
param(
    [string]$EngineRoot = $env:ERLING_ENGINE_ROOT,
    [switch]$Editor,
    [switch]$CheckOnly
)

$ErrorActionPreference = 'Stop'

try {
    $projectFile = Join-Path $PSScriptRoot 'Erling.uproject'
    $project = Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json
    $association = [string]$project.EngineAssociation
    $candidates = [System.Collections.Generic.List[string]]::new()

    if ($EngineRoot) {
        # An explicit override must be valid; do not silently launch another engine.
        $candidates.Add($EngineRoot)
    } else {
        $registered = Get-ItemProperty -LiteralPath "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$association" -ErrorAction SilentlyContinue
        if ($registered.InstalledDirectory) { $candidates.Add($registered.InstalledDirectory) }
        $sourceBuilds = Get-ItemProperty -LiteralPath 'HKCU:\Software\Epic Games\Unreal Engine\Builds' -ErrorAction SilentlyContinue
        if ($sourceBuilds -and $sourceBuilds.PSObject.Properties[$association]) {
            $candidates.Add([string]$sourceBuilds.PSObject.Properties[$association].Value)
        }
        $manifest = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
        if (Test-Path -LiteralPath $manifest) {
            $installed = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
            foreach ($entry in $installed.InstallationList) {
                if ($entry.AppName -eq "UE_$association" -and $entry.InstallLocation) {
                    $candidates.Add($entry.InstallLocation)
                }
            }
        }
        $candidates.Add((Join-Path $env:ProgramFiles "Epic Games\UE_$association"))
    }

    $editorPath = $null
    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        $executable = Join-Path $candidate 'Engine\Binaries\Win64\UnrealEditor.exe'
        $versionFile = Join-Path $candidate 'Engine\Build\Build.version'
        if (!(Test-Path -LiteralPath $executable -PathType Leaf) -or !(Test-Path -LiteralPath $versionFile -PathType Leaf)) { continue }
        $version = Get-Content -LiteralPath $versionFile -Raw | ConvertFrom-Json
        if ($association -match '^\d+\.\d+$' -and "$($version.MajorVersion).$($version.MinorVersion)" -ne $association) { continue }
        $editorPath = (Resolve-Path -LiteralPath $executable).Path
        break
    }
    if (!$editorPath) {
        throw "Unreal Engine $association was not found. Run Start.cmd -EngineRoot `"D:\Path\UE_$association`" or set ERLING_ENGINE_ROOT. See README.md for setup and build instructions."
    }

    Write-Host "Engine: $editorPath ($($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion))"
    Write-Host "Project: $projectFile"
    if ($CheckOnly) { exit 0 }
    $arguments = "`"$projectFile`""
    if (!$Editor) {
        $arguments += ' -game -windowed -ResX=1440 -ResY=900'
    }
    # The editor/game is an interactive window intentionally opened by this launcher.
    Start-Process -FilePath $editorPath -ArgumentList $arguments -WorkingDirectory $PSScriptRoot
    exit 0
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}
