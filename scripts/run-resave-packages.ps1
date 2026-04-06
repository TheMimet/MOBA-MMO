$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$engineRoot = "C:\Users\PC\OneDrive\Desktop\MOBA MMO\UE_5.7.4_Source"
$editorCmdExe = Join-Path $engineRoot "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$projectFile = Join-Path $projectRoot "MOBAMMO.uproject"

if (-not (Test-Path $editorCmdExe)) {
  throw "Source-built UnrealEditor-Cmd.exe not found: $editorCmdExe"
}

if (-not (Test-Path $projectFile)) {
  throw "Project file not found: $projectFile"
}

& $editorCmdExe $projectFile -run=ResavePackages -ProjectOnly -AllowCommandletRendering -IgnoreChangelist
