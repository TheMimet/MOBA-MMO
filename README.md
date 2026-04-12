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

#### Iris Replication And Character Flow Foundation

This update focused on turning the working local join flow into a cleaner multiplayer foundation with Iris enabled and a manual character flow path prepared.

What was added:

- Enabled the `Iris` plugin at the project level and turned on Iris replication with `net.Iris.UseIrisReplication=1`.
- Updated the staged client and staged server launch scripts so local packaged runs explicitly start with Iris replication enabled.
- Added backend support for listing characters after login, caching the character list, tracking character list status, and exposing the selected character id.
- Added a manual character flow guard so old Blueprint auto-create and auto-start calls do not immediately bypass character selection once login succeeds.
- Added explicit backend entry points for `ListCharacters`, `CreateCharacterForCurrentAccount`, `SelectCharacter`, and `StartSessionForSelectedCharacter`.
- Expanded the in-game debug widget so it can show cached characters, character list status, and refresh the character list directly from the overlay.
- Added a native `UMOBAMMOCharacterFlowWidget` overlay that presents a basic character flow UI with refresh, create default character, and continue actions.
- Updated the debug overlay subsystem so it can show or hide the character flow overlay and switch input mode cleanly while the player is in selection/loading flow.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully with Iris enabled and the new character flow code in place.
- The packaged staged client build completes successfully with the updated launch scripts and runtime UI changes.
- The project now has the groundwork for moving from debug bootstrap behavior to a real `login -> character select -> start session -> loading -> travel` flow.

Relevant files:

- `MOBAMMO.uproject`
- `Config/DefaultEngine.ini`
- `Source/MOBAMMO/MOBAMMOBackendSubsystem.cpp`
- `Source/MOBAMMO/MOBAMMOBackendSubsystem.h`
- `Source/MOBAMMO/MOBAMMODebugLoginWidget.cpp`
- `Source/MOBAMMO/MOBAMMODebugLoginWidget.h`
- `Source/MOBAMMO/MOBAMMODebugOverlaySubsystem.cpp`
- `Source/MOBAMMO/MOBAMMODebugOverlaySubsystem.h`
- `Source/MOBAMMO/MOBAMMOCharacterFlowWidget.cpp`
- `Source/MOBAMMO/MOBAMMOCharacterFlowWidget.h`
- `scripts/run-staged-client.ps1`
- `scripts/run-staged-server.ps1`

#### Character Flow UI Integration

This update focused on making the character flow overlay behave more like the real player-facing flow instead of a temporary debug stopgap.

What was added:

- Added a dedicated native character entry widget so each listed character now has an explicit selectable row instead of relying on focus or pressed-state guessing.
- Updated the character flow screen to highlight the currently selected character and rebuild the list from backend state deterministically.
- Improved the character flow subtitle/status messaging so the overlay clearly differentiates between selection, session startup, and server travel.
- Wired the character flow screen to initiate server travel automatically once the session start response succeeds, so the flow can progress without returning to the debug panel.
- Added a Blueprint fallback path for `/Game/WBP_CharacterFlow` similar to the existing debug login panel flow.
- Updated the overlay subsystem to hide the debug panel while the character flow screen is active, keeping the selection/loading experience cleaner.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully with the new character entry widget and flow logic.
- The packaged staged client build/cook/stage completes successfully with the integrated character flow UI.

Relevant files:

- `Source/MOBAMMO/MOBAMMOCharacterEntryWidget.cpp`
- `Source/MOBAMMO/MOBAMMOCharacterEntryWidget.h`
- `Source/MOBAMMO/MOBAMMOCharacterFlowWidget.cpp`
- `Source/MOBAMMO/MOBAMMOCharacterFlowWidget.h`
- `Source/MOBAMMO/MOBAMMODebugOverlaySubsystem.cpp`
- `Source/MOBAMMO/MOBAMMODebugOverlaySubsystem.h`

#### Core Game UI Flow Bootstrap

This update focused on introducing a real player-facing UI chain for login, loading, and in-game HUD instead of relying only on developer-facing overlays.

What was added:

- Added a native login screen widget with a clean local test-account entry point that drives the backend login flow.
- Added a dedicated loading screen widget that reflects session startup and travel states separately from character selection.
- Added a lightweight in-game HUD widget that shows the current replicated session context once the client has joined.
- Added a `UMOBAMMOGameUISubsystem` that automatically creates and manages the login screen, loading screen, and HUD in local game worlds.
- Added Blueprint fallback hooks for `/Game/WBP_LoginScreen`, `/Game/WBP_LoadingScreen`, and `/Game/WBP_GameHUD` so the art/UI layer can swap in Blueprint presentation later without changing the runtime flow code.
- Narrowed the character flow widget so it now handles character selection only, while loading and gameplay presentation are handled by the new UI layer.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully with the new UI subsystem and widgets.
- The packaged staged client build/cook/stage completes successfully with the new UI flow code included.

Relevant files:

- `Source/MOBAMMO/MOBAMMOLoginScreenWidget.cpp`
- `Source/MOBAMMO/MOBAMMOLoginScreenWidget.h`
- `Source/MOBAMMO/MOBAMMOLoadingScreenWidget.cpp`
- `Source/MOBAMMO/MOBAMMOLoadingScreenWidget.h`
- `Source/MOBAMMO/MOBAMMOGameHUDWidget.cpp`
- `Source/MOBAMMO/MOBAMMOGameHUDWidget.h`
- `Source/MOBAMMO/MOBAMMOGameUISubsystem.cpp`
- `Source/MOBAMMO/MOBAMMOGameUISubsystem.h`
- `Source/MOBAMMO/MOBAMMOCharacterFlowWidget.cpp`

#### Iris Replicated Gameplay State Bootstrap

This update focused on moving multiplayer state out of local-only backend memory and into replicated gameplay classes that can travel with the player into the dedicated server world.

What was added:

- Added a native `AMOBAMMOGameMode` that preserves the existing ThirdPerson pawn and player controller Blueprints while assigning custom replicated game state classes.
- Added a replicated `AMOBAMMOPlayerState` with account id, character id, character name, class id, level, health, max health, mana, and max mana.
- Added a replicated `AMOBAMMOGameState` with connected player count so clients can observe basic multiplayer session state directly from gameplay replication.
- Updated client travel so the connect string now carries session identity options such as account id, character id, class id, level, and character name into the dedicated server join.
- Updated the gameplay HUD to read replicated player and game state data instead of only reading cached backend values from the local subsystem.
- Switched the project default game mode to the new C++ game mode so the replicated state classes are consistently active in both client and dedicated server runs.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully with the new replicated gameplay classes.
- The packaged staged client build/cook/stage completes successfully with the replicated game mode and state setup included.

Relevant files:

- `Config/DefaultEngine.ini`
- `Source/MOBAMMO/MOBAMMOBackendSubsystem.cpp`
- `Source/MOBAMMO/MOBAMMOBackendSubsystem.h`
- `Source/MOBAMMO/MOBAMMOGameMode.cpp`
- `Source/MOBAMMO/MOBAMMOGameMode.h`
- `Source/MOBAMMO/MOBAMMOGameState.cpp`
- `Source/MOBAMMO/MOBAMMOGameState.h`
- `Source/MOBAMMO/MOBAMMOPlayerState.cpp`
- `Source/MOBAMMO/MOBAMMOPlayerState.h`
- `Source/MOBAMMO/MOBAMMOGameHUDWidget.cpp`
