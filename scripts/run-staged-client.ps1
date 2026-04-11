$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$stageRoot = Join-Path $projectRoot "Saved\StagedBuilds\WindowsGame\Windows"
$gameExe = Join-Path $stageRoot "MOBAMMO.exe"

if (-not (Test-Path $gameExe)) {
  throw "Staged game executable not found: $gameExe"
}

Start-Process -FilePath $gameExe -ArgumentList @("-log", "-windowed", "-ResX=1280", "-ResY=720") -WorkingDirectory $stageRoot
