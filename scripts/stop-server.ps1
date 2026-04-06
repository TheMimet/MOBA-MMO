$ErrorActionPreference = "Stop"

Get-Process MOBAMMOServer -ErrorAction SilentlyContinue | Stop-Process -Force
