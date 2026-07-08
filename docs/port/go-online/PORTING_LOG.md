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
| winhttp | pending |
| winsock headers (`ws2ipdef.h` in NetworkMesh.h) | pending |
| `localtime_s` (NGMP_Helpers.cpp) | pending |
| lobby-camera-zoom defines undeclared | pending |

**Transport refactor details:** did the refactor mechanically in our tree instead
of copying their files (ours is already POSIX-ported and modernized). `Transport`
in Core is now an abstract base (metrics + `isGeneralsPacket` stay); the UDP
socket implementation moved verbatim to new `Core` files `UDPTransport.h/.cpp`.
Unlike their fork (UDPTransport under GeneralsMD only), ours lives in Core so the
non-MD Generals build keeps working — verified `g_gameengine` still links.
Call sites switched to `new UDPTransport`: `LANAPI.cpp`, `NAT.cpp`,
`ConnectionManager.cpp`. NextGenTransport selection in ConnectionManager is
deliberately NOT wired yet — that's a Phase 4 hook.
