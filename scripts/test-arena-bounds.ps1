param(
  [string]$CharacterName = "BPHero",
  [string]$BackendBaseUrl = "http://127.0.0.1:3000",
  [string]$PgHost = "127.0.0.1",
  [int]$PgPort = 5433,
  [string]$PgUser = "mobammo_app",
  [string]$PgDatabase = "mobammo",
  [string]$PgPassword = "mobammo_app_pw",
  [string]$PsqlPath = "C:\Program Files\PostgreSQL\17\bin\psql.exe",
  [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$runClientScript = Join-Path $PSScriptRoot "run-staged-client.ps1"
$backendRoot = Join-Path $projectRoot "server"
$backendDbScript = Join-Path $projectRoot "server\scripts\start-local-db.ps1"
$serverLogPath = Join-Path $projectRoot "Saved\StagedBuilds\WindowsServer\WindowsServer\MOBAMMO\Saved\Logs\MOBAMMO.log"

$arenaMin = @{
  X = -3500.0
  Y = -2500.0
  Z = -500.0
}
$arenaMax = @{
  X = 3500.0
  Y = 2500.0
  Z = 1500.0
}
$badPosition = @{
  X = 2402.323
  Y = -1087.347
  Z = -992646.574
}

function Invoke-PsqlScalar {
  param([string]$Sql)

  if (-not (Test-Path $PsqlPath)) {
    throw "psql not found: $PsqlPath"
  }

  $env:PGPASSWORD = $PgPassword
  return ($Sql | & $PsqlPath -h $PgHost -p $PgPort -U $PgUser -d $PgDatabase -t -A -F ",").Trim()
}

function Wait-Until {
  param(
    [scriptblock]$Condition,
    [string]$Label,
    [int]$Seconds = $TimeoutSeconds
  )

  $deadline = (Get-Date).AddSeconds($Seconds)
  do {
    $result = & $Condition
    if ($result) {
      return $result
    }

    Start-Sleep -Milliseconds 500
  } while ((Get-Date) -lt $deadline)

  throw "Timed out waiting for $Label."
}

function Wait-ForBackend {
  Wait-Until -Label "backend health at $BackendBaseUrl" -Seconds 30 -Condition {
    try {
      $health = Invoke-RestMethod -Uri "$BackendBaseUrl/health" -TimeoutSec 2
      return $health.service -eq "moba-mmo-server"
    } catch {
      return $false
    }
  } | Out-Null
}

function Test-Backend {
  try {
    $health = Invoke-RestMethod -Uri "$BackendBaseUrl/health" -TimeoutSec 2
    return $health.service -eq "moba-mmo-server"
  } catch {
    return $false
  }
}

function Test-Database {
  try {
    $env:PGPASSWORD = $PgPassword
    $result = ("SELECT 1;" | & $PsqlPath -h $PgHost -p $PgPort -U $PgUser -d $PgDatabase -t -A 2>$null).Trim()
    return $result -eq "1"
  } catch {
    return $false
  }
}

function Start-BackendIfNeeded {
  if (Test-Backend) {
    return
  }

  if (-not (Test-Database)) {
    if (-not (Test-Path $backendDbScript)) {
      throw "Backend database script not found: $backendDbScript"
    }

    & powershell -NoProfile -ExecutionPolicy Bypass -File $backendDbScript | Out-Host
  }

  if (-not (Test-Path $backendRoot)) {
    throw "Backend project not found: $backendRoot"
  }

  Start-Process powershell -WindowStyle Hidden -ArgumentList @(
    "-NoExit",
    "-ExecutionPolicy", "Bypass",
    "-Command",
    "Set-Location '$backendRoot'; npm run dev"
  ) -WorkingDirectory $backendRoot

  Wait-ForBackend
}

function Get-CharacterRuntimeRow {
  $escapedCharacterName = $CharacterName.Replace("'", "''")
  $row = Invoke-PsqlScalar @"
SELECT c.id, c."positionX", c."positionY", c."positionZ", cs.id, cs.status, cs."updatedAt",
       EXTRACT(EPOCH FROM ((NOW() AT TIME ZONE 'UTC') - cs."updatedAt"))
FROM "Character" c
LEFT JOIN "CharacterSession" cs ON cs."characterId" = c.id
WHERE c.name = '$escapedCharacterName'
LIMIT 1;
"@

  if (-not $row) {
    return $null
  }

  $parts = $row.Split(",")
  if ($parts.Count -lt 8) {
    return $null
  }

  return [pscustomobject]@{
    CharacterId = $parts[0]
    PositionX = [double]$parts[1]
    PositionY = [double]$parts[2]
    PositionZ = [double]$parts[3]
    SessionId = $parts[4]
    Status = $parts[5]
    UpdatedAt = $parts[6]
    AgeSeconds = [double]$parts[7]
  }
}

function Assert-InArena {
  param([object]$Row)

  if (
    $Row.PositionX -lt $arenaMin.X -or $Row.PositionX -gt $arenaMax.X -or
    $Row.PositionY -lt $arenaMin.Y -or $Row.PositionY -gt $arenaMax.Y -or
    $Row.PositionZ -lt $arenaMin.Z -or $Row.PositionZ -gt $arenaMax.Z
  ) {
    throw "Character remained outside arena bounds: $($Row.PositionX),$($Row.PositionY),$($Row.PositionZ)"
  }
}

Get-Process -Name "MOBAMMO", "MOBAMMOServer" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

Start-BackendIfNeeded

$escapedCharacterName = $CharacterName.Replace("'", "''")
$badRow = Invoke-PsqlScalar @"
UPDATE "Character"
SET "positionX" = $($badPosition.X),
    "positionY" = $($badPosition.Y),
    "positionZ" = $($badPosition.Z),
    "updatedAt" = (NOW() AT TIME ZONE 'UTC')
WHERE name = '$escapedCharacterName'
RETURNING id, "positionX", "positionY", "positionZ";
"@

if (-not $badRow) {
  throw "Character not found: $CharacterName"
}

$null = Invoke-PsqlScalar @"
UPDATE "CharacterSession"
SET status = 'ended',
    "updatedAt" = (NOW() AT TIME ZONE 'UTC') - INTERVAL '3 minutes'
WHERE "characterId" = (
  SELECT id FROM "Character" WHERE name = '$escapedCharacterName' LIMIT 1
);
"@

Write-Output "Seeded unsafe saved position for ${CharacterName}: $badRow"

if (-not (Test-Path $runClientScript)) {
  throw "run-staged-client script not found: $runClientScript"
}

& powershell -NoProfile -ExecutionPolicy Bypass -File $runClientScript | Out-Host
Wait-ForBackend

$activeRow = Wait-Until -Label "active arena-safe runtime session for $CharacterName" -Condition {
  $row = Get-CharacterRuntimeRow
  if ($row -and $row.Status -eq "active" -and $row.SessionId -and $row.AgeSeconds -le 20.0) {
    Assert-InArena -Row $row
    return $row
  }

  return $false
}

if ([Math]::Abs($activeRow.PositionZ - $badPosition.Z) -lt 1.0) {
  throw "Unsafe saved Z was reused instead of being clamped."
}

if (Test-Path $serverLogPath) {
  $restoreMatch = Select-String -Path $serverLogPath -Pattern "Restored player pawn to saved position" | Select-Object -Last 1
  if (-not $restoreMatch) {
    Write-Warning "Server log did not contain a saved-position restore entry yet; DB/session arena safety already passed."
  }
}

Write-Output "Arena bounds smoke passed."
Write-Output "Unsafe seed: $($badPosition.X),$($badPosition.Y),$($badPosition.Z)"
Write-Output "Runtime position: $($activeRow.PositionX),$($activeRow.PositionY),$($activeRow.PositionZ)"
Write-Output "Session: $($activeRow.SessionId) -> $($activeRow.Status)"
