$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$serverExe = Join-Path $projectRoot "Binaries\Win64\MOBAMMOServer.exe"
$projectFile = Join-Path $projectRoot "MOBAMMO.uproject"
$defaultMap = "/Game/ThirdPerson/Lvl_ThirdPerson"

if (-not (Test-Path $serverExe)) {
  throw "Dedicated server executable not found: $serverExe"
}

if (-not (Test-Path $projectFile)) {
  throw "Project file not found: $projectFile"
}

Start-Process -FilePath $serverExe -ArgumentList @($projectFile, $defaultMap, "-log") -WorkingDirectory $projectRoot
