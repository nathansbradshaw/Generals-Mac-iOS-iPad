# License audit (T0.4)

Their repo carries EA's GPL v3 `LICENSE.md` (same as ours). NGMP files have no
per-file headers — as derivative works inside the GPLv3 repo they are GPLv3.

| Component | License | GPLv3-compatible? |
|---|---|---|
| NGMP code (GeneralsOnlineDevelopmentTeam) | GPLv3 (repo license, no per-file headers) | yes |
| GameNetworkingSockets headers (Valve) | BSD-3-Clause (per upstream repo; headers say "Copyright Valve") | yes |
| libcurl headers | curl license (MIT-like) | yes |
| nlohmann json.hpp | MIT (SPDX header present) | yes |
| stb_image_write / stb_image_resize | public domain / MIT dual | yes |
| libplum (vendored source) | MPL-2.0 | yes (file-level copyleft, GPL-compatible) |
| miniupnpc (vendored source) | BSD-3-Clause | yes |
| libnatpmp (vendored source) | BSD-3-Clause | yes |
| sentry.h / sentry.lib | MIT (sentry-native) | yes, but excluded anyway (category c) |
| EasyAntiCheat plugin repos | n/a | not imported (category c) |

Verdict: **no blockers.** Everything in the import set is GPLv3-compatible.
Note for T3.2: we replace vendored *binaries* with vcpkg builds (GNS BSD-3,
curl, OpenSSL 3 Apache-2.0 — all fine).
