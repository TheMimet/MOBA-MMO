# MOBA-MMO

MOBA MMORG Gaming Development

## Development Log

### 2026-04-12

#### Dedicated Server Join Flow Stabilization

This update focused on getting the local backend -> session creation -> staged dedicated server join flow working reliably end to end.

What was fixed:

- Fixed the staged server launcher to start the real dedicated server binary from `Saved/StagedBuilds/WindowsServer/WindowsServer/MOBAMMO/Binaries/Win64/MOBAMMOServer.exe`.
- Fixed the staged client launcher so it opens the packaged client normally and lets the backend session response drive travel instead of forcing an early direct connect.
- Added backend subsystem state tracking for login, character creation, session start, last username, and last error message to make the flow easier to debug from code and Blueprint.
- Prevented the client from restarting the mock login flow after it had already joined the dedicated server. This was the main source of the reconnect loop.
- Removed temporary on-screen debug messages after the flow was verified, while keeping useful log output in the Unreal log.
- Updated character creation on the backend so repeated local test runs can safely reuse an existing `(accountId, name)` character instead of failing on duplicate creation.
- Added and used a `build-cook-stage-client.ps1` helper so the client package can be rebuilt consistently during local verification.

Validated result:

- Local backend login succeeds.
- Character creation/session bootstrap succeeds.
- The staged client receives `Welcomed by server`.
- Travel completes successfully into `/Game/ThirdPerson/Lvl_ThirdPerson`.
- The backend bootstrap is skipped once the world is already a network client, so the previous loop no longer restarts.

Relevant files:

- `Source/MOBAMMO/MOBAMMOBackendSubsystem.cpp`
- `Source/MOBAMMO/MOBAMMOBackendSubsystem.h`
- `scripts/run-staged-client.ps1`
- `scripts/run-staged-server.ps1`
- `scripts/build-cook-stage-client.ps1`
- `server/src/modules/characters/routes.ts`

#### Debug Overlay Bootstrap

This follow-up update focused on making the backend/session flow observable in-game without relying on log copy-paste.

What was added:

- Added a native debug login widget class that can display backend URL, login status, character status, session status, last error, last username, account id, character id, and connect string.
- Added widget actions for `Mock Login`, `Create Character`, `Start Session`, and `Join Server`.
- Added a lightweight debug overlay subsystem that automatically creates the debug panel in local game worlds.
- Added a fallback chain so the game tries `/Game/WBP_DebugLogin` first and falls back to the native C++ widget if the Blueprint class is not ready.
- Added a general backend debug state change signal so the panel can refresh immediately when requests start, fail, succeed, or travel begins.
- Added `UMG` as a module dependency for the new runtime debug UI.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully with the new widget and overlay subsystem.
- Staged client build/cook/stage completes successfully with the new debug overlay code included.

Relevant files:

- `Source/MOBAMMO/MOBAMMO.Build.cs`
- `Source/MOBAMMO/MOBAMMOBackendSubsystem.cpp`
- `Source/MOBAMMO/MOBAMMOBackendSubsystem.h`
- `Source/MOBAMMO/MOBAMMODebugLoginWidget.cpp`
- `Source/MOBAMMO/MOBAMMODebugLoginWidget.h`
- `Source/MOBAMMO/MOBAMMODebugOverlaySubsystem.cpp`
- `Source/MOBAMMO/MOBAMMODebugOverlaySubsystem.h`
