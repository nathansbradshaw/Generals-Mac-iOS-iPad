# Dependency notes (Phase 2, done 07/2026)

## GameNetworkingSockets 1.6.0 via vcpkg

- **arm64-osx**: builds unmodified (`vcpkg install gamenetworkingsockets --triplet arm64-osx`).
- **arm64-ios**: needs our overlay port `cmake/vcpkg-overlay-ports/gamenetworkingsockets`
  (install with `--overlay-ports=<repo>/cmake/vcpkg-overlay-ports`). Patch
  `ios-system-name.patch`, offer upstream to Valve eventually:
  1. Two CMake OS whitelists don't recognize `CMAKE_SYSTEM_NAME=iOS` → treat like Darwin.
  2. `src/tier0/dbg.cpp` debugger-detect has no iOS branch → `return false`.
- OpenSSL 3.6.x for arm64-ios builds fine via vcpkg — the feared crypto blocker
  does not exist.
- Static-only on both triplets: `libGameNetworkingSockets_s.a`. Link partners:
  `protobuf utf8_range utf8_validity ssl crypto` + all `absl_*` libs
  + `-framework Foundation -framework Security`. Prefer
  `find_package(GameNetworkingSockets CONFIG)` which resolves all of it
  (verified working on macOS; init smoke test passed).
- Headers live under the `GameNetworkingSockets/` include prefix
  (`#include <steam/...>` needs `-I<prefix>/include/GameNetworkingSockets`).
- ⚠ vcpkg's stock arm64-ios triplet builds at the current SDK target (26.5),
  our app targets iOS 16.0 — before shipping, use a custom triplet with
  `IPHONEOS_DEPLOYMENT_TARGET=16.0` to avoid min-version mismatch warnings.

## curl (T2.3)

Their `WebSocket` class is built on curl handles (`OnlineServices_Init.cpp`
teardown comments) and the backend serves `wss://…/ws` — curl needs the
`websockets` feature. Our `vcpkg.json` currently requests only `ssl`; add
`websockets` when wiring `SAGE_GENERALS_ONLINE` (T3), which will rebuild curl
for all triplets. GNS itself: add `gamenetworkingsockets` to the manifest as a
feature-gated dependency at the same time.
