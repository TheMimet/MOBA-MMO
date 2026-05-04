# MOBA-MMO

MOBA MMORG Gaming Development

## Canonical Workspace

The active project root is:

`C:\Users\PC\OneDrive\Desktop\MOBA MMO\MOBAMMO`

Manage and launch the game from this folder only. Older sibling web-design prototype folders such as `character-flow`, `LoginAndCharacterFlowDesign`, and `NewLoginandCharacterFlowDesign` are no longer runtime dependencies. The login and character flow now live in native Unreal UI code inside this project.

Use `TamBaslatma.bat` for the normal one-click local flow: database/backend, dedicated server, and staged client.

Persistence/session operations are documented in `docs/persistence-operations.md`.

## Server Roadmap (Next)

- Logout + final save kapanis akisini client ve dedicated server tarafinda tamamlamak.
- Session-server secret rotasyonu icin deploy ortami secret kaynagini netlestirmek (bkz. `docs/persistence-operations.md`).
- Persistence telemetry icin latency/hata/multiplayer metriklerini genisletmek.
- Ops smoke testlerini (resume/reconnect/arena bounds) duzenli calistirma ve otomasyon planini netlestirmek.
- Gameplay server oncelikleri: ability execution pipeline (cooldown/mana), server-dogrulama hasar/heal kaynagi, temel combat event loglama.

## Development Log

### 2026-04-17

#### AI Sparring Minion Vertical Slice

This update added the first visible AI-layer combat target beside the Training Dummy so the gameplay slice can test more than a static replicated dummy state.

What was added:

- Added a native `AMOBAMMOTrainingMinionActor` that derives from the `MOBAMMOAI` module's `AMOBAMMOAICharacter`.
- Gave the Sparring Minion a replicated health component path through the AI character base, a simple visible body, target ring, overhead nameplate, and floating damage feedback.
- Added lightweight server-side patrol movement so the target is visibly alive in the arena instead of being another static object.
- Updated GameMode to spawn the Sparring Minion on the dedicated server and route debug damage ability `1` into the minion when selected.
- Updated PlayerController targeting so left-click can select the minion by trace, `E` can select it as a fallback target, and `6` cycles between Training Dummy and Sparring Minion when no player targets exist.
- Updated the in-game HUD target frame to read the Sparring Minion's replicated health directly from the world actor.
- Hardened the AI health component replication path so server-side damage/heal changes force a net update and clients receive death state transitions from replicated health.
- Promoted `MOBAMMOAI` to a public module dependency for the main game module because the new native target class derives from AI-layer code.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully.
- Staged dedicated server cook/stage completes successfully.
- Staged client cook/stage completes successfully.
- The only cook warning observed is the existing `Orion_Proto_Retarget` missing `Rig` class warning.

How to test in-game:

- Start the local flow with `TamBaslatma.bat`.
- Login, select/create a character, and enter the dedicated server session.
- Press `E` to select the Sparring Minion fallback target, or aim at the minion and left-click.
- Press `6` to cycle debug targets between Sparring Minion and Training Dummy.
- Press `1` to cast the debug damage ability against the selected target and watch the HUD/overhead health feedback.

Relevant files:

- `Source/MOBAMMO/MOBAMMOTrainingMinionActor.cpp`
- `Source/MOBAMMO/MOBAMMOTrainingMinionActor.h`
- `Source/MOBAMMO/MOBAMMOGameMode.cpp`
- `Source/MOBAMMO/MOBAMMOGameMode.h`
- `Source/MOBAMMO/MOBAMMOPlayerController.cpp`
- `Source/MOBAMMO/MOBAMMOPlayerController.h`
- `Source/MOBAMMO/MOBAMMOGameHUDWidget.cpp`
- `Source/MOBAMMO/MOBAMMO.Build.cs`
- `Source/MOBAMMOAI/Private/HealthComponent.cpp`

#### Sparring Minion Respawn Pass

This update made the first AI target reusable during a longer playtest instead of becoming a one-time kill target.

What was added:

- Added a server-authoritative respawn timer for the Sparring Minion after it is killed by the debug damage ability.
- The combat log now announces when reinforcements will arrive after the minion drops.
- Respawn reuses the same gameplay target identity (`training-minion`), so the HUD target frame and target-cycle flow can reconnect to the newly spawned minion.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully.
- Staged dedicated server cook/stage completes successfully.
- Staged client cook/stage completes successfully.
- The only cook warning observed is the existing `Orion_Proto_Retarget` missing `Rig` class warning.

How to test in-game:

- Enter the dedicated server session.
- Press `E` or `6` until Sparring Minion is selected.
- Press `1` until the minion is defeated.
- Wait roughly 5 seconds and verify the minion respawns as a selectable combat target again.

Relevant files:

- `Source/MOBAMMO/MOBAMMOGameMode.cpp`
- `Source/MOBAMMO/MOBAMMOGameMode.h`

#### Sparring Minion Counter-Attack Pass

This update made the AI target interact back with the player so the vertical slice now has two-way combat feedback.

What was added:

- Added a server-authoritative counter-attack after the player damages a living Sparring Minion.
- The counter-attack applies damage through replicated `AMOBAMMOPlayerState` health, so the player frame, roster, and incoming combat feedback can reflect the hit.
- If the counter-attack defeats the player, the existing death and Reforge cooldown path is reused instead of adding a separate debug-only death branch.
- Added range and damage tuning values on `AMOBAMMOGameMode` for the first minion strike pass.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully.
- Staged client cook/stage completes successfully.
- Staged dedicated server cook/stage completes successfully.
- The first parallel server stage attempt hit the expected UnrealBuildTool mutex because client staging was already running; rerunning server staging by itself succeeded.
- The only cook warning observed is the existing `Orion_Proto_Retarget` missing `Rig` class warning.

How to test in-game:

- Enter the dedicated server session.
- Select Sparring Minion with `E`, `6`, or left-click.
- Press `1` to damage the minion.
- Watch the player HP, incoming feedback, and combat log for `Minion Strike`.

Relevant files:

- `Source/MOBAMMO/MOBAMMOGameMode.cpp`
- `Source/MOBAMMO/MOBAMMOGameMode.h`

#### Sparring Minion Auto-Attack Pass

This update added the first autonomous combat pressure from the AI layer.

What was added:

- Sparring Minion now starts a server-side repeating auto-attack loop when it enters the arena.
- Every few seconds, the server finds the nearest alive player in range and applies a small replicated `Minion Pulse` hit.
- The auto-attack loop stops when the minion dies and starts again when the minion respawns.
- Minion strike damage uses the same replicated PlayerState health, incoming feedback, combat log, death, and Reforge cooldown path as the counter-attack.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully.
- Staged client cook/stage completes successfully.
- Staged dedicated server cook/stage completes successfully.
- The only cook warning observed is the existing `Orion_Proto_Retarget` missing `Rig` class warning.

How to test in-game:

- Enter the dedicated server session and move close to the Sparring Minion.
- Wait roughly 3 seconds.
- Watch player HP, incoming feedback, and combat log for `Minion Pulse`.
- Kill the minion, wait for respawn, and verify the pulse starts again after the new minion enters the arena.

Relevant files:

- `Source/MOBAMMO/MOBAMMOGameMode.cpp`
- `Source/MOBAMMO/MOBAMMOGameMode.h`

#### Replicated AI Threat HUD Pass

This update made the Sparring Minion's pressure easier to verify in one-client and two-client tests.

What was added:

- Added replicated GameState fields for the minion's latest threat target, strike name, and strike server time.
- Updated minion strike application so `Minion Strike` and `Minion Pulse` both publish AI threat state through GameState.
- Updated the HUD to show recent AI threat state in the selected minion target frame.
- When no target is selected, the action hint can temporarily show the latest replicated AI threat so testers can see minion pressure without opening logs.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully.
- Staged client cook/stage completes successfully.
- Staged dedicated server cook/stage completes successfully.
- The only cook warning observed is the existing `Orion_Proto_Retarget` missing `Rig` class warning.

How to test in-game:

- Enter the dedicated server session and move close to Sparring Minion.
- Wait for `Minion Pulse`, or attack the minion to trigger `Minion Strike`.
- Verify HUD text briefly shows `AI Threat: StrikeName -> PlayerName`.
- In a two-client test, verify both clients see the same recent AI threat state.

Relevant files:

- `Source/MOBAMMO/MOBAMMOGameState.cpp`
- `Source/MOBAMMO/MOBAMMOGameState.h`
- `Source/MOBAMMO/MOBAMMOGameMode.cpp`
- `Source/MOBAMMO/MOBAMMOGameHUDWidget.cpp`

#### Sparring Minion Aggro Tracking Pass

This update made the Sparring Minion react visibly in the world when it damages a player, closing the loop between AI pressure, replicated HUD state, and world-space feedback.

What was added:

- Added a replicated aggro window to `AMOBAMMOTrainingMinionActor` after `Minion Strike` or `Minion Pulse`.
- The minion now briefly chases the damaged player server-side before falling back to its idle patrol.
- The minion nameplate switches to `[AGGRO] Sparring Minion` and the target ring shifts to an orange pulse while aggro is active.
- The same GameMode strike path now publishes both replicated HUD threat state and world-space minion aggro state.
- Updated client/server staging scripts to call UnrealBuildTool through bundled DotNet with `-NoUBA`, reducing sandbox/UBA friction during local packaging.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully.
- Staged client cook/stage completes successfully.
- Staged dedicated server cook/stage completes successfully.
- A short staged dedicated server smoke test starts Iris, opens port `7777`, and reports no warning/error hits for `registered SubObjectsLists`, `Ensure condition failed`, or `Error:`.
- The only cook warning observed is the existing `Orion_Proto_Retarget` missing `Rig` class warning.

How to test in-game:

- Enter the dedicated server session and move close to Sparring Minion.
- Wait for `Minion Pulse`, or attack the minion with `1` to trigger `Minion Strike`.
- Watch the minion nameplate/ring switch into orange `[AGGRO]` state and briefly move toward the damaged player.
- Verify the HUD still shows the replicated AI threat text while the minion reacts in the world.

Relevant files:

- `Source/MOBAMMO/MOBAMMOTrainingMinionActor.cpp`
- `Source/MOBAMMO/MOBAMMOTrainingMinionActor.h`
- `Source/MOBAMMO/MOBAMMOGameMode.cpp`
- `Source/MOBAMMO/MOBAMMOGameMode.h`
- `scripts/build-cook-stage-client.ps1`
- `scripts/build-cook-stage-server.ps1`

#### Iris AI Aggro HUD Observability Pass

This update made the new minion aggro state easier to verify in the two-client Iris path.

What was added:

- Added replicated `TrainingMinionAggroEndServerTime` state to `AMOBAMMOGameState`.
- The same minion strike path now publishes the threat target, strike name, strike server time, and aggro end time together.
- HUD threat text now switches from passive `AI Threat` to active `AI AGGRO` while the replicated aggro window is still running.
- The target frame and no-target action hint can show the aggro countdown, making it easier to compare both clients without opening logs.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully.
- Staged client cook/stage completes successfully.
- Staged dedicated server cook/stage completes successfully.
- A short staged dedicated server smoke test starts Iris, opens port `7777`, and reports no warning/error hits for `registered SubObjectsLists`, `Ensure condition failed`, or `Error:`.
- The only cook warning observed is the existing `Orion_Proto_Retarget` missing `Rig` class warning.

How to test in-game:

- Run the normal flow or `scripts/run-iris-two-clients.ps1`.
- Join both clients into the dedicated server session.
- Move one client close enough for `Minion Pulse`, or hit the minion with `1` to trigger `Minion Strike`.
- Verify both clients can see `AI AGGRO: StrikeName -> PlayerName (Xs)` in the HUD while the minion is reacting.

Relevant files:

- `Source/MOBAMMO/MOBAMMOGameState.cpp`
- `Source/MOBAMMO/MOBAMMOGameState.h`
- `Source/MOBAMMO/MOBAMMOGameMode.cpp`
- `Source/MOBAMMO/MOBAMMOGameHUDWidget.cpp`

#### Iris Registered Subobject Replication Pass

This update tightened the native actor/component replication setup for Iris before expanding the two-client gameplay tests.

What was added:

- Enabled registered subobject list replication on the native playable `AMOBAMMOCharacter`.
- Enabled registered subobject list replication on the `MOBAMMOAI` character base used by the Sparring Minion.
- Kept Iris active in the staged launch path with `-UseIrisReplication=1` and `-net.SubObjects.DefaultUseSubObjectReplicationList=1`.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully.
- Staged dedicated server cook/stage completes successfully.
- Staged client cook/stage completes successfully.
- A short staged dedicated server smoke test starts Iris successfully.
- The server smoke log reports `BroadcastLoadedModulesUpdated()` while there are `0 active ReplicationSystems`, which is the safe startup path.
- No `registered SubObjectsLists` ensure/warning appeared in the staged server smoke log.
- The only cook warning observed is the existing `Orion_Proto_Retarget` missing `Rig` class warning.

Relevant files:

- `Source/MOBAMMO/MOBAMMOCharacter.cpp`
- `Source/MOBAMMOAI/Private/MOBAMMOAICharacter.cpp`
- `scripts/run-staged-client.ps1`
- `scripts/run-staged-server.ps1`

#### Two-Client Iris HUD Observability Pass

This update made the two-client Iris smoke test easier to read from inside the game.

What was added:

- Updated the top HUD roster to show each replicated player's current selected target beside health and mana.
- Updated the two-client Iris launcher so it can prepare the backend, confirm/start the staged dedicated server, and launch both clients directly.
- The launcher now positions both clients side-by-side and prints the exact gameplay smoke flow for roster, target, health/mana, and `AI AGGRO` countdown validation.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully.
- Staged client cook/stage completes successfully.
- The only cook warning observed is the existing `Orion_Proto_Retarget` missing `Rig` class warning.
- `scripts/run-iris-two-clients.ps1` parses successfully as a PowerShell script.

How to test in-game:

- Run `scripts/run-iris-two-clients.ps1`.
- Login both clients and enter the dedicated server session.
- On one client, press `E` or `6` to select Sparring Minion or Training Dummy.
- On the other client, watch the top roster for `PlayerName HP MP -> TargetName`.
- Press `1` on the selected target or stand close enough for `Minion Pulse`.
- Verify HP changes remain visible through the target frame/roster, and both clients can see the replicated `AI AGGRO` countdown when the minion reacts.

Relevant files:

- `Source/MOBAMMO/MOBAMMOGameHUDWidget.cpp`
- `scripts/run-iris-two-clients.ps1`

#### Training Target Visibility and Interaction Pass

This update focused on making the first in-game combat target easier to see, select, and test after entering the dedicated server session.

What was added:

- Moved the spawned Training Dummy to a clearer side-forward position so it is less likely to be hidden by the player character or the center wall marker.
- Added a world-space target ring and overhead beacon to the Training Dummy actor, making the target visually recognizable in the arena.
- Raised the Training Dummy nameplate and floating feedback text so health, mana, and damage/heal feedback are easier to read in third-person view.
- Split left-click and `E` target selection behavior: left-click still selects the viewed target, while `E` now falls back to selecting the Training Dummy if the center trace misses.
- Added combat log feedback when selecting another player, selecting the Training Dummy, or clearing the current target.
- Preloaded the late Development runtime modules (`AutomationWorker`, `AutomationController`, `PerfCounters`) from the project GameInstance so Iris no longer receives a module-list update after the replication system is active.
- Detached the old external `character-flow` web UI prototype launcher from the active runtime and redirected the legacy `baslat.bat` entry point to the canonical `TamBaslatma.bat` launcher.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully.
- Staged dedicated server cook/stage completes successfully.
- Staged client cook/stage completes successfully.
- Staged dedicated server runtime starts with Iris enabled and no longer emits the tick-1 `BroadcastLoadedModulesUpdated()` warning.
- The only cook warning observed is the existing `Orion_Proto_Retarget` missing `Rig` class warning.

Relevant files:

- `Source/MOBAMMO/MOBAMMOGameInstance.cpp`
- `Source/MOBAMMO/MOBAMMOGameMode.h`
- `Source/MOBAMMO/MOBAMMOPlayerController.cpp`
- `Source/MOBAMMO/MOBAMMOPlayerController.h`
- `Source/MOBAMMO/MOBAMMOTrainingDummyActor.cpp`
- `Source/MOBAMMO/MOBAMMOTrainingDummyActor.h`

### 2026-04-16

#### Playable Vertical Slice Stabilization

This update focused on turning the login and character selection flow into a playable gameplay slice after joining the dedicated server.

What was added:

- Switched the server GameMode default pawn to the native `AMOBAMMOCharacter` so movement, camera, ability component, and replication are no longer dependent on a Blueprint parent setup.
- Added a native spring-arm follow camera and Aurora mesh/animation bootstrap to `AMOBAMMOCharacter`.
- Added guaranteed legacy input mappings for WASD, mouse look, jump, and `Ability1`-`Ability6` in `DefaultInput.ini`.
- Added a replicated Training Dummy state to `AMOBAMMOGameState` for single-client target, damage, mana drain, and HUD testing.
- Added a replicated world-space Training Dummy actor so the target is visible in the map, with a nameplate mirroring health and mana.
- Added look-at interaction for the Training Dummy: aim at it and press left mouse or `E` to select it without using the debug target cycle.
- Updated target cycling so pressing `6` selects the Training Dummy when no other player target exists.
- Updated debug ability flow so `1` and `3` can mutate Training Dummy health/mana through server-owned GameMode logic.
- Updated the HUD target frame to show Training Dummy health and mana from replicated GameState data.
- Updated the Training Dummy nameplate to show `[TARGET]` with a selected color when the local player has it targeted.
- Added world-space floating feedback on the Training Dummy for replicated HP/MP changes, plus camera-facing nameplate text for easier in-game testing.
- Added an Iris roster readout to the HUD score bar so two-client tests can see replicated player HP/MP state at a glance.
- Added `scripts/run-iris-two-clients.ps1` for launching a two-client local Iris replication smoke test.
- Cleaned up the ability component lifecycle so Unreal initializes the replicated component once, avoiding manual `InitializeComponent` calls from the character.
- Hardened the staged client launcher so it validates the MOBAMMO backend on `127.0.0.1:3000` and detects the dedicated server by process instead of probing the Unreal UDP game port as TCP.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully after the playable pawn, input, HUD, Training Dummy, and world feedback changes.
- Staged dedicated server and staged client cook/stage both complete successfully with the playable vertical slice.

Relevant files:

- `Config/DefaultInput.ini`
- `Source/MOBAMMOAbilities/Private/AbilityComponent.cpp`
- `Source/MOBAMMO/MOBAMMOCharacter.cpp`
- `Source/MOBAMMO/MOBAMMOCharacter.h`
- `Source/MOBAMMO/MOBAMMOGameMode.cpp`
- `Source/MOBAMMO/MOBAMMOGameMode.h`
- `Source/MOBAMMO/MOBAMMOGameState.cpp`
- `Source/MOBAMMO/MOBAMMOGameState.h`
- `Source/MOBAMMO/MOBAMMOGameHUDWidget.cpp`
- `Source/MOBAMMO/MOBAMMOPlayerController.cpp`
- `Source/MOBAMMO/MOBAMMOPlayerController.h`
- `Source/MOBAMMO/MOBAMMOTrainingDummyActor.cpp`
- `Source/MOBAMMO/MOBAMMOTrainingDummyActor.h`
- `scripts/run-iris-two-clients.ps1`
- `scripts/run-staged-client.ps1`

### 2026-04-13

#### Player Controller ve Debug Yetenek Sistemi

WASD hareket sisteminden bağımsız olarak, oyuncu kontrolcüsü artık sunucu otoritesine sahip debug yetenek tetikleyicileri içeriyor.

Eklenenler:

- `AMOBAMMOPlayerController` eklendi — `1-5` tuşlarıyla debug yeteneklerini tetikleyen, `6` ile hedef döndüren, `7` ile hedef temizleyen özel bir player controller.
- Tüm yetenek tetikleyicileri `Server, Reliable` RPC'ler üzerinden çalışıyor; client tarafında authority kontrolü yapılıp uygun yoldan çağrılıyor.
- Hedef seçme sistemi eklendi — `CycleToNextDebugTarget` ile GameState'teki tüm oyuncular arasında döngüsel hedef seçimi yapılıyor.
- `GetSelectedTargetDisplayText` ile HUD'da seçilen hedefin ismini gösterme desteği eklendi.

İlgili dosyalar:

- `Source/MOBAMMO/MOBAMMOPlayerController.cpp`
- `Source/MOBAMMO/MOBAMMOPlayerController.h`

#### Yetenek Tanımlama Sistemi (Ability Set)

İleride tam yetenek sistemi için temel oluşturacak bir yetenek tanımlama altyapısı eklendi.

Eklenenler:

- `EMOBAMMOAbilityKind` enum'u eklendi — `Damage`, `Heal`, `Utility`, `Respawn` türleri.
- `FMOBAMMOAbilityDefinition` struct'ı eklendi — slot, tuş etiketi, isim, açıklama, mana maliyeti, bekleme süresi ve tür alanları.
- 5 adet öntanımlı yetenek tanımlandı:
  - **Arc Burst** (Slot 1) — Tek hedef hasar, 10 mana, 2s cooldown
  - **Renew** (Slot 2) — Kendine iyileştirme, 8 mana, 4s cooldown
  - **Drain Pulse** (Slot 3) — Hedeften mana çekme, 12 mana, 3s cooldown
  - **Mana Surge** (Slot 4) — Mana yenileme patlaması, 0 mana, 6s cooldown
  - **Reforge** (Slot 5) — Tam kaynaklarla yeniden doğma, 0 mana, 0s cooldown

İlgili dosyalar:

- `Source/MOBAMMO/MOBAMMOAbilitySet.h`

#### Oyun İçi HUD Büyük Güncelleme

Game HUD widget'ı basit bir durum panelinden tam teşekküllü bir MOBA HUD'una dönüştürüldü.

Eklenenler:

- Sağlık ve mana barları (`UProgressBar`) eklendi — hem oyuncu hem hedef için ayrı barlar.
- Yetenek tepsisi (ability tray) eklendi — 5 yetenek slotu ile tuş etiketleri, cooldown barları ve cooldown süreleri gösteriliyor.
- Hedef paneli eklendi — seçilen hedefin adı, sağlık/mana barları ve durumu gösteriliyor.
- Savaş logu ve savaş olayı vurgulama sistemi eklendi — `CombatLogText` ve `CombatEventText` ile son olaylar gösteriliyor.
- Roster (oyuncu listesi) paneli eklendi — sunucudaki tüm oyuncuların durumunu gösteriyor.
- Yeniden doğma ipucu metni eklendi — ölü oyuncular için `[5] Reforge - Yeniden Doğ` uyarısı.
- `bBoundToPlayerState`, `bBoundToGameState` takibi ile replicated state değişikliklerine dinamik bağlanma.
- `NativeTick` ile savaş olayı vurgulama zamanlayıcısı yönetimi.

İlgili dosyalar:

- `Source/MOBAMMO/MOBAMMOGameHUDWidget.cpp`
- `Source/MOBAMMO/MOBAMMOGameHUDWidget.h`

#### WebUI Karakter Akış Entegrasyonu

Karakter seçim ekranı tamamen yeniden yazıldı ve bir WebUI tabanlı yaklaşımla entegre edildi.

Eklenenler:

- Karakter akış widget'ı (`MOBAMMOCharacterFlowWidget`) WebUI entegrasyonu ile yeniden yapılandırıldı.
- Karakter giriş widget'ı (`MOBAMMOCharacterEntryWidget`) güncellendi.
- WebUI asset dosyaları eklendi — `Content/WebUI/` altında karakter önizleme görselleri, ikonlar ve 12 adet preset dosyası.
- `scripts/run-character-flow-webui.ps1` scripti eklendi.

İlgili dosyalar:

- `Source/MOBAMMO/MOBAMMOCharacterFlowWidget.cpp`
- `Source/MOBAMMO/MOBAMMOCharacterFlowWidget.h`
- `Source/MOBAMMO/MOBAMMOCharacterEntryWidget.cpp`
- `Content/WebUI/*`
- `scripts/run-character-flow-webui.ps1`

#### Paragon Aurora Karakter Asset'leri

Epic Games'in Paragon projesinden Aurora karakterinin tüm asset'leri projeye eklendi.

Eklenenler:

- Aurora karakter mesh'leri, animasyonları, ve blueprint'leri — `Content/ParagonAurora/Characters/Heroes/Aurora/`
- FX (efekt) sistemi — materyaller, parçacık sistemleri, niagara efektleri
- Texture'lar — karakter dokuları, çevresel dokular, duman, kıvılcım, iz efektleri
- Vector field'lar — `VF_RandomNoise_01_90`, `VF_Vortex_6`
- Toplam ~1600 yeni asset dosyası (2.9 GB LFS)

İlgili dosyalar:

- `Content/ParagonAurora/*` (tüm alt dizinler)

#### Input Action Yeniden Yapılandırma

Tek eksenden (`IA_Move`) çift eksen giriş yapısına (`IA_MoveForward` + `IA_MoveRight`) geçiş yapıldı.

Eklenenler:

- `IA_MoveForward` ve `IA_MoveRight` input action'ları eklendi.
- `IA_Move` input action'ı kaldırıldı.
- `IA_Look` ve `IA_MouseLook` asset'leri güncellendi.
- `IMC_Default` (Input Mapping Context) güncellendi.

İlgili dosyalar:

- `Content/Input/Actions/IA_MoveForward.uasset` [YENİ]
- `Content/Input/Actions/IA_MoveRight.uasset` [YENİ]
- `Content/Input/Actions/IA_Move.uasset` [SİLİNDİ]
- `Content/Input/Actions/IA_Look.uasset`
- `Content/Input/Actions/IA_MouseLook.uasset`
- `Content/Input/IMC_Default.uasset`

#### Login ve Loading Ekranı Güncellemeleri

Login ve loading ekranlarının UI mantığı ve görünümü güncellendi.

Eklenenler:

- Login ekranı (`MOBAMMOLoginScreenWidget`) — arayüz düzeni ve oturum yönetimi mantığı yeniden düzenlendi.
- Loading ekranı (`MOBAMMOLoadingScreenWidget`) — durum gösterge ve geçiş mantığı güncellendi.
- Debug overlay subsystem'ı güncellendi — yeni UI akışına uyum sağlandı.
- Game UI subsystem'ı güncellendi — ekran yönetimi ve geçiş mantığı iyileştirildi.

İlgili dosyalar:

- `Source/MOBAMMO/MOBAMMOLoginScreenWidget.cpp`
- `Source/MOBAMMO/MOBAMMOLoginScreenWidget.h`
- `Source/MOBAMMO/MOBAMMOLoadingScreenWidget.cpp`
- `Source/MOBAMMO/MOBAMMOLoadingScreenWidget.h`
- `Source/MOBAMMO/MOBAMMODebugOverlaySubsystem.cpp`
- `Source/MOBAMMO/MOBAMMODebugOverlaySubsystem.h`
- `Source/MOBAMMO/MOBAMMOGameUISubsystem.cpp`
- `Source/MOBAMMO/MOBAMMOGameUISubsystem.h`

#### GameMode, GameState ve PlayerState Oturum Yönetimi Genişletmeleri

Sunucu taraflı oyun yönetimi ve replicated state sınıfları genişletildi.

Eklenenler:

- `AMOBAMMOGameMode` — debug büyü sistemi (`CastDebugDamageSpell`, `CastDebugHealSpell`, `CastDebugDrainSpell`), mana yenileme (`TriggerDebugManaRestore`), ve oyuncu yeniden doğma (`RespawnPlayer`) eklendi.
- `AMOBAMMOPlayerState` — hedef seçimi (`SetSelectedTargetIdentity`, `ClearSelectedTarget`, `GetSelectedTargetName`, `GetSelectedTargetCharacterId`) eklendi.
- `AMOBAMMOGameState` — bağlı oyuncu sayısı replikasyonu güncellendi.
- Backend subsystem — oturum kimliği ve karakter bilgisi taşıma mantığı güncellendi.

İlgili dosyalar:

- `Source/MOBAMMO/MOBAMMOGameMode.cpp`
- `Source/MOBAMMO/MOBAMMOGameMode.h`
- `Source/MOBAMMO/MOBAMMOGameState.cpp`
- `Source/MOBAMMO/MOBAMMOGameState.h`
- `Source/MOBAMMO/MOBAMMOPlayerState.cpp`
- `Source/MOBAMMO/MOBAMMOPlayerState.h`
- `Source/MOBAMMO/MOBAMMOBackendSubsystem.cpp`
- `Source/MOBAMMO/MOBAMMOBackendSubsystem.h`

#### Başlatma Scripti ve Build/Run Güncellemeleri

Projeyi tek komutla başlatma ve paketleme/çalıştırma scriptleri güncellendi.

Eklenenler:

- `baslat.bat` eklendi — veritabanını başlatır, backend sunucuyu yeni pencerede açar ve Unreal Editor'ı açar.
- `scripts/build-cook-stage-client.ps1` ve `scripts/build-cook-stage-server.ps1` güncellendi.
- `scripts/run-staged-client.ps1` ve `scripts/run-staged-server.ps1` güncellendi.
- `MOBAMMO.uproject` güncellendi — yeni modül bağımlılıkları eklendi.
- `MOBAMMO.Build.cs` güncellendi — yeni modül referansı eklendi.

İlgili dosyalar:

- `baslat.bat` [YENİ]
- `scripts/build-cook-stage-client.ps1`
- `scripts/build-cook-stage-server.ps1`
- `scripts/run-staged-client.ps1`
- `scripts/run-staged-server.ps1`
- `MOBAMMO.uproject`
- `Source/MOBAMMO/MOBAMMO.Build.cs`

#### Sunucu Şema ve API Güncellemeleri

Backend Prisma şeması ve karakter route'ları genişletildi.

Eklenenler:

- `server/prisma/schema.prisma` — yeni alanlar ve ilişki güncellemeleri.
- `server/src/modules/characters/routes.ts` — karakter listeleme ve seçim için yeni endpoint'ler.

İlgili dosyalar:

- `server/prisma/schema.prisma`
- `server/src/modules/characters/routes.ts`

#### Blueprint Güncellemeleri

- `BP_ThirdPersonCharacter` — yeni player controller ve input action referansları güncellendi.
- `BP_ThirdPersonGameMode` — yeni C++ game mode sınıfına bağlantı güncellendi.

İlgili dosyalar:

- `Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.uasset`
- `Content/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.uasset`

---

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

#### Authoritative Stat Mutation Bootstrap

This update focused on taking the replicated player state one step further so the dedicated server owns basic gameplay mutation instead of leaving stats as passive replicated values only.

What was added:

- Added authority-only health helpers on `AMOBAMMOPlayerState` for damage and healing.
- Added authority-only mana helpers on `AMOBAMMOPlayerState` for spending and restoring mana.
- Added an `IsAlive` state query so UI and gameplay code can read a simple alive/dead view from replicated health.
- Added server-owned gameplay helpers on `AMOBAMMOGameMode` for applying damage, healing, mana consumption, mana restoration, and respawning a controller.
- Centralized default attribute initialization in the game mode so spawn and respawn use the same health and mana baseline.
- Updated the in-game HUD so it now reflects `Alive` or `Dead` alongside the replicated health, mana, and player count values.

Validated result:

- `MOBAMMOEditor Win64 Development` builds successfully with the new authoritative gameplay helpers in place.
- The gameplay mutation layer now has a clean server authority entry point that future ability, combat, and respawn systems can call instead of mutating replicated values ad hoc.

Relevant files:

- `Source/MOBAMMO/MOBAMMOGameMode.cpp`
- `Source/MOBAMMO/MOBAMMOGameMode.h`
- `Source/MOBAMMO/MOBAMMOPlayerState.cpp`
- `Source/MOBAMMO/MOBAMMOPlayerState.h`
- `Source/MOBAMMO/MOBAMMOGameHUDWidget.cpp`
