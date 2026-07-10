# NGMP porting log (T3.2+)

## T3.3 milestone status (reached — macOS, arm64, Clang)

**`SAGE_GENERALS_ONLINE=ON` builds and links.** The full `z_generals`
executable (`build/macos-vulkan/GeneralsMD/GeneralsXZH`) links with only
pre-existing warnings (duplicate-lib, SDK-version-skew). The non-MD
`g_gameengine` still links clean, confirming the shared-Core changes
(Transport refactor + guarded engine additions) don't break the base game.

**Boot check:** launched the ON binary; it initializes SDL3/Vulkan, creates
its window, and runs engine subsystem init (LocalFileSystem, ArchiveFileSystem,
WritableGlobalData) before stopping at `No files read from directory
'Data\INI\Default\GameData'` — i.e. it gets past all NGMP static
initialization and only halts because this machine has no retail `.big`
assets. Booting fully to the main menu (ON and OFF) requires the user's
retail game data and is the one remaining "Done when" step to confirm on a
machine that has it.

Themes below were burned down one commit each; see the table for status.

Initial compile of the imported subtree on macOS (arm64, Clang, `SAGE_GENERALS_ONLINE=ON`)
failed with these themes:

1. **Relative include paths** — their build adds NGMP dirs to the include path
   so `"../NGMP_include.h"`, `"../json.hpp"`, `"libcurl/curl.h"` resolve.
   Fix: add `Include/GameNetwork/GeneralsOnline{,/HTTP,/Vendor}` include dirs.
2. **Transport base-class refactor** — their fork virtualizes `Transport`
   (legacy `UDPTransport` + `NextGenTransport` siblings). Our tree has the old
   concrete `Transport`, so `NextGenTransport.h` `override`s fail. Fix: import
   their Transport/UDPTransport refactor (hook files, from HOOK_POINTS.md).
3. **`std::chrono::utc_clock`** — not in Apple libc++. Replace with
   `system_clock` (wall-clock semantics equivalent for their usage).
4. **`winhttp.h`** — Win32 HTTP in `HTTP/HTTPManager.h`. Port to curl (the
   rest of their HTTP stack is already curl-based).

| Theme | Status |
|---|---|
| include paths | fixed |
| Transport refactor | fixed |
| utc_clock | fixed |
| winhttp | fixed — `_WIN32`-guarded, POSIX no-proxy fallback (curl honors `http_proxy`) |
| winsock headers (`ws2ipdef.h`/`ws2tcpip.h`) | fixed — `_WIN32`-guarded (no winsock symbols actually used) |
| `localtime_s` (NGMP_Helpers.cpp) | fixed — CompatLib shim in time_compat.h |
| lobby-camera-zoom defines undeclared | fixed — Settings.h now includes NextGenMP_defines.h |
| anti-cheat plugin interface | fixed — `GENERALS_ONLINE_USE_PLUGINS_INTERFACE` left undefined (cut from MVP), inert stub branch used everywhere |
| Win32 safe-string/mem funcs (`memcpy_s`,`sprintf_s`,`GetCurrentDirectoryA`,`SetEnvironmentVariableA`) | pending |
| `byte` type (NextGenTransport/NetworkMesh) | pending |
| `MAX_MESSAGE_LEN` constant | pending |
| Win32 safe-string/mem funcs (`memcpy_s`,`sprintf_s`,`GetCurrentDirectoryA`,`SetEnvironmentVariableA`) | fixed — CompatLib shims/aliases |
| `byte` type (NextGenTransport/NetworkMesh) | fixed — CompatLib typedef |
| `MAX_MESSAGE_LEN` constant | fixed — renamed to MAX_NETWORK_MESSAGE_LEN in NextGenTransport.cpp |
| launcher/updater Win32 (`shellapi.h`,`ShellExecuteA`,`__declspec`,`LoadLibraryA`) | fixed — shellapi_compat.h shim, `_WIN32`-guarded GPU hints, DownloadManager::SetFileName stub added; DPAPI credential encryption gated to `_WIN32` (plaintext fallback off-Windows) |
| GameSpyOverlay message-box variants (`GSMessageBoxNoButtons`,`GSMessageBoxCancel`) | fixed — ported their GS wrappers + `MessageBoxNoButtons`/`MSG_BOX_NONE` into both MD and non-MD engines |
| StatsInterface macro `stats.##name` paste + `PSPlayerStats` elo fields | fixed — `.##`→`.`, added `elo_rating`/`elo_num_matches` to PSPlayerStats |
| NGMPGame countdown member drift | fixed — NGMPGame.h now includes NextGenMP_defines.h (self-contained, like Settings.h) |
| NetworkInterface `GetConnectionManager`/`SeedLatencyData` + ConnectionManager/FrameMetrics `SeedLatencyData` | fixed — ported their engine additions, guarded `#if defined(SAGE_GENERALS_ONLINE)` (Core sources inherit that define via z_gameengine) |
| GameLogic `IsLoadScreenActive` | fixed — ported inline getter (MD only) |
| ISteamNetworkingSockets `GetConnectionType` | fixed — substituted GetDetailedConnectionStatus (their vendored GNS has both; vcpkg only the latter) |
| `View::setDefaultView` 4-arg (lobby camera) | fixed (compile-only) — added no-op 4-arg overload guarded by SAGE_GENERALS_ONLINE; full lobby-camera behavior deferred to Phase 5 (would couple W3DView to NGMP Settings) |
| `MapCache::getUserMapDir`/`getMapDir` bCustomMapDebug param | fixed — added defaulted bool, ignored (matches their impl) |
| non-POD varargs (std::string `mwid` to `%s`) | fixed — added .c_str() (real bug off-MSVC) |

**SAGE_GENERALS_ONLINE as a preprocessor macro:** the CMake option adds
`target_compile_definitions(z_gameengine PUBLIC SAGE_GENERALS_ONLINE=1)`.
Core sources build via the `corei_gameengine_private` INTERFACE library
straight into z_gameengine, so they see that define — which lets shared
engine additions (NetworkInterface/ConnectionManager/FrameMetrics/View) be
guarded so the non-MD `g_gameengine` build and the option-OFF build are
unaffected.
| NetworkInterface/ConnectionManager API drift (`GetConnectionManager`,`SeedLatencyData`) | pending |
| NGMPGame countdown member drift | pending |
| DownloadManager/GameLogic/PSPlayerStats API drift | pending |
| non-POD varargs (std::string to printf) | pending — real bug off-MSVC |

**Transport refactor details:** did the refactor mechanically in our tree instead
of copying their files (ours is already POSIX-ported and modernized). `Transport`
in Core is now an abstract base (metrics + `isGeneralsPacket` stay); the UDP
socket implementation moved verbatim to new `Core` files `UDPTransport.h/.cpp`.
Unlike their fork (UDPTransport under GeneralsMD only), ours lives in Core so the
non-MD Generals build keeps working — verified `g_gameengine` still links.
Call sites switched to `new UDPTransport`: `LANAPI.cpp`, `NAT.cpp`,
`ConnectionManager.cpp`. NextGenTransport selection in ConnectionManager is
deliberately NOT wired yet — that's a Phase 4 hook.

## Phase 4 — T4.1 hook points + backend connection prep (2026-07-09)

Ported the custom-match-path hooks from their fork using a 3-way
merge+guard tool (`scripts/go-online/merge_guard.py`): base = their pre-fork
commit `bf8d5be0`, ours = HEAD, theirs = `references/generalsonline-gameclient`.
The tool wraps each their-side hunk in `#if defined(SAGE_GENERALS_ONLINE)`
so the OFF build preprocesses to today's source verbatim (verified per file
with `unifdef -USAGE_GENERALS_ONLINE`). It resolves their `GENERALS_ONLINE`
macro (always defined when the option is ON) up front, and fuses hunks that
would otherwise split a `/* */` comment or an enclosing `#if` across a guard.

Files ported (all guarded, OFF path = HEAD):
- Engine pump: `Common/GameEngine.{h,cpp}` (TearDownGeneralsOnline, sentry
  init/shutdown, settings init, per-frame Tick + delayed teardown),
  `Win32Device/Win32GameEngine.cpp` (Tick + isInMultiplayerGame loop gate),
  `Main/WinMain.cpp` (version gate). SDL3 entry (`Main/SDL3Main.cpp`): version
  gate only — **AttemptLoadSteam intentionally NOT called** (Steam cut from
  friends-scale MVP, and calling it in `main()` hangs in NetworkLog before
  TheGlobalData exists).
- Menus: MainMenu (online button already routes via StartPatchCheck),
  WOLLoginMenu, WOLWelcomeMenu, WOLLobbyMenu, WOLGameSetupMenu, WOLMapSelectMenu,
  PopupHostGame, PopupJoinGame. Support: GUIUtil, GameWindowManagerScript,
  GameClient.{h,cpp} (render-rate 60Hz frame counter), GameLogic.{h,cpp},
  version.cpp.
- **MainMenuUtils.cpp** — found during the build burn-down (T0.3 grep missed
  it). This is the real Online-button→login path: `StartPatchCheck` now creates
  and inits `NGMP_OnlineServicesManager`, runs a version check, and pushes
  `Menus/GameSpyLoginProfile.wnd`. The upstream **ARM-processor rejection was
  removed** — this port targets Apple Silicon.

Compile/link burn-down themes (all guarded to ON):
| Theme | Fix |
|---|---|
| `std::chrono::utc_clock` (GameClient, WOLLobbyMenu, WOLGameSetupMenu) | → `system_clock` (Apple libc++ has no utc_clock; delta-only use) |
| pointer→`Int`/`UnsignedInt` narrowing casts on `GadgetX GetItemData` | → `static_cast<Int>(reinterpret_cast<intptr_t>(...))` (64-bit safe, matches our tree) |
| GameClient.h high-fps frame members not visible | added guarded `#include NextGenMP_defines.h` (self-contained, like Settings.h) |
| GameClient.h / GameLogic.h drift (m_frameLegacy, m_progressMade, setDefaults, progress-timeout enums) | guard-ported the header additions; dropped the duplicate `IsLoadScreenActive` Phase 3 already added |
| `winsock` (`ws2ipdef.h`) in WOLGameSetupMenu ON include | `#if defined(_WIN32)`-guarded (no winsock symbols used off-Windows) |
| sentry stub missing `sentry_value_new_int32` / `sentry_set_extra` | added no-op template stubs (sentry stays a no-op; USE_SENTRY left as-is) |
| `NetworkInterface::setSawCRCMismatch()` drift → takes `UnicodeString&` | guard-ported interface + `Network` override/impl |
| `CustomMatchPreferences::get/setLastLobbyName` | guard-ported header + impl in Core `UserPreferences.cpp` |
| `LobbyGameModeFilter` enum + `theLobbyFilter` global | guard-ported into Core `LobbyUtils.{h,cpp}` |
| `PlayerInfo::m_nameUni` | guard-ported into Core `PeerDefs.h` |
| `SetLookAtPlayer(int64_t, UnicodeString)` overload | guard-ported decl (`PersistentStorageDefs.h`) + adapter impl forwarding to the ASCII version (avoids pulling in non-MVP PopupPlayerInfo rewrite) |
| link: `updateBuddyInfo(bool,bool)` / `showNotificationBox(...,bool)` | guarded forwarding overloads in WOLBuddyOverlay.cpp (non-MVP social overlay left unported) |

Deferred (recorded in HOOK_POINTS.md T4.1f): all `[verify]`-tagged in-match /
60Hz-render / sim-math hooks (InGameUI, InGameChat, CommandXlat, SelectionXlat,
Weapon, EMPUpdate, registry, OptionsMenu, ww3d) — Phase 5/6.

**Milestone met:** macOS builds and links with `SAGE_GENERALS_ONLINE=ON`; the
game boots to the main menu with the option **ON and OFF**; the online entry
point is present and wired to the ported NGMP login/lobby UI (network calls not
yet exercised — that's T4.2/T4.3).

## Phase 4 — T4.2 backend URL + T4.3 auth (2026-07-09)

**T4.2 — configurable services URL (done).** `NGMP_OnlineServicesManager::GetAPIEndpoint`
resolved its host at compile time from `g_Environment` (DEV localhost / TEST /
PROD `api.playgenerals.online`); RelWithDebInfo defaulted to the official PROD
pool. Rewrote it to resolve the base URL at runtime: env var
`GENERALSX_ONLINE_URL` overrides, default `https://localhost:9000/env/prod/contract/1`
(the self-hosted instance). TLS needs nothing extra — HTTPRequest already
disables peer/host verification when no `cacert.pem` is present, so the
backend's ASP.NET dev cert is accepted. Verified: with `-onlineAutostart` the
client's `VersionCheck` hit the self-hosted backend and got **200**.

**T4.3 — friends-scale login without the launcher (done).** The stock flow polls
`CheckLogin` waiting for a launcher/web login (`pending_logins`). BeginLogin
already supports a token path (`GetCredentials` → POST `LoginWithToken`); it just
had no token to use. Added an env-var source to `GetCredentials`:
`GENERALSX_ONLINE_REFRESH_TOKEN` supplies a pre-minted refresh token (the minimum
replacement for the launcher). Recreated the HS256 mint script from the T1.4
contract at `scripts/go-online/mint_refresh_token.py` (Python stdlib; takes the
DB `user_id`, displayname, and `JwtSettings.Key`). Verified end-to-end against
the self-hosted backend with seeded user 34621 "nathan": `LoginWithToken` → 200,
`LOGIN: Logged in`, `[WebSocket] Connected` (wss://…/ws), `MOTD` 200, shell
advanced to `WOLWelcomeMenu` → `WOLCustomLobby.wnd`, and `Rooms` → 200 with the
live room list (all games / general / 1v1 / 2v2 / no rules / pro rules / RotR).

**Test hook — `-onlineAutostart`.** New command-line flag (guarded) that enters
the online flow at the main menu (stage 1) and advances the welcome screen to the
custom lobby (stage 2), so login/lobby can be exercised headlessly. Files:
`CommandLine.cpp`, `MainMenu.cpp` (MainMenuUpdate), `WOLWelcomeMenu.cpp`
(WOLWelcomeMenuUpdate).

**NGMP-flow null-deref fixes (GameSpy scaffolding reused without a GameSpy login).**
The online flow constructs GameSpy-era objects before `TheGameSpyInfo`/`TheGameSpyConfig`
exist (they're only created by `SetUpGameSpy`, which the NGMP path never calls —
matching their fork, which `#if !defined(GENERALS_ONLINE)`-guards that call).
Each was hanging on a virtual call through a null pointer during `Init()` /
welcome-menu init; guarded with the stock defaults / an early return:
- `RankPoints::RankPoints()` (PopupPlayerInfo.cpp) — hardcoded rank thresholds when `TheGameSpyConfig==nullptr`.
- `LadderList::LadderList()` (Core LadderDefs.cpp) — empty ladder list when `TheGameSpyConfig==nullptr` (ladders non-MVP).
- `PopulatePlayerInfoWindows()` (PopupPlayerInfo.cpp) — skip the legacy GameSpy stats panel when `TheGameSpyInfo==nullptr` (NGMP has its own stats; their fork ships an NGMP-aware version we didn't port).

**Boundary to Phase 5:** after the room list renders, the client auto-joins the
default room to populate the per-room staging/game list; sampling shows the
join-complete callback (`RoomsInterface::JoinRoom` → WOLLobbyMenuInit lambda)
pinned — that room-join/game-list population is where Phase 5 (join/host a match)
begins.

## Phase 5 — T5.0 auto-join-room hang (2026-07-10)

On entering the custom lobby the client auto-joins the default room, whose
completion callback calls `RefreshGameListBox` (Core LobbyUtils.cpp). That
function builds the game list from `TheGameSpyInfo->getStagingRoomList()` — null
in the NGMP flow — so it hung (virtual call through null). Sampling pinned it at
`RoomsInterface::JoinRoom` → WOLLobbyMenuInit lambda → `RefreshGameListBox`
(LobbyUtils.cpp:716).

Their fork replaces `RefreshGameListBox` wholesale with an NGMP
`SearchForLobbies`-driven version (operating on `LobbyEntry`, with
`detectGameMode` / `GameSortStruct<LobbyEntry>` / `populateBuddyGames(vector)` /
`insertGame(LobbyEntry)` helpers). Porting that whole subsystem is **T5.1** (it's
what actually renders joinable games). For T5.0, guarded `RefreshGameListBox` to
reset the listbox and return when `TheGameSpyInfo == nullptr`, so the lobby is
reachable and stable (empty game list) instead of hanging. Verified: client
reaches `WOLCustomLobby.wnd`, fetches `Rooms` (200), and sits in the normal
`GameEngine::update()` loop — no hang.

Note: re-running the whole-file merge_guard on LobbyUtils.cpp did not work
cleanly because HEAD already contains the partial T4 port of this file (committed
in `39ed98a5`), so the 3-way merge double-applied `theLobbyFilter`/helpers. The
T5.1 NGMP game-list port should be done as targeted per-function edits, not a
whole-file re-merge.

## Phase 5 — T5.1 (part 1): NGMP game-list rendering (2026-07-10)

Ported the NGMP-driven custom-lobby game list into Core `LobbyUtils.cpp`
(`RefreshGameListBox`). Their fork rewrites this together with
`insertGame`/`GameSortStruct`/`populateBuddyGames` (~700 lines) to render
backend lobbies; a whole-file merge is not viable here (HEAD already carries the
partial T4 port of this file, so a 3-way re-merge double-applies symbols — see
T5.0 note). Instead this is a **focused, self-contained port**:

- Guarded NGMP includes (`NGMP_interfaces.h`, `OnlineServices_LobbyInterface.h`)
  under `#if defined(SAGE_GENERALS_ONLINE)` — the upstream file includes these
  unguarded, which would break our OFF build.
- `detectGameMode()` ported verbatim (lobby-name → `LobbyGameModeFilter`, used
  by the room filter).
- `RefreshGameListBox` restructured as `#if SAGE (NGMP) #else (GameSpy) #endif`.
  The NGMP branch calls `pLobbyInterface->SearchForLobbies(startCb, resultCb)`
  and, in the result callback, applies the game-mode filter then populates the
  essential columns (name / map / players) with item-data column 0 = lobbyID
  (read by the join path via `GetLobbyFromID`). Full-fidelity columns (ladder,
  password/observer/stats icons, ping), sorting and buddy-highlighting are a
  later enhancement — the 358-line `insertGame` was intentionally not ported.
- OFF path is byte-identical to HEAD (verified with `unifdef -USAGE_GENERALS_ONLINE`).

Verified end-to-end: entering the custom lobby fires `SearchForLobbies` →
`GET /Lobbies` → 200 `{"lobbies":[]}`; the list renders "No lobbies were found"
and the client stays in the normal `GameEngine::update()` loop (no hang/crash).
The list is empty only because nothing is hosted yet — populated-row rendering
and host/join are exercised by the remaining T5.1 two-client match test.

## Phase 5 — T5.1 (part 2): two-client lobby discovery (2026-07-10)

Added a `-hostAutostart` test hook (mirrors `-onlineAutostart`; implies it):
after auto-login reaches the custom lobby, `NGMP_HostAutostartCreateLobby()`
(PopupHostGame.cpp) calls `LobbyInterface::CreateLobby` with default settings
(default map, name "GeneralsX Autohost") without driving the host-popup UI.
Triggered once from `WOLLobbyMenuUpdate` after a short settle delay. All guarded
`SAGE_GENERALS_ONLINE`; OFF byte-identical to HEAD.

Verified two-client discovery on one machine against the self-hosted backend
(three seeded accounts: nathan 34621, friend1 34622, viewer 34623):

1. Host (nathan) `-hostAutostart` → `POST /Lobbies` + `/Lobby/0` (200): backend
   created lobby id 0, "generalsx autohost", map alpine assault, 1/2 players,
   host in slot 0. Host stays in the staging room.
2. Viewer (friend1) `-onlineAutostart` → its `SearchForLobbies` → `/Lobbies`
   (200) returned the hosted lobby in the results; the ported NGMP
   `RefreshGameListBox` consumed the non-empty `LobbyEntry` list and rendered
   the row without crashing (viewer stayed in the normal update loop).

This closes the "see the lobby list" half of T5.1 with real data across two
independent logins. Remaining T5.1: viewer JOINs the lobby (JoinLobby), both
ready-up, match start + NextGenTransport handoff, play. (GUI screenshot not
capturable in this headless session; verification is via backend responses +
process stability.)

## Phase 5 — T5.1 (part 3): join + staging room + ready (2026-07-10)

Added a `-joinAutostart` test hook (implies `-onlineAutostart`): once the custom
lobby is up, `NGMP_JoinAutostartJoinFirstLobby()` (WOLLobbyMenu.cpp) searches
lobbies and joins the first one not owned by the local user (CRC-checked,
non-passworded), mirroring the Join-button handler.

Two-client match sequence verified against the self-hosted backend (nathan
34621 hosts, friend1 34622 joins):
- Host `-hostAutostart` → lobby created; **friend1 sees the row rendered in the
  game list** (confirmed visually — name / map "Alpine Assault (2)" / "1/2").
- friend1 `-joinAutostart` → `POST /Lobby/N` join → `[NGMP] Joined lobby`;
  backend `numcurrentplayers:2`, both members present (nathan slot 0, friend1
  slot 1). Both clients reach the staging room (WOLGameSetupMenu).
- **Both players ready up** — backend shows nathan + friend1 `isready:true`,
  start positions assigned (0 / 1). The client "Accept" is the Start button
  relabeled (host → StartPressed, client → `localSlot->setAccept()` +
  `ApplyLocalUserPropertiesToCurrentNetworkRoom` → `SendData_MarkReady`).

Hangs fixed along the way (all the null-`TheGameSpyInfo` pattern in the NGMP
flow, all guarded `SAGE_GENERALS_ONLINE`, OFF byte-identical to HEAD):
- `gameTooltip` (LobbyUtils.cpp) — hovering a game-list row dereferenced null
  `TheGameSpyInfo->findStagingRoomByID`; guarded to skip the tooltip. (This was
  the beachball that blocked clicking Accept.)
- `CustomMatchPreferences` / `GameSpyMiscPreferences` / `IgnorePreferences` /
  `QuickMatchPreferences` ctors (UserPreferences.cpp) — keyed the prefs filename
  off `TheGameSpyInfo->getLocalProfileID()`; now use the online user id from the
  auth interface (CustomMatch also creates its GeneralsOnlineData folder), which
  fixed the staging-room-entry hang.

Remaining T5.1: host presses Start → `SendData_StartGame` → NextGenTransport /
NetworkMesh handoff → actual in-match play (5 min) → clean exit. That transport
handoff is the next milestone (Phase 5 "game-start handoff" bullet) and is
untested so far.
