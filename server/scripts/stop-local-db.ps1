$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$dataDir = Join-Path $root ".postgres-data"
$pgCtl = "C:\Program Files\PostgreSQL\17\bin\pg_ctl.exe"

if (-not (Test-Path $dataDir)) {
  throw "Local database data directory not found: $dataDir"
}

& $pgCtl stop -D $dataDir -m fast | Out-Null

Write-Output "Local PostgreSQL stopped."
