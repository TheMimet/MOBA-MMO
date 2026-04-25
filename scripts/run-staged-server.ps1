$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$stageRoot = Join-Path $projectRoot "Saved\StagedBuilds\WindowsServer\WindowsServer"
$serverExe = Join-Path $stageRoot "MOBAMMO\Binaries\Win64\MOBAMMOServer.exe"
$defaultMap = "/Game/ThirdPerson/Lvl_ThirdPerson?game=/Script/MOBAMMO.MOBAMMOGameMode"
$backendEnvPath = Join-Path $projectRoot "server\.env"

function Read-DotEnvValue {
  param(
    [string]$Path,
    [string]$Name
  )

  if (-not (Test-Path $Path)) {
    return ""
  }

  $line = Get-Content -LiteralPath $Path |
    Where-Object { $_ -match "^\s*$([regex]::Escape($Name))\s*=" } |
    Select-Object -Last 1

  if (-not $line) {
    return ""
  }

  $value = ($line -split "=", 2)[1].Trim()
  if (($value.StartsWith('"') -and $value.EndsWith('"')) -or ($value.StartsWith("'") -and $value.EndsWith("'"))) {
    $value = $value.Substring(1, $value.Length - 2)
  }

  return $value
}

if (-not (Test-Path $serverExe)) {
  throw "Staged server executable not found: $serverExe"
}

$sessionServerSecret = $env:MOBAMMO_SESSION_SERVER_SECRET
if ([string]::IsNullOrWhiteSpace($sessionServerSecret)) {
  $sessionServerSecret = $env:SESSION_SERVER_SECRET
}
if ([string]::IsNullOrWhiteSpace($sessionServerSecret)) {
  $sessionServerSecret = Read-DotEnvValue -Path $backendEnvPath -Name "SESSION_SERVER_SECRET"
}
if ([string]::IsNullOrWhiteSpace($sessionServerSecret)) {
  throw "SESSION_SERVER_SECRET is required for the staged dedicated server. Set it in server\.env or the environment."
}

$env:MOBAMMO_SESSION_SERVER_SECRET = $sessionServerSecret

Start-Process -FilePath $serverExe -ArgumentList @(
  $defaultMap,
  "-log",
  "-UseIrisReplication=1",
  "-net.SubObjects.DefaultUseSubObjectReplicationList=1"
) -WorkingDirectory (Split-Path -Parent $serverExe)
