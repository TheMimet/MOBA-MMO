# Persistence Operations

This project now treats character persistence like an MMO service:

- Saves are automatic; there is no manual save button.
- Every save belongs to the currently active `sessionId`.
- Old, ended, timed-out, or replaced sessions cannot write character state.
- The backend rejects invalid positions and suspicious movement deltas.
- The client HUD shows `SAVE OK`, `SAVING`, `SAVE ISSUE`, or `RECONNECT [R]`.
- Pressing `R` while reconnect is required starts a fresh session and travels back into the active game server.

## Backend Flow

1. `POST /session/start` creates a new active session and returns the latest character snapshot.
2. `POST /characters/:id/save` writes position, stats, combat counters, and save sequence for the active session.
3. `POST /session/heartbeat` refreshes `CharacterSession.updatedAt` without writing full character state.
4. `POST /session/end` performs the final save and marks the session as `ended`.
5. If a session exceeds `SESSION_TIMEOUT_SECONDS`, heartbeat/save marks it as `timed_out` and returns `409`.
6. A background session reaper scans stale `active` sessions every `SESSION_REAPER_INTERVAL_SECONDS` seconds and marks them `timed_out`.

## Runtime Flow

1. Client logs in, selects a character, and starts a session.
2. Travel options replicate account, character, session, position, appearance, and stats to the dedicated server.
3. The server restores the pawn to the persisted position after spawn.
4. Autosave runs on a server timer, while heartbeat runs separately as a lighter liveness signal.
5. Logout calls `/session/end`; abrupt disconnects are handled by timeout.

## Smoke Test

Run the backend persistence smoke test:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test-session-resume.ps1
```

Run the staged runtime reconnect smoke test:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test-runtime-reconnect.ps1
```

Inspect in-memory persistence telemetry:

```powershell
Invoke-RestMethod http://127.0.0.1:3000/ops/persistence
```

Tracked counters include accepted saves, stale ignored saves, rejected persistence requests, timed-out sessions, reconnect attempts, accepted heartbeats, started sessions, and ended sessions. These counters are process-local and reset when the backend restarts. The ops response also exposes the active session timeout and reaper interval.

Persistence routes now enforce signed bearer tokens issued by `/auth/login`:

```http
Authorization: Bearer <signed-auth-token>
```

Character list/create/load/save and session start/end/heartbeat reject missing, invalid, expired, or non-owner tokens for player-driven requests.

Dedicated server persistence writes use a separate service header instead of the player's bearer token:

```http
X-Session-Server-Secret: <session-server-secret>
```

The Unreal client no longer puts the signed user token into travel URLs. Dedicated server autosave, heartbeat, and logout final-save authenticate as the session server, while the active `characterId` + `sessionId` still gate which session can write state.

The test verifies:

- Ended sessions cannot save.
- Active sessions can heartbeat.
- Timed-out sessions cannot heartbeat or save.
- A fresh session resumes from the last accepted position.
- The fresh session can save again.

## Current Next Roadmap

1. Rotate `SESSION_SERVER_SECRET` through environment-specific secret management instead of checked-in dev defaults.
2. Persist telemetry to an ops table or metrics backend instead of process memory.
