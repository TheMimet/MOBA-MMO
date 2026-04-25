$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$stageRoot = Join-Path $projectRoot "Saved\StagedBuilds\WindowsGame\Windows"
$gameExe = Join-Path $stageRoot "MOBAMMO\Binaries\Win64\MOBAMMO.exe"
$serverScript = Join-Path $PSScriptRoot "run-staged-server.ps1"
$backendRoot = Join-Path $projectRoot "server"
$backendDbScript = Join-Path $backendRoot "scripts\start-local-db.ps1"
$webUiRoot = Join-Path (Split-Path -Parent $projectRoot) "LoginAndCharacterFlowDesign"

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

function Test-MOBAMMOBackend {
  try {
    $response = Invoke-WebRequest -Uri "http://127.0.0.1:3000/health" -UseBasicParsing -TimeoutSec 3
    return $response.StatusCode -eq 200 -and $response.Content -like "*moba-mmo-server*"
  } catch {
    return $false
  }
}

function Test-StagedServerProcess {
  return [bool](Get-Process -Name "MOBAMMOServer" -ErrorAction SilentlyContinue)
}

function Wait-ForStagedServerProcess {
  param([int]$TimeoutSeconds = 15)

  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  while ((Get-Date) -lt $deadline) {
    if (Test-StagedServerProcess) {
      return $true
    }

    Start-Sleep -Milliseconds 500
  }

  return $false
}

function Start-MOBAMMOBackend {
  if (-not (Test-Path $backendDbScript)) {
    throw "Backend database script not found: $backendDbScript"
  }

  & powershell -NoProfile -ExecutionPolicy Bypass -File $backendDbScript | Out-Host

  Start-Process powershell -ArgumentList @(
    "-NoExit",
    "-ExecutionPolicy", "Bypass",
    "-Command",
    "Set-Location '$backendRoot'; npm run dev"
  ) -WorkingDirectory $backendRoot
}

if (-not (Test-MOBAMMOBackend)) {
  if (-not (Test-Path $backendRoot)) {
    throw "Backend project not found: $backendRoot"
  }

  Start-MOBAMMOBackend

  if (-not (Wait-ForPort -HostName "127.0.0.1" -Port 3000 -TimeoutSeconds 30) -or -not (Test-MOBAMMOBackend)) {
    throw "MOBAMMO backend did not start on http://127.0.0.1:3000. If another app owns port 3000, close it and retry."
  }
}

if (-not (Test-StagedServerProcess)) {
  if (-not (Test-Path $serverScript)) {
    throw "Staged server script not found: $serverScript"
  }

  & $serverScript

  if (-not (Wait-ForStagedServerProcess -TimeoutSeconds 15)) {
    Write-Warning "Dedicated server process was not confirmed within the wait window. Launching client anyway."
  }
}

# Start WebUI dev server if not already running on port 3002
if (-not (Test-PortListening -HostName "127.0.0.1" -Port 3002)) {
  if (Test-Path $webUiRoot) {
    Write-Output "Starting WebUI dev server on port 3002..."
    Start-Process powershell -ArgumentList @(
      "-NoExit",
      "-ExecutionPolicy", "Bypass",
      "-Command",
      "Set-Location '$webUiRoot'; npm run dev -- --port 3002"
    ) -WorkingDirectory $webUiRoot

    if (-not (Wait-ForPort -HostName "127.0.0.1" -Port 3002 -TimeoutSeconds 20)) {
      Write-Warning "WebUI dev server did not start on port 3002 within the wait window. Login screen may show a blank page."
    }
  } else {
    Write-Warning "WebUI project not found at $webUiRoot. Login screen will use fallback UI."
  }
}

Start-Process -FilePath $gameExe -ArgumentList @(
  "/Game/ThirdPerson/Lvl_ThirdPerson?game=/Script/MOBAMMO.MOBAMMOGameMode",
  "-log",
  "-windowed",
  "-ResX=1280",
  "-ResY=720",
  "-UseIrisReplication=1",
  "-net.SubObjects.DefaultUseSubObjectReplicationList=1",
  "-MOBAMMOAutoSession",
  "-MOBAMMOAutoUser=mimet_bp_test",
  "-MOBAMMOAutoCharacter=BPHero"
) -WorkingDirectory (Split-Path -Parent $gameExe)
