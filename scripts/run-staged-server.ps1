$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$stageRoot = Join-Path $projectRoot "Saved\StagedBuilds\WindowsServer\WindowsServer"
$serverExe = Join-Path $stageRoot "MOBAMMO\Binaries\Win64\MOBAMMOServer.exe"
$defaultMap = "/Game/ThirdPerson/Lvl_ThirdPerson?game=/Script/MOBAMMO.MOBAMMOGameMode"

if (-not (Test-Path $serverExe)) {
  throw "Staged server executable not found: $serverExe"
}

Start-Process -FilePath $serverExe -ArgumentList @(
  $defaultMap,
  "-log",
  "-UseIrisReplication=1",
  "-net.SubObjects.DefaultUseSubObjectReplicationList=1"
) -WorkingDirectory (Split-Path -Parent $serverExe)
