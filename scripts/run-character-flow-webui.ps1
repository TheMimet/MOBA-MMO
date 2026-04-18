$ErrorActionPreference = "Stop"

throw @"
The external character-flow web UI prototypes have been detached from the active MOBAMMO runtime.

Canonical project root:
  C:\Users\PC\OneDrive\Desktop\MOBA MMO\MOBAMMO

Use this project only:
  - TamBaslatma.bat for the staged DB/backend/server/client flow
  - scripts\run-staged-client.ps1 for the same flow from PowerShell

The login and character flow now live in native Unreal UI code inside this MOBAMMO project.
"@
