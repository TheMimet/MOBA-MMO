$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$engineRoot = "C:\Users\PC\OneDrive\Desktop\MOBA MMO\UE_5.7.4_Source"
$editorExe = Join-Path $engineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
$projectFile = Join-Path $projectRoot "MOBAMMO.uproject"

if (-not (Test-Path $editorExe)) {
  throw "Source-built UnrealEditor.exe not found: $editorExe"
}

if (-not (Test-Path $projectFile)) {
  throw "Project file not found: $projectFile"
}

Start-Process -FilePath $editorExe -ArgumentList @($projectFile) -WorkingDirectory $projectRoot
