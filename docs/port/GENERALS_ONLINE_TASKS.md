# GeneralsOnline integration — task breakdown

Companion to [`GENERALS_ONLINE_INTEGRATION_PLAN.md`](GENERALS_ONLINE_INTEGRATION_PLAN.md).
Each task is meant to be executable in isolation by an agent with no memory of
the others. Rules for the executing agent:

- Do tasks **in order**; each has a **Done when** check — verify it before moving on.
- If a **Done when** check fails twice, STOP and report; don't improvise around it.
- Never edit files under `references/` (read-only checkouts for diffing).
- All new engine code goes behind the CMake option `SAGE_GENERALS_ONLINE`
  (default `OFF`). The game must always still build and boot with it OFF.
- Commit after every task that changes files, one task per commit, message
  prefixed `go-online:`.

---

## Phase 0 — Recon

**T0.1 — Add the GameClient reference submodule**
```sh
git submodule add https://github.com/GeneralsOnlineDevelopmentTeam/GameClient references/generalsonline-gameclient
git -C references/generalsonline-gameclient submodule update --init --recursive
```
Done when: `references/generalsonline-gameclient/GeneralsMD/Code/GameEngine/Include/GameNetwork/GeneralsOnline/` exists and is non-empty.

**T0.2 — Inventory the NGMP subtree**
List every file under `Include/GameNetwork/GeneralsOnline/` and
`Source/GameNetwork/GeneralsOnline/` (if Source exists — find it) in their fork.
Write the list to `docs/port/go-online/NGMP_FILE_INVENTORY.md`, split into:
(a) NGMP code, (b) `Vendor/` third-party, (c) anything anti-cheat/sentry-related.
Done when: the doc exists and every file in the subtree appears in exactly one category.

**T0.3 — Map the hook points**
In their fork, grep for where NGMP is *called from* outside its own subtree
(search terms: `NGMP`, `GeneralsOnline`, `OnlineServices_`, `NextGenTransport`).
Write every call-site file + a one-line description of what the hook does to
`docs/port/go-online/HOOK_POINTS.md`.
Done when: the doc lists the call sites, grouped by file, and none of the grep
hits outside `GameNetwork/GeneralsOnline/` are missing from it.

**T0.4 — License audit**
Check the license headers / LICENSE files of: their NGMP code, GameNetworkingSockets
headers under `Vendor/ValveNetworkingSockets/`, `Vendor/libcurl/`, `json.hpp`,
`stb_image`. Record findings in `docs/port/go-online/LICENSES.md`. Flag anything
not compatible with GPLv3.
Done when: every vendored dependency has a recorded license and a yes/no
GPLv3-compatible verdict. If any verdict is "no", STOP and report.

## Phase 1 — Self-hosted backend

**T1.1 — Backend builds locally**
Clone `GeneralsOnlineDevelopmentTeam/Services` (outside this repo, e.g. `~/go-services`).
Install .NET 10 SDK if missing (`brew install dotnet-sdk` or their docs' method).
`dotnet build` the solution.
Done when: build succeeds with zero errors on this Mac.

**T1.2 — Database up**
Install MariaDB (`brew install mariadb && brew services start mariadb`), create a
database, import their `GenOnlineService/Database_Structure/structure.sql`.
Done when: `SHOW TABLES` lists the imported tables.

**T1.3 — Service runs**
Fill the TODO sections of `appsettings.json` (token settings, DB connection
string; skip Discord/S3/anti-cheat). Run the service.
Done when: the service starts without exceptions and answers on its configured
port (curl any documented health/root endpoint; a 401/404 is fine, connection
refused is not).

**T1.4 — Prove it with a stock Windows client** *(needs a human with a Windows PC)*
Point an official GeneralsOnline Windows client at the self-hosted instance
(their config/hosts mechanism — discover how the client selects its server and
document it in `docs/port/go-online/BACKEND_NOTES.md`).
Done when: a Windows client logs in and sees a lobby against our backend.
This task gates nothing else in Phases 2–3; run it in parallel.

## Phase 2 — Dependencies on macOS/iOS

**T2.1 — GameNetworkingSockets, macOS**
Add `gamenetworkingsockets` to `vcpkg.json`; install for `arm64-osx`.
Done when: `vcpkg_installed/arm64-osx/lib` contains the built library and a
trivial test program that calls `GameNetworkingSockets_Init` links and runs.

**T2.2 — GameNetworkingSockets, iOS**
Install for `arm64-ios`. Expect crypto-backend friction (OpenSSL on iOS);
if the default backend fails, try the port's alternative crypto features.
Done when: static lib exists for `arm64-ios` and links into a trivial iOS test
binary via the `ios-vulkan` toolchain. If blocked after two approaches, STOP
and write up what failed in `docs/port/go-online/BLOCKERS.md`.

**T2.3 — libcurl feature check**
Verify the existing vcpkg curl on both triplets has TLS. Grep their NGMP code
for `curl_ws_` / websocket usage; if present, ensure the `websockets` feature
is on.
Done when: a note in `docs/port/go-online/BACKEND_NOTES.md` records the
required curl features and that both triplets have them.

## Phase 3 — Import NGMP (compile-only)

**T3.1 — CMake option scaffold**
Add `option(SAGE_GENERALS_ONLINE "GeneralsOnline multiplayer client" OFF)` in the
top-level options file (follow the pattern of existing `SAGE_USE_*` options).
Done when: configuring with `-DSAGE_GENERALS_ONLINE=ON` and OFF both succeed
(no sources added yet).

**T3.2 — Copy the subtree**
Copy the NGMP code (per T0.2 inventory, category (a) + needed vendor headers (b),
excluding category (c)) from the reference submodule into our tree at the same
relative paths under `GeneralsMD/Code/GameEngine/`. Add the source files to the
GameEngine CMakeLists guarded by `SAGE_GENERALS_ONLINE`.
Done when: with the option OFF, the build is bit-for-bit unaffected; with ON,
the build *fails* (expected — Win32-isms) and the failure list is captured to
`docs/port/go-online/PORTING_LOG.md`.

**T3.3..T3.N — Burn down the compile errors, one commit per theme**
Iterate: build with ON, take the first error theme (e.g. winsock includes,
`CreateThread`, wide-string APIs, `GetUserNameA`, registry), fix it using the
existing CompatLib patterns (look at how `Core/GameEngine/Source/GameNetwork/`
POSIX-ports the same idioms — see `udp.cpp`, `IPEnumeration.cpp`). Append each
theme + fix to `PORTING_LOG.md`.
Rules: no `#ifdef _WIN32`-ing out functionality that online play needs — port it.
Stubbing is allowed only for anti-cheat/sentry/telemetry calls.
Done when: macOS build succeeds with ON, and the game boots to main menu with
ON and OFF.

## Phase 4 — Hooks + auth

**T4.1 — Import hooks incrementally**
Using T0.3's `HOOK_POINTS.md`, port the call sites one file at a time, each
guarded by `#if SAGE_GENERALS_ONLINE` (or runtime flag, matching their pattern).
Start with whatever makes the online menu entry appear; defer stats/social hooks.
Done when: with ON, the main menu shows the GeneralsOnline entry point and
clicking it reaches their login/lobby UI code (network calls may still fail).

**T4.2 — Point the client at our backend**
Find where the client resolves its services URL (from T1.4 notes / grep for the
hostname or a config key). Make it configurable (env var or `Options.ini` key)
defaulting to our self-hosted instance for these builds.
Done when: with the backend from Phase 1 running, the client's login attempt
reaches it (server logs show the request).

**T4.3 — Auth flow on macOS**
Get login working end-to-end against our backend. Their Windows launcher may
handle token acquisition — if so, replicate the minimum in-game (their
`OnlineServices_Auth` code shows the token contract; our backend's
`appsettings.json` token settings are the other half).
Done when: login succeeds and the lobby list renders in-game on macOS.

## Phase 5 — Apple↔Apple matches

**T5.1 — Mac↔Mac custom match** — two Mac builds, our backend: create room,
join, start, play 5 minutes.
Done when: no desync dialog, game completes/exits cleanly, both stderr logs free
of NGMP errors.

**T5.2 — iOS build with ON** — get the iOS target compiling/linking with the
option ON (expect a smaller round of Win32-ism/iOS issues; log to `PORTING_LOG.md`).
Done when: iPad boots the game with ON and reaches the online login.

**T5.3 — Lifecycle handling on iOS** — backgrounding mid-lobby and mid-match:
hook NGMP's connection teardown/reconnect into the existing iOS pause machinery
(see the app-lifecycle section of `PORTING_PLAYBOOK.md`).
Done when: app-switcher round trip in lobby reconnects; mid-match backgrounding
either reconnects or fails to a clean error, never a crash.

**T5.4 — iPad↔Mac internet match** — same as T5.1 with iPad + Mac on
different networks (e.g. iPad on cellular).
Done when: full match, no desync.

## Phase 6 — Windows friends

**T6.1 — Windows build of this fork** *(needs Windows machine/VM or CI)*
Build the `win32` preset of this fork with `SAGE_GENERALS_ONLINE=ON`.
Done when: Windows client logs into our backend and plays Windows↔Windows.

**T6.2 — Determinism harness**
Build a replay-compare script: run the same replay on macOS-ARM and Windows
builds, dump per-frame sim CRCs (the engine has CRC plumbing — see
`sawCRCMismatch` in `Core/GameEngine/Source/GameNetwork/Network.cpp`), diff them.
Done when: the harness reports first-divergence frame (or none) for a given replay.

**T6.3 — Converge float behavior**
If T6.2 diverges: first try building Windows with clang-cl and FP flags matched
to ours (`-ffp-contract=off`, no fast-math — see our flags in the CMake presets).
Re-run harness. Log each experiment in `docs/port/go-online/DETERMINISM_LOG.md`.
Done when: harness shows zero divergence on three different replays, or every
cheap option is exhausted and the log proposes the fixed-point plan. STOP for
human review either way.

**T6.4 — Cross-platform match** — Mac or iPad vs Windows through our backend.
Done when: full match, no desync, on two consecutive attempts.

## Phase 7 — Wrap up

**T7.1 — Docs** — README section: how a friend group self-hosts and connects
(server setup pointer, client env/ini key, platform notes).
**T7.2 — Upstream offer** — prepare a patch series of the NGMP-on-POSIX changes
against their GameClient repo; open an issue/PR offering it.
