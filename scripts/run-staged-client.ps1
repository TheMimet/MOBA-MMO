$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$stageRoot = Join-Path $projectRoot "Saved\StagedBuilds\WindowsGame\Windows"
$gameExe = Join-Path $stageRoot "MOBAMMO\Binaries\Win64\MOBAMMO.exe"
$webUiScript = Join-Path $PSScriptRoot "run-character-flow-webui.ps1"
$serverScript = Join-Path $PSScriptRoot "run-staged-server.ps1"
$backendRoot = Join-Path $projectRoot "server"

if (-not (Test-Path $gameExe)) {
  throw "Staged game executable not found: $gameExe"
}

function Test-PortListening {
  param(
    [string]$HostName,
    [int]$Port
  )

  try {
    $client = New-Object System.Net.Sockets.TcpClient
    $async = $client.BeginConnect($HostName, $Port, $null, $null)
    $connected = $async.AsyncWaitHandle.WaitOne(1500, $false)

    if (-not $connected) {
      $client.Close()
      return $false
    }

    $client.EndConnect($async)
    $client.Close()
    return $true
  } catch {
    return $false
  }
}

function Wait-ForPort {
  param(
    [string]$HostName,
    [int]$Port,
    [int]$TimeoutSeconds = 20
  )

  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  while ((Get-Date) -lt $deadline) {
    if (Test-PortListening -HostName $HostName -Port $Port) {
      return $true
    }

    Start-Sleep -Milliseconds 500
  }

  return $false
}

if (-not (Test-PortListening -HostName "127.0.0.1" -Port 3000)) {
  if (-not (Test-Path $backendRoot)) {
    throw "Backend project not found: $backendRoot"
  }

  Start-Process powershell -ArgumentList @(
    "-NoExit",
    "-ExecutionPolicy", "Bypass",
    "-Command",
    "Set-Location '$backendRoot'; npm run db:start; npm run dev"
  ) -WorkingDirectory $backendRoot

  if (-not (Wait-ForPort -HostName "127.0.0.1" -Port 3000 -TimeoutSeconds 30)) {
    throw "Backend did not start listening on 127.0.0.1:3000"
  }
}

if (-not (Test-PortListening -HostName "127.0.0.1" -Port 7777)) {
  if (-not (Test-Path $serverScript)) {
    throw "Staged server script not found: $serverScript"
  }

  & $serverScript

  if (-not (Wait-ForPort -HostName "127.0.0.1" -Port 7777 -TimeoutSeconds 45)) {
    Write-Warning "Dedicated server was not confirmed on 127.0.0.1:7777 within the wait window. Launching client anyway."
  }
}

if (Test-Path $webUiScript) {
  & $webUiScript
}

Start-Process -FilePath $gameExe -ArgumentList @(
  "/Game/ThirdPerson/Lvl_ThirdPerson?game=/Script/MOBAMMO.MOBAMMOGameMode",
  "-log",
  "-windowed",
  "-ResX=1280",
  "-ResY=720",
  "-UseIrisReplication=1",
  "-net.SubObjects.DefaultUseSubObjectReplicationList=1"
) -WorkingDirectory (Split-Path -Parent $gameExe)
