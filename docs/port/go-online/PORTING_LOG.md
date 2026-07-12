# NGMP porting log (T3.2+)

## Phase 5 — persistent server connection test (2026-07-12)

The cross-platform server-address field now has a non-destructive Test
Connection action. It normalizes the typed URL, probes `<base>/ServiceConfig`
through the existing curl-backed `HTTPManager`, applies a five-second timeout,
and reports success, HTTP status failures, or connection/TLS failures. The
button is disabled while its request is pending and is hidden with the rest of
the GeneralsOnline controls in option-OFF builds. `z_generals` builds and links
under `macos-vulkan`; physical-device and in-game UI validation remains.

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

## Phase 5 — T5.1 (part 4): match-start driver + P2P transport blocker (2026-07-10)

Added a `-startAutostart` test hook (implies `-hostAutostart`): the host, in
`WOLGameSetupMenuUpdate`, auto-presses Start (`StartPressed`) once every human
member is ready (`m_bIsReady`) and the P2P mesh reports a connection to each
peer. Guarded `SAGE_GENERALS_ONLINE`, OFF byte-identical to HEAD.

Ran the full two-client sequence (host `-startAutostart`, joiner `-joinAutostart`)
and reached the **P2P transport handoff**, which is where it currently stops:

- Host + joiner reach the staging room; the host begins P2P signalling with the
  joiner: `[NETWORK_CONNECTION_START_SIGNALLING] Starting signalling with 34622`.
- GameNetworkingSockets then loops `Sending P2P ConnectRequest` + `[SIGNAL] SEND
  SIGNAL!` for ~3s and gives up:
  `problem detected locally (5003): Timed out attempting to connect`, then the
  disconnect handler retries (1/3) and fails. Joiner side logs repeated
  `[DISC] Recv Failed 1 from user 34621`.
- Because the mesh never connects, `StartPressed` keeps hitting its "players are
  still connecting" guard and the match never starts (lobby stays `state:0`).

**Root cause found (pending rerun): recursive WebSocket-lock attempt drops the
signalling payloads before they leave the client.** `CSignalingClient::Poll()`
held the WebSocket's non-recursive lock while calling `SendData_Signalling()`;
that calls `WebSocket::Send()`, which tries to acquire the same lock and returns
without sending. This precisely matches the evidence: both clients logged
`[SIGNAL] SEND SIGNAL!`, but neither logged `[SIGNAL] GOT SIGNAL!`. The backend
relay implementation was inspected and correctly forwards `NETWORK_SIGNAL` to
the other game-client session. The fix moves queued sends out of the locked
section; it still drains incoming signals atomically.

**Build validation:** `cmake -B build/macos-vulkan -DSAGE_GENERALS_ONLINE=ON`
followed by `cmake --build build/macos-vulkan --target GeneralsXZH -j4`
succeeds. The first post-fix runtime rerun could not be completed from this
headless agent environment: the detached macOS GUI clients exited immediately
without writing their redirected logs. Run the existing `TWO_CLIENT_TEST.md`
sequence from an interactive macOS session; the success evidence is a
`[SIGNAL] GOT SIGNAL!` / `[SIGNAL] PROCESS SIGNAL!` pair on both logs, followed
by GameNetworkingSockets' accepted and connected states.

The self-hosted transport configuration also needs hardening for a genuine LAN
test: NGMP hard-codes the public `turn.playgenerals.online` host, which fails
DNS lookup here, while the local debug backend issues deliberately fake TURN
credentials. That does not preclude direct RFC1918 LAN candidates (ICE is set
to `_All`), but it means relay fallback cannot work until self-hosted TURN
configuration is made explicit.

**Staging-room UI fix (pending rerun):** the joiner reaches `JoinLobby` (200)
and the backend shows both human members, but `WOLGameSetupMenuInit` disables
its Accept button. The legacy GameSpy peer-slot event that re-enables it is not
present on the NGMP services path. `WOLGameSetupMenuUpdate` now enables Accept
when the services cache provides a valid local human slot. The click continues
to use `ApplyLocalUserPropertiesToCurrentNetworkRoom`, which sends the existing
`NETWORK_ROOM_MARK_READY` request.

**macOS deployment fix:** a freshly copied ad-hoc signed executable was killed
by the macOS code-signing monitor before `main` (`SIGKILL: Code Signature
Invalid`, termination namespace `CODESIGNING`, indicator `Invalid Page`). The
macOS deploy script now re-signs and verifies `GeneralsXZH` after copying it to
the runtime directory.

If the post-fix rerun still cannot establish the NetworkMesh P2P connection,
likely remaining suspects are:
- **ICE transport config**: `NetworkMesh.cpp` sets a TURN server list from the
  service config (our self-hosted backend has none) and enables ICE
  `_All` (direct+STUN+relay) unless `relay_all_traffic`. With no relay and STUN
  pointing at external `stun.playgenerals.online`, two clients may fail to
  rendezvous — worth trying an explicit local/LAN path.
- **Single-machine caveat**: both clients share one host/public IP; GNS ICE
  loopback P2P may simply not work in this configuration. The real MVP target is
  two machines on a LAN, which this single-Mac harness can't fully represent —
  a genuine second device (or a LAN peer) may be required to validate the
  transport.

Everything up to the transport is working end-to-end across two independent
logins: login → lobby list (rows rendered) → host → join → staging room →
ready. The remaining T5.1 work is entirely in the P2P mesh / signalling layer.

## Phase 5 — T5.1 (part 5): backend dropped ALL inbound WS messages (2026-07-11)

Symptom (manual two-client run): both clients reach the staging room, but a
joiner's Accept click never appears on the other screen, and P2P signalling
never completes (`SEND SIGNAL!` x60 on both clients, zero `GOT SIGNAL!`).

**Real root cause — in the backend, not the client.** The previous entry's
claim that "the backend relay was inspected and correctly forwards
NETWORK_SIGNAL" was wrong. `WebSocketController` (the service's inbound WS
dispatcher) has a static field:

```csharp
private static readonly DatabaseReader GeoIpReader = new("data/GeoLite2-City.mmdb");
```

The GeoLite2 database is not shipped with the repo and doesn't exist in the
service's working directory, so the class's **type initializer throws**. .NET
caches that failure: every subsequent access to *any* static member of the
class (including `JsonOpts`, used to parse each incoming frame) rethrows
`TypeInitializationException`. Both catch sites swallowed it silently — the
WS connect path falls back to default geo coordinates (so connections looked
healthy), and the per-message dispatch treated every inbound frame as
"malformed" and dropped it. Net effect: **every client→server WS message —
MARK_READY (Accept), NETWORK_SIGNAL (ICE rendezvous), chat — was discarded**,
while server→client pushes (lobby updates, START_SIGNALLING on join, PONG
keepalives) kept working, which made the failure look client-side.

Proved with a raw-socket WS probe (`/tmp/ws_probe.py`): login → wss connect →
send NETWORK_SIGNAL/PING. Before the fix, instrumented catches logged
`The type initializer for 'WebSocketController' threw an exception` for every
frame; after the fix the server logs `[SIGNAL] received/forwarded` and answers
PING immediately.

**Fix** (applied to both `~/go-services` and the running copy in
`/private/tmp/go-services-debug`, rebuilt + restarted via
`launchctl kickstart -k gui/$UID/com.generalsx.service`):
- `GeoIpReader` initialization moved into a non-throwing `TryOpenGeoIpDatabase()`
  (missing DB → `[GeoIP] disabled` log + default location, dispatch unaffected).
- The two silent catches in the WS receive path now log
  (`[WS] malformed envelope …` / `[WS] dispatch exception for msg …`) so a
  future dispatch failure can never be invisible again.

Side effects this should also fix on rerun: Accept/ready propagation, P2P
signalling (GOT SIGNAL), lobby chat, and the client-side CURLWS_TEXT change
made earlier is orthogonal (server accepts both text and binary frames).

Ops notes discovered along the way:
- The backend now runs as launchd agent `com.generalsx.service` from
  `/private/tmp/go-services-debug/...` (stdout → `/tmp/go-service-debug.log`),
  with a TURN agent `com.generalsx.turn`. It respawns if killed — restart with
  `launchctl kickstart -k`, and rebuild with `dotnet build -p:Platform=ARM64`
  (a plain `dotnet build` writes to `bin/Debug`, NOT the `bin/ARM64/Debug` path
  launchd runs). **/tmp is wiped on reboot** — this deployment should move
  somewhere persistent.

## Phase 5 — T5.1 (part 6): lobby-roster stats crash unmasked by the WS fix (2026-07-11)

With inbound WS now working, `NETWORK_ROOM_CHANGE_ROOM` actually joins the
network room, so `PopulateLobbyPlayerListbox()` runs against a *real* roster
for the first time. Its stats callback calls
`GetAdditionalDisconnectsFromUserFile()` (PopupPlayerInfo.cpp), which
dereferenced `TheGameSpyInfo` unconditionally — null on the NGMP path →
SIGSEGV on the main thread right after the `PlayerStats/Batch` response
(crash report `GeneralsXZH-2026-07-11-100128.ips`). Fixed by null-guarding
`TheGameSpyInfo` in both overloads (also protects the LoadScreen and
player-info popup callers). Rebuilt + redeployed.

## Phase 5 — T5.1 (part 7): match-start replay-recorder crash (2026-07-11)

Second unmasked crash — this one is real progress: the match actually STARTS
now. `GameLogic::update` → `RecorderClass::updateRecord` → `startRecording`
SIGSEGV (crash report `GeneralsXZH-2026-07-11-100840.ips`). On the network
branch, `startRecording` (and the RTS_DEBUG `getLastReplayFileName`) used
`TheGameSpyGame`, which is null on the NGMP path — the current-match GameInfo
is `TheNGMPGame`. The upstream GeneralsOnline fork guards these with
`#if defined(GENERALS_ONLINE)` → `TheNGMPGame`; that hook was never ported into
our Recorder.cpp. Ported both sites under `SAGE_GENERALS_ONLINE` (matching the
reference), added the `NGMPGame.h`/`OnlineServices_Init.h` includes + extern.
Left the reference's third site (replay upload via `CommitReplay` on stop) —
non-MVP telemetry, absence just means no upload, no crash. Rebuilt + redeployed.

## Phase 5 — T5.1 (part 8): multiplayer load-screen crash (2026-07-11)

Third unmasked crash, further progress — both clients now start the game and
hit the shared multiplayer load screen. `GameLogic::tryStartNewGame` →
`GameSpyLoadScreen::init` SIGSEGV (crash `GeneralsXZH-2026-07-11-102533.ips`,
the joiner). The per-slot stats block dereferenced two GameSpy singletons that
are null on the NGMP path: `TheGameSpyPSMessageQueue->findPlayerStatsByID()`
and `TheGameSpyInfo->didPlayerPreorder()`. The upstream fork replaces these
with the NGMP stats cache under `#if defined(GENERALS_ONLINE)`, but
`LoadScreen.cpp` lives in **Core** (shared) — pulling NGMP types in would add a
Core→NGMP link dependency. Since the reference's NGMP branch resolves to empty
stats + no-preorder anyway (the load-screen W/L/rank display is cosmetic), the
fix here is a runtime null-guard on both globals (`... ? ... : PSPlayerStats()`
/ `: FALSE`) — strictly safer than the prior unconditional deref, no regression
for real GameSpy games, and no new dependency. `GetAdditionalDisconnectsFromUserFile`
in the same loop was already null-guarded in part 6. Rebuilt + redeployed.

Note: crashes 6/7/8 are the same bug class (GameSpy singletons null on the NGMP
path) surfacing one game-start stage at a time as each fix lets execution reach
the next. Expect possibly more in the in-match sim (CRC recording, end-of-game
stats upload).

## Phase 5 — T5.1 complete: full Mac↔Mac match (2026-07-11)

The two-client test now completes end-to-end: host/join, ready, direct ICE mesh,
game start, full gameplay, score screen, and return to the online lobby. The
final blockers uncovered by the complete run were:

- `ConnectionManager::initTransport()` still instantiated legacy
  `UDPTransport`, blocking the main thread in `recvfrom()` after loading. Ported
  the reference selection hook so GeneralsOnline uses `NextGenTransport` while
  LAN and `SAGE_GENERALS_ONLINE=OFF` retain UDP.
- The Internet score screen entered legacy GameSpy ladder/profile upload code
  and dereferenced null GameSpy singletons. NGMP now keeps the visible local
  results and skips that block; friends-scale stats/ladders are outside MVP.
- Returning to the lobby exposed a 64-bit bug in its batch-stats callback: it
  wrote selected indices into an uninitialized pointer variable. It now owns an
  initialized `std::vector<Int>` selection buffer.

Both clients returned cleanly after the final rerun. Phase 5 moves to iOS NGMP
bring-up (compile with the option ON, then socket foreground reconnection and
interface-selection validation).

## Phase 5 — iOS NGMP compile milestone (2026-07-11)

Configured `ios-vulkan` with `SAGE_GENERALS_ONLINE=ON` and built
`GeneralsXZH.app/GeneralsXZH` as an ARM64 iOS executable. The dependency graph
now includes iOS curl with WebSockets, GameNetworkingSockets with ICE, OpenSSL,
protobuf, and the existing FFmpeg/OpenAL/MoltenVK stack.

Two platform blockers were fixed:

- `ShellExecuteA` used desktop `system("open ...")`; iOS forbids process
  creation. The compatibility shim now returns failure on iOS, matching the
  friends-scale design that does not use browser login or an external updater.
- libc++ `std::format` requires floating-point `to_chars`, introduced in iOS
  16.3 even for the imported string-only format calls. Raised the device
  deployment floor from 16.0 to 16.3 instead of rewriting dozens of stable
  NGMP formatting sites.

Next: package/install on an iPad, verify token login and lobby entry, then test
background/foreground WebSocket + mesh reconnection and `IP_BOUND_IF` behavior.
Before device testing, add one cross-platform persistent in-game server-address
setting shared by macOS, iOS, Linux, and Windows. It must accept a self-hosted
LAN IP/hostname or public DNS name and port, apply common URL normalization and
connectivity validation, and load before auth/WebSocket initialization. The
`GENERALSX_ONLINE_URL` environment variable remains the highest-priority
automation/deployment override.

## Phase 5 — shared self-hosted server address core (2026-07-11)

The services base URL is no longer a hard-coded/cached value in
`OnlineServices_Init.cpp`. `GenOnlineSettings` now owns one persisted
`network.service_url` value in `GeneralsOnlineData/settings.json`, used by the
same NGMP code on macOS, iOS, Linux, and Windows.

Resolution order is:

1. Valid `GENERALSX_ONLINE_URL` environment override.
2. Valid persisted `network.service_url`.
3. `https://localhost:9000/env/prod/contract/1` development default.

Normalization trims surrounding whitespace and trailing slashes, requires an
`http://` or `https://` scheme, and rejects an empty or whitespace-containing
authority. The endpoint is resolved for each request rather than cached, so a
saved change made before login is used by both HTTP auth and the WebSocket URL
returned by the backend.

For an iPad, `localhost` means the iPad itself. When the backend runs on a Mac,
the saved value must therefore name an address the iPad can reach, such as the
Mac's LAN address or VPN/DNS hostname. The host firewall must allow the service
port and HTTPS must present a certificate valid and trusted for that exact
name/address. An in-game text editor plus connection-test feedback remains; the
storage, validation, precedence, and runtime consumption are now implemented.

The repo-owned Extra Options menu now includes an `Online Server Address`
text-entry field. It is populated from the persisted value, saves through the
shared validator when Apply is pressed, shows an error without closing the menu
for malformed URLs, and resets to localhost with Defaults. The same `.wnd` and
C++ callback are used on all targets; builds without `SAGE_GENERALS_ONLINE`
hide the field. A live connectivity-test action remains separate from syntax
validation.

**Build validation:** both `macos-vulkan` and `ios-vulkan` build and link
`GeneralsXZH` with `SAGE_GENERALS_ONLINE=ON`; the macOS binary and updated
`ExtrasMenu.wnd` were also deployed.

## Phase 5 — T5.1 ACHIEVED: Mac↔Mac custom match, full end-to-end (2026-07-12)

First complete two-client match on the self-hosted backend. Sequence, verified
from logs:
login (both accounts) → lobby list → host creates lobby 0 → joiner joins
(`numcurrentplayers:2`) → both `isready:true` → **P2P mesh connects via direct
ICE** (`Peer appears to be using 'ICE' transport as primary` → `connected`;
LAN host candidate `192.168.1.193` + srflx `38.62.161.115`, no relay needed) →
host auto-presses Start → lobby `state:0 → state:1` (`matchid:123456`) → both
clients enter the sim. Confirmed live: host main-thread sampled in
`GameEngine::execute → FramePacer::update` (game loop, not a menu); both clients
periodically upload gameplay screenshots for the same `match_123456`; no
desync / CRC-mismatch / disconnect; both processes stable for minutes.

**What actually unblocked this** (this session): the "host connected but not
player1" symptom was the self-hosted backend being DOWN — the machine had
rebooted, which wiped `/tmp` (the service had been deployed there +
`com.generalsx.service`/`.turn` were ephemeral launchd agents) and stopped
Docker/MariaDB. Recovery: `open -a Docker` → `docker start go-mariadb` →
rebuilt `~/go-services` (`dotnet build -p:Platform=ARM64`) → ran the service
from the **persistent** `~/go-services` tree (`nohup dotnet
bin/ARM64/Debug/net10.0/GenOnlineService.dll > ~/go-services/service.log`),
CWD = project dir so it finds `appsettings.json`. Once up, the part-5 GeoIP/WS
fix let inbound signalling flow (`GOT SIGNAL`+`PROCESS SIGNAL` bidirectional),
and the part-6/7/8 GameSpy-null crash fixes carried the clients through
match start into live gameplay with no new crashes.

**Deployment durability TODO**: the backend must live somewhere that survives
reboot (not `/tmp`). Re-establish a launchd agent (or equivalent) pointing at
`~/go-services/GenOnlineService/bin/ARM64/Debug/net10.0/GenOnlineService.dll`
with CWD `~/go-services/GenOnlineService`, plus the TURN agent, so a reboot
doesn't silently drop the backend mid-session again.

Remaining T5.1 polish: let a match run the full 5 minutes and exit cleanly
(score screen → back to menu). Note the score-screen path still has a
GameSpy-null crash (`populatePlayerInfo`←`grabMultiPlayerInfo`←
`ScoreScreenInit`, crash `...105031.ips`) — next fix, same bug class.

## Phase 5 — T5.1 follow-up: score-screen path already NGMP-safe (2026-07-12)

Correction to the previous entry's "remaining item". The game-end score-screen
crash (`...105031.ips`, yesterday) is ALREADY fixed in the current tree — it was
an intermediate build from yesterday's session. Traced the full stack and
verified every frame is null-safe on the NGMP path:
- `ScoreScreenInit` — line ~261 guards `TheGameSpyInfo`; all else is
  `parent`/`TheWindowManager` lookups. Calls `initInternetMultiPlayer` for
  internet games.
- `initInternetMultiPlayer` — `TheGameSpyInfo && ...` guarded (line ~1083);
  early-returns on null `TheGameSpyBuddyMessageQueue` (~1088).
- `grabMultiPlayerInfo` — only `ThePlayerList`/`parent`/`ScoreKeeper`, no
  GameSpy singletons.
- `populatePlayerInfo` — the visible score fields (units/buildings/resources)
  come from `ScoreKeeper`; the `SCORESCREEN_INTERNET` block (which derefs
  `TheGameSpyGame`/`TheGameSpyPSMessageQueue`/`TheGameSpyInfo`, all null on NGMP)
  is short-circuited by an early `return` under `SAGE_GENERALS_ONLINE`
  (`BenderAI 11/07/2026`, line ~1657). `+4644` landed in that skipped block.

This is the correct MVP choice: GameSpy stats/ladder recording is non-MVP, so
skipping it (rather than porting ~450 lines onto the NGMP stats interface) is
right. The real-time outcome upload already happens elsewhere
(`OnlineServices_LobbyInterface` CommitMyOutcome on victory/defeat). No code
change needed. NOTE: not yet exercised at runtime — the idle `-*Autostart`
match never ends, and there's no headless "play to a loss" hook, so the
score screen must be validated in a real played-out game.

## Phase 5 — T5.1 COMPLETE (validated at runtime, 2026-07-12)

User played a match to its end: score screen renders and returns to menu with no
crash, confirming the score-screen path (verified null-safe by inspection above)
is correct in practice. Full T5.1 "Done when" met — Mac↔Mac custom match on the
self-hosted backend: create → join → ready → P2P ICE connect → match start →
5+ min live gameplay in lockstep (no desync) → clean game end → score screen →
menu. No NGMP errors, both clients stable throughout.

Crash chain resolved this milestone (all "GameSpy singleton null on NGMP path"):
PopupPlayerInfo (`GetAdditionalDisconnectsFromUserFile`), Recorder
(`startRecording`/`getLastReplayFileName` → `TheNGMPGame`), LoadScreen
(`GameSpyLoadScreen::init` stats/preorder), ScoreScreen (already had the NGMP
early-return). Backend enabler: GeoIP type-initializer fix that had been
silently dropping all inbound WS messages.

Still open (not blockers): (1) backend durability — running as a plain
background process from `~/go-services`; needs a launchd agent so a reboot
doesn't drop it. (2) Next MVP tasks per GENERALS_ONLINE_TASKS.md: T5.2 (iOS
build with ON), T5.3 (iOS lifecycle), T5.4 (iPad↔Mac LAN).

## Backend durability: launchd agent (2026-07-12)

Replaced the ephemeral /tmp deployment (lost on the reboot that broke T5.1
mid-session) with a persistent, auto-respawning setup:

- **launchd agent** `~/Library/LaunchAgents/com.generalsx.service.plist`
  (Label `com.generalsx.service`): runs `~/.dotnet/dotnet` on the ARM64 DLL
  under `~/go-services/GenOnlineService/bin/ARM64/Debug/net10.0/`, with
  `WorkingDirectory` = the project dir (so `appsettings.json` + `data/` resolve),
  `DOTNET_ROOT`/`PATH` set, `RunAtLoad` + `KeepAlive` (respawns; `ThrottleInterval`
  15s so it waits out MariaDB coming up after a reboot rather than tight-looping).
  stdout/stderr → `~/go-services/service.log`.
- **MariaDB container** `go-mariadb`: `docker update --restart unless-stopped`
  so it returns automatically once the Docker daemon is up.

Verified: agent serves (MOTD 401), `kill -9` → launchd respawned with a new pid
and resumed serving, and a real `LoginWithToken` returns `result:1` + session
token (DB reachable through the agent).

Ops:
- (re)load:  `launchctl bootstrap gui/$UID ~/Library/LaunchAgents/com.generalsx.service.plist`
- restart:   `launchctl kickstart -k gui/$UID/com.generalsx.service`
- stop:      `launchctl bootout gui/$UID/com.generalsx.service`
- status:    `launchctl print gui/$UID/com.generalsx.service | grep -E 'state|pid'`
- After rebuilding the service, just kickstart (the DLL path is stable).

Still manual after a reboot: **Docker Desktop must be running** for the
`go-mariadb` container (and thus the service's DB) to come up — enable Docker
Desktop's "start at login" if you want the whole stack hands-off. The old
`com.generalsx.turn` TURN agent was NOT recreated: T5.1 connected via direct
LAN ICE with no relay, so TURN is only needed for restrictive-NAT peers
(deferred until a real cross-NAT test needs it).

## T4.2 in-app backend URL config — complete & verified (2026-07-12)

The "change the backend server from in the app, not hardcoded" capability
already exists (BenderAI groundwork) and is now runtime-verified end-to-end:

- **UI**: Extras menu has an "Online Server" text entry
  (`ExtrasMenu.wnd:TextEntryOnlineServer`, gadget present in the deployed .wnd),
  pre-filled with the current URL. Editing it + Accept validates
  (`Network_NormalizeServiceURL`: must be http(s):// with a host, trailing
  slashes trimmed; bad input → "Invalid Online Server Address" box, save
  blocked) and persists.
- **Persistence**: written to `network.service_url` in
  `~/Library/Application Support/GeneralsX/GeneralsZH/GeneralsOnlineData/settings.json`;
  Load reads it back on next launch.
- **Resolution precedence** (`Network_GetResolvedServiceURL`, read live per API
  call — not cached): `GENERALSX_ONLINE_URL` env var → persisted
  `network.service_url` → default `https://localhost:9000/env/prod/contract/1`.
- **Verified**: set `service_url` to a distinctive `https://192.168.1.193:9000/...`
  and launched a client (no env override) — its VersionCheck request went to
  `192.168.1.193`, proving the persisted in-app value drives the connection.

Usage notes: takes effect at next login (change it before going online, or log
out/in); no recompile. CAVEAT: if `GENERALSX_ONLINE_URL` is exported it silently
wins over the in-app field — the two-client test harness sets only
`GENERALSX_ONLINE_REFRESH_TOKEN`, not `_URL`, so the field controls things
there. Possible future polish: surface the field on the online login/connect
screen for discoverability (currently only in Extras).

## Phase 5 — T5.2: iOS build with SAGE_GENERALS_ONLINE=ON (2026-07-12)

Assessed and advanced T5.2. The hard parts were already done in the tree:

- **T2.2 (GNS for iOS) DONE**: `build/ios-vulkan/vcpkg_installed/arm64-ios/lib`
  has `libGameNetworkingSockets_s.a` + `libcrypto.a`/`libssl.a`/`libprotobuf.a`.
  The overlay port (`cmake/vcpkg-overlay-ports/gamenetworkingsockets`) carries an
  `ios-system-name.patch` ("treat iOS like Darwin") and uses `USE_CRYPTO=OpenSSL`
  with OpenSSL built for arm64-ios — the T2.2 crypto friction was resolved.
- **iOS compiles + links with ON**: `build/ios-vulkan/GeneralsMD/GeneralsXZH.app/
  GeneralsXZH` (arm64, ~47 MB, 173 NGMP symbols). The `generals-online` vcpkg
  feature auto-activates from `SAGE_GENERALS_ONLINE=ON` via CMakeLists.txt:28.
- **Current with T5.1 fixes**: the iOS objects (07-11 11:18–11:35) are newer than
  the Recorder/PopupPlayerInfo/LoadScreen source edits (07-11 10:03–10:28), so the
  iOS binary already contains all the GameSpy-null crash fixes.

Packaged + installed this session:
- `package-ios-zh.sh` (data src `~/GeneralsX/GeneralsZH`, which includes the
  server-config `ExtrasMenu.wnd`): signed .app, 2.7G assets, MoltenVK + dylibs
  embedded, signature OK. Prereqs all present (Xcode shell app, fonts, MoltenVK,
  `Apple Development` identity).
- Installed to paired **iPad mini 6** via `devicectl` (bundle
  `com.nathanbradshaw.generalszh`) and launched (no untrusted-developer prompt —
  profile already trusted). Packaged binary confirmed ON (NGMP symbols) with the
  in-app server-config field bundled.

Env for iOS builds (both were unset this session — the existing build dir has
them cached): `VCPKG_ROOT=~/vcpkg`, `VULKAN_SDK=~/VulkanSDK/1.4.350.1/macOS`.
iOS signing: `GX_TEAM_ID=URX49WBQ9N GX_BUNDLE_ID=com.nathanbradshaw.generalszh`.

Remaining for T5.2 "Done when" (iPad boots with ON → reaches online login):
on-device runtime verification (in progress via device-container log pull). Note
for actual iOS login later: no shell env on iOS, so the macOS
`GENERALSX_ONLINE_REFRESH_TOKEN`/`GENERALSX_ONLINE_URL` env approach doesn't
apply — the in-app Extras server field covers the URL; the token/auth path on
iOS needs its own handling (and for iPad↔Mac T5.4 the backend must bind the LAN
IP, not just 127.0.0.1).

### T5.2 runtime verification (2026-07-12)

Launched the ON build on the iPad mini 6 and pulled its log via
`devicectl device copy from --domain-type appDataContainer` (Documents container
→ `generals-stderr.log`). Result:
- Boots to main menu (SDL3GameEngine/WW3D/DXVK-on-MoltenVK init, `MainMenu`,
  `Shell::` all present), **no crash / no signal / no assert**.
- **Online subsystem initializes**: `[NGMP] Init`, then a `VersionCheck` HTTP
  request to the services URL — i.e. it reaches the online path and contacts the
  backend.
- The VersionCheck fails `curl result 7` (connection refused) because the URL is
  the default `localhost:9000` and on the iPad localhost is the device, not the
  Mac. Expected — this is the T5.4 LAN-config gap, not a build defect.

**T5.2 "Done when" met**: iPad boots with ON and reaches the online login flow.
To actually complete a login on-device (toward T5.4): set the in-app Extras
server field to the Mac's LAN IP (e.g. `https://192.168.1.193:9000/env/prod/
contract/1`), and make the backend bind the LAN interface (currently Kestrel
listens on `127.0.0.1:9000` per appsettings — needs `0.0.0.0` / LAN IP), plus an
iOS-appropriate auth/token path (no shell env on device).
