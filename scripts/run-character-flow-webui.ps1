$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$workspaceRoot = Split-Path -Parent $projectRoot
$primaryWebUiRoot = Join-Path $workspaceRoot "NewLoginandCharacterFlowDesign"
$secondaryWebUiRoot = Join-Path $workspaceRoot "LoginAndCharacterFlowDesign"
$fallbackWebUiRoot = Join-Path $workspaceRoot "character-flow"
$webUiRoot =
if (Test-Path $primaryWebUiRoot) {
  $primaryWebUiRoot
}
elseif (Test-Path $secondaryWebUiRoot) {
  $secondaryWebUiRoot
}
else {
  $fallbackWebUiRoot
}

if (-not (Test-Path $webUiRoot)) {
  throw "character-flow folder not found: $webUiRoot"
}

if (-not (Test-Path (Join-Path $webUiRoot "package.json"))) {
  throw "character-flow package.json not found: $webUiRoot"
}

$port = 3002
$existing = Get-NetTCPConnection -State Listen -LocalPort $port -ErrorAction SilentlyContinue
if ($existing) {
  Write-Host "character-flow web UI is already listening on http://127.0.0.1:$port"
  exit 0
}

if (-not (Test-Path (Join-Path $webUiRoot "node_modules"))) {
  Write-Host "Installing character-flow dependencies..."
  Push-Location $webUiRoot
  try {
    & npm install
    if ($LASTEXITCODE -ne 0) {
      throw "npm install failed for $webUiRoot"
    }
  }
  finally {
    Pop-Location
  }
}

$command = "cd /d `"$webUiRoot`" && npm run dev -- --hostname 127.0.0.1 --port $port"
Start-Process -FilePath "cmd.exe" -ArgumentList "/c", $command -WorkingDirectory $webUiRoot
