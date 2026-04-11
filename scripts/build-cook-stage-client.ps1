$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$engineRoot = "C:\Users\PC\OneDrive\Desktop\MOBA MMO\UE_5.7.4_Source"
$uat = Join-Path $engineRoot "Engine\Build\BatchFiles\RunUAT.bat"
$projectFile = Join-Path $projectRoot "MOBAMMO.uproject"
$stageDir = Join-Path $projectRoot "Saved\StagedBuilds\WindowsGame"

if (-not (Test-Path $uat)) {
  throw "RunUAT.bat not found: $uat"
}

if (-not (Test-Path $projectFile)) {
  throw "Project file not found: $projectFile"
}

& $uat BuildCookRun `
  -project="$projectFile" `
  -build `
  -cook `
  -stage `
  -platform=Win64 `
  -clientconfig=Development `
  -map=/Game/ThirdPerson/Lvl_ThirdPerson `
  -unattended `
  -noxge `
  -stagingdirectory="$stageDir"
