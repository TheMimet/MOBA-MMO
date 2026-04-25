param(
  [string]$CharacterName = "BPHero",
  [string]$BackendBaseUrl = "http://127.0.0.1:3000",
  [string]$PgHost = "127.0.0.1",
  [int]$PgPort = 5433,
  [string]$PgUser = "mobammo_app",
  [string]$PgDatabase = "mobammo",
  [string]$PgPassword = "mobammo_app_pw",
  [string]$PsqlPath = "C:\Program Files\PostgreSQL\17\bin\psql.exe"
)

$ErrorActionPreference = "Stop"
$script:AuthHeaders = @{}

function Invoke-PsqlScalar {
  param([string]$Sql)

  if (-not (Test-Path $PsqlPath)) {
    throw "psql not found: $PsqlPath"
  }

  $env:PGPASSWORD = $PgPassword
  return ($Sql | & $PsqlPath -h $PgHost -p $PgPort -U $PgUser -d $PgDatabase -t -A -F ",").Trim()
}

function Invoke-JsonRequest {
  param(
    [string]$Method,
    [string]$Uri,
    [object]$Body,
    [hashtable]$Headers = $script:AuthHeaders
  )

  $jsonBody = $null
  if ($null -ne $Body) {
    $jsonBody = $Body | ConvertTo-Json -Depth 8
  }

  try {
    $requestArgs = @{
      Method = $Method
      Uri = $Uri
      ContentType = "application/json"
      UseBasicParsing = $true
    }
    if ($null -ne $jsonBody) {
      $requestArgs.Body = $jsonBody
    }
    if ($Headers -and $Headers.Count -gt 0) {
      $requestArgs.Headers = $Headers
    }

    $response = Invoke-WebRequest @requestArgs
    $json = if ($response.Content) { $response.Content | ConvertFrom-Json } else { $null }
    return [pscustomobject]@{
      StatusCode = [int]$response.StatusCode
      Json = $json
      Content = $response.Content
    }
  } catch {
    $statusCode = [int]$_.Exception.Response.StatusCode
    $content = ""
    if ($_.Exception.Response) {
      $reader = New-Object System.IO.StreamReader($_.Exception.Response.GetResponseStream())
      $content = $reader.ReadToEnd()
    }

    $json = if ($content) { $content | ConvertFrom-Json } else { $null }
    return [pscustomobject]@{
      StatusCode = $statusCode
      Json = $json
      Content = $content
    }
  }
}

function Assert-Status {
  param(
    [object]$Response,
    [int]$ExpectedStatus,
    [string]$Label
  )

  if ($Response.StatusCode -ne $ExpectedStatus) {
    throw "$Label failed. Expected HTTP $ExpectedStatus, got HTTP $($Response.StatusCode). Body: $($Response.Content)"
  }
}

function Assert-Near {
  param(
    [double]$Actual,
    [double]$Expected,
    [double]$Tolerance,
    [string]$Label
  )

  if ([Math]::Abs($Actual - $Expected) -gt $Tolerance) {
    throw "$Label mismatch. Expected $Expected, got $Actual."
  }
}

function Wait-ForBackend {
  $deadline = (Get-Date).AddSeconds(20)
  do {
    try {
      $health = Invoke-RestMethod -Uri "$BackendBaseUrl/health" -TimeoutSec 2
      if ($health.service -eq "moba-mmo-server") {
        return
      }
    } catch {
      Start-Sleep -Milliseconds 500
    }
  } while ((Get-Date) -lt $deadline)

  throw "Backend is not ready at $BackendBaseUrl."
}

Wait-ForBackend

$escapedCharacterName = $CharacterName.Replace("'", "''")
$characterId = Invoke-PsqlScalar "SELECT id FROM ""Character"" WHERE name='$escapedCharacterName' LIMIT 1;"
if (-not $characterId) {
  throw "Character not found: $CharacterName"
}

$accountRow = Invoke-PsqlScalar "SELECT a.id, a.username FROM ""Account"" a JOIN ""Character"" c ON c.""accountId"" = a.id WHERE c.id='$characterId' LIMIT 1;"
$accountParts = $accountRow.Split(",")
if ($accountParts.Count -lt 2) {
  throw "Could not read owning account for $CharacterName."
}

$accountId = $accountParts[0]
$username = $accountParts[1]
$login = Invoke-JsonRequest -Method "POST" -Uri "$BackendBaseUrl/auth/login" -Body @{ username = $username } -Headers @{}
Assert-Status -Response $login -ExpectedStatus 200 -Label "Owner login"
if ($login.Json.accountId -ne $accountId) {
  throw "Owner login returned account $($login.Json.accountId), expected $accountId."
}

$ownerAuthHeaders = @{ Authorization = "Bearer $($login.Json.token)" }
$intruderLogin = Invoke-JsonRequest -Method "POST" -Uri "$BackendBaseUrl/auth/login" -Body @{ username = "ownership_intruder" } -Headers @{}
Assert-Status -Response $intruderLogin -ExpectedStatus 200 -Label "Intruder login"
$script:AuthHeaders = @{ Authorization = "Bearer $($intruderLogin.Json.token)" }
$forbiddenStart = Invoke-JsonRequest -Method "POST" -Uri "$BackendBaseUrl/session/start" -Body @{ characterId = $characterId }
Assert-Status -Response $forbiddenStart -ExpectedStatus 403 -Label "Intruder session start rejection"

$script:AuthHeaders = $ownerAuthHeaders

$positionRow = Invoke-PsqlScalar "SELECT ""positionX"", ""positionY"", ""positionZ"" FROM ""Character"" WHERE id='$characterId';"
$parts = $positionRow.Split(",")
if ($parts.Count -lt 3) {
  throw "Could not read current position for $CharacterName."
}

$currentX = [double]$parts[0]
$currentY = [double]$parts[1]
$currentZ = [double]$parts[2]
$resumeX = [Math]::Round($currentX + 12.5, 3)
$resumeY = [Math]::Round($currentY + 8.25, 3)
$resumeZ = [Math]::Round($currentZ, 3)
$nextX = [Math]::Round($resumeX + 3.75, 3)
$nextY = [Math]::Round($resumeY + 2.5, 3)
$nextZ = $resumeZ
$finalX = [Math]::Round($nextX + 3.75, 3)
$finalY = [Math]::Round($nextY + 2.5, 3)
$finalZ = $nextZ
$baseSequence = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()

$start1 = Invoke-JsonRequest -Method "POST" -Uri "$BackendBaseUrl/session/start" -Body @{ characterId = $characterId }
Assert-Status -Response $start1 -ExpectedStatus 200 -Label "Initial session start"
$session1 = $start1.Json.sessionId

$end1 = Invoke-JsonRequest -Method "POST" -Uri "$BackendBaseUrl/session/end" -Body @{
  characterId = $characterId
  sessionId = $session1
  position = @{ x = $resumeX; y = $resumeY; z = $resumeZ }
  hp = 72
  maxHp = 100
  mana = 30
  maxMana = 50
  killCount = 0
  deathCount = 0
  saveSequence = $baseSequence
}
Assert-Status -Response $end1 -ExpectedStatus 202 -Label "Session end final save"
if ($end1.Json.status -ne "ended") {
  throw "Session end returned unexpected status: $($end1.Json.status)"
}

$staleSave = Invoke-JsonRequest -Method "POST" -Uri "$BackendBaseUrl/characters/$characterId/save" -Body @{
  sessionId = $session1
  position = @{ x = $nextX; y = $nextY; z = $nextZ }
  saveSequence = ($baseSequence + 1)
}
Assert-Status -Response $staleSave -ExpectedStatus 409 -Label "Ended session stale save rejection"

$start2 = Invoke-JsonRequest -Method "POST" -Uri "$BackendBaseUrl/session/start" -Body @{ characterId = $characterId }
Assert-Status -Response $start2 -ExpectedStatus 200 -Label "Resume session start"
$session2 = $start2.Json.sessionId
if ($session2 -eq $session1) {
  throw "Resume session reused the ended session id."
}

Assert-Near -Actual ([double]$start2.Json.character.position.x) -Expected $resumeX -Tolerance 0.01 -Label "Resume X"
Assert-Near -Actual ([double]$start2.Json.character.position.y) -Expected $resumeY -Tolerance 0.01 -Label "Resume Y"
Assert-Near -Actual ([double]$start2.Json.character.position.z) -Expected $resumeZ -Tolerance 0.01 -Label "Resume Z"

$heartbeat2 = Invoke-JsonRequest -Method "POST" -Uri "$BackendBaseUrl/session/heartbeat" -Body @{
  characterId = $characterId
  sessionId = $session2
}
Assert-Status -Response $heartbeat2 -ExpectedStatus 202 -Label "Active session heartbeat"
if ($heartbeat2.Json.status -ne "active") {
  throw "Heartbeat returned unexpected status: $($heartbeat2.Json.status)"
}

$activeSave = Invoke-JsonRequest -Method "POST" -Uri "$BackendBaseUrl/characters/$characterId/save" -Body @{
  sessionId = $session2
  position = @{ x = $nextX; y = $nextY; z = $nextZ }
  hp = 72
  maxHp = 100
  mana = 30
  maxMana = 50
  killCount = 0
  deathCount = 0
  saveSequence = ($baseSequence + 2)
}
Assert-Status -Response $activeSave -ExpectedStatus 202 -Label "Active resumed session save"
if ($activeSave.Json.status -ne "saved") {
  throw "Active save returned unexpected status: $($activeSave.Json.status)"
}

$null = Invoke-PsqlScalar "UPDATE ""CharacterSession"" SET ""updatedAt"" = (NOW() AT TIME ZONE 'UTC') - INTERVAL '3 minutes' WHERE ""characterId""='$characterId';"

$expiredHeartbeat = Invoke-JsonRequest -Method "POST" -Uri "$BackendBaseUrl/session/heartbeat" -Body @{
  characterId = $characterId
  sessionId = $session2
}
Assert-Status -Response $expiredHeartbeat -ExpectedStatus 409 -Label "Timed out session heartbeat rejection"
if ($null -ne $expiredHeartbeat.Json -and $expiredHeartbeat.Json.error -ne "session_expired") {
  throw "Timed out heartbeat returned unexpected error: $($expiredHeartbeat.Json.error)"
}

$expiredSave = Invoke-JsonRequest -Method "POST" -Uri "$BackendBaseUrl/characters/$characterId/save" -Body @{
  sessionId = $session2
  position = @{ x = $finalX; y = $finalY; z = $finalZ }
  saveSequence = ($baseSequence + 3)
}
Assert-Status -Response $expiredSave -ExpectedStatus 409 -Label "Timed out session save rejection"
if ($null -ne $expiredSave.Json -and $expiredSave.Json.error -notin @("session_expired", "stale_or_invalid_session")) {
  throw "Timed out save returned unexpected error: $($expiredSave.Json.error)"
}

$timedOutStatus = Invoke-PsqlScalar "SELECT status FROM ""CharacterSession"" WHERE ""characterId""='$characterId';"
if ($timedOutStatus -ne "timed_out") {
  throw "Expected timed out session status, got $timedOutStatus."
}

$start3 = Invoke-JsonRequest -Method "POST" -Uri "$BackendBaseUrl/session/start" -Body @{ characterId = $characterId }
Assert-Status -Response $start3 -ExpectedStatus 200 -Label "Post-timeout resume session start"
$session3 = $start3.Json.sessionId
if ($session3 -eq $session2) {
  throw "Post-timeout resume reused the timed out session id."
}

Assert-Near -Actual ([double]$start3.Json.character.position.x) -Expected $nextX -Tolerance 0.01 -Label "Post-timeout Resume X"
Assert-Near -Actual ([double]$start3.Json.character.position.y) -Expected $nextY -Tolerance 0.01 -Label "Post-timeout Resume Y"
Assert-Near -Actual ([double]$start3.Json.character.position.z) -Expected $nextZ -Tolerance 0.01 -Label "Post-timeout Resume Z"

$postTimeoutSave = Invoke-JsonRequest -Method "POST" -Uri "$BackendBaseUrl/characters/$characterId/save" -Body @{
  sessionId = $session3
  position = @{ x = $finalX; y = $finalY; z = $finalZ }
  hp = 72
  maxHp = 100
  mana = 30
  maxMana = 50
  killCount = 0
  deathCount = 0
  saveSequence = ($baseSequence + 4)
}
Assert-Status -Response $postTimeoutSave -ExpectedStatus 202 -Label "Post-timeout active session save"
if ($postTimeoutSave.Json.status -ne "saved") {
  throw "Post-timeout active save returned unexpected status: $($postTimeoutSave.Json.status)"
}

$finalRow = Invoke-PsqlScalar "SELECT c.""positionX"", c.""positionY"", c.""positionZ"", cs.status, cs.id FROM ""Character"" c LEFT JOIN ""CharacterSession"" cs ON cs.""characterId"" = c.id WHERE c.id='$characterId';"
$finalParts = $finalRow.Split(",")
if ($finalParts.Count -lt 5) {
  throw "Could not read final session row."
}

Assert-Near -Actual ([double]$finalParts[0]) -Expected $finalX -Tolerance 0.01 -Label "Final DB X"
Assert-Near -Actual ([double]$finalParts[1]) -Expected $finalY -Tolerance 0.01 -Label "Final DB Y"
Assert-Near -Actual ([double]$finalParts[2]) -Expected $finalZ -Tolerance 0.01 -Label "Final DB Z"
if ($finalParts[3] -ne "active") {
  throw "Expected final session status active, got $($finalParts[3])."
}
if ($finalParts[4] -ne $session3) {
  throw "Expected final active session $session3, got $($finalParts[4])."
}

Write-Output "PASS session resume smoke test"
Write-Output "Character: $CharacterName ($characterId)"
Write-Output "Owner auth enforced for account: $accountId"
Write-Output "Ended session rejected: $session1"
Write-Output "Timed out heartbeat/save rejected: $session2"
Write-Output "Active resumed session: $session3"
Write-Output "Final position: $finalX, $finalY, $finalZ"
