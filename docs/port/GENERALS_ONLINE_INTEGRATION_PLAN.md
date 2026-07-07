# GeneralsOnline integration plan (macOS / iOS client)

Goal: let this port's macOS and iOS builds play online multiplayer via
[GeneralsOnline](https://github.com/GeneralsOnlineDevelopmentTeam) — the
community replacement for GameSpy (matchmaking, lobbies, ladders, relays).

## What we learned scoping this (July 2026)

- **Their client is our cousin, not a stranger.** `GeneralsOnlineDevelopmentTeam/GameClient`
  forks the same `TheSuperHackers/GeneralsGameCode` upstream this port descends
  from. Their netcode lives in a mostly-contained subtree:
  `GeneralsMD/Code/GameEngine/Include|Source/GameNetwork/GeneralsOnline/**`
  (NGMP = "next-gen multiplayer") plus hooks in menus/GameSpy overlay/LANAPI.
- **Cross-platform-capable libraries, Windows-only build.** They vendor Valve
  GameNetworkingSockets, libcurl, nlohmann/json, stb_image, sentry. But their CI
  builds only `win32` MSVC presets — the NGMP code has never been compiled for
  macOS/ARM64, so expect Win32-isms throughout.
- **Divergence is large:** ~1,178 commits / ~300 files ahead of upstream. A
  whole-fork merge is not viable; import the NGMP subtree + minimal hooks.
- **The backend is a non-issue.** Their `Services` repo (.NET 10) already builds
  on macOS/Linux — we only need the *client* side.
- **Determinism is the real gate, not networking.** Their player pool is
  x86/Windows/MSVC; we're ARM64/Clang. Lockstep sync against Windows clients is
  unproven and likely broken (float divergence). Apple↔Apple determinism is
  proven (LAN iPad↔Mac works end-to-end, July 2026).

## Strategy decision (make first)

**Option A — Apple-only pool (recommended start):** use GeneralsOnline's
matchmaking/lobby/relay infrastructure but match Apple clients only with each
other. Sidesteps determinism entirely; everything else on this plan stays useful.

**Option B — full cross-play vs Windows:** additionally requires making sim
math deterministic across compilers/ISAs (fixed-point or soft-float for
sim-critical paths). Months of engine surgery. Do not attempt first.

## Phase 0 — Contact + recon (no code)

- [ ] Talk to the GeneralsOnline team (Discord) **before writing code**:
      do they welcome an unofficial-platform client? What is their anti-cheat
      posture (EasyAntiCheat plugin exists — an unapproved client may simply be
      rejected)? Is the protocol/API considered stable?
- [ ] Add `references/generalsonline-gameclient` submodule (their fork) for diffing.
- [ ] Produce the authoritative diff of their NGMP subtree + hook points:
      `git diff superhackers/main...go/main -- '**/GameNetwork/**' 'Generals*/…/Menus/**'`
- [ ] License audit: their client code must be GPLv3-compatible to import.

## Phase 1 — Dependencies build on macOS/iOS

- [ ] GameNetworkingSockets via vcpkg for `arm64-osx` and `arm64-ios`
      (watch the crypto backend on iOS — OpenSSL vs Apple crypto).
- [ ] libcurl: already in our vcpkg graph on both platforms — verify features
      (TLS, websockets if they use it).
- [ ] Drop: sentry (Windows crash reporting), EasyAntiCheat plugin (Windows-only;
      moot until Phase 0 conversation resolves).

## Phase 2 — Import NGMP subtree (compile-only milestone)

- [ ] Copy `GameNetwork/GeneralsOnline/**` into our tree behind a CMake option
      (`SAGE_GENERALS_ONLINE`, default OFF).
- [ ] Port Win32-isms (threads, sockets init, wide strings, registry, `GetUserName…`)
      to the CompatLib patterns this port already uses.
- [ ] Milestone: compiles + links on macOS with the option ON, game still boots
      with it OFF. No behavior yet.

## Phase 3 — Hook points + auth flow

- [ ] Wire their menu/overlay hooks (MainMenu, GameSpyOverlay, staging rooms) —
      smallest possible hook set, matching how their fork replaces the GameSpy path.
- [ ] Auth: understand `OnlineServices_Auth` (their launcher does part of this
      on Windows — account login, tokens, auto-update). Decide what replaces the
      launcher on Mac/iOS (in-game UI; no external browser on iOS).
- [ ] Milestone: log in, see lobby list, chat — on macOS first.

## Phase 4 — Match flow on macOS

- [ ] Join/host a custom match Apple↔Apple through their relay.
- [ ] Verify game-start handoff (their NextGenTransport replaces the UDP
      transport — our Darwin socket fixes may be partly superseded here since
      relay traffic is unicast; no broadcast needed for online play).
- [ ] Milestone: full game Mac↔Mac over the internet.

## Phase 5 — iOS bring-up

- [ ] Lifecycle: persistent connections vs iOS backgrounding (reuse the
      render/sim pause machinery; sockets need reconnect-on-foreground).
- [ ] Local Network permission not needed (all unicast to internet) — but
      cellular vs Wi-Fi interface selection matters (revisit `IP_BOUND_IF` usage).
- [ ] Milestone: iPad joins a Mac-hosted online match.

## Phase 6 — Hardening + upstream

- [ ] Determinism spot-checks: replay-compare harness (model on their
      `check-replays.yml` CI).
- [ ] Version-gate handling: their client enforces build compatibility — agree
      with their team how Apple builds identify themselves.
- [ ] Offer portability patches back to their GameClient repo.

## Risks (ranked)

1. **Anti-cheat / client policy** — could veto the whole project; resolve in Phase 0.
2. **Determinism** (only if Option B) — engine-wide, months.
3. **Upstream churn** — they move fast (~1,200 commits); pin a tag, rebase deliberately.
4. **Auth/launcher coupling** — login flow may assume their Windows launcher.
5. **iOS lifecycle vs persistent sockets** — known-hard, but we own prior art.

## Rough sizing

| Phase | Effort |
|---|---|
| 0 | days (mostly conversation latency) |
| 1 | 1–2 days |
| 2 | 1–2 weeks (Win32-ism porting is the bulk) |
| 3 | 1 week |
| 4 | days–1 week (protocol debugging) |
| 5 | 1 week |
| 6 | ongoing |
