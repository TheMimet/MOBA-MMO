$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $false

$root = Split-Path -Parent $PSScriptRoot
$dataDir = Join-Path $root ".postgres-data"
$logDir = Join-Path $root ".postgres-log"
$pwFile = Join-Path $root ".pgpass"
$pgCtl = "C:\Program Files\PostgreSQL\17\bin\pg_ctl.exe"
$psql = "C:\Program Files\PostgreSQL\17\bin\psql.exe"
$isReady = "C:\Program Files\PostgreSQL\17\bin\pg_isready.exe"

if (-not (Test-Path $dataDir)) {
  throw "Local database data directory not found: $dataDir"
}

if (-not (Test-Path $logDir)) {
  New-Item -ItemType Directory -Force -Path $logDir | Out-Null
}

$env:PGPASSWORD = Get-Content -LiteralPath $pwFile -Raw
$env:PGPASSWORD = $env:PGPASSWORD.Trim()

& $isReady -h 127.0.0.1 -p 5433 -U postgres *> $null
if ($LASTEXITCODE -ne 0) {
  & $pgCtl start -D $dataDir -l (Join-Path $logDir "postgres.log") -o '"-p 5433"' -w | Out-Null
}

Set-Variable -Name dbExists -Value (& $psql -h 127.0.0.1 -p 5433 -U postgres -d postgres -tAc "SELECT 1 FROM pg_database WHERE datname = 'mobammo'")
if (-not $dbExists.Trim()) {
  & $psql -h 127.0.0.1 -p 5433 -U postgres -d postgres -c "CREATE DATABASE mobammo;" | Out-Null
}

Set-Variable -Name roleExists -Value (& $psql -h 127.0.0.1 -p 5433 -U postgres -d postgres -tAc "SELECT 1 FROM pg_roles WHERE rolname = 'mobammo_app'")
if (-not $roleExists.Trim()) {
  & $psql -h 127.0.0.1 -p 5433 -U postgres -d postgres -c "CREATE ROLE mobammo_app WITH LOGIN PASSWORD 'mobammo_app_pw';" | Out-Null
} else {
  & $psql -h 127.0.0.1 -p 5433 -U postgres -d postgres -c "ALTER ROLE mobammo_app WITH PASSWORD 'mobammo_app_pw';" | Out-Null
}

& $psql -h 127.0.0.1 -p 5433 -U postgres -d postgres -c "ALTER DATABASE mobammo OWNER TO mobammo_app;" | Out-Null
& $psql -h 127.0.0.1 -p 5433 -U postgres -d postgres -c "GRANT ALL PRIVILEGES ON DATABASE mobammo TO mobammo_app;" | Out-Null

Write-Output "Local PostgreSQL is running on 127.0.0.1:5433"
