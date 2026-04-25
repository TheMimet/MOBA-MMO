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
$clientLogPath = Join-Path $projectRoot "Saved\StagedBuilds\WindowsGame\Windows\MOBAMMO\Saved\Logs\MOBAMMO.log"
$serverLogPath = Join-Path $projectRoot "Saved\StagedBuilds\WindowsServer\WindowsServer\MOBAMMO\Saved\Logs\MOBAMMO.log"

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

function Get-RuntimeSessionRow {
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

function Start-GameIfNeeded {
  $client = Get-Process -Name "MOBAMMO" -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($client) {
    return
  }

  if (-not (Test-Path $runClientScript)) {
    throw "run-staged-client script not found: $runClientScript"
  }

  & powershell -NoProfile -ExecutionPolicy Bypass -File $runClientScript | Out-Host
}

function Send-ReconnectKey {
  if (-not ("Win32Input" -as [type])) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32Input {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
}
"@
  }

  $client = Get-Process -Name "MOBAMMO" -ErrorAction SilentlyContinue |
    Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object -First 1

  if (-not $client) {
    throw "MOBAMMO client window not found."
  }

  [Win32Input]::ShowWindow($client.MainWindowHandle, 9) | Out-Null
  [Win32Input]::SetForegroundWindow($client.MainWindowHandle) | Out-Null
  Start-Sleep -Milliseconds 500
  [Win32Input]::keybd_event(0x52, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 120
  [Win32Input]::keybd_event(0x52, 0, 2, [UIntPtr]::Zero)
  Write-Output "Sent reconnect key R to MOBAMMO PID $($client.Id)."
}

function Assert-LogContains {
  param(
    [string]$Path,
    [string]$Pattern,
    [string]$Label
  )

  if (-not (Test-Path $Path)) {
    throw "$Label log not found: $Path"
  }

  $match = Select-String -Path $Path -Pattern $Pattern | Select-Object -Last 1
  if (-not $match) {
    throw "$Label did not contain expected pattern: $Pattern"
  }
}

Wait-ForBackend
Start-GameIfNeeded

$activeBefore = Wait-Until -Label "active runtime session for $CharacterName" -Condition {
  $row = Get-RuntimeSessionRow
  if ($row -and $row.Status -eq "active" -and $row.SessionId -and $row.AgeSeconds -le 20.0) {
    return $row
  }

  return $false
}

Write-Output "Active session before timeout: $($activeBefore.SessionId) at $($activeBefore.PositionX),$($activeBefore.PositionY),$($activeBefore.PositionZ) age=$([Math]::Round($activeBefore.AgeSeconds, 2))s"

$null = Invoke-PsqlScalar @"
UPDATE "CharacterSession"
SET "updatedAt" = (NOW() AT TIME ZONE 'UTC') - INTERVAL '3 minutes'
WHERE "characterId" = '$($activeBefore.CharacterId)'
  AND status = 'active';
"@

$timedOut = Wait-Until -Label "server to mark session timed_out" -Seconds 45 -Condition {
  $row = Get-RuntimeSessionRow
  if ($row -and $row.SessionId -eq $activeBefore.SessionId -and $row.Status -eq "timed_out") {
    return $row
  }

  if ($row -and $row.SessionId -ne $activeBefore.SessionId) {
    throw "Session changed before timeout was observed. Expected $($activeBefore.SessionId), found $($row.SessionId) with status $($row.Status)."
  }

  return $false
}

Write-Output "Session timed out as expected: $($timedOut.SessionId)"
Send-ReconnectKey
$script:LastReconnectKeySentAt = Get-Date

$activeAfter = Wait-Until -Label "fresh active session after reconnect" -Seconds 45 -Condition {
  $row = Get-RuntimeSessionRow
  if ($row -and $row.Status -eq "active" -and $row.SessionId -and $row.SessionId -ne $activeBefore.SessionId) {
    return $row
  }

  if (((Get-Date) - $script:LastReconnectKeySentAt).TotalSeconds -ge 5) {
    Send-ReconnectKey
    $script:LastReconnectKeySentAt = Get-Date
  }

  return $false
}

Assert-LogContains -Path $clientLogPath -Pattern "Reconnect starting" -Label "client"
Assert-LogContains -Path $serverLogPath -Pattern "Restored player pawn to saved position" -Label "server"

Write-Output "Runtime reconnect smoke passed."
Write-Output "Old session: $($activeBefore.SessionId) -> timed_out"
Write-Output "New session: $($activeAfter.SessionId) -> active"
Write-Output "Resumed position: $($activeAfter.PositionX),$($activeAfter.PositionY),$($activeAfter.PositionZ)"
