$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$stageRoot = Join-Path $projectRoot "Saved\StagedBuilds\WindowsGame\Windows"
$gameExe = Join-Path $stageRoot "MOBAMMO\Binaries\Win64\MOBAMMO.exe"
$serverScript = Join-Path $PSScriptRoot "run-staged-server.ps1"
$backendRoot = Join-Path $projectRoot "server"
$backendDbScript = Join-Path $backendRoot "scripts\start-local-db.ps1"
$defaultMap = "/Game/ThirdPerson/Lvl_ThirdPerson?game=/Script/MOBAMMO.MOBAMMOGameMode"

if (-not (Test-Path $gameExe)) {
  throw "Staged game executable not found: $gameExe"
}

if (-not (Test-Path $serverScript)) {
  throw "Staged server script not found: $serverScript"
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
    [int]$TimeoutSeconds = 30
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

function Start-MOBAMMOClient {
  param(
    [string]$Label,
    [int]$WinX,
    [int]$WinY
  )

  Write-Host "[MOBAMMO] Starting $Label staged client..."
  Start-Process -FilePath $gameExe -ArgumentList @(
    $defaultMap,
    "-log",
    "-windowed",
    "-WinX=$WinX",
    "-WinY=$WinY",
    "-ResX=960",
    "-ResY=540",
    "-UseIrisReplication=1",
    "-net.SubObjects.DefaultUseSubObjectReplicationList=1"
  ) -WorkingDirectory (Split-Path -Parent $gameExe)
}

if (-not (Test-MOBAMMOBackend)) {
  if (-not (Test-Path $backendRoot)) {
    throw "Backend project not found: $backendRoot"
  }

  Write-Host "[MOBAMMO] Backend is not healthy; starting DB/backend first..."
  Start-MOBAMMOBackend

  if (-not (Wait-ForPort -HostName "127.0.0.1" -Port 3000 -TimeoutSeconds 30) -or -not (Test-MOBAMMOBackend)) {
    throw "MOBAMMO backend did not start on http://127.0.0.1:3000. If another app owns port 3000, close it and retry."
  }
}

if (-not (Test-StagedServerProcess)) {
  Write-Host "[MOBAMMO] Dedicated server is not running; starting staged server..."
  & $serverScript

  if (-not (Wait-ForPort -HostName "127.0.0.1" -Port 7777 -TimeoutSeconds 20)) {
    Write-Warning "Dedicated server port 7777 was not confirmed within the wait window. Launching clients anyway."
  }
}

Start-MOBAMMOClient -Label "Client 1" -WinX 40 -WinY 60
Start-Sleep -Seconds 4
Start-MOBAMMOClient -Label "Client 2" -WinX 1040 -WinY 60

Write-Host ""
Write-Host "[MOBAMMO] Two-client Iris gameplay smoke launched."
Write-Host "[MOBAMMO] Test flow:"
Write-Host "  1. Login both clients and enter the same dedicated session."
Write-Host "  2. On one client, select Sparring Minion with E/6 or left-click."
Write-Host "  3. Use 1 or stand close enough for Minion Pulse."
Write-Host "  4. Verify both HUDs show roster HP/MP/target and AI AGGRO countdown consistently."
