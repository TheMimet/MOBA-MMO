$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$webUiRoot = Join-Path (Split-Path -Parent $projectRoot) "LoginAndCharacterFlowDesign"
$port = 3002

if (-not (Test-Path $webUiRoot)) {
  throw "WebUI project not found: $webUiRoot"
}

Write-Output "Starting WebUI dev server on port $port..."

Set-Location $webUiRoot

# Install dependencies if needed
if (-not (Test-Path (Join-Path $webUiRoot "node_modules"))) {
  Write-Output "Installing WebUI dependencies..."
  npm install
}

# Start dev server on the specified port
npm run dev -- --port $port
