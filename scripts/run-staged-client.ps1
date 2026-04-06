$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$stageRoot = Join-Path $projectRoot "Saved\StagedBuilds\WindowsGame\Windows"
$gameExe = Join-Path $stageRoot "MOBAMMO.exe"
$serverAddress = "127.0.0.1:7777"

if (-not (Test-Path $gameExe)) {
  throw "Staged game executable not found: $gameExe"
}

Start-Process -FilePath $gameExe -ArgumentList @($serverAddress, "-log", "-windowed", "-ResX=1280", "-ResY=720") -WorkingDirectory $stageRoot
