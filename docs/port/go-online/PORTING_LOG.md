# NGMP porting log (T3.2+)

Initial compile of the imported subtree on macOS (arm64, Clang, `SAGE_GENERALS_ONLINE=ON`)
fails with these themes, burned down one commit each below:

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
