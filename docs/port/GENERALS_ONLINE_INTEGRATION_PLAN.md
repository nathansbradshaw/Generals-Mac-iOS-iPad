# GeneralsOnline integration plan (macOS / iOS, friends-scale)

Goal: online multiplayer for this port's macOS and iOS builds using
[GeneralsOnline](https://github.com/GeneralsOnlineDevelopmentTeam)'s open-source
stack — **for local play and a small community of friends**, not the public
GeneralsOnline pool. That scope changes everything below:

- **We self-host.** Their `Services` backend (.NET 10) is open source and
  already builds on macOS/Linux. A friends group runs its own instance; we
  don't need the official servers, their approval, or their anti-cheat.
- **Both ends of every match are ours to modify.** Windows friends run a
  Windows build *of our fork*, not retail or official GeneralsOnline. So if
  making ARM↔x86 play work requires changing sim math or the protocol on the
  Windows side too, that's allowed. Compatibility with the wider world is a
  non-goal.

## What we learned scoping this (July 2026)

- **Their client is our cousin, not a stranger.** `GeneralsOnlineDevelopmentTeam/GameClient`
  forks the same `TheSuperHackers/GeneralsGameCode` upstream this port descends
  from. Their netcode lives in a mostly-contained subtree:
  `GeneralsMD/Code/GameEngine/Include|Source/GameNetwork/GeneralsOnline/**`
  (NGMP = "next-gen multiplayer") plus hooks in menus/GameSpy overlay/LANAPI.
- **Cross-platform-capable libraries, Windows-only build.** They vendor Valve
  GameNetworkingSockets, libcurl, nlohmann/json, stb_image, sentry. Their CI
  builds only `win32` MSVC presets — the NGMP code has never been compiled for
  macOS/ARM64, so expect Win32-isms throughout.
- **Divergence from upstream is large** (~1,178 commits / ~300 files). A
  whole-fork merge is not viable; import the NGMP subtree + minimal hooks.
- **Apple↔Apple determinism is proven** (LAN iPad↔Mac full game, July 2026).
  ARM↔x86 determinism is the open question — but see scope note above: we can
  attack it from both sides.

## Current Android validation (2026-10-09)

The ARM64 online APK builds, logs into the self-hosted service, joins a Mac
lobby, becomes ready, connects through direct ICE, and loads Alpine Assault
on both clients. The initial frame-246 desync was traced to native float trig
rounding. After routing common transforms and locomotor angles through rounded
double trig, a hidden Mac↔Android emulator match produced/moved workers and
ended normally by surrender. All 22 state samples through frame 2100 matched;
both recordings report no desync/disconnect. A separate NGMP replay CRC queue
classification fix lets the new recording play completely on both platforms.
The same recording also completes on a physical Pixel 7 (Android 17), with
all 22 sampled state streams matching the live Mac match. Full retail data is
installed and friend1 provisioned. With a temporary USB API tunnel, the phone
logs in, joins, becomes ready, and connects through direct ICE. DXVK emitted
ClipDistance despite the Pixel reporting that feature unsupported, causing a
Mali compiler crash. Gating shader capabilities/outputs and D3D clip-plane caps
on hardware support resolves the tested crash. A rendered GLA ↔ Laser USA
match produces/moves units and ends normally, with all 22 sampled states
matching and clean replay headers. Untethered API reachability, longer combat,
touch input, lifecycle recovery, other maps/factions, and x86 cross-play remain
open. User clip planes are unavailable on hardware without ClipDistance. See the
[Android checklist](../WORKDIR/planning/ANDROID_PORT_CHECKLIST.md) for evidence
and remaining lifecycle, input, relay, and physical-device validation.

## MVP definition

**Windows + Linux + macOS + iOS cross-play on a LAN (or VPN), using
GeneralsOnline's new netcode, custom matches only.**

- All players run clients built from **this fork** (all four platforms, all
  Clang, matched FP flags — best-case starting position for determinism).
- One machine on the LAN self-hosts their `Services` backend (.NET + MariaDB;
  builds on macOS/Linux). No internet dependency at all.
- Why keep the backend even for LAN: their lobby/matchmaking flow is
  intertwined with the services API — running the small service unmodified is
  *less* work than rewiring NGMP to serverless LAN discovery. The part we're
  actually here for (NextGenTransport/NetworkMesh on GameNetworkingSockets,
  60Hz) comes along unmodified.
- Cut from MVP: NAT traversal/STUN/TURN/relays, quickmatch/ladders/stats,
  social/Discord, anti-cheat, auto-update, official-pool compatibility.
  Pre-created accounts; friends update builds together.

MVP critical path: NGMP compiles on POSIX (macOS → Linux → iOS) → custom-match
hooks + backend URL config → Windows/Linux clients from this fork → determinism
harness across ISAs. Linux is cheap once macOS works — the GeneralsX lineage
already builds the engine there.

## Order of battle

Apple↔Apple first (no determinism risk, exercises the whole online stack),
then Android ARM64 while physical Android hardware and the Apple clients are
available. Windows/Linux x86-64 validation follows when suitable hardware is
available again.

## Phase 0 — Recon

- [x] Add `references/generalsonline-gameclient` submodule (their fork) for diffing.
- [x] Produce the authoritative diff of their NGMP subtree + hook points vs
      their upstream base. (`NGMP_FILE_INVENTORY.md`, `HOOK_POINTS.md`)
- [x] License audit: confirm the client code we import is GPLv3-compatible. (`LICENSES.md`)
- [ ] (Courtesy, not a gate) say hi to the GeneralsOnline team — we're reusing
      their code at friends-scale and offering portability patches back.

## Phase 1 — Self-hosted backend

- [x] Stand up their `Services` backend locally (macOS or a cheap Linux box):
      MariaDB + .NET 10, import their SQL schema, fill `appsettings.json`.
      (friends-scale auth verified against the live backend — `BACKEND_NOTES.md`)
- [ ] Point a stock Windows GeneralsOnline client at it to prove the backend
      works before any of our code enters the picture. *(deferred — MVP clients
      are built from this fork, so this proves nothing on the critical path)*
- [x] Skip: Discord app ID, S3, EasyAntiCheat — not needed at friends-scale.
      STUN/TURN only if friends aren't on the same LAN/VPN.

## Phase 2 — Dependencies build on macOS/iOS

- [x] GameNetworkingSockets via vcpkg for `arm64-osx` and `arm64-ios`
      (watch the crypto backend on iOS — OpenSSL vs Apple crypto).
- [x] libcurl: already in our vcpkg graph — verify features (TLS; websockets
      if their client uses it). (`DEPS_NOTES.md`)
- [x] Drop: sentry, anti-cheat plugins. (sentry → no-op stub header; AC plugin
      interface left undefined)

## Phase 3 — Import NGMP subtree (compile-only milestone)

- [x] Copy `GameNetwork/GeneralsOnline/**` into our tree behind a CMake option
      (`SAGE_GENERALS_ONLINE`, default OFF).
- [x] Port Win32-isms (threads, sockets init, wide strings, registry,
      `GetUserName…`) to the CompatLib patterns this port already uses.
- [x] Milestone: compiles + links on macOS with the option ON; game still
      boots with it OFF. (`PORTING_LOG.md`)

## Phase 4 — Hook points + auth flow

- [x] Wire their menu/overlay hooks (MainMenu, GameSpyOverlay, staging rooms) —
      smallest possible hook set, matching how their fork replaces GameSpy.
      *(T4.1: custom-match-path hooks ported behind `SAGE_GENERALS_ONLINE`;
      builds + links ON, boots to main menu ON and OFF, Online entry point
      reaches the ported NGMP login/lobby UI. In-match/60Hz/sim-math `[verify]`
      hooks deferred to Phase 5/6 — see `HOOK_POINTS.md` + `PORTING_LOG.md`.)*
- [x] Auth against *our* backend: understand `OnlineServices_Auth`; their
      Windows launcher handles login/update — decide what replaces it here
      (in-game UI; no external browser on iOS).
      *(T4.2: services URL runtime-configurable via `GENERALSX_ONLINE_URL`,
      defaults to the self-hosted instance; VersionCheck verified 200. T4.3:
      launcher replaced by a pre-minted refresh token supplied via
      `GENERALSX_ONLINE_REFRESH_TOKEN` — `scripts/go-online/mint_refresh_token.py`;
      LoginWithToken verified 200, WebSocket connected. See PORTING_LOG.md.)*
- [~] Milestone: log in, see lobby list, chat — macOS first.
      *(Log in ✓, lobby room list renders ✓ (Rooms → 200, live room list). Chat
      and the per-room staging/game list run into the room-join flow that opens
      Phase 5.)*

## Phase 5 — Apple↔Apple match flow

- [x] Join/host a custom match Mac↔Mac through our backend/relay.
      *(Verified end-to-end 2026-07-11: two independent accounts host/join,
      ready, establish a direct ICE connection, start, play a complete match,
      show results, and return to the online lobby.)*
- [x] Verify game-start handoff (their NextGenTransport replaces the UDP
      transport; online traffic is unicast — no broadcast issues like LAN had).
      *(Verified 2026-07-11. Missing client→server WS dispatch, signalling
      queue locking, and the omitted `ConnectionManager` transport-selection
      hook were fixed; in-match traffic now uses `NextGenTransport` over the
      connected GameNetworkingSockets mesh. See PORTING_LOG.md.)*
- [~] iOS bring-up: lifecycle (reuse the render/sim pause machinery; sockets
      reconnect on foreground), interface selection (`IP_BOUND_IF` audited).
      *(Compile milestone complete 2026-07-11: the ARM64 iOS app builds with
      `SAGE_GENERALS_ONLINE=ON`, GameNetworkingSockets/ICE, WebSocket curl, and
      OpenSSL. Lifecycle integration added 2026-07-12: entering the background
      releases the live curl WebSocket before suspension, and foregrounding
      schedules NGMP's authenticated reconnect flow through a narrow platform
      bridge. macOS and iOS builds pass. Device login and app-switcher runtime
      validation remain. `IP_BOUND_IF` is confined to legacy iOS
      `UDPTransport` LAN broadcast sockets; NGMP uses `NextGenTransport` and
      GameNetworkingSockets, so it cannot constrain GeneralsOnline traffic.
      Physical-device credential provisioning is automated by
      `scripts/go-online/provision-ios-online-account.sh`: it mints from the
      gitignored backend key, copies `credentials.json` into the app's private
      data directory, and relaunches without exposing the token. Physical
      Mac↔iPad LAN validation completed 2026-07-13; app-switcher
      background/foreground reconnection validation remains.)*
- [~] Add a cross-platform, persistent in-game **GeneralsOnline server
      address** setting for macOS, iOS, Linux, and Windows. The shared backend
      setting is now implemented as `network.service_url` in
      `GeneralsOnlineData/settings.json`; all four platforms resolve it through
      the same code before auth and WebSocket initialization. The
      `GENERALSX_ONLINE_URL` environment override remains highest priority for
      development and automation. The Extra Options menu now exposes the value
      through the engine's native text-entry widget on every platform, and
      Apply rejects malformed URLs before saving. A Test Connection action now
      probes the entered server's `ServiceConfig` endpoint with a bounded
      timeout and reports success, HTTP errors, or network/TLS failures without
      saving the value. Remaining: exercise that feedback on physical iOS and
      desktop clients. Players can enter a LAN
      IP/hostname or public DNS name and port (for example,
      `https://192.168.1.217:9000/env/prod/contract/1`), validate/test the
      connection, and account for iOS Local Network permission, platform
      firewalls, and TLS certificates valid for the configured hostname/address.
- [x] Milestone: iPad joins and completes a Mac-hosted match over LAN.
      *(Verified end-to-end 2026-07-13: lobby join, ready/start, direct ICE
      connection, live gameplay, match end, and graceful peer disconnect. LAN
      validation found and fixed the first cross-platform gate: Apple
      builds hashed their platform-specific Mach-O executable bytes, making
      Mac and iOS lobby CRCs inherently different. GeneralsOnline builds now
      use a platform-neutral compatibility CRC over the game version and shared
      multiplayer scripts, while the existing INI CRC continues to protect
      gameplay data compatibility.)*
- [ ] Milestone: iPad joins a Mac-hosted match over the internet/WAN.
      *(LAN/VPN is sufficient for the friends-scale MVP; this validates NAT,
      firewall, certificate, and optional relay behavior when later needed.)*

## Phase 6 — Android ARM64 client

- [~] Add an Android NDK `arm64-v8a` build preset and SDL3 Gradle application
      shell, producing a debuggable APK from the shared Zero Hour codebase.
      *(Bootstrap milestone complete 2026-07-13: the repository-owned API 35
      Gradle project builds and signs an ARM64 APK with NDK r28c and pinned
      SDL 3.4.2; it installs on the Pixel 7 emulator, loads `libmain.so`, enters
      `SDL_main`, and renders successfully. The full Zero Hour target and
      offline emulator mission rendering have since been brought up; see the
      [Android checklist](../WORKDIR/planning/ANDROID_PORT_CHECKLIST.md).
      A standalone NDK CMake preset remains optional work.)*
- [x] Cross-compile DXVK native with the SDL3 WSI against Android Vulkan;
      isolate all Android/Bionic adaptations in the platform and build layers.
      *(Completed 2026-07-13: NDK r28c builds ARM64 D3D8/D3D9, the APK loads
      both libraries, SDL creates `VK_KHR_android_surface`, and
      `Direct3DCreate8` initializes DXVK's Android Vulkan adapter path on the
      Pixel 7 emulator. Full D3D8 device/swapchain validation remains part of
      the main-menu milestone.)*
- [x] Build the mobile dependency graph for Android: OpenAL Soft, FFmpeg,
      libcurl/OpenSSL/WebSockets, and GameNetworkingSockets 1.6/ICE.
      *(Android ARM64 dependency builds verified 2026-10-08; audio/movie
      runtime behavior and complete cross-play remain separate checks.)*
- [x] Add app-private retail-data provisioning. Do not make the engine depend
      on APK `assets://` paths because its legacy file APIs require ordinary
      filesystem paths.
- [ ] Generalize the proven iOS touch, text-entry, Escape gesture, safe-area,
      suspend/resume, and GeneralsOnline reconnect behavior to mobile SDL3
      without regressing iOS.
- [x] Milestone: boot to the Zero Hour main menu in an ARM64 Android emulator.
      *(Recorded in the Android checklist before this continuation; not
      re-tested after the 2026-10-08 request to avoid screen interaction.)*
- [ ] Milestone: complete an Android↔Mac or Android↔iPad custom online match
      with no crash or desync.

## Phase 7 — Cross-platform with Windows/Linux friends

Two-sided approach, since we control the Windows build too:

- [ ] Build a Linux x86-64 client from this fork carrying the same NGMP
      integration.
- [ ] Build a Windows client from this fork (upstream already has `win32`
      presets) carrying the same NGMP integration.
- [ ] Converge float behavior from both ends rather than only ours:
      same-compiler strategy first (Clang on Windows too, matched
      `-ffp-contract=off` / no fast-math / same FP model), before considering
      fixed-point rewrites of sim-critical math.
- [ ] Determinism harness: replay-compare runs of identical matches on ARM
      Mac vs Windows x86 (model on their `check-replays.yml` CI); iterate on
      divergences it finds.
- [ ] Milestone: Mac/iPad↔Windows full game with no desync.

## Phase 8 — Hardening + giving back

- [ ] Version-gating between our builds (all friends update together).
- [ ] Port GeneralsOnline Quick Match and its matchmaking preferences, then
      re-enable the Quick Match button after physical-device and cross-platform
      match-flow testing. Do not reuse the legacy `TheGameSpyInfo` path.
- [ ] Port the GeneralsOnline Communicator/social overlay, including friends,
      invitations, notifications, and safe iOS lifecycle behavior, then
      re-enable the Communicator button.
- [ ] Port the GeneralsOnline Personal Info/player-statistics overlay, including
      the current authenticated identity and backend stats, then re-enable the
      Personal Info button.
- [ ] Offer the portability patches (NGMP-on-POSIX) upstream to their
      GameClient repo.

## Risks (ranked)

1. **ARM↔x86 determinism** (Phase 7 only) — unbounded until the harness
   quantifies it; same-compiler builds may collapse it cheaply, or not.
2. **Upstream churn** — they move fast; pin a tag, rebase deliberately.
3. **Auth/launcher coupling** — login flow may assume their Windows launcher.
4. **iOS lifecycle vs persistent sockets** — known-hard, but we own prior art.
